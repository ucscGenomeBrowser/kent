/* regCluster - Cluster regulator regions. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "obscure.h"
#include "localmem.h"
#include "rangeTree.h"

static char const rcsid[] = "$Id: regCluster.c,v 1.4 2010/05/16 21:41:42 kent Exp $";

int clDims = 1;
double clScoreScale = 1.0;
double clForceJoinScore = 2;
double clWeakLevel = 0.1;

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
   {NULL, 0},
};

struct regDim
/* A regulatory dimension */
    {
    int colIx;		/* Column index in table. */
    char *label;	/* Label */
    };

struct regSource 
/* A source of regulatory information */
    {
    struct regSource *next;
    char *dataSource;		/* File (or table) */
    int chromColIx;		/* Chromosome column index. */
    int startColIx;		/* Start coordinate column index. */
    int endColIx;		/* End ccoordinate column ix. */
    int scoreColIx;		/* Index for score column. */
    double normFactor;		/* Multiply this to get browser score. */
    char **labels;		/* Label for each dimension */
    int minColCount;		/* Minimum number of columns. */
    };

struct regItem
/* An item in a regulatory track */
    {
    struct regItem *next;
    char *chrom;		/* Chromosome. Not allocated here. */
    int chromStart,chromEnd;	/* Half open coordinates. */
    double score;		/* Ideally something like -log(p). */
    struct regSource *source;   /* Source track/file for item. */
    };

struct regCluster
/* A cluster of items. */
    {
    struct regCluster *next;
    char *chrom;		/* Chromosome.  Not allocated here. */
    int chromStart, chromEnd;	/* Half open coordinates. */
    double score;		/* Sum of component scores. */
    double maxSubScore;		/* Max of component scores. */
    struct slRef *itemRefList;	/* List of references to component items. */
    };

struct regSource *regSourceLoadAll(char *fileName, int dimCount)
/* Read file, parse it line by line and return list of regSources. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int rowSize = dimCount + 6;
char *row[rowSize];
struct regSource *sourceList = NULL, *source;
while (lineFileNextRow(lf, row, rowSize))
    {
    /* Allocate struct and read in fixed fields. */
    AllocVar(source);
    source->dataSource = cloneString(row[0]);
    source->chromColIx = sqlUnsigned(row[1]);
    source->startColIx = sqlUnsigned(row[2]);
    source->endColIx = sqlUnsigned(row[3]);
    source->scoreColIx = sqlUnsigned(row[4]);
    source->normFactor = sqlDouble(row[5]);

    /* Read in dimension labels. */
    AllocArray(source->labels, dimCount);
    int i;
    for (i=0; i<dimCount; ++i)
        source->labels[i] = cloneString(row[i+6]);

    /* Calculate required columns. */
    int minColCount = max(source->chromColIx, source->startColIx);
    minColCount = max(minColCount, source->endColIx);
    minColCount = max(minColCount, source->scoreColIx);
    source->minColCount = minColCount + 1;
    slAddHead(&sourceList, source);
    }
lineFileClose(&lf);
slReverse(&sourceList);
return sourceList;
}

void clusterSource(struct regSource *source, struct hash *chromHash, 
	struct rbTreeNode *stack[128])
/* Read through data source and add items to it to rangeTrees in hash */
{
struct lineFile *lf = lineFileOpen(source->dataSource, TRUE);
struct lm *lm = chromHash->lm;	/* Local memory pool - share with hash */
char *row[source->minColCount];
struct regItem *item;
while (lineFileNextRow(lf, row, source->minColCount))
    {
    char *chrom = row[source->chromColIx];
    struct hashEl *hel = hashLookup(chromHash, chrom);
    if (hel == NULL)
        {
	struct rbTree *tree = rangeTreeNewDetailed(lm, stack);
	hel = hashAdd(chromHash, chrom, tree);
	}
    struct rbTree *tree = hel->val;
    lmAllocVar(lm, item);
    item->chrom = hel->name;
    item->chromStart = sqlUnsigned(row[source->startColIx]);
    item->chromEnd = sqlUnsigned(row[source->endColIx]);
    item->score = sqlDouble(row[source->scoreColIx]) * source->normFactor;
    if (item->score > 1000) item->score = 1000;
    item->source = source;
    rangeTreeAddValList(tree, item->chromStart, item->chromEnd, item);
    }

lineFileClose(&lf);
}

int cmpChromEls(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct hashEl *a = *((struct hashEl **)va);
const struct hashEl *b = *((struct hashEl **)vb);
return cmpWordsWithEmbeddedNumbers(a->name, b->name);
}

void addCluster(struct lm *lm, struct regItem *itemList, int start, int end,
	struct regCluster **pList)
/* Make cluster of all items that overlap start/end, and put it on list. */
{
struct regCluster *cluster;
lmAllocVar(lm, cluster);
double score = 0.0;
double maxSubScore = 0.0;
struct slRef  *refList = NULL, *ref;
struct regItem *item;
for (item = itemList; item != NULL; item = item->next)
    {
    if (rangeIntersection(start, end, item->chromStart, item->chromEnd) > 0)
	{
	lmAllocVar(lm, ref);
	ref->val = item;
	slAddHead(&refList, ref);
	score += item->score;
	if (item->score > maxSubScore) maxSubScore = item->score;
	}
    }
slReverse(&refList);
cluster->chrom = itemList->chrom;
cluster->chromStart = start;
cluster->chromEnd = end;
cluster->itemRefList = refList;
cluster->score = score;
cluster->maxSubScore = maxSubScore;
slAddHead(pList, cluster);
}

struct regCluster *clusterItems(struct lm *lm, struct regItem *itemList, 
	double forceJoinScore, double weakLevel)
/* Convert a list of items to a list of clusters of items.  This may break up clusters that
 * have weakly linked parts. 
      [                ]
      AAAAAAAAAAAAAAAAAA 
       BBBBBB   DDDDDD
        CCCC     EEEE
   gets tranformed into
       [    ]   [    ]
      AAAAAAAAAAAAAAAAAA 
       BBBBBB   DDDDDD
        CCCC     EEEE
   The strategy is to build a rangeTree of coverage, which might look something like so:
      123333211123333211 
   then define cluster ends that exceed the minimum limit, which is either 10% of the highest
   or forceJoinScore if 10% of the highest is more than forceJoinScore.  This will go to
   something like so:
        [---]   [----]   
   Finally the items that are overlapping a cluster are assigned to it.  Note that this
   may mean that an item may be in multiple clusters.
        [ABC]   [ ADE]
 */
{
int easyMax = round(1.0/weakLevel);
int itemCount = slCount(itemList);
struct regCluster *clusterList = NULL;
if (itemCount < easyMax)
    {
    struct regItem *item = itemList;
    int chromStart = item->chromStart;
    int chromEnd = item->chromEnd;
    for (item = item->next; item != NULL; item = item->next)
        {
	if (item->chromStart < chromStart) chromStart = item->chromStart;
	if (item->chromEnd > chromEnd) chromEnd = item->chromEnd;
	}
    addCluster(lm, itemList, chromStart, chromEnd, &clusterList);
    }
else
    {
    /* Make up coverage tree. */
    struct rbTree *covTree = rangeTreeNew();
    struct regItem *item;
    for (item = itemList; item != NULL; item = item->next)
	rangeTreeAddToCoverageDepth(covTree, item->chromStart, item->chromEnd);
    struct range *range, *rangeList = rangeTreeList(covTree);

    /* Figure out maximum coverage. */
    int maxCov = 0;
    for (range = rangeList; range != NULL; range = range->next)
        {
	int cov = ptToInt(range->val);
	if (cov > maxCov) maxCov = cov;
	}

    /* Figure coverage threshold. */
    int threshold = round(maxCov * weakLevel);
    if (threshold > forceJoinScore-1) threshold = forceJoinScore-1;

    /* Loop through emitting sections over threshold as clusters */
    boolean inRange = FALSE;
    boolean start = 0, end = 0;
    for (range = rangeList; range != NULL; range = range->next)
        {
	int cov = ptToInt(range->val);
	if (cov > threshold)
	    {
	    if (inRange)
	       end = range->end;
	    else
	       {
	       inRange = TRUE;
	       start = range->start;
	       end = range->end;
	       }
	    }
	else
	    {
	    if (inRange)
		{
		addCluster(lm, itemList, start, end, &clusterList);
		inRange = FALSE;
		}
	    }
	}
    if (inRange)
        addCluster(lm, itemList, start, end, &clusterList);
    }
slReverse(&clusterList);
return clusterList;
}

static int clusterIx = 0;

void outputClustersForChrom(char *chrom, struct rbTree *tree, FILE *fCluster, FILE *fBed)
/* Write out clusters on one chromosome */
{
struct range *range, *rangeList = rangeTreeList(tree);
verbose(2, "Got %d ranges on %s\n", slCount(rangeList), chrom);
struct lm *lm = lmInit(0);
for (range = rangeList; range != NULL; range = range->next)
    {
    struct regCluster *cluster, *clusterList = clusterItems(lm, range->val, 
    	clForceJoinScore, clWeakLevel);
    for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
        {
	struct slRef *ref, *refList=cluster->itemRefList;
	++clusterIx;
	struct regItem *item = refList->val;
	struct hash *uniqHash = hashNew(0);
	for (ref = refList; ref != NULL; ref = ref->next)
	    {
	    item = ref->val;
	    hashStore(uniqHash, item->source->dataSource);
	    fprintf(fCluster, "%d\t%s\t", clusterIx, item->chrom);
	    fprintf(fCluster, "%d\t%d\t", item->chromStart, item->chromEnd);
	    fprintf(fCluster, "%g", item->score);
	    int i;
	    for (i=0; i<clDims; ++i)
	       fprintf(fCluster, "\t%s", item->source->labels[i]);
	    fprintf(fCluster, "\n");
	    }
	double score = clScoreScale * cluster->maxSubScore;
	if (score > 1000) score = 1000;
	fprintf(fBed, "%s\t%d\t%d\t%d\t%g\n", chrom, 
		cluster->chromStart, cluster->chromEnd, uniqHash->elCount, score);
	hashFree(&uniqHash);
	}
    }
lmCleanup(&lm);
}

void regCluster(char *tableOfTables, char *outCluster, char *outBed)
/* regCluster - Cluster regulator regions. */
{
struct regSource *source, *sourceList = regSourceLoadAll(tableOfTables, clDims);
verbose(1, "Read %d sources from %s\n", slCount(sourceList), tableOfTables);
struct hash *chromHash = hashNew(0);
struct rbTreeNode *stack[128];
for (source = sourceList; source != NULL; source = source->next)
    {
    clusterSource(source, chromHash, stack);
    }

/* Get out list of chromosomes and process one at a time. */
FILE *fCluster = mustOpen(outCluster, "w");
FILE *fBed = mustOpen(outBed, "w");
struct hashEl *chrom, *chromList = hashElListHash(chromHash);
slSort(&chromList, cmpChromEls);
int totalClusters = 0;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct rbTree *tree = chrom->val;
    totalClusters += tree->n;
    outputClustersForChrom(chrom->name, tree, fCluster, fBed);
    }
verbose(1, "%d singly-linked clusters, %d clusters in %d chromosomes\n", 
	totalClusters, clusterIx, chromHash->elCount);
carefulClose(&fCluster);
carefulClose(&fBed);

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
regCluster(argv[1], argv[2], argv[3]);
return 0;
}
