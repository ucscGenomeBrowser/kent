/* txgTrim - Trim out parts of txGraph that are not of sufficient weight.. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "txGraph.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txgTrim - Trim out parts of txGraph that are not of sufficient weight.\n"
  "usage:\n"
  "   txgTrim in.txg in.weights threshold out.txg\n"
  "where in.weight is a file with two columns\n"
  "    type - usually 'refSeq' or 'mrna' or 'est' or something\n"
  "    weight -  a floating point number that interacts with threshold\n"
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
         warn("No weight of type %s\n", source->type);
    else
        total += weight->value;
    }
return total;
}

void txGraphTrimOne(struct txGraph *txg, struct hash *weightHash, double threshold)
/* Trim down one graph. */
{
struct txEdge *edge, *next, *newList = NULL;
for (edge = txg->edgeList; edge != NULL; edge = next)
    {
    next = edge->next;
    double weight = weightOfEvidence(txg, edge->evList, weightHash);
    verbose(3, "%s edge: evCount %d, weight %f\n", 
    	txg->name, slCount(edge->evList), weight);
    if (weight < threshold)
        {
	verbose(3, "  trimming\n");
	txg->edgeCount -= 1;
	}
    else
        {
	slAddHead(&newList, edge);
	}
    }
slReverse(&newList);
txg->edgeList = newList;
}

void txgTrim(char *inTxg, char *inWeights, char *asciiThreshold, char *outTxg)
/* txgTrim - Trim out parts of txGraph that are not of sufficient weight.. */
{
struct txGraph *txgList = txGraphLoadAll(inTxg);
verbose(2, "LOaded %d txGraphs from %s\n", slCount(txgList), inTxg);
struct hash *weightHash = hashWeights(inWeights);
verbose(2, "Loaded %d weights from %s\n", weightHash->elCount, inWeights);
double threshold = sqlDouble(asciiThreshold);
verbose(2, "Threshold %f\n", threshold);
struct txGraph *txg;
FILE *f = mustOpen(outTxg, "w");
for (txg = txgList; txg != NULL; txg = txg->next)
    {
    verbose(2, "%s edgeCount %d\n", txg->name, txg->edgeCount);
    txGraphTrimOne(txg, weightHash, threshold);
    if (txg->edgeCount > 0)
	txGraphTabOut(txg, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
txgTrim(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
