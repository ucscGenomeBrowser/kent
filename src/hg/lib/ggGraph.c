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
#include "hdb.h"

enum{ softEndBleedLimit = 5};  /* For soft ends allow some bleeding into next intron */

int ggEvidenceIx(struct ggEvidence *hayStack, struct ggEvidence *needle)
/* return index of needle in haystack or -1 if not present */
{
int count = 0;
struct ggEvidence *ge = NULL;
for(ge = hayStack; ge != NULL; ge = ge->next)
    {
    if(needle->id == ge->id)
	return count;
    count++;
    }
return -1;
}

void addEvidenceIfUnique(struct ggEvidence **currList, struct ggEvidence *newList)
/* add the new evidence to the old if not currently in in */
{
struct ggEvidence *ge = NULL, *geNext = NULL;
for(ge = newList; ge != NULL; ge = geNext)
    {
    int index = -1;
    geNext = ge->next;
    index = ggEvidenceIx(*currList, ge);
    if(index == -1)
	slSafeAddHead(currList, ge);
    else
	ggEvidenceFree(&ge);
    }
}

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

struct ggEvidence * ggEvidenceClone(struct ggEvidence *geList)
/* clone a list of evidence */
{
struct ggEvidence *retList = NULL, *ret=NULL;
struct ggEvidence *ge = NULL;
for(ge = geList; ge != NULL; ge = ge->next)
    {
    AllocVar(ret);
    ret->id = ge->id;
    slAddHead(&retList, ret);
    }
slReverse(&retList);
return retList;
}

int findMrnaIdByName(char *name, char **mrnaRefs, int count)
/* find the index of the name in the mrnaRef array, return -1 if not found */
{
int i;
for(i = 0; i < count ; ++i)
    {
    if(sameString(name, mrnaRefs[i]))
	return i;
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
struct geneGraph *gg = NULL;
bool **em;
int totalWithDupes = 0;
struct maRef *ref;
char **mrnaRefs = NULL;
int mrnaRefCount;
struct ggEvidence ***evM = NULL;
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
	/* if vOne not in vALL vix == -1 and we need to insert it */
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

AllocVar(gg);
/* Fill in other info from ci and mc. */
snprintf(gg->strand, sizeof(gg->strand), "%s", mc->strand);
gg->tName = cloneString(ci->tName);
gg->tStart = mc->tStart;
gg->tEnd = mc->tEnd;
gg->mrnaRefCount = mrnaRefCount = slCount(mc->refList);
gg->mrnaRefs = needMem(sizeof(char *) * mrnaRefCount);
for (ref = mc->refList, i=0; ref != NULL; ref = ref->next, ++i)
     gg->mrnaRefs[i] = cloneString(ref->ma->qName);

/* Allocate gene graph and edge matrix. Also the evidence matrix */

gg->vertexCount = vAllCount;
AllocArray(gg->vertices, vAllCount);
memcpy(gg->vertices, vAll, vAllCount*sizeof(*vAll));
gg->edgeMatrix = AllocArray(em, vAllCount);
gg->evidence = AllocArray(evM, vAllCount);
for (i=0; i<vAllCount; ++i)
    {
    AllocArray(em[i], vAllCount);
    AllocArray(evM[i], vAllCount);
    }

/* Fill in edge matrix. */
/* for each alignment in cluster create edges from each exon to the next,
 *  as we have seen them in mRNAs we know that they are valid splicings. 
 *  Note that the id's stored in the evidences index into the mrnaRefs array. 
 */
for (da = mc->mrnaList; da != NULL; da = da->next)
    {
    int countOne = da->vertexCount;
    struct ggVertex *vOne = da->vertices;
    int vix, lastVix;
    struct ggEvidence *ev = NULL;
    vix = vertexIx(vAll, vAllCount, vOne->position, vOne->type);
    for (i=1; i<countOne; ++i)
	{
	AllocVar(ev);
	++vOne;
	lastVix = vix;
	vix = vertexIx(vAll, vAllCount, vOne->position, vOne->type);
	assert(vix >= 0 && lastVix >= 0);
	em[lastVix][vix] = TRUE;
	ev->id = findMrnaIdByName(da->ma->qName, gg->mrnaRefs, gg->mrnaRefCount);
	if(ev->id < 0)
	    errAbort("ggGraph::makeInitialGraph() - Couldn't find %s in mrnalist.", da->ma->tName);
	slAddHead(&evM[lastVix][vix], ev); 
	}
    }
freeMem(vAll);

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
int bestStartIx = 0;
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
struct ggEvidence ***evid = gg->evidence;
for (endIx=0; endIx < vCount; ++endIx)
    {
    if (edgesOut[endIx])
	{
	int bestStart = findFurthestLinkedStart(gg, endIx);
	if (bestStart != softIx)
	    {
	    anyTrim = TRUE;
	    edgesOut[endIx] = FALSE;
	    addEvidenceIfUnique(&gg->evidence[bestStart][endIx], gg->evidence[softIx][endIx]);
//	    ggEvidenceFreeList(&gg->evidence[softIx][endIx]);
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
int bestEndIx = 0;
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
struct ggEvidence ***evid = gg->evidence;
for (startIx=0; startIx < vCount; ++startIx)
    {
    if (em[startIx][softIx])
	{
	int bestEnd = findFurthestLinkedEnd(gg, startIx);
	if (bestEnd != softIx)
	    {
	    anyTrim = TRUE;
	    em[startIx][softIx] = FALSE; // update evidence
	    addEvidenceIfUnique(&gg->evidence[startIx][bestEnd], gg->evidence[startIx][softIx]);
//	    ggEvidenceFreeList(&gg->evidence[startIx][softIx]);
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


static boolean arcsForSoftStartedExon(struct geneGraph *gg, int softStartIx, int endIx)
/* Put in arcs between hard start, and hard ends of any compatible
 * exons overlapping this one */
{
boolean result = FALSE;
int vCount = gg->vertexCount;
int sIx, eIx;
bool **em = gg->edgeMatrix;
struct ggVertex *vertices = gg->vertices;
int softStartPos = vertices[softStartIx].position;
int hardEndPos = vertices[endIx].position;
struct ggEvidence  ***evid = gg->evidence;
/* for each vertice */
for (eIx = 0; eIx < vCount; ++eIx)
    {
    struct ggVertex *ev = vertices+eIx;
    int type = ev->type;
    /* if the type is an end */
    if (type == ggSoftEnd || type == ggHardEnd)  
	{
	int evpos = ev->position;
	int softBleedAdjust = (type == ggHardEnd ? softEndBleedLimit : 0);
	/* if the position of the vertice is > the softStartPosition, assumes coordinates on + strand? */
	if (evpos > softStartPos)
            /* Why did jim comment this out? looks like now if there is a connection to any downstream exon we create the edge */
	    // if (evpos <= hardEndPos && evpos > softStartPos) 
	    {
	    for (sIx = 0; sIx < vCount; ++sIx)
		{
		if (sIx != softStartIx && em[sIx][eIx]) 
		    {
		    struct ggVertex *sv = vertices+sIx;
		    if (sv->position <= softStartPos + softBleedAdjust)
			{
			struct ggEvidence *ge = NULL;
			ge = ggEvidenceClone(gg->evidence[softStartIx][endIx]);
			em[sIx][endIx] = TRUE; 
			addEvidenceIfUnique(&gg->evidence[sIx][endIx], ge);
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
struct ggEvidence ***evid = gg->evidence;
for (i=0; i<vCount; ++i)
    {
    if (em[softStartIx][i])
	{
	int res;
	res = arcsForSoftStartedExon(gg, softStartIx, i);
	if (res)
	    {
	    em[softStartIx][i] = FALSE;
	    ggEvidenceFreeList(&gg->evidence[softStartIx][i]);
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

static boolean arcsForSoftEndedExon(struct geneGraph *gg, int startIx, int softEndIx)
/* Put in arcs between hard start, and hard ends of any compatible
 * exons overlapping this one */
{
boolean result = FALSE;
int vCount = gg->vertexCount;
int sIx, eIx;
bool **em = gg->edgeMatrix;
struct ggVertex *vertices = gg->vertices;
int startPos = vertices[startIx].position;
int softEndPos = vertices[softEndIx].position;
struct ggEvidence ***evid = gg->evidence;
for (sIx = 0; sIx < vCount; ++sIx)
    {
    struct ggVertex *sv = vertices+sIx;
    int type = sv->type;
    if (type == ggSoftStart || type == ggHardStart)
	{
	int svpos = sv->position;
	int softBleedAdjust = (type == ggHardStart ? softEndBleedLimit : 0);
	// if (svpos >= startPos && svpos < softEndPos)
	if (svpos < softEndPos)
	    {
	    for (eIx = 0; eIx < vCount; ++eIx)
		{
		if (eIx != softEndIx && em[sIx][eIx])
		    {
		    struct ggVertex *ev = vertices+eIx;
		    if (ev->position >= softEndPos - softBleedAdjust)
			{
			struct ggEvidence *ge = NULL;
			ge = ggEvidenceClone(gg->evidence[startIx][softEndIx]);
			em[startIx][eIx] = TRUE; 
			addEvidenceIfUnique(&gg->evidence[startIx][eIx], ge);
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
struct ggEvidence ***evid = gg->evidence;
for (i=0; i<vCount; ++i)
    {
    if (em[i][softEndIx])
	{
	int res;
	res = arcsForSoftEndedExon(gg, i, softEndIx);
	if (res)
	    {
	    em[i][softEndIx] = FALSE;
	    ggEvidenceFreeList(&gg->evidence[i][softEndIx]);
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
	    if(!matricesInSync(gg)) 
		errAbort("matrices are out of synch on vertex %d with ggSoftStart"); 	    
	    break;
	case ggSoftEnd:
	    result |= softEndArcs(gg, i);
	    if(!matricesInSync(gg)) 
		errAbort("matrices are out of synch on vertex %d with ggSoftEnd"); 	    
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
struct ggEvidence ***evid = gg->evidence;
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
		    ggEvidenceFreeList(&gg->evidence[i][j]);
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

void ggOutputDebug(struct geneGraph *gg, int offset)
/* convert to altGraphX and output to stdout */
{
struct altGraphX *ag = NULL;
ag =  ggToAltGraphX(gg);
altGraphXoffset(ag, offset);
altGraphXVertPosSort(ag);
altGraphXTabOut(ag, stdout);
altGraphXFree(&ag);
}

struct geneGraph *ggGraphCluster(struct ggMrnaCluster *mc, struct ggMrnaInput *ci)
/* Make up a gene transcript graph out of the ggMrnaCluster. */
{
struct sqlConnection *conn = hAllocConn();
struct geneGraph *gg = makeInitialGraph(mc, ci);
arcsForOverlaps(gg);
softlyTrim(gg);
hideLittleOrphans(gg);
ggFillInTissuesAndLibraries(gg, conn);
hFreeConn(&conn);
return gg;
}

