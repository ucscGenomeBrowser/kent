/* txgToAgx - Convert from txg (txGraph) format to agx (altGraphX). */
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
struct altGraphX *ag;
AllocVar(ag);
ag->tName = cloneString(tx->tName);
ag->tStart = tx->tStart;
ag->tEnd = tx->tEnd;
ag->name = cloneString(tx->name);
ag->id = 0;
ag->strand[0] = tx->strand[0];
ag->vertexCount = tx->vertexCount;
ag->vTypes = CloneArray(tx->vTypes, tx->vertexCount);
ag->vPositions = CloneArray(tx->vPositions, tx->vertexCount);
ag->edgeCount = tx->edgeCount;
ag->edgeStarts = CloneArray(tx->edgeStarts, tx->edgeCount);
ag->edgeEnds = CloneArray(tx->edgeEnds, tx->edgeCount);
struct txEvList *evs;
for (evs = tx->evidence; evs != NULL; evs = evs->next)
    {
    struct evidence *ev;
    AllocVar(ev);
    ev->evCount = evs->evCount;
    int *mrnaIds = AllocArray(ev->mrnaIds, evs->evCount);
    int i;
    struct txEvidence *txEv = evs->evList;
    for (i=0; i<evs->evCount; ++i)
        {
	mrnaIds[i] = txEv->sourceId;
	txEv = txEv->next;
	}
    slAddHead(&ag->evidence, ev);
    }
slReverse(&ag->evidence);
ag->edgeTypes = CloneArray(tx->edgeTypes, tx->edgeCount);
ag->mrnaRefCount = tx->sourceCount;
AllocArray(ag->mrnaRefs, tx->sourceCount);
AllocArray(ag->mrnaTissues, tx->sourceCount);
AllocArray(ag->mrnaLibs, tx->sourceCount);
struct txSource *source;
int sourceIx = 0;
for (source = tx->sources; source != NULL; source = source->next)
    {
    ag->mrnaRefs[sourceIx] = cloneString(source->accession);
    ++sourceIx;
    }
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
