/* writeCluster - save a cluster to file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "ggMrnaAli.h"
#include "geneGraph.h"
#include "rbTree.h"
#include "rangeTree.h"

static char const rcsid[] = "$Id: writeCluster.c,v 1.3 2007/01/31 08:05:37 kent Exp $";

struct rbTree *rangeTreeFromCluster(struct ggMrnaCluster *cluster)
/* Create a binary tree of more-or-less exons (non-overlapping
 * chromosome ranges) from a cluster. */
{
struct maRef *mr;
struct rbTree *exonTree = rbTreeNew(rangeCmp);
for (mr = cluster->refList; mr != NULL; mr = mr->next)
    {
    struct ggMrnaAli *ma = mr->ma;
    struct ggMrnaBlock *block;
    int i;
    for (i=0; i<ma->blockCount; ++i)
        {
	block = ma->blocks+i;
	rangeTreeAdd(exonTree, block->tStart, block->tEnd);
	}
    }
return exonTree;
}

void writeCluster(struct ggMrnaCluster *cluster, int chromSize, FILE *f)
/* Write out a cluster to file. */
{
struct maRef *mr;
static int id=0;
int refCount = slCount(cluster->refList);
struct rbTree *exonTree = rangeTreeFromCluster(cluster);
struct range *range, *rangeList = rangeTreeList(exonTree);
int chromStart = cluster->tStart, chromEnd = cluster->tEnd;

#ifdef OLD
if (cluster->strand[0] == '-')
    {
    slReverse(&rangeList);
    for (range = rangeList; range != NULL; range = range->next)
        {
	reverseIntRange(&range->start, &range->end, chromSize);
	}
    }
#endif

chromStart = rangeList->start;
range = slLastEl(rangeList);
chromEnd = range->end;

fprintf(f, "%s\t%d\t%d\t", cluster->tName, chromStart, chromEnd);
fprintf(f, "r%de%di%d\t", refCount, exonTree->n, ++id);
fprintf(f, "0\t");	/* score field */
fprintf(f, "%s\t", cluster->strand);
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
fprintf(f, "%d\t", refCount);
for (mr = cluster->refList; mr != NULL; mr = mr->next)
    {
    struct ggMrnaAli *ma = mr->ma;
    fprintf(f, "%s,", ma->qName);
    };
fprintf(f, "\n");
rbTreeFree(&exonTree);
}
