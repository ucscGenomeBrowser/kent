/* clusterPsl - Make clusters of mRNA and ESTs.  Optionally turn clusters into
 * graphs. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "binRange.h"
#include "options.h"
#include "psl.h"
#include "rangeTree.h"
#include "geneGraph.h"
#include "altGraph.h"
#include "nib.h"
#include "twoBit.h"
#include "nibTwo.h"

static char const rcsid[] = "$Id: clusterPsl.c,v 1.9.70.1 2008/07/31 02:24:00 markd Exp $";

int maxMergeGap = 5;
char *prefix = "c";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "clusterPsl - Make clusters of mRNA aligments\n"
  "usage:\n"
  "   clusterPsl input output.bed\n"
  "where input is a psl file (not necessarily sorted)\n"
  "      output is a text file full of cluster info\n"
  "options:\n"
  "   -verbose=2 Make output more verbose.\n"
  "   -agx=output.agx - Create splicing graphs. Note this is usually done\n"
  "                     with txBedToGraph these days.\n"
  "   -dna=db.2bit - DNA - two bit file or nib dir.\n"
  "   -maxMergeGap=N Merge blocks separated by no more than this. Default %d\n"
  "   -prefix=name - Prefix to add before number in bed name. Default %s\n"
      , maxMergeGap, prefix
  );
}

static struct optionSpec options[] = {
   {"agx", OPTION_STRING},
   {"dna", OPTION_STRING},
   {"maxMergeGap", OPTION_INT},
   {"prefix", OPTION_STRING},
   {NULL, 0},
};


struct pslCluster
/* A cluster of overlapping (at the block level on the same strand)
 * alignments. */
    {
    struct pslCluster *next;
    int tStart,tEnd;
    struct psl *pslList;
    struct rbTree *exonTree;
    };

void pslClusterFree(struct pslCluster **pCluster)
/* Free up memory associated with cluster. */
{
struct pslCluster *cluster = *pCluster;
if (cluster != NULL)
    {
    rbTreeFree(&cluster->exonTree);
    pslFreeList(&cluster->pslList);
    freez(pCluster);
    }
}

void pslClusterFreeList(struct pslCluster **pList)
/* Free a list of dynamically allocated pslCluster's */
{
struct pslCluster *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    pslClusterFree(&el);
    }
*pList = NULL;
}

struct pslCluster *pslClusterNew()
/* Create cluster around a single alignment. */
{
struct pslCluster *cluster;
AllocVar(cluster);
cluster->exonTree = rangeTreeNew();
cluster->tStart = BIGNUM;
cluster->tEnd = -BIGNUM;
return cluster;
}


void pslClusterAdd(struct pslCluster *cluster, struct psl *psl)
/* Add new psl to cluster.  Note, uses psl->next to put psl on list
 * inside of cluster. */
{
cluster->tStart = min(psl->tStart, cluster->tStart);
cluster->tEnd = max(psl->tEnd, cluster->tEnd);
int block, blockCount = psl->blockCount;
for (block = 0; block < blockCount; ++block)
    {
    int blockSize = psl->blockSizes[block];
    int tStart = psl->tStarts[block];
    int tEnd = tStart + blockSize;
    rangeTreeAdd(cluster->exonTree, tStart, tEnd);
    }
slAddHead(&cluster->pslList, psl);
}

void pslClusterMerge(struct pslCluster *a, struct pslCluster **pB)
/* Merge b into a.  Destroys b. */
{
struct pslCluster *b = *pB;
a->pslList = slCat(a->pslList, b->pslList);
b->pslList = NULL;
a->tStart = min(a->tStart, b->tStart);
a->tEnd = max(a->tEnd, b->tEnd);
struct range *range;
for (range = rangeTreeList(b->exonTree); range != NULL; range = range->next)
    rangeTreeAdd(a->exonTree, range->start, range->end);
pslClusterFree(pB);
}

boolean pslIntersectsCluster(struct pslCluster *cluster, struct psl *psl)
/* Return TRUE if any block in psl intersects with cluster. */
{
int block, blockCount = psl->blockCount;
for (block = 0; block < blockCount; ++block)
    {
    struct range tempR;
    int start = psl->tStarts[block];
    int end = start + psl->blockSizes[block];
    tempR.start = start;
    tempR.end = end;
    if (rbTreeFind(cluster->exonTree, &tempR))
        return TRUE;
    }
return FALSE;
}

void pslIntoBinsOfClusters(struct psl *psl, struct binKeeper *bins)
/* Add projection onto target into a binKeeper full of pslClusters.
 * This will create and merge clusters as need be. */
{
/* Create new cluster around psl. */
struct pslCluster *newCluster = pslClusterNew();
pslClusterAdd(newCluster, psl);

/* Merge in any existing overlapping clusters.. */
struct binElement *bel, *belList = binKeeperFind(bins, psl->tStart, psl->tEnd);
for (bel = belList; bel != NULL; bel = bel->next)
    {
    struct pslCluster *oldCluster = bel->val;
    if (pslIntersectsCluster(oldCluster, psl))
	{
	binKeeperRemove(bins, oldCluster->tStart, oldCluster->tEnd, oldCluster);
	pslClusterMerge(oldCluster, &newCluster);
	newCluster = oldCluster;
	}
    }
slFreeList(&belList);

/* Add to binKeeper. */
binKeeperAdd(bins, newCluster->tStart, newCluster->tEnd, newCluster);
}

struct pslCluster *clusterPslsOnChrom(struct psl *pslList)
/* Make clusters of overlapping alignments on a single chromosome. 
 * Return a list of such clusters. */
{
if (pslList == NULL)
    return NULL;

/* Creat binKeeper full of clusters. */
struct psl *psl, *next = NULL;
struct binKeeper *bins = binKeeperNew(0, pslList->tSize);
for (psl = pslList; psl != NULL; psl = next)
    {
    next = psl->next;
    pslIntoBinsOfClusters(psl, bins);
    }

/* Convert from binKeeper of clusters to simple list of clusters. */
struct pslCluster *clusterList = NULL;
struct binElement *binList, *bin;
binList = binKeeperFindAll(bins);
for (bin = binList; bin != NULL; bin = bin->next)
    {
    struct pslCluster *cluster = bin->val;
    slAddHead(&clusterList, cluster);
    }
slReverse(&clusterList);

/* Clean up and go home. */
binKeeperFree(&bins);
return clusterList;
}



void writeClusterGraph(struct pslCluster *cluster, struct dnaSeq *chrom, 
	char *chromName, FILE *f)
/* Create a geneGraph out of cluster, and write it to file. */
{
struct ggMrnaAli *maList = pslListToGgMrnaAliList(cluster->pslList, 
	chromName, 0, chrom->size, chrom, maxMergeGap);
struct ggMrnaInput *ci = ggMrnaInputFromAlignments(maList, chrom);
struct ggMrnaCluster *mcList = ggClusterMrna(ci);
struct ggMrnaCluster *mc;
for (mc = mcList; mc != NULL; mc = mc->next)
    {
    // FIXME: not sure how this worked, as ggGraphConsensusCluster wants to talk
    // to a database, but this program doesn't take a databases
    struct geneGraph *gg = ggGraphConsensusCluster(NULL, mc, ci, NULL, FALSE);
    struct altGraphX *ag = ggToAltGraphX(gg);
    if (ag != NULL)
	{
	static int id=0;
	char name[16];
	safef(name, sizeof(name), "a%d", ++id);
	freez(&ag->name);
	ag->name = name;
	altGraphXTabOut(ag, f);
	ag->name = NULL;
	}
    altGraphXFree(&ag);
    freeGeneGraph(&gg);
    }
ggFreeMrnaClusterList(&mcList);
ggMrnaAliFreeList(&maList);
freez(&ci);	/* Note - DON'T call freeGgMrnaInput, it'll free chrom! */
}

void pslClusterWrite(struct pslCluster *cluster, FILE *f)
/* Write out info on cluster to file as a bed12 + 2 */
{
static int id=0;
int pslCount = slCount(cluster->pslList);
struct rbTree *exonTree = cluster->exonTree;
struct range *range, *rangeList = rangeTreeList(exonTree);
int chromStart = cluster->tStart;
int chromEnd = cluster->tEnd;

assert(cluster->tStart == rangeList->start);
fprintf(f, "%s\t%d\t%d\t", cluster->pslList->tName, chromStart, chromEnd);
fprintf(f, "%s%d\t", prefix, ++id);
fprintf(f, "0\t");	/* score field */
fprintf(f, "%s\t", cluster->pslList->strand);
fprintf(f, "%d\t", chromStart);	/* thick start */
fprintf(f, "%d\t", chromEnd);	/* thick end */
fprintf(f, "0\t");	/* itemRgb*/
fprintf(f, "%d\t", exonTree->n);	/* Block count */
for (range = rangeList; range != NULL; range = range->next)
    fprintf(f, "%d,", range->end - range->start);
fprintf(f, "\t");
for (range = rangeList; range != NULL; range = range->next)
    fprintf(f, "%d,", range->start - chromStart);
fprintf(f, "\t");
fprintf(f, "%d\t", pslCount);
struct psl *psl;
for (psl = cluster->pslList; psl != NULL; psl = psl->next)
    {
    fprintf(f, "%s,", psl->qName);
    };
fprintf(f, "\n");
}

void clusterPsl(char *pslName, char *clusterOut, char *dnaSource, char *agxOut)
/* Transform a file full of psl's into a file full of rnaClusters */
{
FILE *fCluster = mustOpen(clusterOut, "w");

/* Set up DNA seqence accesser */
struct nibTwoCache *nibTwo = NULL;
if (dnaSource != NULL)
    nibTwo = nibTwoCacheNew(dnaSource);

FILE *fAgx = NULL;
if (agxOut != NULL)
    fAgx = mustOpen(agxOut, "w");
  
struct dnaSeq *chrom = NULL;

/* Load in list and sort by chromosomes. */
struct psl *pslList = pslLoadAll(pslName);
slSort(&pslList, pslCmpTargetAndStrand);

/* Go through input one chromosome at a time. */
struct psl *chromStart, *chromEnd = NULL;
char *oldChromName = cloneString("");
for (chromStart = pslList; chromStart != NULL; chromStart = chromEnd)
    {
    /* Find chromosome end. */
    char *chromName = chromStart->tName;
    char strand = chromStart->strand[0];
    struct psl *psl, *dummy, **endAddress = &dummy;
    for (psl = chromStart; psl != NULL; psl = psl->next)
        {
	if (!sameString(psl->tName, chromName))
	    break;
	if (psl->strand[0] != strand)
	    break;
	endAddress = &psl->next;
	}
    chromEnd = psl;

    /* Terminate list before next chromosome */
    *endAddress = NULL;

    /* Get chromosome sequence */
    verbose(1, "chrom %s %s\n", chromStart->tName, chromStart->strand); 
    if (nibTwo != NULL && !sameString(chromName, oldChromName))
        {
	dnaSeqFree(&chrom);
	chrom = nibTwoCacheSeq(nibTwo, chromName);
	toLowerN(chrom->dna, chrom->size);
	verbose(2, "Loaded %d bases in %s\n", chrom->size, chromName);
	}

    /* Create clusters. */
    struct pslCluster *clusterList = clusterPslsOnChrom(chromStart);

    /* Write clusters to file. */
    struct pslCluster *cluster;
    for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
	 {
         pslClusterWrite(cluster, fCluster);
	 if (fAgx != NULL)
	     writeClusterGraph(cluster, chrom, chromName, fAgx);
	 }

    freez(&oldChromName);
    oldChromName = cloneString(chromName);
    pslClusterFreeList(&clusterList);	/* Note free's psls as well! */
    }
nibTwoCacheFree(&nibTwo);
carefulClose(&fCluster);
carefulClose(&fAgx);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
if (optionExists("agx") && !optionExists("dna"))
   errAbort("Need to use dna option with agx option.");
maxMergeGap = optionInt("maxMergeGap", maxMergeGap);
prefix = optionVal("prefix", prefix);
clusterPsl(argv[1], argv[2], optionVal("dna", NULL), optionVal("agx", NULL));
return 0;
}
