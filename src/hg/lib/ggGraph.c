/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* ggGraph - Third pass of routine that generates a representation
 * of possible splicing patterns of a gene as a graph from mrna
 * clusters. */

#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "ggPrivate.h"

enum{ softEndBleedLimit = 5};  /* For soft ends allow some bleeding into next intron */


static int vertexIx(struct ggVertex *array, int arraySize, int pos, int type)
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

static struct geneGraph *makeInitialGraph(struct ggMrnaCluster *mc, struct ggMrnaInput *ci)
/* Create initial gene graph from input input. */
{
int vAlloc = 512;
int i;
struct ggVertex *vAll;
int vAllCount = 0;
struct ggAliInfo *da;
struct geneGraph *gg;
bool **em;
int totalWithDupes = 0;
struct maRef *ref;
HGID *mrnaRefs;
int mrnaRefCount;

AllocArray(vAll, vAlloc);

/* First fill up array with unique vertices. */
for (da = mc->mrnaList; da != NULL; da = da->next)
    {
    int countOne = da->vertexCount;
    struct ggVertex *vOne = da->vertices;
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
for (da = mc->mrnaList; da != NULL; da = da->next)
    {
    int countOne = da->vertexCount;
    struct ggVertex *vOne = da->vertices;
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


/* Fill in other info from ci and mc. */
gg->orientation = mc->orientation;
gg->startBac = gg->endBac = ci->seqIds[mc->seqIx];
gg->startPos = mc->tStart;
gg->endPos = mc->tEnd;
gg->mrnaRefCount = mrnaRefCount = slCount(mc->refList);
gg->mrnaRefs = AllocArray(mrnaRefs, mrnaRefCount);
for (ref = mc->refList, i=0; ref != NULL; ref = ref->next, ++i)
    mrnaRefs[i] = ref->ma->id;
return gg;
}

static int findFurthestLinkedStart(struct geneGraph *gg, int endIx)
/* Find index furthest connected start vertex. */
{
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
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

static boolean softlyTrimStart(struct geneGraph *gg, int softIx)
/* Try and trim one soft start. Return TRUE if did trim. */
{
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
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
	    }
	else
	    {
	    anyLeft = TRUE;
	    }
	}
    }
if (!anyLeft)
    vertices[softIx].type = ggUnused;
return anyTrim;
}

static int findFurthestLinkedEnd(struct geneGraph *gg, int startIx)
/* Find index furthest connected end vertex. */
{
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
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

static boolean softlyTrimEnd(struct geneGraph *gg, int softIx)
/* Try and trim one soft start. Return TRUE if did trim. */
{
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
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
	    }
	else
	    {
	    anyLeft = TRUE;
	    }
	}
    }
if (!anyLeft)
    vertices[softIx].type = ggUnused;
return anyTrim;
}


static boolean softlyTrim(struct geneGraph *gg)
/* Trim all redundant soft starts and ends.  Return TRUE if trimmed 
 * any. */
{
int i;
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
boolean result = FALSE;


for (i=0; i<vCount; ++i)
    {
    switch (vertices[i].type)
	{
	case ggSoftStart:
	    result |= softlyTrimStart(gg, i);
	    break;
	case ggSoftEnd:
	    result |= softlyTrimEnd(gg, i);
	    break;
	}
    }
return result;
}

static boolean arcsForSoftStartedExon(struct geneGraph *gg, int softStartIx, int hardEndIx)
/* Put in arcs between hard start, and hard ends of any compatible
 * exons overlapping this one */
{
boolean result = FALSE;
int vCount = gg->vertexCount;
int sIx, eIx;
bool **em = gg->edgeMatrix;
struct ggVertex *vertices = gg->vertices;
int softStartPos = vertices[softStartIx].position;
int hardEndPos = vertices[hardEndIx].position;

for (eIx = 0; eIx < vCount; ++eIx)
    {
    struct ggVertex *ev = vertices+eIx;
    int type = ev->type;
    if (type == ggSoftEnd || type == ggHardEnd)  
	{
	int evpos = ev->position;
	int softBleedAdjust = (type == ggHardEnd ? softEndBleedLimit : 0);
	if (evpos > softStartPos)
	// if (evpos <= hardEndPos && evpos > softStartPos)
	    {
	    for (sIx = 0; sIx < vCount; ++sIx)
		{
		if (sIx != softStartIx && em[sIx][eIx])
		    {
		    struct ggVertex *sv = vertices+sIx;
		    if (sv->position <= softStartPos + softBleedAdjust)
			{
			em[sIx][hardEndIx] = TRUE;
			result = TRUE;
			}
		    }
		}
	    }
	}
    }
return result;
}


static boolean softStartArcs(struct geneGraph *gg, int softStartIx)
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
    gg->vertices[softStartIx].type = ggUnused;
    }
return result;
}

static boolean arcsForSoftEndedExon(struct geneGraph *gg, int hardStartIx, int softEndIx)
/* Put in arcs between hard start, and hard ends of any compatible
 * exons overlapping this one */
{
boolean result = FALSE;
int vCount = gg->vertexCount;
int sIx, eIx;
bool **em = gg->edgeMatrix;
struct ggVertex *vertices = gg->vertices;
int hardStartPos = vertices[hardStartIx].position;
int softEndPos = vertices[softEndIx].position;

for (sIx = 0; sIx < vCount; ++sIx)
    {
    struct ggVertex *sv = vertices+sIx;
    int type = sv->type;
    if (type == ggSoftStart || type == ggHardStart)
	{
	int svpos = sv->position;
	int softBleedAdjust = (type == ggHardStart ? softEndBleedLimit : 0);
	// if (svpos >= hardStartPos && svpos < softEndPos)
	if (svpos < softEndPos)
	    {
	    for (eIx = 0; eIx < vCount; ++eIx)
		{
		if (eIx != softEndIx && em[sIx][eIx])
		    {
		    struct ggVertex *ev = vertices+eIx;
		    if (ev->position >= softEndPos - softBleedAdjust)
			{
			em[hardStartIx][eIx] = TRUE;
			result = TRUE;
			}
		    }
		}
	    }
	}
    }
return result;
}

static boolean softEndArcs(struct geneGraph *gg, int softEndIx)
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
	    }
	else
	    {
	    anyLeft = TRUE;
	    }
	}
    }
if (!anyLeft)
    {
    gg->vertices[softEndIx].type = ggUnused;
    }
return result;
}

static boolean arcsForOverlaps(struct geneGraph *gg)
/* Add in arcs for exons with soft ends that overlap other
 * exons. */
{
int i;
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
boolean result = FALSE;


for (i=0; i<vCount; ++i)
    {
    switch (vertices[i].type)
	{
	case ggSoftStart:
	    result |= softStartArcs(gg, i);
	    break;
	case ggSoftEnd:
	    result |= softEndArcs(gg, i);
	    break;
	}
    }
return result;
}

static boolean exonEnclosed(struct geneGraph *gg, int startIx, int endIx)
/* See if defined by start to end is completely enclosed by another exon. */
{
int i,j;
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
bool **em = gg->edgeMatrix;
boolean result = FALSE;
int eStart = vertices[startIx].position;
int eEnd = vertices[endIx].position;

for (i=0; i<vCount; ++i)
    {
    struct ggVertex *sv = vertices+i;
    UBYTE type = sv->type;
    if ((type == ggSoftStart || type == ggHardStart) && sv->position <= eStart)
	{
	for (j=0; j<vCount; ++j)
	    {
	    if (em[i][j] && (i != startIx || j != endIx))
		{
		struct ggVertex *dv = vertices+j;
		type = dv->type;
		if (type == ggSoftEnd || type == ggHardEnd && dv->position >= endIx)
		    return TRUE;
		}
	    }
	}
    }
return FALSE;
}

static boolean hideLittleOrphans(struct geneGraph *gg)
/* Get rid of exons which are soft edged on both sides and
 * completely enclosed by another exon. 
 * (The arcsForOverlaps doesn't handle these. */
{
int i,j;
int vCount = gg->vertexCount;
struct ggVertex *vertices = gg->vertices;
bool **em = gg->edgeMatrix;
boolean result = FALSE;
for (i=0; i<vCount; ++i)
    {
    if (vertices[i].type == ggSoftStart)
	{
	bool *waysOut = em[i];
	boolean anyLeft = FALSE;
	for (j=0; j<vCount; ++j)
	    {
	    if (waysOut[j])
		{
		if (vertices[j].type == ggSoftEnd && exonEnclosed(gg, i, j))
		    {
		    waysOut[j] = FALSE;
		    result = TRUE;
		    }
		else
		    anyLeft = TRUE;
		}
	    }
	if (!anyLeft)
	    {
	    vertices[i].type = ggUnused;
	    }
	}
    }
return result;
}

struct geneGraph *ggGraphCluster(struct ggMrnaCluster *mc, struct ggMrnaInput *ci)
/* Make up a gene transcript graph out of the ggMrnaCluster. */
{
struct geneGraph *gg = makeInitialGraph(mc, ci);
arcsForOverlaps(gg);
softlyTrim(gg);
hideLittleOrphans(gg);
return gg;
}

