/* spliceWalk.c - program to construct major splice isoforms by
   walking a series of paths through the splice graph. */
#include "common.h"
#include "geneGraph.h"
#include "altGraphX.h"
#include "spliceGraph.h"
#include "jksql.h"
#include "dystring.h"

void usage()
{
errAbort("spliceWalk - uses altGraphX files to construct a splicing graph\n"
	 "which is a combination of all the paths seen by RNA and EST evidence.\n"
	 "The paths are joined where there are overlaps and the resulting super\n"
	 "graph is walked to determine a set of transcripts which are not sub-paths\n"
	 "of each other.\n"
	 "usage:\n\t"
	 "spliceWalk <infile.altGraphX.tab> <outFile.bed>\n");
}

struct spliceGraph *currentGraph = NULL;

void nodeSanityCheck(struct spliceGraph *sg)
/* Loop through and make sure all the nodes have legit ids. */
{
int i,j;
for(i=0;i<sg->nodeCount; i++)
    {
    struct spliceNode *sn = sg->nodes[i];
    for(j=0;j<sn->edgeCount;j++)
	{
	if(sn->edges[j] >= sg->nodeCount)
	    errAbort("spliceWalk::nodeSanityCheck() - node with id %d when max is %d\n", sn->edges[j], sg->nodeCount);
	if(sn->tStart > sg->nodes[sn->edges[j]]->tStart)
	    errAbort("spliceWalk::nodeSanityCheck() -Node %-%u connects to %u-%u but that is the wrong direction.\n",  
		     sn->id, sn->tStart, sg->nodes[sn->edges[j]]->id, sg->nodes[sn->edges[j]]->tStart);
	}
    }
}

void addNodeToPath(struct spliceGraph *sg, struct splicePath *sp, struct altGraphX *agx, int edge)
/* Add the necessary node to the splicePath to include the indicated edge
   from the altGraphX. */
{
struct spliceNode *e1 = NULL, *e2 = NULL;
struct spliceNode *tail = NULL;


AllocVar(e2);

tail = slLastEl(sp->nodes);

if(sp->nodes == NULL)
    {
/* Create the first node. */
    AllocVar(e1);
    e1->tName = cloneString(agx->tName);
    snprintf(e1->strand,sizeof(e1->strand),"%s", agx->strand);
    e1->tStart = e1->tEnd = agx->vPositions[agx->edgeStarts[edge]];
    e1->type = agx->vTypes[agx->edgeStarts[edge]];
    e1->class = agx->edgeStarts[edge];
    spliceGraphAddNode(sg, e1);
    sp->tStart = e1->tStart;
    }
/* Create the second node. */
e2->tName = cloneString(agx->tName);
snprintf(e2->strand,sizeof(e2->strand),"%s", agx->strand);
e2->tStart = e2->tEnd = agx->vPositions[agx->edgeEnds[edge]];
e2->type = agx->vTypes[agx->edgeEnds[edge]];
e2->class = agx->edgeEnds[edge];
/* Register new nodes with the spliceGraph. */
spliceGraphAddNode(sg, e2);

/* Connect nodes to form path. */
if(sp->nodes == NULL)
    {
//    spliceNodeConnect(tail, e1);
    spliceNodeConnect(e1, e2);
    
/* Add nodes to splicePath. */
    slAddTail(&sp->nodes, e1);
    slAddTail(&sp->nodes, e2);
    sp->nodeCount += 2;
    }
else 
    {
    spliceNodeConnect(tail, e2);
    slAddTail(&sp->nodes, e2);
    sp->nodeCount += 1;
    }
/* Expand the limits of the path as necessary. */
if(e2->tEnd > sp->tEnd)
    sp->tEnd = e2->tEnd;
nodeSanityCheck(sg);
}

boolean isExon(struct spliceNode *sn1, struct spliceNode *sn2)
/* Return TRUE if edge between sn1 and sn2 is an exon, FALSE otherwise. */
{
if((sn1->type == ggHardStart || sn1->type == ggSoftStart)
   && (sn2->type == ggHardEnd || sn2->type == ggSoftEnd))
    return TRUE;
return FALSE;
}

int splicePathCountExons(struct splicePath *sp)
/* Count the number of exons in a splicePath. */
{
int i=0, count=0;
struct spliceNode *sn = NULL;
for(sn=sp->nodes; sn->next != NULL; sn = sn->next)
    {
    if(isExon(sn,sn->next))
	count++;
    }
return count;
}

void splicePathBedOut(struct splicePath *sp, FILE *out)
/* Print out a splice path in the form of a bed file, for viewing in the browser. */    
{
int i=0;
struct spliceNode *sn = NULL;
fprintf(out, "%s\t%u\t%u\t%s\t1000\t%s\t%u\t%u\t0\t", 
	sp->tName, sp->tStart, sp->tEnd, sp->qName, sp->strand, sp->tStart, sp->tEnd);
/* Only print out blocks that correspond to exons. */
fprintf(out, "%d\t",  splicePathCountExons(sp));
for(sn=sp->nodes; sn->next != NULL; sn = sn->next)
    {
    int size = sn->next->tStart - sn->tStart;
    if(isExon(sn,sn->next))
	fprintf(out, "%d,",size);
    }
fprintf(out, "\t");
for(sn=sp->nodes; sn->next != NULL; sn = sn->next)
    {
    int start = sn->tStart - sp->tStart;
    if(isExon(sn,sn->next))
	fprintf(out, "%d,",start);
    }
fprintf(out, "\n");
}

void initSpliceGraphFromAgx(struct spliceGraph *sg, struct altGraphX *agx)
/* Fill in the simple stuff from the agx to the spliceGraph. */
{
sg->tName = cloneString(agx->tName);
sg->tStart = agx->tStart;
sg->tEnd = agx->tEnd;
snprintf(sg->strand, sizeof(sg->strand), "%s", agx->strand);
sg->classCount = agx->vertexCount;
sg->classStarts = CloneArray(agx->vPositions,  sg->classCount);
sg->classEnds  = CloneArray(agx->vPositions,  sg->classCount);
sg->classTypes = CloneArray(agx->vTypes, sg->classCount);
}

struct spliceGraph *spliceGraphFromAgx(struct altGraphX *agx)
/* Construct a list of splice paths, one for every mrnaRef in the altGraphX, and return it. */
{
int i,j,k;
struct splicePath *sp = NULL, *spList = NULL;
struct spliceGraph *sg = NULL;
struct evidence *ev = NULL;
struct spliceNode *sn = NULL;
char *acc = NULL;
AllocVar(sg);
initSpliceGraphFromAgx(sg, agx);
for(i=0; i < agx->mrnaRefCount; i++)
    {
    AllocVar(sp);
    acc = agx->mrnaRefs[i];
    sp->qName = cloneString(acc);
    snprintf(sp->strand, sizeof(sp->strand), "%s", agx->strand);
    sp->tName = cloneString(agx->tName);
    for(ev=agx->evidence; ev != NULL; ev = ev->next)
	{
        for(k=0; k<ev->evCount; k++)
	    {
	    if(sameString(acc, agx->mrnaRefs[ev->mrnaIds[k]]))
		{
		addNodeToPath(sg, sp, agx, slIxFromElement(agx->evidence,ev));

		}
	    }
	}
//    warn("%d nodes in %s.", slCount(sp->nodes), sp->qName);
    nodeSanityCheck(sg);
    slAddHead(&spList, sp);
    } 

slReverse(&spList);
sg->pathCount = slCount(spList);
sg->paths =  spList;
nodeSanityCheck(sg);
return sg;
}

void splicePathCreateStringRep(struct splicePath *sp)
/* Loop through the nodes in this path and create a string of exons. */
{
struct dyString *dy = newDyString(256);
struct spliceNode *sn = NULL;
for(sn = sp->nodes; sn != NULL; sn = sn->next)
    {
//    if(isExon(sn,sn->next))
	dyStringPrintf(dy, "%d-", sn->class);
//	printf("%s\t%d\t%d\t%d-%d\n", sn->tName, sn->tStart, sn->tEnd, sn->class, sn->next->class);
    }
sp->path = cloneString(dy->string);
freeDyString(&dy);
}

char *getRefForNode(struct spliceGraph *sg, int id)
/* Return the mrna accession that generated the node id by
   looking it up in the spliceGraph. */
{
struct splicePath *sp = NULL;
struct spliceNode *sn = NULL;
for(sp = sg->paths; sp != NULL; sp = sp->next)
    {
    for(sn = sp->nodes; sn != NULL; sn = sn->next)
	if(sn->id == id)
	    return sp->qName;
    }
}

boolean restHardEnds(struct spliceNode *sn)
/* Return true if this node and all the nodes after it
   are hard ends. If possible partial exons are extended to
   meet potential potential splice sites. */
{
for(; sn != NULL; sn=sn->next)
    if(sn->type != ggHardEnd && sn->type != ggSoftEnd)
	return FALSE;
return TRUE;
}


boolean matchInHardEnds(struct spliceNode *sn1, struct spliceNode *sn2)
/* Try to find a match in sn1 list to sn2 node. */
{
for(; sn1 != NULL; sn1 = sn1->next)
    for(; sn2 != NULL; sn2 = sn2->next)
	if(sn1->class == sn2->class)
	    return TRUE;
return FALSE;
}

boolean extendsToEnd(struct spliceNode *sn1, struct spliceNode *sn2, boolean allSn2)
/* See if the list of nodes starting at sn1 and sn2 are of identical
   classes until the end. If allSn2 then all of sn2 must be present
   in sn1 to return TRUE.*/
{
boolean extends = TRUE;
for(; sn1 != NULL && sn2 != NULL; sn1=sn1->next, sn2=sn2->next)
    {
    if(sn1->class != sn2->class) 
	{
	struct spliceNode *test = NULL;
	if(restHardEnds(sn1) || restHardEnds(sn2))
	    extends = matchInHardEnds(sn1, sn2);
	else
	    extends = FALSE;
	}

    }
if(extends && allSn2)
    extends = restHardEnds(sn2);
return extends;
}

void connectNodeList(struct spliceNode *sn1, struct spliceNode *sn2)
/* Connect each pair of nodes in the list togther. */
{
for(; sn1 != NULL && sn2 != NULL; sn1=sn1->next, sn2=sn2->next)
    {
    if(sn1->class == sn2->class)
	spliceNodeConnect(sn1,sn2);
    else if(restHardEnds(sn1) || restHardEnds(sn2))
	{
	struct spliceNode *fuzzy1 = sn1;
	struct spliceNode *fuzzy2 = sn2;
	boolean problem = FALSE;
	for(; fuzzy1 != NULL; fuzzy1 = fuzzy1->next)
	    {
	    for(; fuzzy2 != NULL; fuzzy2 = fuzzy2->next)
		{
		if(fuzzy1->class == fuzzy2->class)
		    {
		    spliceNodeConnect(fuzzy1,fuzzy2);
		    problem = FALSE;
		    }
		}
	    }
	if(problem)
	    {
	    char *mRna1 = NULL, *mRna2 = NULL;
	    mRna1 = getRefForNode(currentGraph, sn1->id);
	    mRna2 = getRefForNode(currentGraph, sn2->id);
	    errAbort("spliceWalk::connectNodeList() - Having trouble finding match in mRna1: %s node %d and mRna2: %s node %d",
		     mRna1, sn1->id, mRna2, sn2->id);
	    }
	}
    else
	{
	char *mRna1 = NULL, *mRna2 = NULL;
	mRna1 = getRefForNode(currentGraph, sn1->id);
	mRna2 = getRefForNode(currentGraph, sn2->id);
	errAbort("spliceWalk::connectNodeList() - Can't connect mRna1: %s node %d and mRna2: %s node %d",
		 mRna1, sn1->id, mRna2, sn2->id);
	}
    }
}



void splicePathConnectEquivalentNodes(struct splicePath *hayStack, struct splicePath *needle)
/* Try to align one path to another and connect nodes that
   overlap in alignment. */
{
struct spliceNode *sn = NULL;
struct spliceNode *mark = NULL;
boolean keepGoing = TRUE;
if(strstr(hayStack->qName, "AI635570") && strstr(needle->qName,"BI910851"))
    uglyf("watchout...\n");
for(sn = hayStack->nodes; sn != NULL && keepGoing; sn = sn->next)
    {
    if(needle->nodes->class == sn->class)
	{
	if(extendsToEnd(sn, needle->nodes, FALSE))
	    {
	    connectNodeList(sn, needle->nodes);
	    }
	keepGoing = FALSE;
	}
    }
}

boolean isSubPath(struct splicePath *hayStack, struct splicePath *needle)
/* Return TRUE if the needle is a subpath of the haystack, FALSE otherwise. */
{
struct spliceNode *sn = NULL;
boolean subPath = FALSE;
boolean keepGoing = TRUE;
if(sameString(hayStack->path, "0-1-2-3-4-5-6-7-8-10-11-") && sameString(needle->path,"4-5-6-7-8-9-10-11-"))
    warn("Why isn't this working");
for(sn = hayStack->nodes; sn != NULL && keepGoing; sn = sn->next)
    {
    if(needle->nodes->class == sn->class)
	{
	if(extendsToEnd(sn, needle->nodes, TRUE))
	    subPath = TRUE;
	keepGoing = FALSE;
	}

    }
return subPath;
}


void spliceGraphConnectEquivalentNodes(struct spliceGraph *sg)
/* Go through and find the equivalent nodes for each path by
   aligning one path against another. */
{
struct splicePath *sp = NULL;
struct splicePath *path1 = NULL, *path2 = NULL;
char *mark = NULL;
/* Create string representations of our exon paths for comparison. */
for(sp = sg->paths; sp != NULL; sp = sp->next)
    splicePathCreateStringRep(sp);
for(path1 = sg->paths; path1 != NULL; path1 = path1->next)
    {
    if(sameString(path1->qName,"BE738125"))
	warn("Get ready");
    for(path2 = path1->next; path2 != NULL; path2 = path2->next)
	{
	if(path1 != path2)
	    splicePathConnectEquivalentNodes(path1, path2);
	nodeSanityCheck(sg);
	}
    }
}

struct splicePath *splicePathCopy(struct splicePath *orig)
/* Allocate and fill in a new splicePath that is a copy of the original. */
{
struct splicePath *sp = NULL;
struct spliceNode *sn = NULL, *snCopy = NULL;
assert(orig);
assert(orig->nodes);
sp = CloneVar(orig);
sp->tName = cloneString(orig->nodes->tName);
snprintf(sp->strand, sizeof(sp->strand), "%s", orig->nodes->strand);
sp->qName = NULL;
sp->tStart = sp->tEnd = orig->nodes->tStart;
sp->path = cloneString(orig->path);
sp->nodes = NULL;
for(sn=orig->nodes; sn != NULL; sn = sn->next)
    {
    snCopy = CloneVar(sn);
    snCopy->edgeCount =0;
    snCopy->edges = NULL;
    if(snCopy->tStart > sp->tEnd)
	sp->tEnd = snCopy->tStart;
    if(snCopy->tStart < sp->tStart)
	sp->tStart = snCopy->tStart;
    slAddHead(&sp->nodes, snCopy);
    }
slReverse(&sp->nodes);
return sp;
}


void checkForMaximalPath(struct spliceGraph *sg, struct splicePath **maximalPaths, struct splicePath *history)
/* Look to see if the history path is a subpath of any of the 
   maximalPaths or vice versa. */
{
struct splicePath *sp = NULL, *spNext = NULL;
boolean maximal = TRUE;
slReverse(&history->nodes);
splicePathCreateStringRep(history);
if(history->nodes != NULL && strstr(history->path, "-1-"))
    uglyf("Checking path %s\n', history->path");
for(sp = *maximalPaths; sp != NULL; sp = spNext)
    {
    spNext = sp->next;
    if(isSubPath(sp, history))
	{
	maximal = FALSE;
	break;
	}
    else if(isSubPath(history, sp))
	{
	slRemoveEl(maximalPaths, sp);
//	splicePathFree(&sp);
	freez(&sp);
	}
    }
if(maximal)
    {
    struct splicePath *copy = splicePathCopy(history);
    slAddHead(maximalPaths, copy);
    }
freez(&history->path);
slReverse(&history->nodes);
}



void spliceGraphPathDfs(struct spliceGraph *sg, struct spliceNode *sn, int nodeIndex, 
			struct splicePath **maximalPaths, struct splicePath *history)
/* Depth first search of the spliceGraph starting from node at nodeIndex.
   keeps track of path taken using history, and will check history against
   maximal paths at black nodes. */
{
int i;
struct spliceNode *copy = CloneVar(sn);
boolean pop = FALSE;
nodeSanityCheck(sg);
copy->next = NULL;
copy->edges = NULL;
copy->edgeCount = 0;
if(history->nodes == NULL || history->nodes->class != sn->class)
    {
    slAddHead(&history->nodes, copy);
    history->nodeCount++;
    pop =TRUE;
    }
if(sn->class == 0)
    printf("wow\n");
sn->color = snGray;
if(sn->type == ggSoftEnd || sn->type == ggSoftStart) /* It is a terminal node...*/
    {
//    printf("%s\t%d\t%d\n", sn->tName, sn->tStart, sn->tEnd);
    checkForMaximalPath(sg, maximalPaths, history);
    }
for(i=0; i<sn->edgeCount; i++)
    {
    struct spliceNode *next = sg->nodes[sn->edges[i]];
    if(next->color == snWhite)
	{
	spliceGraphPathDfs(sg, next, next->id, maximalPaths, history);
	}
    }
if(pop)
    {
    slPopHead(&history->nodes);
    history->nodeCount--;
    }
freez(&copy);
sn->color = snBlack;
}

int nodeStartCmp(const void *va, const void *vb)
/* Compart to sort based on tStart of node. */
{
const struct spliceNode *a = *((struct spliceNode **)va);
const struct spliceNode *b = *((struct spliceNode **)vb);
return a->tStart - b->tStart;
}

struct splicePath *spliceGraphMaximalPaths(struct spliceGraph *sg)
/* Construct maximal paths through the spliceGraph. Maximal paths
   are those which are not subpaths of any other path. */
{
int i=0;
int index = 0;
struct splicePath *maximalPaths = NULL, *sp = NULL;
struct spliceNode **orderedNodes = CloneArray(sg->nodes, sg->nodeCount);
qsort(orderedNodes, sg->nodeCount, sizeof(struct spliceNode *), nodeStartCmp);
for(i=0; i<sg->nodeCount; i++)
    {
    if(orderedNodes[i]->color == snWhite)
	{
	struct splicePath *history = NULL;
	AllocVar(history);
	spliceGraphPathDfs(sg, orderedNodes[i], i, &maximalPaths, history);
	freez(&history);
	}
    }

for(sp = maximalPaths; sp != NULL; sp = sp->next)
    {
    char buff[256];
    snprintf(buff, sizeof(buff), "%d", index++);
    sp->qName = cloneString(buff);
    splicePathCreateStringRep(sp);
    printf("%s\n", sp->path);
    splicePathBedOut(sp, stdout);
    }
return maximalPaths;
}

void writeOutNodes(struct altGraphX *agx, FILE *f)
/* Write out a bed record for each vertice. */
{
int i;
for(i=0; i < agx->vertexCount; i++)
    {
    fprintf(f, "%s\t%u\t%u\t%d\n", agx->tName, agx->vPositions[i], agx->vPositions[i], i);
    }
}

void spliceWalk(char *inFile, char *bedOut)
/* Starting routine. */
{
struct altGraphX *agx = NULL, *agxList = NULL;
struct spliceGraph *sg = NULL, *sgList = NULL;
FILE *f = NULL;
struct splicePath *sp=NULL, *spList = NULL;
warn("Loading file %s", inFile);
agxList = altGraphXLoadAll(inFile);
f = mustOpen(bedOut, "w");
for(agx = agxList; agx != NULL; agx = agx->next)
    {
    warn("Creating splice graph.");
    fprintf(f, "track\n");
    writeOutNodes(agx, f);
    sg = spliceGraphFromAgx(agx);
    currentGraph = sg;
    fprintf(f, "track\n");
    for(sp = sg->paths; sp != NULL; sp = sp->next)
	splicePathBedOut(sp,f);
    warn("Connecting Equivalent nodes.");
    spliceGraphConnectEquivalentNodes(sg);
    warn("Getting maximal paths.");
    nodeSanityCheck(sg);
    fprintf(f, "track\n");
    spList = spliceGraphMaximalPaths(sg);
    for(sp = spList; sp != NULL; sp = sp->next)
	splicePathBedOut(sp,f);
    slAddHead(&sgList, sg);
    currentGraph = NULL;
    }
warn("Done.");
carefulClose(&f);
spliceGraphFreeList(&sgList);
altGraphXFreeList(&agxList);
}


int main(int argc, char *argv[])
{
if(argc != 3)
    usage();
spliceWalk(argv[1], argv[2]);
return 0;
}
