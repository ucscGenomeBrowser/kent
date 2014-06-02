/* txgGoodEdges - Get edges that are above a certain threshold.. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "ggTypes.h"
#include "sqlNum.h"
#include "txGraph.h"
#include "txEdgeBed.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txgGoodEdges - Get edges that are above a certain threshold.\n"
  "usage:\n"
  "   txgGoodEdges in.txg in.weights threshold type out.edges\n"
  "where in.weight is a file with two columns\n"
  "    type - usually 'refSeq' or 'mrna' or 'est' or something\n"
  "    weight -  a floating point number that interacts with threshold\n"
  "threshold is a numerical threshold\n"
  "type is the edge output type, which will appear often in another weight table\n"
  "out.edges is the output edge file\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct weight
/* A named weight. */
    {
    char *type;		/* Not allocated here */
    double value;	/* Weight value */
    };

struct hash *hashWeights(char *in)
/* Return hash full of weights. */
{
struct lineFile *lf = lineFileOpen(in, TRUE);
char *row[2];
struct hash *hash = hashNew(0);
while (lineFileRow(lf, row))
    {
    struct weight *weight;
    AllocVar(weight);
    weight->value = lineFileNeedDouble(lf, row, 1);
    hashAddSaveName(hash, row[0], weight, &weight->type);
    }
lineFileClose(&lf);
return hash;
}

double weightOfEvidence(struct txGraph *txg, struct txEvidence *evList, struct hash *weightHash)
/* Sum up weight of all evidence and return. */
{
struct txEvidence *ev;
double total = 0;
for (ev = evList; ev != NULL; ev = ev->next)
    {
    struct txSource *source = &txg->sources[ev->sourceId];
    struct weight *weight = hashFindVal(weightHash, source->type);
    if (weight == NULL)
        errAbort("No weight of type %s\n", source->type);
    else
        total += weight->value;
    }
return total;
}

void processOneGraph(struct txGraph *txg, struct hash *weightHash, double threshold, 
	char *outType, FILE *f)
/* Write out edges for one graph. */
{
struct txEdge *edge;
struct txEdgeBed e;
ZeroVar(&e);
e.chrom = txg->tName;
for (edge = txg->edgeList; edge != NULL; edge = edge->next)
    {
    double weight = weightOfEvidence(txg, edge->evList, weightHash);
    if (weight >= threshold)
        {
	struct txVertex *start = &txg->vertices[edge->startIx];
	struct txVertex *end = &txg->vertices[edge->endIx];
	e.chromStart = start->position;
	e.chromEnd = end->position;
	e.name = outType;
	e.score = edge->evCount;
	e.strand[0] = txg->strand[0];
	e.startType[0] = ggVertexTypeAsString(start->type)[0];
	e.type = edge->type;
	e.endType[0] = ggVertexTypeAsString(end->type)[0];
	txEdgeBedTabOut(&e, f);
	}
    }
}

void txgGoodEdges(char *inTxg, char *inWeights, char *asciiThreshold, 
	char *outType, char *outEdges)
/* txgGoodEdges - Get edges that are above a certain threshold.. */
{
struct txGraph *txgList = txGraphLoadAll(inTxg);
verbose(2, "LOaded %d txGraphs from %s\n", slCount(txgList), inTxg);
struct hash *weightHash = hashWeights(inWeights);
verbose(2, "Loaded %d weights from %s\n", weightHash->elCount, inWeights);
double threshold = sqlDouble(asciiThreshold);
verbose(2, "Threshold %f\n", threshold);
struct txGraph *txg;
FILE *f = mustOpen(outEdges, "w");
for (txg = txgList; txg != NULL; txg = txg->next)
    {
    verbose(2, "%s edgeCount %d\n", txg->name, txg->edgeCount);
    processOneGraph(txg, weightHash, threshold, outType, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
txgGoodEdges(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
