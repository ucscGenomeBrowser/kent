/* txWalk - Walk transcription graph and output transcripts.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "geneGraph.h"
#include "txGraph.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txWalk - Walk transcription graph and output transcripts.\n"
  "usage:\n"
  "   txWalk in.txg in.weights threshold out.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
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


double weightFromHash(struct hash *hash, char *name)
/* Find weight given name. */
{
struct weight *weight = hashMustFindVal(hash, name);
return weight->value;
}

struct slRef *findEdgesThatStartWith(struct txGraph *txg, int startIx, struct lm *lm)
/* Build up list of all edges with given start. */
{
struct txEdge *edge;
struct slRef *exitList = NULL;
for (edge = txg->edges; edge != NULL; edge = edge->next)
    {
    if (edge->startIx == startIx)
	{
	struct slRef *ref;
	lmAllocVar(lm, ref);
	ref->val = edge;
	slAddHead(&exitList, ref);
	}
    }
slReverse(&exitList);
return exitList;
}

double edgeTotalWeight(struct txEdge *edge, double sourceTypeWeights[])
/* Return sum of all sources at edge. */
{
double total = 0;
struct txEvidence *ev;
for (ev = edge->evList; ev != NULL; ev = ev->next)
    total += sourceTypeWeights[ev->sourceId];
return total;
}

struct edgeTracker
/* Keep track of info on an edge temporarily */
   {
   struct edgeTracker *next;
   struct txEdge *edge;	/* Corresponding edge. */
   boolean visited;	/* True if visited during traversal. */
   struct slRef *exitList;	/* References to edges that exit this one. */
   double weight;	/* Total weight of edge. */
   int start, end;	/* Start/end in sequence. */
   };

int edgeTrackerCmpStart(const void *va, const void *vb)
/* Compare to sort based on start. */
{
const struct edgeTracker *a = *((struct edgeTracker **)va);
const struct edgeTracker *b = *((struct edgeTracker **)vb);
return a->start - b->start;
}

struct edgeTracker *makeTrackerList(struct txGraph *txg, double sourceTypeWeights[],
	double threshold, struct lm *lm)
/* Make up a structure to help walk the graph. */
{
struct edgeTracker *trackerList = NULL;
struct txEdge *edge;
int i;
for (edge = txg->edges, i=0; edge != NULL; edge = edge->next, i++)
    {
    struct edgeTracker *tracker;
    lmAllocVar(lm, tracker);
    tracker->edge = edge;
    tracker->exitList = findEdgesThatStartWith(txg, edge->endIx, lm);
    tracker->weight = edgeTotalWeight(edge, sourceTypeWeights);
    if (tracker->weight < threshold)
        tracker->visited = TRUE;
    tracker->start = txg->vertices[edge->startIx].position;
    tracker->end = txg->vertices[edge->endIx].position;
    slAddHead(&trackerList, tracker);
    }
slSort(&trackerList, edgeTrackerCmpStart);
return trackerList;
}

struct weightedRna
/* A source RNA and some weight */
    {
    struct weightedRna	 *next;	/* Next in list */
    int id;		/* Index in sources table. */
    double typeWeight;  /* Weight of type. */
    double txWeight;	/* Weight of exons. */
    };


int weightedRnaCmp(const void *va, const void *vb)
/* Compare to sort based on typeWeight,txWeight). */
{
const struct weightedRna *a = *((struct weightedRna **)va);
const struct weightedRna *b = *((struct weightedRna **)vb);
double diff = b->typeWeight - a->typeWeight;
if (diff == 0)
    diff = b->txWeight - a->txWeight;
if (diff < 0)
    return -1;
else if (diff > 0)
    return 1;
else
    return 0;
}

struct weightedRna *makeWeightedRna(struct txGraph *txg, double sourceTypeWeights[],
	double sourceTxWeights[], struct lm *lm)
/* Create a list of sources, sorted with best first. */
{
int sourceId;
struct weightedRna *rnaList = NULL, *rna;
for (sourceId = 0; sourceId < txg->sourceCount; ++sourceId)
    {
    struct txSource *source = &txg->sources[sourceId];
    if (sameString(source->type, "refSeq") || sameString(source->type, "mrna")
    	|| sameString(source->type, "est"))
	{
	lmAllocVar(lm, rna);
	rna->id = sourceId;
	rna->typeWeight = sourceTypeWeights[sourceId];
	rna->txWeight = sourceTxWeights[sourceId];
	slAddHead(&rnaList, rna);
	}
    }
slSort(&rnaList, weightedRnaCmp);
return rnaList;
}

boolean allTrackerRefsVisited(struct slRef *trackerRefs)
/* Return TRUE if every tracker on refList is visited. */
{
struct slRef *ref;
for (ref = trackerRefs; ref != NULL; ref = ref->next)
    {
    struct edgeTracker *tracker = ref->val;
    if (!tracker->visited)
        return FALSE;
    }
return TRUE;
}

void rnaOut(struct txGraph *txg, struct slRef *trackerRefList, 
	struct txSource *source, FILE *f)
/* Write out one RNA transcript and mark corresponding edges as visited. */
{
struct slRef *ref;
int exonCount = 0;
int minBase = BIGNUM, maxBase = -BIGNUM;

/* Count up exons and set visit flags, figure min/max. */
for (ref = trackerRefList; ref != NULL; ref = ref->next)
    {
    struct edgeTracker *tracker = ref->val;
    tracker->visited = TRUE;
    if (tracker->edge->type == ggExon)
	{
	exonCount += 1;
	minBase = min(minBase, tracker->start);
	maxBase = max(maxBase, tracker->end);
	}
    }

/* Write out bed stuff except for blocks. */
fprintf(f, "%s\t%d\t%d\t", txg->tName, minBase, maxBase);
fprintf(f, "%s.%s\t0\t%s\t", txg->name, source->accession, txg->strand);
fprintf(f, "0\t0\t0\t%d\t", exonCount);

/* Write out block sizes */
for (ref = trackerRefList; ref != NULL; ref = ref->next)
    {
    struct edgeTracker *tracker = ref->val;
    tracker->visited = TRUE;
    if (tracker->edge->type == ggExon)
	fprintf(f, "%d,", tracker->end - tracker->start);
    }
fprintf(f, "\t");

/* Write out block starts */
for (ref = trackerRefList; ref != NULL; ref = ref->next)
    {
    struct edgeTracker *tracker = ref->val;
    if (tracker->edge->type == ggExon)
	fprintf(f, "%d,", tracker->start - minBase);
    }
fprintf(f, "\n");
}

void walkOut(struct txGraph *txg, struct hash *weightHash, double threshold, FILE *f)
/* Generate transcripts and write to file. */
{
/* Local memory pool for fast, easy cleanup. */
struct lm *lm = lmInit(0);	 

/* Look up type weights for all sources. */
double sourceTypeWeights[txg->sourceCount];
int i;
for (i=0; i<txg->sourceCount; ++i)
    sourceTypeWeights[i] = weightFromHash(weightHash, txg->sources[i].type);

/* Set up structure to track edges. */
struct edgeTracker *tracker, *trackerList = makeTrackerList(txg, sourceTypeWeights, threshold, lm);

/* Calculate txWeights for all sources. */
double sourceTxWeights[txg->sourceCount];
for (i=0; i<txg->sourceCount; ++i)
     sourceTxWeights[i] = 0;
struct txEdge *edge;
for (edge = txg->edges, tracker=trackerList; 
     edge != NULL; 
     edge = edge->next, tracker=tracker->next)
    {
    struct txEvidence *ev;
    for (ev = edge->evList; ev != NULL; ev = ev->next)
        sourceTxWeights[ev->sourceId] += tracker->weight;
    }

/* Make up list of edges used by each source. */
struct slRef *edgesUsedBySource[txg->sourceCount];
for (i=0; i<txg->sourceCount; ++i)
    edgesUsedBySource[i] = 0;
for (tracker = trackerList; tracker != NULL; tracker = tracker->next)
    {
    struct txEdge *edge = tracker->edge;
    struct txEvidence *ev;
    for (ev = edge->evList; ev != NULL; ev = ev->next)
        {
	struct slRef *ref;
	lmAllocVar(lm, ref);
	ref->val = tracker;
	slAddHead(&edgesUsedBySource[ev->sourceId], ref);
	}
    }
for (i=0; i<txg->sourceCount; ++i)
    {
    slReverse(&edgesUsedBySource[i]);
    }


/* Go from best to worst RNA, checking to see if it's exons are already
 * covered.  If not, then output transcript containing all exons in that RNA. */
struct weightedRna *rna, *rnaList = makeWeightedRna(txg, sourceTypeWeights, sourceTxWeights, lm);
for (rna = rnaList; rna != NULL; rna = rna->next)
    {
    struct slRef *trackerRefList = edgesUsedBySource[rna->id];
    if (!allTrackerRefsVisited(trackerRefList))
	{
        rnaOut(txg, trackerRefList, &txg->sources[rna->id], f);
	}
    }

lmCleanup(&lm);
}

void txWalk(char *inTxg, char *inWeights, char *asciiThreshold, char *outBed)
/* txWalk - Walk transcription graph and output transcripts. */
{
struct hash *weightHash = hashWeights(inWeights);
struct lineFile *lf = lineFileOpen(inTxg, TRUE);
double threshold = sqlDouble(asciiThreshold);
FILE *f = mustOpen(outBed, "w");
char *row[TXGRAPH_NUM_COLS];
while (lineFileRow(lf, row))
    {
    struct txGraph *txg = txGraphLoad(row);
    verbose(3, "Processing %s\n", txg->name);
    walkOut(txg, weightHash, threshold, f);
    txGraphFree(&txg);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
txWalk(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

