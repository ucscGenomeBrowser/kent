/* peakCluster - cluster peak calls from different sources. */

#ifndef PEAKCLUSTER_H
#define PEAKCLUSTER_H

struct peakDim
/* A peak dimension */
    {
    int colIx;		/* Column index in table. */
    char *label;	/* Label */
    };

struct peakSource 
/* A source of peak information */
    {
    struct peakSource *next;
    char *dataSource;		/* File (or table) */
    int chromColIx;		/* Chromosome column index. */
    int startColIx;		/* Start coordinate column index. */
    int endColIx;		/* End ccoordinate column ix. */
    int scoreColIx;		/* Index for score column. */
    double normFactor;		/* Multiply this to get browser score. */
    char **labels;		/* Label for each dimension */
    int minColCount;		/* Minimum number of columns. */
    };

struct peakItem
/* An item in a peak track */
    {
    struct peakItem *next;
    char *chrom;		/* Chromosome. Not allocated here. */
    int chromStart,chromEnd;	/* Half open coordinates. */
    double score;		/* Ideally something like -log(p). */
    struct peakSource *source;   /* Source track/file for item. */
    char *asciiLine;		/* Ascii representation of line. */
    };

struct peakCluster
/* A cluster of items. */
    {
    struct peakCluster *next;
    char *chrom;		/* Chromosome.  Not allocated here. */
    int chromStart, chromEnd;	/* Half open coordinates. */
    double score;		/* Sum of component scores. */
    double maxSubScore;		/* Max of component scores. */
    struct slRef *itemRefList;	/* List of references to component items. */
    };

struct peakClusterMaker
/* Help make a cluster of peaks on multiple-chromosome data sets. */
    {
    struct peakClusterMaker *next;
    struct hash *chromHash;	   /* Key is chromosome, value is a rbTree of items. */
    struct rbTreeNode *stack[128]; /* Stack for rbTree evaluations. */
    };

struct peakClusterMaker *peakClusterMakerNew();
/* Return a new peakClusterMaker. */

void peakClusterMakerFree(struct peakClusterMaker **pMaker);
/* Free up a peakClusterMaker. */

struct hashEl *peakClusterMakerChromList(struct peakClusterMaker *maker);
/* Return list of chromosomes.  In hashEl format where the hashEl val is
 * a rangeTree filled with items. Do a slFreeList when done. */

struct peakSource *peakSourceLoadAll(char *fileName, int dimCount);
/* Read file, parse it line by line and return list of peakSources. */

void peakClusterMakerAddFromSource(struct peakClusterMaker *maker, struct peakSource *source);
/* Read through data source and add items to it to rangeTrees in maker */

struct peakCluster *peakClusterItems(struct lm *lm, struct peakItem *itemList, 
	double forceJoinScore, double weakLevel);
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

#endif /* PEAKCLUSTER_H */

