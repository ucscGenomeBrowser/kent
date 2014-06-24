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
#include "memalloc.h"
#include "errAbort.h"
#include "malloc.h"
#include "options.h"
#include "portable.h"
#include "verbose.h"
#include "obscure.h"


static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"maxWalkTime", OPTION_INT},
    {"keepFrags", OPTION_BOOLEAN},
    {"mrnaFilter", OPTION_BOOLEAN},
    {"db", OPTION_STRING},
    {"mrnaWeight", OPTION_INT},
    {"minPercent", OPTION_FLOAT},
    {"trumpSize", OPTION_INT},
    {"noPathMerging", OPTION_BOOLEAN},
    {"diagnostics", OPTION_BOOLEAN},
    {"maxMem", OPTION_INT},
    {"debug", OPTION_BOOLEAN},
    {"noTrim", OPTION_BOOLEAN},
    {"singleExonOk", OPTION_BOOLEAN},
    {"verbose", OPTION_INT},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "Time in seconds to spend on an exon walk before giving up. (420 sec default)",
    "Keep all fragments regardless of number of exons.",
    "Weight mRNAs more than ESTs. Requires -db to be set.",
    "Database to look up accessions (i.e. hg17, mm5).",
    "How much to weight mRNAs default 20",
    "Minimum percentage of transcripts that exon occurs in. (0.0 default)",
    "Keep exons that are >= trumpSize (100000 default)",
    "Don't merge paths that are the same. (debugging)",
    "Write out files that contain:\n\t\t cleaned transcripts, splice sites, and rejected paths.",
    "Size in bytes to use before giving up on an exonWalk. Default 256000000",
    "Print debugging info.",
    "Don't merge vertices in same connected component.",
    "Allow single exon transcripts to slip through.",
    "How verbose to be about things (default 0)"
};

void usage()
/* Introduce ourselves. */
{
int i = 0;
warn("exonWalk - Genebuilding program to produce full length transcripts\n"
     "from cDNAs.  Uses altGraphX files to reconstruct transcripts and\n"
     "construct a exon graph which is a combination of all the paths seen by\n"
     "RNA and EST evidence. The paths are joined where there are overlaps\n"
     "and the resulting super graph is walked to determine a set of\n"
     "transcripts which are not sub-paths of each other. Can use some\n"
     "heuristics to trim paths that contain bad nodes, but it is better to\n"
     "filter splice graphs prior to exonWalk.\n"
     "usage:\n"
	 "   exonWalk altGraphXFile.agx transcriptsOut.bed\n"
     "options:");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "   -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("");
}

int maxMem = 0; /* Maximum memory that a breadth first search can use. Not maximum for program. */
int exonProblem = -1;   /* Code for a problematic exon. Will cause error handling. */
int inActive = -10;    /* Symbol for  when an exonNode should no longer be used. */
long maxWalkTime = 60 * 10; /* Amout of time to walk a graph before giving up in seconds. */
boolean debug = FALSE; /** Turns on some debugging output. */
boolean diagnostics = FALSE; /* Print out some extra tracks. */
boolean noPathMerging = FALSE; /* Overrides usual behavior of merging two paths that are identical together. */
boolean singleExonOk = FALSE;	/* Allows export of single exons. */
FILE *hippos = NULL;   /* File for logging graphs that have too many edges or nodes. */
FILE *whites = NULL;   /* Log file for exons which were never seen. */
static jmp_buf exonWalkRecover; /* Used for error recoverys. */

struct exonNodeWrap 
/* Wrapper around an exonNode to make a list. */
{
    struct exonNodeWrap *next;   /* Next in list. */
    struct exonNode *en;         /* Pointer to exon node. */
};

void smallPathFree(struct smallPath **pEl)
/* Free a single smallPath. */
{
struct smallPath *el;
if((el = *pEl) == NULL) return;
freeMem(el->path);
freez(pEl);
}

void smallPathFreeList(struct smallPath **pList)
/* Free a list of dynamically allocated smallPath's */
{
struct smallPath *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    smallPathFree(&el);
    }
*pList = NULL;
}

struct smallPath *smallPathCopy(struct smallPath *sp)
/* Create a copy of the smallPath variable. */
{
struct smallPath *new = CloneVar(sp);
new->path = CloneArray(sp->path, sp->maxCount);
return new;
}

void appendToSmallPath(struct smallPath *sp, int id)
/* Append the id to the current path. */
{
if(sp->currCount + 1 >= sp->maxCount)
    {
    ExpandArray(sp->path, sp->maxCount, sp->maxCount+10);
    sp->maxCount += 10;
    }
sp->path[sp->currCount++] = id;
}

int exonPathCmp(const void *va, const void *vb)
/* Compare to sort based on tStart, tEnd. */
{
const struct exonPath *a = *((struct exonPath **)va);
const struct exonPath *b = *((struct exonPath **)vb);
int dif;
dif = a->tStart - b->tStart;
if(dif == 0)
    dif = b->tEnd - b->tEnd;
return dif;
}

/* if we abort return with non-zero status */
static void exonWalkAbort()
{
longjmp(exonWalkRecover, -1);
}

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
	    errAbort("exonWalk::nodeSanityCheck() - How can node %d still point to node %d which is inactive?", 
		     sn->id, target->id);
	if(sn->edgesOut[j] >= sg->nodeCount)
	    errAbort("exonWalk::nodeSanityCheck() - node with id %d when max is %d\n", 
		     sn->edgesOut[j], sg->nodeCount);
	if((rangeIntersection(sn->tStart, sn->tEnd, target->tStart, target->tEnd) > 0) && 
	   target->class != BIGNUM && target->class != sn->class )
	    errAbort("exonWalk::nodeSanityCheck() -Node %u-%u connects to %u-%u but that is the wrong direction.\n",  
		     sn->id, sn->tStart, target->id, target->tStart);
	if(sn->id == target->id)
	    errAbort("exonWalk::nodeSanityCheck() -Node %u-%u connects to %u-%u but they are the same node.\n",  
		     sn->id, sn->tStart, target->id, target->tStart);

	}
    }
}

void addEvidenceToNode(struct exonNode *en, char *acc)
/* If the acc isn't alread in the nodes accession list, expand the
   array and add it. */
{
int i;
boolean unique = TRUE;
for(i=0; i<en->accCount; i++)
    {
    if(sameString(en->accs[i], acc))
	{
	unique = FALSE;
	break;
	}
    }
if(unique)
    {
    ExpandArray(en->accs, en->accCount, en->accCount +1);
    en->accs[en->accCount] = cloneString(acc);
    en->accCount++;
    en->accDeepFree = TRUE;
    }
}

void printActiveNodesInClass(struct exonGraph* eg, int class)
/* Print out all of the nodes for a given class
   that are not inactive. */
{
int nodeIx = 0;
struct exonNode *en = NULL;
for(nodeIx = 0; nodeIx < eg->nodeCount; nodeIx++) 
    {
    en = eg->nodes[nodeIx];
    if(en->class == class) 
	fprintf(stderr, "%d,", en->id);
    }
fprintf(stderr, "\n");
}

boolean stackCheck() 
{
static int count = 0;
count++;
stackCheck();
if(count == 1000000)
    printf("Made it here.\n");
}

boolean queueEmpty(int *queue, int nCount)
{
int i = 0;
for(i = 0; i < nCount; i++)
    if(queue[i] != -1)
	return FALSE;
return TRUE;
}

int getMinEntry(int* queue, int nCount)
{
int minVal = -1, currentMin = BIGNUM, i = 0;
for(i = 0; i < nCount; i++) 
    if(queue[i] < currentMin && queue[i] >= 0)
	{
	minVal = i;
	currentMin = queue[i];
	}
return minVal;
}

void updateFringe(struct exonGraph *eg, int vIx, int *priority, int *dist, int nCount)
{
int mydist = dist[vIx];
struct exonNode *en = eg->nodes[vIx];
int i = 0;
for(i = 0; i < en->edgeOutCount; i++)
    {
    int d = mydist + 1;
    if(d <= dist[en->edgesOut[i]])
	{
	priority[en->edgesOut[i]] = d;
	dist[en->edgesOut[i]] = d;
	}
    }
}

int nodesConnectDijkstra(struct exonGraph *eg, struct exonNode *start, struct exonNode *end)
/* Here we go.... */
{
int *dist = NULL;
int *status = NULL;
int *priority = NULL;
int toRet = 0;
int i = 0;
int nCount = eg->nodeCount;
AllocArray(dist, nCount);
AllocArray(status, nCount);
AllocArray(priority, nCount);
for(i = 0; i < nCount; i++) 
    {
    dist[i] = BIGNUM;
    priority[i] = BIGNUM;
    }
priority[start->id] = 0;
dist[start->id] = 0;
while(!queueEmpty(priority, nCount))
    {
    int v = getMinEntry(priority, nCount);
    if(v == -1) 
	break;
    updateFringe(eg, v, priority, dist, nCount);
    priority[v] = -1;
    }
toRet = dist[end->id];
freez(&dist);
freez(&priority);
freez(&status);
return toRet;
}

boolean nodesConnect(struct exonGraph *eg, struct exonNode *start, struct exonNode *end)
/* Return TRUE if the two nodes connect FALSE if they do not. */
{
static int count = 0;
int nodeIx = 0;
boolean connected = FALSE;
struct exonNode *en = NULL;
int outIx = -1;
for(nodeIx = 0; nodeIx < start->edgeOutCount; nodeIx++) 
    {
    count++;
    outIx = start->edgesOut[nodeIx];
    en = eg->nodes[outIx];
    if(outIx == end->id)
	return TRUE;
    else if(en->tStart >= end->tEnd)
	return FALSE;
    else if(nodesConnect(eg, en, end))
	return TRUE;
    }
return FALSE;
}

int nodesConnectClass(struct exonGraph *eg, struct exonNode *start, int classIx)
/* Return end node id if starts connects to a node of class "classIx, else -1" */
{
int nodeIx = 0;
int connector = 0;
struct exonNode *en = NULL;
for(nodeIx = 0; nodeIx < start->edgeOutCount; nodeIx++) 
    {
    en = eg->nodes[start->edgesOut[nodeIx]];
    if(en->class == classIx)
	return en->id;
    else if((connector = nodesConnectClass(eg, en, classIx)) != -1)
	return connector;
    }
return -1;
}

void printIntArray(int count, int *array)
/* Little utility function, useful for debugging. */
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
/* Return TRUE if both needle and haystack have the same classes of
 outgoing nodes and incoming nodes.  FALSE otherwise. */
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
while((array != last && (*(array)) == id))
    {
    array++;
    finalCount--;
    }
/* Remove duplicates in rest of array. */
head = start = array;
while(array != last && ++array != last)
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
struct exonNode *nodeIn = NULL, *nodeOut = NULL;
int i,j;

/* First modify the incoming nodes. */
for(i=0; i<mark->accCount; i++)
    addEvidenceToNode(unique, mark->accs[i]);
for(i=0; i < mark->edgeInCount; i++)
    {
    nodeIn = eg->nodes[mark->edgesIn[i]];
    for(j=0; j < nodeIn->edgeOutCount; j++)
	{
	if(nodeIn->edgesOut[j] == mark->id)
	    {
	    nodeIn->edgesOut[j] = unique->id;
	    }
	}
    nodeIn->edgeOutCount = mergeDuplicatesFast(nodeIn->edgeOutCount, nodeIn->edgesOut, nodeIn->id);
    }

for(i=0; i < mark->edgeOutCount; i++)
    {
    nodeOut = eg->nodes[mark->edgesOut[i]];
    for(j=0; j < nodeOut->edgeInCount; j++)
	{
	if(nodeOut->edgesIn[j] == mark->id)
	    {
	    nodeOut->edgesIn[j] = unique->id;
	    }
	}
    if(unique->id != nodeOut->id)
	{
	exonNodeConnect(unique, nodeOut); 
	}
    nodeOut->edgeInCount = mergeDuplicatesFast(nodeOut->edgeInCount, nodeOut->edgesIn, nodeOut->id);
    }
mark->class = inActive;
}

void mergeNodesSameClass(struct exonGraph *eg, struct exonNode *keeper, struct exonNode *cut)
/* Remove "cut" and replace with "keeper" calling recursivllly
   on all of th enodes with same class as keeper. */
{
int nodeIx = 0;
for(nodeIx = 0; nodeIx < cut->edgeOutCount; nodeIx++)
    {
    struct exonNode *nOut = eg->nodes[cut->edgesOut[nodeIx]];
    if(nOut->class == inActive)
	continue;
    if(nOut->class == keeper->class)
	{
	mergeNodesSameClass(eg, keeper, nOut);
	}
    }
spliceOutExonNode(eg, cut, keeper);
}

void trimGraphByDijkstra(struct exonGraph *eg, struct exonNode *start)
/* Find all the nodes connected to this one of the same class and  */
{
int *dist = NULL;
int *status = NULL;
int *priority = NULL;
int i = 0;
int nCount = eg->nodeCount;
AllocArray(dist, nCount);
AllocArray(status, nCount);
AllocArray(priority, nCount);
for(i = 0; i < nCount; i++) 
    {
    dist[i] = BIGNUM;
    priority[i] = BIGNUM;
    }
priority[start->id] = 0;
dist[start->id] = 0;
while(!queueEmpty(priority, nCount))
    {
    int v = getMinEntry(priority, nCount);
    if(v == -1) 
	break;
    updateFringe(eg, v, priority, dist, nCount);
    priority[v] = -1;
    }

for(i = 0; i < nCount; i++)
    {
    struct exonNode *en = eg->nodes[i];
    if(en->id == start->id)
	continue;
    if(en->class != inActive && en->class == start->class && dist[i] != BIGNUM)
	{
	spliceOutExonNode(eg, en, start);
	}
    }
freez(&dist);
freez(&priority);
freez(&status);
}

void exonGraphTrim(struct exonGraph *eg)
/* Merge nodes of the same class that are connected. */
{
struct exonNode *current = NULL, *enOut = NULL;
int i = 0;
for(i=0; i < eg->nodeCount; i++) 
    {
    struct exonNode *current = eg->nodes[i];
    if(current->class != inActive)
	{
	trimGraphByDijkstra(eg, current);
	}
    }
}

void exonGraphTrimOrig(struct exonGraph *eg)
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

    /* Avoid nodes that have already been labeled as redundant. */
    if(mark->class == inActive)
	continue;

    /* Look to see if this node is redundant in a less obvious way. */
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

/* Convenience variables - less typing. */
int *vPos = agx->vPositions;
int *starts = agx->edgeStarts;
int *ends = agx->edgeEnds;
unsigned char *vTypes = agx->vTypes;

/* Create the second node as we'll need that no matter what. */
if(!isExonEdge(agx,edge))
    return;
AllocVar(en);
en->tName = cloneString(agx->tName);
snprintf(en->strand,sizeof(en->strand),"%s", agx->strand);
en->tStart = vPos[starts[edge]];
addExonNodeStart(en, en->tStart);
en->tEnd = vPos[ends[edge]];
addExonNodeEnd(en, en->tEnd);
en->type = ggExon;
en->class = edge;
en->startClass = starts[edge];
en->endClass = ends[edge];
en->startType = vTypes[starts[edge]];
en->endType = vTypes[ends[edge]];
en->ep = ep;
en->accCount++;
en->accs = needMem(sizeof(char *));
en->accs[0] = cloneString(ep->qName);
/* Check to make sure this makes sense. */
if(en->tEnd <= en->tStart)
    {
    warn("\nexonWalk::addNodeToPath() - exon start: %d > exon end:%d for accession %s", 
	 en->tStart, en->tEnd, ep->qName);
    exonNodeFree(&en);
    return exonProblem;
    }
if(sameExonDifferentEnds(tail, en)) /* If this is a duplicate type deal with fuzzy edge. */
    {
    tail->type = ggExon;
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
    tail->type = ggExon;
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
ep->tEnd = max(ep->tEnd, en->tEnd);
ep->tStart = min(ep->tStart, en->tStart);
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

void singleExonBedOut(char *chrom, int start, int end, char *name, char *strand,
	FILE *out)
/* Print out single exon bed. */
{
fprintf(out, "%s\t%u\t%u\t%s\t1000\t%s\t%u\t%u\t0\t", 
        chrom, start, end, name, strand, start, end);
fprintf(out, "1\t");			// exonCount
fprintf(out, "%d,\t", end-start);	// sizes
fprintf(out, "0\n");		        // relative starts
}

void exonPathBedOut(struct altGraphX *agx, struct exonPath *ep, FILE *out)
/** Print out a exon path in the form of a bed file, for viewing in the browser. */    
{
int i=0;
struct exonNode *en = NULL;
int exonCount = 0;
exonCount = exonPathCountExons(ep);
if(exonCount > 0) 
    {
    fprintf(out, "%s\t%u\t%u\t%s.%s\t1000\t%s\t%u\t%u\t0\t", 
	    ep->tName, ep->tStart, ep->tEnd, agx->name, ep->qName, ep->strand, ep->tStart, ep->tEnd);
/* Only print out blocks that correspond to exons. */
    
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



void mergePathEvidence(struct exonPath *ep, struct exonPath *query)
/* Add the evidence in query to nodes in ep. */
{
struct exonNode *en=NULL;
for(en = ep->nodes; en != NULL; en = en->next)
    {
    addEvidenceToNode(en, query->qName);
    }
}

boolean canMergePaths(struct exonPath *epList, struct exonPath *query)
/** Return TRUE if there is a path just like this in the list already. */
{
struct exonPath *ep = NULL;
for(ep = epList; ep != NULL; ep = ep->next)
    if(sameString(ep->path, query->path))
	{
	mergePathEvidence(ep, query);
	return TRUE;
	}
return FALSE;
}

struct exonPath *initExonPathFromAgxAcc(struct altGraphX *agx, char *acc)
/** Allocated and initialize an exon path with 
    the info in the agx and given accesion identifier. */
{
struct exonPath *ep = NULL;
AllocVar(ep);
ep->tStart = BIGNUM;
ep->qName = cloneString(acc);
snprintf(ep->strand, sizeof(ep->strand), "%s", agx->strand);
ep->tName = cloneString(agx->tName);
return ep;
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

void finishPath(struct exonGraph *eg, struct exonPath **epList, struct exonPath *ep,
		boolean pathNotCorrupt, boolean *goodPath)
/** Finish off a path and add it to the list if it is good. */
{
struct exonNode *en = NULL;
exonPathCreateStringRep(ep);
if(debug) { nodeSanityCheck(eg); }
if(!noPathMerging && canMergePaths(*epList, ep))
    {
    goodPath = FALSE;
    slAddHead(&eg->rejects, ep);
    for(en = ep->nodes; en != NULL; en = en->next) 
	en->class = inActive;
    }
if(pathNotCorrupt && goodPath)
    slSafeAddHead(epList, ep);
else
    {
    if(!pathNotCorrupt)
	exonPathFree(&ep);
    }
}

struct exonGraph *exonGraphFromAgx(struct altGraphX *agx)
/** Construct a list of exon paths, one for every mrnaRef in the altGraphX, and return it. */
{
int i,j,k;
struct exonPath *ep = NULL, *epList = NULL;
struct exonGraph *eg = NULL;
struct evidence *ev = NULL;
bool **em = NULL; /* Edge matrix for agx. */
int *starts = agx->edgeStarts;
int *ends = agx->edgeEnds;
char *acc = NULL;
em = altGraphXCreateEdgeMatrix(agx);
AllocVar(eg);
initExonGraphFromAgx(eg, agx);
for(i=0; i < agx->mrnaRefCount; i++)
    {
    int lastSplice = -1; /* Use this to keep track of last exon end
			    and make sure that the intron we are creating
			    implicitly in the transcript actually exists 
			    in the splicing graph. */
    boolean pathNotCorrupt = TRUE;
    boolean goodPath = TRUE;
    acc = agx->mrnaRefs[i];
    ep = initExonPathFromAgxAcc(agx, acc);

    /* Loop through the evidence blocks stringing together the
       exons for this accession. Note that some accessions may
       have had exons removed by graph pruning (i.e. by ortholgous
       exons in mm5) */
    for(ev=agx->evidence; ev != NULL; ev = ev->next)
	{
        for(k=0; k<ev->evCount; k++)
	    {
	    if(sameString(acc, agx->mrnaRefs[ev->mrnaIds[k]]))
		{
		int edgeIx = slIxFromElement(agx->evidence,ev);
		int sig = 0;
		int currentSplice = starts[edgeIx];

		/* Only stringing exons together. */
		if(!isExonEdge(agx, edgeIx))
		    continue;

		/* If this edge isn't in the splice graph we
		   break the accession into two separate paths. */
		if(lastSplice != -1 && !em[lastSplice][currentSplice]) 
		    {
		    /* Finish of this path. */
		    finishPath(eg, &epList, ep, pathNotCorrupt, &goodPath);

		    /* Initialize the next path. */
		    ep = initExonPathFromAgxAcc(agx, acc);
		    lastSplice = -1;
		    pathNotCorrupt = TRUE;
		    goodPath = TRUE;
		    }
		
		sig = addNodeToPath(eg, ep, agx, edgeIx);
		if(sig == exonProblem)
		    pathNotCorrupt = FALSE;
		lastSplice = ends[edgeIx];
		/* We found a match alread for this exon, skip the rest. */
		break;
		}
	    }
	}
    finishPath(eg, &epList, ep, pathNotCorrupt, &goodPath);
    }
altGraphXFreeEdgeMatrix(&em, agx->vertexCount);
/* slReverse(&epList); */
slSort(&epList, exonPathCmp);
slReverse(&eg->rejects);
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
//assert(sameString(en1->strand, en2->strand));
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

boolean extendsToEndSmall(struct exonGraph *eg, struct smallPath *smHayStack, int offSet, 
			  struct smallPath *smNeedle, boolean allEn2)
/** See if the list of nodes starting at en1 and en2 are of identical
   classes until the end. If allEn2 then all of en2 must be present
   in en1 to return TRUE.*/
{
boolean extends = TRUE;
int i = 0;
for(i = 0; i < smHayStack->currCount - offSet && i < smNeedle->currCount; i++)
    {
    if(!compatibleNodes(eg->nodes[smHayStack->path[i+offSet]],
			eg->nodes[smNeedle->path[i]]))
	extends = FALSE;
    }
if(extends && allEn2)
    extends = (i == smNeedle->currCount);
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
for(en = hayStack->nodes; en != NULL && keepGoing && needle->nodes != NULL; en = en->next)
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
/* Quick check...*/
if(needle->tStart < hayStack->tStart || needle->tEnd > hayStack->tEnd)
    return FALSE;

/* Ok on to more thorough checks. */
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

boolean isSmallSubPath(struct exonGraph *eg, struct smallPath  *smHayStack, struct smallPath *smNeedle)
/** Return TRUE if the needle is a subpath of the haystack, FALSE otherwise. */
{
struct exonNode *en = NULL;
boolean subPath = FALSE;
boolean keepGoing = TRUE;
struct exonNode **n = eg->nodes;
int i = 0;
/* Quick check...*/
if(n[smNeedle->path[0]]->tStart < n[smHayStack->path[0]]->tStart || 
   n[smNeedle->path[smNeedle->currCount-1]]->tEnd > n[smHayStack->path[smHayStack->currCount-1]]->tEnd)
    return FALSE;

/* Ok on to more thorough checks. */
for(i = 0; i < smHayStack->currCount && keepGoing; i++)
    {
    if(compatibleNodes(n[smNeedle->path[0]], n[smHayStack->path[i]]))
	{
	if(extendsToEndSmall(eg, smHayStack, i, smNeedle, TRUE))
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
    for(path2 = eg->paths; path2 != NULL; path2 = path2->next)
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
int i=0;
assert(en);
enCopy = CloneVar(en);
enCopy->tName = cloneString(en->tName);
enCopy->edgesIn = NULL;
enCopy->edgesOut = NULL;
if(en->edgeOutCount != 0)
    enCopy->edgesOut = CloneArray(en->edgesOut, en->edgeOutCount);
if(en->edgeInCount != 0)
    enCopy->edgesIn = CloneArray(en->edgesIn, en->edgeInCount);
if(en->accCount != 0)
    {
    enCopy->accs = CloneArray(en->accs, en->accCount);
    for(i=0; i<en->accCount; i++)
	enCopy->accs[i] = cloneString(en->accs[i]);
    }
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

struct exonPath *exonPathFromSmallPath(struct exonGraph *eg, struct smallPath *sp)
/* Create an exon path from the small path provided. */
{
struct exonNode *en = NULL;
int i = 0;
struct exonPath *ep = NULL;
AllocVar(ep);
ep->next = NULL;
ep->tName = cloneString(eg->tName);
snprintf(ep->strand, sizeof(ep->strand), "%s", eg->strand);
ep->qName = NULL;
ep->tStart = BIGNUM;
ep->tEnd = 0;
ep->nodes = NULL;
for(i = 0; i < sp->currCount; i++) 
    {
    en = exonNodeCopy(eg->nodes[sp->path[i]]);
    slAddHead(&ep->nodes, en);
    ep->tStart = min(en->tStart, ep->tStart);
    ep->tEnd = max(ep->tEnd, en->tEnd);
    }
exonPathCreateStringRep(ep);
slReverse(&ep->nodes);
return ep;
}

void checkForMaximalSmallPath(struct exonGraph *eg, struct smallPath **maximalPaths, struct smallPath *spList)
/** Look to see if the history path is a subpath of any of the 
   maximalPaths or vice versa. */
{
struct smallPath *sp = NULL, *spNext = NULL;
boolean maximal = TRUE;
struct smallPath *history = NULL;
for(history = spList; history != NULL; history = history->next)
    {
    maximal = TRUE;
    for(sp = *maximalPaths; sp != NULL; sp = spNext)
	{
	spNext = sp->next;
	if(isSmallSubPath(eg, sp, history))
	    {
	    maximal = FALSE;
	    break;
	    }
	else if(isSmallSubPath(eg, history, sp))
	    {
	    slRemoveEl(maximalPaths, sp);
	    smallPathFree(&sp);
	    }
	}
    if(maximal)
	{
	struct smallPath *copy = smallPathCopy(history);
	slAddHead(maximalPaths, copy);
	}
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

boolean noOutEdges(struct exonGraph *eg, struct exonNode *en)
/* Return TRUE if there is no path out of this node that isn't
   of the same class. FALSE otherwise. */
{
struct exonNode *target = NULL;
int i;
for(i=0; i < en->edgeOutCount; i++)
    {
    target = eg->nodes[en->edgesOut[i]];
    if(target->class != en->class)
	return FALSE;
    }
return TRUE;
}

int maxOutNodeDegree(struct exonGraph *eg)
{
int i = 0;
int max = 0;
int maxIx = 0;
for(i = 0; i<eg->nodeCount; i++)
    {
    if(eg->nodes[i]->class != inActive && eg->nodes[i]->edgeOutCount >= max)
	{
	max = eg->nodes[i]->edgeOutCount;
	maxIx = i;
	}
    
    }
return max;
}

int maxInNodeDegree(struct exonGraph *eg)
{
int i = 0;
int max = 0;
int maxIx = 0;
for(i = 0; i<eg->nodeCount; i++)
    {
    if(eg->nodes[i]->class != inActive && eg->nodes[i]->edgeInCount >= max)
	{
	max = eg->nodes[i]->edgeInCount;
	maxIx = i;
	}
    
    }
return max;
}

void exonGraphBfs(struct exonGraph *eg, struct smallPath **maximalPaths, struct heap *queue )
/** A modified breadth first search of the graph. Nodes are investigated not
only in the order of distance in nodes but also in order of distance from
transcription start. All paths to nodes are kept track of as search is made. */
{
int globalCount = 0;
boolean memOk = TRUE;
int status;
int *memSavings =NULL;
int baseLineMem = 0;
long startTime = clock1();
struct mallinfo mi = mallinfo();
assert(queue);
/* Push the abort handler to skip this one but not die if something happens
   (like we exceed maximum memory) */
pushAbortHandler(exonWalkAbort);
memSavings = needMem(sizeof(int) * 1000);
status = setjmp(exonWalkRecover);
if(status == 0)
    {
    while(!heapEmpty(queue))
	{
	int i =0;
	struct exonNode *en = heapExtractMin(queue);
	struct smallPath *sp = NULL, *spCopy = NULL;
	struct exonNode *enCopy = NULL;
	long currentTime = clock1();

	if(currentTime - startTime > 1)
	    verbose(1, "Time so far: %d seconds", currentTime - startTime);

	/* If this is taking too long then give up... */
	if((currentTime - startTime) >= maxWalkTime)
	    {
	    warn("%s:%d-%d taking to long quitting.", eg->tName, eg->tStart, eg->tEnd);
	    errAbort("");
	    } 
	for(i=0; i<en->edgeOutCount; i++)
	    {
	    struct exonNode *target = eg->nodes[en->edgesOut[i]];
	    if(target->class == inActive)
		warn("exonGraphBfs() - %s:%d-%d, target %d (%s:%d-%d) is inactive!, en = %d", 
		     eg->tName, eg->tStart, eg->tEnd, target->id, 
		     target->tName, target->tStart, target->tEnd, en->id);
	    for(sp = en->paths; sp != NULL; sp = sp->next)
		{
		struct mallinfo mem = mallinfo();



		if((mem.uordblks - baseLineMem) > maxMem)
		    errAbort("exonGraphBfs() - %s:%d-%d exceeded maximum memory usage %d for breadth first search, used %d.",
			     eg->tName, eg->tStart, eg->tEnd, maxMem, (mem.uordblks - baseLineMem));
		if(debug) { nodeSanityCheck(eg); }
		spCopy = smallPathCopy(sp);
		/* If we're making a move to another exon then include that exon in the path... */
		if(target->class != en->class && en->class != BIGNUM)
		    {
		    appendToSmallPath(spCopy, target->id);
		    }
		slAddHead(&target->paths, spCopy);
		
		/* Some debugging stuff. */
		if(debug) 
		    { 
		    nodeSanityCheck(eg); 
		    }
		}
	    
	    /* If the haven't been seen yet, queue them up... */
	    if(target->color == enWhite) 
		{
		verbose(2, ".");
		target->color = enGray;
		if(!heapContainsNode(queue, target))
		    heapMinInsert(queue, target);
		}
	    }
	if(noOutEdges(eg,en) && en->class != BIGNUM) /* It is a terminal node...*/
	    {
	    verbose(2, "M", stderr);
	    checkForMaximalSmallPath(eg, maximalPaths, en->paths);
	    verbose(2, "D");
	    }
	smallPathFreeList(&en->paths);
	en->color = enBlack;
	}
    }
else 
    {
    freez(&memSavings);
    warn("exonGraphBfs() - %s:%d-%d, caught error, something aborted "
	 "(tried to use too much memory or took too long?). Skipping.", 
	 eg->tName, eg->tStart, eg->tEnd);
    smallPathFreeList(maximalPaths);
    }
verbose(2, "\n");
freez(&memSavings);
popAbortHandler();
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


int nodeStartHeapCmp(const void *va, const void *vb)
/** Compart to sort based on tStart of node. */
{
const struct exonNode *a = va;
const struct exonNode *b = vb;
return a->tStart - b->tStart;
}

struct smallPath *newSmallPath(struct exonNode *en)
/* Allocate a new small exon path. */
{
struct smallPath *sp = NULL;
AllocVar(sp);
sp->maxCount = 10;
AllocArray(sp->path, sp->maxCount);
sp->path[sp->currCount++] = en->id;
return sp;
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

struct exonPath *exonGraphBreadthFirstMaxPaths(struct altGraphX *agx, struct exonGraph *eg)
/** Construct maximal paths through the exonGraph. Maximal paths
   are those which are not subpaths of any other path. Based on breadth
   first search using implicit ordering of exons.
*/
{
int i =0, x = 0;
int index = 0;
struct heap *queue = newHeap(8, nodeStartHeapCmp);
struct smallPath *maximalPaths = NULL, *smallPath = NULL;
struct exonPath *ep = NULL, *epList = NULL;
struct exonNode **orderedNodes = CloneArray(eg->nodes, eg->nodeCount);
boolean skipTrimGraph = optionExists("noTrim");
int activeNodes, activeEdges;
int grayCount=0,whiteCount=0,blackCount=0;
int maxOut = 0, maxIn = 0;
activeNodes = countActiveNodes(eg);
activeEdges = countActiveEdges(eg);
qsort(orderedNodes, eg->nodeCount, sizeof(struct exonNode *), nodeStartCmp);
if(debug) { nodeSanityCheck(eg); }
maxOut = maxOutNodeDegree(eg);
maxIn = maxInNodeDegree(eg);
/* warn("Before Trim: %s:%u-%u\tactiveNodes = %d\tactiveEdges = %d (maxIn %d, maxOut %d)",  */
/* 	eg->tName, eg->tStart, eg->tEnd, activeNodes, activeEdges, maxIn, maxOut); */
fflush(stderr);
if(debug) { nodeSanityCheck(eg); }
/* Trim the graph down to a smaller number of active nodes. */
if(!skipTrimGraph)
    {
    exonGraphTrim(eg);
    activeNodes = countActiveNodes(eg);
    activeEdges = countActiveEdges(eg);
    maxOut = maxOutNodeDegree(eg);
    maxIn = maxInNodeDegree(eg);
/*     warn("After Trim: %s:%u-%u\tactiveNodes = %d\tactiveEdges = %d (maxIn %d, maxOut %d)",  */
/* 	eg->tName, eg->tStart, eg->tEnd, activeNodes, activeEdges, maxIn, maxOut); */
    fflush(stderr);
    }
fflush(stderr);
if(debug) { nodeSanityCheck(eg); }
if(activeEdges > 500)
    {
    altGraphXTabOut(agx, hippos);
    warn("%s:%d-%d is a hippo...", eg->tName, eg->tStart, eg->tEnd);
    }
else 
    {
/* Seed the priority queue with nodes that don't have anything pointing
   to them, or that only have incoming edges from other exons that are the same. */
    for(i=0; i<eg->nodeCount; i++)
	{
	if(noIncomingEdges(eg, orderedNodes[i]) && orderedNodes[i]->class != inActive)
	    {
	    struct smallPath *sp = newSmallPath(orderedNodes[i]);
	    slAddHead(&orderedNodes[i]->paths, sp);
	    heapMinInsert(queue, orderedNodes[i]);
	    }
	}
    
    /* Do the exonWalk. */
    if(debug) { nodeSanityCheck(eg); }
    exonGraphBfs(eg, &maximalPaths, queue);
    
    /* Do some cleanup and reporting. */
    for(i=0; i<eg->nodeCount; i++)
	{
	struct exonNode *target = eg->nodes[i];
	if(target->class != inActive)
	    {
	    smallPathFreeList(&target->paths);
	    if(diagnostics)
		{
		if(target->color == enWhite)
		    {
		    whiteCount++;
		    fprintf(whites, "%s\t%d\t%d\n", target->tName, target->tStart, target->tEnd);
		    }
		else if(target->color == enGray)
		    {
		    grayCount++;
		    }
		else if(target->color == enBlack)
		    {
		    blackCount++;
		    }
		else
		    {
		    errAbort("Don't know what color %d is for %d", target->color, target->id);
		    }
		}
	    }
	
	}
    }
heapFree(&queue);
freez(&orderedNodes);
for(smallPath = maximalPaths; smallPath != NULL; smallPath = smallPath->next)
    {
    ep = exonPathFromSmallPath(eg, smallPath);
    slAddHead(&epList, ep);
    }
slReverse(&epList);
smallPathFreeList(&maximalPaths);
return epList;
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
for(i=0; i <  mark->accCount; i++)
    {
    if(hashLookup(mrnaHash, mark->accs[0]))
	return TRUE;
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
for(i=0; i<en->accCount; i++)
    {
    count++;
    }
return count;
}

boolean confidentPath(struct hash *mrnaHash, struct exonGraph *eg, 
		      struct exonPath *maximalPaths, struct exonPath *ep)
/** Return TRUE if all edges in ep were seen in 5% or more
   of the paths.  Always believe edges with 4 or more paths. 
   Also always believe all edges when there are 10 or less paths.*/
{
double minPercent = optionFloat("minPercent", .05);
int trumpSize = optionInt("trumpSize", 450);
int mrnaVal = optionInt("mrnaWeight", 20);
double minNum = 0;
struct exonNode *en = NULL;
int size = 0;
int pathCount = slCount(eg->paths) + slCount(eg->rejects);
boolean result = TRUE;
if(pathCount > 10)
    {
    minNum = minPercent * pathCount + 1;
    if(minNum > 4)
	minNum = 4;
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
}

void addMrnaAccsToHash(struct hash *hash)
/** Load up all the accessions from the database that are mRNAs. */
{
char query[256];
char *db = optionVal("db",NULL);
if(db == NULL)
    errAbort("Must specify a database.");
struct sqlConnection *conn = NULL;
hSetDb(db);
conn = hAllocConn();
sqlSafef(query, sizeof(query), "select acc from gbCdnaInfo where type = 2");
hashRow0(conn, query, hash);
}

void removeShortFragments(struct exonGraph *eg)
/** Remove paths that doen't have a least a few exons of the
    maximal path.  The minimum # of exons is 2 for short genes.
    For genes with more than 16 exons the minimum might go up to 3 
    or a little more. */
{
struct exonPath *ep = NULL, *epList = NULL, *epNext = NULL, *rejects = NULL;
int maxExons = 0;
int threshold = 0;
/* Find the transcript with the most exons. */
for(ep = eg->paths; ep != NULL; ep = ep->next)
    maxExons = max(ep->nodeCount, maxExons);

/* Now remove paths that don't meet threshold. */
threshold = max(floor(log2(maxExons)-2), 2); 
/* threshold = 2; */
for(ep = eg->paths; ep != NULL; ep = epNext)
    {
    epNext = ep->next;
    if(ep->nodeCount >= threshold) 
	{
	slAddHead(&epList, ep);
	}
    else 
	{
	struct exonNode *en = NULL;
	slAddHead(&rejects, ep);
	for(en = ep->nodes; en != NULL; en = en->next)
	    en->class = inActive;
	}
    }
slReverse(&epList);
slReverse(&rejects);
eg->rejects = slCat(eg->rejects, rejects);
eg->paths = epList;
eg->pathCount = slCount(epList);
}

boolean agxSingleExon(struct altGraphX *agx)
/* Return true for single exon graphs. */
{
if ( agx->vertexCount == 2 && agx->edgeCount == 1)
    {
    unsigned char v0Type = agx->vTypes[0];
    unsigned char v1Type = agx->vTypes[1];
    int eType = agx->edgeTypes[0];
    verbose(2, "agxSingleExon(%s) %s:%d-%d v0Type %d, v1Type %d, eType %d\n",
    	agx->name, agx->tName, agx->tStart+1, agx->tEnd, v0Type, v1Type, eType);
    return TRUE;
    }
else
    return FALSE;
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

boolean mrnaFilter = optionExists("mrnaFilter");
boolean removeFrags = !optionExists("keepFrags");
struct hash *mrnaHash = newHash(10);
int index = 0;
int count = 0;
struct exonPath *ep=NULL, *epList = NULL;
agxList = altGraphXLoadAll(inFile);
warn("Read %d graphs from %s", slCount(agxList), inFile);
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
/* fprintf(f, "track name=exonWalk description=\"exonWalk Final Picks\" color=23,117,15\n"); */
warn("Creating exon graphs %d graphs per dot.", max(round(slCount(agxList)/20), 1));
dotForUserInit( max(round(slCount(agxList)/20), 1));
for(agx = agxList; agx != NULL; agx = agx->next)
    {
    dotForUser();
    if(diagnostics)
	writeOutNodes(agx, exonSites);
    if (agxSingleExon(agx))
        {
	singleExonBedOut(agx->tName, agx->tStart, agx->tEnd, agx->name, agx->strand, f);
	}
    else
	{

	eg = exonGraphFromAgx(agx);
	if(removeFrags)
	    removeShortFragments(eg);
	currentGraph = eg;
	if(diagnostics)
	    {
	    for(ep = eg->paths; ep != NULL; ep = ep->next)
		exonPathBedOut(agx, ep,cleanEsts);
	    fflush(cleanEsts);
	    }
	exonGraphConnectEquivalentNodes(eg);
	if(debug) { nodeSanityCheck(eg); }

	/* If we have something to output. */
	if(eg->nodeCount != 0)
	    epList = exonGraphBreadthFirstMaxPaths(agx, eg);
	
	for(ep = epList; ep != NULL; ep = ep->next)
	    {
	    char buff[256];
	    snprintf(buff, sizeof(buff), "%d", index++);
	    ep->qName = cloneString(buff);
	    exonPathCreateStringRep(ep);
	    if(TRUE || confidentPath(mrnaHash, eg, epList, ep))
		{
		if(debug)
		    {
		    printf("%s\n", ep->path);
		    exonPathBedOut(agx, ep, stdout);
		    }
		exonPathBedOut(agx, ep,f);
		}
	    else if(diagnostics)
		exonPathBedOut(agx, ep, rejects);
	    }
	exonPathFreeList(&epList);
	exonGraphFree(&eg);
	currentGraph = NULL;
	}
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
/* Everybody's favorite function. */
{
optionInit(&argc, argv, optionSpecs);
if(argc < 3 || optionExists("help"))
    usage();
diagnostics = optionExists("diagnostics");
noPathMerging = optionExists("noPathMerging");
maxMem = optionInt("maxMem", 256000000);
debug = optionExists("debug");
maxWalkTime = optionInt("maxWalkTime", 60 * 7);
verboseSetLevel(optionInt("verbose",0));
exonWalk(argv[1], argv[2]);
return 0;
}

