/* wigCorrelate - Produce a table that correlates all pairs of wigs.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "correlate.h"
#include "bbiFile.h"
#include "metaWig.h"
#include "errCatch.h"
#include "sig.h"

static char const rcsid[] = "$Id: wigCorrelate.c,v 1.7 2010/06/03 17:09:12 kent Exp $";
boolean gotClampMax = FALSE;
double clampMax = 100;

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
  "   -clampMax=N - values larger than this are clipped to this value\n"
  );
}

static struct optionSpec options[] = {
   {"clampMax", OPTION_DOUBLE},
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
 * passed to the apply function, but is otherwise unused. */
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

double clamp(double x)
/* Apply some clamping constraints to x. */
{
if (gotClampMax)
    if (x > clampMax)
        x = clampMax;
return x;
}

void bbiIntervalCorrelatePair(struct bbiInterval *a, struct bbiInterval *b, struct correlate *c)
/* Update c with information from bits of a and b that overlap. */
{
int overlap = rangeIntersection(a->start, a->end, b->start, b->end);
assert(overlap > 0);
correlateNextMulti(c, clamp(a->val), clamp(b->val), overlap);
}

void bbiIntervalCorrelatePairWrapper(void *a, void *b, void *context)
/* Wrap voidness around bwgCorrelatePair */
{
bbiIntervalCorrelatePair(a, b, context);
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
    verbose(2, "%s a(%d) b(%d)\n", chrom->name, slCount(aList), slCount(bList));
    sortedApplyOverlapping(aList, bList, slListNextWrapper, 
	    bbiIntervalCmpEnd, bbiIntervalOverlap, bbiIntervalCorrelatePairWrapper, c);
    lmCleanup(&lm);
    }
slFreeList(&chromList);
double result = correlateResult(c);
verbose(2, "correlate: r %g, sumX %g, sumY %g, n %lld\n", result, c->sumX, c->sumY, c->n);
correlateFree(&c);
return result;
}

void wigCorrelate(int inCount, char **inNames, char *outName)
/* wigCorrelate - Produce a table that correlates all pairs of wigs.. */
{
int i,j;
FILE *f = mustOpen(outName, "w");
verboseTimeInit();
for (i=0; i<inCount-1; ++i)
    {
    char *iName = inNames[i];
    struct metaWig *iMeta = metaWigOpen(iName);
    verboseTime(2, "parsed %s into %p", iName, iMeta);
    for (j=i+1; j<inCount; ++j)
        {
	char *jName = inNames[j];
	struct metaWig *jMeta = metaWigOpen(jName);
	verboseTime(2, "parsed %s into %p", jName, jMeta);
	fprintf(f, "%s\t%s\t", iName, jName);
	fflush(f);
	double r = correlatePair(iMeta, jMeta);
	fprintf(f, "%g\n", r);
	fflush(f);
	verboseTime(2, "correlated %g from %s and %s", r, iName, jName);
	metaWigClose(&jMeta);
	}
    metaWigClose(&iMeta);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
if (optionExists("clampMax"))
    {
    gotClampMax = TRUE;
    clampMax = optionDouble("clampMax", clampMax);
    }
wigCorrelate(argc-1, argv+1, "stdout");
return 0;
}
