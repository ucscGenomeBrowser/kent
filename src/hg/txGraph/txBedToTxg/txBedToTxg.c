/* txBedToTxg - Create a transcription graph out of a bed-12 file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "options.h"
#include "bed.h"
#include "ggMrnaAli.h"
#include "geneGraph.h"
#include "dystring.h"
#include "txGraph.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txBedToTxg - Create a transcription graph out of a bed-12 file.\n"
  "usage:\n"
  "   txBedToTxg in.lst out.txg\n"
  "where in.lst is a file with two columns:\n"
  "    type - usually 'refSeq' or 'mrna' or 'est' or something\n"
  "    bed -  a bed file\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct ggMrnaAli *bedToMa(struct bed *bed, char *sourceType)
/* convert psl to format the clusterer likes. */
{
/* Figure out numerical as well as character representation of strand. */
char *strand = bed->strand;
int orientation = (strand[0] == '-' ? -1 : 1); 
int blockCount = bed->blockCount;
verbose(2, "bedToMa %s\n", bed->name);

/* Allocate structure and fill in all but blocks. */
struct ggMrnaAli *ma;
AllocVar(ma);
ma->orientation = orientation;
ma->qName = cloneString(bed->name);
ma->qStart = 0;
ma->qEnd = bed->chromEnd - bed->chromStart;
ma->baseCount = ma->qEnd;
ma->milliScore = bed->score;
snprintf(ma->strand, sizeof(ma->strand), "%s", strand);
ma->hasIntrons = (blockCount > 1);
ma->tName = cloneString(bed->chrom);
ma->tStart = bed->chromStart;
ma->tEnd = bed->chromEnd;
ma->sourceType = sourceType;

/* Deal with blocks. */
struct ggMrnaBlock *blocks, *block;
if (blockCount > 0)
    {
    ma->blockCount = blockCount;
    ma->blocks = AllocArray(blocks, blockCount);

    int i;
    for (i = 0; i<blockCount; ++i)
	{
	int bSize = bed->blockSizes[i];
	int tStart = bed->chromStarts[i] + bed->chromStart;
	int qStart = tStart;
	block = blocks+i;
	block->qStart = qStart;
	block->qEnd = qStart + bSize;
	block->tStart = tStart;
	block->tEnd = tStart + bSize;
	}
    }
else
    {
    /* If no block list, make single block. */
    ma->blockCount = 1;
    ma->blocks = AllocVar(block);
    block->qStart = block->tStart = bed->chromStart;
    block->qEnd = block->tEnd = bed->chromEnd;
    }
return ma;
}

struct ggMrnaAli *bedListToGgMrnaAliList(struct bed *bedList, char *sourceType)
/* Copy list of beds to list of ggMrnaAli. */
{
struct bed *bed;
struct ggMrnaAli *maList = NULL;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    struct ggMrnaAli *ma = bedToMa(bed, sourceType);
    slAddHead(&maList, ma);
    }
slReverse(&maList);
return maList;
}

static boolean vertexUsed(struct geneGraph *gg, int vertexIx)
/* Is the vertex attached to anything in the edgeMatrix? */
// JK - duplicate code from geneGraph.h.  Fix
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
// JK - duplicate code from geneGraph.h.  Fix
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

enum ggEdgeType ggClassifyEdge(struct geneGraph *gg, int v1, int v2);
/* Classify and edge as ggExon or ggIntron. ggExon is not
 * necessarily coding. */
// JK - duplicate code from geneGraph.h.  Fix

struct txGraph *txGraphFromGeneGraph(struct geneGraph *gg)
/* Convert a gene graph to an altGraphX data structure for storage in
 * database. */
{
struct txGraph *tg = NULL;
bool **em = gg->edgeMatrix;
int edgeCount = 0;
int totalVertexCount = gg->vertexCount;
int usedVertexCount;
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
AllocVar(tg);
snprintf(tg->strand, sizeof(tg->strand), "%s", gg->strand);
tg->tName = cloneString(gg->tName);
tg->tStart = gg->tStart;
tg->tEnd = gg->tEnd;
tg->name = cloneString("NA");
tg->vertexCount = usedVertexCount;
tg->vTypes = AllocArray(vTypes, usedVertexCount);
tg->vPositions = AllocArray(vPositions, usedVertexCount);
tg->sourceCount = gg->mrnaRefCount;

/* Cope with gg->mrnaRefs -> tg->sources conversion here. */
for (i=0; i<tg->sourceCount; ++i)
    {
    struct txSource *source;
    AllocVar(source);
    source->type = cloneString(gg->mrnaTypes[i]);
    source->accession = cloneString(gg->mrnaRefs[i]);
    slAddHead(&tg->sources, source);
    }
slReverse(&tg->sources);

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
tg->edgeCount = edgeCount;
AllocArray(tg->edgeStarts, edgeCount);
AllocArray(tg->edgeEnds, edgeCount);
AllocArray(tg->edgeTypes, edgeCount);
tg->evidence = NULL;
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
	{
	if (waysOut[j] && gg->vertices[j].type != ggUnused)
	    {
	    /* Store the mrna evidence. */
	    struct txEvList *evList;
	    AllocVar(evList);
	    evList->evCount = slCount(gg->evidence[i][j]);
	    // evList->mrnaIds = AllocArray(evList->mrnaIds, evList->evCount);
	    for(ev = gg->evidence[i][j]; ev != NULL; ev = ev->next)
		{
		struct txEvidence *txEv;
		AllocVar(txEv);
		txEv->sourceId = ev->id;
		txEv->start = ev->start;
		txEv->end = ev->end;
		slAddHead(&evList->evList, txEv);
		}
	    slReverse(&evList->evList);
	    slAddHead(&tg->evidence, evList);

	    /* Store the edge information. */
	    tg->edgeStarts[edgeCount] = translator[i];
	    tg->edgeEnds[edgeCount] = translator[j];
	    tg->edgeTypes[edgeCount] = ggClassifyEdge(gg, i, j);
	    ++edgeCount;
	    }
	}
    }
slReverse(&tg->evidence);
freeMem(translator);
return tg;
}


void txBedToTxg(char *inList, char *outGraph)
/* txBedToTxg - Create a transcription graph out of a bed-12 file.. */
{
struct lineFile *lf = lineFileOpen(inList, TRUE);
FILE *f = mustOpen(outGraph, "w");
char *row[2];
struct ggMrnaAli *maList = NULL;

/* Load in all beds. */
while (lineFileRow(lf, row))
    {
    char *type = cloneString(row[0]);
    char *inBed = row[1];
    struct bed *bedList = bedLoadAll(inBed);
    verbose(1, "Loaded %d beds from %s\n", slCount(bedList), inBed);
    maList = slCat(maList, bedListToGgMrnaAliList(bedList, type));
    }
verbose(2, "Created %d ma's\n", slCount(maList));

struct ggMrnaInput *ci = ggMrnaInputFromAlignments(maList, NULL);
struct ggMrnaCluster *mc, *mcList = ggClusterMrna(ci);
verbose(1, "Reduced to %d clusters\n", slCount(mcList));
for (mc = mcList; mc != NULL; mc = mc->next)
    {
    struct geneGraph *gg = ggGraphConsensusCluster(mc, ci, NULL, FALSE);
    struct txGraph *tg = txGraphFromGeneGraph(gg);
    if (tg != NULL)
	{
	static int id=0;
	char name[16];
	safef(name, sizeof(name), "a%d", ++id);
	verbose(3, "writing %s\n", name);
	freez(&tg->name);
	tg->name = name;
	txGraphTabOut(tg, f);
	tg->name = NULL;
	}
    freeGeneGraph(&gg);
    txGraphFree(&tg);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
// pushCarefulMemHandler(500000000);
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
txBedToTxg(argv[1], argv[2]);
return 0;
}
