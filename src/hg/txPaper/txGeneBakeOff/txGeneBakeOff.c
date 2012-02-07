/* txGeneBakeOff - Compare gene finder results to reference annotations.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "obscure.h"
#include "hdb.h"
#include "binRange.h"
#include "rangeTree.h"
#include "refCluster.h"
#include "bed.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneBakeOff - Compare gene finder results to reference annotations.\n"
  "usage:\n"
  "   txGeneBakeOff db refReviewed.lst ref.cluster geneTrack\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct bed *hWholeTrackAsBedList(char *track)
/* Get entire track as a list of beds. */
{
struct slName *chrom, *chromList = hAllChromNames();
struct bed *bedList = NULL;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct bed *chromBedList = hGetBedRange(track, chrom->name, 0, 0, NULL);
    bedList = slCat(chromBedList, bedList);
    }
slFreeList(&chromList);
return bedList;
}

double bedOverlapRatio(struct bed *a, struct bed *b)
/* Return TRUE if a overlaps b by at least given ratio. */
{
int aSize = bedTotalBlockSize(a);
int bSize = bedTotalBlockSize(b);
int overlapSize = bedSameStrandOverlap(a,b);
if (aSize == bSize && aSize == overlapSize)
    return 1.0;	/* Avoid rounding here. */
double x = overlapSize;
double aCov = x / aSize;
double bCov = x / bSize;
return min(aCov, bCov);
}

struct bed *mostOverlappingBed(struct bed *ref, struct hash *geneHash, double *retRatio)
/* Find most overlapping gene to ref. */
{
struct bed *bestBed = NULL;
double bestRatio = 0;
struct binKeeper *bk = hashFindVal(geneHash, ref->chrom);
if (bk != NULL)
    {
    struct binElement *el, *list = binKeeperFind(bk, ref->chromStart, ref->chromEnd);
    for (el = list; el != NULL; el = el->next)
        {
	struct bed *bed = el->val;
	if (bed->strand[0] == ref->strand[0])
	    {
	    double ratio = bedOverlapRatio(ref, bed);
	    if (ratio > bestRatio)
	        {
		bestRatio = ratio;
		bestBed = bed;
		}
	    }
	}
    }
*retRatio = bestRatio;
return bestBed;
}


boolean findMostOverlappingInCluster(struct refCluster *cluster, struct hash *geneHash, struct hash *refBedHash,
	struct bed **retGene, struct bed **retRef, double *retRatio)
/* Find gene that most overlaps any gene in cluster. */
{
struct bed *bestRef = NULL, *bestGene = NULL;
double bestRatio = 0;
int i;
for (i=0; i<cluster->refCount; ++i)
    {
    char *refName = cluster->refArray[i];
    double ratio;
    struct bed *ref = hashFindVal(refBedHash, refName);
    if (ref != NULL)
	{
	struct bed *gene = mostOverlappingBed(ref, geneHash, &ratio);
	if (gene != NULL && ratio > bestRatio)
	    {
	    bestRatio = ratio;
	    bestGene = gene;
	    bestRef = ref;
	    }
	}
    }
*retGene = bestGene;
*retRef = bestRef;
*retRatio = bestRatio;
return (bestGene != NULL);
}

struct rbTree *bedBkToTree(struct binKeeper *bk)
/* Make a rangeTree covering all exons in tree. */
{
struct binElement *el, *list = binKeeperFindAll(bk);
struct rbTree *tree = rangeTreeNew();
for (el = list; el != NULL; el = el->next)
    {
    struct bed *bed = el->val;
    bedIntoRangeTree(bed, tree);
    }
slFreeList(&list);
return tree;
}

long rangeTreeTotalSize(struct rbTree *tree)
/* Return total size of all ranges in tree. */
{
struct range *range, *list = rangeTreeList(tree);
long total = 0;
for (range = list; range != NULL; range = range->next)
    total += range->end - range->start;
return total;
}

long rangeTreeRangeTreeOverlap(struct rbTree *a, struct rbTree *b)
/* Return total overlap between two range trees. */
{
struct range *range, *list = rangeTreeList(a);
long total = 0;
for (range = list; range != NULL; range = range->next)
    total += rangeTreeOverlapSize(b, range->start, range->end);
return total;
}

double calcBaseCoverage(struct hash *refHash, struct hash *geneHash)
/* Figure proportion refBases covered by genes.  Both refHash and geneHash
 * are keyed by chromosome and filled with genes. */
{
struct hashEl *chrom, *chromList = hashElListHash(refHash);
long totalRef = 0, totalOverlap = 0;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    char *chromName = chrom->name;
    struct binKeeper *refBk = chrom->val;
    struct rbTree *refTree = bedBkToTree(refBk);
    totalRef += rangeTreeTotalSize(refTree);
    struct binKeeper *geneBk = hashFindVal(geneHash, chromName);
    if (geneBk != NULL)
	{
	struct rbTree *geneTree = bedBkToTree(geneBk);
        totalOverlap += rangeTreeRangeTreeOverlap(refTree, geneTree);
	rangeTreeFree(&geneTree);
	}
    rangeTreeFree(&refTree);
    }
return (double)totalOverlap / totalRef;
}

void txGeneBakeOff(char *database, char *refRevFile, char *refClusterFile, char *geneTrack)
/* txGeneBakeOff - Compare gene finder results to reference annotations.. */
{
hSetDb(database);

/* Make list of our clusters. */
struct refCluster *cluster, *clusterList = refClusterLoadAll(refClusterFile);

/* Make list of only refseqs in reviewed list. */
struct hash *refRevOnlyHash = hashWordsInFile(refRevFile, 16);
struct bed *refAll = bedThickOnlyList(hWholeTrackAsBedList("refGene"));
struct bed *refList = NULL, *nextRef, *ref;
struct hash *refBedHash = hashNew(18);
for (ref = refAll; ref != NULL; ref = nextRef)
    {
    nextRef = ref->next;
    if (hashLookup(refRevOnlyHash, ref->name))
        {
	slAddHead(&refList, ref);
	hashAdd(refBedHash, ref->name, ref);
	}
    }
verbose(2, "%d of %d reviewed are still in refGene track\n", 
	slCount(refList), refRevOnlyHash->elCount);

/* Turn this into hash. */
struct hash *refHash = bedsIntoKeeperHash(refList);
verbose(2, "Loaded %d items from %s into %d chromosomes\n", 
	slCount(refList), refRevFile, refHash->elCount);

struct bed *geneList = bedThickOnlyList(hWholeTrackAsBedList(geneTrack));
struct hash *geneHash = bedsIntoKeeperHash(geneList);
verbose(2, "Loaded %d items from %s into %d chromosomes\n", 
	slCount(geneList), geneTrack, geneHash->elCount);

int allCount = 0, allMiss = 0;
int allExact = 0, allClose = 0, allHalf = 0, allAny = 0;
// struct hash *uniqHash = hashNew(0);
for (ref = refList; ref != NULL; ref = ref->next)
   {
   double ratio;
   struct bed *gene = mostOverlappingBed(ref, geneHash, &ratio);
   if (gene != NULL)
       {
       ++allAny;
       if (ratio == 1.0)
           ++allExact;
       else if (ratio >= 0.80)
           ++allClose;
       else if (ratio >= 0.50)
           ++allHalf;
       }
    else
       ++allMiss;
   ++allCount;
   }
printf("Exact match:    %d (%4.2f%%)\n", allExact, 100.0 * allExact/allCount);
printf("80%% match:     %d (%4.2f%%)\n", allClose, 100.0 * allClose/allCount);
//printf("50%% match:     %d (%4.2f%%)\n", allHalf, 100.0 * allHalf/allCount);
//printf("any match:      %d (%4.2f%%)\n", allAny, 100.0 * allAny/allCount);
//printf("Clean miss:     %d (%4.2f%%)\n", allMiss, 100.0 * allMiss/allCount);

int anyCount = 0, anyMiss = 0;
int anyExact = 0, anyClose = 0, anyHalf = 0, anyAny = 0;
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
   {
   struct bed *gene, *ref;
   double ratio;
   if (findMostOverlappingInCluster(cluster, geneHash, refBedHash, &gene, &ref, &ratio))
       {
       ++anyAny;
       if (ratio == 1.0)
           ++anyExact;
       else if (ratio >= 0.80)
           ++anyClose;
       else if (ratio >= 0.50)
           ++anyHalf;
       }
    else
       ++anyMiss;
   ++anyCount;
   }

// printf("Total reviewed clusters: %d\n", anyCount);
printf("Exact match any:    %d (%4.2f%%)\n", anyExact, 100.0 * anyExact/anyCount);
printf("80%% match any:     %d (%4.2f%%)\n", anyClose, 100.0 * anyClose/anyCount);
// printf("50%% match any:     %d (%4.2f%%)\n", anyHalf, 100.0 * anyHalf/anyCount);
// printf("any match any:      %d (%4.2f%%)\n", anyAny, 100.0 * anyAny/anyCount);
// printf("Clean miss any:     %d (%4.2f%%)\n", anyMiss, 100.0 * anyMiss/anyCount);

printf("Base coverage:  (%4.2f%%)\n", 100.0*calcBaseCoverage(refHash, geneHash));

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
txGeneBakeOff(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
