/* bedGraphToBigWig - Convert a bedGraph program to bigWig.. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "cirTree.h"
#include "bPlusTree.h"
#include "bbiFile.h"
#include "bwgInternal.h"
#include "bigWig.h"

static char const rcsid[] = "$Id: bedGraphToBigWig.c,v 1.2 2009/06/24 00:38:35 kent Exp $";

int blockSize = 256;
int itemsPerSlot = 1024;
boolean clipDontDie = FALSE;


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedGraphToBigWig - Convert a bedGraph program to bigWig.\n"
  "usage:\n"
  "   bedGraphToBigWig in.bedGraph chrom.sizes out.bw\n"
  "where in.bedGraph is a four column file in the format:\n"
  "      <chrom> <start> <end> <value>\n"
  "and chrom.sizes is two column: <chromosome name> <size in bases>\n"
  "and out.bw is the output indexed big wig file.\n"
  "options:\n"
  "   -blockSize=N - Number of items to bundle in r-tree.  Default %d\n"
  "   -itemsPerSlot=N - Number of data points bundled at lowest level. Default %d\n"
  "   -clip - If set just issue warning messages rather than dying if wig\n"
  "                  file contains items off end of chromosome."
  , blockSize, itemsPerSlot
  );
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {"itemsPerSlot", OPTION_INT},
   {"clip", OPTION_BOOLEAN},
   {NULL, 0},
};


struct bwgBedGraphItem
/* An bedGraph-type item in a bwgSection. */
    {
    struct bwgBedGraphItem *next;	/* Next in list. */
    bits32 start,end;		/* Range of chromosome covered. */
    float val;			/* Value. */
    };

struct chromUsage
/* Information on how many items per chromosome etc. */
    {
    struct chromUsage *next;
    char *name;	/* chromosome name. */
    bits32 itemCount;	/* Number of items for this chromosome. */
    bits32 id;	/* Unique ID for chromosome. */
    };



struct chromUsage *readPass1(struct lineFile *lf, int *retMinDiff)
/* Go through chromGraph file and collect chromosomes and statistics. */
{
char *row[2];
struct hash *uniqHash = hashNew(0);
struct chromUsage *usage = NULL, *usageList = NULL;
int lastStart = -1;
bits32 id = 0;
int minDiff = BIGNUM;
for (;;)
    {
    int rowSize = lineFileChopNext(lf, row, ArraySize(row));
    if (rowSize == 0)
        break;
    lineFileExpectWords(lf, 2, rowSize);
    char *chrom = row[0];
    int start = lineFileNeedNum(lf, row, 1);
    if (usage == NULL || differentString(usage->name, chrom))
        {
	if (hashLookup(uniqHash, chrom))
	    {
	    errAbort("%s is not sorted.  Please use \"sort -k1 -k2n\" or bedSort and try again.",
	    	lf->fileName);
	    }
	hashAdd(uniqHash, chrom, NULL);
	AllocVar(usage);
	usage->name = cloneString(chrom);
	usage->id = id++;
	slAddHead(&usageList, usage);
	lastStart = -1;
	}
    usage->itemCount += 1;
    if (lastStart >= 0)
        {
	int diff = start - lastStart;
	if (diff < minDiff)
	    minDiff = diff;
	}
    lastStart = start;
    }
slReverse(&usageList);
*retMinDiff = minDiff;
return usageList;
}

void writeDummyHeader(FILE *f)
/* Write out all-zero header, just to reserve space for it. */
{
repeatCharOut(f, 0, 64);
}

void writeDummyZooms(FILE *f)
/* Write out zeroes to reserve space for ten zoom levels. */
{
int maxZoomLevels = 10;
repeatCharOut(f, 0, maxZoomLevels * 24);
}

void writeChromInfo(struct chromUsage *usageList, int blockSize, 
	struct hash *chromSizesHash, FILE *f)
/* Write out information on chromosomes to file. */
{
int chromCount = slCount(usageList);
struct chromUsage *usage;

/* Allocate and fill in array from list. */
struct bbiChromInfo *chromInfoArray;
AllocArray(chromInfoArray, chromCount);
int i;
int maxChromNameSize = 0;
for (i=0, usage = usageList; i<chromCount; ++i, usage = usage->next)
    {
    char *chromName = usage->name;
    int len = strlen(chromName);
    if (len > maxChromNameSize)
        maxChromNameSize = len;
    chromInfoArray[i].name = chromName;
    chromInfoArray[i].id = usage->id;
    chromInfoArray[i].size = hashIntVal(chromSizesHash, chromName);
    }

/* Write chromosome bPlusTree */
int chromBlockSize = min(blockSize, chromCount);
bptFileBulkIndexToOpenFile(chromInfoArray, sizeof(chromInfoArray[0]), chromCount, chromBlockSize,
    bbiChromInfoKey, maxChromNameSize, bbiChromInfoVal, 
    sizeof(chromInfoArray[0].id) + sizeof(chromInfoArray[0].size), 
    f);

freeMem(chromInfoArray);
}

int countSectionsNeeded(struct chromUsage *usageList, int itemsPerSlot)
/* Count up number of sections needed for data. */
{
struct chromUsage *usage;
int count = 0;
for (usage = usageList; usage != NULL; usage = usage->next)
    {
    int countOne = (usage->itemCount + itemsPerSlot - 1)/itemsPerSlot;
    count += countOne;
    uglyf("%s %d, %d blocks of %d\n", usage->name, usage->itemCount, countOne, itemsPerSlot);
    }
return count;
}

struct sectionBounds
/* What a section covers and where it is on disk. */
    {
    bits64 offset;		/* Offset within file. */
    struct cirTreeRange range;	/* What is covered. */
    };

static struct cirTreeRange sectionBoundsFetchKey(const void *va, void *context)
/* Fetch sectionBounds key for r-tree */
{
const struct sectionBounds *a = ((struct sectionBounds *)va);
return a->range;
}

static bits64 sectionBoundsFetchOffset(const void *va, void *context)
/* Fetch sectionBounds file offset for r-tree */
{
const struct sectionBounds *a = ((struct sectionBounds *)va);
return a->offset;
}

struct sectionItem
/* An item in a section of a bedGraph. */
    {
    bits32 start, end;			/* Position in chromosome, half open. */
    float val;				/* Single precision value. */
    };

void writeSections(struct chromUsage *usageList, struct lineFile *lf, 
	int itemsPerSlot, struct sectionBounds *bounds, int sectionCount, FILE *f,
	int resTryCount, int resScales[], int resSizes[])
/* Read through lf, chunking it into sections that get written to f.  Save info
 * about sections in bounds. */
{
struct chromUsage *usage = usageList;
int itemIx = 0, sectionIx = 0;
bits32 reserved32 = 0;
bits16 reserved16 = 0;
UBYTE reserved8 = 0;
struct sectionItem items[itemsPerSlot];
struct sectionItem *lastB = NULL;
bits32 resEnds[resTryCount];
int resTry;
for (resTry = 0; resTry < resTryCount; ++resTry)
    resEnds[resTry] = 0;
for (;;)
    {
    /* Get next line of input if any. */
    char *row[5];
    int rowSize = lineFileChopNext(lf, row, ArraySize(row));

    /* Figure out whether need to output section. */
    boolean sameChrom = FALSE;
    if (rowSize > 0)
	sameChrom = sameString(row[0], usage->name);
    if (itemIx >= itemsPerSlot || rowSize == 0 || !sameChrom)
        {
	/* Figure out section position. */
	bits32 chromId = usage->id;
	bits32 sectionStart = items[0].start;
	bits32 sectionEnd = items[itemIx-1].start;

	/* Save section info for indexing. */
	assert(sectionIx < sectionCount);
	struct sectionBounds *section = &bounds[sectionIx++];
	section->offset = ftell(f);
	section->range.chromIx = chromId;
	section->range.start = sectionStart;
	section->range.end = sectionEnd;

	/* Output section header to file. */
	UBYTE type = bwgTypeBedGraph;
	bits16 itemCount = itemIx;
	writeOne(f, chromId);			// chromId
	writeOne(f, sectionStart);		// start
	writeOne(f, sectionEnd);	// end
	writeOne(f, reserved32);		// itemStep
	writeOne(f, reserved32);		// itemSpan
	writeOne(f, type);			// type
	writeOne(f, reserved8);			// reserved
	writeOne(f, itemCount);			// itemCount

	/* Output each item in section to file. */
	int i;
	for (i=0; i<itemIx; ++i)
	    {
	    struct sectionItem *item = &items[i];
	    writeOne(f, item->start);
	    writeOne(f, item->end);
	    writeOne(f, item->val);
	    }

	/* If at end of input we are done. */
	if (rowSize == 0)
	    break;

	/* Set up for next section. */
	itemIx = 0;

	if (!sameChrom)
	    {
	    usage = usage->next;
	    assert(usage != NULL);
	    assert(sameString(row[0], usage->name));
	    lastB = NULL;
	    for (resTry = 0; resTry < resTryCount; ++resTry)
		resEnds[resTry] = 0;
	    }
	}

    /* Parse out input. */
    lineFileExpectWords(lf, 4, rowSize);
    bits32 start = lineFileNeedNum(lf, row, 1);
    bits32 end = lineFileNeedNum(lf, row, 2);
    float val = lineFileNeedDouble(lf, row, 3);

    /* Verify that inputs meets our assumption - that it is a sorted bedGraph file. */
    if (start > end)
        errAbort("Start (%u) after end (%u) line %d of %s", start, end, lf->lineIx, lf->fileName);
    if (lastB != NULL)
        {
	if (lastB->start > start)
	    errAbort("BedGraph not sorted on start line %d of %s", lf->lineIx, lf->fileName);
	if (lastB->end > start)
	    errAbort("Overlapping regions in bedGraph line %d of %s", lf->lineIx, lf->fileName);
	}


    /* Do zoom counting. */
    for (resTry = 0; resTry < resTryCount; ++resTry)
        {
	bits32 resEnd = resEnds[resTry];
	if (start >= resEnd)
	    {
	    resSizes[resTry] += 1;
	    resEnds[resTry] = resEnd = start + resScales[resTry];
	    }
	while (end > resEnd)
	    {
	    resSizes[resTry] += 1;
	    resEnds[resTry] = resEnd = resEnd + resScales[resTry];
	    }
	}

    /* Save values in output array. */
    struct sectionItem *b = &items[itemIx];
    b->start = start;
    b->end = end;
    b->val = val;
    lastB = b;
    itemIx += 1;
    }
assert(sectionIx == sectionCount);
}

void bedGraphToBigWig(char *inName, char *chromSizes, char *outName)
/* bedGraphToBigWig - Convert a bedGraph program to bigWig.. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
struct hash *chromSizesHash = bbiChromSizesFromFile(chromSizes);
verbose(2, "%d chroms in %s\n", chromSizesHash->elCount, chromSizes);
int minDiff;
bits64 totalDiff, diffCount;
struct chromUsage *usage, *usageList = readPass1(lf, &minDiff);
verbose(2, "%d chroms in %s\n", slCount(usageList), inName);
for (usage = usageList; usage != NULL; usage = usage->next)
    uglyf("%s\t%d\n", usage->name, usage->itemCount);

/* Write out dummy header, zoom offsets. */
FILE *f = mustOpen(outName, "wb");
writeDummyHeader(f);
writeDummyZooms(f);

/* Write out chromosome/size database. */
bits64 chromTreeOffset = ftell(f);
writeChromInfo(usageList, blockSize, chromSizesHash, f);

/* Set up to keep track of reduction levels. */
int resTryCount = 10, resTry;
int resIncrement = 4;
int resScales[resTryCount], resSizes[resTryCount];
int res = minDiff * 10;
if (res > 0)
    {
    for (resTry = 0; resTry < resTryCount; ++resTry)
	{
	resSizes[resTry] = 0;
	resScales[resTry] = res;
	res *= resIncrement;
	}
    }
else
    resTryCount = 0;

/* Write out primary full resolution data in sections, collect stats to use for reductions. */
bits64 dataOffset = ftell(f);
bits32 sectionCount = countSectionsNeeded(usageList, itemsPerSlot);
struct sectionBounds *sectionBounds;
AllocArray(sectionBounds, sectionCount);
lineFileRewind(lf);
writeSections(usageList, lf, itemsPerSlot, sectionBounds, sectionCount, f,
	resTryCount, resScales, resSizes);

for (resTry = 0; resTry < resTryCount; ++resTry)
    uglyf("%d scale %d, size %d\n", resTry, resScales[resTry], resSizes[resTry]);

/* Write out primary data index. */
bits64 indexOffset = ftell(f);
cirTreeFileBulkIndexToOpenFile(sectionBounds, sizeof(sectionBounds[0]), sectionCount,
    blockSize, 1, NULL, sectionBoundsFetchKey, sectionBoundsFetchOffset, 
    indexOffset, f);

/* Write out first zoomed section while storing in memory next zoom level. */
if (minDiff > 0)
    {
    uglyf("minDiff %d\n", minDiff);
    lineFileRewind(lf);
#ifdef SOON
    writeReducedOnceReturnReducedTwice(usageList, lf, minDiff, maxDiff, aveDiff, f);
#endif /* SOON */
    }
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
blockSize = optionInt("blockSize", blockSize);
itemsPerSlot = optionInt("itemsPerSlot", itemsPerSlot);
clipDontDie = optionExists("clip");
if (argc != 4)
    usage();
bedGraphToBigWig(argv[1], argv[2], argv[3]);
return 0;
}
