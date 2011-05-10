/* peakCluster - cluster peak calls from different sources. Used by regCluster
 * and encodeMergeReplicates programs. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "localmem.h"
#include "sqlNum.h"
#include "obscure.h"
#include "peakCluster.h"
#include "rangeTree.h"

struct peakSource *peakSourceLoadAll(char *fileName, int dimCount)
/* Read file, parse it line by line and return list of peakSources. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int rowSize = dimCount + 6;
char *row[rowSize];
struct peakSource *sourceList = NULL, *source;
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

void peakClusterMakerAddFromSource(struct peakClusterMaker *maker, struct peakSource *source)
/* Read through data source and add items to it to rangeTrees in maker */
{
struct hash *chromHash = maker->chromHash;
struct lineFile *lf = lineFileOpen(source->dataSource, TRUE);
struct lm *lm = chromHash->lm;	/* Local memory pool - share with hash */
char *row[source->minColCount];
struct peakItem *item;
char *line;
while (lineFileNextReal(lf, &line))
    {
    char *asciiLine = lmCloneString(lm, line);
    int wordCount = chopByWhite(line, row, source->minColCount);
    lineFileExpectAtLeast(lf, source->minColCount, wordCount);
    char *chrom = row[source->chromColIx];
    struct hashEl *hel = hashLookup(chromHash, chrom);
    if (hel == NULL)
        {
	struct rbTree *tree = rangeTreeNewDetailed(lm, maker->stack);
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
    item->asciiLine = asciiLine;
    rangeTreeAddValList(tree, item->chromStart, item->chromEnd, item);
    }
lineFileClose(&lf);
}

static void addCluster(struct lm *lm, struct peakItem *itemList, int start, int end,
	struct peakCluster **pList)
/* Make cluster of all items that overlap start/end, and put it on list. */
{
struct peakCluster *cluster;
lmAllocVar(lm, cluster);
double score = 0.0;
double maxSubScore = 0.0;
struct slRef  *refList = NULL, *ref;
struct peakItem *item;
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

struct peakCluster *peakClusterItems(struct lm *lm, struct peakItem *itemList, 
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
   then define cluster ends that exceed the minimum limit, which is either weakLevel
   (usually 10%) of the highest or forceJoinScore if weakLevel times the highest is 
   more than forceJoinScore.  This will go to something like so:
        [---]   [----]   
   Finally the items that are overlapping a cluster are assigned to it.  Note that this
   may mean that an item may be in multiple clusters.
        [ABC]   [ ADE]
 */
{
int easyMax = round(1.0/weakLevel);
int itemCount = slCount(itemList);
struct peakCluster *clusterList = NULL;
if (itemCount < easyMax)
    {
    struct peakItem *item = itemList;
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
    struct peakItem *item;
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

struct peakClusterMaker *peakClusterMakerNew()
/* Return a new peakClusterMaker. */
{
struct peakClusterMaker *maker;
AllocVar(maker);
maker->chromHash = hashNew(0);
return maker;
}

void peakClusterMakerFree(struct peakClusterMaker **pMaker)
/* Free up a peakClusterMaker. */
{
struct peakClusterMaker *maker = *pMaker;
if (maker != NULL)
    {
    hashFree(&maker->chromHash);
    freez(pMaker);
    }
}

static int cmpChromEls(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct hashEl *a = *((struct hashEl **)va);
const struct hashEl *b = *((struct hashEl **)vb);
return cmpWordsWithEmbeddedNumbers(a->name, b->name);
}

struct hashEl *peakClusterMakerChromList(struct peakClusterMaker *maker)
/* Return list of chromosomes.  In hashEl format where the hashEl val is
 * a rangeTree filled with items. */
{
struct hashEl *chromList =  hashElListHash(maker->chromHash);
slSort(&chromList, cmpChromEls);
return chromList;
}

