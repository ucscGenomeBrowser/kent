/* bigWigCat - Merge a collection of non-overlapping bigWig files 
 * directly into binary big wig format.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "obscure.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bigWig.h"
#include "bwgInternal.h"
#include "zlibFace.h"
#include "errAbort.h"
#include "sqlNum.h"
#include "sig.h"
#include "bPlusTree.h"
#include "cirTree.h"
#include "bbiFile.h"
#include "udc.h"


static int blockSize = 256;
static int itemsPerSlot = 1024;
static boolean doCompress = FALSE;

static struct optionSpec options[] = 
{
{"itemsPerSlot", OPTION_INT},
{NULL, 0},
};

struct bwmSection 
{
bits32 chromId;			/* Chromosome name. */
bits32 start,end;			/* Range of chromosome covered. */
bits64 fileOffset;			/* Offset of section in file. */
};

static struct cirTreeRange bwmSectionFetchKey(const void *va, void *context)
/* Fetch bwmSection key for r-tree */
{
struct cirTreeRange res;
const struct bwmSection *a = (struct bwmSection *)va;
res.chromIx = a->chromId;
res.start = a->start;
res.end = a->end;
return res;
}

static bits64 bwmSectionFetchOffset(const void *va, void *context)
/* Fetch bwmSection file offset for r-tree */
{
const struct bwmSection *a = (struct bwmSection *)va;
return a->fileOffset;
}

static void collectChromInfo(struct bbiFile ** inBbiFiles, int inBbiFilesCount, 
	int *retChromCount, struct bbiChromInfo **retChromArray,
	int *retMaxChromNameSize)
/* Fill in chromId field in sectionList.  Return array of chromosome name/ids. 
 * The chromSizeHash is keyed by name, and has int values. */
{
/* Build up list of unique chromosome names. */
int maxChromNameSize = 0;
int chromCount = 0;
int i;
struct bbiChromInfo *chromInfo, *chromArray;
for (i = 0, chromInfo = bbiChromList(inBbiFiles[0]); chromInfo; ++i, chromInfo = chromInfo->next)
    chromCount++;

/* Allocate and fill in results array. */
AllocArray(chromArray, chromCount);
for (i = 0, chromInfo = bbiChromList(inBbiFiles[0]); chromInfo; ++i, chromInfo = chromInfo->next)
    {
    chromArray[i].name = chromInfo->name;
    chromArray[i].id = i;
    chromArray[i].size = chromInfo->size;
    if (strlen(chromInfo->name) > maxChromNameSize)
	maxChromNameSize = strlen(chromInfo->name);
    }

/* Clean up, set return values and go home. */
*retChromCount = chromCount;
*retChromArray = chromArray;
*retMaxChromNameSize = maxChromNameSize;
}

static void bbiSummaryHandleSwapped(struct bbiFile *bbi, struct bbiSummaryOnDisk *in)
/* Swap integer fields in summary as needed. */
{
if (bbi->isSwapped)
    {
    in->chromId = byteSwap32(in->chromId);
    in->start = byteSwap32(in->start);
    in->end = byteSwap32(in->end);
    in->validCount = byteSwap32(in->validCount);
    in->minVal = byteSwapFloat(in->minVal);
    in->maxVal = byteSwapFloat(in->maxVal);
    in->sumData = byteSwapFloat(in->sumData);
    in->sumSquares = byteSwapFloat(in->sumSquares);
    }
}

static void bbiWriteFileSummary(bits32 reductionLevel, struct bbiFile * input, FILE *f, struct bwmSection ** summary)
/* Write out summary and index to summary uncompressed, returning start position of
 * summary index. */
{
struct udcFile *udc = input->udc;
struct bbiZoomLevel *zoom;
for (zoom = input->levelList; zoom; zoom = zoom->next) 
    if (zoom->reductionLevel == reductionLevel)
	break;
udcSeek(udc, zoom->indexOffset);
struct cirTreeFile *ctf = cirTreeFileAttach(input->fileName, udc);
struct fileOffsetSize *blockList = cirTreeEnumerateBlocks(ctf);
struct fileOffsetSize *block, *beforeGap, *afterGap;

char *uncompressBuf = NULL;
if (input->uncompressBufSize > 0)
    uncompressBuf = needLargeMem(input->uncompressBufSize);

/* This loop is a little complicated because we merge the read requests for efficiency, but we 
* have to then go back through the data one unmerged block at a time. */
for (block = blockList; block != NULL; )
   {
   /* Find contigious blocks and read them into mergedBuf. */
   fileOffsetSizeFindGap(block, &beforeGap, &afterGap);
   bits64 mergedOffset = block->offset;
   bits64 mergedSize = beforeGap->offset + beforeGap->size - mergedOffset;
   udcSeek(udc, mergedOffset);
   char *mergedBuf = needLargeMem(mergedSize);
   udcMustRead(udc, mergedBuf, mergedSize);
   char *blockBuf = mergedBuf;

   /* Loop through individual blocks within merged section. */
   for (;block != afterGap; block = block->next)
       {
       // Copy paste
       bits64 filePos = ftell(f);
       mustWrite(f, blockBuf, block->size);

       /* Uncompress if necessary. */
       char *blockPt, *blockEnd;
       if (uncompressBuf)
	    {
	    blockPt = uncompressBuf;
	    int uncSize = zUncompress(blockBuf, block->size, uncompressBuf, input->uncompressBufSize);
	    blockEnd = blockPt + uncSize;
	    }
	else
	    {
	    blockPt = blockBuf;
	    blockEnd = blockPt + block->size;
	    }

	/* Figure out bounds and number of items in block. */
	int blockSize = blockEnd - blockPt;
	struct bbiSummaryOnDisk *dSum;
	int itemSize = sizeof(*dSum);
	assert(blockSize % itemSize == 0);
	int itemCount = blockSize / itemSize;

	/* Read in items and store coords. */
	int i;
	for (i=0; i<itemCount; ++i)
	    {
	    dSum = (void *)blockPt;
	    bbiSummaryHandleSwapped(input, dSum);
	    (*summary)->chromId = dSum->chromId;
	    (*summary)->start = dSum->start;
	    (*summary)->end = dSum->end;
	    (*summary)->fileOffset = filePos;
	    (*summary)++;
	    blockPt += sizeof(*dSum);
	    }

	assert(blockPt == blockEnd);
	blockBuf += block->size;
	}
    freeMem(mergedBuf);
    }
freeMem(uncompressBuf);
slFreeList(&blockList);
cirTreeFileDetach(&ctf);
}

static bits32 countSummaryElementsInFile(bits32 reductionLevel, struct bbiFile * inputFile) 
{
struct udcFile *udc = inputFile->udc;
struct bbiZoomLevel *zoom;
for (zoom = inputFile->levelList; zoom; zoom = zoom->next) 
    if (zoom->reductionLevel == reductionLevel)
	break;
udcSeek(udc, zoom->indexOffset);
struct cirTreeFile *ctf = cirTreeFileAttach(inputFile->fileName, udc);
bits32 res = ctf->itemCount;
cirTreeFileDetach(&ctf);
return res;
}

static bits32 countSummaryElements(bits32 reductionLevel, struct bbiFile ** inputFiles, int inputFilesCount) 
{
int i;
bits32 res = 0;

for (i = 0; i < inputFilesCount; i++)
    res += countSummaryElementsInFile(reductionLevel, inputFiles[i]);

return res;
}

void bbiWriteSummary(bits32 reductionLevel, struct bbiFile ** inputFiles, int inputFilesCount, FILE *f, struct bwmSection ** summary) 
{
int i;

for (i = 0; i < inputFilesCount; i++) 
    bbiWriteFileSummary(reductionLevel, inputFiles[i], f, summary);
}


bits64 bwmWriteSummaryAndIndex(bits32 reductionLevel, struct bbiFile ** inputFiles, int inputFilesCount, int blockSize, int itemsPerSlot, FILE *f)
/* Write out summary and index to summary, returning start position of
 * summary index. */
{
bits32 count = countSummaryElements(reductionLevel, inputFiles, inputFilesCount);
struct bwmSection * summaryPtr, * summaryArray;
AllocArray(summaryArray, count);
summaryPtr = summaryArray;

bbiWriteSummary(reductionLevel, inputFiles, inputFilesCount, f, &summaryPtr);

bits64 indexOffset = ftell(f);

cirTreeFileBulkIndexToOpenFile(summaryArray, sizeof(summaryArray[0]), count,
    blockSize, itemsPerSlot, NULL, bwmSectionFetchKey, bwmSectionFetchOffset, 
    indexOffset, f);

freez(&summaryArray);
return indexOffset;
}

void mergedBwgCreate(struct bbiFile ** inBbiFiles, int inBbiFilesCount, 
	int blockSize, int itemsPerSlot, boolean doCompress, char *fileName)
{
FILE *f = mustOpen(fileName, "wb");
bits32 sig = bigWigSig;
bits16 version = bbiCurrentVersion;
bits16 summaryCount = 0;
struct bbiZoomLevel *zoom;
for (zoom = inBbiFiles[0]->levelList; zoom; zoom = zoom->next)
    summaryCount++;
bits16 reserved16 = 0;
bits32 reserved32 = 0;
bits64 reserved64 = 0;
bits64 dataOffset = 0, dataOffsetPos;
bits64 indexOffset = 0, indexOffsetPos;
bits64 chromTreeOffset = 0, chromTreeOffsetPos;
bits64 totalSummaryOffset = 0, totalSummaryOffsetPos;
bits32 uncompressBufSize = 0;
bits64 uncompressBufSizePos;
bits64 reductionDataOffsetPos[10];
bits64 reductionDataOffsets[10];
bits64 reductionIndexOffsets[10];
int i;

/* Figure out chromosome ID's. */
struct bbiChromInfo *chromInfoArray;
int chromCount, maxChromNameSize;
collectChromInfo(inBbiFiles, inBbiFilesCount, &chromCount, &chromInfoArray, &maxChromNameSize);

/* Write fixed header. */
writeOne(f, sig);
writeOne(f, version);
writeOne(f, summaryCount);
chromTreeOffsetPos = ftell(f);
writeOne(f, chromTreeOffset);
dataOffsetPos = ftell(f);
writeOne(f, dataOffset);
indexOffsetPos = ftell(f);
writeOne(f, indexOffset);
writeOne(f, reserved16);  /* fieldCount */
writeOne(f, reserved16);  /* definedFieldCount */
writeOne(f, reserved64);  /* autoSqlOffset. */
totalSummaryOffsetPos = ftell(f);
writeOne(f, totalSummaryOffset);
uncompressBufSizePos = ftell(f);
writeOne(f, uncompressBufSize);
writeOne(f, reserved64);  /* nameIndexOffset */
assert(ftell(f) == 64);

/* Write summary headers */
i = 0;
for (zoom = inBbiFiles[0]->levelList; zoom; zoom = zoom->next)
    {
    writeOne(f, zoom->reductionLevel);
    writeOne(f, reserved32);
    reductionDataOffsetPos[i++] = ftell(f);
    writeOne(f, reserved64);	// Fill in with data offset later
    writeOne(f, reserved64);	// Fill in with index offset later
    }

/* Write dummy summary */
struct bbiSummaryElement totalSum;
ZeroVar(&totalSum);
totalSummaryOffset = ftell(f);
bbiSummaryElementWrite(f, &totalSum);

/* Write chromosome bPlusTree */
chromTreeOffset = ftell(f);
int chromBlockSize = min(blockSize, chromCount);
bptFileBulkIndexToOpenFile(chromInfoArray, sizeof(chromInfoArray[0]), chromCount, chromBlockSize,
    bbiChromInfoKey, maxChromNameSize, bbiChromInfoVal, 
    sizeof(chromInfoArray[0].id) + sizeof(chromInfoArray[0].size), 
    f);

/* Write out data section count and sections themselves. */
dataOffset = ftell(f);
bits64 sectionCount = 0;
int index;
for (index = 0; index < inBbiFilesCount; index++)
    sectionCount += inBbiFiles[index]->unzoomedCir->itemCount;
writeOne(f, sectionCount);
struct bwmSection *section, *sectionArray;
AllocArray(sectionArray, sectionCount);
section = sectionArray;

for (index = 0; index < inBbiFilesCount; index++)
    {
    struct bbiFile * bwf = inBbiFiles[index];
    struct udcFile *udc = bwf->udc;

    // Get maximum uncompressed size
    bits32 uncSizeOne = bwf->uncompressBufSize;
    if (uncSizeOne > uncompressBufSize)
         uncompressBufSize = uncSizeOne;

    char *uncompressBuf = NULL;
    if (bwf->uncompressBufSize > 0)
        uncompressBuf = needLargeMem(bwf->uncompressBufSize);

    // Copy paste all the blocks
    struct bbiChromInfo *chrom, *chromList = bbiChromList(bwf);
    for (chrom = chromList; chrom != NULL; chrom = chrom->next)
        {
        struct fileOffsetSize *block, *blockList = bbiOverlappingBlocks(bwf, bwf->unzoomedCir, 
	        chrom->name, 0, chrom->size, NULL);
        for (block = blockList; block != NULL; ) 
	    {
            struct fileOffsetSize *beforeGap, *afterGap;
            fileOffsetSizeFindGap(block, &beforeGap, &afterGap);
            bits64 mergedOffset = block->offset;
            bits64 mergedSize = beforeGap->offset + beforeGap->size - mergedOffset;
            udcSeek(udc, mergedOffset);
            char *mergedBuf = needLargeMem(mergedSize);
            udcMustRead(udc, mergedBuf, mergedSize);
	    char *blockBuf = mergedBuf;

	    for (;block != afterGap; block = block->next) 
		{
		struct bwgSectionHead head;
		char * blockPt = blockBuf;
		if (uncompressBuf)
		    {
		    blockPt = uncompressBuf;
		    zUncompress(blockBuf, block->size, uncompressBuf, bwf->uncompressBufSize);
		    }
		 else
		    {
		    blockPt = blockBuf;
		    }	
		bwgSectionHeadFromMem(&blockPt, &head, bwf->isSwapped);
		section->chromId = chrom->id;
		section->start = head.start;
		section->end = head.end;
		section->fileOffset = ftell(f);
		section++;

		mustWrite(f, blockBuf, block->size);
		blockBuf += block->size;
		}
            freeMem(mergedBuf);
            }
	if (blockList)
	    slFreeList(blockList);
	}
    freeMem(uncompressBuf);
    slFreeList(chromList);
    }

/* Write out index */
indexOffset = ftell(f);
cirTreeFileBulkIndexToOpenFile(sectionArray, sizeof(sectionArray[0]), sectionCount,
    blockSize, 1, NULL, bwmSectionFetchKey, bwmSectionFetchOffset, 
    indexOffset, f);
freez(&sectionArray);

/* Write out summary sections. */
verbose(2, "bwgCreate writing %d summaries\n", summaryCount);
i = 0;
for (zoom = inBbiFiles[0]->levelList; zoom; zoom = zoom->next) 
    {
    reductionDataOffsets[i] = ftell(f);
    reductionIndexOffsets[i++] = bwmWriteSummaryAndIndex(zoom->reductionLevel, inBbiFiles, inBbiFilesCount, blockSize, itemsPerSlot, f);
    }

/* Calculate summary */
struct bbiSummaryElement sum = bbiTotalSummary(inBbiFiles[0]);
totalSum.validCount = sum.validCount;
totalSum.minVal = sum.minVal;
totalSum.maxVal = sum.maxVal;
totalSum.sumData = sum.sumData;
totalSum.sumSquares = sum.sumSquares;
for (index = 1; index < inBbiFilesCount; index++) 
    {
    sum = bbiTotalSummary(inBbiFiles[index]);
    totalSum.validCount += sum.validCount;
    if (totalSum.minVal > sum.minVal)
	totalSum.minVal = sum.minVal;
    if (totalSum.maxVal > sum.maxVal)
	totalSum.maxVal = sum.maxVal;
    totalSum.sumData += sum.sumData;
    totalSum.sumSquares += sum.sumSquares;
    }
/* Write real summary */
fseek(f, totalSummaryOffset, SEEK_SET);
bbiSummaryElementWrite(f, &totalSum);

/* Go back and fill in offsets properly in header. */
fseek(f, dataOffsetPos, SEEK_SET);
writeOne(f, dataOffset);
fseek(f, indexOffsetPos, SEEK_SET);
writeOne(f, indexOffset);
fseek(f, chromTreeOffsetPos, SEEK_SET);
writeOne(f, chromTreeOffset);
fseek(f, totalSummaryOffsetPos, SEEK_SET);
writeOne(f, totalSummaryOffset);

if (doCompress)
    {
    int maxZoomUncompSize = itemsPerSlot * sizeof(struct bbiSummaryOnDisk);
    if (maxZoomUncompSize > uncompressBufSize)
	uncompressBufSize = maxZoomUncompSize;
    fseek(f, uncompressBufSizePos, SEEK_SET);
    writeOne(f, uncompressBufSize);
    }

/* Also fill in offsets in zoom headers. */
for (i=0; i<summaryCount; ++i)
    {
    fseek(f, reductionDataOffsetPos[i], SEEK_SET);
    writeOne(f, reductionDataOffsets[i]);
    writeOne(f, reductionIndexOffsets[i]);
    }

/* Write end signature. */
fseek(f, 0L, SEEK_END);
writeOne(f, sig);

/* Clean up */
freez(&chromInfoArray);
carefulClose(&f);
}

void checkCompression(char ** inNames, struct bbiFile ** inFiles, int inNamesCount) 
{
int index;
doCompress = inFiles[0]->uncompressBufSize > 0;
for (index = 1; index < inNamesCount; index++) 
    if ((inFiles[index]->uncompressBufSize > 0) != doCompress)
	errAbort("Some of the files are compressed, some are not.\n");
}

void checkBlockSize(char ** inNames, struct bbiFile ** inFiles, int inNamesCount) 
{
int index;
blockSize = inFiles[0]->unzoomedCir->blockSize;
for (index = 1; index < inNamesCount; index++) 
    if (inFiles[index]->unzoomedCir->blockSize != blockSize)
	errAbort("Not all files have the same block size.\n");
}

void checkChromosomes(char ** inNames, struct bbiFile ** inFiles, int inNamesCount) 
{
int index;
struct bbiChromInfo *chromList0 = bbiChromList(inFiles[0]);
for (index = 1; index < inNamesCount; index++) 
    {
    struct bbiChromInfo *chrom0, *chrom, *chromList = bbiChromList(inFiles[index]);
    for (chrom = chromList, chrom0 = chromList0; chrom && chrom0; chrom = chrom->next, chrom0 = chrom0->next) 
	if (chrom0->size != chrom->size || strcmp(chrom0->name, chrom->name)) 
	    errAbort("The bigwig files do not have the same chromosome details.\n");
    if (chrom || chrom0) 
	errAbort("The bigwigs files do not have the same number of chromosomes.\n");
    slFreeList(chromList);
    }
slFreeList(chromList0);
}

void checkReductions(char ** inNames, struct bbiFile ** inFiles, int inNamesCount) 
{
int index;
for (index = 1; index < inNamesCount; index++) 
    {
    struct bbiZoomLevel *zoom, *zoom0;
    for (zoom = inFiles[index]->levelList, zoom0 = inFiles[0]->levelList; zoom && zoom0; zoom = zoom->next, zoom0 = zoom0->next) 
	{
	if (zoom->reductionLevel != zoom0->reductionLevel)
	    errAbort("The bigwig files do not have the same reduction levels\n");
	}
    if (zoom || zoom0)
	errAbort("The bigwig files do not all have the same number of reduction levels.\n");
    }
}

struct fileNamePair 
{
char * name;
struct bbiFile * file;
};

int comparePairs(const void * a, const void * b) 
{
struct fileNamePair * A , * B;
A = (struct fileNamePair *) a;
B = (struct fileNamePair *) b;
if (A->file->unzoomedCir->startChromIx > B->file->unzoomedCir->startChromIx)
    return 1;
else if (B->file->unzoomedCir->startChromIx > A->file->unzoomedCir->startChromIx)
    return -1;
else
    return (int) (A->file->unzoomedCir->startBase - B->file->unzoomedCir->startBase);
}

void sortFilesByCoords(char ** inNames, struct bbiFile ** inFiles, int inNamesCount) 
{
struct fileNamePair * pairs;
AllocArray(pairs, inNamesCount);
int index; 

for (index = 0; index < inNamesCount; index++) 
    {
    pairs[index].name = inNames[index];
    pairs[index].file = inFiles[index];
    }

qsort(pairs, inNamesCount, sizeof(struct fileNamePair), comparePairs);

for (index = 0; index < inNamesCount; index++) 
    {
    inNames[index] = pairs[index].name;
    inFiles[index] = pairs[index].file;
    }

// Clean up
freez(&pairs);
}

char filesOverlap(struct bbiFile * A, struct bbiFile * B) 
{
if (A->unzoomedCir->endChromIx < B->unzoomedCir->startChromIx)
    return 0;
else if (A->unzoomedCir->endChromIx > B->unzoomedCir->startChromIx)
    return 1;
else
    return A->unzoomedCir->endBase >= B->unzoomedCir->startBase;
}

void checkOverlaps(char ** inNames, struct bbiFile ** inFiles, int inNamesCount) 
{
int index;

for (index = 1; index < inNamesCount; index++)
    if (filesOverlap(inFiles[index-1], inFiles[index])) {
	struct cirTreeFile * ct1 = inFiles[index-1]->unzoomedCir;
	struct cirTreeFile * ct2 = inFiles[index]->unzoomedCir;
	char chrom1[100], chrom2[100], chrom3[100], chrom4[100];
	bbiCachedChromLookup(inFiles[index-1], ct1->startChromIx, -1, chrom1, 100);
	bbiCachedChromLookup(inFiles[index-1], ct1->endChromIx, -1, chrom2, 100);
	bbiCachedChromLookup(inFiles[index], ct2->startChromIx, -1, chrom3, 100);
	bbiCachedChromLookup(inFiles[index], ct2->endChromIx, -1, chrom4, 100);
	errAbort("Files %s and %s overlap, cannot continue!\n"
		 "%s spans from %s:%d to %s:%d\n"
		 "%s spans from %s:%d to %s:%d\n",
		 inNames[index-1], inNames[index], 
		 inNames[index-1], chrom1, ct1->startBase, chrom2, ct1->endBase,
		 inNames[index], chrom3, ct2->startBase, chrom4, ct2->endBase);
    }
}

void checkFileSettings(char ** inNames, struct bbiFile ** inFiles, int inNamesCount) 
{
checkCompression(inNames, inFiles, inNamesCount);
checkBlockSize(inNames, inFiles, inNamesCount);
checkChromosomes(inNames, inFiles, inNamesCount);
sortFilesByCoords(inNames, inFiles, inNamesCount);
checkOverlaps(inNames, inFiles, inNamesCount);
checkReductions(inNames, inFiles, inNamesCount);
}

void bigWigCat(
	char **inNames, 	/* Input files in big wig format. */
	int inNamesCount,	/* Number of input files */
	char *outName)
/* Merge multiple non-overlapping bigwig files
 * into a single big wig file. */
{
/* This code needs to agree with code in two other places currently - bigBedFileCreate,
 * and bbiFileOpen.  I'm thinking of refactoring to share at least between
 * bigBedFileCreate and bigWigFileCreate.  It'd be great so it could be structured
 * so that it could send the input in one chromosome at a time, and send in the zoom
 * stuff only after all the chromosomes are done.  This'd potentially reduce the memory
 * footprint by a factor of 2 or 4.  Still, for now it works. -JK */
struct lm *lm = lmInit(0);

struct bbiFile ** inBbiFiles;
AllocArray(inBbiFiles, inNamesCount);
int i;
for (i = 0; i < inNamesCount; i++) 
    {
    inBbiFiles[i] = bigWigFileOpen(inNames[i]);
    bbiAttachUnzoomedCir(inBbiFiles[i]);
    }

checkFileSettings(inNames, inBbiFiles, inNamesCount);
mergedBwgCreate(inBbiFiles, inNamesCount, blockSize, itemsPerSlot, doCompress, outName);

lmCleanup(&lm);
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigWigCat v %d - merge non-overlapping bigWig files\n" 
  "directly into bigWig format\n"
  "usage:\n"
  "   bigWigCat out.bw in1.bw in2.bw ...\n"
  "Where in*.bw is in big wig format\n"
  "and out.bw is the output indexed big wig file.\n"
  "options:\n"
  "   -itemsPerSlot=N - Number of data points bundled at lowest level. Default %d\n"
  "\n"
  "Note: must use wigToBigWig -fixedSummaries -keepAllChromosomes (perhaps in parallel cluster jobs) to create the input files.\n"
  "Note: By non-overlapping we mean the entire span of each file, from first data point to last data point, must not overlap with that of other files.\n"
  , bbiCurrentVersion, itemsPerSlot
  );
}

int main(int argc, char *argv[])
{
optionInit(&argc, argv, options);

/* Process command line. */
optionInit(&argc, argv, options);
itemsPerSlot = optionInt("itemsPerSlot", itemsPerSlot);
if (argc < 4)
    usage();
char * outName = argv[1];
char ** inNames = argv + 2;
int inNamesCount = argc - 2;

bigWigCat(inNames, inNamesCount, outName);

if (verboseLevel() > 1)
    printVmPeak();
return 0;
}
