/* altPaths - Program to examine altGraphX graphs and discover
   alternative splicing events and their paths. */
#include "common.h"
#include "altGraphX.h"
#include "splice.h"
#include "options.h"
#include "bed.h"
#include "obscure.h"
#include "dystring.h"

static char const rcsid[] = "$Id: altPaths.c,v 1.1 2004/07/08 01:05:35 sugnet Exp $";

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"dumpDistMatrix", OPTION_STRING},
    {"dumpPathBeds", OPTION_STRING},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "Dump out the distance matrix to this file (for debugging and testing)",
    "Dump out the paths as a bed file (for debugging and testing)",
};

FILE *distMatrixFile = NULL; /* File to dump out distance matrixes to
			      * for debugging and testing. */

FILE *pathBedFile = NULL;    /* File to dump out paths in bed form for
				debugging and testing. */

void usage() 
/* How to use the program. */
{
int i = 0;
fprintf(stderr,
	"altPaths - Examin altGraphX graphs and discover alternative\n"
	"splicing events and their paths.\n"
	"usage:\n"
	"   altPaths fileIn.agx fileOut.splice\n"
	"where options are:\n");
for(i=0; i<ArraySize(optionSpecs) -1; i++)
    fprintf(stderr, "   -%s -- %s\n", optionSpecs[i].name, optionDescripts[i]);
errAbort("");
}

boolean connected(struct altGraphX *agx, bool **em, int v1, int v2, 
		  int source, int sink)
/* Return whether or not two edges are connected in 
   the graph. */
{
/* source doesn't have any incoming edges, sink doesn't
   have any outgoing ones. */
if(v2 == source || v1 == sink) 
    return FALSE;
/* Disallow source->sink connection. */
else if(v1 == source && v2 == sink) 
    return FALSE;
/* If non-source,non-sink in edge matrix return TRUE. */
else if(v1 != source && v2 != sink && em[v1][v2])
    return TRUE;
/* All soft starts are connected to source. */
else if(v1 == source && agx->vTypes[v2] == ggSoftStart)
    return TRUE;
/* All soft ends are connected to sink. */
else if(v2 == sink && agx->vTypes[v1] == ggSoftEnd)
    return TRUE;
return FALSE;
}

void writeDistMatrix(FILE *out, int **D, int source, int sink, char *id)
/* Write the distannce matrix to a file. Used only for debugging and
 testing. */
{
int i = 0, j = 0;

/* Print id with a fasta like header. */
fprintf(out, ">%s\n", id);

/* print header. */
fprintf(out, "    ");
for(i = source; i <= sink; i++) 
    {
    if(i == source || i == sink)
	fprintf(out, "%4s", "s");
    else
	fprintf(out, "%4d", i);
    }
fprintf(out, "\n");

/* Nifty formatting. */
fprintf(out, "   +");
for(i = source; i <= sink; i++) 
    fprintf(out, "----");
fprintf(out, "\n");

/* Print the matrix. */
for(i = source; i <= sink; i++) 
    {
    /* Print vertex number. */
    if(i == source || i == sink)
	fprintf(out, "%3s|", "s");
    else
	fprintf(out, "%3d|", i);
    for(j = source; j <= sink; j++)
	{
	if(D[i][j] == BIGNUM)
	    fprintf(out, "%4s", "-");
	else
	    fprintf(out, "%4d", D[i][j]);
	}
    fprintf(out, "\n");
    }
}

int **createDistanceMatrix(struct altGraphX *agx, bool **em, int source, int sink)
/* Calculate the shortest distance between all pairs of vertices. */
{
int **D = NULL;
int vC = agx->vertexCount + 2;
int i = 0, j = 0, k = 0;

/* Allocate and initialize D matrix. */
AllocArray(D, vC);
for(i = 0; i < vC; i++)
    {
    AllocArray(D[i], vC);
    for(j = 0; j < vC; j++)
	{
	/* Initialize to zero for self edge,
	   one if edges are connected,
	   BIGNUM (infinity) otherwise. */
	if(i == j)
	    D[i][j] = 0;
	else if(connected(agx, em, i-1, j-1, source, sink))
	    D[i][j] = 1;
	else
	    D[i][j] = BIGNUM;
	}

    }

/* Increment pointer so -1 (source) is a valid location. */
for(i = 0; i < vC; i++)
    D[i]++;

/* Increment pointer so -1 (source) is a valid location. */
D++;

/* Fill in the shortest paths between vertexes. */
for(k = source; k <= sink; k++)
    {
    for(i = source; i <= sink; i++)
	{
	for(j = source; j <= sink; j++)
	    {
	    D[i][j] = min( D[i][j], (D[i][k] +  D[k][j]) );
	    }
	}
    }
if(distMatrixFile != NULL)
    writeDistMatrix(distMatrixFile, D, source, sink, agx->name);
return D;
}

void fillInAltStarts(int **distance, int vertIx, int source, int sink, 
		     int **altStarts, int *altCount)
/* Fill in all of the vertexes that vertIx connect to. */
{
int i = 0;
int outCount = 0;
/* First count the possible alts. */
for(i = source; i <= sink; i++)
    {
    if(distance[vertIx][i] == 1)
	{
	outCount++;
	}
    }
AllocArray((*altStarts), outCount);
*altCount = outCount;

/* Now fill in the array. */
outCount = 0;
for(i = source; i <= sink; i++)
    {
    if(distance[vertIx][i] == 1)
	{
	(*altStarts)[outCount++] = i;
	}
    }
}

int vertConnect(int **distance, int source, int sink, int vertIx,
		int *verts, int vertCount)
/* Return the number of vertices in verts that are
   connected to vertIx. */
{
int i = 0;
int connections = 0;
for(i = 0; i < vertCount; i++)
    {
    if(distance[verts[i]][vertIx] != BIGNUM)
	connections++;
    }
return connections;
}

boolean altSplicedVertex(int **distance, int vertIx, int source, int sink)
/* Is this vertex alt-spliced? Formally, does this vertex connect
   to more than one other vertex with distance == 1? */
{
int i = 0;
int outCount = 0;
for(i = source; i <= sink; i++)
    {
    if(distance[vertIx][i] == 1)
	{
	if(++outCount >= 2)
	    return TRUE;
	}
    }
return FALSE;
}	

void pathAddHead(int vert, struct path *path)
/* Add vert to the beginning of a path. */
{
int i = 0;
if(path->maxVCount <= path->vCount + 1)
    {
    ExpandArray(path->vertices, path->maxVCount, path->maxVCount*2);
    path->maxVCount = 2*path->maxVCount;
    }
for(i = path->vCount; i > 0; i--)
    {
    path->vertices[i] = path->vertices[i-1];
    }
path->vertices[0] = vert;
path->vCount++;
}
    
struct path *pathsBetweenVerts(int **distance, int source, int sink, 
				struct altGraphX *ag, bool **em, int startIx, int endIx)
/* Enumerate all the paths between startIx and endIx. */
{
struct path *pathList = NULL;
struct path *path = NULL;
int i = 0, j = 0;

/* This amounts to a breadth first search of paths
   between startIx and endIx. */
for(i = source; i <= sink; i++)
    {
    if(distance[startIx][i] == 1)
	{
	if(i == endIx)
	    {
	    /* We've found the end vertex. Initialize
	       a path and add it to the list to be returned. */
	    AllocVar(path);
	    path->maxVCount = endIx - startIx + 1;
	    AllocArray(path->vertices, path->maxVCount);
	    path->vertices[path->vCount++] = startIx;
	    path->vertices[path->vCount++] = i;
	    path->upV = -1;
	    path->downV = -1;
	    slAddHead(&pathList, path);
	    }
	else
	    {
	    /* We need to recurse and find all the paths out
	       of this vertex that lead to the endIx eventually. */
	    struct path *sub = NULL, *subNext = NULL;
	    struct path *subPaths = pathsBetweenVerts(distance, source, sink, ag,
						      em, i, endIx);
	    for(sub = subPaths; sub != NULL; sub = subNext)
		{
		subNext = sub->next;
		pathAddHead(startIx, sub);
		slAddHead(&pathList, sub);
		}
	    }
	}
    }
return pathList;
}

int pathBpCountCmp(const void *va, const void *vb)
/* Compare to sort based on size in base pairs. */
{
const struct path *a = *((struct path **)va);
const struct path *b = *((struct path **)vb);
return a->bpCount - b->bpCount;
}

struct bed *bedForPath(struct path *path, struct splice *splice,
		       struct altGraphX *ag, int source, int sink, boolean spoofEnds)
/* Construct a bed for the path. If spoofEnds is TRUE,
   ensure that there is at least a 1bp exon at splice
   sites. */
{
struct bed *bed = NULL;
int vertIx = 0;
int *verts = path->vertices;
int *vPos = ag->vPositions;
int i = 0;
struct dyString *buff = newDyString(256);

AllocVar(bed);
bed->chrom = cloneString(ag->tName);
bed->chromStart = BIGNUM;
bed->chromEnd = 0;
safef(bed->strand, sizeof(bed->strand), "%s", ag->strand);
bed->score = splice->type;
AllocArray(bed->chromStarts, path->vCount);
AllocArray(bed->blockSizes, path->vCount);

/* If necessary tack on a fake exon. */
if(spoofEnds && altGraphXEdgeVertexType(ag, verts[vertIx], verts[vertIx+1]) != ggExon)
    {
    bed->blockSizes[bed->blockCount] = 1;
    bed->chromStarts[bed->blockCount] = vPos[verts[vertIx]] - 1;
    bed->chromStart = bed->thickStart = min(bed->chromStart, vPos[verts[vertIx]] - 1 );
    bed->chromEnd = bed->thickEnd = max(bed->chromEnd, vPos[verts[vertIx+1]]);
    bed->blockCount++;
    }

/* For each edge that is an exon count up the base pairs. */
for(vertIx = 0; vertIx < path->vCount - 1; vertIx++)
    {
    if(verts[vertIx] != source && verts[vertIx] != sink)
	{
	/* If exon add up the base pairs. */
	if(altGraphXEdgeVertexType(ag, verts[vertIx], verts[vertIx+1]) == ggExon)
	    {
	    bed->blockSizes[bed->blockCount] = vPos[verts[vertIx+1]] - vPos[verts[vertIx]];
	    bed->chromStarts[bed->blockCount] = vPos[verts[vertIx]];
	    bed->chromStart = bed->thickStart = min(bed->chromStart, vPos[verts[vertIx]]);
	    bed->chromEnd = bed->thickEnd = max(bed->chromEnd, vPos[verts[vertIx+1]]);
	    bed->blockCount++;
	    }
	}
    }

/* if spoofing ends tack on a 1bp exon as necessary. */
vertIx = path->vCount - 2;
if(spoofEnds && altGraphXEdgeVertexType(ag, verts[vertIx], verts[vertIx+1]) != ggExon)
    {
    bed->blockSizes[bed->blockCount] = 1;
    bed->chromStarts[bed->blockCount] = vPos[verts[vertIx+1]] - 1;
    bed->chromStart = bed->thickStart = min(bed->chromStart, vPos[verts[vertIx]] - 1);
    bed->chromEnd = bed->thickEnd = max(bed->chromEnd, vPos[verts[vertIx+1]]);
    bed->blockCount++;
    }

/* Fix up the name and adjust the chromStarts. */
dyStringPrintf(buff, "%s.%d.", splice->name, slIxFromElement(splice->paths, path));
for(i = 0; i < path->vCount; i++)
    dyStringPrintf(buff, "%d,", path->vertices[i]);
bed->name = cloneString(buff->string);
for(i = 0; i < bed->blockCount; i++)
    bed->chromStarts[i] -= bed->chromStart;

/* If we don't have any blocks, quit now. */
if(bed->blockCount == 0)
    bedFree(&bed);
dyStringFree(&buff);
return bed;
}

int calcBpLength(struct path *path, struct splice *splice,
		 struct altGraphX *ag, int source, int sink)
/* Calculate the length in base pairs of a given 
   path. */
{
int vertIx = 0;
int *verts = path->vertices;
int *vPos = ag->vPositions;
int bpCount = 0;
 
assert(path->vCount > 1);

/* For each edge that is an exon count up the base pairs. */
for(vertIx = 0; vertIx < path->vCount - 1; vertIx++)
    {
    if(verts[vertIx] != source && verts[vertIx] != sink)
	{
	/* If exon add up the base pairs. */
	if(altGraphXEdgeVertexType(ag, verts[vertIx], verts[vertIx+1]) == ggExon)
	    bpCount += vPos[verts[vertIx+1]] - vPos[verts[vertIx]];
	}
    }

/* If writing beds finish off the bed and write it out. */
if(pathBedFile != NULL)
    {
    struct bed *bed = bedForPath(path, splice, ag, source, sink, TRUE);
    if(bed != NULL)
	bedTabOutN(bed, 12, pathBedFile);
    bedFree(&bed);
    }
return bpCount;
}
	    
void printIntArray(int *array, int count)
/* For debugging. */
{
int i;
for(i = 0; i < count; i++)
    printf("%d,", array[i]);
printf("\n");
}

int pathFindUpstreamVertex(struct path *path, struct altGraphX *ag,
			   int **distance, int source, int sink)
/* Return the path that is the closest vertex connected to the
   start of the path. */
{
int i = 0;
for(i = path->vertices[0]; i >= source; i--)
    {
    if(distance[i][path->vertices[0]] == 1)
	return i;
    }
return -1;
}

int pathFindDownstreamVertex(struct path *path, struct altGraphX *ag,
			     int **distance, int source, int sink)
/* Return the path that is the closest vertex connected to the
   end of the path. */
{
int i = 0;
int endPath = path->vertices[path->vCount-1];
for(i = path->vertices[endPath] + 1; i <= sink; i++)
    {
    if(distance[path->vertices[endPath]][i] == 1)
	return i;
    }
return -1;
}

void simplifyAndOrderPaths(struct splice *splice, int **distance,
			   struct altGraphX *ag, int source, int sink)
/* Loop through all of the paths and set their
   array size to the minimum possible size. */
{
struct path *path = NULL;
for(path = splice->paths; path != NULL; path = path->next)
    {
    int *tmpPath = CloneArray(path->vertices, path->vCount);
    freez(&path->vertices);
    path->vertices = tmpPath;
    path->maxVCount = path->vCount;
    path->upV = pathFindUpstreamVertex(path, ag, distance, source, sink);
    path->downV = pathFindDownstreamVertex(path, ag, distance, source, sink);
    path->bpCount = calcBpLength(path, splice, ag, source, sink);
    }
slSort(&splice->paths, pathBpCountCmp);
}

boolean isCassettePath(struct splice *splice, struct altGraphX *ag, bool **em,
		       int source, int sink)
/* Return TRUE if splice is an cassette exon, FALSE otherwise. */
{
int altStart = 0, altEnd = 0, startV = 0, endV = 0;
struct path *longPath = splice->paths->next;
int endVert = longPath->vCount-1;
int startVert = 0;
int *verts = longPath->vertices;
boolean cassette = FALSE;
/* Cassettes have 4 vertices in their path.
   Cassettes don't start with the source or end with the sink. */
if(endVert != 3 || verts[startVert] == source || verts[endVert] == sink)
    return FALSE;
cassette = agxIsCassette(ag, em, verts[0], verts[1], verts[3],
			 &altStart, &altEnd, &startV, &endV);
return cassette;
}

boolean isRetIntPath(struct splice *splice, struct altGraphX *ag, bool **em,
		       int source, int sink)
/* Return TRUE if splice is an retInt exon, FALSE otherwise. */
{
int altStart = 0, altEnd = 0;
/* short and long refer to bpCount. */
struct path *shortPath = splice->paths; 
struct path *longPath = splice->paths->next;
int *shortVerts = shortPath->vertices;
int *longVerts = longPath->vertices;
boolean retInt = FALSE;
/* RetInts have 2 vertices in their long path and 4 in the short path.
   RetInts don't start with the source or end with the sink. */
if(longPath->vCount != 2 || shortPath->vCount != 4 ||
   shortVerts[0] == source || shortVerts[3] == sink)
    return FALSE;
assert(shortVerts[0] == longVerts[0] && shortVerts[3] == longVerts[1]);
retInt = agxIsRetainIntron(ag, em, shortVerts[0], shortVerts[1], longVerts[1],
			   &altStart, &altEnd);
return retInt;
}

boolean isAlt5Path(struct splice *splice, struct altGraphX *ag, bool **em,
		   int source, int sink)
/* Return TRUE if splice is an alt5 exon, FALSE otherwise. */
{
int altStart = 0, altEnd = 0, startV = 0, endV = 0;
/* short and long refer to bpCount. */
struct path *shortPath = splice->paths; 
struct path *longPath = splice->paths->next;
int *shortVerts = shortPath->vertices;
int *longVerts = longPath->vertices;
boolean alt5 = FALSE;
/* Alt5s have 3 vertices in both paths.
   Alt5s don't start with the source or end with the sink. */
if( shortPath->vCount != 3 || longPath->vCount != 3 || 
    shortVerts[0] == source || shortVerts[2] == sink ||
    longVerts[0] == source || longVerts[2] == sink)
    return FALSE;
assert(shortVerts[0] == longVerts[0] && shortVerts[2] == longVerts[2]);
alt5 = agxIsAlt5Prime(ag, em, shortVerts[0], shortVerts[1], longVerts[1],
		      &altStart, &altEnd, &startV, &endV);
return alt5;
}

boolean isAlt3Path(struct splice *splice, struct altGraphX *ag, bool **em,
		   int source, int sink)
/* Return TRUE if splice is an alt3 exon, FALSE otherwise. */
{
int altStart = 0, altEnd = 0, startV = 0, endV = 0;
/* short and long refer to bpCount. */
struct path *shortPath = splice->paths->next;
struct path *longPath = splice->paths; 
int *shortVerts = shortPath->vertices;
int *longVerts = longPath->vertices;
boolean alt3 = FALSE;
/* Alt3s have 3 vertices in both paths.
   Alt3s don't start with the source or end with the sink. */
if( shortPath->vCount != 3 || longPath->vCount != 3 || 
    shortVerts[0] == source || shortVerts[2] == sink ||
    longVerts[0] == source || longVerts[2] == sink)
    return FALSE;
assert(shortVerts[0] == longVerts[0] && shortVerts[2] == longVerts[2]);
alt3 = agxIsAlt3Prime(ag, em, shortVerts[0], shortVerts[1], longVerts[1],
		      &altStart, &altEnd, &startV, &endV);
return alt3;
}

boolean isMutallyExclusive(struct splice *splice, struct altGraphX *ag,
			   bool **em, int source, int sink)
/* Return TRUE if the paths in splice are mutally exclusive, FALSE
   otherwise. */
{
bool *seen = NULL;
int i = 0;
struct path *path = NULL, *pathNext = NULL;
boolean mutExculsive = TRUE;
AllocArray(seen, ag->vertexCount);
/* Loop through each path recording vertices that are
   seen. If the same one is seen twice then mutExculsive = FALSE. */
for(path = splice->paths; path != NULL; path = pathNext)
    {
    pathNext = path->next;
    for(i = 1; i < path->vCount -1; i++)
	{
	if(seen[path->vertices[i]] == TRUE) 
	    {
	    mutExculsive = FALSE;
	    pathNext = NULL;
	    }
	else
	    {
	    seen[path->vertices[i]] = TRUE;
	    }
	}
    }
freez(&seen);
return mutExculsive;
}

boolean isAltTxStart(struct splice *splice, struct altGraphX *ag, bool **em,
		     int source, int sink)
/* Return TRUE if starting vertex is source and paths
   are mutally exclusive. */
{
if(splice->paths->vertices[0] == source &&
   isMutallyExclusive(splice, ag, em, source, sink))
    return TRUE;
return FALSE;
}

boolean isAltTxEnd(struct splice *splice, struct altGraphX *ag, bool **em,
		     int source, int sink)
/* Return TRUE if ending vertex is sink and paths
   are mutally exclusive. */
{
struct path *path = splice->paths;
int endIx = path->vCount - 1;
if(path->vertices[endIx] == sink &&
   isMutallyExclusive(splice, ag, em, source, sink))
    return TRUE;
return FALSE;
}

int typeForSplice(struct splice *splice, struct altGraphX *ag, bool **em,
		  int source, int sink)
/* Determine the type of splicing seen in this splice event. */
{
int type = altOther;
if(isAltTxStart(splice, ag, em, source, sink))
    type = alt5PrimeSoft;
else if(isAltTxEnd(splice, ag, em, source, sink))
    type = alt3PrimeSoft;
else if(splice->pathCount > 2)
    type = altOther;
else if(isCassettePath(splice, ag, em, source, sink))
    type = altCassette;
else if(isAlt5Path(splice, ag, em, source, sink)) 
    type = alt5Prime; 
else if(isAlt3Path(splice, ag, em, source, sink))
    type = alt3Prime;
else if(isRetIntPath(splice, ag, em, source, sink))
    type = altRetInt;
else if(isMutallyExclusive(splice, ag, em, source, sink))
    type = altMutExclusive;

/* Fix the strand. */
if(ag->strand[0] == '-')
    {
    if(type == alt5Prime) type = alt3Prime;
    else if(type == alt3Prime) type = alt5Prime;
    else if(type == alt5PrimeSoft) type = alt3PrimeSoft;
    else if(type == alt3PrimeSoft) type = alt5PrimeSoft;
    }
return type;
}

void findAltSplicesFromVertice(struct altGraphX *ag, int source, int sink, bool **em, 
			       int **distance, struct splice **spliceList, 
			       int vertIx, int *endIx)
/* Create an alternative splicing event from this vertex and
   the position where it ends. This is a recursive function that
   will call itself again if an alt-spliced vertex is encountered
   while looking for an end to this splicing event. */
{
struct splice *splice = NULL;
int *altStarts = NULL;
int altCount = 0;
int i = 0;
char buff[256];

/* Allocate and initialize the splice. */
AllocVar(splice);
splice->agxId = ag->id;
safef(buff, sizeof(buff), "%s.%d", ag->name, vertIx);
splice->name = cloneString(buff);
splice->agx = ag;
slAddHead(spliceList, splice);

/* Find all of the splice sites that vertIx connects
   to for evaluation. */
fillInAltStarts(distance, vertIx, source, sink, &altStarts, &altCount);
assert(altCount);

/* Loop through vertexes in order to find those the one
   that terminates the alternative splicing event. If another
   alternative splicing event is found recurse into it. */
for(i = vertIx + 1; i <= sink; ) 
    {
    int connections = vertConnect(distance, source, sink, i, altStarts, altCount);
    if(connections == altCount)
	{
	/* Found end of altsplice event. Enumerate all the paths
	   between it and end vertex. */
	*endIx = i;
	splice->paths = pathsBetweenVerts(distance, source, sink, ag,
					  em, vertIx, i);
	splice->pathCount = slCount(splice->paths);
	simplifyAndOrderPaths(splice, distance, ag, source, sink);
	splice->type = typeForSplice(splice, ag, em, source, sink);
	break;
	}
    else if(connections > 0)
	{
	/* Check to see if this vertex is alt spliced and recurse. */
	if(altSplicedVertex(distance, i, source, sink))
	    {
	    int newEnd = i;
	    findAltSplicesFromVertice(ag, source, sink, em, distance, spliceList, i, &newEnd);
	    assert(newEnd > i);
	    i = newEnd;
	    }
	else /* Otherwise move on to the next vertex. */
	    i++;
	}
    else /* No connections, try the next vertex. */
	i++;
    }
freez(&altStarts);
}

void freeDistanceMatrix(int ***pD, int source, int sink)
/* Free the memory associated with a distance matrix
   and set the pointer to NULL. */
{
int i = 0;
int **D = *pD;
/* Have to decrement the pointers as we offset things
   so that the source (-1) is a valid array position. */
for(i = source; i <= sink; i++)
    {
    D[i]--;
    freez(&D[i]);
    }
D--;
freez(&D);
*pD = NULL;
}

void doAltPathsAnalysis(struct altGraphX *agx, FILE *spliceOut)
/* Examine each altGraphX record for splicing events. */
{
bool **em = altGraphXCreateEdgeMatrix(agx);
int source = -1, sink = agx->vertexCount;
int vC = agx->vertexCount + 2;
int *vPos = agx->vPositions;
int **distance = createDistanceMatrix(agx, em, source, sink);
int vertIx = 0;
struct splice *splice = NULL, *spliceList = NULL;
for(vertIx = source; vertIx <= sink; )
    {
    /* If a vertex is alt spliced, find the end of the alt-splicing
       event and continue from there. */
    if(altSplicedVertex(distance, vertIx, source, sink))
	{
	int nextVertIx = vertIx;
	findAltSplicesFromVertice(agx, source, sink, em, distance, &spliceList, vertIx, &nextVertIx);
	assert(vertIx < nextVertIx);
	vertIx = nextVertIx;
	}
    else /* Try the next vertex. */
	{
	vertIx++;
	}
    }

/* Output the alternative splicing events. */
for(splice = spliceList; splice != NULL; splice = splice->next)
    {
    spliceTabOut(splice, spliceOut);
    }

/* Cleanup. */
spliceFreeList(&spliceList);
freeDistanceMatrix(&distance, source, sink);
altGraphXFreeEdgeMatrix(&em, agx->vertexCount);
}

void altPaths(char *fileIn, char *fileOut)
/* Loop through each graph and output alternative splicing paths. */
{
struct altGraphX *agx = NULL, *agxList = NULL;
FILE *out = NULL;
agxList = altGraphXLoadAll(fileIn);
out = mustOpen(fileOut, "w");
for(agx = agxList; agx != NULL; agx = agx->next)
    {
    doAltPathsAnalysis(agx, out);
    }
altGraphXFreeList(&agxList);
}

void initDumpDistMatrix() 
/* Setup a file handle to dump distance matrixes to. */
{
char *dumpName = optionVal("dumpDistMatrix", NULL);
assert(dumpName);
distMatrixFile = mustOpen(dumpName, "w");
}

void initPathBeds()
/* Set up a file handle to dump paths in bed form to. */
{
char *fileName = optionVal("dumpPathBeds", NULL);
assert(fileName);
pathBedFile = mustOpen(fileName, "w");
}

int main(int argc, char *argv[])
/* Everybody's favorite function. */
{
optionInit(&argc, argv, optionSpecs);
if(optionExists("help"))
    usage();
if(argc != 3)
    usage();
if(optionExists("dumpDistMatrix"))
    initDumpDistMatrix();
if(optionExists("dumpPathBeds"))
    initPathBeds();
altPaths(argv[1], argv[2]);
carefulClose(&distMatrixFile);
carefulClose(&pathBedFile);
return 0;
}

	 








   
