/* bigBedNamedItems - Extract item(s) of given name(s) from bigBed. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "udc.h"
#include "bPlusTree.h"
#include "bigBed.h"
#include "obscure.h"
#include "zlibFace.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigBedNamedItems - Extract item of given name from bigBed\n"
  "usage:\n"
  "   bigBedNamedItems file.bb name output.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void bigBedAttachNameIndex(struct bbiFile *bbi)
/* Attach name index part of bbiFile to bbi */
{
if (bbi->nameBpt == NULL)
    {
    if (bbi->nameIndexOffset == 0)
	errAbort("%s has no name index", bbi->fileName);
    udcSeek(bbi->udc, bbi->nameIndexOffset);
    bbi->nameBpt = bptFileAttach(bbi->fileName, bbi->udc);
    }
}

struct offsetSize 
/* Simple file offset and file size. */
    {
    bits64 offset; 
    bits64 size;
    };

int cmpOffsetSizeRef(const void *va, const void *vb)
/* Compare to sort slRef pointing to offsetSize.  Sort is kind of hokey,
 * but guarantees all items that are the same will be next to each other
 * at least, which is all we care about. */
{
const struct slRef *a = *((struct slRef **)va);
const struct slRef *b = *((struct slRef **)vb);
return memcmp(a->val, b->val, sizeof(struct offsetSize));
}

struct fileOffsetSize *bigBedChunksMatchingName(struct bbiFile *bbi, char *name)
/* Get list of file chunks that match name.  Can slFreeList this when done. */
{
bigBedAttachNameIndex(bbi);
struct slRef *blockList = bptFileFindMultiple(bbi->nameBpt, name, strlen(name), sizeof(struct offsetSize));
slSort(&blockList, cmpOffsetSizeRef);

struct fileOffsetSize *fosList = NULL, *fos;
struct offsetSize lastOffsetSize = {0,0};
struct slRef *blockRef;
for (blockRef = blockList; blockRef != NULL; blockRef = blockRef->next)
    {
    if (memcmp(&lastOffsetSize, blockRef->val, sizeof(lastOffsetSize)) != 0)
        {
	memcpy(&lastOffsetSize, blockRef->val, sizeof(lastOffsetSize));
	AllocVar(fos);
	if (bbi->isSwapped)
	    {
	    fos->offset = byteSwap64(lastOffsetSize.offset);
	    fos->size = byteSwap64(lastOffsetSize.size);
	    }
	else
	    {
	    fos->offset = lastOffsetSize.offset;
	    fos->size = lastOffsetSize.size;
	    }
	slAddHead(&fosList, fos);
	}
    }
slRefFreeListAndVals(&blockList);
slReverse(&fosList);
return fosList;
}

struct bigBedInterval *bigBedNameQuery(struct bbiFile *bbi, char *name, struct lm *lm)
/* Return list of intervals matching file. These intervals will be allocated out of lm. */
{
bigBedAttachNameIndex(bbi);
boolean isSwapped = bbi->isSwapped;
struct fileOffsetSize *fos, *fosList = bigBedChunksMatchingName(bbi, name);
struct bigBedInterval *interval, *intervalList = NULL;
for (fos = fosList; fos != NULL; fos = fos->next)
    {
    /* Read in raw data */
    udcSeek(bbi->udc, fos->offset);
    char *rawData = needLargeMem(fos->size);
    udcRead(bbi->udc, rawData, fos->size);

    /* Optionally uncompress data, and set data pointer to uncompressed version. */
    char *uncompressedData = NULL;
    char *data = NULL;
    int dataSize = 0;
    if (bbi->uncompressBufSize > 0)
	{
	data = uncompressedData = needLargeMem(bbi->uncompressBufSize);
	dataSize = zUncompress(rawData, fos->size, uncompressedData, bbi->uncompressBufSize);
	}
    else
	{
        data = rawData;
	dataSize = fos->size;
	}

    /* Set up for "memRead" routines to more or less treat memory block like file */
    char *blockPt = data, *blockEnd = data + dataSize;
    struct dyString *dy = dyStringNew(32); // Keep bits outside of chrom/start/end here


    /* Read next record into local variables. */
    while (blockPt < blockEnd)
	{
	bits32 chromIx = memReadBits32(&blockPt, isSwapped);
	bits32 s = memReadBits32(&blockPt, isSwapped);
	bits32 e = memReadBits32(&blockPt, isSwapped);
	int c;
	dyStringClear(dy);
	while ((c = *blockPt++) >= 0)
	    {
	    if (c == 0)
		break;
	    dyStringAppendC(dy, c);
	    }
	if (startsWithWordByDelimiter(name, '\t', dy->string))
	    {
	    lmAllocVar(lm, interval);
	    interval->start = s;
	    interval->end = e;
	    interval->rest = cloneString(dy->string);
	    interval->chromId = chromIx;
	    slAddHead(&intervalList, interval);
	    }
	}

    /* Clean up temporary buffers. */
    dyStringFree(&dy);
    freez(&uncompressedData);
    freez(&rawData);
    }
slFreeList(&fosList);
slReverse(&intervalList);
return intervalList;
}

void bigBedIntervalListToBedFile(struct bbiFile *bbi, struct bigBedInterval *intervalList, FILE *f)
/* Write out big bed interval list to bed file,  looking up chromosome mostly */
{
char chromName[bbi->chromBpt->keySize+1];
int chromId = -1;
struct bigBedInterval *interval;
for (interval = intervalList; interval != NULL; interval = interval->next)
    {
    if (interval->chromId != chromId)
        {
	chromId = interval->chromId;
	bptStringKeyAtPos(bbi->chromBpt, chromId, chromName, sizeof(chromName));
	}
    fprintf(f, "%s\t%u\t%u\t%s\n", chromName, interval->start, interval->end, interval->rest);
    }
}

void bigBedNamedItems(char *bigBedFile, char *name, char *outFile)
/* bigBedNamedItems - Extract item(s) of given name(s) from bigBed. */
{
struct bbiFile *bbi = bigBedFileOpen(bigBedFile);
FILE *f = mustOpen(outFile, "w");
struct lm *lm = lmInit(0);
struct bigBedInterval *intervalList = bigBedNameQuery(bbi, name, lm);
bigBedIntervalListToBedFile(bbi, intervalList, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
bigBedNamedItems(argv[1], argv[2], argv[3]);
return 0;
}
