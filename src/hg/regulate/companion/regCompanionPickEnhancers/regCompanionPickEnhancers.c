/* regCompanionPickEnhancers - Pick enhancer regions by a number of criteria. 
 *  o Start with the top 50,000 DNAse peaks for the cell line.
 *  o No overlap with CTCF peak.
 *  o No overlap with promoter (+-100 bytes from txStart)
 *  o Minimal transcription within 100 bases on either side(< 10 average)
 *  o h3k4me1 average 1000 bases centered on DNAse site is > 30
 * This program will pick enhancers based on just one cell line at a time.  
 * Up to regCompanionFindConcordant to pair up with genes and go for the more refined set. 
 *
 * If the loose parameters are used, instead start with top 100,000 dnase peaks,
 * extend h3k4me1 area to 1200, and reduce average to 20. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "genomeRangeTree.h"
#include "bigWig.h"
#include "basicBed.h"
#include "hmmstats.h"
#include "encode/encodePeak.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

// Some vars that are command line parameters:
int histoneBoxSize = 1000;
double histoneMinBoxStds = 2.5;
int maxDnasePeaks = 50000;
double maxTxn = 10;

// Some other vars it might be good to make command line parameters some day.
int promoBefore = 100;
int promoAfter = 100;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regCompanionPickEnhancers - Pick enhancer regions by a number of criteria\n"
  "usage:\n"
  "   regCompanionPickEnhancers dnase.peak genes.bed txn.bigWig ctcf.peak h3k4me1.bigWig output.tab\n"
  "Where:\n"
  "   dnase.peak is a narrow peak format file with DNAse called peaks\n"
  "   genes.bed is a set of gene predictions.  Only txStart, txEnd, and strand are used.\n"
  "            The program just uses this to filter out promoters\n"
  "   txn.bigWig is a RNA-seq derived file for this cell line\n"
  "   ctcf.peak is a narrow peak format file with CTCF called peaks\n"
  "   h3k4me1.bigWig is a histone H3K4Me1 derived chip seq signal file\n"
  "   output.peak is the subset of dnase.peak that passes all the filters\n"
  "options:\n"
  "    histoneBoxSize=N - Size of box around DNAse peak to look for H3K4Me1 levels. Default %d\n"
  "    histoneMinBoxStds=N.N - Minimum average H3K4Me1 level in box must be over background in\n"
  "                            units of standard deviations. Default %g\n"
  "    maxDnasePeaks=N - After score sorting take this many DNAse peaks to next level. Default %d\n"
  "    maxTxn=N.N - Maximum average transcription level.  Default %g\n"
  , histoneBoxSize, histoneMinBoxStds, maxDnasePeaks, maxTxn
  );
}

static struct optionSpec options[] = {
   {"histoneBoxSize", OPTION_INT},
   {"histoneMinBoxStds", OPTION_DOUBLE},
   {"maxDnasePeaks", OPTION_INT},
   {"maxTxn", OPTION_DOUBLE},
   {NULL, 0},
};

int encodePeakCmpChrom(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct encodePeak *a = *((struct encodePeak **)va);
const struct encodePeak *b = *((struct encodePeak **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
return dif;
}

int encodePeakCmpSignalVal(const void *va, const void *vb)
/* Compare to sort based on signalValue */
{
const struct encodePeak *a = *((struct encodePeak **)va);
const struct encodePeak *b = *((struct encodePeak **)vb);
double dif = b->signalValue - a->signalValue;
if (dif < 0)
    return -1;
else if (dif == 0)
    return 0;
else
    return 1;
}


struct encodePeak *narrowPeakLoadAll(char *fileName)
/* Load all narrowPeaks in file. */
{
struct encodePeak *list = NULL, *peak;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[10];
while (lineFileRow(lf, row) != 0)
    {
    peak = narrowPeakLoad(row);
    slAddHead(&list, peak);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct encodePeak *filterOnListSize(struct encodePeak *inList, int maxRetSize)
/* Return sublist of inList that has at most maxRetSize items. */
{
struct encodePeak *lastEl = slElementFromIx(inList, maxRetSize-1);
if (lastEl != NULL)
     lastEl->next = NULL;
return inList;
}

struct encodePeak *filterOnSignal(struct encodePeak *inList, double minValue)
/* Return sublist of inList that has at most maxRetSize items. */
{
struct encodePeak *outList = NULL, *el, *next;
for (el = inList; el != NULL; el = next)
    {
    next = el->next;
    if (el->signalValue >= minValue)
        slAddHead(&outList, el);
    }
slReverse(&outList);
return outList;
}

struct encodePeak *filterOutOverlapping(struct encodePeak *inList, struct genomeRangeTree *ranges)
/* Return sublist of inList that doesn't overlap ranges. */
{
struct encodePeak *outList = NULL, *el, *next;
for (el = inList; el != NULL; el = next)
    {
    next = el->next;
    if (!genomeRangeTreeOverlaps(ranges, el->chrom, el->chromStart, el->chromEnd))
        slAddHead(&outList, el);
    }
slReverse(&outList);
return outList;
}

struct encodePeak *nextPeakOutOfChrom(struct encodePeak *list, char *chrom)
/* Return next peak in list that is not in the same chromosome.  May return NULL at end. */
{
struct encodePeak *el;
for (el = list; el != NULL; el = el->next)
    if (!sameString(chrom, el->chrom))
        break;
return el;
}

struct encodePeak *filterOutTranscribed(struct encodePeak *inList, struct bbiFile *bbi, 
	struct bigWigValsOnChrom *chromVals)
/* Return sublist of inList that has low average transcription for self and 100 bases on
 * either side. Assumes inList is sorted by chromosome.  This is more to get rid of promoters
 * that didn't make it into gene set than anything. */
{
struct encodePeak *outList = NULL;
struct encodePeak *peak, *next, *chromStart, *chromEnd;

for (chromStart = inList; chromStart != NULL; chromStart = chromEnd)
    {
    char *chrom = chromStart->chrom;
    chromEnd = nextPeakOutOfChrom(chromStart->next, chrom);
    bigWigValsOnChromFetchData(chromVals, chrom, bbi);
    int chromSize = chromVals->chromSize;

    if (chromSize == 0)	/* No transcription on this chromosome, so add all peaks. */
        {
	for (peak = chromStart; peak != chromEnd; peak = next)
	    {
	    next = peak->next;
	    slAddHead(&outList, peak);
	    }
	continue;
	}

    struct encodePeak *peak;
    double *valBuf = chromVals->valBuf;
    for (peak = chromStart; peak != chromEnd; peak = next)
        {
	next = peak->next;
	int start = peak->chromStart - 100;
	if (start < 0) start = 0;
	int end = peak->chromEnd + 100;
	if (end >chromSize) end = chromSize;

	double ave = 0;
	int size = end - start;
	if (size > 0)
	    {
	    double sum = 0;
	    int i;
	    for (i=start; i<end; ++i)
		sum += valBuf[i];
	    ave = sum/size;
	    }

	if (ave < maxTxn)
	    {
	    slAddHead(&outList, peak);
	    }
	}
    }
slReverse(&outList);
return outList;
}

struct encodePeak *filterOutUnderInNeighborhood(struct encodePeak *inList, struct bbiFile *bbi,
	struct bigWigValsOnChrom *chromVals, int neighborhoodSize, double stdDevAboveMean)
/* Return sublist of inList that has levels in bbi averaging at least stdDevAboveMean
 * in window of neighborhoodSize centered around peak. */
{
struct encodePeak *outList = NULL;
struct encodePeak *peak, *next, *chromStart, *chromEnd;

/* Figure out threshold based on stdDevAboveMean */
struct bbiSummaryElement summary = bbiTotalSummary(bbi);
double mean = summary.sumData / summary.validCount;
double std = calcStdFromSums(summary.sumData, summary.sumSquares, summary.validCount);
double minSignal = mean + stdDevAboveMean * std;

for (chromStart = inList; chromStart != NULL; chromStart = chromEnd)
    {
    char *chrom = chromStart->chrom;
    chromEnd = nextPeakOutOfChrom(chromStart->next, chrom);
    bigWigValsOnChromFetchData(chromVals, chrom, bbi);
    int chromSize = chromVals->chromSize;
    Bits *covBuf = chromVals->covBuf;
    double *valBuf = chromVals->valBuf;

    if (chromSize == 0)	/* No transcription on this chromosome, so add all peaks. */
        {
	for (peak = chromStart; peak != chromEnd; peak = next)
	    {
	    next = peak->next;
	    slAddHead(&outList, peak);
	    }
	continue;
	}

    struct encodePeak *peak;
    int fullSize = neighborhoodSize;
    int halfSize = fullSize/2;
    for (peak = chromStart; peak != chromEnd; peak = next)
        {
	next = peak->next;
	int center = (peak->chromStart + peak->chromEnd)/2;
	int start = center - halfSize;
	int end = start + fullSize;
	if (start < 0) start = 0;
	if (end >chromSize) end = chromSize;
	int size = end - start;
	double ave = 0;
	if (size > 0)
	    {
	    int n = bitCountRange(covBuf, start, size);
	    if (n > 0)
		{
		double sum = 0;
		int i;
		for (i=start; i<end; ++i)
		    sum += valBuf[i];
		ave = sum/n;
		}
	    }
	if (ave >= minSignal)
	    {
	    slAddHead(&outList, peak);
	    }
	}
    }
slReverse(&outList);
return outList;
}

#ifdef DEBUG
boolean uglyCheckStillThere(struct encodePeak *list, char *message)
/* Check that a particular peak is still there - just for debugging. */
{
struct encodePeak *el;
for (el = list; el != NULL; el = el->next)
    {
    if (el->chromStart == 157333420 && el->chromEnd == 157333570 && sameString(el->chrom, "chr2"))
       {
       uglyf("Got %s:%d-%d %s\n", el->chrom, el->chromStart, el->chromEnd, message);
       return TRUE;
       }
    }
uglyf("no chr2:157333421-157333570 %s\n", message);
return FALSE;
}
#endif /* DEBUG */

void regCompanionPickEnhancers(char *dnasePeaks, char *genesBed, char *txnWig, char *ctcfPeaks,
	char *h3k4me1Wig, char *outputTab)
/* regCompanionPickEnhancers - Pick enhancer regions by a number of criteria. */
{
struct encodePeak *dnaseList = narrowPeakLoadAll(dnasePeaks);

/* Load up genes and fill up genome range tree with promoters from them. */
struct bed *geneList = bedLoadAll(genesBed);
struct genomeRangeTree *promoterRanges = genomeRangeTreeNew();
struct bed *gene;
int start=0, end=0;
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    char strand = gene->strand[0];
    if (strand == '+')
       {
       start = gene->chromStart - promoBefore;
       end = gene->chromStart + promoAfter;
       }
    else if (strand == '-')
       {
       start = gene->chromEnd - promoAfter;
       end = gene->chromEnd + promoBefore;
       }
    else 
       errAbort("Gene %s has strand '%c' - has to be '+' or '-'", gene->name, strand);
    genomeRangeTreeAdd(promoterRanges, gene->chrom, start, end);
    }

struct encodePeak *ctcfList = narrowPeakLoadAll(ctcfPeaks);
struct genomeRangeTree *ctcfRanges = genomeRangeTreeNew();
struct encodePeak *ctcf;
for (ctcf = ctcfList; ctcf != NULL; ctcf = ctcf->next)
    {
    genomeRangeTreeAdd(ctcfRanges, ctcf->chrom, ctcf->chromStart, ctcf->chromEnd);
    }

struct bbiFile *txnBbi = bigWigFileOpen(txnWig);
struct bbiFile *h3k4me1Bbi = bigWigFileOpen(h3k4me1Wig);
struct bigWigValsOnChrom *chromVals = bigWigValsOnChromNew();

verbose(1, "Initial: %d\n", slCount(dnaseList));
slSort(&dnaseList, encodePeakCmpSignalVal);
dnaseList = filterOnListSize(dnaseList, maxDnasePeaks);
verbose(1, "top%d: %d\n", maxDnasePeaks, slCount(dnaseList));
slSort(&dnaseList, encodePeakCmpChrom);
dnaseList = filterOutOverlapping(dnaseList, ctcfRanges);
verbose(1, "nonCtcf: %d\n", slCount(dnaseList));
dnaseList = filterOutOverlapping(dnaseList, promoterRanges);
verbose(1, "nonPromoter: %d\n", slCount(dnaseList));
dnaseList = filterOutTranscribed(dnaseList, txnBbi, chromVals);
verbose(1, "nonTranscribed: %d\n", slCount(dnaseList));
dnaseList = filterOutUnderInNeighborhood(dnaseList, h3k4me1Bbi, chromVals, 
	histoneBoxSize, histoneMinBoxStds);
verbose(1, "gotH3k4me1: %d\n", slCount(dnaseList));

FILE *f = mustOpen(outputTab, "w");
struct encodePeak *peak;
for (peak = dnaseList; peak != NULL; peak = peak->next)
    {
    encodePeakOutputWithType(peak, narrowPeak, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
histoneBoxSize = optionInt("histoneBoxSize", histoneBoxSize);
histoneMinBoxStds = optionDouble("histoneMinBoxStds", histoneMinBoxStds);
maxDnasePeaks = optionInt("maxDnasePeaks", maxDnasePeaks);
maxTxn = optionDouble("maxTxn", maxTxn);
regCompanionPickEnhancers(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
