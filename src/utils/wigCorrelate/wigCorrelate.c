/* wigCorrelate - Produce a table that correlates all pairs of wigs.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "correlate.h"
#include "bbiFile.h"
#include "bigWig.h"
#include "bwgInternal.h"
#include "errCatch.h"

static char const rcsid[] = "$Id: wigCorrelate.c,v 1.1 2010/05/26 04:43:30 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "wigCorrelate - Produce a table that correlates all pairs of wigs.\n"
  "usage:\n"
  "   wigCorrelate one.wig two.wig ... n.wig\n"
  "This works on bigWig as well as wig files.\n"
  "The output is to stdout\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


void sortedApplyOverlapping(void *aList, void *bList, 
	void *(*advance)(void *v), 	     /* Return next item given current item */
	int (*cmpEnd)(void *a, void *b),	  /* Return logical a->end - b->end */
	int (*overlap)(void *a, void *b),    /* Return overlapping amount (<= 0 for  no overlap) */
	void (*apply)(void *a, void *b, void *context),   
	void *context)
/* Step through aList and bList, which are sorted, and call apply function
 * on each pair that overlaps between lists.  The context parameter is
 * passed to the appy function, but is otherwise unused. */
{
while (aList != NULL && bList != NULL)
    {
    if ( (*overlap)(aList, bList) > 0)
        (*apply)(aList, bList, context);
    if ( (*cmpEnd)(aList, bList) < 0)
        aList = (*advance)(aList);
    else
        bList = (*advance)(bList);
    }
}

void *slListNextWrapper(void *v)
/* Return next position in list if possible.  Return NULL if not. */
{
struct slList *p = v;
if (p == NULL || p->next == NULL)
    return NULL;
return p->next;
}

int bwgSectionOverlap(struct bwgSection *a, struct bwgSection *b)
/* Return amount two sections overlap, or <= 0 for no overlap. */
{
if (!sameString(a->chrom, b->chrom))
    return -1;
return rangeIntersection(a->start, a->end, b->start, b->end);
}

int bwgSectionOverlapWrapper(void *a, void *b)
/* Wrap voidness around bwgSectionOverlap */
{
return bwgSectionOverlap(a, b);
}

int bwgSectionCmpEnd(void *va, void *vb)
/* Wrap voidness around bwgSectionCmp */
{
struct bwgSection *a = va, *b = vb;
return a->end - b->end;
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

void addIntervals(struct bwgSection *section, struct lm *lm, struct bbiInterval **pList)
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

int bbiIntervalCmpEnd(void *va, void *vb)
/* Compare bbiInterval ends. */
{
struct bbiInterval *a = va, *b = vb;
return a->end - b->end;
}

int bbiIntervalOverlap(void *va, void *vb)
/* Return amount two sections overlap, or <= 0 for no overlap. */
{
struct bbiInterval *a = va, *b = vb;
return rangeIntersection(a->start, a->end, b->start, b->end);
}

void bbiIntervalCorrelatePair(struct bbiInterval *a, struct bbiInterval *b, struct correlate *c)
/* Update c with information from bits of a and b that overlap. */
{
int overlap = rangeIntersection(a->start, a->end, b->start, b->end);
assert(overlap > 0);
correlateNextMulti(c, a->val, b->val, overlap);
}

void bbiIntervalCorrelatePairWrapper(void *a, void *b, void *context)
/* Wrap voidness around bwgCorrelatePair */
{
bbiIntervalCorrelatePair(a, b, context);
}

#ifdef OLD
void bwgCorrelatePair(struct bwgSection *a, struct bwgSection *b, struct correlate *c)
/* Update c with information from bits of a and b that overlap. */
{
struct lm *lm = lmInit(0);
struct bbiInterval *aIntervals = sectionToIntervals(a, lm);
struct bbiInterval *bIntervals = sectionToIntervals(b, lm);
sortedApplyOverlapping(aIntervals, bIntervals, slListNextWrapper, 
	bbiIntervalCmpEnd, bbiIntervalOverlap, bbiIntervalCorrelatePairWrapper, c);
lmCleanup(&lm);
}

void bwgCorrelatePairWrapper(void *a, void *b, void *context)
/* Wrap voidness around bwgCorrelatePair */
{
bwgCorrelatePair(a,b, context);
}
#endif /* OLD */

enum metaWigType 
    {
    mwtSections,
    mwtBigWig,
    };

struct metaWig
/* Interface class so that wigs and bigWigs look similar */
    {
    struct metaWig *next;
    enum metaWigType type;

    /* For type mwtSection */
    struct bwgSection *sectionList;
    struct hash *chromHash; /* Hash value is first section in that chrom */

    /* For type mwtBigWig */
    struct bbiFile *bwf;
    };

struct bbiFile *bigWigMayOpen(char *fileName)
/* Wrap error catching around bigWigFileOpen and return NULL if there was
 * a problem. */
{
struct errCatch *errCatch = errCatchNew();
struct bbiFile *bwf = NULL;
if (errCatchStart(errCatch))
    {
    bwf = bigWigFileOpen(fileName);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    bwf = NULL;	// necessary??
return bwf;
}

struct hash *hashSectionChroms(struct bwgSection *sectionList)
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

struct metaWig *metaWigOpen(char *fileName, struct lm *lm)
/* Wrap self around file.  Read all of it if it's wig, just header if bigWig. */
{
struct metaWig *mw;
lmAllocVar(lm, mw);
mw->bwf = bigWigMayOpen(fileName);
if (mw->bwf == NULL)
    {
    mw->sectionList = bwgParseWig(fileName, FALSE, NULL, 512, lm);
    mw->chromHash = hashSectionChroms(mw->sectionList);
    mw->type = mwtSections;
    }
else
    mw->type = mwtBigWig;
return mw;
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

double correlatePair(struct metaWig *a, struct metaWig *b)
/* Correlate a pair of metaWigs. */
{
struct correlate *c = correlateNew();
struct slName *chrom, *chromList = metaWigChromList(a);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct lm *lm = lmInit(0);
    struct bbiInterval *aList = metaIntervalsForChrom(a, chrom->name, lm);
    struct bbiInterval *bList = metaIntervalsForChrom(b, chrom->name, lm);
    sortedApplyOverlapping(aList, bList, slListNextWrapper, 
	    bbiIntervalCmpEnd, bbiIntervalOverlap, bbiIntervalCorrelatePairWrapper, c);
    lmCleanup(&lm);
    }
slFreeList(&chromList);
double result = correlateResult(c);
correlateFree(&c);
return result;
}

void wigCorrelate(int inCount, char **inNames, char *outName)
/* wigCorrelate - Produce a table that correlates all pairs of wigs.. */
{
int i,j;
FILE *f = mustOpen(outName, "w");
uglyTime(NULL);
for (i=0; i<inCount; ++i)
    {
    char *iName = inNames[i];
    struct lm *iLm = lmInit(0);
    struct metaWig *iMeta = metaWigOpen(iName, iLm);
    uglyTime("parsed %s into %p", iName, iMeta);
    for (j=i+1; j<inCount; ++j)
        {
	char *jName = inNames[j];
	struct lm *jLm = lmInit(0);
	struct metaWig *jMeta = metaWigOpen(jName, jLm);
	uglyTime("parsed %s into %p", jName, jMeta);
	fprintf(f, "%s\t%s\t", iName, jName);
	fflush(f);
	double r = correlatePair(iMeta, jMeta);
	fprintf(f, "%g\n", r);
	fflush(f);
	uglyTime("correlated %g from %s and %s", r, iName, jName);
	lmCleanup(&jLm);
	}
    lmCleanup(&iLm);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
wigCorrelate(argc-1, argv+1, "stdout");
return 0;
}
