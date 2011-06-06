/* bwgCreate - create big wig files.  Implements write side of bwgInternal.h module. 
 * See the comment in bwgInternal.h for a description of the file format. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "errabort.h"
#include "sqlNum.h"
#include "sig.h"
#include "zlibFace.h"
#include "bPlusTree.h"
#include "cirTree.h"
#include "bbiFile.h"
#include "bwgInternal.h"
#include "bigWig.h"

static char const rcsid[] = "$Id: bwgCreate.c,v 1.27 2010/06/10 20:13:29 braney Exp $";

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

void bwgDumpSummary(struct bbiSummary *sum, FILE *f)
/* Write out summary info to file. */
{
fprintf(f, "summary %d:%d-%d min=%f, max=%f, sum=%f, sumSquares=%f, validCount=%d, mean=%f\n",
     sum->chromId, sum->start, sum->end, sum->minVal, sum->maxVal, sum->sumData,
     sum->sumSquares, sum->validCount, sum->sumData/sum->validCount);
}

static int bwgSectionWrite(struct bwgSection *section, boolean doCompress, FILE *f)
/* Write out section to file, filling in section->fileOffset. */
{
UBYTE type = section->type;
UBYTE reserved8 = 0;
int itemSize;
switch (section->type)
    {
    case bwgTypeBedGraph:
        itemSize = 12;
	break;
    case bwgTypeVariableStep:
        itemSize = 8;
	break;
    case bwgTypeFixedStep:
        itemSize = 4;
	break;
    default:
        itemSize = 0;  // Suppress compiler warning
	internalErr();
	break;
    }
int fixedSize = sizeof(section->chromId) + sizeof(section->start) + sizeof(section->end) + 
     sizeof(section->itemStep) + sizeof(section->itemSpan) + sizeof(type) + sizeof(reserved8) +
     sizeof(section->itemCount);
int bufSize = section->itemCount * itemSize + fixedSize;
char buf[bufSize];
char *bufPt = buf;

section->fileOffset = ftell(f);
memWriteOne(&bufPt, section->chromId);
memWriteOne(&bufPt, section->start);
memWriteOne(&bufPt, section->end);
memWriteOne(&bufPt, section->itemStep);
memWriteOne(&bufPt, section->itemSpan);
memWriteOne(&bufPt, type);
memWriteOne(&bufPt, reserved8);
memWriteOne(&bufPt, section->itemCount);

int i;
switch (section->type)
    {
    case bwgTypeBedGraph:
	{
	struct bwgBedGraphItem *item = section->items.bedGraphList;
	for (item = section->items.bedGraphList; item != NULL; item = item->next)
	    {
	    memWriteOne(&bufPt, item->start);
	    memWriteOne(&bufPt, item->end);
	    memWriteOne(&bufPt, item->val);
	    }
	break;
	}
    case bwgTypeVariableStep:
	{
	struct bwgVariableStepPacked *items = section->items.variableStepPacked;
	for (i=0; i<section->itemCount; ++i)
	    {
	    memWriteOne(&bufPt, items->start);
	    memWriteOne(&bufPt, items->val);
	    items += 1;
	    }
	break;
	}
    case bwgTypeFixedStep:
	{
	struct bwgFixedStepPacked *items = section->items.fixedStepPacked;
	for (i=0; i<section->itemCount; ++i)
	    {
	    memWriteOne(&bufPt, items->val);
	    items += 1;
	    }
	break;
	}
    default:
        internalErr();
	break;
    }
assert(bufSize == (bufPt - buf) );

if (doCompress)
    {
    size_t maxCompSize = zCompBufSize(bufSize);
    char compBuf[maxCompSize];
    int compSize = zCompress(buf, bufSize, compBuf, maxCompSize);
    mustWrite(f, compBuf, compSize);
    }
else
    mustWrite(f, buf, bufSize);
return bufSize;
}


int bwgSectionCmp(const void *va, const void *vb)
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

static void parseFixedStepSection(struct lineFile *lf, boolean clipDontDie, struct lm *lm,
	int itemsPerSlot, char *chrom, bits32 chromSize, bits32 span, bits32 sectionStart, 
	bits32 step, struct bwgSection **pSectionList)
/* Read the single column data in section until get to end. */
{
struct lm *lmLocal = lmInit(0);

/* Stream through section until get to end of file or next section,
 * adding values from single column to list. */
char *words[1];
char *line;
struct bwgFixedStepItem *item, *itemList = NULL;
int originalSectionSize = 0;
bits32 sectionEnd = sectionStart;
while (lineFileNextReal(lf, &line))
    {
    if (steppedSectionEnd(line, 1))
	{
        lineFileReuse(lf);
	break;
	}
    chopLine(line, words);
    lmAllocVar(lmLocal, item);
    item->val = lineFileNeedDouble(lf, words, 0);
    if (sectionEnd + span > chromSize)
	{
	warn("line %d of %s: chromosome %s has %u bases, but item ends at %u",
	    lf->lineIx, lf->fileName, chrom, chromSize, sectionEnd + span);
	if (!clipDontDie)
	    noWarnAbort();
	}
    else
	{
	slAddHead(&itemList, item);
	++originalSectionSize;
	}
    sectionEnd += step;
    }
slReverse(&itemList);

/* Break up into sections of no more than items-per-slot size, and convert to packed format. */
int sizeLeft = originalSectionSize;
for (item = itemList; item != NULL; )
    {
    /* Figure out size of this section  */
    int sectionSize = sizeLeft;
    if (sectionSize > itemsPerSlot)
        sectionSize = itemsPerSlot;
    sizeLeft -= sectionSize;


    /* Allocate and fill in section. */
    struct bwgSection *section;
    lmAllocVar(lm, section);
    section->chrom = chrom;
    section->start = sectionStart;
    sectionStart += sectionSize * step;
    section->end = sectionStart - step + span;
    section->type = bwgTypeFixedStep;
    section->itemStep = step;
    section->itemSpan = span;
    section->itemCount = sectionSize;

    /* Allocate array for data, and copy from list to array representation */
    struct bwgFixedStepPacked *packed;		/* An array */
    section->items.fixedStepPacked = lmAllocArray(lm, packed, sectionSize);
    int i;
    for (i=0; i<sectionSize; ++i)
        {
	packed->val = item->val;
	item = item->next;
	++packed;
	}

    /* Add section to list. */
    slAddHead(pSectionList, section);
    }
lmCleanup(&lmLocal);
}

static void parseVariableStepSection(struct lineFile *lf, boolean clipDontDie, struct lm *lm,
	int itemsPerSlot, char *chrom, int chromSize, bits32 span, struct bwgSection **pSectionList)
/* Read the single column data in section until get to end. */
{
struct lm *lmLocal = lmInit(0);

/* Stream through section until get to end of file or next section,
 * adding values from single column to list. */
char *words[2];
char *line;
struct bwgVariableStepItem *item, *nextItem, *itemList = NULL;
int originalSectionSize = 0;
while (lineFileNextReal(lf, &line))
    {
    if (steppedSectionEnd(line, 2))
	{
        lineFileReuse(lf);
	break;
	}
    chopLine(line, words);
    lmAllocVar(lmLocal, item);
    int start = lineFileNeedNum(lf, words, 0);
    if (start <= 0)
	{
	errAbort("line %d of %s: zero or negative chromosome coordinate not allowed",
	    lf->lineIx, lf->fileName);
	}
    item->start = start - 1;
    item->val = lineFileNeedDouble(lf, words, 1);
    if (item->start + span > chromSize)
        {
	warn("line %d of %s: chromosome %s has %u bases, but item ends at %u",
	    lf->lineIx, lf->fileName, chrom, chromSize, item->start + span);
	if (!clipDontDie)
	    noWarnAbort();
	}
    else
        {
	slAddHead(&itemList, item);
	++originalSectionSize;
	}
    }
slSort(&itemList, bwgVariableStepItemCmp);

/* Make sure no overlap between items. */
if (itemList != NULL)
    {
    item = itemList;
    for (nextItem = item->next; nextItem != NULL; nextItem = nextItem->next)
        {
	if (item->start + span > nextItem->start)
	    errAbort("Overlap on %s between items starting at %d and %d.\n"
	             "Please remove overlaps and try again",
		    chrom, item->start, nextItem->start);
	item = nextItem;
	}
    }

/* Break up into sections of no more than items-per-slot size. */
int sizeLeft = originalSectionSize;
for (item = itemList; item != NULL; )
    {
    /* Figure out size of this section  */
    int sectionSize = sizeLeft;
    if (sectionSize > itemsPerSlot)
        sectionSize = itemsPerSlot;
    sizeLeft -= sectionSize;

    /* Convert from list to array representation. */
    struct bwgVariableStepPacked *packed, *p;		
    p = lmAllocArray(lm, packed, sectionSize);
    int i;
    for (i=0; i<sectionSize; ++i)
        {
	p->start = item->start;
	p->val = item->val;
	item = item->next;
	++p;
	}

    /* Fill in section and add it to list. */
    struct bwgSection *section;
    lmAllocVar(lm, section);
    section->chrom = chrom;
    section->start = packed[0].start;
    section->end = packed[sectionSize-1].start + span;
    section->type = bwgTypeVariableStep;
    section->items.variableStepPacked = packed;
    section->itemSpan = span;
    section->itemCount = sectionSize;
    slAddHead(pSectionList, section);
    }
lmCleanup(&lmLocal);
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

static void parseSteppedSection(struct lineFile *lf, boolean clipDontDie, 
	struct hash *chromSizeHash, char *initialLine, 
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
int span = 0;
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
	{
        start = parseUnsignedVal(lf, var, val);
	}
    else
	errAbort("Unknown setting %s=%s line %d of %s", var, val, lf->lineIx, lf->fileName);
    }

/* Check that we have all that are required and no more, and call type-specific routine to parse
 * rest of section. */
if (chrom == NULL)
    errAbort("Missing chrom= setting line %d of %s\n", lf->lineIx, lf->fileName);
bits32 chromSize = (chromSizeHash ? hashIntVal(chromSizeHash, chrom) : BIGNUM);
if (start > chromSize)
    {
    warn("line %d of %s: chromosome %s has %u bases, but item starts at %u",
    	lf->lineIx, lf->fileName, chrom, chromSize, start);
    if (!clipDontDie)
        noWarnAbort();
    }
if (type == bwgTypeFixedStep)
    {
    if (start == 0)
	errAbort("Missing start= setting line %d of %s\n", lf->lineIx, lf->fileName);
    if (step == 0)
	errAbort("Missing step= setting line %d of %s\n", lf->lineIx, lf->fileName);
    if (span == 0)
	span = step;
    parseFixedStepSection(lf, clipDontDie, lm, itemsPerSlot, 
    	chrom, chromSize, span, start-1, step, pSectionList);
    }
else
    {
    if (start != 0)
	errAbort("Extra start= setting line %d of %s\n", lf->lineIx, lf->fileName);
    if (step != 0)
	errAbort("Extra step= setting line %d of %s\n", lf->lineIx, lf->fileName);
    if (span == 0)
	span = 1;
    parseVariableStepSection(lf, clipDontDie, lm, itemsPerSlot, 
    	chrom, chromSize, span, pSectionList);
    }
}

struct bedGraphChrom
/* A chromosome in bed graph format. */
    {
    struct bedGraphChrom *next;		/* Next in list. */
    char *name;			/* Chromosome name - not allocated here. */
    bits32 size;		/* Chromosome size. */
    struct bwgBedGraphItem *itemList;	/* List of items. */
    };

static int bedGraphChromCmpName(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct bedGraphChrom *a = *((struct bedGraphChrom **)va);
const struct bedGraphChrom *b = *((struct bedGraphChrom **)vb);
return strcmp(a->name, b->name);
}

static void parseBedGraphSection(struct lineFile *lf, boolean clipDontDie, 
	struct hash *chromSizeHash, struct lm *lm, 
	int itemsPerSlot, struct bwgSection **pSectionList)
/* Parse out bedGraph section until we get to something that is not in bedGraph format. */
{
/* Set up hash and list to store chromosomes. */
struct hash *chromHash = hashNew(0);
struct bedGraphChrom *chrom, *chromList = NULL;

/* Collect lines in items on appropriate chromosomes. */
struct bwgBedGraphItem *item;
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
	chrom->size = (chromSizeHash ? hashIntVal(chromSizeHash, chromName) : BIGNUM);
	slAddHead(&chromList, chrom);
	}

    /* Convert to item and add to chromosome list. */
    lmAllocVar(lm, item);
    item->start = lineFileNeedNum(lf, words, 1);
    item->end = lineFileNeedNum(lf, words, 2);
    item->val = lineFileNeedDouble(lf, words, 3);

    /* Do sanity checking on coordinates. */
    if (item->start > item->end)
        errAbort("bedGraph error: start (%u) after end line (%u) %d of %s.", 
		item->start, item->end, lf->lineIx, lf->fileName);
    if (item->end > chrom->size)
	{
        warn("bedGraph error line %d of %s: chromosome %s has size %u but item ends at %u",
	        lf->lineIx, lf->fileName, chrom->name, chrom->size, item->end);
	if (!clipDontDie)
	    noWarnAbort();
	}
    else
	{
	slAddHead(&chrom->itemList, item);
	}
    }
slSort(&chromList, bedGraphChromCmpName);

/* Loop through each chromosome and output the item list, broken into sections
 * for that chrom. */
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    slSort(&chrom->itemList, bwgBedGraphItemCmp);

    /* Check to make sure no overlap between items. */
    struct bwgBedGraphItem *item = chrom->itemList, *nextItem;
    for (nextItem = item->next; nextItem != NULL; nextItem = nextItem->next)
        {
	if (item->end > nextItem->start)
	    errAbort("Overlap between %s %d %d and %s %d %d.\nPlease remove overlaps and try again",
	        chrom->name, item->start, item->end, chrom->name, nextItem->start, nextItem->end);
	item = nextItem;
	}

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
	section->items.bedGraphList = startItem;
	section->itemCount = sectionSize;
	slAddHead(pSectionList, section);
	}
    }

/* Free up hash, no longer needed. Free's chromList as a side effect since chromList is in 
 * hash's memory. */
hashFree(&chromHash);
chromList = NULL;
}

void bwgMakeChromInfo(struct bwgSection *sectionList, struct hash *chromSizeHash,
	int *retChromCount, struct bbiChromInfo **retChromArray,
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
struct bbiChromInfo *chromArray;
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
int i;
for (section = sectionList; section != NULL; section = section->next)
    {
    int sectionRes = 0;
    switch (section->type)
        {
	case bwgTypeBedGraph:
	    {
	    struct bwgBedGraphItem *item;
	    sectionRes = BIGNUM;
	    for (item = section->items.bedGraphList; item != NULL; item = item->next)
		{
		int size = item->end - item->start;
		if (sectionRes > size)
		    sectionRes = size;
		}
	    break;
	    }
	case bwgTypeVariableStep:
	    {
	    struct bwgVariableStepPacked *items = section->items.variableStepPacked, *prev;
	    bits32 smallestGap = BIGNUM;
	    for (i=1; i<section->itemCount; ++i)
	        {
		prev = items;
		items += 1;
		bits32 gap = items->start - prev->start;
		if (smallestGap > gap)
		    smallestGap = gap;
		}
	    if (smallestGap != BIGNUM)
	        sectionRes = smallestGap;
	    else
	        sectionRes = section->itemSpan;
	    break;
	    }
	case bwgTypeFixedStep:
	    {
	    sectionRes = section->itemStep;
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

static void bwgReduceBedGraph(struct bwgSection *section, bits32 chromSize, int reduction, 
	struct bbiSummary **pOutList)
/*Reduce a bedGraph section onto outList. */
{
struct bwgBedGraphItem *item = section->items.bedGraphList;
for (item = section->items.bedGraphList; item != NULL; item = item->next)
    {
    bbiAddRangeToSummary(section->chromId, chromSize, item->start, item->end, 
    	item->val, reduction, pOutList);
    }
}

static void bwgReduceVariableStep(struct bwgSection *section, bits32 chromSize, int reduction, 
	struct bbiSummary **pOutList)
/*Reduce a variableStep section onto outList. */
{
struct bwgVariableStepPacked *items = section->items.variableStepPacked;
int i;
for (i=0; i<section->itemCount; ++i)
    {
    bbiAddRangeToSummary(section->chromId, chromSize, 
    	items->start, items->start + section->itemSpan, items->val, reduction, pOutList);
    items += 1;
    }
}

static void bwgReduceFixedStep(struct bwgSection *section, bits32 chromSize, int reduction, 
	struct bbiSummary **pOutList)
/*Reduce a fixedStep section onto outList. */
{
struct bwgFixedStepPacked *items = section->items.fixedStepPacked;
int start = section->start;
int i;
for (i=0; i<section->itemCount; ++i)
    {
    bbiAddRangeToSummary(section->chromId, chromSize, start, start + section->itemSpan, items->val, 
    	reduction, pOutList);
    start += section->itemStep;
    items += 1;
    }
}

struct bbiSummary *bwgReduceSectionList(struct bwgSection *sectionList, 
	struct bbiChromInfo *chromInfoArray, int reduction)
/* Return summary of section list reduced by given amount. */
{
struct bbiSummary *outList = NULL;
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

static void bwgCreate(struct bwgSection *sectionList, struct hash *chromSizeHash, 
	int blockSize, int itemsPerSlot, boolean doCompress, char *fileName)
/* Create a bigWig file out of a sorted sectionList. */
{
bits64 sectionCount = slCount(sectionList);
FILE *f = mustOpen(fileName, "wb");
bits32 sig = bigWigSig;
bits16 version = bbiCurrentVersion;
bits16 summaryCount = 0;
bits16 reserved16 = 0;
bits32 reserved32 = 0;
bits64 reserved64 = 0;
bits64 dataOffset = 0, dataOffsetPos;
bits64 indexOffset = 0, indexOffsetPos;
bits64 chromTreeOffset = 0, chromTreeOffsetPos;
bits64 totalSummaryOffset = 0, totalSummaryOffsetPos;
bits32 uncompressBufSize = 0;
bits64 uncompressBufSizePos;
struct bbiSummary *reduceSummaries[10];
bits32 reductionAmounts[10];
bits64 reductionDataOffsetPos[10];
bits64 reductionDataOffsets[10];
bits64 reductionIndexOffsets[10];
int i;

/* Figure out chromosome ID's. */
struct bbiChromInfo *chromInfoArray;
int chromCount, maxChromNameSize;
bwgMakeChromInfo(sectionList, chromSizeHash, &chromCount, &chromInfoArray, &maxChromNameSize);

/* Figure out initial summary level - starting with a summary 10 times the amount
 * of the smallest item.  See if summarized data is smaller than half input data, if
 * not bump up reduction by a factor of 2 until it is, or until further summarying
 * yeilds no size reduction. */
int  minRes = bwgAverageResolution(sectionList);
int initialReduction = minRes*10;
bits64 fullSize = bwgTotalSectionSize(sectionList);
bits64 maxReducedSize = fullSize/2;
struct bbiSummary *firstSummaryList = NULL, *summaryList = NULL;
bits64 lastSummarySize = 0, summarySize;
for (;;)
    {
    summaryList = bwgReduceSectionList(sectionList, chromInfoArray, initialReduction);
    bits64 summarySize = bbiTotalSummarySize(summaryList);
    if (doCompress)
	{
        summarySize *= 2;	// Compensate for summary not compressing as well as primary data
	}
    if (summarySize >= maxReducedSize && summarySize != lastSummarySize)
        {
	/* Need to do more reduction.  First scale reduction by amount that it missed
	 * being small enough last time, with an extra 10% for good measure.  Then
	 * just to keep from spinning through loop two many times, make sure this is
	 * at least 2x the previous reduction. */
	int nextReduction = 1.1 * initialReduction * summarySize / maxReducedSize;
	if (nextReduction < initialReduction*2)
	    nextReduction = initialReduction*2;
	initialReduction = nextReduction;
	bbiSummaryFreeList(&summaryList);
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
    reduction *= 4;
    if (reduction > 1000000000)
        break;
    summaryList = bbiReduceSummaryList(reduceSummaries[summaryCount-1], chromInfoArray, 
    	reduction);
    summarySize = bbiTotalSummarySize(summaryList);
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
for (i=0; i<2; ++i)
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
writeOne(f, sectionCount);
struct bwgSection *section;
for (section = sectionList; section != NULL; section = section->next)
    {
    bits32 uncSizeOne = bwgSectionWrite(section, doCompress, f);
    if (uncSizeOne > uncompressBufSize)
         uncompressBufSize = uncSizeOne;
    }

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
verbose(2, "bwgCreate writing %d summaries\n", summaryCount);
for (i=0; i<summaryCount; ++i)
    {
    reductionDataOffsets[i] = ftell(f);
    reductionIndexOffsets[i] = bbiWriteSummaryAndIndex(reduceSummaries[i], blockSize, itemsPerSlot, doCompress, f);
    verbose(3, "wrote %d of data, %d of index on level %d\n", (int)(reductionIndexOffsets[i] - reductionDataOffsets[i]), (int)(ftell(f) - reductionIndexOffsets[i]), i);
    }

/* Calculate summary */
struct bbiSummary *sum = firstSummaryList;
if (sum != NULL)
    {
    totalSum.validCount = sum->validCount;
    totalSum.minVal = sum->minVal;
    totalSum.maxVal = sum->maxVal;
    totalSum.sumData = sum->sumData;
    totalSum.sumSquares = sum->sumSquares;
    for (sum = sum->next; sum != NULL; sum = sum->next)
	{
	totalSum.validCount += sum->validCount;
	if (sum->minVal < totalSum.minVal) totalSum.minVal = sum->minVal;
	if (sum->maxVal > totalSum.maxVal) totalSum.maxVal = sum->maxVal;
	totalSum.sumData += sum->sumData;
	totalSum.sumSquares += sum->sumSquares;
	}
    /* Write real summary */
    fseek(f, totalSummaryOffset, SEEK_SET);
    bbiSummaryElementWrite(f, &totalSum);
    }
else
    totalSummaryOffset = 0;	/* Edge case, no summary. */

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

struct bwgSection *bwgParseWig(
	char *fileName,       /* Name of ascii wig file. */
	boolean clipDontDie,  /* Skip items outside chromosome rather than aborting. */
	struct hash *chromSizeHash,  /* If non-NULL items checked to be inside chromosome. */
	int maxSectionSize,   /* Biggest size of a section.  100 - 100,000 is usual range. */
	struct lm *lm)	      /* Memory pool to allocate from. */
/* Parse out ascii wig file - allocating memory in lm. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
struct bwgSection *sectionList = NULL;

/* remove initial browser and track lines */
lineFileRemoveInitialCustomTrackLines(lf);

while (lineFileNextReal(lf, &line))
    {
    verbose(2, "processing %s\n", line);
    if (stringIn("chrom=", line))
	parseSteppedSection(lf, clipDontDie, chromSizeHash, line, lm, maxSectionSize, &sectionList);
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
	double val = lineFileNeedDouble(lf, words, 3);
	verbose(2, "bedGraph %s:%d-%d@%g\n", chrom, start, end, val);

	/* Push back line and call bed parser. */
	lineFileReuse(lf);
	parseBedGraphSection(lf, clipDontDie, chromSizeHash, lm, maxSectionSize, &sectionList);
	}
    }
slSort(&sectionList, bwgSectionCmp);

/* Check for overlap at section level. */
struct bwgSection *section, *nextSection;
for (section = sectionList; section != NULL; section = nextSection)
    {
    nextSection = section->next;
    if (nextSection != NULL)
        {
	if (sameString(section->chrom, nextSection->chrom))
	    {
	    if (section->end > nextSection->start)
	        {
		errAbort("There's more than one value for %s base %d (in coordinates that start with 1).\n",
		    section->chrom, nextSection->start+1);
		}
	    }
	}
    }

return sectionList;
}

void bigWigFileCreate(
	char *inName, 		/* Input file in ascii wiggle format. */
	char *chromSizes, 	/* Two column tab-separated file: <chromosome> <size>. */
	int blockSize,		/* Number of items to bundle in r-tree.  1024 is good. */
	int itemsPerSlot,	/* Number of items in lowest level of tree.  512 is good. */
	boolean clipDontDie,	/* If TRUE then clip items off end of chrom rather than dying. */
	boolean compress,	/* If TRUE then compress data. */
	char *outName)
/* Convert ascii format wig file (in fixedStep, variableStep or bedGraph format) 
 * to binary big wig format. */
{
/* This code needs to agree with code in two other places currently - bigBedFileCreate,
 * and bbiFileOpen.  I'm thinking of refactoring to share at least between
 * bigBedFileCreate and bigWigFileCreate.  It'd be great so it could be structured
 * so that it could send the input in one chromosome at a time, and send in the zoom
 * stuff only after all the chromosomes are done.  This'd potentially reduce the memory
 * footprint by a factor of 2 or 4.  Still, for now it works. -JK */
struct hash *chromSizeHash = bbiChromSizesFromFile(chromSizes);
struct lm *lm = lmInit(0);
struct bwgSection *sectionList = bwgParseWig(inName, clipDontDie, chromSizeHash, itemsPerSlot, lm);
if (sectionList == NULL)
    errAbort("%s is empty of data", inName);
bwgCreate(sectionList, chromSizeHash, blockSize, itemsPerSlot, compress, outName);
lmCleanup(&lm);
}

