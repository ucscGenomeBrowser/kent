/* bwTest - Test out some big wig related things.. */

/* bigWig file structure:
 *     fixedWidthHeader
 *         magic# 		4 bytes
 *	   zoomLevels		4 bytes
 *         chromosomeTreeOffset	8 bytes
 *         unzoomedDataOffset	8 bytes
 *	   unzoomedIndexOffset	8 bytes
 *         reserved            32 bytes
 *     zoomHeaders
 *         zoomFactor		float (4 bytes)
 *	   reserved		4 bytes
 *	   dataOffset		8 bytes
 *         indexOffset          8 bytes
 *     chromosome b+ tree       
 *     unzoomed data
 *         sectionCount		4 bytes
 *         sectionData
 *     unzoomed index
 *     zoom info
 *         zoomed data
 *             sectionCount		4 bytes
 *             sectionData
 *         zoomed index
 */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "sqlNum.h"
#include "sig.h"
#include "bPlusTree.h"
#include "cirTree.h"
#include "bigWig.h"

static char const rcsid[] = "$Id: bwTest.c,v 1.6 2009/01/24 21:06:12 kent Exp $";

int blockSize = 1024;
int itemsPerSlot = 512;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bwTest - Test out some big wig related things.\n"
  "usage:\n"
  "   bwTest in.wig out.bw\n"
  "options:\n"
  "   -blockSize=N - Number of items to bundle in r-tree.  Default %d\n"
  "   -itemsPerSlot=N - Number of data points bundled at lowest level. Default %d\n"
  , blockSize, itemsPerSlot
  );
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {"itemsPerSlot", OPTION_INT},
   {NULL, 0},
};

struct bigWigBedGraphItem
/* An bedGraph-type item in a bigWigSect. */
    {
    struct bigWigBedGraphItem *next;	/* Next in list. */
    bits32 start,end;		/* Range of chromosome covered. */
    float val;			/* Value. */
    };

int bigWigBedGraphItemCmp(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct bigWigBedGraphItem *a = *((struct bigWigBedGraphItem **)va);
const struct bigWigBedGraphItem *b = *((struct bigWigBedGraphItem **)vb);
int dif = (int)a->start - (int)b->start;
if (dif == 0)
    dif = (int)a->end - (int)b->end;
return dif;
}

struct bigWigVariableStepItem
/* An variableStep type item in a bigWigSect. */
    {
    struct bigWigVariableStepItem *next;	/* Next in list. */
    bits32 start;		/* Start position in chromosome. */
    float val;			/* Value. */
    };

int bigWigVariableStepItemCmp(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct bigWigVariableStepItem *a = *((struct bigWigVariableStepItem **)va);
const struct bigWigVariableStepItem *b = *((struct bigWigVariableStepItem **)vb);
return (int)a->start - (int)b->start;
}

struct bigWigFixedStepItem
/* An fixedStep type item in a bigWigSect. */
    {
    struct bigWigFixedStepItem *next;	/* Next in list. */
    float val;			/* Value. */
    };

union bigWigItem
/* Union of item pointers for all possible section types. */
    {
    struct bigWigBedGraphItem *bedGraph;
    struct bigWigVariableStepItem *variableStep;
    struct bigWigFixedStepItem *fixedStep;
    };

struct bigWigSect
/* A section of a bigWig file - all on same chrom.  This is a somewhat fat data
 * structure used by the bigWig creation code.  See also bigWigSection for the
 * structure returned by the bigWig reading code. */
    {
    struct bigWigSect *next;		/* Next in list. */
    char *chrom;			/* Chromosome name. */
    bits32 start,end;			/* Range of chromosome covered. */
    enum bigWigSectionType type;
    union bigWigItem itemList;		/* List of items in this section. */
    bits32 itemStep;			/* Step within item if applicable. */
    bits32 itemSpan;			/* Item span if applicable. */
    bits16 itemCount;			/* Number of items in section. */
    bits32 chromId;			/* Unique small integer value for chromosome. */
    bits64 fileOffset;			/* Offset of section in file. */
    };

struct bigWigSummary
/* A summary type item. */
    {
    struct bigWigSummary *next;
    bits32 chromId;		/* ID of associated chromosome. */
    bits32 start,end;		/* Range of chromosome covered. */
    bits32 validCount;		/* Count of (bases) with actual data. */
    float lowerLimit;		/* Minimum value of items */
    float upperLimit;		/* Maximum value of items */
    float sumData;		/* sum of values for each base. */
    float sumSquares;		/* sum of squares for each base. */
    };

void bigWigDumpSummary(struct bigWigSummary *sum, FILE *f)
/* Write out summary info to file. */
{
fprintf(f, "summary %d:%d-%d min=%f, max=%f, sum=%f, sumSquares=%f, validCount=%d, mean=%f\n",
     sum->chromId, sum->start, sum->end, sum->lowerLimit, sum->upperLimit, sum->sumData,
     sum->sumSquares, sum->validCount, sum->sumData/sum->validCount);
}

void bigWigSectWrite(struct bigWigSect *section, FILE *f)
/* Write out section to file, filling in section->fileOffset. */
{
UBYTE type = section->type;
UBYTE reserved8 = 0;

section->fileOffset = ftell(f);
writeOne(f, section->chromId);
writeOne(f, section->start);
writeOne(f, section->end);
writeOne(f, section->itemStep);
writeOne(f, section->itemSpan);
writeOne(f, type);
writeOne(f, reserved8);
writeOne(f, section->itemCount);

switch (section->type)
    {
    case bigWigTypeBedGraph:
	{
	struct bigWigBedGraphItem *item;
	for (item = section->itemList.bedGraph; item != NULL; item = item->next)
	    {
	    writeOne(f, item->start);
	    writeOne(f, item->end);
	    writeOne(f, item->val);
	    }
	break;
	}
    case bigWigTypeVariableStep:
	{
	struct bigWigVariableStepItem *item;
	for (item = section->itemList.variableStep; item != NULL; item = item->next)
	    {
	    writeOne(f, item->start);
	    writeOne(f, item->val);
	    }
	break;
	}
    case bigWigTypeFixedStep:
	{
	struct bigWigFixedStepItem *item;
	for (item = section->itemList.fixedStep; item != NULL; item = item->next)
	    {
	    writeOne(f, item->val);
	    }
	break;
	break;
	}
    default:
        internalErr();
	break;
    }
}


int bigWigSectCmp(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct bigWigSect *a = *((struct bigWigSect **)va);
const struct bigWigSect *b = *((struct bigWigSect **)vb);
int dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    {
    dif = (int)a->start - (int)b->start;
    if (dif == 0)
	dif = (int)a->end - (int)b->end;
    }
return dif;
}

struct cirTreeRange bigWigSectFetchKey(const void *va, void *context)
/* Fetch bigWigSect key for r-tree */
{
struct cirTreeRange res;
const struct bigWigSect *a = *((struct bigWigSect **)va);
res.chromIx = a->chromId;
res.start = a->start;
res.end = a->end;
return res;
}

bits64 bigWigSectFetchOffset(const void *va, void *context)
/* Fetch bigWigSect file offset for r-tree */
{
const struct bigWigSect *a = *((struct bigWigSect **)va);
return a->fileOffset;
}

void sectionWriteBedGraphAsAscii(struct bigWigSect *section, FILE *f)
/* Write out a bedGraph section. */
{
struct bigWigBedGraphItem *item;
fprintf(f, "#bedGraph chrom=%s start=%u end=%u\n",
	section->chrom, (unsigned)section->start+1, (unsigned)section->end+1);
for (item = section->itemList.bedGraph; item != NULL; item = item->next)
    fprintf(f, "%s\t%u\t%u\t%g\n",  section->chrom, (unsigned)item->start, (unsigned)item->end,
    	item->val);

}

void sectionWriteFixedStepAsAscii(struct bigWigSect *section, FILE *f)
/* Write out a fixedStep section. */
{
struct bigWigFixedStepItem *item;
fprintf(f, "fixedStep chrom=%s start=%u step=%d span=%d\n",
	section->chrom, (unsigned)section->start+1, section->itemStep, section->itemSpan);
for (item = section->itemList.fixedStep; item != NULL; item = item->next)
    fprintf(f, "%g\n", item->val);
}

void sectionWriteVariableStepAsAscii(struct bigWigSect *section, FILE *f)
/* Write out a variableStep section. */
{
struct bigWigVariableStepItem *item;
fprintf(f, "variableStep chrom=%s span=%d\n", section->chrom, section->itemSpan);
for (item = section->itemList.variableStep; item != NULL; item = item->next)
    fprintf(f, "%u\t%g\n", (unsigned)item->start+1, item->val);
}

void sectionWriteAsAscii(struct bigWigSect *section, FILE *f)
/* Write out ascii representation of section (which will be wig-file readable). */
{
switch (section->type)
    {
    case bigWigTypeBedGraph:
	sectionWriteBedGraphAsAscii(section, f);
	break;
    case bigWigTypeVariableStep:
	sectionWriteVariableStepAsAscii(section, f);
	break;
    case bigWigTypeFixedStep:
	sectionWriteFixedStepAsAscii(section, f);
	break;
    default:
        internalErr();
	break;
    }
}

boolean stepTypeLine(char *line)
/* Return TRUE if it's a variableStep or fixedStep line. */
{
return (startsWithWord("variableStep", line) || startsWithWord("fixedStep", line));
}

boolean steppedSectionEnd(char *line, int maxWords)
/* Return TRUE if line indicates the start of another section. */
{
int wordCount = chopByWhite(line, NULL, 5);
if (wordCount > maxWords)
    return TRUE;
return stepTypeLine(line);
}

void parseFixedStepSection(struct lineFile *lf, struct lm *lm,
	char *chrom, bits32 span, bits32 sectionStart, 
	bits32 step, struct bigWigSect **pSectionList)
/* Read the single column data in section until get to end. */
{
/* Stream through section until get to end of file or next section,
 * adding values from single column to list. */
char *words[1];
char *line;
struct bigWigFixedStepItem *item, *itemList = NULL;
while (lineFileNextReal(lf, &line))
    {
    if (steppedSectionEnd(line, 1))
	{
        lineFileReuse(lf);
	break;
	}
    chopLine(line, words);
    lmAllocVar(lm, item);
    item->val = lineFileNeedDouble(lf, words, 0);
    slAddHead(&itemList, item);
    }
slReverse(&itemList);

/* Break up into sections of no more than items-per-slot size. */
struct bigWigFixedStepItem *startItem, *endItem, *nextStartItem = itemList;
for (startItem = nextStartItem; startItem != NULL; startItem = nextStartItem)
    {
    /* Find end item of this section, and start item for next section.
     * Terminate list at end item. */
    int sectionSize = 0;
    int i;
    endItem = startItem;
    for (i=0; i<itemsPerSlot; ++i)
        {
	if (nextStartItem == NULL)
	    break;
	endItem = nextStartItem;
	nextStartItem = nextStartItem->next;
	++sectionSize;
	}
    endItem->next = NULL;

    /* Fill in section and add it to list. */
    struct bigWigSect *section;
    lmAllocVar(lm, section);
    section->chrom = chrom;
    section->start = sectionStart;
    sectionStart += sectionSize * step;
    section->end = sectionStart - step + span;
    section->type = bigWigTypeFixedStep;
    section->itemList.fixedStep = startItem;
    section->itemStep = step;
    section->itemSpan = span;
    section->itemCount = sectionSize;
    slAddHead(pSectionList, section);
    }
}

void parseVariableStepSection(struct lineFile *lf, struct lm *lm,
	char *chrom, bits32 span, struct bigWigSect **pSectionList)
/* Read the single column data in section until get to end. */
{
/* Stream through section until get to end of file or next section,
 * adding values from single column to list. */
char *words[2];
char *line;
struct bigWigVariableStepItem *item, *itemList = NULL;
while (lineFileNextReal(lf, &line))
    {
    if (steppedSectionEnd(line, 2))
	{
        lineFileReuse(lf);
	break;
	}
    chopLine(line, words);
    lmAllocVar(lm, item);
    item->start = lineFileNeedNum(lf, words, 0) - 1;
    item->val = lineFileNeedDouble(lf, words, 1);
    slAddHead(&itemList, item);
    }
slSort(&itemList, bigWigVariableStepItemCmp);

/* Break up into sections of no more than items-per-slot size. */
struct bigWigVariableStepItem *startItem, *endItem, *nextStartItem = itemList;
for (startItem = itemList; startItem != NULL; startItem = nextStartItem)
    {
    /* Find end item of this section, and start item for next section.
     * Terminate list at end item. */
    int sectionSize = 0;
    int i;
    endItem = startItem;
    for (i=0; i<itemsPerSlot; ++i)
        {
	if (nextStartItem == NULL)
	    break;
	endItem = nextStartItem;
	nextStartItem = nextStartItem->next;
	++sectionSize;
	}
    endItem->next = NULL;

    /* Fill in section and add it to list. */
    struct bigWigSect *section;
    lmAllocVar(lm, section);
    section->chrom = chrom;
    section->start = startItem->start;
    section->end = endItem->start + span;
    section->type = bigWigTypeVariableStep;
    section->itemList.variableStep = startItem;
    section->itemSpan = span;
    section->itemCount = sectionSize;
    slAddHead(pSectionList, section);
    }
}

unsigned parseUnsignedVal(struct lineFile *lf, char *var, char *val)
/* Return val as an integer, printing error message if it's not. */
{
char c = val[0];
if (!isdigit(c))
    errAbort("Expecting numerical value for %s, got %s, line %d of %s", 
    	var, val, lf->lineIx, lf->fileName);
return sqlUnsigned(val);
}

void parseSteppedSection(struct lineFile *lf, char *initialLine, 
	struct lm *lm, struct bigWigSect **pSectionList)
/* Parse out a variableStep or fixedStep section and add it to list, breaking it up as need be. */
{
/* Parse out first word of initial line and make sure it is something we recognize. */
char *typeWord = nextWord(&initialLine);
enum bigWigSectionType type = bigWigTypeFixedStep;
if (sameString(typeWord, "variableStep"))
    type = bigWigTypeVariableStep;
else if (sameString(typeWord, "fixedStep"))
    type = bigWigTypeFixedStep;
else
    errAbort("Unknown type %s\n", typeWord);

/* Set up defaults for values we hope to parse out of rest of line. */
int span = 1;
bits32 step = 0;
bits32 start = 0;
char *chrom = NULL;

/* Parse out var=val pairs. */
char *varEqVal;
while ((varEqVal = nextWord(&initialLine)) != NULL)
    {
    char *wordPairs[2];
    int wc = chopByChar(varEqVal, '=', wordPairs, 2);
    if (wc != 2)
        errAbort("strange var=val pair line %d of %s", lf->lineIx, lf->fileName);
    char *var = wordPairs[0];
    char *val = wordPairs[1];
    if (sameString(var, "chrom"))
        chrom = cloneString(val);
    else if (sameString(var, "span"))
	span = parseUnsignedVal(lf, var, val);
    else if (sameString(var, "step"))
	step = parseUnsignedVal(lf, var, val);
    else if (sameString(var, "start"))
        start = parseUnsignedVal(lf, var, val);
    else
	errAbort("Unknown setting %s=%s line %d of %s", var, val, lf->lineIx, lf->fileName);
    }

/* Check that we have all that are required and no more, and call type-specific routine to parse
 * rest of section. */
if (chrom == NULL)
    errAbort("Missing chrom= setting line %d of %s\n", lf->lineIx, lf->fileName);
if (type == bigWigTypeFixedStep)
    {
    if (start == 0)
	errAbort("Missing start= setting line %d of %s\n", lf->lineIx, lf->fileName);
    if (step == 0)
	errAbort("Missing step= setting line %d of %s\n", lf->lineIx, lf->fileName);
    parseFixedStepSection(lf, lm, chrom, span, start-1, step, pSectionList);
    }
else
    {
    if (start != 0)
	errAbort("Extra start= setting line %d of %s\n", lf->lineIx, lf->fileName);
    if (step != 0)
	errAbort("Extra step= setting line %d of %s\n", lf->lineIx, lf->fileName);
    parseVariableStepSection(lf, lm, chrom, span, pSectionList);
    }
}

struct bedGraphChrom
/* A chromosome in bed graph format. */
    {
    struct bedGraphChrom *next;		/* Next in list. */
    char *name;			/* Chromosome name - not allocated here. */
    struct bigWigBedGraphItem *itemList;	/* List of items. */
    };

int bedGraphChromCmpName(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct bedGraphChrom *a = *((struct bedGraphChrom **)va);
const struct bedGraphChrom *b = *((struct bedGraphChrom **)vb);
return strcmp(a->name, b->name);
}

void parseBedGraphSection(struct lineFile *lf, struct lm *lm, struct bigWigSect **pSectionList)
/* Parse out bedGraph section until we get to something that is not in bedGraph format. */
{
/* Set up hash and list to store chromosomes. */
struct hash *chromHash = hashNew(0);
struct bedGraphChrom *chrom, *chromList = NULL;

/* Collect lines in items on appropriate chromosomes. */
struct bigWigBedGraphItem *item, *itemList = NULL;
char *line;
while (lineFileNextReal(lf, &line))
    {
    /* Check for end of section. */
    if (stepTypeLine(line))
        {
	lineFileReuse(lf);
	break;
	}

    /* Parse out our line and make sure it has exactly 4 columns. */
    char *words[5];
    int wordCount = chopLine(line, words);
    lineFileExpectWords(lf, 4, wordCount);

    /* Get chromosome. */
    char *chromName = words[0];
    chrom = hashFindVal(chromHash, chromName);
    if (chrom == NULL)
        {
	lmAllocVar(chromHash->lm, chrom);
	hashAddSaveName(chromHash, chromName, chrom, &chrom->name);
	slAddHead(&chromList, chrom);
	}

    /* Convert to item and add to chromosome list. */
    lmAllocVar(lm, item);
    item->start = lineFileNeedNum(lf, words, 1);
    item->end = lineFileNeedNum(lf, words, 2);
    item->val = lineFileNeedDouble(lf, words, 3);
    slAddHead(&chrom->itemList, item);
    }
slSort(&chromList, bedGraphChromCmpName);

for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    slSort(&chrom->itemList, bigWigBedGraphItemCmp);

    /* Break up into sections of no more than items-per-slot size. */
    struct bigWigBedGraphItem *startItem, *endItem, *nextStartItem = chrom->itemList;
    for (startItem = chrom->itemList; startItem != NULL; startItem = nextStartItem)
	{
	/* Find end item of this section, and start item for next section.
	 * Terminate list at end item. */
	int sectionSize = 0;
	int i;
	endItem = startItem;
	for (i=0; i<itemsPerSlot; ++i)
	    {
	    if (nextStartItem == NULL)
		break;
	    endItem = nextStartItem;
	    nextStartItem = nextStartItem->next;
	    ++sectionSize;
	    }
	endItem->next = NULL;

	/* Fill in section and add it to section list. */
	struct bigWigSect *section;
	lmAllocVar(lm, section);
	section->chrom = cloneString(chrom->name);
	section->start = startItem->start;
	section->end = endItem->end;
	section->type = bigWigTypeBedGraph;
	section->itemList.bedGraph = startItem;
	section->itemCount = sectionSize;
	slAddHead(pSectionList, section);
	}
    }

/* Free up hash, no longer needed. Free's chromList as a side effect since chromList is in 
 * hash's memory. */
hashFree(&chromHash);
chromList = NULL;
}

struct name32
/* Pair of a name and a 32-bit integer. Used to assign IDs to chromosomes. */
    {
    struct name32 *next;
    char *name;
    bits32 val;
    };

static void name32Key(const void *va, char *keyBuf)
/* Get key field. */
{
const struct name32 *a = ((struct name32 *)va);
strcpy(keyBuf, a->name);
}

static void *name32Val(const void *va)
/* Get key field. */
{
const struct name32 *a = ((struct name32 *)va);
return (void*)(&a->val);
}

static void makeChromIds(struct bigWigSect *sectionList,
	int *retChromCount, struct name32 **retChromArray,
	int *retMaxChromNameSize)
/* Fill in chromId field in sectionList.  Return array of chromosome name/ids. */
{
/* Build up list of unique chromosome names. */
struct bigWigSect *section;
char *chromName = "";
int chromCount = 0;
int maxChromNameSize = 0;
struct slRef *uniq, *uniqList = NULL;
for (section = sectionList; section != NULL; section = section->next)
    {
    if (!sameString(section->chrom, chromName))
        {
	chromName = section->chrom;
	refAdd(&uniqList, chromName);
	++chromCount;
	int len = strlen(chromName);
	if (len > maxChromNameSize)
	    maxChromNameSize = len;
	}
    section->chromId = chromCount-1;
    }
slReverse(&uniqList);

/* Allocate and fill in results array. */
struct name32 *chromArray;
AllocArray(chromArray, chromCount);
int i;
for (i = 0, uniq = uniqList; i < chromCount; ++i, uniq = uniq->next)
    {
    chromArray[i].name = uniq->val;
    chromArray[i].val = i;
    }

/* Clean up, set return values and go home. */
slFreeList(&uniqList);
*retChromCount = chromCount;
*retChromArray = chromArray;
*retMaxChromNameSize = maxChromNameSize;
}

int smallestSpan(struct bigWigSect *sectionList)
/* Return smallest span in section list. */
{
if (sectionList == NULL)
    return 1;
int minSpan = sectionList->itemSpan;
struct bigWigSect *section;
for (section = sectionList; section != NULL; section = section->next)
    {
    int sectionSpan = sectionList->itemSpan;
    if (section->type == bigWigTypeBedGraph)
        {
	struct bigWigBedGraphItem *item;
	sectionSpan = BIGNUM;
	for (item = section->itemList.bedGraph; item != NULL; item = item->next)
	    {
	    int size = item->end - item->start;
	    if (sectionSpan > size)
	        sectionSpan = size;
	    }
	}
    if (minSpan > sectionSpan)
        minSpan = sectionSpan;
    }
return minSpan;
}

#define bigWigSectionHeaderSize 24

int bigWigItemSize(enum bigWigSectionType type)
/* Return size of an item inside a particular section. */
{
switch (type)
    {
    case bigWigTypeBedGraph:
	return 2*sizeof(bits32) + sizeof(float);
	break;
    case bigWigTypeVariableStep:
	return sizeof(bits32) + sizeof(float);
	break;
    case bigWigTypeFixedStep:
	return sizeof(float);
	break;
    default:
        internalErr();
	return 0;
    }
}

int bigWigSectSize(struct bigWigSect *section)
/* Return size (on disk) of section. */
{
return bigWigSectionHeaderSize + bigWigItemSize(section->type) * section->itemCount;
}

bits64 bigWigTotalSectSize(struct bigWigSect *sectionList)
/* Return total size of all sections. */
{
bits64 total = 0;
struct bigWigSect *section;
for (section = sectionList; section != NULL; section = section->next)
    total += bigWigSectSize(section);
return total;
}

void addToSummary(bits32 chromId, bits32 start, bits32 end, float val, int reduction,
	struct bigWigSummary **pOutList)
/* Add chromosome range to summary - putting it onto top of list if possible, otherwise
 * expanding list. */
{
struct bigWigSummary *sum = *pOutList;
while (start < end)
    {
    /* See if need to allocate a new summary. */
    if (sum == NULL || sum->chromId != chromId || sum->end <= start)
        {
	struct bigWigSummary *newSum;
	AllocVar(newSum);
	newSum->chromId = chromId;
	if (sum == NULL || sum->chromId != chromId || sum->end + reduction <= start)
	    newSum->start = start;
	else
	    newSum->start = sum->end;
	newSum->end = newSum->start + reduction;
	newSum->lowerLimit = newSum->upperLimit = val;
	sum = newSum;
	slAddHead(pOutList, sum);
	}

    /* Figure out amount of overlap between current summary and item */
    int overlap = rangeIntersection(start, end, sum->start, sum->end);
    assert(overlap > 0);

    /* Fold overlapping bits into output. */
    sum->validCount += overlap;
    if (sum->lowerLimit > val)
        sum->lowerLimit = val;
    if (sum->upperLimit < val)
        sum->upperLimit = val;
    sum->sumData += overlap * val;
    sum->sumSquares += overlap * val * val;

    /* Advance over overlapping bits. */
    start += overlap;
    }
}


void bigWigReduceBedGraph(struct bigWigSect *section, int reduction, 
	struct bigWigSummary **pOutList)
/*Reduce a bedGraph section onto outList. */
{
struct bigWigBedGraphItem *item;
for (item = section->itemList.bedGraph; item != NULL; item = item->next)
    addToSummary(section->chromId, item->start, item->end, item->val, reduction, pOutList);
}

void bigWigReduceVariableStep(struct bigWigSect *section, int reduction, 
	struct bigWigSummary **pOutList)
/*Reduce a variableStep section onto outList. */
{
struct bigWigVariableStepItem *item;
for (item = section->itemList.variableStep; item != NULL; item = item->next)
    addToSummary(section->chromId, item->start, item->start + section->itemSpan, item->val, 
    	reduction, pOutList);
}

void bigWigReduceFixedStep(struct bigWigSect *section, int reduction, 
	struct bigWigSummary **pOutList)
/*Reduce a variableStep section onto outList. */
{
struct bigWigFixedStepItem *item;
int start = section->start;
for (item = section->itemList.fixedStep; item != NULL; item = item->next)
    {
    addToSummary(section->chromId, start, start + section->itemSpan, item->val, 
    	reduction, pOutList);
    start += section->itemStep;
    }
}

struct bigWigSummary *bigWigReduceSectionList(struct bigWigSect *sectionList, int reduction)
/* Reduce section by given amount. */
{
uglyf("bigWigReduceSectionList(%d %d)\n", slCount(sectionList), reduction);
struct bigWigSummary *outList = NULL;
struct bigWigSect *section = NULL;

/* Loop through input section list reducing into outList. */
for (section = sectionList; section != NULL; section = section->next)
    {
    switch (section->type)
        {
	case bigWigTypeBedGraph:
	    bigWigReduceBedGraph(section, reduction, &outList);
	    break;
	case bigWigTypeVariableStep:
	    bigWigReduceVariableStep(section, reduction, &outList);
	    break;
	case bigWigTypeFixedStep:
	    bigWigReduceFixedStep(section, reduction, &outList);
	    break;
	default:
	    internalErr();
	    return 0;
	}
    }

slReverse(&outList);

struct bigWigSummary *sum;
for (sum = outList; sum != NULL; sum = sum->next) bigWigDumpSummary(sum, uglyOut);
return outList;
}

void bigWigCreate(struct bigWigSect *sectionList, char *fileName)
/* Create a bigWig file out of a sorted sectionList. */
{
bits32 sectionCount = slCount(sectionList);
FILE *f = mustOpen(fileName, "wb");
bits32 sig = bigWigSig;
bits32 zoomCount = 0;
bits32 reserved32 = 0;
bits64 dataOffset = 0, dataOffsetPos;
bits64 indexOffset = 0, indexOffsetPos;
bits64 chromTreeOffset = 0, chromTreeOffsetPos;
struct bigWigSummary *zoomSummaries[10];
int i;

/* Write fixed header. */
writeOne(f, sig);
writeOne(f, zoomCount);
chromTreeOffsetPos = ftell(f);
writeOne(f, chromTreeOffset);
dataOffsetPos = ftell(f);
writeOne(f, dataOffset);
indexOffsetPos = ftell(f);
writeOne(f, indexOffset);
for (i=0; i<8; ++i)
    writeOne(f, reserved32);

/* Figure out chromosome ID's. */
struct name32 *chromIdArray;
int chromCount, maxChromNameSize;
makeChromIds(sectionList, &chromCount, &chromIdArray, &maxChromNameSize);

/* Write zoom index. */
int minSpan = smallestSpan(sectionList);
int initialReduction = minSpan*10;
uglyf("minSpan=%d, initialReduction=%d\n", minSpan, initialReduction);
zoomSummaries[0] = bigWigReduceSectionList(sectionList, initialReduction);

/* Write chromosome bPlusTree */
chromTreeOffset = ftell(f);
int chromBlockSize = min(blockSize, chromCount);
bptFileBulkIndexToOpenFile(chromIdArray, sizeof(chromIdArray[0]), chromCount, chromBlockSize,
    name32Key, maxChromNameSize, name32Val, sizeof(chromIdArray[0].val), f);

/* Write out data section count and sections themselves. */
dataOffset = ftell(f);
writeOne(f, sectionCount);
struct bigWigSect *section;
for (section = sectionList; section != NULL; section = section->next)
    bigWigSectWrite(section, f);

/* Write out index - creating a temporary array rather than list representation of
 * sections in the process. */
indexOffset = ftell(f);
struct bigWigSect **sectionArray;
AllocArray(sectionArray, sectionCount);
for (section = sectionList, i=0; section != NULL; section = section->next, ++i)
    sectionArray[i] = section;
cirTreeFileBulkIndexToOpenFile(sectionArray, sizeof(sectionArray[0]), sectionCount,
    blockSize, 1, NULL, bigWigSectFetchKey, bigWigSectFetchOffset, 
    indexOffset - dataOffset, f);
freez(&sectionArray);

/* Go back and fill in offsets properly in header. */
fseek(f, dataOffsetPos, SEEK_SET);
writeOne(f, dataOffset);
fseek(f, indexOffsetPos, SEEK_SET);
writeOne(f, indexOffset);
fseek(f, chromTreeOffsetPos, SEEK_SET);
writeOne(f, chromTreeOffset);


uglyf("chromTreeOffsetPos %llu, chromTreeOffset %llu\n", chromTreeOffsetPos, chromTreeOffset);
uglyf("dataOffsetPos %llu, dataOffset %llu\n", dataOffsetPos, dataOffset);
uglyf("indexOffsetPos %llu, indexOffset %llu\n", indexOffsetPos, indexOffset);
/* Clean up */
freez(&chromIdArray);
carefulClose(&f);
}


void bwTest(char *inName, char *outName)
/* bwTest - Test out some big wig related things. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
char *line;
struct bigWigSect *sectionList = NULL;
struct lm *lm = lmInit(0);
while (lineFileNextReal(lf, &line))
    {
    verbose(2, "processing %s\n", line);
    if (stringIn("chrom=", line))
	parseSteppedSection(lf, line, lm, &sectionList);
    else
        {
	/* Check for bed... */
	char *dupe = cloneString(line);
	char *words[5];
	int wordCount = chopLine(dupe, words);
	if (wordCount != 4)
	    errAbort("Unrecognized line %d of %s:\n%s\n", lf->lineIx, lf->fileName, line);

	/* Parse out a bed graph line just to check numerical format. */
	char *chrom = words[0];
	int start = lineFileNeedNum(lf, words, 1);
	int end = lineFileNeedNum(lf, words, 2);
	float val = lineFileNeedDouble(lf, words, 3);

	/* Push back line and call bed parser. */
	lineFileReuse(lf);
	parseBedGraphSection(lf, lm, &sectionList);
	}
    }

slSort(&sectionList, bigWigSectCmp);

bigWigCreate(sectionList, outName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
blockSize = optionInt("blockSize", blockSize);
itemsPerSlot = optionInt("itemsPerSlot", itemsPerSlot);
if (argc != 3)
    usage();
bwTest(argv[1], argv[2]);
return 0;
}
