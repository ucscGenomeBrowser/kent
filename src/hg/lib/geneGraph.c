/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* geneGraph - Relatively short, high level routines for using
 * (but not necessarily creating) a geneGraph.  A geneGraph
 * is a way of representing possible alt-splicing patterns
 * of a gene.
 */

#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "geneGraph.h"
#include "ggPrivate.h"


void freeGeneGraph(struct geneGraph **pGg)
/* Free up a gene graph. */
{
struct geneGraph *gg;

if ((gg = *pGg) != NULL)
    {
    int i, vcount;
    bool **em = gg->edgeMatrix;
    vcount = gg->vertexCount;
    for (i=0; i<vcount; ++i)
	freeMem(em[i]);
    freeMem(em);
    freeMem(gg->vertices);
    freez(pGg);
    }
}


/* Some variables and routines for managing the depth first
 * traversal of geneGraph - to generate individual
 * constraints from it. */

static GgTransOut rOutput;		/* Output transcript. */
static struct ggVertex *rVertices;	/* Vertex stack. */
static int rVertexAlloc = 0;		/* Allocated size of stack. */
static int rVertexCount = 0;            /* Count on stack. */
static int rStart, rEnd;                /* Start/end of whole set in genomic coordinates*/

static void pushVertex(struct ggVertex *v)
/* Add vertex to end of rVertices. */
{
if (rVertexCount >= 1000)
    errAbort("Looks like a loop in graph!");
if (rVertexCount >= rVertexAlloc)
    {
    if (rVertexAlloc == 0)
	{
	rVertexAlloc = 256;
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

static void popVertex()
/* Remove vertex from end of rVertices. */
{
--rVertexCount;
if (rVertexCount < 0)
    errAbort("ggVertex stack underflow");
}

static void outputTranscript()
/* Output one transcript. */
{
struct ggAliInfo da;
da.next = NULL;
da.vertices = rVertices;
da.vertexCount = rVertexCount;
rOutput(&da, rStart, rEnd);
}

static void rTraverse(struct geneGraph *gg, int startVertex)
/* Recursive traversal of gene graph - printing transcripts
 * when hit end nodes. */
{
struct ggVertex *v = &gg->vertices[startVertex];
pushVertex(v);
if (v->type == ggSoftEnd)
    {
    outputTranscript();
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

void traverseGeneGraph(struct geneGraph *gg, int cStart, int cEnd, GgTransOut output)
/* Print out constraints for gene graph. */
{
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
int i;

rStart = cStart;
rEnd = cEnd;
rOutput = output;

for (i=0; i<vCount; ++i)
    {
    if (vertices[i].type == ggSoftStart)
	rTraverse(gg, i);
    }
}

static int countUsed(struct ggVertex *v, int vCount, int *translator)
/* Count number of vertices actually used. */
{
int i;
int count = 0;
for (i=0; i<vCount; ++i)
    {
    translator[i] = count;
    if (v[i].type != ggUnused)
	++count;
    }
return count;
}


struct altGraph *ggToAltGraph(struct geneGraph *gg)
/* convert a gene graph to an altGraph data structure */
{
struct altGraph *ag = NULL;
bool **em = gg->edgeMatrix;
int edgeCount = 0;
int totalVertexCount = gg->vertexCount;
int usedVertexCount;
int usedCount;
int *translator;	/* Translates away unused vertices. */
struct ggVertex *vertices = gg->vertices;
int i,j;
UBYTE *vTypes;
unsigned *vBacs, *vPositions, *edgeStarts, *edgeEnds, *mrnaRefs;
struct maRef *ref;

AllocArray(translator, totalVertexCount);
usedVertexCount = countUsed(vertices, totalVertexCount, translator);
for (i=0; i<totalVertexCount; ++i)
    {
    bool *waysOut = em[i];
    for (j=0; j<totalVertexCount; ++j)
	if (waysOut[j])
	    ++edgeCount;
    }
AllocVar(ag);
snprintf(ag->strand, sizeof(ag->strand), "%s", gg->strand);
ag->tName = cloneString(gg->tName);
ag->tStart = gg->tStart;
ag->tEnd = gg->tEnd;
ag->vertexCount = usedVertexCount;
ag->vTypes = AllocArray(vTypes, usedVertexCount);
ag->vPositions = AllocArray(vPositions, usedVertexCount);
ag->mrnaRefCount = gg->mrnaRefCount;
ag->mrnaRefs = AllocArray(ag->mrnaRefs, gg->mrnaRefCount);
for(i=0; i < gg->mrnaRefCount; i++)
    {
    ag->mrnaRefs[i] = cloneString(gg->mrnaRefs[i]);
    }

for (i=0,j=0; i<totalVertexCount; ++i)
    {
    struct ggVertex *v = vertices+i;
    if (v->type != ggUnused)
	{
	vTypes[j] = v->type;
	vPositions[j] = v->position;
	++j;
	}
    }
ag->edgeCount = edgeCount;
ag->edgeStarts = AllocArray(edgeStarts, edgeCount);
ag->edgeEnds = AllocArray(edgeEnds, edgeCount);
edgeCount = 0;
for (i=0; i<totalVertexCount; ++i)
    {
    bool *waysOut = em[i];
    for (j=0; j<totalVertexCount; ++j)
	if (waysOut[j])
	    {
	    edgeStarts[edgeCount] = translator[i] ;
	    edgeEnds[edgeCount] = translator[j];
	    ++edgeCount;
	    }
    }
freeMem(translator);
return ag;
}
