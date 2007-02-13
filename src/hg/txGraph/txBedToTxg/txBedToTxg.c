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

int maxJoinSize = 70000;	/* This excludes most of the chr14 IG mess */
boolean forceRefSeqJoin = TRUE;
char *prefix = "a";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txBedToTxg - Create a transcription graph out of a bed-12 file.\n"
  "usage:\n"
  "   txBedToTxg in1.bed in1Type [in2.bed in2type ...]  out.txg\n"
  "where in.lst is a file with two columns:\n"
  "    type - usually 'refSeq' or 'mrna' or 'est' or something\n"
  "    bed -  a bed file\n"
  "options:\n"
  "    -maxJoinSize=N - Maximum size of gap to join for introns with\n"
  "                    noisy ends. Default %d.\n"
  "    -noForceRefSeqJoin - If set don't force joins above maxJoinSize\n"
  "                    on refSeq type\n"
  "    -prefix=xyz - Use the given prefix for the graph names, default %s\n"
  , maxJoinSize, prefix
  );
}

static struct optionSpec options[] = {
   {"maxJoinSize", OPTION_INT},
   {"noForceRefSeqJoin", OPTION_BOOLEAN},
   {"prefix", OPTION_STRING},
   {NULL, 0},
};

struct ggMrnaAli *bedListToMa(struct bed *startBed, struct bed *endBed, 
	char *sourceType)
/* convert psl to format the clusterer likes. */
{
/* Total up all blocks in list. */
struct bed *bed;
int blockCount = 0;
int chromStart = startBed->chromStart;
int chromEnd = startBed->chromEnd;
int qStart = 0;
for (bed = startBed; bed != endBed; bed = bed->next)
    {
    if (bed->blockCount == 0)
        errAbort("txBedToTxg needs to be run on beds with blocks (bed12)");
    blockCount += bed->blockCount;
    chromEnd = bed->chromEnd;
    }

/* Figure out numerical as well as character representation of strand. */
char *strand = startBed->strand;
int orientation = (strand[0] == '-' ? -1 : 1); 
verbose(2, "bedListToMa %s\n", startBed->name);

/* Allocate structure and fill in all but blocks. */
struct ggMrnaAli *ma;
AllocVar(ma);
ma->orientation = orientation;
ma->qName = cloneString(startBed->name);
ma->qStart = 0;
ma->qEnd = chromEnd - chromStart;
ma->baseCount = ma->qEnd;
ma->milliScore = startBed->score;
ma->strand[0] = strand[0];
ma->hasIntrons = (blockCount > 1);
ma->tName = cloneString(startBed->chrom);
ma->tStart = chromStart;
ma->tEnd = chromEnd;
ma->sourceType = sourceType;

/* Deal with blocks. */
struct ggMrnaBlock *blocks, *block;
ma->blockCount = blockCount;
ma->blocks = block = AllocArray(blocks, blockCount);
for (bed = startBed;  bed != endBed; bed = bed->next)
    {
    boolean isFirstBed = (bed == startBed);
    boolean isLastBed = (bed->next == endBed);
    int i;
    for (i=0; i<bed->blockCount; ++i)
	{
	boolean isFirstBlock = (i == 0);
	boolean isLastBlock = (i == bed->blockCount-1);

	/* Do the basic block. */
	int bSize = bed->blockSizes[i];
	int tStart = bed->chromStarts[i] + bed->chromStart;
	block->qStart = qStart;
	qStart += bSize;
	block->qEnd = qStart;
	block->tStart = tStart;
	block->tEnd = tStart + bSize;

	/* Possibly soften ends */
	if (isLastBlock && !isLastBed)
	    {
	    block->qEnd -= 1;
	    block->tEnd -= 1;
	    }
	if (isFirstBlock && !isFirstBed)
	    {
	    block->qStart += 1;
	    block->tStart += 1;
	    }
	block += 1;
	}
    }
return ma;
}

struct ggMrnaAli *bedListToGgMrnaAliList(struct bed *bedList, char *sourceType)
/* Copy list of beds to list of ggMrnaAli. */
{
struct bed *startBed, *bed, *endBed;
struct ggMrnaAli *maList = NULL;
boolean bigGapOk = (sameString(sourceType, "refSeq") && forceRefSeqJoin);
for (startBed = bedList; startBed != NULL; startBed = endBed)
    {
    /* Figure out first bed with different name, or that otherwise can't be
     * a continuation of previous bed. */
    struct bed *lastBed = startBed;
    for (bed = startBed->next; bed != NULL; bed = bed->next)
        {
	if ( lastBed->chromEnd >= bed->chromStart || !sameString(lastBed->name, bed->name) 
		|| bed->strand[0] != lastBed->strand[0] 
		|| !sameString(bed->chrom, lastBed->chrom) )
	    break;
	if (!bigGapOk && lastBed->chromEnd - bed->chromStart > maxJoinSize)
	    break;
	lastBed = bed;
	}
    endBed = bed;

    struct ggMrnaAli *ma = bedListToMa(startBed, endBed, sourceType);
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

struct txGraph *txGraphFromGeneGraph(struct geneGraph *gg, char *name)
/* Convert a gene graph to an altGraphX data structure for storage in
 * database. */
{
struct txGraph *tg = NULL;
bool **em = gg->edgeMatrix;
int totalVertexCount = gg->vertexCount;
int usedVertexCount;
int *translator;	/* Translates away unused vertices. */
struct ggVertex *ggVertices = gg->vertices;
struct ggEvidence *ev = NULL;
int i,j;

/* We'll just write out the used vertices. Find them. */
AllocArray(translator, totalVertexCount);
usedVertexCount = countUsed(gg, totalVertexCount, translator);

AllocVar(tg);
snprintf(tg->strand, sizeof(tg->strand), "%s", gg->strand);
tg->tName = cloneString(gg->tName);
tg->tStart = gg->tStart;
tg->tEnd = gg->tEnd;
tg->name = cloneString(naForNull(name));
tg->vertexCount = usedVertexCount;
AllocArray(tg->vertices, usedVertexCount);
tg->sourceCount = gg->mrnaRefCount;
AllocArray(tg->sources, tg->sourceCount);

/* Cope with gg->mrnaRefs -> tg->sources conversion here. */
for (i=0; i<tg->sourceCount; ++i)
    {
    struct txSource *source = &tg->sources[i];
    source->type = cloneString(gg->mrnaTypes[i]);
    source->accession = cloneString(gg->mrnaRefs[i]);
    }

/* convert vertexes */
for (i=0,j=0; i<totalVertexCount; ++i)
    {
    struct ggVertex *v = ggVertices+i;
    if (v->type != ggUnused && vertexUsed(gg, i))
	{
	struct txVertex *tv = &tg->vertices[j];
	tv->type = v->type;
	tv->position = v->position;
	++j;
	}
    }

/* convert edges */
for (i=0; i<totalVertexCount; ++i)
    {
    bool *waysOut = em[i];
    if(ggVertices[i].type == ggUnused)
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
	if (waysOut[j] && ggVertices[j].type != ggUnused)
	    {
	    /* Allocate edge and store basic info. */
	    struct txEdge *edge;
	    AllocVar(edge);
	    edge->startIx = translator[i];
	    edge->endIx = translator[j];
	    edge->type = ggClassifyEdge(gg, i, j);

	    /* Store the mrna evidence. */
	    for(ev = gg->evidence[i][j]; ev != NULL; ev = ev->next)
		{
		struct txEvidence *txEv;
		AllocVar(txEv);
		txEv->sourceId = ev->id;
		txEv->start = ev->start;
		txEv->end = ev->end;
		slAddHead(&edge->evList, txEv);
		edge->evCount += 1;
		}
	    slReverse(&edge->evList);

	    /* Add edge to graph. */
	    tg->edgeCount += 1;
	    slAddHead(&tg->edges, edge);
	    }
	}
    }
slReverse(&tg->edges);
freeMem(translator);
return tg;
}


void txBedToTxg(char *outGraph, char **inPairs, int inCount)
/* txBedToTxg - Create a transcription graph out of a bed-12 file.. */
{
FILE *f = mustOpen(outGraph, "w");
struct ggMrnaAli *maList = NULL;
int i;

/* Load all bed files and convert to ggMrnaAli. */
for (i=0; i<inCount; ++i)
    {
    char *inBed = *inPairs++;
    char *type = *inPairs++;
    struct bed *bedList = bedLoadAll(inBed);
    verbose(1, "Loaded %d beds from %s\n", slCount(bedList), inBed);
    maList = slCat(maList, bedListToGgMrnaAliList(bedList, type));
    }
verbose(2, "Created %d ma's\n", slCount(maList));

if (maList != NULL)
    {
    struct ggMrnaInput *ci = ggMrnaInputFromAlignments(maList, NULL);
    struct ggMrnaCluster *mc, *mcList = ggClusterMrna(ci);
    verbose(1, "Reduced to %d clusters\n", slCount(mcList));
    for (mc = mcList; mc != NULL; mc = mc->next)
	{
	static int id=0;
	char name[256];
	safef(name, sizeof(name), "%s%d", prefix, ++id);
	struct geneGraph *gg = ggGraphConsensusCluster(mc, ci, NULL, FALSE);
	struct txGraph *tg = txGraphFromGeneGraph(gg, name);
	if (tg != NULL)
	    txGraphTabOut(tg, f);
	freeGeneGraph(&gg);
	txGraphFree(&tg);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
// pushCarefulMemHandler(500000000);
optionInit(&argc, argv, options);
maxJoinSize = optionInt("maxJoinSize", maxJoinSize);
forceRefSeqJoin = !optionExists("noForceRefSeqJoin");
prefix = optionVal("prefix", prefix);
if (argc < 4 || argc%2 != 0)
    usage();
txBedToTxg(argv[argc-1], argv+1, argc/2-1);
return 0;
}
