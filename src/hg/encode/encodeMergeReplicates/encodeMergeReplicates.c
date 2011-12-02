/* encodeMergeReplicates - Merge together replicates for a pooled output.  Only works on 
 * narrowPeak and broadPeak files currently. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "encode/encodePeak.h"
#include "peakCluster.h"
#include "rangeTree.h"


int clAgree = 1;
boolean clAdd = FALSE;
double clThreshold = 0.0;
double clGotThreshold = FALSE;
boolean clUniqueName = FALSE;

#define SCORE_COL_IX 6

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encodeMergeReplicates - Merge together replicates for a pooled output.  \n"
  "Only works on narrowPeak and broadPeak files currently. Ignores strand.\n"
  "usage:\n"
  "   encodeMergeReplicates in1 in2 in3 ... output\n"
  "options:\n"
  "   -agree=N - only output where have agreement between N replicates, default is 1\n"
  "   -add - add together signals rather than averaging them\n"
  "   -threshold=N.N - only output where signal is over threshold\n"
  "   -maxMin - set threshold to be the maximum of the minima of two replicates\n"
  "   -addMin - set threshold to be the sum of the minima of the two replicates\n"
  "   -uniqueName - make output names unique instead of name of best in cluster\n"
  );
}

static struct optionSpec options[] = {
   {"agree", OPTION_INT},
   {"add", OPTION_BOOLEAN},
   {"threshold", OPTION_DOUBLE},
   {"maxMin", OPTION_BOOLEAN},
   {"addMin", OPTION_BOOLEAN},
   {"uniqueName", OPTION_BOOLEAN},
   {NULL, 0},
};

struct peakItem *bestItemInRefList(struct slRef *refList)
/* Return highest scoring item in list */
{
struct peakItem *bestItem = refList->val;
struct slRef *ref;
for (ref = refList->next; ref != NULL; ref = ref->next)
    {
    struct peakItem *item = ref->val;
    if (item->score > bestItem->score)
        bestItem = item;
    }
return bestItem;
}

static int peakClusterSourceCount(struct peakCluster *cluster)
/* Return number of sources in cluster. */
{
/* Use simple algorithm here since # of sources likely to just be 1-3 */
struct slRef *uniqList = NULL, *ref;
for (ref = cluster->itemRefList; ref != NULL; ref = ref->next)
     {
     struct peakItem *item = ref->val;
     refAddUnique(&uniqList, item->source);
     }
int count = slCount(uniqList);
slFreeList(&uniqList);
return count;
}

static int countNonnumericPrefix(char *s)
/* Return number of characters until get first digit. */
{
int count = 0;
char c;
while ((c = *s++) != 0)
    {
    if (isdigit(c))
        break;
    else
        ++count;
    }
return count;
}

static void copyNonnumericPrefix(char *source, char *dest, int destSize)
/* Copy non-numerical prefix if any of source to dest. */
{
int size = countNonnumericPrefix(source);
if (size >= destSize)
    errAbort("Nonnumerical prefix of %s longer than %d chars.", source, destSize);
memcpy(dest, source, size);
dest[size] = 0;
}

void outputClusterNarrowPeak(struct peakCluster *cluster, FILE *f, boolean add,
	boolean gotThreshold, double threshold, boolean forceUniqueName)
/* Output cluster of overlapping narrowPeaks - doing average of items. */
{
/* Some variables to keep statistics on all items in cluster. */
double sumP = 0.0, sumQ = 0.0, sumSignal = 0.0;
long long sumStart = 0, sumEnd = 0;
long long sumPeak = 0, sumScore = 0;

/* To figure out what to name the cluster, we look for the name of the best 
 * (highest scoring) item. */
struct slRef *ref, *refList=cluster->itemRefList;
struct peakItem *bestItem = bestItemInRefList(refList);
int itemCount = 0;
char *bestLine = bestItem->asciiLine;
char *bestName = NULL;

/* Figure out # of words to output based on best item (oh my) */
int wordCount = chopByWhite(bestLine, NULL, 0);
boolean gotP = FALSE, gotQ = FALSE;
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct peakItem *item = ref->val;
    char *words[wordCount];
    chopByWhite(item->asciiLine, words, wordCount);
    int chromStart = sqlUnsigned(words[1]);
    sumStart += chromStart;
    sumEnd += sqlUnsigned(words[2]);
    if (bestItem == item)
        bestName = words[3];
    sumScore += sqlSigned(words[4]);
    sumSignal += sqlDouble(words[6]);
    if (!sameString(words[7], "-1"))
        {
	gotP = TRUE;
	sumP += sqlDouble(words[7]);
	}
    if (!sameString(words[8], "-1"))
	{
	gotQ = TRUE;
	sumQ += sqlDouble(words[8]);
	}
    int peak = -1;
    if (wordCount > 9)
        peak = sqlSigned(words[9]) + chromStart;
    sumPeak += peak;
    itemCount += 1;
    }

/* Handle much of the averaging by multiplying by scale factor rather than 
 * dividing by itemCount. */
double scaleFactor = 1.0;
if (!add)
    scaleFactor = 1.0/itemCount;
double signalValue = sumSignal * scaleFactor;

/* Apply threshold if have one. */
if (!gotThreshold || signalValue >= threshold)
    {
    /* Output chrom/start/end fields. Start and end are averaged from all items regardless 
     * of add flag*/
    fprintf(f, "%s\t", cluster->chrom);		// chrom
    int chromStart = sumStart/itemCount;
    fprintf(f, "%d\t", chromStart);			// chromStart
    fprintf(f, "%d\t", (int)(sumEnd/itemCount));	// chromEnd

    /* Figure out what to call it,  same name as best item usually, but can force a
     * unique numerical suffix.  This will replace any existing numerical suffix. */
    char *name = bestName;
    char uniqPrefix[64];
    char uniqName[100];
    if (forceUniqueName)
        {
	static int uniqIx = 0;
	copyNonnumericPrefix(name, uniqPrefix, sizeof(uniqPrefix));
	safef(uniqName, sizeof(uniqName), "%s%d", uniqPrefix, ++uniqIx);
	name = uniqName;
	}
    fprintf(f, "%s\t", name);			// name

    /* Output score - possibly averaged. */
    int score = sumScore*scaleFactor;
    if (score > 1000) score = 1000;
    fprintf(f, "%d\t", score);			// score 0-1000

    /* Some day need to come back to strand... */
    fprintf(f, ".\t");				// strand

    /* Output signal, P and Q, possibly doing average, handling not. */
    fprintf(f, "%g\t", sumSignal*scaleFactor);	// signalValue
    if (gotP)
	fprintf(f, "%g\t", sumP*scaleFactor);		// pValue
    else
	fprintf(f, "-1\t");
    if (gotQ)
	fprintf(f, "%g", sumQ*scaleFactor);		// qValue
    else
	fprintf(f, "-1");

    /* Here we need to do something else if there's a mix of narrow and broad in input, or
     * some but not all are -1. */
    if (wordCount > 9)
	fprintf(f, "\t%d", (int)(sumPeak/itemCount - chromStart));
    fprintf(f, "\n");
    }
}

static double minOfCol(char *fileName, int colIx)
/* Return minimum value seen in given column of file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int minColCount = colIx+1;
char *row[minColCount];
boolean gotAny = FALSE;
double minVal = 0;
while (lineFileNextRow(lf, row, minColCount))
    {
    double val = lineFileNeedDouble(lf, row, colIx);
    if (!gotAny || val < minVal)
	{
	gotAny = TRUE;
        minVal = val;
	}
    }
lineFileClose(&lf);
if (!gotAny)
    errAbort("No data in %s", fileName);
return minVal;
}

static double maxMinOfReplicates(int inCount, char *inNames[], int scoreColIx)
/* Return max of mins-within-a-file. */
{
if (inCount <= 0)
    errAbort("Need positive inCount in maxMinOfReplicates.  Got %d", inCount);
double maxVal = minOfCol(inNames[0], scoreColIx);
int i;
for (i=1; i<inCount; ++i)
    {
    double val = minOfCol(inNames[i], scoreColIx);
    if (val > maxVal)
        maxVal = val;
    }
return maxVal;
}

static double addMinOfReplicates(int inCount, char *inNames[], int scoreColIx)
/* Return sum of all mins-within-a-file. */
{
double sum = 0;
int i;
for (i=0; i<inCount; ++i)
    sum += minOfCol(inNames[i], scoreColIx);
return sum;
}

void encodeMergeReplicates(int inCount, char *inNames[], char *outName)
/* encodeMergeReplicates - Merge together replicates for a pooled output.  
 * Only works on broadPeak and narrowPeak files currently. */
{
/* Make list of sources out of input files. */
struct peakSource *source, *sourceList = NULL;
int i;
for (i=0; i<inCount; ++i)
    {
    AllocVar(source);
    source->dataSource = inNames[i];
    source->chromColIx = 0;
    source->startColIx = 1;
    source->endColIx = 2;
    source->scoreColIx = SCORE_COL_IX;
    source->normFactor = 1.0;
    source->minColCount = SCORE_COL_IX+1;
    slAddTail(&sourceList, source);
    }

/* Load in from all sources. */
struct peakClusterMaker *maker = peakClusterMakerNew();
for (source = sourceList; source != NULL; source = source->next)
    peakClusterMakerAddFromSource(maker, source);

/* Cluster each chromosome. */
FILE *f = mustOpen(outName, "w");
struct hashEl *chrom, *chromList = peakClusterMakerChromList(maker);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct rbTree *tree = chrom->val;
    struct range *range, *rangeList = rangeTreeList(tree);
    struct lm *lm = lmInit(0);
    for (range = rangeList; range != NULL; range = range->next)
         {
	 struct peakCluster *cluster, *clusterList = peakClusterItems(lm, range->val,
		 BIGNUM, 0.0);
	 for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
	     {
	     if (clAgree < 2 || peakClusterSourceCount(cluster) >= clAgree)
		 outputClusterNarrowPeak(cluster, f, clAdd, clGotThreshold, clThreshold,
		 	clUniqueName);
	     }
	 }
    lmCleanup(&lm);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 4)
    usage();
clAgree = optionInt("agree", clAgree);
clAdd = optionExists("add");
clGotThreshold = optionExists("threshold");
clThreshold = optionDouble("threshold", clThreshold);
if (optionExists("maxMin"))
   {
   clGotThreshold = TRUE;
   clThreshold = maxMinOfReplicates(argc-2, argv+1, SCORE_COL_IX);
   }
if (optionExists("addMin"))
   {
   clGotThreshold = TRUE;
   clThreshold = addMinOfReplicates(argc-2, argv+1, SCORE_COL_IX);
   }
if (clGotThreshold)
   verbose(2, "Threshold %g\n", clThreshold);
clUniqueName = optionExists("uniqueName");
encodeMergeReplicates(argc-2, argv+1, argv[argc-1]);
return 0;
}
