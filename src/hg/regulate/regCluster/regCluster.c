/* regCluster - Cluster regulator regions. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "rangeTree.h"
#include "peakCluster.h"

static char const rcsid[] = "$Id: regCluster.c,v 1.4 2010/05/16 21:41:42 kent Exp $";

int clDims = 1;
double clScoreScale = 1.0;
double clForceJoinScore = 2;
double clWeakLevel = 0.1;
boolean clAveScore = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
"regCluster - Cluster regulator regions\n"
"usage:\n"
"   regCluster tableOfTables output.cluster output.bed\n"
"Where the table-of-tables is space or tab separated in the format:\n"
"   <fileName> <chrom> <start> <end> <score> <normScore> <dim1 label> ... <dimN label>\n"
"where chrom, start, end, score are the indexes (starting with 0) of the chromosome, start, \n"
"and end fields in the file, normScore is a factor to multiply score by to get it into the\n"
"0-1000 range, and the dim# labels are the labels in each dimention.\n"
"for example\n"
"   simpleReg.bed 0 1 2 4 aCell aFactor\n"
"options:\n"
"   -dims=N - number of dimensions in data.  Would be 2 for cell-line + antibody. Default %d\n"
"   -scoreScale=0.N - scale score by this factor. Default %g\n"
"   -weakLevel=0.N - within a cluster ratio of highest depth of coverage of cluster to lowest\n"
"                    Default %g\n"
"   -forceJoinScore=0.N - if combined score of elements joining 2 clusters above this, the\n"
"                         clusters will be joined. Default %g\n"
"   -aveScore - if set cluster score is average of components, not max\n"
, clDims
, clScoreScale
, clWeakLevel
, clForceJoinScore
);
}

static struct optionSpec options[] = {
   {"dims", OPTION_INT},
   {"scoreScale", OPTION_DOUBLE},
   {"forceJoinScore", OPTION_DOUBLE},
   {"weakLevel", OPTION_DOUBLE},
   {"aveScore", OPTION_BOOLEAN},
   {NULL, 0},
};

static int clusterIx = 0;

void outputClustersForChrom(char *chrom, struct rbTree *tree, FILE *fCluster, FILE *fBed)
/* Write out clusters on one chromosome */
{
struct range *range, *rangeList = rangeTreeList(tree);
verbose(2, "Got %d ranges on %s\n", slCount(rangeList), chrom);
struct lm *lm = lmInit(0);
for (range = rangeList; range != NULL; range = range->next)
    {
    struct peakCluster *cluster, *clusterList = peakClusterItems(lm, range->val, 
    	clForceJoinScore, clWeakLevel);
    for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
        {
	struct slRef *ref, *refList=cluster->itemRefList;
	++clusterIx;
	struct hash *uniqHash = hashNew(0);
	for (ref = refList; ref != NULL; ref = ref->next)
	    {
	    struct peakItem *item = ref->val;
	    hashStore(uniqHash, item->source->dataSource);
	    fprintf(fCluster, "%d\t%s\t", clusterIx, item->chrom);
	    fprintf(fCluster, "%d\t%d\t", item->chromStart, item->chromEnd);
	    fprintf(fCluster, "%g", item->score);
	    int i;
	    for (i=0; i<clDims; ++i)
	       fprintf(fCluster, "\t%s", item->source->labels[i]);
	    fprintf(fCluster, "\n");
	    }
	double score;
	if (clAveScore)
	    score = clScoreScale * (cluster->score / uniqHash->elCount);
	else
	    score = clScoreScale * cluster->maxSubScore;
	if (score > 1000) score = 1000;
	fprintf(fBed, "%s\t%d\t%d\t%d\t%d\n", chrom, 
		cluster->chromStart, cluster->chromEnd, uniqHash->elCount, round(score));
	hashFree(&uniqHash);
	}
    }
lmCleanup(&lm);
}

void regCluster(char *tableOfTables, char *outCluster, char *outBed)
/* regCluster - Cluster regulator regions. */
{
struct peakSource *source, *sourceList = peakSourceLoadAll(tableOfTables, clDims);
verbose(1, "Read %d sources from %s\n", slCount(sourceList), tableOfTables);
struct peakClusterMaker *maker = peakClusterMakerNew();
for (source = sourceList; source != NULL; source = source->next)
    peakClusterMakerAddFromSource(maker, source);

/* Get out list of chromosomes and process one at a time. */
FILE *fCluster = mustOpen(outCluster, "w");
FILE *fBed = mustOpen(outBed, "w");
struct hashEl *chrom;
struct hashEl *chromList = peakClusterMakerChromList(maker);
int totalClusters = 0;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct rbTree *tree = chrom->val;
    totalClusters += tree->n;
    outputClustersForChrom(chrom->name, tree, fCluster, fBed);
    }
verbose(1, "%d singly-linked clusters, %d clusters in %d chromosomes\n", 
	totalClusters, clusterIx, maker->chromHash->elCount);
carefulClose(&fCluster);
carefulClose(&fBed);
peakClusterMakerFree(&maker);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
clDims = optionInt("dims", clDims);
clScoreScale = optionDouble("scoreScale", clScoreScale);
clWeakLevel = optionDouble("weakLevel", clWeakLevel);
clAveScore = optionExists("aveScore");
regCluster(argv[1], argv[2], argv[3]);
return 0;
}
