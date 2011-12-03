/** pslToPileup -- Extract pileup locations from PSL, output as BED. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "psl.h"
#include "bed.h"
#include "bits.h"
#include "options.h"


int minHits = 50;
int maxGap = 20;

static struct optionSpec optionSpecs[] =  
/* option for pslToPileup */
{
    {"minHits", OPTION_INT},
    {"maxGap", OPTION_INT},
    {"help", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/** provide usage information */
{
errAbort("pslToPileup: Extract pileup locations from PSL (target), output as BED 5+.\n"
	 "usage:\n"
	 "    pslToPileup [options] in.psl chrom.sizes out.bed\n"
	 "options:\n"
	 "    -minHits=N  Minimum number of alignments covering a given base in order for it\n"
	 "                to be considered part of a pileup.  Default: %d.\n"
	 "    -maxGap=N   Maximum number of consecutive bases within a pileup which may have\n"
	 "                fewer than minHits.  Default: %d.\n"
	 "    \n",
	 minHits, maxGap
	 );
} 

/** Allocating 4 bytes per base for entire chroms proved too big even for 
 * kolossus, so use short (and watch out for count overflow -- very unlikely, 
 * but just in case). */
#define MAXSHORT 0x7fff

short *getCountArr(struct hash *chromCounts, struct hash *chromSizes,
		 char *chrom, int chromSize)
/** Return the array of counts associated with chrom.  Hash sizes too. */
{
struct hashEl *el = hashLookup(chromCounts, chrom);
short *arr = el ? el->val : NULL;
if (arr == NULL)
    {
    arr = needLargeZeroedMem(chromSize * sizeof(short));
    hashAdd(chromCounts, chrom, arr);
    hashAddInt(chromSizes, chrom, chromSize);
    }
return arr;
}

void pslToPileup(char *pslFile, char *chromSizeFile, char *outBedName)
/** Extract pileup locations from PSL, output as BED. */
{
FILE *outBed = NULL;
struct hash *chromCounts = hashNew(8);
struct hash *chromSizes = hashNew(8);
struct hashCookie cookie;
struct hashEl *el = NULL;
struct lineFile *lf = pslFileOpen(pslFile);
struct psl *psl = NULL;

outBed = mustOpen(outBedName, "w");

while ((psl = pslNext(lf)) != NULL)
    {
    short *countArr = getCountArr(chromCounts, chromSizes, psl->tName,
				psl->tSize);
    int i=0, j=0;
    for (i=0;  i < psl->blockCount;  i++)
	{
	for (j=0;  j < psl->blockSizes[i];  j++)
	    {
	    //#*** if tStrand (strand[1]) is -, do we need to flip??
	    if (countArr[psl->tStarts[i]+j] < MAXSHORT)
		countArr[psl->tStarts[i]+j]++;
	    }
	}
    }

cookie = hashFirst(chromCounts);
while ((el = hashNext(&cookie)) != NULL)
    {
    char *chrom = el->name;
    short *countArr = (short *)(el->val);
    int i=0;
    boolean inPileup = FALSE;
    int pStart=0, pEnd=0;
    short pMin=MAXSHORT, pMax=0;
    int pTotal=0;
    int gapCount = 0;
    int chromSize = hashIntVal(chromSizes, chrom);
    for (i=0;  i < chromSize;  i++)
	{
	if (countArr[i] >= minHits)
	    {
	    if (! inPileup)
		pStart = i;
	    pEnd = i+1;
	    if (countArr[i] < pMin)
		pMin = countArr[i];
	    if (countArr[i] > pMax)
		pMax = countArr[i];
	    pTotal += countArr[i];
	    inPileup = TRUE;
	    }
	else if (inPileup)
	    {
	    gapCount++;
	    if (gapCount > maxGap)
		{
		fprintf(outBed, "%s\t%d\t%d\t%s:%d-%d\t%d\t%d\t%.1f\t%d\n",
			chrom, pStart, pEnd, chrom, pStart+1, pEnd,
			(int)(pTotal/(pEnd-pStart)),
			pMin, (double)pTotal/(pEnd-pStart), pMax);
		pStart = pEnd = pMax = pTotal = 0;
		pMin = MAXSHORT;
		gapCount = 0;
		inPileup = FALSE;
		}
	    }
	}
    if (inPileup)
	fprintf(outBed, "%s\t%d\t%d\t%s:%d-%d\t%d\t%d\t%.1f\t%d\n",
		chrom, pStart, pEnd, chrom, pStart+1, pEnd,
		(int)(pTotal/(pEnd-pStart)),
		pMin, (double)pTotal/(pEnd-pStart), pMax);
    }

lineFileClose(&lf);
carefulClose(&outBed);
}


int main(int argc, char* argv[])
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage();
if (optionExists("help"))
    usage();
minHits = optionInt("minHits", minHits);
maxGap = optionInt("maxGap", maxGap);
pslToPileup(argv[1], argv[2], argv[3]);
return 0;
}
    

