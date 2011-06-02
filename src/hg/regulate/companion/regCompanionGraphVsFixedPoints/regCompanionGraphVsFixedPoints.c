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
#include "verbose.h"
#include "bits.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

int fetchBefore = 50000, fetchAfter = 50000;
boolean fromCenter = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regCompanionGraphVsFixedPoints - Given a bunch of fixed point in genome as input\n"
  " (promoters perhaps) help make graphs of many features in bins relative to that point.\n"
  "usage:\n"
  "   regCompanionGraphVsFixedPoints in.bed input.tab out.tab\n"
  "where:\n"
  "    in.bed is the regions to collect data around.  By default the bed is treated as\n"
  "           a gene, and the region is before and after the txStart, strand considered\n"
  "           but with the -fromCenter it is taken from the center instead\n"
  "    input.tab is a tab-separated file with the following fields\n"
  "           name - Short human and machine readable name.\n"
  "           normFactor - Multiply every val in wig by this. Use . for no normalizatin\n"
  "           maxVal - Clamp values above this to this. Use . for no max.\n"
  "           missingZero - true or false. If true treat missing values as 0.\n"
  "           fileName - Name of bigWigFile.\n"
  "     out.tab is the output with one line per bigWig.  The first word in line is\n"
  "           the name from the input.tab, the rest of the words are floating point numbers\n"
  "           one for each base between fetchBefore and fetchAfter below (so they are long\n"
  "           lines!)\n"
  "options:\n"
  "   -fetchBefore=N (default %d)\n"
  "   -fetchAfter=N (default %d)\n"
  "   -fromCenter - fetch out from center point of in beds rather than stranded start\n"
  "                 Happy to ignore strand (treats . as +)\n"
  , fetchBefore, fetchAfter
  );
}

static struct optionSpec options[] = {
   {"fetchBefore", OPTION_INT},
   {"fetchAfter", OPTION_INT},
   {"fromCenter", OPTION_BOOLEAN},
   {NULL, 0},
};

struct rcgInputInfo
/* Input list to regCompanionGraphVsFixedPoints */
    {
    struct rcgInputInfo *next;
    char *name;		/* Short human and machine readable name. */
    double normFactor;	/* Multiply every val in wig by this. */
    double maxVal;	/* Clamp values above this to this. */
    boolean missingZero; /* If true treat missing values as 0 */
    char *fileName;	/* Name of bigWigFile. */
    };

struct rcgInputInfo *readInputInfo(char *fileName)
/* Read input info. */
{
struct rcgInputInfo *list = NULL, *el;
char *row[5];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, row))
    {
    AllocVar(el);
    el->name = cloneString(row[0]);
    if (!sameString(row[1], "."))
	el->normFactor = lineFileNeedDouble(lf, row, 1);
    if (sameString(row[2], "."))
        el->maxVal = BIGDOUBLE;
    else
        el->maxVal = lineFileNeedDouble(lf, row, 2);
    char *missingZero = row[3];
    if (sameWord(missingZero, "TRUE"))
         el->missingZero = 1;
    else if (sameWord(missingZero, "FALSE"))
        el->missingZero = 0;
    else
        errAbort("Expecting 'true' or 'false' in missingZero field line %d of %s",
		lf->lineIx, lf->fileName);
    el->fileName = cloneString(row[4]);
    if (!isBigWig(el->fileName))
        errAbort("%s is not a bigWig file\n", el->fileName);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

void regCompanionGraphVsFixedPoints(char *inBed, char *inBigWigFileList, char *outTab)
/* regCompanionGraphVsFixedPoints - Given a bunch of fixed point in genome as input 
 * (promoters perhaps) help make graphs of many features in bins relative to that point. */
{
struct bed *bedList = bedLoadAll(inBed);
slSort(&bedList, bedCmp);
struct rcgInputInfo *inTabList = readInputInfo(inBigWigFileList);
FILE *f = mustOpen(outTab, "w");

/* Set up some buffers and variables to hold unpacked double-per-base representation. */
struct bigWigValsOnChrom *chromVals = bigWigValsOnChromNew();

int sumBufSize = fetchBefore + fetchAfter;
double *sumBuf;
AllocArray(sumBuf, sumBufSize);
long *nBuf;
AllocArray(nBuf, sumBufSize);

struct rcgInputInfo *inTab;
int inCount = slCount(inTabList);
int inIx = 1;
for (inTab = inTabList; inTab != NULL; inTab = inTab->next, ++inIx)
    {
    verbose(1, "%d of %d processing %s", inIx, inCount, inTab->fileName);
    boolean missingZero = inTab->missingZero;
    double maxVal = (inTab->maxVal == 0 ? BIGDOUBLE : inTab->maxVal);

    /* Zero out sum buffers. */
    int i;
    for (i=0; i<sumBufSize; ++i)
        {
	sumBuf[i] = 0.0;
	nBuf[i] = 0;
	}
    struct bbiFile *bbi = bigWigFileOpen(inTab->fileName);
    struct bed *startChrom, *endChrom;
    for (startChrom = bedList; startChrom != NULL; startChrom = endChrom)
	{
	/* Figure out end condition. */
	endChrom = bedListNextDifferentChrom(startChrom);
	char *chrom = startChrom->chrom;

	if (bigWigValsOnChromFetchData(chromVals, chrom, bbi))
	    {
	    verboseDot();
	    /* Fetch intervals and use them to paint array of doubles. */
	    int chromSize = chromVals->chromSize;

	    /* Now loop through beds for this chromosome, adding to sums. */
	    struct bed *bed;
	    for (bed = startChrom; bed != endChrom; bed = bed->next)
		{
		char strand = bed->strand[0];
		int bufIx, bufIxInc;
		int start, end, pos;
		if (fromCenter)
		    {
		    pos = (bed->chromStart + bed->chromEnd)/2;
		    if (strand == '-')
		        {
			start = pos - fetchAfter;
			end = pos + fetchBefore;
			bufIx = sumBufSize - 1;
			bufIxInc = -1;
			}
		    else
		        {
			start = pos - fetchBefore;
			end = pos + fetchAfter;
			bufIx = 0;
			bufIxInc = 1;
			}
		    }
		else
		    {
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
    fprintf(f, "%s", inTab->name);
    for (i=0; i<sumBufSize; ++i)
	{
	int n = nBuf[i];
	double ave = (n>0 ? sumBuf[i]/n : 0);
        fprintf(f, "\t%g", ave);
	}
    fprintf(f, "\n");
    verbose(1, "\n");
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
fetchBefore = optionInt("fetchBefore", fetchBefore);
fetchAfter = optionInt("fetchAfter", fetchAfter);
fromCenter = optionExists("fromCenter");
if (argc != 4)
    usage();
regCompanionGraphVsFixedPoints(argv[1], argv[2], argv[3]);
return 0;
}
