/* txgToAgx - Convert from txg (txGraph) format to agx (altGraphX). */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "altGraphX.h"
#include "txGraph.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txgToAgx - Convert from txg (txGraph) format to agx (altGraphX)\n"
  "usage:\n"
  "   txgToAgx in.txg out.agx\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct altGraphX *txGraphToAltGraphX(struct txGraph *tx)
/* Copy transcription graph to altSpliceX format. */
{
/* Allocate struct and deal with easy fields. */
struct altGraphX *ag;
AllocVar(ag);
ag->tName = cloneString(tx->tName);
ag->tStart = tx->tStart;
ag->tEnd = tx->tEnd;
ag->name = cloneString(tx->name);
ag->id = 0;
ag->strand[0] = tx->strand[0];

/* Deal with vertices. */
int vertexCount = ag->vertexCount = tx->vertexCount;
AllocArray(ag->vTypes, vertexCount);
AllocArray(ag->vPositions, vertexCount);
int i;
for (i=0; i<vertexCount; ++i)
    {
    struct txVertex *v = &tx->vertices[i];
    ag->vTypes[i] = v->type;
    ag->vPositions[i] = v->position;
    }

/* Deal with edges. */
int edgeCount = ag->edgeCount = tx->edgeCount;
AllocArray(ag->edgeStarts, edgeCount);
AllocArray(ag->edgeEnds, edgeCount);
AllocArray(ag->edgeTypes, edgeCount);
struct txEdge *edge;
for (edge = tx->edgeList, i=0; edge != NULL; edge = edge->next, ++i)
    {
    assert(i < edgeCount);
    ag->edgeStarts[i] = edge->startIx;
    ag->edgeEnds[i] = edge->endIx;
    ag->edgeTypes[i] = edge->type;
    }

/* Deal with evidence inside of edges. */
for (edge = tx->edgeList; edge != NULL; edge = edge->next)
    {
    struct evidence *ev;
    AllocVar(ev);
    int *mrnaIds = AllocArray(ev->mrnaIds, edge->evCount);
    int i;
    struct txEvidence *txEv;
    for (txEv = edge->evList, i=0; txEv != NULL; txEv = txEv->next, ++i)
        {
	assert(i < edge->evCount);
	struct txSource *source = &tx->sources[txEv->sourceId];
	char *sourceType = source->type;
	if (sameString(sourceType, "refSeq") || sameString(sourceType, "mrna") || sameString(sourceType, "est"))
	    {
	    mrnaIds[ev->evCount] = txEv->sourceId;
	    ev->evCount += 1;
	    }
        }
    slAddHead(&ag->evidence, ev);
    }
slReverse(&ag->evidence);

/* Convert sources into mrnaRefs. */
int sourceCount = ag->mrnaRefCount = tx->sourceCount;
AllocArray(ag->mrnaRefs, sourceCount);
int sourceIx;
for (sourceIx=0; sourceIx<sourceCount; ++sourceIx)
    {
    struct txSource *source = &tx->sources[sourceIx];
    ag->mrnaRefs[sourceIx] = cloneString(source->accession);
    }

/* Deal with tissues and libs by just making arrays of all zero. */
AllocArray(ag->mrnaTissues, tx->sourceCount);
AllocArray(ag->mrnaLibs, tx->sourceCount);
return ag;
}

void txgToAgx(char *inTxg, char *outAgx)
/* txgToAgx - Convert from txg (txGraph) format to agx (altGraphX). */
{
struct lineFile *lf = lineFileOpen(inTxg, TRUE);
char *row[TXGRAPH_NUM_COLS];
FILE *f = mustOpen(outAgx, "w");

while (lineFileRow(lf, row))
    {
    struct txGraph *txg = txGraphLoad(row);
    verbose(2, "loaded txGraph %s\n", txg->name);
    struct altGraphX *agx = txGraphToAltGraphX(txg);
    altGraphXTabOut(agx, f);
    altGraphXFree(&agx);
    txGraphFree(&txg);
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
txgToAgx(argv[1], argv[2]);
return 0;
}
