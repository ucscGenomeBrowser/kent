/* encodeMergeReplicates - Merge together replicates for a pooled output.  Only works on narrowPeak files currently.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "encode/encodePeak.h"
#include "peakCluster.h"
#include "rangeTree.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

int clAgree = 1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encodeMergeReplicates - Merge together replicates for a pooled output.  \n"
  "Only works on narrowPeak files currently.\n"
  "usage:\n"
  "   encodeMergeReplicates in1 in2 in3 ... output\n"
  "options:\n"
  "   -agree=N - only output where have agreement between N replicates, default is 1\n"
  );
}

static struct optionSpec options[] = {
   {"agree", OPTION_INT},
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

void outputClusterNarrowPeak(struct peakCluster *cluster, FILE *f)
/* Output cluster of overlapping narrowPeaks - doing average of items. */
{
struct slRef *ref, *refList=cluster->itemRefList;
double sumP = 0.0, sumQ = 0.0, sumSignal = 0.0;
long long sumStart = 0, sumEnd = 0;
long long sumPeak = 0, sumScore = 0;
struct peakItem *bestItem = bestItemInRefList(refList);
int itemCount = 0;
char *bestLine = bestItem->asciiLine;
char *bestName = NULL;
int wordCount = chopByWhite(bestLine, NULL, 0);
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
    sumP += sqlDouble(words[7]);
    sumQ += sqlDouble(words[8]);
    int peak = -1;
    if (wordCount > 9)
        peak = sqlSigned(words[9]) + chromStart;
    sumPeak += peak;
    itemCount += 1;
    }
fprintf(f, "%s\t", cluster->chrom);		// chrom
int chromStart = sumStart/itemCount;
fprintf(f, "%d\t", chromStart);			// chromStart
fprintf(f, "%d\t", (int)(sumEnd/itemCount));	// chromEnd
fprintf(f, "%s\t", bestName);			// name
fprintf(f, "%d\t", (int)(sumScore/itemCount));	// score 0-100
fprintf(f, ".\t");				// strand
fprintf(f, "%g\t", sumSignal/itemCount);	// signalValue
fprintf(f, "%g\t", sumP/itemCount);		// pValue
fprintf(f, "%g\t", sumQ/itemCount);		// qValue
if (wordCount > 9)
    fprintf(f, "%d\n", (int)(sumPeak/itemCount - chromStart));
else
    fprintf(f, "-1\n");
}

void encodeMergeReplicates(int inCount, char *inNames[], char *outName)
/* encodeMergeReplicates - Merge together replicates for a pooled output.  
 * Only works on narrowPeak files currently. */
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
    source->scoreColIx = 6;
    source->normFactor = 1.0;
    source->minColCount = 7;
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
		 outputClusterNarrowPeak(cluster, f);
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
encodeMergeReplicates(argc-2, argv+1, argv[argc-1]);
return 0;
}
