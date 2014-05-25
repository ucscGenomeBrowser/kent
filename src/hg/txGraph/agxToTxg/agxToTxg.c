/* agxToTxg - Convert from old altGraphX format to newer txGraph format.. */

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
  "agxToTxg - Convert from old altGraphX format to newer txGraph format.\n"
  "usage:\n"
  "   agxToTxg in.agx out.txg\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct txGraph *txgFromAltGraphX(struct altGraphX *ag)
/* Create txGraph that is as nearly possible the same ag altGraphX */
{
struct txGraph *txg;
AllocVar(txg);
txg->tName = cloneString(ag->tName);
txg->tStart = ag->tStart;
txg->tEnd = ag->tEnd;
txg->name = cloneString(ag->name);
txg->strand[0] = ag->strand[0];

/* Deal with vertices. */
int vertexCount = txg->vertexCount = ag->vertexCount;
AllocArray(txg->vertices, vertexCount);
int i;
for (i=0; i<vertexCount; ++i)
    {
    struct txVertex *v = &txg->vertices[i];
    v->type = ag->vTypes[i];
    v->position = ag->vPositions[i];
    }

/* Deal with edges. */
int edgeCount = txg->edgeCount = ag->edgeCount;
struct evidence *agEv = ag->evidence;
for (i=0; i<edgeCount; ++i, agEv=agEv->next)
    {
    struct txEdge *e;
    AllocVar(e);
    e->startIx = ag->edgeStarts[i];
    e->endIx = ag->edgeEnds[i];
    e->type = ag->edgeTypes[i];
    slAddHead(&txg->edgeList, e);

    /* Deal with evidence. */
    int j;
    e->evCount = agEv->evCount;
    for (j=0; j<agEv->evCount; ++j)
        {
	struct txEvidence *ev;
	AllocVar(ev);
	ev->sourceId = agEv->mrnaIds[j];
	ev->start = txg->vertices[e->startIx].position;
	ev->end = txg->vertices[e->endIx].position;
	slAddHead(&e->evList, ev);
	}
    slReverse(&e->evList);
    }
slReverse(&txg->edgeList);

/* Deal with sources. */
int sourceCount = txg->sourceCount = ag->mrnaRefCount;
AllocArray(txg->sources, sourceCount);
for (i=0; i<sourceCount; ++i)
    {
    char *acc = ag->mrnaRefs[i];
    char *type = (startsWith("NM_", acc) ? "refSeq" : "mrna");
    struct txSource *source = &txg->sources[i];
    source->accession = cloneString(acc);
    source->type = cloneString(type);
    }
return txg;
}

void agxToTxg(char *inAgx, char *outTxg)
/* agxToTxg - Convert from old altGraphX format to newer txGraph format.. */
{
struct lineFile *lf = lineFileOpen(inAgx, TRUE);
FILE *f = mustOpen(outTxg, "w");
char *row[ALTGRAPHX_NUM_COLS];
while (lineFileRow(lf, row))
    {
    struct altGraphX *ag = altGraphXLoad(row);
    struct txGraph *txg = txgFromAltGraphX(ag);
    txGraphTabOut(txg, f);
    altGraphXFree(&ag);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
agxToTxg(argv[1], argv[2]);
return 0;
}
