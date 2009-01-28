/* bwgCreate - create big wig files.  Implements write side of bwgInternal.h module. 
 * See the comment in bwgInternal.h for a description of the file format. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "sqlNum.h"
#include "sig.h"
#include "bPlusTree.h"
#include "cirTree.h"
#include "bwgInternal.h"
#include "bigWig.h"

static char const rcsid[] = "$Id: bwgCreate.c,v 1.2 2009/01/28 02:44:43 kent Exp $";

static int bwgBedGraphItemCmp(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct bwgBedGraphItem *a = *((struct bwgBedGraphItem **)va);
const struct bwgBedGraphItem *b = *((struct bwgBedGraphItem **)vb);
int dif = (int)a->start - (int)b->start;
if (dif == 0)
    dif = (int)a->end - (int)b->end;
return dif;
}

static int bwgVariableStepItemCmp(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct bwgVariableStepItem *a = *((struct bwgVariableStepItem **)va);
const struct bwgVariableStepItem *b = *((struct bwgVariableStepItem **)vb);
return (int)a->start - (int)b->start;
}

void bwgDumpSummary(struct bwgSummary *sum, FILE *f)
/* Write out summary info to file. */
{
fprintf(f, "summary %d:%d-%d min=%f, max=%f, sum=%f, sumSquares=%f, validCount=%d, mean=%f\n",
     sum->chromId, sum->start, sum->end, sum->minVal, sum->maxVal, sum->sumData,
     sum->sumSquares, sum->validCount, sum->sumData/sum->validCount);
}

static void bwgSectionWrite(struct bwgSection *section, FILE *f)
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
    case bwgTypeBedGraph:
	{
	struct bwgBedGraphItem *item;
	for (item = section->itemList.bedGraph; item != NULL; item = item->next)
	    {
	    writeOne(f, item->start);
	    writeOne(f, item->end);
	    writeOne(f, item->val);
	    }
	break;
	}
    case bwgTypeVariableStep:
	{
	struct bwgVariableStepItem *item;
	for (item = section->itemList.variableStep; item != NULL; item = item->next)
	    {
	    writeOne(f, item->start);
	    writeOne(f, item->val);
	    }
	break;
	}
    case bwgTypeFixedStep:
	{
	struct bwgFixedStepItem *item;
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


static int bwgSectionCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,start,end.  */
{
const struct bwgSection *a = *((struct bwgSection **)va);
const struct bwgSection *b = *((struct bwgSection **)vb);
int dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    {
    dif = (int)a->start - (int)b->start;
    if (dif == 0)
	dif = (int)a->end - (int)b->end;
    }
return dif;
}

static struct cirTreeRange bwgSectionFetchKey(const void *va, void *context)
/* Fetch bwgSection key for r-tree */
{
struct cirTreeRange res;
const struct bwgSection *a = *((struct bwgSection **)va);
res.chromIx = a->chromId;
res.start = a->start;
res.end = a->end;
return res;
}

static bits64 bwgSectionFetchOffset(const void *va, void *context)
/* Fetch bwgSection file offset for r-tree */
{
const struct bwgSection *a = *((struct bwgSection **)va);
return a->fileOffset;
}

static boolean stepTypeLine(char *line)
/* Return TRUE if it's a variableStep or fixedStep line. */
{
return (startsWithWord("variableStep", line) || startsWithWord("fixedStep", line));
}

static boolean steppedSectionEnd(char *line, int maxWords)
/* Return TRUE if line indicates the start of another section. */
{
int wordCount = chopByWhite(line, NULL, 5);
if (wordCount > maxWords)
    return TRUE;
return stepTypeLine(line);
}

static void parseFixedStepSection(struct lineFile *lf, struct lm *lm,
	int itemsPerSlot, char *chrom, bits32 span, bits32 sectionStart, 
	bits32 step, struct bwgSection **pSectionList)
/* Read the single column data in section until get to end. */
{
/* Stream through section until get to end of file or next section,
 * adding values from single column to list. */
char *words[1];
char *line;
struct bwgFixedStepItem *item, *itemList = NULL;
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
struct bwgFixedStepItem *startItem, *endItem, *nextStartItem = itemList;
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
    struct bwgSection *section;
    lmAllocVar(lm, section);
    section->chrom = chrom;
    section->start = sectionStart;
    sectionStart += sectionSize * step;
    section->end = sectionStart - step + span;
    section->type = bwgTypeFixedStep;
    section->itemList.fixedStep = startItem;
    section->itemStep = step;
    section->itemSpan = span;
    section->itemCount = sectionSize;
    slAddHead(pSectionList, section);
    }
}

static void parseVariableStepSection(struct lineFile *lf, struct lm *lm,
	int itemsPerSlot, char *chrom, bits32 span, struct bwgSection **pSectionList)
/* Read the single column data in section until get to end. */
{
/* Stream through section until get to end of file or next section,
 * adding values from single column to list. */
char *words[2];
char *line;
struct bwgVariableStepItem *item, *itemList = NULL;
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
slSort(&itemList, bwgVariableStepItemCmp);

/* Break up into sections of no more than items-per-slot size. */
struct bwgVariableStepItem *startItem, *endItem, *nextStartItem = itemList;
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
    struct bwgSection *section;
    lmAllocVar(lm, section);
    section->chrom = chrom;
    section->start = startItem->start;
    section->end = endItem->start + span;
    section->type = bwgTypeVariableStep;
    section->itemList.variableStep = startItem;
    section->itemSpan = span;
    section->itemCount = sectionSize;
    slAddHead(pSectionList, section);
    }
}

static unsigned parseUnsignedVal(struct lineFile *lf, char *var, char *val)
/* Return val as an integer, printing error message if it's not. */
{
char c = val[0];
if (!isdigit(c))
    errAbort("Expecting numerical value for %s, got %s, line %d of %s", 
    	var, val, lf->lineIx, lf->fileName);
return sqlUnsigned(val);
}

static void parseSteppedSection(struct lineFile *lf, char *initialLine, 
	struct lm *lm, int itemsPerSlot, struct bwgSection **pSectionList)
/* Parse out a variableStep or fixedStep section and add it to list, breaking it up as need be. */
{
/* Parse out first word of initial line and make sure it is something we recognize. */
char *typeWord = nextWord(&initialLine);
enum bwgSectionType type = bwgTypeFixedStep;
if (sameString(typeWord, "variableStep"))
    type = bwgTypeVariableStep;
else if (sameString(typeWord, "fixedStep"))
    type = bwgTypeFixedStep;
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
if (type == bwgTypeFixedStep)
    {
    if (start == 0)
	errAbort("Missing start= setting line %d of %s\n", lf->lineIx, lf->fileName);
    if (step == 0)
	errAbort("Missing step= setting line %d of %s\n", lf->lineIx, lf->fileName);
    parseFixedStepSection(lf, lm, itemsPerSlot, chrom, span, start-1, step, pSectionList);
    }
else
    {
    if (start != 0)
	errAbort("Extra start= setting line %d of %s\n", lf->lineIx, lf->fileName);
    if (step != 0)
	errAbort("Extra step= setting line %d of %s\n", lf->lineIx, lf->fileName);
    parseVariableStepSection(lf, lm, itemsPerSlot, chrom, span, pSectionList);
    }
}

struct bedGraphChrom
/* A chromosome in bed graph format. */
    {
    struct bedGraphChrom *next;		/* Next in list. */
    char *name;			/* Chromosome name - not allocated here. */
    struct bwgBedGraphItem *itemList;	/* List of items. */
    };

static int bedGraphChromCmpName(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct bedGraphChrom *a = *((struct bedGraphChrom **)va);
const struct bedGraphChrom *b = *((struct bedGraphChrom **)vb);
return strcmp(a->name, b->name);
}

static void parseBedGraphSection(struct lineFile *lf, struct lm *lm, 
	int itemsPerSlot, struct bwgSection **pSectionList)
/* Parse out bedGraph section until we get to something that is not in bedGraph format. */
{
/* Set up hash and list to store chromosomes. */
struct hash *chromHash = hashNew(0);
struct bedGraphChrom *chrom, *chromList = NULL;

/* Collect lines in items on appropriate chromosomes. */
struct bwgBedGraphItem *item, *itemList = NULL;
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
    slSort(&chrom->itemList, bwgBedGraphItemCmp);

    /* Break up into sections of no more than items-per-slot size. */
    struct bwgBedGraphItem *startItem, *endItem, *nextStartItem = chrom->itemList;
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
	struct bwgSection *section;
	lmAllocVar(lm, section);
	section->chrom = cloneString(chrom->name);
	section->start = startItem->start;
	section->end = endItem->end;
	section->type = bwgTypeBedGraph;
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

static void chromInfoKey(const void *va, char *keyBuf)
/* Get key field. */
{
const struct bigWigChromInfo *a = ((struct bigWigChromInfo *)va);
strcpy(keyBuf, a->name);
}

static void *chromInfoVal(const void *va)
/* Get val field. */
{
const struct bigWigChromInfo *a = ((struct bigWigChromInfo *)va);
return (void*)(&a->id);
}

void bwgMakeChromInfo(struct bwgSection *sectionList, struct hash *chromSizeHash,
	int *retChromCount, struct bigWigChromInfo **retChromArray,
	int *retMaxChromNameSize)
/* Fill in chromId field in sectionList.  Return array of chromosome name/ids. 
 * The chromSizeHash is keyed by name, and has int values. */
{
/* Build up list of unique chromosome names. */
struct bwgSection *section;
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
struct bigWigChromInfo *chromArray;
AllocArray(chromArray, chromCount);
int i;
for (i = 0, uniq = uniqList; i < chromCount; ++i, uniq = uniq->next)
    {
    chromArray[i].name = uniq->val;
    chromArray[i].id = i;
    chromArray[i].size = hashIntVal(chromSizeHash, uniq->val);
    }

/* Clean up, set return values and go home. */
slFreeList(&uniqList);
*retChromCount = chromCount;
*retChromArray = chromArray;
*retMaxChromNameSize = maxChromNameSize;
}

int bwgAverageResolution(struct bwgSection *sectionList)
/* Return the average resolution seen in sectionList. */
{
if (sectionList == NULL)
    return 1;
bits64 totalRes = 0;
bits32 sectionCount = 0;
struct bwgSection *section;
for (section = sectionList; section != NULL; section = section->next)
    {
    int sectionRes = sectionList->itemSpan;
    switch (section->type)
        {
	case bwgTypeBedGraph:
	    {
	    struct bwgBedGraphItem *item;
	    sectionRes = BIGNUM;
	    for (item = section->itemList.bedGraph; item != NULL; item = item->next)
		{
		int size = item->end - item->start;
		if (sectionRes > size)
		    sectionRes = size;
		}
	    break;
	    }
	case bwgTypeVariableStep:
	    {
	    struct bwgVariableStepItem *item, *next;
	    bits32 smallestGap = BIGNUM;
	    for (item = section->itemList.variableStep; item != NULL; item = next)
		{
		next = item->next;
		if (next != NULL)
		    {
		    bits32 gap = next->start - item->start;
		    if (smallestGap > gap)
		        smallestGap = gap;
		    }
		}
	    if (smallestGap != BIGNUM)
	        sectionRes = smallestGap;
	    else
	        sectionRes = section->itemSpan;
	    break;
	    }
	case bwgTypeFixedStep:
	    {
	    sectionRes = section->itemSpan + section->itemStep;
	    break;
	    }
	default:
	    internalErr();
	    break;
	}
    totalRes += sectionRes;
    ++sectionCount;
    }
return (totalRes + sectionCount/2)/sectionCount;
}

#define bwgSectionHeaderSize 24	/* Size of section header in file. */

static int bwgItemSize(enum bwgSectionType type)
/* Return size of an item inside a particular section. */
{
switch (type)
    {
    case bwgTypeBedGraph:
	return 2*sizeof(bits32) + sizeof(float);
	break;
    case bwgTypeVariableStep:
	return sizeof(bits32) + sizeof(float);
	break;
    case bwgTypeFixedStep:
	return sizeof(float);
	break;
    default:
        internalErr();
	return 0;
    }
}

static int bwgSectionSize(struct bwgSection *section)
/* Return size (on disk) of section. */
{
return bwgSectionHeaderSize + bwgItemSize(section->type) * section->itemCount;
}

static bits64 bwgTotalSectionSize(struct bwgSection *sectionList)
/* Return total size of all sections. */
{
bits64 total = 0;
struct bwgSection *section;
for (section = sectionList; section != NULL; section = section->next)
    total += bwgSectionSize(section);
return total;
}

static bits64 bwgTotalSummarySize(struct bwgSummary *list)
/* Return size on disk of all summaries. */
{
struct bwgSummary *el;
bits64 total = 0;
for (el = list; el != NULL; el = el->next)
    total += 4*sizeof(bits32) + 4*sizeof(double);
return total;
}

static void addToSummary(bits32 chromId, bits32 chromSize, bits32 start, bits32 end, 
	bits32 validCount, float minVal, float maxVal, float sumData, float sumSquares,  
	int reduction, struct bwgSummary **pOutList)
/* Add data range to summary - putting it onto top of list if possible, otherwise
 * expanding list. */
{
struct bwgSummary *sum = *pOutList;
while (start < end)
    {
    /* See if need to allocate a new summary. */
    if (sum == NULL || sum->chromId != chromId || sum->end <= start)
        {
	struct bwgSummary *newSum;
	AllocVar(newSum);
	newSum->chromId = chromId;
	if (sum == NULL || sum->chromId != chromId || sum->end + reduction <= start)
	    newSum->start = start;
	else
	    newSum->start = sum->end;
	newSum->end = newSum->start + reduction;
	if (newSum->end > chromSize)
	    newSum->end = chromSize;
	newSum->minVal = minVal;
	newSum->maxVal = maxVal;
	sum = newSum;
	slAddHead(pOutList, sum);
	}

    /* Figure out amount of overlap between current summary and item */
    int overlap = rangeIntersection(start, end, sum->start, sum->end);
    if (overlap <= 0) 
	{
        warn("%u %u doesn't intersect %u %u, chromId %d", start, end, sum->start, sum->end, chromId);
	internalErr();
	}
    int itemSize = end - start;
    float overlapFactor = (float)overlap/itemSize;

    /* Fold overlapping bits into output. */
    sum->validCount += overlapFactor * validCount;
    if (sum->minVal > minVal)
        sum->minVal = minVal;
    if (sum->maxVal < maxVal)
        sum->maxVal = maxVal;
    sum->sumData += overlapFactor * sumData;
    sum->sumSquares += overlapFactor * sumSquares;

    /* Advance over overlapping bits. */
    start += overlap;
    }
}

static void addRangeToSummary(bits32 chromId, bits32 chromSize, bits32 start, bits32 end, 
	float val, int reduction, struct bwgSummary **pOutList)
/* Add chromosome range to summary - putting it onto top of list if possible, otherwise
 * expanding list. */
{
int size = end-start;
float sum = size*val;
float sumSquares = sum*val;
addToSummary(chromId, chromSize, start, end, size, val, val, sum, sumSquares, reduction, pOutList);
}

static void bwgReduceBedGraph(struct bwgSection *section, bits32 chromSize, int reduction, 
	struct bwgSummary **pOutList)
/*Reduce a bedGraph section onto outList. */
{
struct bwgBedGraphItem *item;
for (item = section->itemList.bedGraph; item != NULL; item = item->next)
    addRangeToSummary(section->chromId, chromSize, item->start, item->end, 
    	item->val, reduction, pOutList);
}

static void bwgReduceVariableStep(struct bwgSection *section, bits32 chromSize, int reduction, 
	struct bwgSummary **pOutList)
/*Reduce a variableStep section onto outList. */
{
struct bwgVariableStepItem *item;
for (item = section->itemList.variableStep; item != NULL; item = item->next)
    addRangeToSummary(section->chromId, chromSize, item->start, item->start + section->itemSpan, 
    	item->val, reduction, pOutList);
}

static void bwgReduceFixedStep(struct bwgSection *section, bits32 chromSize, int reduction, 
	struct bwgSummary **pOutList)
/*Reduce a variableStep section onto outList. */
{
struct bwgFixedStepItem *item;
int start = section->start;
for (item = section->itemList.fixedStep; item != NULL; item = item->next)
    {
    addRangeToSummary(section->chromId, chromSize, start, start + section->itemSpan, item->val, 
    	reduction, pOutList);
    start += section->itemStep;
    }
}

struct bwgSummary *bwgReduceSummaryList(struct bwgSummary *inList, 
	struct bigWigChromInfo *chromInfoArray, int reduction)
/* Reduce summary list to another summary list. */
{
struct bwgSummary *outList = NULL;
struct bwgSummary *sum;
for (sum = inList; sum != NULL; sum = sum->next)
    addToSummary(sum->chromId, chromInfoArray[sum->chromId].size, sum->start, sum->end, sum->validCount, sum->minVal,
    	sum->maxVal, sum->sumData, sum->sumSquares, reduction, &outList);
slReverse(&outList);
return outList;
}

struct bwgSummary *bwgReduceSectionList(struct bwgSection *sectionList, 
	struct bigWigChromInfo *chromInfoArray, int reduction)
/* Reduce section by given amount. */
{
struct bwgSummary *outList = NULL;
struct bwgSection *section = NULL;

/* Loop through input section list reducing into outList. */
for (section = sectionList; section != NULL; section = section->next)
    {
    bits32 chromSize = chromInfoArray[section->chromId].size;
    switch (section->type)
        {
	case bwgTypeBedGraph:
	    bwgReduceBedGraph(section, chromSize, reduction, &outList);
	    break;
	case bwgTypeVariableStep:
	    bwgReduceVariableStep(section, chromSize, reduction, &outList);
	    break;
	case bwgTypeFixedStep:
	    bwgReduceFixedStep(section, chromSize, reduction, &outList);
	    break;
	default:
	    internalErr();
	    return 0;
	}
    }
slReverse(&outList);
return outList;
}

static bits64 bwgSummaryFetchOffset(const void *va, void *context)
/* Fetch bwgSummary file offset for r-tree */
{
const struct bwgSummary *a = *((struct bwgSummary **)va);
return a->fileOffset;
}

static struct cirTreeRange bwgSummaryFetchKey(const void *va, void *context)
/* Fetch bwgSummary key for r-tree */
{
struct cirTreeRange res;
const struct bwgSummary *a = *((struct bwgSummary **)va);
res.chromIx = a->chromId;
res.start = a->start;
res.end = a->end;
return res;
}

static bits64 writeSummaryAndIndex(struct bwgSummary *summaryList, 
	int blockSize, int itemsPerSlot, FILE *f)
/* Write out summary and index to summary, returning start position of
 * summary index. */
{
bits32 i, count = slCount(summaryList);
struct bwgSummary **summaryArray;
AllocArray(summaryArray, count);
writeOne(f, count);
bits64 dataOffset = ftell(f);
struct bwgSummary *summary;
for (summary = summaryList, i=0; summary != NULL; summary = summary->next, ++i)
    {
    summaryArray[i] = summary;
    summary->fileOffset = ftell(f);
    writeOne(f, summary->chromId);
    writeOne(f, summary->start);
    writeOne(f, summary->end);
    writeOne(f, summary->validCount);
    writeOne(f, summary->minVal);
    writeOne(f, summary->maxVal);
    writeOne(f, summary->sumData);
    writeOne(f, summary->sumSquares);
    }
bits64 indexOffset = ftell(f);
cirTreeFileBulkIndexToOpenFile(summaryArray, sizeof(summaryArray[0]), count,
    blockSize, itemsPerSlot, NULL, bwgSummaryFetchKey, bwgSummaryFetchOffset, 
    indexOffset, f);
freez(&summaryArray);
return indexOffset;
}

static void bwgCreate(struct bwgSection *sectionList, struct hash *chromSizeHash, 
	int blockSize, int itemsPerSlot, char *fileName)
/* Create a bigWig file out of a sorted sectionList. */
{
bits32 sectionCount = slCount(sectionList);
FILE *f = mustOpen(fileName, "wb");
bits32 sig = bigWigSig;
bits32 summaryCount = 0;
bits32 reserved32 = 0;
bits64 reserved64 = 0;
bits64 dataOffset = 0, dataOffsetPos;
bits64 indexOffset = 0, indexOffsetPos;
bits64 chromTreeOffset = 0, chromTreeOffsetPos;
struct bwgSummary *reduceSummaries[10];
bits32 reductionAmounts[10];
bits64 reductionDataOffsetPos[10];
bits64 reductionDataOffsets[10];
bits64 reductionIndexOffsets[10];
int i;

/* Figure out chromosome ID's. */
struct bigWigChromInfo *chromInfoArray;
int chromCount, maxChromNameSize;
bwgMakeChromInfo(sectionList, chromSizeHash, &chromCount, &chromInfoArray, &maxChromNameSize);

/* Figure out initial summary level - starting with a summary 10 times the amount
 * of the smallest item.  See if summarized data is smaller than input data, if
 * not bump up reduction by a factor of 2 until it is, or until further summarying
 * yeilds no size reduction. */
int  minRes = bwgAverageResolution(sectionList);
int initialReduction = minRes*10;
bits64 fullSize = bwgTotalSectionSize(sectionList);
struct bwgSummary *firstSummaryList = NULL, *summaryList = NULL;
bits64 lastSummarySize = 0, summarySize;
for (;;)
    {
    summaryList = bwgReduceSectionList(sectionList, chromInfoArray, initialReduction);
    bits64 summarySize = bwgTotalSummarySize(summaryList);
    if (summarySize >= fullSize && summarySize != lastSummarySize)
        {
	initialReduction *= 2;
	bwgSummaryFreeList(&summaryList);
	lastSummarySize = summarySize;
	}
    else
        break;
    }
summaryCount = 1;
reduceSummaries[0] = firstSummaryList = summaryList;
reductionAmounts[0] = initialReduction;

/* Now calculate up to 10 levels of further summary. */
bits64 reduction = initialReduction;
for (i=0; i<ArraySize(reduceSummaries)-1; i++)
    {
    reduction *= 10;
    if (reduction > 1000000000)
        break;
    summaryList = bwgReduceSummaryList(reduceSummaries[summaryCount-1], chromInfoArray, 
    	reduction);
    summarySize = bwgTotalSummarySize(summaryList);
    if (summarySize != lastSummarySize)
        {
 	reduceSummaries[summaryCount] = summaryList;
	reductionAmounts[summaryCount] = reduction;
	++summaryCount;
	}
    int summaryItemCount = slCount(summaryList);
    if (summaryItemCount <= chromCount)
        break;
    }

/* Write fixed header. */
writeOne(f, sig);
writeOne(f, summaryCount);
chromTreeOffsetPos = ftell(f);
writeOne(f, chromTreeOffset);
dataOffsetPos = ftell(f);
writeOne(f, dataOffset);
indexOffsetPos = ftell(f);
writeOne(f, indexOffset);
for (i=0; i<8; ++i)
    writeOne(f, reserved32);

/* Write summary headers */
for (i=0; i<summaryCount; ++i)
    {
    writeOne(f, reductionAmounts[i]);
    writeOne(f, reserved32);
    reductionDataOffsetPos[i] = ftell(f);
    writeOne(f, reserved64);	// Fill in with data offset later
    writeOne(f, reserved64);	// Fill in with index offset later
    }

/* Write chromosome bPlusTree */
chromTreeOffset = ftell(f);
int chromBlockSize = min(blockSize, chromCount);
bptFileBulkIndexToOpenFile(chromInfoArray, sizeof(chromInfoArray[0]), chromCount, chromBlockSize,
    chromInfoKey, maxChromNameSize, chromInfoVal, 
    sizeof(chromInfoArray[0].id) + sizeof(chromInfoArray[0].size), 
    f);

/* Write out data section count and sections themselves. */
dataOffset = ftell(f);
writeOne(f, sectionCount);
struct bwgSection *section;
for (section = sectionList; section != NULL; section = section->next)
    bwgSectionWrite(section, f);

/* Write out index - creating a temporary array rather than list representation of
 * sections in the process. */
indexOffset = ftell(f);
struct bwgSection **sectionArray;
AllocArray(sectionArray, sectionCount);
for (section = sectionList, i=0; section != NULL; section = section->next, ++i)
    sectionArray[i] = section;
cirTreeFileBulkIndexToOpenFile(sectionArray, sizeof(sectionArray[0]), sectionCount,
    blockSize, 1, NULL, bwgSectionFetchKey, bwgSectionFetchOffset, 
    indexOffset, f);
freez(&sectionArray);

/* Write out summary sections. */
for (i=0; i<summaryCount; ++i)
    {
    reductionDataOffsets[i] = ftell(f);
    reductionIndexOffsets[i] = writeSummaryAndIndex(reduceSummaries[i], blockSize, itemsPerSlot, f);
    }

/* Go back and fill in offsets properly in header. */
fseek(f, dataOffsetPos, SEEK_SET);
writeOne(f, dataOffset);
fseek(f, indexOffsetPos, SEEK_SET);
writeOne(f, indexOffset);
fseek(f, chromTreeOffsetPos, SEEK_SET);
writeOne(f, chromTreeOffset);

/* Also fill in offsets in zoom headers. */
for (i=0; i<summaryCount; ++i)
    {
    fseek(f, reductionDataOffsetPos[i], SEEK_SET);
    writeOne(f, reductionDataOffsets[i]);
    writeOne(f, reductionIndexOffsets[i]);
    }

/* Clean up */
freez(&chromInfoArray);
carefulClose(&f);
}

struct hash *bwgChromSizesFromFile(char *fileName)
/* Read two column file into hash keyed by chrom. */
{
struct hash *hash = hashNew(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
while (lineFileRow(lf, row))
    hashAddInt(hash, row[0], sqlUnsigned(row[1]));
return hash;
}

struct bwgSection *bwgParseWig(char *fileName, int maxSectionSize, struct lm *lm)
/* Parse out ascii wig file - allocating memory in lm. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
struct bwgSection *sectionList = NULL;
while (lineFileNextReal(lf, &line))
    {
    verbose(2, "processing %s\n", line);
    if (stringIn("chrom=", line))
	parseSteppedSection(lf, line, lm, maxSectionSize, &sectionList);
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
	parseBedGraphSection(lf, lm, maxSectionSize, &sectionList);
	}
    }
slSort(&sectionList, bwgSectionCmp);
return sectionList;
}

void bigWigFileCreate(
	char *inName, 		/* Input file in ascii wiggle format. */
	char *chromSizes, 	/* Two column tab-separated file: <chromosome> <size>. */
	int blockSize,		/* Number of items to bundle in r-tree.  1024 is good. */
	int itemsPerSlot,	/* Number of items in lowest level of tree.  512 is good. */
	char *outName)
/* Convert ascii format wig file (in fixedStep, variableStep or bedGraph format) 
 * to binary big wig format. */
{
struct hash *chromSizeHash = bwgChromSizesFromFile(chromSizes);
struct lm *lm = lmInit(0);
struct bwgSection *sectionList = bwgParseWig(inName, itemsPerSlot, lm);
bwgCreate(sectionList, chromSizeHash, blockSize, itemsPerSlot, outName);
lmCleanup(&lm);
}

