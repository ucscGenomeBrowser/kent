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
#include "altGraphX.h"
#include "geneGraph.h"
#include "dystring.h"
#include "genbank.h"

void ggEvidenceFree(struct ggEvidence **pEl)
/* Free a single dynamically allocated ggEvidence */
{
struct ggEvidence *el;

if ((el = *pEl) == NULL) return;
freez(pEl);
}

void ggEvidenceFreeList(struct ggEvidence **pList)
/* Free a list of dynamically allocated ggEvidence's */
{
struct ggEvidence *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ggEvidenceFree(&el);
    }
*pList = NULL;
}

void freeGeneGraph(struct geneGraph **pGg)
/* Free up a gene graph. */
{
struct geneGraph *gg;

if ((gg = *pGg) != NULL)
    {
    int i, j, vcount;
    bool **em = gg->edgeMatrix;
    vcount = gg->vertexCount;
    for (i=0; i<vcount; ++i)
	{
	freez(&em[i]);
	if(gg->evidence != NULL)
	    {
	    for(j=0; j<vcount; ++j)
		ggEvidenceFreeList(&gg->evidence[i][j]);
	    freez(&gg->evidence[i]);
	    }
	}
    if(gg->evidence != NULL)
	freez(&gg->evidence);
    for(i=0; i<gg->mrnaRefCount; i++)
	{
	freez(&gg->mrnaRefs[i]);
	}
    freez(&gg->mrnaRefs);
    freez(&gg->mrnaTypes);
    freez(&gg->tName);
    freez(&gg->mrnaTissues);
    freez(&gg->mrnaLibs);
    freez(&gg->evidence);
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

boolean vertexUsed(struct geneGraph *gg, int vertexIx)
/* Is the vertex attached to anything in the edgeMatrix? */
{
bool **em = gg->edgeMatrix;
int vC = gg->vertexCount;
int i=0;
/* First check the row. */
for(i=0; i<vC; i++)
    if(em[vertexIx][i])
	return TRUE;
/* Now check the column. */
for(i=0; i<vC; i++)
    if(em[i][vertexIx])
	return TRUE;

/* Guess it isn't used. */
return FALSE;
}

static int countUsed(struct geneGraph *gg, int vCount, int *translator)
/* Count number of vertices actually used. */
{
int i;
int count = 0;
struct ggVertex *v = gg->vertices;
for (i=0; i<vCount; ++i)
    {
    translator[i] = count;
    if (v[i].type != ggUnused && vertexUsed(gg,i))
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
int *translator;	/* Translates away unused vertices. */
struct ggVertex *vertices = gg->vertices;
int i,j;
UBYTE *vTypes;
int *vPositions, *edgeStarts, *edgeEnds;

AllocArray(translator, totalVertexCount);
usedVertexCount = countUsed(gg, totalVertexCount, translator);
for (i=0; i<totalVertexCount; ++i)
    {
    bool *waysOut = em[i];
    for (j=0; j<totalVertexCount; ++j)
	if (waysOut[j])
	    ++edgeCount;
    }
AllocVar(ag);
safef(ag->strand, sizeof(ag->strand), "%s", gg->strand);
ag->tName = cloneString(gg->tName);
ag->tStart = gg->tStart;
ag->tEnd = gg->tEnd;
ag->vertexCount = usedVertexCount;
ag->vTypes = AllocArray(vTypes, usedVertexCount);
ag->vPositions = AllocArray(vPositions, usedVertexCount);
ag->mrnaRefCount = gg->mrnaRefCount;
AllocArray(ag->mrnaRefs, gg->mrnaRefCount);
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

void ggPrintHeader(int count)
/* Print a header with numbers for vertices. Assumes less than 100 edges,
 * used for debugging output. */
{
int i;
printf("   ");
for(i=0; i<count; i++)
    {
    if(i != 0 && i % 10 == 0)
	printf("%d", i/10);
    else
	printf(" ");
    }
printf("\n   ");
for(i=0; i<count; i++)
    printf("%d", i % 10);
printf("\n   ");
for(i=0; i<count; i++)
    printf("-");
printf("\n");
}

void ggPrintStart(int count)
/* print a number plus bar, i.e. ' 0|'. Used for debugging output. */
{
if(count % 10 == 0 && count != 0)
    printf("%d", count/10);
else
    printf(" ");
printf("%d", count%10);
printf("|");
}

void ggEvidenceDebug(struct geneGraph *gg)
/* Dump out the edge matrix and evidence matrix for a genegraph as 1 and 0's. */
{
int i, j;
int totalVertexCount = gg->vertexCount;
printf("EdgeMatrix:\n");
ggPrintHeader(totalVertexCount);
for(i=0; i<totalVertexCount; ++i)
    {
    ggPrintStart(i);
    for(j=0; j<totalVertexCount; ++j)
	{
	printf("%d",gg->edgeMatrix[i][j]);
	}
    printf("\n");
    }
printf("evidence matrix:\n");
ggPrintHeader(totalVertexCount);
for(i=0; i<totalVertexCount; ++i)
    {
    ggPrintStart(i);
    for(j=0; j<totalVertexCount; ++j)
	{
	printf("%d", (gg->evidence[i][j] == NULL ? 0 : 1));
	}
    printf("\n");
    }
printf("difference:\n");
ggPrintHeader(totalVertexCount);
for(i=0; i<totalVertexCount; ++i)
    {
    ggPrintStart(i);
    for(j=0; j<totalVertexCount; ++j)
	{
	if((gg->evidence[i][j] == NULL && gg->edgeMatrix[i][j] == 1))
	    printf("m");
	else if  (gg->evidence[i][j] != NULL && gg->edgeMatrix[i][j] == 0)
	    printf("e");
	else
	    printf("0");
	}
    printf("\n");
    }
}

boolean checkEvidenceMatrix(struct geneGraph *gg)
/* Check to make sure that no edge has more weight than possible.
 * Used for testing. */
{
boolean allOk = TRUE;
int i,j,eCount;
struct ggEvidence *ge = NULL;
for(i =0; i<gg->vertexCount; i++)
    {
    for(j=0; j<gg->vertexCount; j++)
	{
	ge = gg->evidence[i][j];
	eCount = slCount(ge);
	if(eCount > gg->mrnaRefCount)
	    {
	    warn("Too many rnas for edge at %d-%d. Max is %d, counted %d.", i, j, gg->mrnaRefCount, eCount );
	    allOk = FALSE;
	    }
	}
    }
return allOk;
}

boolean matricesInSync(struct geneGraph *gg)
/* Return TRUE if edge and evidence matrices are in synch, FALSE otherwise. 
 * Used for testing purposes. */
{
boolean sync = TRUE;
int i,j;
int totalVertexCount = gg->vertexCount;
for(i=0; i<totalVertexCount; ++i)
    {
    for(j=0; j<totalVertexCount; ++j)
	{
	if((gg->evidence[i][j] == NULL && gg->edgeMatrix[i][j] == 1))
	    sync = FALSE;
	else if  (gg->evidence[i][j] != NULL && gg->edgeMatrix[i][j] == 0)
	    sync = FALSE;
	}
    }
return sync;
}

void ggFillInTissuesAndLibraries(struct geneGraph *gg, struct hash *tissLibHash,
				 struct sqlConnection *conn)
/* Load up the library and tissue information for mrnas from the mrna table. 
   If the tissLibHash != NULL use it to find the library and tissue. They
   will be stored as an slInt list keyed by the accessions. */
{
int i;
int mrnaCount = gg->mrnaRefCount;
AllocArray(gg->mrnaTissues, mrnaCount);
gg->mrnaLibs = needMem(sizeof(int)*mrnaCount);
//gg->mrnaLibs = AllocArray(gg->mrnaLibs, mrnaCount);
for(i=0; i< mrnaCount; ++i)
    {
    /* Look in the hash if provided first. */
    if(tissLibHash != NULL)
	{
	struct slInt *library = NULL, *tissue=NULL;
	library = hashMustFindVal(tissLibHash, gg->mrnaRefs[i]);
	gg->mrnaLibs[i] = library->val;
	tissue = library->next;
	assert(tissue);
	gg->mrnaTissues[i] = tissue->val;
	}
    else
	{
	
	struct sqlResult *sr = NULL;
	char **row = NULL;
	char query[256];
	assert(gg->mrnaRefs[i]);
	sqlSafef(query, sizeof(query), "select library, tissue from %s where acc='%s'", gbCdnaInfoTable, gg->mrnaRefs[i]);
	sr = sqlGetResult(conn, query);
	row = sqlNextRow(sr);
	if(row == NULL)
	    errAbort("geneGraph.c::ggFillInTissuesAndLibraries() - Couldn't load library and tissue info for est: %s using query:\n%s", gg->mrnaRefs[i], query);
	gg->mrnaLibs[i] = sqlSigned(row[0]);
	gg->mrnaTissues[i] = sqlSigned(row[1]);
	sqlFreeResult(&sr);
	}
    }
}

boolean isSameGeneGraph(struct geneGraph *gg1, struct geneGraph *gg2)
/* Returns true if the gene graphs are the same, otherwise returns false. */
{
boolean allOk = TRUE;
int i,j;
if(gg1->vertexCount != gg2->vertexCount)
    return FALSE;
if(gg1->mrnaRefCount != gg2->mrnaRefCount)
    return FALSE;
if(differentString(gg1->tName, gg2->tName))
    allOk = FALSE;
if(gg1->tStart != gg2->tStart)
    allOk = FALSE;
if(gg1->tEnd != gg2->tEnd)
    allOk = FALSE;
if(differentString(gg1->strand,gg2->strand))
    allOk = FALSE;

for(i=0; i<gg1->vertexCount; i++)
    for(j=0; j<gg1->vertexCount; j++)
	{
	int ev1,ev2;
	if(gg1->edgeMatrix[i][j] != gg2->edgeMatrix[i][j])
	    allOk = FALSE;
        ev1 = slCount(gg1->evidence[i][j]);
	ev2 = slCount(gg2->evidence[i][j]);
	if(ev1 != ev2)
	    allOk = FALSE;
	}
for(i=0; i<gg1->vertexCount; i++)
    {
    if(gg1->vertices[i].type != gg2->vertices[i].type)
	allOk = FALSE;
    if(gg1->vertices[i].position != gg2->vertices[i].position)
	allOk = FALSE;
    }

for(i=0; i<gg1->mrnaRefCount; i++)
    {
    if(differentString(gg1->mrnaRefs[i], gg2->mrnaRefs[i]))
	allOk = FALSE;
    if(gg1->mrnaTissues[i] != gg2->mrnaTissues[i])
	allOk = FALSE;
    if(gg1->mrnaLibs[i] != gg2->mrnaLibs[i])
	allOk = FALSE;
    }
return allOk;
}

enum ggEdgeType ggClassifyEdge(struct geneGraph *gg, int v1, int v2)
/* Classify and edge as ggExon or ggIntron. ggExon is not
 * necessarily coding. */
{
struct ggVertex *vertices = gg->vertices;
enum ggEdgeType et = 0;
/* Make sure the indexes are in bounds and edge exists. */
assert(v1 < gg->vertexCount && v2 < gg->vertexCount && gg->edgeMatrix[v1][v2]);

if( (vertices[v1].type == ggHardStart || vertices[v1].type == ggSoftStart) &&
    (vertices[v2].type == ggHardEnd || vertices[v2].type == ggSoftEnd))
    et =  ggExon;
else if((vertices[v1].type == ggHardEnd || vertices[v1].type == ggSoftEnd) &&
    (vertices[v2].type == ggHardStart || vertices[v2].type == ggSoftStart))
    et = ggIntron;
else
    errAbort("geneGraph::ggClassifyEdge() - "
	     "Edge from %d -> %d has types %d-%d which isn't recognized as intron or exon.\n",
	     v1, v2, vertices[v1].type, vertices[v2].type);
#ifdef NEVER
// What the heck? Flipping inton/exon on strand???? */
if(sameString(gg->strand, "-"))
    {
    if(et == ggExon)
	et = ggIntron;
    else if(et == ggIntron)
	et = ggExon;
    }
#endif /* NEVER */
return et;
}


struct altGraphX *ggToAltGraphX(struct geneGraph *gg)
/* Convert a gene graph to an altGraphX data structure for storage in
 * database. */
{
struct altGraphX *ag = NULL;
bool **em = gg->edgeMatrix;
int edgeCount = 0;
int totalVertexCount = gg->vertexCount;
int usedVertexCount;
struct dyString *accessionList = NULL;
int *translator;	/* Translates away unused vertices. */
struct ggVertex *vertices = gg->vertices;
struct ggEvidence *ev = NULL;
int i,j;
UBYTE *vTypes;
int *vPositions;

AllocArray(translator, totalVertexCount);
usedVertexCount = countUsed(gg, totalVertexCount, translator);
for (i=0; i<totalVertexCount; ++i)
    {
    bool *waysOut = em[i];
    for (j=0; j<totalVertexCount; ++j)
	if (waysOut[j] && gg->vertices[j].type != ggUnused)
	    ++edgeCount;
    }
AllocVar(ag);
safef(ag->strand, sizeof(ag->strand), "%s", gg->strand);
ag->tName = cloneString(gg->tName);
ag->tStart = gg->tStart;
ag->tEnd = gg->tEnd;
ag->name = cloneString("NA");
ag->vertexCount = usedVertexCount;
ag->vTypes = AllocArray(vTypes, usedVertexCount);
ag->vPositions = AllocArray(vPositions, usedVertexCount);
ag->mrnaRefCount = gg->mrnaRefCount;
accessionList = newDyString(10*gg->mrnaRefCount);
/* Have to print the accessions all out in the same string to conform
   to how the memory is handled later. */
for(i=0; i<gg->mrnaRefCount; i++)
    dyStringPrintf(accessionList, "%s,", gg->mrnaRefs[i]);
sqlStringDynamicArray(accessionList->string, &ag->mrnaRefs, &ag->mrnaRefCount);
dyStringFree(&accessionList);
if(gg->mrnaRefCount > 0)
    {
    ag->mrnaTissues = CloneArray(gg->mrnaTissues, gg->mrnaRefCount);
    ag->mrnaLibs = CloneArray(gg->mrnaLibs, gg->mrnaRefCount);
    }

/* convert vertexes */
for (i=0,j=0; i<totalVertexCount; ++i)
    {
    struct ggVertex *v = vertices+i;
    if (v->type != ggUnused && vertexUsed(gg, i))
	{
	vTypes[j] = v->type;
	vPositions[j] = v->position;
	++j;
	}
    }

/* convert edges */
ag->edgeCount = edgeCount;
AllocArray(ag->edgeStarts, edgeCount);
AllocArray(ag->edgeEnds, edgeCount);
AllocArray(ag->edgeTypes, edgeCount);
ag->evidence = NULL;
edgeCount = 0;
for (i=0; i<totalVertexCount; ++i)
    {
    bool *waysOut = em[i];
    if(gg->vertices[i].type == ggUnused)
	{	
	for (j=0; j<totalVertexCount; ++j)
	    {
	    if(gg->edgeMatrix[i][j])
		errAbort("Shouldn't be any connections to an unused vertex %d-%d.", i, j);
	    }
	continue;
	}
    for (j=0; j<totalVertexCount; ++j)
	if (waysOut[j] && gg->vertices[j].type != ggUnused)
	    {
	    struct evidence *e;
	    int count =0;
	    /* Store the mrna evidence. */
	    AllocVar(e);
	    e->evCount = slCount(gg->evidence[i][j]);
	    AllocArray(e->mrnaIds, e->evCount);
	    for(ev = gg->evidence[i][j]; ev != NULL; ev = ev->next)
		{
		assert(count < e->evCount);
		e->mrnaIds[count++] = ev->id;
		}
	    slAddHead(&ag->evidence, e);

	    /* Store the edge information. */
	    ag->edgeStarts[edgeCount] = translator[i];
	    ag->edgeEnds[edgeCount] = translator[j];
	    ag->edgeTypes[edgeCount] = ggClassifyEdge(gg, i, j);
	    ++edgeCount;
	    }
    }
slReverse(&ag->evidence);
freeMem(translator);
return ag;
}

struct geneGraph *altGraphXToGG(struct altGraphX *ag)
/* Convert an altGraphX to a geneGraph. Free with freeGeneGraph */
{
struct geneGraph *gg = NULL;
int i,j;
AllocVar(gg);
gg->tName = cloneString(ag->tName);
gg->tStart = ag->tStart;
gg->tEnd = ag->tEnd;
gg->vertexCount = ag->vertexCount;
safef(gg->strand, sizeof(gg->strand), "%s", ag->strand);
    gg->mrnaRefCount = ag->mrnaRefCount;
gg->mrnaTissues = CloneArray(ag->mrnaTissues, ag->mrnaRefCount);
gg->mrnaLibs = CloneArray(ag->mrnaLibs, ag->mrnaRefCount);
AllocArray(gg->mrnaRefs, gg->mrnaRefCount);
for(i=0; i<gg->mrnaRefCount; i++)
    gg->mrnaRefs[i] = cloneString(ag->mrnaRefs[i]);
gg->edgeMatrix = altGraphXCreateEdgeMatrix(ag);  /* will be free'd when geneGraph free'd */

AllocArray(gg->vertices, gg->vertexCount);
for(i=0; i<gg->vertexCount; i++)
    {
    gg->vertices[i].position = ag->vPositions[i];
    gg->vertices[i].type = ag->vTypes[i];
    }
AllocArray(gg->evidence, gg->vertexCount);
for(i=0; i<gg->vertexCount; i++)
    AllocArray(gg->evidence[i], gg->vertexCount);

for(i=0; i < ag->edgeCount; i++)
    {
    int x = ag->edgeStarts[i];
    int y = ag->edgeEnds[i];
    struct evidence *e = slElementFromIx(ag->evidence, i);
    for(j=0; j<e->evCount; j++)
	{
	struct ggEvidence *ge = NULL;
	AllocVar(ge);
	ge->id = e->mrnaIds[j];
	ge->start = gg->vertices[x].position;
	ge->end = gg->vertices[y].position;
	ggEvAddHeadWrapper(&gg->evidence[x][y], &ge);
	}
    slReverse(&gg->evidence[x][y]);
    }
return gg;
}
   
boolean ggIsCassetteExonEdge(struct geneGraph *gg, int vertex1, int vertex2)
/* Return TRUE if there is evidence that this exon is optional
 * or FALSE otherwise.  */
{
int i,j;
struct ggVertex *vertices = NULL;         /* shorthand for vertices */
int vCount = 9;                           /* shorthand for vertexCount */
bool **em = NULL;                         /* shorthand for edgeMatrix */
boolean cassette = FALSE;                 /* final result, is this a cassette exon? */
int *sCandidates = NULL;                  /* array of vertices that connect to vertex1 */
int sCount = 0;                           /* count of vertices that connect to vertex1 */
int *eCandidates = NULL;                  /* array of vertices that vertex2 connects to */
int eCount = 0;                           /* count of vertices that vertex2 connects to */
assert(gg);
vertices = gg->vertices;
vCount = gg->vertexCount;
em = gg->edgeMatrix; 
AllocArray(sCandidates, vCount);
AllocArray(eCandidates, vCount);
if(vertices[vertex1].type != ggHardStart && vertices[vertex1].type != ggSoftStart)
    {
    warn("geneGraph::ggIsCassetteExonEdge() - Hard to be an exon when at %s:%d-%d vertex 1 isn't a start", 
	 gg->tName, vertices[vertex1].position, vertices[vertex2].position);
    return FALSE;
    }
if(vertices[vertex2].type != ggHardEnd && vertices[vertex2].type != ggSoftEnd)
    {
    warn("geneGraph::ggIsCassetteExonEdge() - Hard to be an exon when at %s:%d-%d vertex 2 isn't an end", 
	 gg->tName, vertices[vertex2].position, vertices[vertex2].position);
    return FALSE;
    }

/* find all of our candidates that link to the exon */
for(i=0; i<vCount; i++)
    {
    if(i != vertex1 && em[i][vertex1])
	{
	assert(sCount < vCount);
	sCandidates[sCount++] = i;
	}
    }
/* find coordinate of what the exon links to */
for(i=0; i<vCount; i++)
    {
    if(i != vertex2 && em[vertex2][i])
	{
	assert(eCount < vCount);
	eCandidates[eCount++] = i;
	}
    }

/* check to see if the starts connect to the ends */
for(i=0; i< sCount && !cassette; i++)
    {
    for(j=0; j<eCount && !cassette; j++)
	{
	if(em[sCandidates[i]][eCandidates[j]])
	    cassette=TRUE;
	}
    }
freez(&eCandidates);
freez(&sCandidates);
return cassette;
}

struct ggEdge *ggFindCassetteExons(struct geneGraph *gg)
/* Return a list of edges that appear to be cassette exons. */
{
bool **em;
int vCount = 0;
struct ggVertex *vertices = NULL;
struct ggEdge *edgeList = NULL, *edge = NULL;
int i, j;
assert(gg);
vertices = gg->vertices;
vCount = gg->vertexCount;
em = gg->edgeMatrix; 
for(i=0; i<vCount; i++)
    {
    for(j=0; j<vCount; j++)
	{
	if(i != j && 
	   em[i][j] && 
	   (vertices[i].type == ggHardStart) &&
	   (vertices[j].type == ggHardEnd))
	    {
	    if(ggIsCassetteExonEdge(gg, i, j))
		{
		AllocVar(edge);
		edge->vertex1 = i;
		edge->vertex2 = j;
		edge->type = ggExon;
		slAddHead(&edgeList, edge);
		}
	    }
	}
    }
return edgeList;
}

struct ggEdge *ggCreateEdge(int v1, int v2, int type)
/* create and return and graph edge, free with freez(). */
{
struct ggEdge *edge;
AllocVar(edge);
edge->vertex1 = v1;
edge->vertex2 = v2;
edge->type = type;
return edge;
}
