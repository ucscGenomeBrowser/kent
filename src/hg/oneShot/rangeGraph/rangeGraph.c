/* rangeGraph - test ideas about graph that tracks possible distance ranges. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "diGraph.h"
#include "rgi.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "rangeGraph - test ideas about graph that tracks possible distance ranges\n"
  "usage:\n"
  "   rangeGraph XXX\n");
}

struct rgi *readRgi(char *inName)
{
struct rgi *rgiList = NULL, *rgi;
struct lineFile *lf = lineFileOpen(inName, TRUE);
int wordCount;
char *words[8];

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    lineFileExpectWords(lf, 4, wordCount);
    rgi = rgiLoad(words);
    slAddHead(&rgiList, rgi);
    uglyf("%s %s: min %d, max %d\n", rgi->a, rgi->b, rgi->minDistance, rgi->maxDistance);
    }
lineFileClose(&lf);
slReverse(&rgiList);
return rgiList;
}

struct edgeRange
/* A distance range. */
   {
   int minDist;	/* Must be at least this long. */
   int maxDist;     /* But no longer than this. */
   };

struct nodeRange
/* A distance range. */
   {
   int minDist;	/* Must be at least this long. */
   int maxDist;     /* But no longer than this. */
   };

void maybeMakeNode(struct diGraph *graph, char *nodeName)
/* Make node in graph if not there already. */
{
if (!dgFindNode(graph, nodeName))
    {
    struct nodeRange *r;
    AllocVar(r);
    dgAddNode(graph, nodeName, r);
    }
}

boolean localFindEdgeRange(struct dgEdge *edge, int *retMin, int *retMax)
/* Find min and max vals for range. */
{
struct edgeRange *range = edge->val;
*retMin = range->minDist;
*retMax = range->maxDist;
return TRUE;
}

boolean tryAddingRangeEdge(struct diGraph *graph, struct dgNode *a, struct dgNode *b,
	int minDist, int maxDist)
/* Try adding node from a to b to graph with the given min and max distance
 * constraints.  Return FALSE if can't. */
{
struct edgeRange oldRange, *r;
boolean gotOldRange = FALSE;
struct dgEdge *edge;

edge = dgDirectlyFollows(graph, a, b);
if (edge != NULL)
    {
    /* Save old value of edge in case have to back out. */
    r = edge->val;
    oldRange = *r;
    gotOldRange = TRUE;
    }
else
   {
   edge = dgConnect(graph, a, b);
   AllocVar(r);
   edge->val = r;
   }
r->minDist = minDist;
r->maxDist = maxDist;
if (!dgRangesConsistent(graph, localFindEdgeRange))
    {
    if (gotOldRange)
        {
	*r = oldRange;
	}
    else
        {
	freeMem(&edge->val);
	dgDisconnect(graph, a, b);
	}
    return FALSE;
    }
else
    return TRUE;
}

void rangeGraph(char *inName)
/* rangeGraph - test ideas about graph that tracks possible distance ranges. */
{
struct rgi *rgiList, *rgi;
struct diGraph *graph = dgNew();
boolean ok;

/* Read in range list and add all nodes to graph. */
rgiList = readRgi(inName);
for (rgi = rgiList; rgi != NULL; rgi = rgi->next)
    {
    maybeMakeNode(graph, rgi->a);
    maybeMakeNode(graph, rgi->b);
    }

for (rgi = rgiList; rgi != NULL; rgi = rgi->next)
    {
    struct dgNode *a, *b;
    a = dgFindNode(graph, rgi->a);
    b = dgFindNode(graph, rgi->b);
    ok = tryAddingRangeEdge(graph, a, b, rgi->minDistance, rgi->maxDistance);
    printf("Added %s->%s min %d, max %d. %s\n",
    	rgi->a, rgi->b, rgi->minDistance, rgi->maxDistance,
	(ok ? "ok" : "conflict") );
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
rangeGraph(argv[1]);
return 0;
}
