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

int fetchBefore = 50000, fetchAfter = 50000;
boolean missingZero = FALSE;
double maxVal = BIGDOUBLE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regCompanionGraphVsFixedPoints - Given a bunch of fixed point in genome as input\n"
  " (promoters perhaps) help make graphs of many features in bins relative to that point.\n"
  "usage:\n"
  "   regCompanionGraphVsFixedPoints in.bed inBigWigs.lst out.tab\n"
  "options:\n"
  "   -fetchBefore=N (default %d)\n"
  "   -fetchAfter=N (default %d)\n"
  "   -missingZero - if set treat missing data in bigWigs as zero values\n"
  "   -maxVal=N.N - if set clamp values in wig files above this value to this value\n"
  , fetchBefore, fetchAfter
  );
}

static struct optionSpec options[] = {
   {"fetchBefore", OPTION_INT},
   {"fetchAfter", OPTION_INT},
   {"missingZero", OPTION_BOOLEAN},
   {"maxVal", OPTION_DOUBLE},
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
struct bigWigValsOnChrom *chromVals = bigWigValsOnChromNew();

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
	char *chrom = startChrom->chrom;

	if (bigWigValsOnChromFetchData(chromVals, chrom, bbi))
	    {
	    /* Fetch intervals and use them to paint array of doubles. */
	    int chromSize = chromVals->chromSize;

	    /* Now loop through beds for this chromosome, adding to sums. */
	    struct bed *bed;
	    for (bed = startChrom; bed != endChrom; bed = bed->next)
		{
		char strand = bed->strand[0];
		int bufIx, bufIxInc;
		int start, end, pos;
		if (strand == '+')
		    {
		    pos = bed->chromStart;
		    start = pos - fetchBefore;
		    end = pos + fetchAfter;
		    bufIx = 0;
		    bufIxInc = 1;
		    }
		else if (strand == '-')
		    {
		    pos = bed->chromEnd;
		    start = pos - fetchAfter;
		    end = pos + fetchBefore;
		    bufIx = sumBufSize - 1;
		    bufIxInc = -1;
		    }
		else
		    {
		    // Keep compiler happy about uninitialized vars then abort.
		    bufIx = bufIxInc = start = end = pos = 0;  
		    errAbort("Expecting input bed to be stranded\n");
		    }
		if (rangeIntersection(start, end, 0, chromSize) == sumBufSize)
		    {
		    Bits *covBuf = chromVals->covBuf;
		    double *valBuf = chromVals->valBuf;
		    for (i=start; i<end; ++i)
			{
			if (bitReadOne(covBuf, i))
			    {
			    double val = valBuf[i];
			    if (val > maxVal)
				val = maxVal;
			    sumBuf[bufIx] += val;
			    nBuf[bufIx] += 1;
			    }
			else if (missingZero)
			    nBuf[bufIx] += 1;
			bufIx += bufIxInc;
			}
		    }
		}
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
fetchBefore = optionInt("fetchBefore", fetchBefore);
fetchAfter = optionInt("fetchAfter", fetchAfter);
missingZero = optionExists("missingZero");
maxVal = optionDouble("maxVal", maxVal);
if (argc != 4)
    usage();
regCompanionGraphVsFixedPoints(argv[1], argv[2], argv[3]);
return 0;
}
