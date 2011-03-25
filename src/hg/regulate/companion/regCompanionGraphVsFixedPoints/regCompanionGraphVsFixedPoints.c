/* regCompanionGraphVsFixedPoints - Given a bunch of fixed point in genome as input 
 * (promoters perhaps) help make graphs of many features in bins relative to that point. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "basicBed.h"
#include "bigWig.h"
#include "obscure.h"
#include "localmem.h"
#include "bits.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

int fetchBefore = 10000, fetchAfter = 10000;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regCompanionGraphVsFixedPoints - Given a bunch of fixed point in genome as input\n"
  " (promoters perhaps) help make graphs of many features in bins relative to that point.\n"
  "usage:\n"
  "   regCompanionGraphVsFixedPoints in.bed inBigWigs.lst out.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct bed *nextChromDifferent(struct bed *bedList)
/* Return next bed in list that is from a different chrom than the start of the list. */
{
char *firstChrom = bedList->chrom;
struct bed *bed;
for (bed = bedList->next; bed != NULL; bed = bed->next)
    if (!sameString(firstChrom, bed->chrom))
        break;
return bed;
}

void regCompanionGraphVsFixedPoints(char *inBed, char *inBigWigFileList, char *outTab)
/* regCompanionGraphVsFixedPoints - Given a bunch of fixed point in genome as input 
 * (promoters perhaps) help make graphs of many features in bins relative to that point. */
{
struct bed *bedList = bedLoadAll(inBed);
slSort(&bedList, bedCmp);
struct slName *inFileList = readAllLines(inBigWigFileList);
FILE *f = mustOpen(outTab, "w");

/* Set up some buffers and variables to hold unpacked double-per-base representation. */
double *valBuf = NULL;
Bits *covBuf = NULL;
int bufSize = 0;

int sumBufSize = fetchBefore + fetchAfter;
double *sumBuf;
AllocArray(sumBuf, sumBufSize);
long *nBuf;
AllocArray(nBuf, sumBufSize);

struct slName *inFile;
int inCount = slCount(inFileList);
int inIx = 1;
for (inFile = inFileList; inFile != NULL; inFile = inFile->next, ++inIx)
    {
    verbose(1, "%d of %d processing %s\n", inIx, inCount, inFile->name);
    /* Zero out sum buffers. */
    int i;
    for (i=0; i<sumBufSize; ++i)
        {
	sumBuf[i] = 0.0;
	nBuf[i] = 0;
	}
    struct bbiFile *bbi = bigWigFileOpen(inFile->name);
    struct bed *startChrom, *endChrom;
    for (startChrom = bedList; startChrom != NULL; startChrom = endChrom)
	{
	/* Figure out end condition. */
	endChrom = nextChromDifferent(startChrom);

	/* Fetch intervals and use them to paint array of doubles. */
	char *chrom = startChrom->chrom;
	int chromSize = bbiChromSize(bbi, chrom);
	if (chromSize > 0)   /* It's legit for it to be zero - just no data for that chrom */
	    {
	    /* Make sure  buffers are big enough. */
	    if (chromSize > bufSize)
		{
		bufSize = chromSize;
		freeMem(covBuf);
		freeMem(valBuf);
		valBuf = needHugeMem(bufSize * sizeof(double));
		covBuf = bitAlloc(bufSize);
		}

	    /* Zero out buffers */
	    bitClear(covBuf, chromSize);
	    for (i=0; i<chromSize; ++i)
		valBuf[i] = 0.0;

	    /* Fetch intervals for this chromosome and fold into buffers. */
	    struct lm *lm = lmInit(0);
	    struct bbiInterval *iv, *ivList = bigWigIntervalQuery(bbi, chrom, 0, chromSize, lm);
	    for (iv = ivList; iv != NULL; iv = iv->next)
		{
		double val = iv->val;
		int end = iv->end;
		for (i=iv->start; i<end; ++i)
		    valBuf[i] = val;
		bitSetRange(covBuf, iv->start, iv->end - iv->start);
		}

	    /* Now loop through beds for this chromosome, adding to sums. */
	    struct bed *bed;
	    for (bed = startChrom; bed != endChrom; bed = bed->next)
	        {
		char strand = bed->strand[0];
		if (strand == '+')
		    {
		    int pos = bed->chromStart;
		    int start = pos - fetchBefore;
		    int end = pos + fetchAfter;
		    if (rangeIntersection(start, end, 0, chromSize) == sumBufSize)
		        {
			int bufIx = 0;
			for (i=start; i<end; ++i, ++bufIx)
			    {
			    if (bitReadOne(covBuf, i))
			        {
				sumBuf[bufIx] += valBuf[i];
				nBuf[bufIx] += 1;
				}
			    }
			}
		    }
		else if (strand == '-')
		    {
		    int pos = bed->chromEnd;
		    int start = pos - fetchAfter;
		    int end = pos + fetchBefore;
		    if (rangeIntersection(start, end, 0, chromSize) == sumBufSize)
		        {
			int bufIx = sumBufSize-1;
			for (i=start; i<end; ++i, --bufIx)
			    {
			    if (bitReadOne(covBuf, i))
			        {
				sumBuf[bufIx] += valBuf[i];
				nBuf[bufIx] += 1;
				}
			    }
			}
		    }

		else
		    errAbort("Expecting input bed to be stranded\n");
		}
	    lmCleanup(&lm);
	    }
	}
    bigWigFileClose(&bbi);

    /* Now go and print results: file name first and then a bunch of numbers then newline */
    fprintf(f, "%s", inFile->name);
    for (i=0; i<sumBufSize; ++i)
	{
	int n = nBuf[i];
	double ave = (n>0 ? sumBuf[i]/n : 0);
        fprintf(f, "\t%g", ave);
	}
    fprintf(f, "\n");
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
regCompanionGraphVsFixedPoints(argv[1], argv[2], argv[3]);
return 0;
}
