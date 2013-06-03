/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* ggDbIo.c - Load and save to database format (as an altGraph table in mySQL). */
#include "common.h"
#include "altGraph.h"
#include "geneGraph.h"





void ggTabOut(struct geneGraph *gg, FILE *f)
/* Convert into database format and save as line in tab-delimited file. */
{
struct altGraph *ag;
ag = ggToAltGraph(gg);
altGraphTabOut(ag, f);
altGraphFree(&ag);
}

struct geneGraph *ggFromAltGraph(struct altGraph *ag)
/* Convert gene graph from database representation to
 * internal representation. */
{
struct geneGraph *gg;
struct ggVertex *v;
int vertexCount = ag->vertexCount;
bool **em;
int i;

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

safef(gg->strand, sizeof(gg->strand), "%s", ag->strand);
gg->tStart = ag->tStart;
gg->tEnd = ag->tEnd;
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
