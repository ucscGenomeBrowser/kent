/* edwBamStats - Collect some info on a BAM file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "bamFile.h"
#include "hmmstats.h"
#include "bamFile.h"
#include "obscure.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwBamStats - Collect some info on a BAM file\n"
  "usage:\n"
  "   edwBamStats in.bam out.ra\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct targetPos
/* A position in a target sequence */
    {
    struct targetPos *next;
    int targetId;   /* Bam tid */
    unsigned pos;   /* Position. */
    };

int targetPosCmp(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct targetPos *a = *((struct targetPos **)va);
const struct targetPos *b = *((struct targetPos **)vb);
int dif;
dif = a->targetId - b->targetId;
if (dif == 0)
    dif = a->pos - b->pos;
return dif;
}

int countUniqueFromSorted(struct targetPos *tpList)
/* Count the unique number of positions in a sorted list. */
{
int count = 1;
struct targetPos *tp = tpList, *next;
for (;;)
    {
    next = tp->next;
    if (next == NULL)
        break;
    if (next->pos != tp->pos || next->targetId != tp->targetId)
        ++count;
    tp = next;
    }
return count;
}

struct targetPos *initialReduction(struct targetPos *list, double reduceRatio)
/* Do an initial reduction to reduce sort time and memory footprint. */
{
if (reduceRatio >= 1.0)
    return list;
int threshold = RAND_MAX * reduceRatio;
struct targetPos *newList = NULL, *next, *el;
for (el = list; el != NULL; el = next)
    {
    next = el->next;
    if (rand() <= threshold)
        {
	slAddHead(&newList, el);
	}
    }
return newList;
}

struct targetPos *randomSublist(struct targetPos *list, int maxCount)
/* Return a sublist of list with at most maxCount. Destroy list in process */
{
if (list == NULL)
    return list;
int initialCount = slCount(list);
if (initialCount <= maxCount)
    return list;
double reduceRatio = (double)maxCount/initialCount;
if (reduceRatio < 0.9)
    {
    double conservativeReduceRatio = reduceRatio * 1.05;
    list = initialReduction(list, conservativeReduceRatio);
    }
int midCount = slCount(list);
if (midCount <= maxCount)
    return list;
shuffleList(list);
struct targetPos *lastEl = slElementFromIx(list, maxCount-1);
lastEl->next = NULL;
return list;
}

void edwBamStats(char *inBam, char *outRa)
/* edwBamStats - Collect some info on a BAM file. */
{
/* Statistics we'll gather. */
long long mappedCount = 0, uniqueMappedCount = 0;
long long maxReadBases=0, minReadBases=0, readCount=0, sumReadBases=0;
double sumSquaredReadBases = 0.0;

/* List of positions. */
struct lm *lm = lmInit(0);
struct targetPos *tp, *tpList = NULL;

samfile_t *sf = samopen(inBam, "rb", NULL);
bam1_t one;
ZeroVar(&one);	// This seems to be necessary!
for (;;)
    {
    /* Read next record. */
    if (bam_read1(sf->x.bam, &one) < 0)
	break;

    /* Gather read count and length statistics. */
    long long seqSize = one.core.l_qseq;
    if (readCount == 0)
        {
	maxReadBases = minReadBases = seqSize;
	}
    else
	{
	if (maxReadBases < seqSize)
	    maxReadBases = seqSize;
	if (minReadBases > seqSize)
	    minReadBases = seqSize;
	}
    sumReadBases += seqSize;
    sumSquaredReadBases += (seqSize * seqSize);
    ++readCount;

    /* Gather position for uniquely mapped reads. */
    if ((one.core.flag & BAM_FUNMAP) == 0)
        {
	++mappedCount;
	if (one.core.qual > edwMinMapQual)
	    {
	    ++uniqueMappedCount;
	    lmAllocVar(lm, tp);
	    tp->targetId = one.core.tid;
	    tp->pos = one.core.pos;
	    slAddHead(&tpList, tp);
	    }
	}
    }
samclose(sf);
verbose(1, "Scanned %lld reads in %s\n", readCount, inBam);

FILE *f = mustOpen(outRa, "w");
fprintf(f, "readCount %lld\n", readCount);
fprintf(f, "mappedCount %lld\n", mappedCount);
fprintf(f, "uniqueMappedCount %lld\n", uniqueMappedCount);
fprintf(f, "baseCount %lld\n", sumReadBases);
fprintf(f, "readSizeMean %g\n", (double)sumReadBases/readCount);
if (minReadBases != maxReadBases)
    fprintf(f, "readSizeStd %g\n", calcStdFromSums(sumReadBases, sumSquaredReadBases, readCount));
else
    fprintf(f, "readSizeStd 0\n");
fprintf(f, "readSizeMin %lld\n", minReadBases);
fprintf(f, "readSizeMax %lld\n", maxReadBases);
tpList = randomSublist(tpList, 4000000);
slSort(&tpList, targetPosCmp);
int m4ReadCount = slCount(tpList);
fprintf(f, "m4ReadCount %d\n", m4ReadCount);
int m4UniquePos = countUniqueFromSorted(tpList);
fprintf(f, "m4UniquePos %d\n", m4UniquePos);
fprintf(f, "m4UniqueRatio %g\n", (double)m4UniquePos/m4ReadCount);
verbose(1, "m4UniqueRatio %g\n", (double)m4UniquePos/m4ReadCount);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwBamStats(argv[1], argv[2]);
return 0;
}
