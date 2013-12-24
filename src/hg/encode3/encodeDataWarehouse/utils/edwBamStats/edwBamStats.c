/* edwBamStats - Collect some info on a BAM file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "bamFile.h"
#include "hmmstats.h"
#include "bamFile.h"
#include "md5.h"
#include "obscure.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

int sampleBedSize = 250000;
char *sampleBed = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwBamStats - Collect some info on a BAM file\n"
  "usage:\n"
  "   edwBamStats in.bam out.ra\n"
  "options:\n"
  "   -sampleBed=file.bed\n"
  "   -sampleBedSize=N Max # of items sampleBed, default %d\n"
  , sampleBedSize
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"sampleBed", OPTION_STRING},
   {"sampleBedSize", OPTION_INT},
   {NULL, 0},
};

struct targetPos
/* A position in a target sequence */
    {
    struct targetPos *next;
    int targetId;   /* Bam tid */
    unsigned pos;   /* Position. */
    bits16 size;    /* Read size. */
    char strand;    /* + or - */
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
if (dif == 0)
    dif = a->strand - b->strand;
return dif;
}

int targetPosCmpNoStrand(const void *va, const void *vb)
/* Compare to sort based on query start ignoring strand. */
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
    if (next->pos != tp->pos || next->targetId != tp->targetId || next->strand != tp->strand)
        ++count;
    tp = next;
    }
return count;
}

int uint32_tCmp(const void *va, const void *vb)
/* Compare function to sort array of ints. */
{
const uint32_t *pa = va;
const uint32_t *pb = vb;
uint32_t a = *pa, b = *pb;
if (a < b)
    return -1;
else if (a > b)
    return 1;
else
    return 0;
}

typedef char *CharPt;

int charPtCmp(const void *va, const void *vb)
/* Compare function to sort array of ints. */
{
const CharPt *pa = va;
const CharPt *pb = vb;
return strcmp(*pa, *pb);
}

void edwBamStats(char *inBam, char *outRa)
/* edwBamStats - Collect some info on a BAM file. */
{
/* Statistics we'll gather. */
long long mappedCount = 0, uniqueMappedCount = 0;
long long maxReadBases=0, minReadBases=0, readCount=0, sumReadBases=0;
double sumSquaredReadBases = 0.0;
boolean sortedByChrom = TRUE, isPaired = FALSE;


/* List of positions. */
struct lm *lm = lmInit(0);
struct targetPos *tp, *tpList = NULL;

/* Open file and get header for it. */
samfile_t *sf = samopen(inBam, "rb", NULL);
if (sf == NULL)
    errnoAbort("Couldn't open %s.\n", inBam);
bam_header_t *head = sf->header;
if (head == NULL)
    errAbort("Aborting ... Bad BAM header in file: %s", inBam);

/* Start with some processing on the headers.  Get sorted versions of them. */
uint32_t *sortedSizes = CloneArray(head->target_len, head->n_targets);
qsort(sortedSizes, head->n_targets, sizeof(sortedSizes[0]), uint32_tCmp);
char **sortedNames = CloneArray(head->target_name, head->n_targets);
qsort(sortedNames, head->n_targets, sizeof(sortedNames[0]), charPtCmp);

/* Sum up some target into in 2 hex md5s by first building up string of info */
long long targetBaseCount = 0;   /* Total size of all bases in target seq */
int i;
for (i=0; i<head->n_targets; ++i)
    {
    targetBaseCount  += head->target_len[i];
    }

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
	    tp->size = one.core.l_qseq;
	    tp->strand = ((one.core.flag & BAM_FREVERSE) ? '-' : '+');
	    if (tpList != NULL && targetPosCmpNoStrand(&tpList, &tp) > 0)
	        sortedByChrom = FALSE; 
	    slAddHead(&tpList, tp);
	    }
	}
    if (one.core.flag & BAM_FPAIRED)
        isPaired = TRUE;
    }
verbose(1, "Scanned %lld reads in %s\n", readCount, inBam);


FILE *f = mustOpen(outRa, "w");
fprintf(f, "isPaired %d\n", isPaired);
fprintf(f, "isSortedByTarget %d\n", sortedByChrom);
fprintf(f, "readCount %lld\n", readCount);
fprintf(f, "readBaseCount %lld\n", sumReadBases);
fprintf(f, "mappedCount %lld\n", mappedCount);
fprintf(f, "uniqueMappedCount %lld\n", uniqueMappedCount);
fprintf(f, "readSizeMean %g\n", (double)sumReadBases/readCount);
if (minReadBases != maxReadBases)
    fprintf(f, "readSizeStd %g\n", calcStdFromSums(sumReadBases, sumSquaredReadBases, readCount));
else
    fprintf(f, "readSizeStd 0\n");
fprintf(f, "readSizeMin %lld\n", minReadBases);
fprintf(f, "readSizeMax %lld\n", maxReadBases);
tpList = slListRandomSample(tpList, 4000000);
slSort(&tpList, targetPosCmp);
int m4ReadCount = slCount(tpList);
fprintf(f, "u4mReadCount %d\n", m4ReadCount);
int m4UniquePos = countUniqueFromSorted(tpList);
fprintf(f, "u4mUniquePos %d\n", m4UniquePos);
double m4UniqueRatio = (double)m4UniquePos/m4ReadCount;
fprintf(f, "u4mUniqueRatio %g\n", m4UniqueRatio);
verbose(1, "u4mUniqueRatio %g\n", m4UniqueRatio);

fprintf(f, "targetCount %d\n", (int) head->n_targets);
fprintf(f, "targetBaseCount %lld\n", targetBaseCount);


/* Deal with bed output if any */
if (sampleBed != NULL)
    {
    tpList = slListRandomSample(tpList, sampleBedSize);
    slSort(&tpList, targetPosCmp);
    FILE *bf = mustOpen(sampleBed, "w");
    bam_header_t *bamHeader = sf->header;
    for (tp = tpList; tp != NULL; tp = tp->next)
        {
        char *chrom = bamHeader->target_name[tp->targetId];
	fprintf(bf, "%s\t%u\t%u\t.\t0\t%c\n", 
	    chrom, tp->pos, tp->pos + tp->size, tp->strand);
	}
    carefulClose(&bf);
    }


/* Clean up */
samclose(sf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
sampleBed = optionVal("sampleBed", sampleBed);
sampleBedSize = optionInt("sampleBedSize", sampleBedSize);
edwBamStats(argv[1], argv[2]);
return 0;
}
