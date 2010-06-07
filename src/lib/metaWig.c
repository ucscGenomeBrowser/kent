#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "localmem.h"
#include "bigWig.h"
#include "bwgInternal.h"
#include "metaWig.h"

static struct hash *hashSectionChroms(struct bwgSection *sectionList)
/* Return hash keyed by chromosome name with values that are the
 * first section in the chromosome. */
{
struct hash *hash = hashNew(0);
struct bwgSection *section;
for (section = sectionList; section != NULL; section = section->next)
    {
    if (!hashLookup(hash, section->chrom))
        hashAdd(hash, section->chrom, section);
    }
return hash;
}

struct metaWig *metaWigOpen(char *fileName)
/* Wrap self around file.  Read all of it if it's wig, just header if bigWig. */
{
struct metaWig *mw;
AllocVar(mw);
if (isBigWig(fileName))
    {
    mw->type = mwtBigWig;
    mw->bwf = bigWigFileOpen(fileName);
    }
else
    {
    mw->lm = lmInit(0);
    mw->sectionList = bwgParseWig(fileName, FALSE, NULL, 512, mw->lm);
    mw->chromHash = hashSectionChroms(mw->sectionList);
    mw->type = mwtSections;
    }
return mw;
}

void metaWigClose(struct metaWig **pMw)
/* Close up metaWig file */
{
struct metaWig *mw = *pMw;
if (mw != NULL)
    {
    bigWigFileClose(&mw->bwf);
    lmCleanup(&mw->lm);
    freez(pMw);
    }
}

struct slName *metaWigChromList(struct metaWig *mw)
/* Return list of chromosomes covered in wig. */
{
struct slName *list = NULL;
switch (mw->type)
    {
    case mwtSections:
        {
	struct hashEl *chrom, *chromList = hashElListHash(mw->chromHash);
	for (chrom = chromList; chrom != NULL; chrom = chrom->next)
	    slNameAddHead(&list, chrom->name);
	hashElFreeList(&chromList);
	break;
	}
    case mwtBigWig:
        {
	struct bbiChromInfo *chrom, *chromList = bbiChromList(mw->bwf);
	for (chrom = chromList; chrom != NULL; chrom = chrom->next)
	    slNameAddHead(&list, chrom->name);
	bbiChromInfoFreeList(&chromList);
	break;
	}
    }
slSort(&list, slNameCmpStringsWithEmbeddedNumbers);
return list;
}

static void bedGraphSectionToIntervals(struct bwgSection *section, struct lm *lm,
	struct bbiInterval **pList)
/* Convert bedGraph section to interval format. */
{
struct bwgBedGraphItem *item;
for (item = section->items.bedGraphList; item != NULL; item = item->next)
    {
    struct bbiInterval *el;
    lmAllocVar(lm, el);
    el->start = item->start;
    el->end = item->end;
    el->val = item->val;
    slAddHead(pList, el);
    }
}

static void varStepSectionToIntervals(struct bwgSection *section, struct lm *lm,
	struct bbiInterval **pList)
/* Convert bedGraph section to interval format. */
{
struct bwgVariableStepPacked *array = section->items.variableStepPacked;
int i, count = section->itemCount;
bits32 span = section->itemSpan;
for (i=0; i<count; ++i)
    {
    struct bwgVariableStepPacked *item = &array[i];
    struct bbiInterval *el;
    lmAllocVar(lm, el);
    el->start = item->start;
    el->end = item->start + span;
    el->val = item->val;
    slAddHead(pList, el);
    }
}

static void fixedStepSectionToIntervals(struct bwgSection *section, struct lm *lm,
	struct bbiInterval **pList)
/* Convert bedGraph section to interval format. */
{
struct bwgFixedStepPacked *array = section->items.fixedStepPacked;
int i, count = section->itemCount;
bits32 span = section->itemSpan;
bits32 step = section->itemStep;
bits32 start = section->start;
for (i=0; i<count; ++i)
    {
    struct bwgFixedStepPacked *item = &array[i];
    struct bbiInterval *el;
    lmAllocVar(lm, el);
    el->start = start;
    el->end = start + span;
    el->val = item->val;
    start += step;
    slAddHead(pList, el);
    }
}

static void addIntervals(struct bwgSection *section, struct lm *lm, struct bbiInterval **pList)
/* Convert section to list of allocated in lm */
{
switch (section->type)
    {
    case bwgTypeBedGraph:
        bedGraphSectionToIntervals(section, lm, pList);
        break;
    case bwgTypeVariableStep:
	varStepSectionToIntervals(section, lm, pList);
        break;
    case bwgTypeFixedStep:
	fixedStepSectionToIntervals(section, lm, pList);
	break;
    default:
        errAbort("unknown section type %d in addIntervals", section->type);
	break;
    }
}

struct bbiInterval *metaIntervalsForChrom(struct metaWig *mw, char *chrom, struct lm *lm)
/* Get sorted list of all intervals with data on chromosome. */
{
struct bbiInterval *list = NULL;
switch (mw->type)
    {
    case mwtSections:
        {
	struct bwgSection *section, *sectionList = hashFindVal(mw->chromHash, chrom);
	for (section = sectionList; section != NULL; section = section->next)
	   {
	   if (!sameString(section->chrom, chrom))
	       break;
	   addIntervals(section, lm, &list);
	   }
	slReverse(&list);
	break;
	}
    case mwtBigWig:
        {
	list = bigWigIntervalQuery(mw->bwf, chrom, 0, bbiChromSize(mw->bwf, chrom), lm);
	break;
	}
    }
return list;
}

