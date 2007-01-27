/* addCluster - Guts of rna clustering. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "binRange.h"
#include "options.h"
#include "nibTwo.h"
#include "psl.h"
#include "ggMrnaAli.h"
#include "geneGraph.h"

static int exonOverlap(struct ggMrnaCluster *cluster, int eStart, int eEnd)
/* Return the approximate number of bases overlapped between
 * cluster and an exon. */
{
/* This approximates overlap by taking biggest overlap with
 * any mRNA in cluster. */
struct ggAliInfo *da;
int oneOverlap, overlap = 0;

for (da = cluster->mrnaList; da != NULL; da = da->next)
    {
    struct ggVertex *v = da->vertices;
    int vCount = da->vertexCount;
    int i;
    for (i=0; i<vCount; i+=2,v+=2)
        {
        int mStart = v[0].position;
        int mEnd = v[1].position;
        int s = max(eStart, mStart);
        int e = min(eEnd, mEnd);
	oneOverlap = e - s;
	if (oneOverlap > overlap)
	    overlap = oneOverlap;
        }
    }
return overlap;
}

static int exonsOverlapCluster(struct ggMrnaCluster *cluster, struct ggMrnaCluster *ali)
/* Return the number of bases that overlap between cluster and alignment.
 * Alignment (ali) is a cluster too, but only with a single alignment in 
 * it. */
{
struct ggAliInfo *da = ali->mrnaList;
int i, vCount = da->vertexCount;
struct ggVertex *v = da->vertices;
int overlap = 0;

assert(da->next == NULL);
for (i=0; i<vCount; i+=2,v+=2)
    overlap += exonOverlap(cluster, v[0].position, v[1].position);
return overlap;
}

void addCluster(struct ggMrnaAli *maList, struct binKeeper *bins, struct dnaSeq *chrom)
/* Do basic clustering. */
{
struct ggMrnaAli *ma, *nextMa;
struct ggMrnaCluster *newCluster, *oldCluster;
struct binElement *binList = NULL, *binEl, *nextBinEl, *overlapList = NULL;

for (ma = maList; ma != NULL; ma = nextMa)
    {
    int overlap = 0, overlapSign = 0;
    char targetStrand = '?';
    nextMa = ma->next;

    /* Make up cluster for this RNA and merge with overlapping clusters
     * if any. */
    newCluster = ggMrnaSoftFilteredClusterOfOne(ma, chrom, 10, 130);
    if (newCluster == NULL)
	continue;

    /* Get list of overlapping clusters on either strand. */
    binList = binKeeperFind(bins, newCluster->tStart, newCluster->tEnd);

    /* Weed out clusters that don't overlap at exon level as well. 
     * While we're at this calculate predominant strand of overlap. */
    overlapList = NULL;
    for (binEl = binList; binEl != NULL; binEl = nextBinEl)
	{
	nextBinEl = binEl->next;
	oldCluster = binEl->val;
	overlap = exonsOverlapCluster(oldCluster, newCluster);
	if (overlap > 0)
	    {
	    slAddHead(&overlapList, binEl);
	    if (oldCluster->strand[0] == '+')
		overlapSign += overlap;
	    else if (oldCluster->strand[0] == '-')
		overlapSign -= overlap;
	    }
	else
	    {
	    freez(&binEl);
	    }
	}
    binList = NULL;

    /* Figure out strand to cluster on. */
    if (overlapSign != 0 && ma->orientation == 0)
        {
	targetStrand = (overlapSign < 0 ? '-' : '+');
	}
    else
        {
	if (ma->orientation < 0)
	    targetStrand = '-';
	else if (ma->orientation > 0)
	    targetStrand = '+';
	else
	    targetStrand = '?';
	}


    /* Weed out clusters that are on the wrong strand. */
    binList = overlapList;
    overlapList = NULL;
    for (binEl = binList; binEl != NULL; binEl = nextBinEl)
        {
	nextBinEl = binEl->next;
	oldCluster = binEl->val;
	if (oldCluster->strand[0] == '?' || oldCluster->strand[0] == targetStrand)
	    {
	    slAddHead(&overlapList, binEl);
	    }
	else
	    {
	    freez(&binEl);
	    }
	}
    binList = NULL;


    /* Merge new  cluster for this RNA and merge with overlapping 
     * old clusters if any. */
    for (binEl = overlapList; binEl != NULL; binEl = binEl->next)
        {
	struct ggMrnaCluster *oldCluster = binEl->val;
	char oldStrand = oldCluster->strand[0];

	binKeeperRemove(bins, oldCluster->tStart, oldCluster->tEnd, oldCluster);
	ggMrnaClusterMerge(newCluster, oldCluster);
	if (oldStrand != '?')
	    newCluster->strand[0] = oldStrand;
	}
    
    /* Add new cluster to bin and clean up. */
    binKeeperAdd(bins, newCluster->tStart, newCluster->tEnd, newCluster);
    slFreeList(&overlapList);
    }
}

