/* altPaths - Program to examine altGraphX graphs and discover
   alternative splicing events and their paths. */
#include "common.h"
#include "altGraphX.h"
#include "splice.h"
#include "options.h"
#include "bed.h"
#include "obscure.h"
#include "dystring.h"

static char const rcsid[] = "$Id: altPaths.c,v 1.6 2004/07/20 04:21:28 sugnet Exp $";

static struct optionSpec optionSpecs[] = 
/* Our acceptable options to be called with. */
{
    {"help", OPTION_BOOLEAN},
    {"saveGraphs", OPTION_STRING},
    {"pathBeds", OPTION_STRING},
    {"dumpDistMatrix", OPTION_STRING},
    {"oldClassification", OPTION_BOOLEAN},
    {NULL, 0}
};

static char *optionDescripts[] = 
/* Description of our options for usage summary. */
{
    "Display this message.",
    "Save the sub-graphs records in this file.",
    "Print all of the paths to a file in bed form.",
    "Dump out the distance matrix to this file (for debugging and testing)",
    "Use the older style of classifying events (like altAnalysis).",
};

FILE *distMatrixFile = NULL; /* File to dump out distance matrixes to
			      * for debugging and testing. */

FILE *pathBedFile = NULL;    /* File to dump out paths in bed form for
				debugging and testing. */
boolean oldClassification = FALSE; /* Use altAnalysis style of classification. */

/* Keep track of how many of each event are occuring. */
static int alt5Flipped = 0;
static int alt3Flipped = 0;
static int alt5PrimeCount = 0;
static int alt3PrimeCount = 0;
static int alt3Prime3bpCount = 0;
static int altCassetteCount = 0;
static int altRetIntCount = 0;
static int altOtherCount = 0;
static int alt3PrimeSoftCount = 0;
static int alt5PrimeSoftCount = 0;
static int altMutExclusiveCount = 0;
static int altControlCount = 0;

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

void logSpliceType(enum altSpliceType type, int size)
/* Log the different types of splicing. */
{
switch (type) 
    {
    case alt5Prime:
	alt5PrimeCount++;
	break;
    case alt3Prime: 
	alt3PrimeCount++;
	if(size == 3)
	    alt3Prime3bpCount++;
	break;
    case altCassette:
	altCassetteCount++;
	break;
    case altRetInt:
	altRetIntCount++;
	break;
    case altOther:
	altOtherCount++;
	break;
    case alt5PrimeSoft:
	alt5PrimeSoftCount++;
	break;
    case alt3PrimeSoft:
	alt3PrimeSoftCount++;
	break;
    case altMutExclusive:
	altMutExclusiveCount++;
	break;
    case altControl:
	altControlCount++;
	break;
    default:
	errAbort("logSpliceType() - Don't recognize type %d", type);
    }
}

void printSpliceTypeInfo(int totalLoci, int altSpliceLoci, int totalSplices)
{
fprintf(stderr, "alt 5' Count:\t%d (%d on '-' strand)\n", alt5PrimeCount, alt3Flipped);
fprintf(stderr, "alt 3' Count:\t%d (%d on '-' strand, %d are 3bp)\n", alt3PrimeCount, alt5Flipped, alt3Prime3bpCount);
fprintf(stderr, "alt Cass Count:\t%d\n", altCassetteCount);
fprintf(stderr, "alt Ret. Int. Count:\t%d\n", altRetIntCount);
fprintf(stderr, "alt Mutual Exclusive Count:\t%d\n", altMutExclusiveCount);
fprintf(stderr, "alt Txn Start Count:\t%d\n", alt5PrimeSoftCount);
fprintf(stderr, "alt Txn End Count:\t%d\n", alt3PrimeSoftCount);
fprintf(stderr, "alt Other Count:\t%d\n", altOtherCount);
fprintf(stderr, "%d control paths.\n", altControlCount);
fprintf(stderr, "%d alt spliced sites out of %d total loci with %.2f alt splices per alt spliced loci\n",
	totalSplices, totalLoci, (float)totalSplices/altSpliceLoci);
}

boolean connected(struct altGraphX *agx, bool **em, int *adjStartCounts, int *adjEndCounts, 
		  int v1, int v2, int source, int sink)
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
else if(v1 == source && 
	(agx->vTypes[v2] == ggSoftStart || adjEndCounts[v2] == 0))
    return TRUE;
/* All soft ends are connected to sink. */
else if(v2 == sink && 
	(agx->vTypes[v1] == ggSoftEnd || adjStartCounts[v1] == 0))
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
int **adjList = NULL;   /* Adjacency list. */
int *adjStartCounts = NULL;  /* Number of vertices in the adj list. */
int *adjEndCounts = NULL;  /* Number of vertices in the adj list. */

/* Allocate some memory. */
AllocArray(adjStartCounts, vC);
AllocArray(adjEndCounts, vC);
for(i = 0; i < agx->edgeCount; i++)
    {
    adjStartCounts[agx->edgeStarts[i]]++;
    adjEndCounts[agx->edgeEnds[i]]++;
    }

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
	else if(connected(agx, em, adjStartCounts, adjEndCounts, 
			  i-1, j-1, source, sink))
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

/* Cleanup some memory. */
freez(&adjStartCounts);
freez(&adjEndCounts);
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

void pathAddHead(struct path *path, int vert)
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

void pathAddTail(struct path *path, int vert)
/* Add vert to the end of a path. */
{
int i = 0;
if(path->maxVCount <= path->vCount + 1)
    {
    ExpandArray(path->vertices, path->maxVCount, path->maxVCount*2);
    path->maxVCount = 2*path->maxVCount;
    }
path->vertices[path->vCount++] = vert;
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
	    path->vertices[path->vCount++] = i;
	    path->vertices[path->vCount++] = startIx;
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
		/* Add to tail for now. Later reverse the 
		   order. */
		pathAddTail(sub, startIx); 
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

int spliceCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct splice *a = *((struct splice **)va);
const struct splice *b = *((struct splice **)vb);
int dif;
dif = strcmp(a->tName, b->tName);
if (dif == 0)
    dif = a->tStart - b->tStart;
return dif;
}

struct bed *bedForPath(struct path *path, struct splice *splice, int pathIx,
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
if(spoofEnds && verts[vertIx] != source && verts[vertIx+1] != sink &&
   altGraphXEdgeVertexType(ag, verts[vertIx], verts[vertIx+1]) != ggExon)
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
if(spoofEnds && verts[vertIx] != source && verts[vertIx+1] != sink &&
   altGraphXEdgeVertexType(ag, verts[vertIx], verts[vertIx+1]) != ggExon)
    {
    bed->blockSizes[bed->blockCount] = 1;
    bed->chromStarts[bed->blockCount] = vPos[verts[vertIx+1]] - 1;
    bed->chromStart = bed->thickStart = min(bed->chromStart, vPos[verts[vertIx]] - 1);
    bed->chromEnd = bed->thickEnd = max(bed->chromEnd, vPos[verts[vertIx+1]]);
    bed->blockCount++;
    }

/* Fix up the name and adjust the chromStarts. */
dyStringPrintf(buff, "%s.%d.", splice->name, pathIx);
for(i = 0; i < path->vCount; i++)
    {
    if(path->vertices[i] != sink && path->vertices[i] != source)
	dyStringPrintf(buff, "%d,", path->vertices[i]);
    }
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
int endIx = path->vCount-1;
for(i = path->vertices[endIx] + 1; i <= sink; i++)
    {
    if(distance[path->vertices[endIx]][i] == 1)
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
int *vPos = ag->vPositions;
for(path = splice->paths; path != NULL; path = path->next)
    {
    int *tmpPath = CloneArray(path->vertices, path->vCount);
    int endIx = path->vCount - 1;
    freez(&path->vertices);
    path->vertices = tmpPath;
    path->maxVCount = path->vCount;
    path->upV = pathFindUpstreamVertex(path, ag, distance, source, sink);
    path->downV = pathFindDownstreamVertex(path, ag, distance, source, sink);
    path->bpCount = calcBpLength(path, splice, ag, source, sink);
    path->tName = cloneString(ag->tName);
    /* Set the chromosome starts and ends, but
       ignore source and sink. */
    if(path->vertices[0] == source)
	path->tStart = vPos[path->vertices[1]];
    else
	path->tStart = vPos[path->vertices[0]];
    if(path->vertices[endIx] == sink)
	path->tEnd = vPos[path->vertices[endIx - 1]];
    else
	path->tEnd = vPos[path->vertices[endIx]];
    }
slSort(&splice->paths, pathBpCountCmp);
}

boolean cassettePaths(struct splice *splice, struct altGraphX *ag, bool **em,
		       int source, int sink)
/* Return TRUE if paths are mutally exclusive and the longer
   path forms a cassette exon. */
{
/* short and long refer to bpCount. */
struct path *shortPath = splice->paths; 
struct path *longPath = splice->paths->next;
int *shortVerts = shortPath->vertices;
int *longVerts = longPath->vertices;

int endVert = longPath->vCount-1;
int startVert = 0;
int *verts = longPath->vertices;
boolean cassette = FALSE;
unsigned char *vTypes = ag->vTypes;

/* Cassettes have 4 vertices in their path.
   Cassettes don't start with the source or end with the sink. */
if(endVert != 3 || verts[startVert] == source || verts[endVert] == sink)
    return FALSE;

/* Check to see if the path is an exon inclusion and then
   that they are mutually exclusive. */
if(shortPath->vCount == 2 && longPath->vCount == 4 && 
   vTypes[verts[0]] == ggHardEnd && 
   vTypes[verts[1]] == ggHardStart &&
   vTypes[verts[2]] == ggHardEnd &&
   vTypes[verts[3]] == ggHardStart &&
   isMutallyExclusive(splice, ag, em, source,sink))
    cassette = TRUE;
return cassette;
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

if(oldClassification)
    cassette = agxIsCassette(ag, em, verts[0], verts[1], verts[3],
			     &altStart, &altEnd, &startV, &endV);
else
    cassette = cassettePaths(splice, ag, em, source, sink);
return cassette;
}

boolean retIntPaths(struct splice *splice, struct altGraphX *ag, bool **em,
		   int source, int sink)
/* Return TRUE if splice is an retInt exon, FALSE otherwise. */
{
/* short and long refer to bpCount. */
struct path *shortPath = splice->paths; 
struct path *longPath = splice->paths->next;
int *shortVerts = shortPath->vertices;
int *longVerts = longPath->vertices;
unsigned char *vTypes = ag->vTypes;
boolean retInt = FALSE;
/* RetInts have 2 vertices in their long path and 4 in the short path.
   RetInts don't start with the source or end with the sink. */
if(longPath->vCount != 2 || shortPath->vCount != 4 ||
   shortVerts[0] == source || shortVerts[3] == sink)
    return FALSE;
assert(shortVerts[0] == longVerts[0] && shortVerts[3] == longVerts[1]);

/* If one path has an intron and the other doesn't and are mutally
   exclusive they are retInt. */
if(shortPath->vCount == 4 && longPath->vCount == 2 &&
   vTypes[shortVerts[0]] == ggHardStart &&
   vTypes[shortVerts[1]] == ggHardEnd &&
   vTypes[shortVerts[2]] == ggHardStart &&
   vTypes[shortVerts[3]] == ggHardEnd &&
   vTypes[longVerts[0]] == ggHardStart &&
   vTypes[longVerts[1]] == ggHardEnd &&
   isMutallyExclusive(splice, ag, em, source,sink))
    retInt = TRUE;
return retInt;
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

if(oldClassification)
    retInt = agxIsRetainIntron(ag, em, shortVerts[0], shortVerts[1], longVerts[1],
			   &altStart, &altEnd);
else
    retInt = retIntPaths(splice, ag, em, source, sink);
return retInt;
}

boolean alt5Paths(struct splice *splice, struct altGraphX *ag, bool **em,
		   int source, int sink)
/* Return TRUE if splice is an alt5 exon, FALSE otherwise. */
{
/* short and long refer to bpCount. */
struct path *shortPath = splice->paths; 
struct path *longPath = splice->paths->next;
int *shortVerts = shortPath->vertices;
int *longVerts = longPath->vertices;
unsigned char *vTypes = ag->vTypes;
boolean alt5 = FALSE;
/* Alt5s have 3 vertices in both paths.
   Alt5s don't start with the source or end with the sink. */
if( shortPath->vCount != 3 || longPath->vCount != 3 || 
    shortVerts[0] == source || shortVerts[2] == sink ||
    longVerts[0] == source || longVerts[2] == sink)
    return FALSE;
assert(shortVerts[0] == longVerts[0] && shortVerts[2] == longVerts[2]);

/* If both paths form an exon end and are mutally
   exclusive they are alt5. */
if(shortPath->vCount == 3 && longPath->vCount == 3 &&
   vTypes[shortVerts[0]] == ggHardStart &&
   vTypes[shortVerts[1]] == ggHardEnd &&
   vTypes[shortVerts[2]] == ggHardStart &&
   vTypes[longVerts[1]] == ggHardEnd &&
   isMutallyExclusive(splice, ag, em, source,sink))
    alt5 = TRUE;
return alt5;
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

if(oldClassification)
    alt5 = agxIsAlt5Prime(ag, em, shortVerts[0], shortVerts[1], longVerts[1],
		      &altStart, &altEnd, &startV, &endV);
else
    alt5 = alt5Paths(splice, ag, em, source, sink);
return alt5;
}

boolean alt3Paths(struct splice *splice, struct altGraphX *ag, bool **em,
		   int source, int sink)
/* Return TRUE if splice is an alt3 exon, FALSE otherwise. */
{
/* short and long refer to bpCount. */
struct path *shortPath = splice->paths; 
struct path *longPath = splice->paths->next;
int *shortVerts = shortPath->vertices;
int *longVerts = longPath->vertices;
unsigned char *vTypes = ag->vTypes;
boolean alt3 = FALSE;
/* Alt3s have 3 vertices in both paths.
   Alt3s don't start with the source or end with the sink. */
if( shortPath->vCount != 3 || longPath->vCount != 3 || 
    shortVerts[0] == source || shortVerts[2] == sink ||
    longVerts[0] == source || longVerts[2] == sink)
    return FALSE;
assert(shortVerts[0] == longVerts[0] && shortVerts[2] == longVerts[2]);

/* If both paths form an exon end and are mutally
   exclusive they are alt3. */
if(shortPath->vCount == 3 && longPath->vCount == 3 &&
   vTypes[shortVerts[0]] == ggHardEnd &&
   vTypes[shortVerts[1]] == ggHardStart &&
   vTypes[shortVerts[2]] == ggHardEnd &&
   vTypes[longVerts[1]] == ggHardStart &&
   isMutallyExclusive(splice, ag, em, source,sink))
    alt3 = TRUE;
return alt3;
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
if(oldClassification)
    alt3 = agxIsAlt3Prime(ag, em, shortVerts[0], shortVerts[1], longVerts[1],
		      &altStart, &altEnd, &startV, &endV);
else
    alt3 = alt3Paths(splice, ag, em, source, sink);
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
unsigned char *vTypes = ag->vTypes;
int *verts = splice->paths->vertices;
struct path *path = NULL;
int softStartCount = 0;
/* Count how many soft starts are attached to the source. */
for(path = splice->paths; path != NULL; path = path->next)
    if(vTypes[path->vertices[1]] == ggSoftStart)
	softStartCount++;

if(verts[0] == source && softStartCount > 1 &&
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
unsigned char *vTypes = ag->vTypes;
int *verts = splice->paths->vertices;
int softEndCount = 0;
/* Count how many soft starts are attached to the source. */
for(path = splice->paths; path != NULL; path = path->next)
    if(vTypes[path->vertices[path->vCount - 2]] == ggSoftEnd)
	softEndCount++;
if(verts[endIx] == sink && softEndCount > 1 &&
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
    if(type == alt5Prime) { alt5Flipped++; type = alt3Prime; }
    else if(type == alt3Prime) { alt3Flipped++; type = alt5Prime; }
    else if(type == alt5PrimeSoft) type = alt3PrimeSoft;
    else if(type == alt3PrimeSoft) type = alt5PrimeSoft;
    }
return type;
}

struct path *newPath(int maxVerts)
/* Create a path with the number of possible vertices set 
   to maxVerts. */
{
struct path *path = NULL;
assert(maxVerts); /* Can't call with zero. */
AllocVar(path);
path->maxVCount = maxVerts;
AllocArray(path->vertices, path->maxVCount);
return path;
}

struct splice *newSplice(struct altGraphX *ag, int vertIx, int sink, int source)
/* Create a new splice given an altGraphX record and return it. */
{
struct splice *splice = NULL;
char buff[256];
/* Allocate and initialize the splice. */
AllocVar(splice);
splice->tName = cloneString(ag->tName);
splice->tStart = BIGNUM;
splice->tEnd = -1;
splice->strand[0] = ag->strand[0];
splice->agxId = ag->id;
splice->vCount = ag->vertexCount;
splice->vPositions = CloneArray(ag->vPositions, splice->vCount);
splice->vTypes = CloneArray(ag->vTypes, splice->vCount);
if(vertIx == source)
    safef(buff, sizeof(buff), "%s.s", ag->name, vertIx);
else
    safef(buff, sizeof(buff), "%s.%d", ag->name, vertIx);
splice->name = cloneString(buff);
return splice;
}

void reverseVertOrder(struct path *pathList)
/* Reverse the order of the vertices in the path. */
{
int *vertArray = NULL;
struct path *path = NULL;
int i = 0;
int vC = 0;
for(path = pathList; path != NULL; path = path->next)
    {
    vC = path->vCount;
    AllocArray(vertArray, vC);
    for(i = vC - 1; i >= 0; i--)
	{
	vertArray[vC - i - 1] = path->vertices[i];
	}
    CopyArray(vertArray, path->vertices, vC);
    freez(&vertArray);
    }
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
struct path *path = NULL;

splice = newSplice(ag, vertIx, sink, source);
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
	int altBpSize = 0;
	/* Found end of altsplice event. Enumerate all the paths
	   between it and end vertex. */
	*endIx = i;
	splice->paths = pathsBetweenVerts(distance, source, sink, ag,
					  em, vertIx, i);
	reverseVertOrder(splice->paths);
	splice->pathCount = slCount(splice->paths);
	simplifyAndOrderPaths(splice, distance, ag, source, sink);
	splice->type = typeForSplice(splice, ag, em, source, sink);
	/* For now just keep track of difference between first
	   two paths. That way can find 3bp differences. */
	if(splice->pathCount >= 2)
	    altBpSize = splice->paths->next->bpCount - splice->paths->bpCount;
	else
	    altBpSize = splice->paths->bpCount;
	logSpliceType(splice->type, altBpSize);
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

/* Loop through paths and get the min and max chromosomal
   coordinates. */
for(path = splice->paths; path != NULL; path = path->next)
    {
    splice->tStart = min(splice->tStart, path->tStart);
    splice->tEnd = max(splice->tEnd, path->tEnd);
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

void finishSplice(struct splice *splice, int **distance,
		  struct altGraphX *ag, int source, int sink)
/* Set the tStart, tEnd, etc. of a splice to make 
   it ready for recording. */
{
struct path *path = NULL;
simplifyAndOrderPaths(splice, distance, ag, source, sink);
/* Loop through paths and get the min and max chromosomal
   coordinates. */
for(path = splice->paths; path != NULL; path = path->next)
    {
    splice->tStart = min(splice->tStart, path->tStart);
    splice->tEnd = max(splice->tEnd, path->tEnd);
    }
}

int countAltSplices(struct splice *spliceList)
/* Return the number of entries that are not type==altControl */
{
struct splice *splice = NULL;
int altCount = 0;
for(splice = spliceList; splice != NULL; splice = splice->next)
    if(splice->type != altControl)
	altCount++;
return altCount;
}

int doAltPathsAnalysis(struct altGraphX *agx, FILE *spliceOut)
/* Examine each altGraphX record for splicing events. Return number
 of alt events in graph. */
{
bool **em = altGraphXCreateEdgeMatrix(agx);
int source = -1, sink = agx->vertexCount;
int vC = agx->vertexCount + 2;
int *vPos = agx->vPositions;
int **distance = createDistanceMatrix(agx, em, source, sink);
int vertIx = 0;
int numAltEvents = 0;
struct splice *splice = NULL, *spliceList = NULL;
struct path *path = NULL;

/* Initialize first splice. */
splice = newSplice(agx, 0, sink, source);
splice->paths = newPath(sink - source);
splice->pathCount++;
/* Start searching for alt-splicing events. */
for(vertIx = source; vertIx <= sink; )
    {
    /* If a vertex is alt spliced, find the end of the alt-splicing
       event and continue from there. */
    if(altSplicedVertex(distance, vertIx, source, sink))
	{
	int nextVertIx = vertIx;
	/* If we have a control splice going, finish it and push it on
	   the list. */
	pathAddTail(splice->paths, vertIx);
	if(splice->paths->vCount > 1)
	    {
	    splice->type = altControl;
	    finishSplice(splice, distance, agx, source, sink);
	    logSpliceType(splice->type, splice->paths->bpCount);
	    slAddHead(&spliceList, splice);
	    }
	else
	    spliceFree(&splice);

	/* Find the end of this alt-splicing event. */
	findAltSplicesFromVertice(agx, source, sink, em, distance, 
				  &spliceList, vertIx, &nextVertIx);

	/* Make sure that we found an end that is ahead of the start. */
	if(vertIx >= nextVertIx) 
	    errAbort("vertIx = %d and nextVertIx = %d in agx: %s", 
		     vertIx, nextVertIx, agx->name);
	vertIx = nextVertIx;
	
	splice = NULL;
	/* Initialize next control path. */
	splice = newSplice(agx, vertIx, sink, source);
	splice->paths = newPath(max(sink - vertIx,2));
	splice->pathCount++;
	}
    else 
	{
	/* Add a vertex to the control path. */
	pathAddTail(splice->paths, vertIx);
        /* Try the next vertex. */
	vertIx++;
	}
    }
/* If we have a control splice going, finish it and push it on
   the list. */
if(splice != NULL && splice->paths->vCount > 1)
    {
    splice->type = altControl;
    finishSplice(splice, distance, agx, source, sink);
    logSpliceType(splice->type, splice->paths->bpCount);
    slAddHead(&spliceList, splice);
    }
else
    spliceFree(&splice);

/* Count the alt-events. */
numAltEvents = countAltSplices(spliceList);

/* Output the alternative splicing events. */
slSort(&spliceList, spliceCmp);
for(splice = spliceList; splice != NULL; splice = splice->next)
    {
    for(path = splice->paths; path != NULL; path = path->next)
	{
	int pathCount = 0;
	/* If writing beds, create the bed and write it out. */
	if(pathBedFile != NULL)
	    {	
	    struct bed *bed = bedForPath(path, splice, pathCount++, agx, source, sink, TRUE);
	    if(bed != NULL)
		bedTabOutN(bed, 12, pathBedFile);
	    bedFree(&bed);
	    }
	}
    if(splice->pathCount != 0)
	spliceTabOut(splice, spliceOut);
    }

/* Cleanup. */
spliceFreeList(&spliceList);
freeDistanceMatrix(&distance, source, sink);
altGraphXFreeEdgeMatrix(&em, agx->vertexCount);
return numAltEvents;
}

void altPaths(char *fileIn, char *fileOut)
/* Loop through each graph and output alternative splicing paths. */
{
struct altGraphX *agx = NULL, *agxList = NULL;
FILE *out = NULL, *subOut = NULL;
int altGraphCount = 0, numAlts = 0;
int totalAltCount = 0;
int idCount = 0;
char *subOutName = optionVal("saveGraphs", NULL);
warn("Loading graphs from %s", fileIn);
agxList = altGraphXLoadAll(fileIn);
out = mustOpen(fileOut, "w");
if(subOutName != NULL)
    subOut = mustOpen(subOutName, "w");
dotForUserInit(max(slCount(agxList)/20, 1));
for(agx = agxList; agx != NULL; agx = agx->next)
    {
    struct altGraphX *subAgx = NULL, *subAgxList = NULL;
    boolean counted = FALSE;
    dotForUser();
    /* Break the graph into connected components, 
       with the orthologous graphs it is possible that the
       graph is not connected. */
    subAgxList = agxConnectedComponents(agx);
    for(subAgx = subAgxList; subAgx != NULL; subAgx = subAgx->next)
	{
	subAgx->id = idCount++;
	if(subOut != NULL)
	    altGraphXTabOut(subAgx, subOut);
	numAlts = doAltPathsAnalysis(subAgx, out);
	totalAltCount += numAlts;
	if(numAlts > 0 && !counted)
	    {
	    altGraphCount++;
	    counted = TRUE;
	    }
	}
    altGraphXFreeList(&subAgxList);
    }
warn("");
carefulClose(&subOut);
printSpliceTypeInfo(slCount(agxList), altGraphCount, totalAltCount);
warn("Cleaning up.");
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
char *fileName = optionVal("pathBeds", NULL);
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
if(optionExists("pathBeds"))
    initPathBeds();
if(optionExists("oldClassification"))
    oldClassification = TRUE;
altPaths(argv[1], argv[2]);
carefulClose(&distMatrixFile);
carefulClose(&pathBedFile);
return 0;
}

	 








   
