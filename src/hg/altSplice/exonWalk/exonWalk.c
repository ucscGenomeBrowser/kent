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
int inActive = -10;    /* Symbol for when an exonNode should no longer be used. */
boolean debug = FALSE; /** Turns on some debugging output. */
boolean diagnostics = FALSE; /* Print out some extra tracks. */
FILE *hippos = NULL;   /* File for logging graphs that have too many edges or nodes. */
FILE *whites = NULL;   /* Log file for exons which were never seen. */

struct exonNodeWrap 
/* Wrapper around an exonNode to make a list. */
{
    struct exonNodeWrap *next;   /* Next in list. */
    struct exonNode *en;         /* Pointer to exon node. */
};

void exonNodeWrapFreeList(struct exonNodeWrap **pList)
/* Free a list of dynamically allocated exonNodeWrap's */
{
struct exonNodeWrap *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    freeMem(el);
    }
*pList = NULL;
}


void nodeSanityCheck(struct exonGraph *sg)
/** Loop through and make sure all the nodes have legit ids. */
{
int i,j;
for(i=0;i<sg->nodeCount; i++)
    {
    struct exonNode *sn = sg->nodes[i];
    for(j=0;j<sn->edgeOutCount && sn->class != inActive; j++)
	{
	struct exonNode *target = sg->nodes[sn->edgesOut[j]];
	if(target->class == inActive)
	    errAbort("exonWalk::nodeSanityCheck() - How can node %d still point to node %d which is inactive?", sn->id, target->id);
	if(sn->edgesOut[j] >= sg->nodeCount)
	    errAbort("exonWalk::nodeSanityCheck() - node with id %d when max is %d\n", sn->edgesOut[j], sg->nodeCount);
	if((rangeIntersection(sn->tStart, sn->tEnd, target->tStart, target->tEnd) > 0)	   && target->class != BIGNUM && target->class != sn->class )
	    errAbort("exonWalk::nodeSanityCheck() -Node %u-%u connects to %u-%u but that is the wrong direction.\n",  sn->id, sn->tStart, target->id, target->tStart);
	if(sn->id == target->id)
	    errAbort("exonWalk::nodeSanityCheck() -Node %u-%u connects to %u-%u but they are the same node.\n",  sn->id, sn->tStart, target->id, target->tStart);

	}
    }
}

void printIntArray(int count, int *array)
{
int i;
for(i=0;i<count; i++)
    {
    fprintf(stderr, "%d,", array[i]);
    }
fprintf(stderr, "\n");
fflush(stderr);
}

int nodeStartCmp(const void *va, const void *vb)
/** Compart to sort based on tStart of node. */
{
const struct exonNode *a = *((struct exonNode **)va);
const struct exonNode *b = *((struct exonNode **)vb);
int diff =  a->tStart - b->tStart;
if(diff == 0)
    diff = b->edgeInCount - a->edgeInCount;
return diff;
}

int exonGraphCountClasses(struct exonGraph *eg)
/** Count how many different types of classes we have
    in our graph. */
{
int max = 0;
int i = 0;
for(i=0; i<eg->nodeCount; i++)
    {
    if(eg->nodes[i]->class > max)
	max = eg->nodes[i]->class;
    }
return max +1;
}

boolean nodesConnected(struct exonNode *from, struct exonNode *to)
/* Return TRUE if from points to to, FALSE otherwise. */
{
int i;
for(i=0; i < from->edgeOutCount; i++)
    if(from->edgesOut[i] == to->id)
	return TRUE;
return FALSE;
}



boolean equivalentNodes(struct exonGraph *eg, struct exonNode *unique, struct exonNode *test)
/* Return TRUE if both needle and haystack  have the same classes of outgoing nodes and incoming nodes. 
 FALSE otherwise. */
{
int i,j;
boolean inMatch = TRUE;
boolean outMatch = TRUE;
boolean match = FALSE;

if(unique->endClass != test->endClass || unique->startClass != test->startClass )
  return FALSE;
if(!nodesConnected(unique, test) || !nodesConnected(test, unique))
    return FALSE;
for(i=0; i < test->edgeInCount; i++)
    {
    struct exonNode *nIn = eg->nodes[test->edgesIn[i]];
    boolean currentMatch = FALSE;
    for(j=0; j < unique->edgeInCount; j++)
	{
	struct exonNode *hIn = eg->nodes[unique->edgesIn[j]];
	if(hIn->class == nIn->class)
	    {
	    currentMatch = TRUE;
	    break;
	    }
	}
    inMatch = (inMatch && currentMatch);
    }

for(i=0; i < test->edgeOutCount; i++)
    {
    struct exonNode *nOut = eg->nodes[test->edgesOut[i]];
    boolean currentMatch = FALSE;
    for(j=0; j < unique->edgeOutCount; j++)
	{
	struct exonNode *hOut = eg->nodes[unique->edgesOut[j]];
	if(hOut->class == nOut->class)
	    {
	    currentMatch = TRUE;
	    break;
	    }
	}
    outMatch = (outMatch && currentMatch);
    }
match = (inMatch && outMatch);
return match;
}

boolean notUnique(struct exonGraph *eg, struct exonNodeWrap *exonClass, 
		  struct exonNode *mark, struct exonNode **unique)
/** Check to see if an equivalent exonNode exists in the exonClass list,
    if so return TRUE and assign unique to be the exon in the exonClass list. 
    Else return FALSE and set unique to NULL. */
{
struct exonNodeWrap *ec = NULL;
*unique = NULL;
for(ec = exonClass; ec != NULL; ec = ec->next)
    {
    if(equivalentNodes(eg, ec->en, mark))
	{
	*unique = ec->en;
	return TRUE;
	}
    }
return FALSE;
}

void addUnique(struct exonNodeWrap **exonClass, struct exonNode *en)
/* Create an exonNodeWapper for en and add the exonNodeWrapper to the
   exonClass list. */
{
struct exonNodeWrap *enw = NULL;
AllocVar(enw);
enw->en = en;
slAddHead(exonClass, enw);
}

int intCmp(const void *va, const void *vb)
/** Compart to sort based on tStart of node. */
{
return *((int *)va) - *((int *)vb);
}

int mergeDuplicates(int count, int *array, int id)
/* Remove any duplicate entries in array which has starting size count. 
   Return the unique number of entries in array. */
{
int i,j;
int finalCount = count;
for(i=0; i< finalCount; i++)
    {
    if(array[i] == id)
	{
	int k;
	finalCount--;
	for(k = i; k < count -1; k++)
	    {
	    array[k] = array[k+1];
	    }
	}
    else 
	{
	for(j=i+1; j<finalCount; j++)
	    {
	    if(array[i] == array[j] )
		{
		int k;
		finalCount--;
		for(k = j; k < count -1; k++)
		    {
		    array[k] = array[k+1];
		    }
		}
	    }
	}
    }
return finalCount;
}

int mergeDuplicatesFast(int count, int *array, int id)
/* Remove duplicates and entries that correspond to id. */
{
int finalCount = count;
int *head = NULL, *orig = array;
int *last = array+count;
int *start = NULL;
qsort(array, count, sizeof(int), intCmp);
/* Check to make sure that we don't have forbidden id at start. */
while((*(array)) == id && (array != last))
    {
    array++;
    finalCount--;
    }
/* Remove duplicates in rest of array. */
head = start = array;
while(++array != last && array != (last+1))
    {
    if( ((*(head)) != (*(array))) && ((*(array)) != id) )
	*(++head) = *array;
    else
	finalCount--;
    }
CopyArray(start, orig, finalCount);
return finalCount;
}

void spliceOutExonNode(struct exonGraph *eg, struct exonNode *mark, struct exonNode *unique)
/* Detach mark from its connections, replace with unique. */
{
struct exonNode *en;
int i,j;
/* if(debug) { nodeSanityCheck(eg); } */
/* assert(equivalentNodes(eg, unique, mark)); */
/* First modify the incoming nodes. */
for(i=0; i < mark->edgeInCount; i++)
    {
    en = eg->nodes[mark->edgesIn[i]];
    for(j=0; j < en->edgeOutCount; j++)
	{
	if(en->edgesOut[j] == mark->id)
	    {
	    en->edgesOut[j] = unique->id;
	    }
	}
    en->edgeOutCount = mergeDuplicatesFast(en->edgeOutCount, en->edgesOut, en->id);
    }
/* if(debug) { nodeSanityCheck(eg); } */
for(i=0; i < mark->edgeOutCount; i++)
    {
    en = eg->nodes[mark->edgesOut[i]];
    for(j=0; j < en->edgeInCount; j++)
	{
	if(en->edgesIn[j] == mark->id)
	    {
	    en->edgesIn[j] = unique->id;
	    }
	}
    en->edgeInCount = mergeDuplicatesFast(en->edgeInCount, en->edgesIn, en->id);
    }
/* if(debug) { nodeSanityCheck(eg); } */
mark->class = inActive;
}

void exonGraphTrim(struct exonGraph *eg)
/** Look through the nodes in the exonGraph and merge those that have
    connections to the same class output nodes and same class input nodes.*/
{
int numClasses = exonGraphCountClasses(eg);
int i = 0;
struct exonNodeWrap **classes = NULL;
struct exonNodeWrap **exonClass = NULL, *enw = NULL;
struct exonNode *left = NULL, *right = NULL, *mark=NULL, *unique = NULL;

struct exonNode **orderedNodes = CloneArray(eg->nodes, eg->nodeCount);
classes = needMem(sizeof(struct exonNodeWrap *) * numClasses);
qsort(orderedNodes, eg->nodeCount, sizeof(struct exonNode *), nodeStartCmp);
for(i=0; i < eg->nodeCount; i++)
    {
    mark = eg->nodes[i];
    exonClass = &classes[mark->class];
    if(notUnique(eg, *exonClass, mark, &unique))
	{
	spliceOutExonNode(eg, mark, unique);
	}
    else
	{
	addUnique(exonClass, mark);
	}
    }
/* Cleanup. */
for(i=0; i < numClasses; i++)
    {
    exonNodeWrapFreeList(&classes[i]);
    }
freez(&classes);
freez(&orderedNodes);
}

int countActiveNodes(struct exonGraph *eg)
/* Count how many nodes in eg don't have class == inActive. */
{
int i;
int count = 0;
for(i=0; i<eg->nodeCount; i++)
    {
    if(eg->nodes[i]->class != inActive)
	count++;
    }
return count;
}

int countActiveEdges(struct exonGraph *eg)
/* Count how many edges in eg from nodes don't have class == inActive. */
{
int i;
int count = 0;
for(i=0; i<eg->nodeCount; i++)
    {
    if(eg->nodes[i]->class != inActive)
	count += eg->nodes[i]->edgeOutCount;
    }
return count;
}

int exonNodeCmp(const void *va, const void *vb)
/* Compare based on tStart. */
{
const struct exonNode *a = va;
const struct exonNode *b = vb;	
return a->tStart - b->tStart;
}

struct exonGraph *currentGraph = NULL; /** Pointer to current graph, useful for debugging. */

boolean sameExonDifferentEnds(struct exonNode *en1, struct exonNode *en2)
{
if(en1 != NULL && en1->tStart == en2->tStart && en1->tEnd != en2->tEnd)
    return TRUE;
else
    return FALSE;
}

boolean sameExonDifferentStarts(struct exonNode *en1, struct exonNode *en2)
{
if(en1 != NULL && en1->tEnd == en2->tEnd && en1->tStart != en2->tStart)
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
if(sameExonDifferentEnds(tail, en)) /* If this is a duplicate type deal with fuzzy edge. */
    {
    tail->type = ggRangeExon;
    if(en->tEnd < tail->tEnd)
	warn("exonWalk::addNodeToPath() - How can node at %s:%u-%u be greater than node at %s:%u-%u?", 
	     tail->tName, tail->tStart, tail->tEnd, en->tName, en->tStart, en->tEnd);
    /* if fuzzy edge, stick with closest hardEnd or longest softEnd. */
    if((en->endType == ggHardEnd && tail->endType != ggHardEnd) || (tail->endType == ggSoftEnd))
	{
	tail->tEnd = en->tEnd;
	tail->ends[0] = en->tEnd;
	end = en->tEnd;
	tail->class = en->class;
	tail->endClass = en->endClass;
	}
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
    /* if fuzzy edge contract to smallest hardStart or stick with biggest softStart. */
    if(en->startType == ggHardStart)
	{
	tail->tStart = en->tStart;
	tail->starts[0] = en->tStart;
	tail->startClass = en->startClass;
	tail->class = en->class;
	}
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

boolean seenPathBefore(struct exonPath *epList, struct exonPath *query)
/** Return TRUE if there is a path just like this in the list already. */
{
struct exonPath *ep = NULL;
for(ep = epList; ep != NULL; ep = ep->next)
    if(sameString(ep->path, query->path))
	return TRUE;
return FALSE;
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
    exonPathCreateStringRep(ep);
    if(debug) { nodeSanityCheck(eg); }
    if(seenPathBefore(epList, ep))
	addPath = FALSE;
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
	{
	exonNodeConnect(en1,en2);
	exonNodeConnect(en2,en1);
	}
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
enCopy->tName = cloneString("chr22");
enCopy->edgesIn = NULL;
enCopy->edgesOut = NULL;
if(en->edgeOutCount != 0)
    enCopy->edgesOut = CloneArray(en->edgesOut, en->edgeOutCount);
if(en->edgeInCount != 0)
    enCopy->edgesIn = CloneArray(en->edgesIn, en->edgeInCount);
enCopy->starts = CloneArray(en->starts, en->startCount);
enCopy->ends = CloneArray(en->ends, en->endCount);
enCopy->paths = NULL;

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
    enNext = en->next;
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

boolean checkForCurrentNode(struct exonPath *ep, struct exonNode *en)
/* Check to see if current node is on the path of the exonPath. */
{
if(ep->nodes->class != en->class)
    {
    warn("ExonPath doesn't contain exonNode of class %d, last node is class %d", en->class, ep->nodes->class);
    return FALSE;
    }
return TRUE;
}

boolean doSanityCheck(struct exonGraph *eg)
{
struct exonPath *ep = NULL;
struct exonNode *en = NULL;
int i;
for(i=0; i< eg->nodeCount; i++) 
    {
    en = eg->nodes[i];
    for(ep = en->paths; ep != NULL; ep = ep->next)
	if(ep->nodes->class != en->class)
	    {
	    warn("exonWalk.c::doSanityCheck() - Can't have a path to a node that doesn't include that node. Problem at %s:%d-%d", en->tName, en->tStart, en->tEnd);
	    return FALSE;
	    }
    }
return TRUE;
}

void exonGraphBfs(struct exonGraph *eg, struct exonPath **maximalPaths, struct heap *queue )
/** A modified breadth first search of the graph. Nodes are investigated not
only in the order of distance in nodes but also in order of distance from
transcription start. All paths to nodes are kept track of as search is made. */
{
int globalCount = 0;
assert(queue);
while(!heapEmpty(queue))
    {
    int i =0;
    struct exonNode *en = heapExtractMin(queue);
    struct exonPath *ep = NULL;
    struct exonPath *epCopy = NULL;
    struct exonNode *enCopy = NULL;
    for(i=0; i<en->edgeOutCount; i++)
	{
	struct exonNode *target = eg->nodes[en->edgesOut[i]];
	if(target->class == inActive)
	    warn("exonGraphBfs() - %s:%d-%d, target %d is inactive!, en = %d", eg->tName, eg->tStart, eg->tEnd, target->id, en->id);
	for(ep = en->paths; ep != NULL; ep = ep->next)
	    {
	    if(debug) { nodeSanityCheck(eg); }
	    epCopy = exonPathCopy(eg, ep);
	    /* If we're making a move to another exon then include that exon in the path... */
	    if(target->class != en->class && en->class != BIGNUM)
		{
		enCopy = exonNodeCopy(target);
		slAddHead(&epCopy->nodes, enCopy);
		epCopy->nodeCount++;
		}
	    slAddHead(&target->paths, epCopy);

	    /* Some debuggint stuff. */
	    if(debug) 
		{ 
		nodeSanityCheck(eg); 
		if( !checkForCurrentNode(target->paths, target))
		    warn("Why didn't this work at %s:%d-%d", target->tName, target->tStart, target->tEnd);
		for(epCopy = target->paths; epCopy->next != NULL; epCopy = epCopy->next)
		    {
		    if(epCopy->nodes == epCopy->next->nodes)
			warn("How can I have two paths with the same nodes?");
		    }
		}
	    }
	
	/* If the haven't been seen yet, queue them up... */
	if(target->color == enWhite) 
	    {
	    target->color = enGray;
	    if(!heapContainsNode(queue, target))
	       heapMinInsert(queue, target);
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

int countNodeForAcc(struct exonGraph *eg, char *acc, boolean onlyActive)
{
int count=0;
int i;
struct exonNode *en = NULL;
for(i=0; i<eg->nodeCount; i++)
    {
    en = eg->nodes[i];
    if(sameString(en->ep->qName, acc))
	{
	if(onlyActive)
	    {
	    if(en->class != inActive)
		{
		count++;
		warn("Node %d:", i);
		}
	    }
	else 
	    {
	    count++;
	    }
	}
    }
return count;
}

void exonGraphPathDfs(struct exonGraph *eg, struct exonNode *en, int nodeIndex, 
			struct exonPath **maximalPaths, struct exonPath *history)
/** 
   ********* Deprecated ***********
   Depth first search of the exonGraph starting from node at nodeIndex.
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
if(en->endType == ggSoftEnd && en->class != BIGNUM) /* It is a terminal node...*/
    {
//    printf("%s\t%d\t%d\n", en->tName, en->tStart, en->tEnd);
    checkForMaximalPath(eg, maximalPaths, history);
    }
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


int nodeStartHeapCmp(const void *va, const void *vb)
/** Compart to sort based on tStart of node. */
{
const struct exonNode *a = va;
const struct exonNode *b = vb;
return a->tStart - b->tStart;
}

struct exonPath *newExonPath(struct exonNode *en)
/* Construct a path with only one node. */
{
struct exonPath *ep = NULL;
struct exonNode *enCopy = exonNodeCopy(en);
AllocVar(ep);
ep->tName = cloneString(en->tName);
snprintf(ep->strand, sizeof(ep->strand), "%s", en->strand);
ep->qName = NULL;
ep->tStart = BIGNUM;
ep->tEnd = 0;
ep->path = NULL;

slAddHead(&ep->nodes, enCopy);
return ep;
}

void logTooComplicated(struct exonGraph *eg)
/* Write a little ditty to the hippo log file. */
{
int maxNodes = cgiUsualInt("maxNodes", 100);
int maxEdges = cgiUsualInt("maxEdges", 100);
int activeNodes, activeEdges;
activeNodes = countActiveNodes(eg);
activeEdges = countActiveEdges(eg);
warn("%s:%u-%u\tmaxNodes = %d\tmaxEdges = %d\tactiveNodes = %d\tactiveEdges = %d\n", 
	eg->tName, eg->tStart, eg->tEnd, maxNodes, maxEdges, activeNodes, activeEdges);
fprintf(hippos, "%s:%u-%u\tmaxNodes = %d\tmaxEdges = %d\tactiveNodes = %d\tactiveEdges = %d\n", 
	eg->tName, eg->tStart, eg->tEnd, maxNodes, maxEdges, activeNodes, activeEdges);
}

boolean noIncomingEdges(struct exonGraph *eg, struct exonNode *en)
/* Return TRUE if there is no node that points to this one that
   isn't of the same class. FALSE otherwise. */
{
struct exonNode *target = NULL;
int i;
for(i=0; i < en->edgeInCount; i++)
    {
    target = eg->nodes[en->edgesIn[i]];
    if(target->class != en->class)
	return FALSE;
    }
return TRUE;
}

struct exonPath *exonGraphBreadthFirstMaxPaths(struct exonGraph *eg)
/** Construct maximal paths through the exonGraph. Maximal paths
   are those which are not subpaths of any other path. Based on breadth
   first search using implicit ordering of exons.
*/
{
int i =0;
int index = 0;
struct heap *queue = newHeap(8, nodeStartHeapCmp);
struct exonPath *maximalPaths = NULL, *ep = NULL;
struct exonNode **orderedNodes = CloneArray(eg->nodes, eg->nodeCount);
boolean trimGraph = cgiBoolean("trim");
int maxNodes = cgiUsualInt("maxNodes", 100);
int maxEdges = cgiUsualInt("maxEdges", 100);
int activeNodes, activeEdges;
int grayCount=0,whiteCount=0,blackCount=0;

activeNodes = countActiveNodes(eg);
activeEdges = countActiveEdges(eg);
qsort(orderedNodes, eg->nodeCount, sizeof(struct exonNode *), nodeStartCmp);
if(debug) { nodeSanityCheck(eg); }
warn("Before Trim: %s:%u-%u\tactiveNodes = %d\tactiveEdges = %d", 
	eg->tName, eg->tStart, eg->tEnd, activeNodes, activeEdges);
fflush(stderr);
if(debug) { nodeSanityCheck(eg); }
if(trimGraph)
    {
    exonGraphTrim(eg);
    activeNodes = countActiveNodes(eg);
    activeEdges = countActiveEdges(eg);
    warn("After Trim: %s:%u-%u\tactiveNodes = %d\tactiveEdges = %d", 
	eg->tName, eg->tStart, eg->tEnd, activeNodes, activeEdges);
    fflush(stderr);
    }
fflush(stderr);
if(debug) { nodeSanityCheck(eg); }
/* If we have too complicated of a situation quit. */
if(activeNodes > maxNodes || activeEdges > maxEdges)
    {
    logTooComplicated(eg);
    return NULL;
    }
for(i=0; i<eg->nodeCount; i++)
    {
    if( noIncomingEdges(eg, orderedNodes[i]) && orderedNodes[i]->class != inActive)
	{
	struct exonPath *ep = newExonPath(orderedNodes[i]);
	ep->nodeCount++;
	slAddHead(&orderedNodes[i]->paths, ep);
	heapMinInsert(queue, orderedNodes[i]);
//	warn("%s\t%d\t%d", orderedNodes[i]->tName, orderedNodes[i]->tStart, orderedNodes[i]->tEnd);
	}
    }
if(debug) { nodeSanityCheck(eg); }
exonGraphBfs(eg, &maximalPaths, queue);
for(i=0; i<eg->nodeCount; i++)
    {
    struct exonNode *target = eg->nodes[i];
    if(target->class != inActive)
	{
	exonPathFreeList(&target->paths);
	if(diagnostics)
	    {
	    if(target->color == enWhite)
		{
//		warn("White: Node %s:%d-%d %d is %d should be black: %d", target->tName, target->tStart, target->tEnd, target->id, target->color, enBlack);
		whiteCount++;
		fprintf(whites, "%s\t%d\t%d\n", target->tName, target->tStart, target->tEnd);
		}
	    else if(target->color == enGray)
		{
//		warn("Gray: Node %s:%d-%d %d is %d should be black: %d", target->tName, target->tStart, target->tEnd, target->id, target->color, enBlack);
		grayCount++;
		}
	    else if(target->color == enBlack)
		{
//		warn("Black: Node %s:%d-%d %d is %d should be black: %d", target->tName, target->tStart, target->tEnd, target->id, target->color, enBlack);
		blackCount++;
		}
	    else
		{
		errAbort("Don't know what color %d is for %d", target->color, target->id);
		}
	    }
	}

    }
if(diagnostics)
    warn("White: %d, Gray: %d, Black: %d", whiteCount, grayCount, blackCount);

heapFree(&queue);
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

boolean mrnaFilter = !(cgiBoolean("mrnaFilterOff"));
struct hash *mrnaHash = newHash(10);
int index = 0;
int count = 0;
struct exonPath *ep=NULL, *epList = NULL;
agxList = altGraphXLoadAll(inFile);
f = mustOpen(bedOut, "w");
hippos = mustOpen("tooBig.log", "w");
if(mrnaFilter)
    addMrnaAccsToHash(mrnaHash);

/* If we're doing diagnostics we need to start a bunch of bed files. */
if(diagnostics) 
    {
    whites = mustOpen("whites.bed", "w");
    exonSites = mustOpen("exonSites.bed", "w");
    cleanEsts = mustOpen("cleanEsts.bed", "w");
    rejects = mustOpen("rejects.bed", "w");
    fprintf(rejects, "track name=rejects description=\"exonWalk rejects\" color=178,44,26\n");
    fprintf(exonSites, "track name=exonSites description=\"exonWalk exon sites.\"\n");
    fprintf(cleanEsts, "track name=cleanEsts description=\"exonWalk cleaned ESTs\"\n");
    fprintf(whites, "track name=rejects description=\"exonWalk white nodes\" color=178,44,26\n"); 
    }
fprintf(f, "track name=exonWalk description=\"exonWalk Final Picks\" color=23,117,15\n");
warn("Creating exon graphs.");

for(agx = agxList; agx != NULL; agx = agx->next)
    {
/*     if(count++ % 100 == 0) */
/* 	putTic(); */
    if(diagnostics)
	writeOutNodes(agx, exonSites);
    eg = exonGraphFromAgx(agx);
    if(eg->nodeCount > 10000)
	{
	logTooComplicated(eg);
	exonGraphFree(&eg);
	continue;
	}
    currentGraph = eg;
    if(diagnostics)
	{
	for(ep = eg->paths; ep != NULL; ep = ep->next)
	    exonPathBedOut(ep,cleanEsts);
	fflush(cleanEsts);
	}
    exonGraphConnectEquivalentNodes(eg);
    if(debug) { nodeSanityCheck(eg); }
    epList = exonGraphBreadthFirstMaxPaths(eg);
//    epList = exonGraphMaximalPaths(eg);
    for(ep = epList; ep != NULL; ep = ep->next)
	{
	char buff[256];
	snprintf(buff, sizeof(buff), "%d", index++);
	ep->qName = cloneString(buff);
	exonPathCreateStringRep(ep);
	if(TRUE) //confidentPath(mrnaHash, eg, epList, ep))
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
    exonPathFreeList(&epList);
    exonGraphFree(&eg);
    currentGraph = NULL;
    }
warn("\tDone.");
/* Do some cleanup. */
if(diagnostics)
    {
    carefulClose(&exonSites);
    carefulClose(&cleanEsts);
    carefulClose(&rejects);
    carefulClose(&whites);
    }
hashFree(&mrnaHash);
carefulClose(&f);
carefulClose(&hippos);
exonGraphFreeList(&egList);
altGraphXFreeList(&agxList);
}


int main(int argc, char *argv[])
{
if(argc < 3)
    usage();
cgiSpoof(&argc, argv);
diagnostics = cgiBoolean("diagnostics");
debug = cgiBoolean("debug");
exonWalk(argv[1], argv[2]);
return 0;
}
