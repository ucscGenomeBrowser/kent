/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* ggDbIo.c - Load and save to database format (as an altGraph table in mySQL). */
#include "common.h"
#include "ggDbRep.h"
#include "geneGraph.h"

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

void ggTabOut(struct geneGraph *gg, FILE *f)
/* Convert into database format and save as line in tab-delimited file. */
{
struct altGraph *ag;
HGID bac = gg->startBac;
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
ag->id = hgNextId();
ag->orientation = gg->orientation;
ag->startBac = gg->startBac;
ag->startPos = gg->startPos;
ag->endBac = gg->endBac;
ag->endPos = gg->endPos;
ag->vertexCount = usedVertexCount;
ag->vTypes = AllocArray(vTypes, usedVertexCount);
ag->vBacs = AllocArray(vBacs, usedVertexCount);
ag->vPositions = AllocArray(vPositions, usedVertexCount);
for (i=0,j=0; i<totalVertexCount; ++i)
    {
    struct ggVertex *v = vertices+i;
    if (v->type != ggUnused)
	{
	vTypes[j] = v->type;
	vBacs[j] = bac;
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
	    edgeStarts[edgeCount] = translator[i];
	    edgeEnds[edgeCount] = translator[j];
	    ++edgeCount;
	    }
    }
ag->mrnaRefCount = gg->mrnaRefCount;
ag->mrnaRefs = CloneArray(gg->mrnaRefs, gg->mrnaRefCount);

altGraphTabOut(ag, f);
altGraphFree(&ag);
freeMem(translator);
}

struct geneGraph *ggFromAltGraph(struct altGraph *ag)
/* Convert gene graph from database representation to
 * internal representation. */
{
struct geneGraph *gg;
struct ggVertex *v;
int vertexCount = ag->vertexCount;
bool **em;
int i,j;

AllocVar(gg);
gg->vertices = AllocArray(v, vertexCount);
gg->vertexCount = vertexCount;
gg->edgeMatrix = AllocArray(em, vertexCount);
for (i=0; i<vertexCount; ++i)
    {
    AllocArray(em[i], vertexCount);
    v->position = ag->vPositions[i];
    v->type = ag->vTypes[i];
    ++v;
    }
for (i=0; i<ag->edgeCount; ++i)
    em[ag->edgeStarts[i]][ag->edgeEnds[i]] = TRUE;

gg->orientation = ag->orientation;
gg->startBac = ag->startBac;
gg->startPos = ag->startPos;
gg->endBac = ag->endBac;
gg->endPos = ag->endPos;
gg->mrnaRefs = CloneArray(ag->mrnaRefs, ag->mrnaRefCount);
gg->mrnaRefCount = ag->mrnaRefCount;
return gg;
}

struct geneGraph *ggFromRow(char **row)
/* Create a geneGraph from a row in altGraph table. */
{
struct altGraph *ag;
struct geneGraph *gg;

ag = altGraphLoad(row);
gg = ggFromAltGraph(ag);
altGraphFree(&ag);
return gg;
}
