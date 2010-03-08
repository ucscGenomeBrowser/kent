/* regCluster - Cluster regulator regions. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "localmem.h"
#include "rangeTree.h"

static char const rcsid[] = "$Id: regCluster.c,v 1.1 2010/03/08 23:35:07 kent Exp $";

int clDims = 1;
double clScoreScale = 2.0;

void usage()
/* Explain usage and exit. */
{
errAbort(
"regCluster - Cluster regulator regions\n"
"usage:\n"
"   regCluster tableOfTables output.cluster output.bed\n"
"Where the table-of-tables is space or tab separated in the format:\n"
"   <fileName> <chrom> <start> <end> <score> <dim1 label> ... <dimN label>\n"
"where chrom, start, end are the indexes (starting with 0) of the chromosome, start, and end\n"
"fields in the file.\n"
"for example\n"
"   simpleReg.bed 0 1 2 4 A 5 a 6\n"
"options:\n"
"   -dims=N - number of dimensions in data.  Would be 2 for cell-line + antibody. Default %d\n"
"   -scoreScale=0.N - scale score by this factor. Default %g\n"
, clDims
, clScoreScale
);
}

static struct optionSpec options[] = {
   {"dims", OPTION_INT},
   {"scoreScale", OPTION_DOUBLE},
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

void outputClustersForChrom(char *chrom, struct rbTree *tree, FILE *fCluster, FILE *fBed)
/* Write out clusters on one chromosome */
{
struct range *range, *rangeList = rangeTreeList(tree);
uglyf("Got %d ranges\n", slCount(rangeList));
int clusterIx = 0;
for (range = rangeList; range != NULL; range = range->next)
    {
    struct regItem *item, *itemList = range->val;
    ++clusterIx;
    double totalScore = 0.0;
    int start = itemList->chromStart, end = itemList->chromEnd;
    for (item = itemList; item != NULL; item = item->next)
        {
	fprintf(fCluster, "%d\t%s\t", clusterIx, item->chrom);
	fprintf(fCluster, "%d\t%d\t", item->chromStart, item->chromEnd);
	fprintf(fCluster, "%d", (int)item->score);
	int i;
	for (i=0; i<clDims; ++i)
	   fprintf(fCluster, "\t%s", item->source->labels[i]);
	fprintf(fCluster, "\n");
	start = min(start, item->chromStart);
	end = max(end, item->chromEnd);
	totalScore += item->score;
	}
    int score = clScoreScale * totalScore;
    if (score > 1000) score = 1000;
    fprintf(fBed, "%s\t%d\t%d\t%d\t%d\n", chrom, start, end, slCount(itemList), score);
    }
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
uglyf("%d chromosomes in sources\n", chromHash->elCount);

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
uglyf("%d total clusters in %d chromosomes\n", totalClusters, chromHash->elCount);
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
regCluster(argv[1], argv[2], argv[3]);
return 0;
}
