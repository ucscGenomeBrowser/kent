/** spliceWalk.c - program to construct major splice isoforms by
   walking a series of paths through the splice graph. */
#include "common.h"
#include "geneGraph.h"
#include "altGraphX.h"
#include "spliceGraph.h"
#include "jksql.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "hdb.h"


void usage()
{
errAbort("spliceWalk - uses altGraphX files to construct a splicing graph\n"
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
	 "spliceWalk <infile.altGraphX.tab> <outFile.bed>\n");
}

struct spliceGraph *currentGraph = NULL; /** Pointer to current graph, useful for debugging. */
boolean debug = FALSE; /** Turns on some debugging output. */

void nodeSanityCheck(struct spliceGraph *sg)
/** Loop through and make sure all the nodes have legit ids. */
{
int i,j;
for(i=0;i<sg->nodeCount; i++)
    {
    struct spliceNode *sn = sg->nodes[i];
    for(j=0;j<sn->edgeCount;j++)
	{
	if(sn->edges[j] >= sg->nodeCount)
	    errAbort("spliceWalk::nodeSanityCheck() - node with id %d when max is %d\n", sn->edges[j], sg->nodeCount);
	if(sn->tStart > sg->nodes[sn->edges[j]]->tStart && sg->nodes[sn->edges[j]]->class != BIGNUM )
	    errAbort("spliceWalk::nodeSanityCheck() -Node %u-%u connects to %u-%u but that is the wrong direction.\n",  
		     sn->id, sn->tStart, sg->nodes[sn->edges[j]]->id, sg->nodes[sn->edges[j]]->tStart);
	}
    }
}

boolean twoDifferentEnds(struct spliceNode *sn1, struct spliceNode *sn2)
/** Return true if both spliceNodes are end types, but occur at different
   loacations, otherwise return FALSE. */
{
if(sn1->type == ggSoftStart || sn1->type == ggHardStart || sn2->type == ggHardStart || sn2->type == ggSoftStart)
    return FALSE;
if(sn1->class == sn2->class)
    return FALSE;
assert(sn1->tStart != sn2->tStart);
return TRUE;
}

struct spliceNode *initEndSpliceNode(struct altGraphX *agx, int edge)
{
struct spliceNode *e2 = NULL;
AllocVar(e2);
e2->tName = cloneString(agx->tName);
snprintf(e2->strand,sizeof(e2->strand),"%s", agx->strand);
e2->tStart = e2->tEnd = agx->vPositions[agx->edgeEnds[edge]];
e2->type = agx->vTypes[agx->edgeEnds[edge]];
e2->class = agx->edgeEnds[edge];
return e2;
}

struct spliceNode *initStartSpliceNode(struct altGraphX *agx, int edge)
{
struct spliceNode *e1 = NULL;
AllocVar(e1);
e1->tName = cloneString(agx->tName);
snprintf(e1->strand,sizeof(e1->strand),"%s", agx->strand);
e1->tStart = e1->tEnd = agx->vPositions[agx->edgeStarts[edge]];
e1->type = agx->vTypes[agx->edgeStarts[edge]];
e1->class = agx->edgeStarts[edge];
return e1;
}

void addNodeToPath(struct spliceGraph *sg, struct splicePath *sp, struct altGraphX *agx, int edge)
/** Add the necessary node to the splicePath to include the indicated edge
   from the altGraphX. */
{
struct spliceNode *e1 = NULL, *e2 = NULL;
struct spliceNode *tail = NULL;
unsigned int end = sp->tEnd;
/* Create the second node as we'll need that no matter what. */
e2 = initEndSpliceNode(agx, edge);
e2->sp = sp;
tail = slLastEl(sp->nodes);
if(tail == NULL) /* Start new path with both nodes. */
    {
    /* Create the first node. */
    e1 = initStartSpliceNode(agx, edge);
    e1->sp = sp;
    spliceGraphAddNode(sg, e1);
    sp->tStart = e1->tStart;

    spliceGraphAddNode(sg, e2);
    spliceNodeConnect(e1, e2);
    /* Add nodes to splicePath. */
    slAddTail(&sp->nodes, e1);
    slAddTail(&sp->nodes, e2);
    sp->nodeCount += 2;
    
    end = e2->tEnd;
    }
else if(twoDifferentEnds(tail, e2)) /* If this is a duplicate type, change tail to range. */
    {
    tail->type = ggRangeEnd;
    if(e2->tEnd < tail->tEnd)
	warn("spliceWalk::addNodeToPath() - How can node at %s:%u-%u be greater than node at %s:%u-%u?", 
	     tail->tName, tail->tStart, tail->tEnd, e2->tName, e2->tStart, e2->tEnd);
    tail->tEnd = e2->tEnd;
    tail->class = BIGNUM;
    end = e2->tEnd;
    spliceNodeFree(&e2);
    }
else /* Normal addition of a node. */
    {
    spliceGraphAddNode(sg, e2);
    spliceNodeConnect(tail, e2);
    slAddTail(&sp->nodes, e2);
    sp->nodeCount += 1;
    end = e2->tEnd;
    }
/* Expand the limits of the path as necessary. */
if(end > sp->tEnd)
    sp->tEnd = end;
if(debug) { nodeSanityCheck(sg); }
}

boolean isExon(struct spliceNode *sn1, struct spliceNode *sn2)
/** Return TRUE if edge between sn1 and sn2 is an exon, FALSE otherwise. */
{
if((sn1->type == ggHardStart || sn1->type == ggSoftStart || sn1->type == ggRangeStart)
   && (sn2->type == ggHardEnd || sn2->type == ggSoftEnd || sn2->type == ggRangeEnd))
    return TRUE;
return FALSE;
}

int splicePathCountExons(struct splicePath *sp)
/** Count the number of exons in a splicePath. */
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
/** Print out a splice path in the form of a bed file, for viewing in the browser. */    
{
int i=0;
struct spliceNode *sn = NULL;
int exonCount = 0;
fprintf(out, "%s\t%u\t%u\t%s\t1000\t%s\t%u\t%u\t0\t", 
	sp->tName, sp->tStart, sp->tEnd, sp->qName, sp->strand, sp->tStart, sp->tEnd);
/* Only print out blocks that correspond to exons. */
exonCount = splicePathCountExons(sp);
if(exonCount > 0)
    {
    fprintf(out, "%d\t", exonCount );
    for(sn=sp->nodes; sn->next != NULL; sn = sn->next)
	{
	int size = sn->next->tEnd - sn->tStart;
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
else
    { 
    struct spliceNode *tail = slLastEl(sp->nodes);
    fprintf(out, "%d\t%d\t%d\n", 1, tail->tEnd-sp->nodes->tStart, sp->nodes->tStart);
    }
}

void initSpliceGraphFromAgx(struct spliceGraph *sg, struct altGraphX *agx)
/** Fill in the simple stuff from the agx to the spliceGraph. */
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
/** Construct a list of splice paths, one for every mrnaRef in the altGraphX, and return it. */
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
    if(debug) { nodeSanityCheck(sg); }
    slAddHead(&spList, sp);
    } 

slReverse(&spList);
sg->pathCount = slCount(spList);
sg->paths =  spList;
if(debug) { nodeSanityCheck(sg); }
return sg;
}

void splicePathCreateStringRep(struct splicePath *sp)
/** Loop through the nodes in this path and create a string of exons. */
{
struct dyString *dy = newDyString(256);
struct spliceNode *sn = NULL;
if(sp->path != NULL)
    freez(&sp->path);
for(sn = sp->nodes; sn != NULL; sn = sn->next)
    {
    dyStringPrintf(dy, "%d-", sn->class);
    }
sp->path = cloneString(dy->string);
freeDyString(&dy);
}

char *getRefForNode(struct spliceGraph *sg, int id)
/** Return the mrna accession that generated the node id by
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

boolean compatibleNodes(struct spliceNode *sn1, struct spliceNode *sn2)
/** Return TRUE if these two nodes are compatible (overlapping) with eachother
   FALSE otherwise. */
{
assert(sameString(sn1->strand, sn2->strand));
if(sn1->class == BIGNUM || sn2->class == BIGNUM)
    {
    if(compatibleTypes(sn1->type, sn2->type))
	if(rangeIntersection(sn1->tStart, sn1->tEnd, sn2->tStart, sn2->tEnd) >= 0)
	    return TRUE;
    }
else if(sn1->class == sn2->class)
    return TRUE;
return FALSE;
}

boolean extendsToEnd(struct spliceNode *sn1, struct spliceNode *sn2, boolean allSn2)
/** See if the list of nodes starting at sn1 and sn2 are of identical
   classes until the end. If allSn2 then all of sn2 must be present
   in sn1 to return TRUE.*/
{
boolean extends = TRUE;
for(; sn1 != NULL && sn2 != NULL; sn1=sn1->next, sn2=sn2->next)
    {
    if(!compatibleNodes(sn1, sn2))
	extends = FALSE;
    }
if(extends && allSn2)
    extends = (sn2==NULL);
return extends;
}

void connectNodeList(struct spliceNode *sn1, struct spliceNode *sn2)
/** Connect each pair of nodes in the list togther. */
{
for(; sn1 != NULL && sn2 != NULL; sn1=sn1->next, sn2=sn2->next)
    {
    if(compatibleNodes(sn1, sn2))
	spliceNodeConnect(sn1,sn2);
    }
}

void splicePathConnectEquivalentNodes(struct splicePath *hayStack, struct splicePath *needle)
/** Try to align one path to another and connect nodes that
   overlap in alignment. */
{
struct spliceNode *sn = NULL;
struct spliceNode *mark = NULL;
boolean keepGoing = TRUE;
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
/** Return TRUE if the needle is a subpath of the haystack, FALSE otherwise. */
{
struct spliceNode *sn = NULL;
boolean subPath = FALSE;
boolean keepGoing = TRUE;
for(sn = hayStack->nodes; sn != NULL && keepGoing; sn = sn->next)
    {
    if(compatibleNodes(needle->nodes, sn))
	{
	if(extendsToEnd(sn, needle->nodes, TRUE))
	    subPath = TRUE;
	keepGoing = FALSE;
	}
    }
return subPath;
}

void spliceGraphConnectEquivalentNodes(struct spliceGraph *sg)
/** Go through and find the equivalent nodes for each path by
   aligning one path against another. */
{
struct splicePath *sp = NULL;
struct splicePath *path1 = NULL, *path2 = NULL;
char *mark = NULL;
/** Create string representations of our exon paths for comparison. */
for(sp = sg->paths; sp != NULL; sp = sp->next)
    splicePathCreateStringRep(sp);
for(path1 = sg->paths; path1 != NULL; path1 = path1->next)
    {
    for(path2 = path1->next; path2 != NULL; path2 = path2->next)
	{
	if(path1 != path2)
	    splicePathConnectEquivalentNodes(path1, path2);
	if(debug) { nodeSanityCheck(sg); }
	}
    }
}

struct splicePath *splicePathCopy(struct splicePath *orig)
/** Allocate and fill in a new splicePath that is a copy of the original. */
{
struct splicePath *sp = NULL;
struct spliceNode *sn = NULL, *snCopy = NULL;
assert(orig);
assert(orig->nodes);

sp = CloneVar(orig);
sp->next = NULL;
sp->tName = cloneString(orig->nodes->tName);
snprintf(sp->strand, sizeof(sp->strand), "%s", orig->nodes->strand);
sp->qName = NULL;
sp->tStart = sp->tEnd = orig->nodes->tStart;
sp->path = cloneString(orig->path);
sp->nodes = NULL;
for(sn=orig->nodes; sn != NULL; sn = sn->next)
    {
    snCopy = CloneVar(sn);
    snCopy->tName = cloneString(sn->tName);
    if(sn->edgeCount != 0)
	snCopy->edges = CloneArray(sn->edges, sn->edgeCount);
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
/** Look to see if the history path is a subpath of any of the 
   maximalPaths or vice versa. */
{
struct splicePath *sp = NULL, *spNext = NULL;
boolean maximal = TRUE;
slReverse(&history->nodes);
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
	splicePathFree(&sp);
	}
    }
if(maximal)
    {
    struct splicePath *copy = splicePathCopy(history);
    slAddHead(maximalPaths, copy);
    }
slReverse(&history->nodes);
}

void spliceGraphPathDfs(struct spliceGraph *sg, struct spliceNode *sn, int nodeIndex, 
			struct splicePath **maximalPaths, struct splicePath *history)
/** Depth first search of the spliceGraph starting from node at nodeIndex.
   keeps track of path taken using history, and will check history against
   maximal paths at black nodes. */
{
int i;
struct spliceNode *copy = CloneVar(sn);
boolean pop = FALSE;
struct spliceNode *notCompat = NULL;
boolean notCompatPush = FALSE;
if(debug) { nodeSanityCheck(sg); }
copy->next = NULL;
copy->tName = cloneString(sn->tName);
if(sn->edgeCount != 0)
    copy->edges = CloneArray(sn->edges, sn->edgeCount);
//copy->edgeCount = 0;
if(history->nodes == NULL || history->nodes->class != sn->class && sn->class != BIGNUM)
    {
    if(history->nodes != NULL && compatibleTypes(history->nodes->type, sn->type))
	{
	notCompatPush = TRUE;
	notCompat = slPopHead(&history->nodes);
	}
    slAddHead(&history->nodes, copy);
    history->nodeCount++;
    pop =TRUE;
    }
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
    if(notCompatPush)
	slSafeAddHead(&history->nodes, notCompat);
    }
spliceNodeFree(&copy);
sn->color = snBlack;
}

int nodeStartCmp(const void *va, const void *vb)
/** Compart to sort based on tStart of node. */
{
const struct spliceNode *a = *((struct spliceNode **)va);
const struct spliceNode *b = *((struct spliceNode **)vb);
return a->tStart - b->tStart;
}

struct splicePath *spliceGraphMaximalPaths(struct spliceGraph *sg)
/** Construct maximal paths through the spliceGraph. Maximal paths
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

int uniqueSize(struct spliceGraph *sg, struct splicePath *maximalPaths, struct splicePath *currentPath,
	       struct spliceNode *sn1, struct spliceNode *sn2)
/** Find out how much of the exon laid out by sn1 and s2 overlaps with other
   exons in the maximal paths. */
{
int size = sn2->tEnd - sn1->tStart;
int maxOverlap = 0;
struct spliceNode *sn;
struct splicePath *sp;
for(sp = maximalPaths; sp != NULL; sp = sp->next)
    {
    if(currentPath != sp)
	{
	for(sn = sp->nodes; sn->next != NULL; sn = sn->next)
	    {
	    if(isExon(sn, sn->next) && (sn->class != sn1->class && sn->next->class != sn2->class))
		{
		int overlap = rangeIntersection(sn->tStart, sn->next->tEnd, sn1->tStart, sn2->tEnd);
		if(overlap > maxOverlap)
		    maxOverlap = overlap;
		}
	    }
	}
    }
size = size - maxOverlap;
assert(size >= 0);
return size;
}

boolean isMrnaNode(struct spliceNode *mark, struct spliceGraph *sg, struct hash *mrnaHash)
/** Return TRUE if any of the paths that this node connects
   to are from an mRNA. FALSE otherwise. */
{
int i; 
struct splicePath *sp = NULL;
struct spliceNode *sn = NULL;
if(hashLookup(mrnaHash, mark->sp->qName))
    return TRUE;
for(sp = sg->paths; sp != NULL; sp = sp->next)
    {
    if(hashLookup(mrnaHash, sp->qName))
	{
	for(sn=sp->nodes; sn != NULL; sn = sn->next)
	    {
	    if(sn->class == mark->class)
		return TRUE;
	    }
	}
    }
/* for(i=0; i < mark->edgeCount; i++) */
/*     if(hashLookup(mrnaHash, sg->nodes[mark->edges[i]]->sp->qName)) */
/* 	return TRUE; */
return FALSE;
}

boolean confidentPath(struct hash *mrnaHash, struct spliceGraph *sg, 
		      struct splicePath *maximalPaths, struct splicePath *sp)
/** Return TRUE if all edges in sp were seen in 5% or more
   of the paths. */
{
double minPercent = cgiOptionalDouble("minPercent", .05);
int trumpSize = cgiOptionalInt("trumpSize", 200);
int mrnaVal = cgiOptionalInt("mrnaWeight", 20);
double minNum = 0;
boolean exonPresent = FALSE;
struct spliceNode *sn = NULL;
if(slCount(sg->paths) > 10)
    {
    minNum = minPercent * slCount(sg->paths) + 1;
    if(minNum > 4)
	minNum =4;
    }
else
    minNum = 1;

for(sn = sp->nodes; sn->next != NULL; sn = sn->next)
    {
    if(isExon(sn, sn->next))
	{
	int count1 = sn->edgeCount;
	int count2 = sn->next->edgeCount;
	int size = uniqueSize(sg, maximalPaths, sp, sn, sn->next);
	exonPresent = TRUE;
	if(sn->next->next == NULL)
	    count2++; 
	if(isMrnaNode(sn, sg, mrnaHash))
	    count1 += mrnaVal;
	if(isMrnaNode(sn->next, sg, mrnaHash))
	    count2 += mrnaVal;
	if(sn->type == ggHardEnd || sn->type == ggHardStart)
	    count1 += 3;
	if(sn->next->type == ggHardEnd || sn->next->type == ggHardStart)
	    count2 += 3;
	if((count1 < minNum || count2 <minNum) && size < trumpSize  )
	    return FALSE;
	}
    }
return exonPresent;
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
sqlSafef(query, sizeof(query), "select acc from gbCdnaInfo where type = 2");
hashRow0(conn, query, hash);
}

void spliceWalk(char *inFile, char *bedOut)
/** Starting routine. */
{
struct altGraphX *agx = NULL, *agxList = NULL;
struct spliceGraph *sg = NULL, *sgList = NULL;
FILE *f = NULL;
FILE *spliceSites = NULL;
FILE *cleanEsts = NULL;
FILE *rejects = NULL;
boolean diagnostics = cgiBoolean("diagnostics");
boolean mrnaFilter = !(cgiBoolean("mrnaFilterOff"));
struct hash *mrnaHash = newHash(10);
int index = 0;
int count = 0;
struct splicePath *sp=NULL, *spList = NULL;
agxList = altGraphXLoadAll(inFile);
f = mustOpen(bedOut, "w");
if(mrnaFilter)
    addMrnaAccsToHash(mrnaHash);

if(diagnostics) 
    {
    spliceSites = mustOpen("spliceSites.bed", "w");
    cleanEsts = mustOpen("cleanEsts.bed", "w");
    rejects = mustOpen("rejects.bed", "w");
    fprintf(rejects, "track name=rejects description=\"spliceWalk rejects\" color=178,44,26\n");
    fprintf(spliceSites, "track name=spliceSites description=\"spliceWalk splice sites.\"\n");
    fprintf(cleanEsts, "track name=cleanEsts description=\"spliceWalk cleaned ESTs\"\n");
    }
fprintf(f, "track name=spliceWalk description=\"spliceWalk Final Picks\" color=23,117,15\n");
warn("Creating splice graphs.");
for(agx = agxList; agx != NULL; agx = agx->next)
    {
    if(count++ % 100 == 0)
	putTic();
    if(diagnostics)
	writeOutNodes(agx, spliceSites);
    sg = spliceGraphFromAgx(agx);
    currentGraph = sg;
    if(diagnostics)
	for(sp = sg->paths; sp != NULL; sp = sp->next)
	    splicePathBedOut(sp,cleanEsts);
    spliceGraphConnectEquivalentNodes(sg);
    if(debug) { nodeSanityCheck(sg); }
    spList = spliceGraphMaximalPaths(sg);
    for(sp = spList; sp != NULL; sp = sp->next)
	{
	char buff[256];
	snprintf(buff, sizeof(buff), "%d", index++);
	sp->qName = cloneString(buff);
	splicePathCreateStringRep(sp);
	if(confidentPath(mrnaHash, sg, spList, sp))
	    {
	    if(debug)
		{
		printf("%s\n", sp->path);
		splicePathBedOut(sp, stdout);
		}
	    splicePathBedOut(sp,f);
	    }
	else if(diagnostics)
	    splicePathBedOut(sp, rejects);
	}
    splicePathFree(&spList);
    spliceGraphFree(&sg);
    currentGraph = NULL;
    }
warn("\tDone.");
if(diagnostics)
    {
    carefulClose(&spliceSites);
    carefulClose(&cleanEsts);
    carefulClose(&rejects);
    }
carefulClose(&f);
spliceGraphFreeList(&sgList);
altGraphXFreeList(&agxList);
}


int main(int argc, char *argv[])
{
if(argc < 3)
    usage();
cgiSpoof(&argc, argv);
debug = cgiBoolean("debug");
spliceWalk(argv[1], argv[2]);
return 0;
}
