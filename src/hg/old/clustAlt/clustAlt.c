#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "cda.h"
#include "clustAlt.h"

boolean lottaDebug = FALSE;

void usage()
/* Describe how to use program and exit. */
{
errAbort(
  "clustAlt - do alt-splice clustering and generate constraints for genie.\n"
  "Usage:\n"
  "   clustAlt bacAccession(s)");
}

struct denseAliInfo *makeTestDa(int *data, int dataSize)
/* Make up denseAliInfo from test data. */
{
int vertexCount;
struct denseAliInfo *da;
struct vertex *v;
int i;

vertexCount = dataSize;
AllocVar(da);
da->vertexCount = vertexCount;
da->vertices = AllocArray(v, vertexCount);
for (i=0; i<dataSize; i += 1 )
    {
    v->position = *data++;
    if (i == 0)
	v->type = softStart;
    else if (i == dataSize-1)
	v->type = softEnd;
    else if (i&1)
	v->type = hardEnd;
    else
	v->type = hardStart;
    ++v;
    }
return da;
}

#ifdef ONE_SET
int rna1[] = {50, 100, 1000, 1150};
int rna2[] = {25, 100, 1000, 1150};
int rna3[] = {50, 100, 1000, 1200};
int rna4[] = {50,  75, 1000, 1150};
int rna5[] = {50, 100, 1050, 1150};
int rna6[] = {50, 100, 500, 600, 1000, 1150};
int rna7[] = {1100, 1250, 1300, 1400};
int rna8[] = {1075, 1175, 1300, 1400};
int rna9[] = {1025, 1175, 1300, 1400};
#endif

int rna1[] = {0, 100, 150, 500};
int rna2[] = {50, 100, 150, 200, 250, 550};
int rna3[] = {300, 700, 750, 800};
int rna4[] = {350, 600, 650, 700, 750, 850};

struct ggInput *makeTestInput()
/* Make up nice test set for graph thing. */
{
struct ggInput *ggi;
AllocVar(ggi);
ggi->orientation = 1;
slAddTail(&ggi->mrnaList, makeTestDa(rna1, ArraySize(rna1)));
slAddTail(&ggi->mrnaList, makeTestDa(rna2, ArraySize(rna2)));
slAddTail(&ggi->mrnaList, makeTestDa(rna3, ArraySize(rna3)));
slAddTail(&ggi->mrnaList, makeTestDa(rna4, ArraySize(rna4)));
#ifdef ONE_SET
slAddTail(&ggi->mrnaList, makeTestDa(rna5, ArraySize(rna5)));
slAddTail(&ggi->mrnaList, makeTestDa(rna6, ArraySize(rna6)));
slAddTail(&ggi->mrnaList, makeTestDa(rna7, ArraySize(rna7)));
slAddTail(&ggi->mrnaList, makeTestDa(rna8, ArraySize(rna8)));
slAddTail(&ggi->mrnaList, makeTestDa(rna9, ArraySize(rna9)));
#endif
ggi->start = 0;
ggi->end = 1000;
}

char charForType(int type)
/* Return character corresponding to edge. */
{
char c;
switch (type)
    {
    case softStart:
	c = '{';
	break;
    case softEnd:
	c = '}';
	break;
    case hardStart:
	c = '[';
	break;
    case hardEnd:
	c = ']';
	break;
    case unused:
        c = 'x';
	break;
    default:
	errAbort("Unknown vertex type %d", type);
	break;
    }
return c;
}

dumpDa(struct denseAliInfo *da, int start, int end)
/* Display list of dense Alis. */
{
enum { screenWidth = 80};
static char line[screenWidth+1];
int ggiWidth = end - start;
double scaler = (double)(screenWidth-1)/ggiWidth;
struct vertex *v;
int pos;
int i;

memset(line, ' ', screenWidth);
v = da->vertices;
for (i=0; i<da->vertexCount; ++i,++v)
    {
    char c = charForType(v->type);
    pos = round((v->position-start) * scaler);
    assert(pos >= 0 && pos < screenWidth);
    line[pos] = c;
    }
printf("%s\n", line);
}

void dumpGgi(struct ggInput *ggi)
/* Display ggi on the screen. */
{
struct denseAliInfo *da;

for (da = ggi->mrnaList; da != NULL; da = da->next)
    dumpDa(da, ggi->start, ggi->end);
}

void dumpGg(struct geneGraph *gg)
/* Print out a gene graph. */
{
int i,j;
int vertexCount = gg->vertexCount;

printf("geneGraph has %d vertices\n", vertexCount);
for (i=0; i<vertexCount; ++i)
    {
    struct vertex *v = &gg->vertices[i];
    bool *arcsOut = gg->edgeMatrix[i];
    printf("  %d %5d %c   ", i, v->position, charForType(v->type));
    for (j=0; j<vertexCount; ++j)
	{
	if (arcsOut[j])
	    printf(" %d", j);
	}
    printf("\n");
    }
}

int vertexIx(struct vertex *array, int arraySize, int pos, int type)
/* Return index of vertex in array, or -1 if not in array. */
{
int i;
for (i=0; i<arraySize; ++i)
    {
    if (array->position == pos && array->type == type)
	return i;
    ++array;
    }
return -1;
}

struct geneGraph *makeInitialGraph(struct ggInput *ggi)
/* Create initial gene graph from input input. */
{
int vAlloc = 512;
int i;
struct vertex *vAll;
int vAllCount = 0;
struct denseAliInfo *da;
struct geneGraph *gg;
bool **em;
int totalWithDupes = 0;

AllocArray(vAll, vAlloc);

/* First fill up array with unique vertices. */
for (da = ggi->mrnaList; da != NULL; da = da->next)
    {
    int countOne = da->vertexCount;
    struct vertex *vOne = da->vertices;
    int vix;
    totalWithDupes += countOne;
    for (i=0; i<countOne; ++i)
	{
	vix = vertexIx(vAll, vAllCount, vOne->position, vOne->type);
	if (vix < 0)
	    {
	    if (vAllCount >= vAlloc)
		{
		vAlloc <<= 1;
		ExpandArray(vAll, vAllCount, vAlloc);
		}
	    vAll[vAllCount++] = *vOne;
	    }
	++vOne;
	}
    }

uglyf("Total with dupes %d without dupes %d\n", totalWithDupes, vAllCount);

/* Allocate gene graph and edge matrix. */
AllocVar(gg);
gg->vertexCount = vAllCount;
AllocArray(gg->vertices, vAllCount);
memcpy(gg->vertices, vAll, vAllCount*sizeof(*vAll));
gg->edgeMatrix = AllocArray(em, vAllCount);
for (i=0; i<vAllCount; ++i)
    {
    AllocArray(em[i], vAllCount);
    }

/* Fill in edge matrix. */
for (da = ggi->mrnaList; da != NULL; da = da->next)
    {
    int countOne = da->vertexCount;
    struct vertex *vOne = da->vertices;
    int vix, lastVix;
    vix = vertexIx(vAll, vAllCount, vOne->position, vOne->type);
    for (i=1; i<countOne; ++i)
	{
	++vOne;
	lastVix = vix;
	vix = vertexIx(vAll, vAllCount, vOne->position, vOne->type);
	assert(vix >= 0 && lastVix >= 0);
	em[lastVix][vix] = TRUE;
	}
    }
freeMem(vAll);
return gg;
}

#ifdef UNUSED
int findFurthestLinkedStart(struct geneGraph *gg, int endIx)
/* Find index furthest connected start vertex. */
{
int vCount = gg->vertexCount;
struct vertex *vertices = gg->vertices;
bool **em = gg->edgeMatrix;
int startIx;
int startPos;
int bestStartIx;
int bestStartPos = 0x3fffffff;


for (startIx=0; startIx<vCount; ++startIx)
    {
    if (em[startIx][endIx])
	{
	startPos = vertices[startIx].position;
	if (startPos < bestStartPos)
	    {
	    bestStartPos = startPos;
	    bestStartIx = startIx;
	    }
	}
    }
return bestStartIx;
}

boolean softlyTrimStart(struct geneGraph *gg, int softIx)
/* Try and trim one soft start. Return TRUE if did trim. */
{
int vCount = gg->vertexCount;
struct vertex *vertices = gg->vertices;
bool **em = gg->edgeMatrix;
bool *edgesOut = em[softIx];
int endIx;
boolean anyTrim = FALSE;
boolean anyLeft = FALSE;

for (endIx=0; endIx < vCount; ++endIx)
    {
    if (edgesOut[endIx])
	{
	int bestStart = findFurthestLinkedStart(gg, endIx);
	if (bestStart != softIx)
	    {
	    anyTrim = TRUE;
	    edgesOut[endIx] = FALSE;
	    uglyf("Trimming %d\n", softIx);
	    }
	else
	    {
	    anyLeft = TRUE;
	    }
	}
    }
if (!anyLeft)
    vertices[softIx].type = unused;
return anyTrim;
}

int findFurthestLinkedEnd(struct geneGraph *gg, int startIx)
/* Find index furthest connected end vertex. */
{
int vCount = gg->vertexCount;
struct vertex *vertices = gg->vertices;
bool **em = gg->edgeMatrix;
bool *edgesOut = em[startIx];
int endIx;
int endPos;
int bestEndIx;
int bestEndPos = -0x3fffffff;


for (endIx=0; endIx<vCount; ++endIx)
    {
    if (edgesOut[endIx])
	{
	endPos = vertices[endIx].position;
	if (endPos > bestEndPos)
	    {
	    bestEndPos = endPos;
	    bestEndIx = endIx;
	    }
	}
    }
return bestEndIx;
}

boolean softlyTrimEnd(struct geneGraph *gg, int softIx)
/* Try and trim one soft start. Return TRUE if did trim. */
{
int vCount = gg->vertexCount;
struct vertex *vertices = gg->vertices;
bool **em = gg->edgeMatrix;
int startIx;
boolean anyTrim = FALSE;
boolean anyLeft = FALSE;

for (startIx=0; startIx < vCount; ++startIx)
    {
    if (em[startIx][softIx])
	{
	int bestEnd = findFurthestLinkedEnd(gg, startIx);
	if (bestEnd != softIx)
	    {
	    anyTrim = TRUE;
	    em[startIx][softIx] = FALSE;
	    uglyf("Trimming %d\n", softIx);
	    }
	else
	    {
	    anyLeft = TRUE;
	    }
	}
    }
if (!anyLeft)
    vertices[softIx].type = unused;
return anyTrim;
}


boolean softlyTrim(struct geneGraph *gg)
/* Trim all redundant soft starts and ends.  Return TRUE if trimmed 
 * any. */
{
int i;
int vCount = gg->vertexCount;
struct vertex *vertices = gg->vertices;
boolean result = FALSE;


for (i=0; i<vCount; ++i)
    {
    switch (vertices[i].type)
	{
	case softStart:
	    result |= softlyTrimStart(gg, i);
	    break;
	case softEnd:
	    result |= softlyTrimEnd(gg, i);
	    break;
	}
    }
return result;
}
#endif /* UNUSED */

boolean arcsForSoftStartedExon(struct geneGraph *gg, int softStartIx, int hardEndIx)
/* Put in arcs between hard start, and hard ends of any compatible
 * exons overlapping this one */
{
boolean result = FALSE;
int vCount = gg->vertexCount;
int sIx, eIx;
bool **em = gg->edgeMatrix;
struct vertex *vertices = gg->vertices;
int softStartPos = vertices[softStartIx].position;
int hardEndPos = vertices[hardEndIx].position;

for (eIx = 0; eIx < vCount; ++eIx)
    {
    struct vertex *ev = vertices+eIx;
    if (ev->type == softEnd || eIx == hardEndIx)  
	{
	int evpos = ev->position;
	if (evpos <= hardEndPos && evpos > softStartPos)
	    {
	    for (sIx = 0; sIx < vCount; ++sIx)
		{
		if (sIx != softStartIx && em[sIx][eIx])
		    {
		    struct vertex *sv = vertices+sIx;
		    if (sv->position <= softStartPos)
			{
			em[sIx][hardEndIx] = TRUE;
			result = TRUE;
			uglyf("New Arc from %d to %d because of %d\n", 
				sIx, hardEndIx,softStartIx);
			}
		    }
		}
	    }
	}
    }
return result;
}


boolean softStartArcs(struct geneGraph *gg, int softStartIx)
/* Replace a soft start with all hard starts of overlapping,
 * but not conflicting exons. */
{
int myEndIx;
int vCount = gg->vertexCount;
bool **em = gg->edgeMatrix;
boolean result = FALSE;
boolean anyLeft = FALSE;
int i;

for (i=0; i<vCount; ++i)
    {
    if (em[softStartIx][i])
	{
	int res;
	res = arcsForSoftStartedExon(gg, softStartIx, i);
	if (res)
	    {
	    em[softStartIx][i] = FALSE;
	    uglyf("Trimming arc from %d to %d\n", softStartIx, i);
	    result = TRUE;
	    }
	else
	    {
	    anyLeft = TRUE;
	    }
	}
    }
if (!anyLeft)
    {
    gg->vertices[softStartIx].type = unused;
    uglyf("Trimming vertex %d\n", softStartIx);
    }
return result;
}

boolean arcsForSoftEndedExon(struct geneGraph *gg, int hardStartIx, int softEndIx)
/* Put in arcs between hard start, and hard ends of any compatible
 * exons overlapping this one */
{
boolean result = FALSE;
int vCount = gg->vertexCount;
int sIx, eIx;
bool **em = gg->edgeMatrix;
struct vertex *vertices = gg->vertices;
int hardStartPos = vertices[hardStartIx].position;
int softEndPos = vertices[softEndIx].position;

for (sIx = 0; sIx < vCount; ++sIx)
    {
    struct vertex *sv = vertices+sIx;
    if (sv->type == softStart || sIx == hardStartIx)
	{
	int svpos = sv->position;
	if (svpos >= hardStartPos && svpos < softEndPos)
	    {
	    for (eIx = 0; eIx < vCount; ++eIx)
		{
		if (eIx != softEndIx && em[sIx][eIx])
		    {
		    struct vertex *ev = vertices+eIx;
		    if (ev->position >= softEndPos)
			{
			em[hardStartIx][eIx] = TRUE;
			result = TRUE;
			uglyf("New Arc from %d to %d because of %d\n", 
			   hardStartIx, eIx, softEndIx);
			}
		    }
		}
	    }
	}
    }
return result;
}

boolean softEndArcs(struct geneGraph *gg, int softEndIx)
/* Replace a soft end with all hard ends of overlapping,
 * but not conflicting gexons. */
{
int myStartIx;
int vCount = gg->vertexCount;
bool **em = gg->edgeMatrix;
boolean result = FALSE;
boolean anyLeft = FALSE;
int i;

for (i=0; i<vCount; ++i)
    {
    if (em[i][softEndIx])
	{
	int res;
	res = arcsForSoftEndedExon(gg, i, softEndIx);
	if (res)
	    {
	    em[i][softEndIx] = FALSE;
	    result = TRUE;
	    uglyf("Trimming arc from %d to %d\n", i, softEndIx);
	    }
	else
	    {
	    anyLeft = TRUE;
	    }
	}
    }
if (!anyLeft)
    {
    gg->vertices[softEndIx].type = unused;
    uglyf("Trimming vertex %d\n", softEndIx);
    }
return result;
}

boolean arcsForOverlaps(struct geneGraph *gg)
/* Add in arcs for exons with soft ends that overlap other
 * exons. */
{
int i;
int vCount = gg->vertexCount;
struct vertex *vertices = gg->vertices;
boolean result = FALSE;


for (i=0; i<vCount; ++i)
    {
    switch (vertices[i].type)
	{
	case softStart:
	    result |= softStartArcs(gg, i);
	    break;
	case softEnd:
	    result |= softEndArcs(gg, i);
	    break;
	}
    }
return result;
}

struct geneGraph *makeGeneGraph(struct ggInput *ggi)
{
struct geneGraph *gg = makeInitialGraph(ggi);
uglyf("\nInitial graph\n");
dumpGg(gg);
uglyf("\narcs for overlaps.\n");
arcsForOverlaps(gg);
dumpGg(gg);
//uglyf("\nSoftly trimming.\n");
//softlyTrim(gg);
//dumpGg(gg);
return gg;
}

struct vertex *rVertices;	/* Vertex stack. */
int rVertexAlloc = 0;		/* Allocated size of stack. */
int rVertexCount = 0;           /* Count on stack. */
int rStart, rEnd;               /* Start/end of whole set in genomic coordinates (for display) */

void pushVertex(struct vertex *v)
/* Add vertex to end of rVertices. */
{
if (rVertexCount >= 1000)
    errAbort("Looks like a loop in graph!");
if (rVertexCount >= rVertexAlloc)
    {
    if (rVertexAlloc == 0)
	{
	rVertexAlloc = 2;
	AllocArray(rVertices, rVertexAlloc);
	}
    else
	{
	rVertexAlloc <<= 1;
	ExpandArray(rVertices, rVertexCount, rVertexAlloc);
	}
    }
rVertices[rVertexCount++] = *v;
}

void popVertex()
{
--rVertexCount;
if (rVertexCount < 0)
    errAbort("vertex stack underflow");
}

void printConstraint()
{
struct denseAliInfo da;
da.next = NULL;
da.vertices = rVertices;
da.vertexCount = rVertexCount;
dumpDa(&da, rStart, rEnd);
}

void rTraverse(struct geneGraph *gg, int startVertex)
{
struct vertex *v = &gg->vertices[startVertex];
pushVertex(v);
if (v->type == softEnd)
    {
    printConstraint();
    }
else
    {
    int i;
    int vCount = gg->vertexCount;
    bool *waysOut = gg->edgeMatrix[startVertex];
    for (i=0; i<vCount; ++i)
	{
	if (waysOut[i])
	    rTraverse(gg, i);
	}
    }
popVertex();
}

void traverseGeneGraph(struct geneGraph *gg, int start, int end)
/* Print out constraints for gene graph. */
{
int vCount = gg->vertexCount;
struct vertex *vertices = gg->vertices;
int i;

rStart = start;
rEnd = end;

for (i=0; i<vCount; ++i)
    {
    if (vertices[i].type == softStart)
	rTraverse(gg, i);
    }
}

int main(int argc, char *argv[])
{
struct ggInput *ggi = makeTestInput();
struct geneGraph *gg;

uglyf("Initial input\n");
dumpGgi(ggi);
uglyf("\n");
gg = makeGeneGraph(ggi);
traverseGeneGraph(gg, ggi->start, ggi->end);
return 0;
}
