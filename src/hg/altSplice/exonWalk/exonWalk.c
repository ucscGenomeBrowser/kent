/** exonWalk.c - program to construct major exon isoforms by
   walking a series of paths through the exon graph. */
#include "common.h"
#include "geneGraph.h"
#include "altGraphX.h"
#include "exonGraph.h"
#include "jksql.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "hdb.h"
#include "heap.h"

void usage()
{
errAbort("exonWalk - uses altGraphX files to construct a splicing graph\n"
	 "which is a combination of all the paths seen by RNA and EST evidence.\n"
	 "The paths are joined where there are overlaps and the resulting super\n"
	 "graph is walked to determine a set of transcripts which are not sub-paths\n"
	 "of each other. Uses some heuristics to trim paths that contain rare nodes.\n"
	 "options:\n"
	 "\tminPercent - Minimum percentage of total transcripts that exon\n"
	 "\t\tvertices must be seen in. (.05 default)\n"
	 "\ttrumpSize - If unique portion of exon is >= trumpSize it will be kept\n"
	 "\t\teven if it is below minPercent.(100 default)\n"
	 "usage:\n\t"
	 "exonWalk <infile.altGraphX.tab> <outFile.bed>\n");
}

int exonProblem = -1;
int exonOk = 0;

int exonNodeCmp(const void *va, const void *vb)
/* Compare based on tStart. */
{
const struct exonNode *a = va;
const struct exonNode *b = vb;	
return a->tStart - b->tStart;
}

struct exonGraph *currentGraph = NULL; /** Pointer to current graph, useful for debugging. */
boolean debug = FALSE; /** Turns on some debugging output. */

void nodeSanityCheck(struct exonGraph *sg)
/** Loop through and make sure all the nodes have legit ids. */
{
int i,j;
for(i=0;i<sg->nodeCount; i++)
    {
    struct exonNode *sn = sg->nodes[i];
    for(j=0;j<sn->edgeOutCount;j++)
	{
	if(sn->edgesOut[j] >= sg->nodeCount)
	    errAbort("exonWalk::nodeSanityCheck() - node with id %d when max is %d\n", sn->edgesOut[j], sg->nodeCount);
	if(sn->tStart > sg->nodes[sn->edgesOut[j]]->tStart && sg->nodes[sn->edgesOut[j]]->class != BIGNUM )
	    errAbort("exonWalk::nodeSanityCheck() -Node %u-%u connects to %u-%u but that is the wrong direction.\n",  
		     sn->id, sn->tStart, sg->nodes[sn->edgesOut[j]]->id, sg->nodes[sn->edgesOut[j]]->tStart);
	}
    }
}

boolean sameExonDifferentEnds(struct exonNode *en1, struct exonNode *en2)
{
if(en1->tStart == en2->tStart && en2->tEnd != en2->tEnd)
    return TRUE;
else
    return FALSE;
}

boolean sameExonDifferentStarts(struct exonNode *en1, struct exonNode *en2)
{
if(en1->tStart == en2->tStart && en2->tEnd != en2->tEnd)
    return TRUE;
else
    return FALSE;
}

boolean isExonEdge(struct altGraphX *agx, int edge)
{
if((agx->vTypes[agx->edgeStarts[edge]] == ggHardStart ||
    agx->vTypes[agx->edgeStarts[edge]] == ggSoftStart) &&
   (agx->vTypes[agx->edgeEnds[edge]] == ggHardEnd ||
    agx->vTypes[agx->edgeEnds[edge]] == ggSoftEnd))
    return TRUE;
return FALSE;
}

int addNodeToPath(struct exonGraph *eg, struct exonPath *ep, struct altGraphX *agx, int edge)
/** Add the necessary node to the exonPath to include the indicated edge
   from the altGraphX. */
{
struct exonNode *en;
struct exonNode *tail = slLastEl(ep->nodes);
unsigned int end = ep->tEnd;
unsigned int start = ep->tStart;
/* Create the second node as we'll need that no matter what. */
if(!isExonEdge(agx,edge))
    return;
AllocVar(en);
en->tName = cloneString(agx->tName);
snprintf(en->strand,sizeof(en->strand),"%s", agx->strand);
en->tStart = agx->vPositions[agx->edgeStarts[edge]];
addExonNodeStart(en, en->tStart);
en->tEnd = agx->vPositions[agx->edgeEnds[edge]];
addExonNodeEnd(en, en->tEnd);
en->type = ggExon;
en->class = edge;
en->startClass = agx->edgeStarts[edge];
en->endClass = agx->edgeEnds[edge];
en->startType = agx->vTypes[agx->edgeStarts[edge]];
en->endType = agx->vTypes[agx->edgeEnds[edge]];
en->ep = ep;
start = en->tStart;
/* Check to make sure this makes sense. */
if(en->tEnd <= en->tStart)
    {
    warn("\nexonWalk::addNodeToPath() - exon start: %d > exon end:%d for accession %s", en->tStart, en->tEnd, ep->qName);
    exonNodeFree(&en);
    return exonProblem;
    }
if(sameExonDifferentEnds(tail, en)) /* If this is a duplicate type, change tail to range. */
    {
    tail->type = ggRangeExon;
    if(en->tEnd < tail->tEnd)
	warn("exonWalk::addNodeToPath() - How can node at %s:%u-%u be greater than node at %s:%u-%u?", 
	     tail->tName, tail->tStart, tail->tEnd, en->tName, en->tStart, en->tEnd);
    tail->tEnd = en->tEnd;
    addExonNodeEnd(tail, en->tEnd);
    tail->class = BIGNUM;
    tail->endClass = BIGNUM;
    assert(tail->tEnd >= tail->tStart);
    end = en->tEnd;

    exonNodeFree(&en);
    }
else if(sameExonDifferentStarts(tail,en))
    {
    tail->type = ggRangeExon;
    if(en->tStart < tail->tStart)
	warn("exonWalk::addNodeToPath() - How can node at %s:%u-%u be greater than node at %s:%u-%u?", 
	     tail->tName, tail->tStart, tail->tStart, en->tName, en->tStart, en->tStart);
    addExonNodeStart(tail, en->tStart);
    tail->class = BIGNUM;
    tail->startClass = BIGNUM;
    assert(tail->tEnd >= tail->tStart);
    exonNodeFree(&en);
    }
else /* Normal addition of a node. */
    {
    exonGraphAddNode(eg, en);
    if(tail != NULL)
	exonNodeConnect(tail, en);
    slAddTail(&ep->nodes, en);
    ep->nodeCount += 1;
    end = en->tEnd;
    }
/* Expand the limits of the path as necessary. */
if(end > ep->tEnd)
    ep->tEnd = end;
if(start < ep->tStart)
    ep->tStart = start;
if(debug) { nodeSanityCheck(eg); }
}

boolean isExon(struct exonNode *sn1, struct exonNode *sn2)
/** Return TRUE if edge between sn1 and sn2 is an exon, FALSE otherwise. */
{
if((sn1->type == ggHardStart || sn1->type == ggSoftStart || sn1->type == ggRangeStart)
   && (sn2->type == ggHardEnd || sn2->type == ggSoftEnd || sn2->type == ggRangeEnd))
    return TRUE;
return FALSE;
}

int exonPathCountExons(struct exonPath *ep)
/** Count the number of exons in a exonPath. */
{
return slCount(ep->nodes);
}

void exonPathBedOut(struct exonPath *ep, FILE *out)
/** Print out a exon path in the form of a bed file, for viewing in the browser. */    
{
int i=0;
struct exonNode *en = NULL;
int exonCount = 0;
fprintf(out, "%s\t%u\t%u\t%s\t1000\t%s\t%u\t%u\t0\t", 
	ep->tName, ep->tStart, ep->tEnd, ep->qName, ep->strand, ep->tStart, ep->tEnd);
/* Only print out blocks that correspond to exons. */
exonCount = exonPathCountExons(ep);
fprintf(out, "%d\t", exonCount );
for(en=ep->nodes; en != NULL; en = en->next)
    {
    int size = en->tEnd - en->tStart;
    fprintf(out, "%d,",size);
    }
fprintf(out, "\t");
for(en=ep->nodes; en != NULL; en = en->next)
    {
    int start = en->tStart - ep->tStart;
    fprintf(out, "%d,",start);
    }
fprintf(out, "\n");
}

void initExonGraphFromAgx(struct exonGraph *eg, struct altGraphX *agx)
/** Fill in the simple stuff from the agx to the exonGraph. */
{
eg->tName = cloneString(agx->tName);
eg->tStart = agx->tStart;
eg->tEnd = agx->tEnd;
snprintf(eg->strand, sizeof(eg->strand), "%s", agx->strand);
eg->classCount = agx->edgeCount;
eg->classStarts = CloneArray(agx->edgeStarts,  eg->classCount);
eg->classEnds  = CloneArray(agx->edgeEnds,  eg->classCount);
eg->classTypes = CloneArray(agx->edgeTypes, eg->classCount);
}

struct exonGraph *exonGraphFromAgx(struct altGraphX *agx)
/** Construct a list of exon paths, one for every mrnaRef in the altGraphX, and return it. */
{
int i,j,k;
struct exonPath *ep = NULL, *epList = NULL;
struct exonGraph *eg = NULL;
struct evidence *ev = NULL;
char *acc = NULL;
AllocVar(eg);
initExonGraphFromAgx(eg, agx);
for(i=0; i < agx->mrnaRefCount; i++)
    {
    boolean addPath = TRUE;
    int numGraphNodes = eg->nodeCount;
    AllocVar(ep);
    ep->tStart = BIGNUM;
    acc = agx->mrnaRefs[i];
    ep->qName = cloneString(acc);
    snprintf(ep->strand, sizeof(ep->strand), "%s", agx->strand);
    ep->tName = cloneString(agx->tName);
    for(ev=agx->evidence; ev != NULL; ev = ev->next)
	{
        for(k=0; k<ev->evCount; k++)
	    {
	    if(sameString(acc, agx->mrnaRefs[ev->mrnaIds[k]]))
		{
		int sig = addNodeToPath(eg, ep, agx, slIxFromElement(agx->evidence,ev));
		if(sig == exonProblem)
		    addPath = FALSE;
		}
	    }
	}
//    warn("%d nodes in %s.", slCount(ep->nodes), ep->qName);
    if(debug) { nodeSanityCheck(eg); }
    if(addPath)
	slSafeAddHead(&epList, ep);
    else
	{
	exonPathFree(&ep);
	eg->nodeCount= numGraphNodes;
	}
    } 
slReverse(&epList);
eg->pathCount = slCount(epList);
eg->paths =  epList;
if(debug) { nodeSanityCheck(eg); }
return eg;
}

void exonPathCreateStringRep(struct exonPath *ep)
/** Loop through the nodes in this path and create a string of exons. */
{
struct dyString *dy = newDyString(256);
struct exonNode *en = NULL;
if(ep->path != NULL)
    freez(&ep->path);
for(en = ep->nodes; en != NULL; en = en->next)
    {
    dyStringPrintf(dy, "%d-%d", en->startClass, en->endClass);
    }
ep->path = cloneString(dy->string);
freeDyString(&dy);
}

char *getRefForNode(struct exonGraph *eg, int id)
/** Return the mrna accession that generated the node id by
   looking it up in the exonGraph. */
{
struct exonPath *ep = NULL;
struct exonNode *en = NULL;
for(ep = eg->paths; ep != NULL; ep = ep->next)
    {
    for(en = ep->nodes; en != NULL; en = en->next)
	if(en->id == id)
	    return ep->qName;
    }
}

boolean compatibleTypes(enum ggVertexType t1, enum ggVertexType t2)
/** Return TRUE if both starts, or both ends. FALSE otherwise. */
{
if(t1 == ggSoftEnd || t1 == ggHardEnd || t1 == ggRangeEnd)
    {
    if(t2 == ggSoftEnd || t2 == ggHardEnd || t2 == ggRangeEnd)
	return TRUE;
    }
if(t1 == ggSoftStart || t1 == ggHardStart || t1 == ggRangeStart)
    {
    if(t2 == ggSoftStart || t2 == ggHardStart || t2 == ggRangeStart)
	return TRUE;
    }
return FALSE;
}

boolean overlappingExons(struct exonNode *en1, struct exonNode *en2)
/* Return TRUE if exons overlap, FALSE otherwise. */
{
int overlap = 0;
overlap = rangeIntersection(en1->tStart, en1->tEnd, en2->tStart, en2->tEnd);
return overlap > 0;
}

boolean compatibleExons(struct exonNode *en1, struct exonNode *en2)
/** Return TRUE if these two nodes are compatible (overlapping) with eachother
   FALSE otherwise. */
{
boolean startMatch = FALSE;
boolean endMatch = FALSE;
int i,j;
for(i=0; i<en1->startCount; i++)
    for(j=0; j<en2->startCount; j++)
	if(en1->starts[i] == en2->starts[j])
	    {
	    startMatch = TRUE;
	    break;
	    }
for(i=0; i<en1->endCount; i++)
    for(j=0; j<en2->endCount; j++)
	if(en1->ends[i] == en2->ends[j])
	    {
	    endMatch = TRUE;
	    break;
	    }
return (startMatch && endMatch);
}

boolean compatibleNodes(struct exonNode *en1, struct exonNode *en2)
/** Return TRUE if these two nodes are compatible (overlapping) with eachother
   FALSE otherwise. */
{
assert(sameString(en1->strand, en2->strand));
if(en1->class == BIGNUM || en2->class == BIGNUM)
    {
    if(compatibleExons(en1, en2))
	return TRUE;
    }
else if(en1->class == en2->class)
    return TRUE;
return FALSE;
}

boolean extendsToEnd(struct exonNode *en1, struct exonNode *en2, boolean allEn2)
/** See if the list of nodes starting at en1 and en2 are of identical
   classes until the end. If allEn2 then all of en2 must be present
   in en1 to return TRUE.*/
{
boolean extends = TRUE;
for(; en1 != NULL && en2 != NULL; en1=en1->next, en2=en2->next)
    {
    if(!compatibleNodes(en1, en2))
	extends = FALSE;
    }
if(extends && allEn2)
    extends = (en2==NULL);
return extends;
}

void connectNodeList(struct exonNode *en1, struct exonNode *en2)
/** Connect each pair of nodes in the list togther. */
{
for(; en1 != NULL && en2 != NULL; en1=en1->next, en2=en2->next)
    {
    if(compatibleNodes(en1, en2))
	exonNodeConnect(en1,en2);
    }
}

void exonPathConnectEquivalentNodes(struct exonPath *hayStack, struct exonPath *needle)
/** Try to align one path to another and connect nodes that
   overlap in alignment. */
{
struct exonNode *en = NULL;
struct exonNode *mark = NULL;
boolean keepGoing = TRUE;
for(en = hayStack->nodes; en != NULL && keepGoing; en = en->next)
    {
    if(needle->nodes->class == en->class)
	{
	if(extendsToEnd(en, needle->nodes, FALSE))
	    {
	    connectNodeList(en, needle->nodes);
	    }
	keepGoing = FALSE;
	}
    }
}

boolean isSubPath(struct exonPath *hayStack, struct exonPath *needle)
/** Return TRUE if the needle is a subpath of the haystack, FALSE otherwise. */
{
struct exonNode *en = NULL;
boolean subPath = FALSE;
boolean keepGoing = TRUE;
for(en = hayStack->nodes; en != NULL && keepGoing; en = en->next)
    {
    if(compatibleNodes(needle->nodes, en))
	{
	if(extendsToEnd(en, needle->nodes, TRUE))
	    subPath = TRUE;
	keepGoing = FALSE;
	}
    }
return subPath;
}

void exonGraphConnectEquivalentNodes(struct exonGraph *eg)
/** Go through and find the equivalent nodes for each path by
   aligning one path against another. */
{
struct exonPath *ep = NULL;
struct exonPath *path1 = NULL, *path2 = NULL;
char *mark = NULL;
/** Create string representations of our exon paths for comparison. */
for(ep = eg->paths; ep != NULL; ep = ep->next)
    exonPathCreateStringRep(ep);
for(path1 = eg->paths; path1 != NULL; path1 = path1->next)
    {
    for(path2 = path1->next; path2 != NULL; path2 = path2->next)
	{
	if(path1 != path2)
	    exonPathConnectEquivalentNodes(path1, path2);
	if(debug) { nodeSanityCheck(eg); }
	}
    }
}

struct exonNode *exonNodeCopy(struct exonNode *en)
/* Copy an exonNode, including it's dynamic memory. */
{
struct exonNode *enCopy = NULL;
assert(en);
enCopy = CloneVar(en);
enCopy->tName = cloneString(en->tName);
if(en->edgeOutCount != 0)
    enCopy->edgesOut = CloneArray(en->edgesOut, en->edgeOutCount);
if(en->edgeInCount != 0)
    enCopy->edgesIn = CloneArray(en->edgesIn, en->edgeInCount);
enCopy->starts = CloneArray(en->starts, en->startCount);
enCopy->ends = CloneArray(en->ends, en->endCount);

return enCopy;
}

struct exonPath *exonPathCopy(struct exonGraph *eg, struct exonPath *orig)
/** Allocate and fill in a new exonPath that is a copy of the original. */
{
struct exonPath *ep = NULL;
struct exonNode *en = NULL, *enCopy = NULL;
struct exonNode *enNext = NULL;
assert(orig);
assert(orig->nodes);

ep = CloneVar(orig);
ep->next = NULL;
ep->tName = cloneString(orig->nodes->tName);
snprintf(ep->strand, sizeof(ep->strand), "%s", orig->nodes->strand);
ep->qName = NULL;
ep->tStart = BIGNUM;
ep->tEnd = 0;
ep->path = cloneString(orig->path);
ep->nodes = NULL;
for(en=orig->nodes; en != NULL; en = enNext)
    {
    if(en->edgeOutCount != 0)
	enNext = eg->nodes[en->edgesOut[0]];
    else
	enNext = NULL;
    enCopy = exonNodeCopy(en);
    if(enCopy->tEnd > ep->tEnd)
	ep->tEnd = enCopy->tEnd;
    if(enCopy->tStart < ep->tStart)
	ep->tStart = enCopy->tStart;
    slAddHead(&ep->nodes, enCopy);
    }
slReverse(&ep->nodes);
return ep;
}


void checkForMaximalPath(struct exonGraph *eg, struct exonPath **maximalPaths, struct exonPath *epList)
/** Look to see if the history path is a subpath of any of the 
   maximalPaths or vice versa. */
{
struct exonPath *ep = NULL, *epNext = NULL;
boolean maximal = TRUE;
struct exonPath *history = NULL;
for(history = epList; history != NULL; history = history->next)
    {
    maximal = TRUE;
    slReverse(&history->nodes);
    for(ep = *maximalPaths; ep != NULL; ep = epNext)
	{
	epNext = ep->next;
	if(isSubPath(ep, history))
	    {
	    maximal = FALSE;
	    break;
	    }
	else if(isSubPath(history, ep))
	    {
	    slRemoveEl(maximalPaths, ep);
	    exonPathFree(&ep);
	    }
	}
    if(maximal)
	{
	struct exonPath *copy = exonPathCopy(eg,history);
	slAddHead(maximalPaths, copy);
	}
    slReverse(&history->nodes);
    }
}

boolean heapContainsNode(struct heap *h, struct exonNode *en)
/** Return TRUE if node with same id as en->id, FALSE otherwise. */
{
int i;
for(i=0; i<h->count; i++)
    {
    struct exonNode *e = h->array[i]->val;
    if(e->id == en->id)
	return TRUE;
    }
return FALSE;
}

void exonGraphBfs(struct exonGraph *eg, struct exonNode *en, int nodeIndex, 
		  struct exonPath **maximalPaths, struct heap *queue )
/** A modified breadth first search of the graph. Nodes are investigated not
only in the order of distance in nodes but also in order of distance from
transcription start. All paths to nodes are kept track of as search is made. */
{
assert(queue);
while(!heapEmpty(queue))
    {
    int i =0;
    struct exonNode *en = heapExtractMin(queue);
    for(i=0; i<en->edgeOutCount; i++)
	{
	/* Pass our path on to all of our connections... */
	struct exonPath *ep = NULL;
	struct exonPath *epCopy = NULL;
	struct exonNode *enCopy = NULL;
	for(ep = en->paths; ep != NULL; ep = ep->next)
	    {
	    epCopy = exonPathCopy(eg,ep);
	    /* If we're making a move to another exon then include this exon in the path... */
	    if(eg->nodes[i]->class != en->class && en->class != BIGNUM)
		{
		enCopy = exonNodeCopy(en);
		slAddHead(&ep->nodes, en);
		}
	    slAddHead(&eg->nodes[i]->paths, ep);
	    }
	/* If the haven't been seen yet, queue them up... */
	if(eg->nodes[i]->color == enWhite) 
	    {
	    eg->nodes[i]->color = enGray;
	    if(!heapContainsNode(queue, eg->nodes[i]))
	       heapMinInsert(queue, eg->nodes[i]);
	    }
	}
    if(en->endType == ggSoftEnd && en->class != BIGNUM) /* It is a terminal node...*/
	{
	checkForMaximalPath(eg, maximalPaths, en->paths);
	}
    exonPathFreeList(&en->paths);
    en->color = enBlack;
    }
}

void exonGraphPathDfs(struct exonGraph *eg, struct exonNode *en, int nodeIndex, 
			struct exonPath **maximalPaths, struct exonPath *history)
/** Depth first search of the exonGraph starting from node at nodeIndex.
   keeps track of path taken using history, and will check history against
   maximal paths at black nodes. */
{
int i;
struct exonNode *copy = exonNodeCopy(en);
boolean pop = FALSE;
struct exonNode *notCompat = NULL;
boolean notCompatPush = FALSE;
if(debug) { nodeSanityCheck(eg); }
copy->next = NULL;

/* Handle logic for pushing nodes onto history stack. */
if((history->nodes == NULL || history->nodes->class != en->class) && en->class != BIGNUM)
    {
    if(history->nodes != NULL && overlappingExons(history->nodes, en))
	{
	notCompatPush = TRUE;
	notCompat = slPopHead(&history->nodes);
	}
    slAddHead(&history->nodes, copy);
    history->nodeCount++;
    pop =TRUE;
    }
en->color = enGray;
//if(en->endType == ggSoftEnd && en->class != BIGNUM) /* It is a terminal node...*/
//    {
//    printf("%s\t%d\t%d\n", en->tName, en->tStart, en->tEnd);
    checkForMaximalPath(eg, maximalPaths, history);
//    }
for(i=0; i<en->edgeOutCount; i++)
    {
    struct exonNode *next = eg->nodes[en->edgesOut[i]];
    if(next->color == enWhite)
	{
	exonGraphPathDfs(eg, next, next->id, maximalPaths, history);
	}
    }
if(pop)
    {
    slPopHead(&history->nodes);
    history->nodeCount--;
    if(notCompatPush)
	slSafeAddHead(&history->nodes, notCompat);
    }
exonNodeFree(&copy);
en->color = enBlack;
}

int nodeStartCmp(const void *va, const void *vb)
/** Compart to sort based on tStart of node. */
{
const struct exonNode *a = *((struct exonNode **)va);
const struct exonNode *b = *((struct exonNode **)vb);
return a->tStart - b->tStart;
}

struct exonPath *exonGraphBreadthFirstMaxPaths(struct exonGraph *eg)
/** Construct maximal paths through the exonGraph. Maximal paths
   are those which are not subpaths of any other path. Based on breadth
   first search using implicit ordering of exons.
*/
{
int i =0;
int index = 0;
struct heap *queue = newHeap(8, nodeStartCmp);
struct exonPath *maximalPaths = NULL, *ep = NULL;
struct exonNode **orderedNodes = CloneArray(eg->nodes, eg->nodeCount);
qsort(orderedNodes, eg->nodeCount, sizeof(struct exonNode *), nodeStartCmp);
for(i=0; i<eg->nodeCount; i++)
    {
    if(orderedNodes[i]->edgeInCount == 0)
	heapMinInsert(queue, orderedNodes[i]);
    }
exonGraphBfs(eg, orderedNodes[0], 0, &maximalPaths, queue);
freez(&orderedNodes);
return maximalPaths;
}

struct exonPath *exonGraphMaximalPaths(struct exonGraph *eg)
/** Construct maximal paths through the exonGraph. Maximal paths
   are those which are not subpaths of any other path. */
{
int i=0;
int index = 0;
struct exonPath *maximalPaths = NULL, *ep = NULL;
struct exonNode **orderedNodes = CloneArray(eg->nodes, eg->nodeCount);
qsort(orderedNodes, eg->nodeCount, sizeof(struct exonNode *), nodeStartCmp);
for(i=0; i<eg->nodeCount; i++)
    {
    if(orderedNodes[i]->color == enWhite)
	{
	struct exonPath *history = NULL;
	AllocVar(history);
	exonGraphPathDfs(eg, orderedNodes[i], i, &maximalPaths, history);
	freez(&history);
	}
    }
freez(&orderedNodes);
return maximalPaths;
}

void writeOutNodes(struct altGraphX *agx, FILE *f)
/** Write out a bed record for each vertice. */
{
int i;
for(i=0; i < agx->vertexCount; i++)
    {
    fprintf(f, "%s\t%u\t%u\t%d\n", agx->tName, agx->vPositions[i], agx->vPositions[i], i);
    }
}

int uniqueSize(struct exonGraph *eg, struct exonPath *maximalPaths, struct exonPath *currentPath,
	       struct exonNode *en1)
/** Find out how much of the exon laid out by en1 and s2 overlaps with other
   exons in the maximal paths. */
{
int size = en1->tEnd - en1->tStart;
int maxOverlap = 0;
struct exonNode *en;
struct exonPath *ep;
if(size < 1)
    errAbort("exonWalk::uniqueSize() - size is less than 1bp: %d", size);
for(ep = maximalPaths; ep != NULL; ep = ep->next)
    {
    if(currentPath != ep)
	{
	for(en = ep->nodes; en != NULL; en = en->next)
	    {
	    if(en->class != en1->class)
		{
		int overlap = rangeIntersection(en->tStart, en->tEnd, en1->tStart, en1->tEnd);
		if(overlap > maxOverlap)
		    maxOverlap = overlap;
		if(size - maxOverlap < 0)
		    errAbort("exonWalk::uniqueSize() - size is %d and overlap is %d", size, overlap);
		}
	    }
	}
    }
size = size - maxOverlap;
assert(size >= 0);
return size;
}

boolean isMrnaNode(struct exonNode *mark, struct exonGraph *eg, struct hash *mrnaHash)
/** Return TRUE if any of the paths that this node connects
   to are from an mRNA. FALSE otherwise. */
{
int i; 
struct exonPath *ep = NULL;
struct exonNode *en = NULL;
if(hashLookup(mrnaHash, mark->ep->qName))
    return TRUE;
for(ep = eg->paths; ep != NULL; ep = ep->next)
    {
    if(hashLookup(mrnaHash, ep->qName))
	{
	for(en=ep->nodes; en != NULL; en = en->next)
	    {
	    if(en->class == mark->class)
		return TRUE;
	    }
	}
    }
/* for(i=0; i < mark->edgeCount; i++) */
/*     if(hashLookup(mrnaHash, sg->nodes[mark->edges[i]]->ep->qName)) */
/* 	return TRUE; */
return FALSE;
}


boolean hardStartsEnds(struct exonNode *en)
/* Return true if starting and ending types are hard. */
{
if(en->startType == ggSoftStart || en->startType == ggSoftEnd)
    return FALSE;
if(en->endType == ggSoftStart || en->endType == ggSoftEnd)
    return FALSE;
return TRUE;
}

int countGoodEdges(struct exonGraph *eg, struct exonNode *en)
/* Count how many edges off the node aren't to wildcard nodes. */
{
int i, count=0;
for(i=0; i<en->edgeOutCount; i++)
    {
    if(eg->nodes[en->edgesOut[i]]->class != BIGNUM)
	count++;
    }
return count;
}

boolean confidentPath(struct hash *mrnaHash, struct exonGraph *eg, 
		      struct exonPath *maximalPaths, struct exonPath *ep)
/** Return TRUE if all edges in ep were seen in 5% or more
   of the paths. */
{
double minPercent = cgiOptionalDouble("minPercent", .05);
int trumpSize = cgiOptionalInt("trumpSize", 450);
int mrnaVal = cgiOptionalInt("mrnaWeight", 20);
double minNum = 0;
struct exonNode *en = NULL;
int size = 0;
boolean result = TRUE;
if(slCount(eg->paths) > 10)
    {
    minNum = minPercent * slCount(eg->paths) + 1;
    if(minNum > 4)
	minNum =4;
    }
else
    minNum = 1;

for(en = ep->nodes; en != NULL; en = en->next)
    {
    int count = countGoodEdges(eg,en);
    size += uniqueSize(eg, maximalPaths, ep, en);
    if(en->next == NULL)
	count++;
    if(hardStartsEnds(en))
	count += 1;
    if(isMrnaNode(en, eg, mrnaHash))
	count += mrnaVal;
    if(count < minNum)
	result =  FALSE;
    }
if(size >= trumpSize)
    result = TRUE; 
return result;
}

void putTic()
{
	fprintf(stderr,".");
	fflush(stderr);
}

void hashRow0(struct sqlConnection *conn, char *query, struct hash *hash)
/** Return hash of row0 of return from query. */
{
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
int count = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(hash, row[0], NULL);
    ++count;
    }
sqlFreeResult(&sr);
uglyf("%d match %s\n", count, query);
}

void addMrnaAccsToHash(struct hash *hash)
/** Load up all the accessions from the database that are mRNAs. */
{
char query[256];
char *db = cgiUsualString("db","hg10");
struct sqlConnection *conn = NULL;
hSetDb(db);
conn = hAllocConn();
snprintf(query, sizeof(query), "select acc from mrna where type = 2");
hashRow0(conn, query, hash);
}

void exonWalk(char *inFile, char *bedOut)
/** Starting routine. */
{
struct altGraphX *agx = NULL, *agxList = NULL;
struct exonGraph *eg = NULL, *egList = NULL;
FILE *f = NULL;
FILE *exonSites = NULL;
FILE *cleanEsts = NULL;
FILE *rejects = NULL;
boolean diagnostics = cgiBoolean("diagnostics");
boolean mrnaFilter = !(cgiBoolean("mrnaFilterOff"));
struct hash *mrnaHash = newHash(10);
int index = 0;
int count = 0;
struct exonPath *ep=NULL, *epList = NULL;
agxList = altGraphXLoadAll(inFile);
f = mustOpen(bedOut, "w");
if(mrnaFilter)
    addMrnaAccsToHash(mrnaHash);

if(diagnostics) 
    {
    exonSites = mustOpen("exonSites.bed", "w");
    cleanEsts = mustOpen("cleanEsts.bed", "w");
    rejects = mustOpen("rejects.bed", "w");
    fprintf(rejects, "track name=rejects description=\"exonWalk rejects\" color=178,44,26\n");
    fprintf(exonSites, "track name=exonSites description=\"exonWalk exon sites.\"\n");
    fprintf(cleanEsts, "track name=cleanEsts description=\"exonWalk cleaned ESTs\"\n");
    }
fprintf(f, "track name=exonWalk description=\"exonWalk Final Picks\" color=23,117,15\n");
warn("Creating exon graphs.");
for(agx = agxList; agx != NULL; agx = agx->next)
    {
    if(count++ % 100 == 0)
	putTic();
    if(diagnostics)
	writeOutNodes(agx, exonSites);
    eg = exonGraphFromAgx(agx);
    currentGraph = eg;
    if(diagnostics)
	for(ep = eg->paths; ep != NULL; ep = ep->next)
	    exonPathBedOut(ep,cleanEsts);
    exonGraphConnectEquivalentNodes(eg);
    if(debug) { nodeSanityCheck(eg); }
    epList = exonGraphMaximalPaths(eg);
    for(ep = epList; ep != NULL; ep = ep->next)
	{
	char buff[256];
	snprintf(buff, sizeof(buff), "%d", index++);
	ep->qName = cloneString(buff);
	exonPathCreateStringRep(ep);

	if(confidentPath(mrnaHash, eg, epList, ep))
	    {
	    if(debug)
		{
		printf("%s\n", ep->path);
		exonPathBedOut(ep, stdout);
		}
	    exonPathBedOut(ep,f);
	    }
	else if(diagnostics)
	    exonPathBedOut(ep, rejects);
	}
    exonPathFree(&epList);
    exonGraphFree(&eg);
    currentGraph = NULL;
    }
warn("\tDone.");
if(diagnostics)
    {
    carefulClose(&exonSites);
    carefulClose(&cleanEsts);
    carefulClose(&rejects);
    }
carefulClose(&f);
exonGraphFreeList(&egList);
altGraphXFreeList(&agxList);
}


int main(int argc, char *argv[])
{
if(argc < 3)
    usage();
cgiSpoof(&argc, argv);
debug = cgiBoolean("debug");
exonWalk(argv[1], argv[2]);
return 0;
}
