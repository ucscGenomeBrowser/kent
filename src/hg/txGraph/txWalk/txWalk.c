/* txWalk - Walk transcription graph and output transcripts.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "rangeTree.h"
#include "sqlNum.h"
#include "ggTypes.h"
#include "txGraph.h"

/* Globals set from command line. */
FILE *evFile = NULL;
double singleExonFactor = 0.75;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txWalk - Walk transcription graph and output transcripts.\n"
  "usage:\n"
  "   txWalk in.txg in.weights threshold out.bed\n"
  "options:\n"
  "   -evidence=out.ev - place mrna associated with each transcript here.\n"
  "   -singleExonFactor=0.N - factor used to lighten up weight of single exon\n"
  "                           transcripts. Default %g.\n"
  , singleExonFactor
  );
}

static struct optionSpec options[] = {
   {"evidence", OPTION_STRING},
   {"singleExonFactor", OPTION_DOUBLE},
   {NULL, 0},
};

struct weight
/* A named weight. */
    {
    char *type;		/* Not allocated here */
    double value;	/* Weight value */
    };

boolean isRnaSource(struct txSource *source)
/* Return TRUE if it's some sort of RNA source. */
{
return sameString(source->type, "refSeq") || sameString(source->type, "mrna");
}

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

double edgeTotalWeight(struct txGraph *txg,
	struct txEdge *edge, double sourceTypeWeights[])
/* Return sum of all sources at edge. */
{
double total = 0;
struct txEvidence *ev;
int edgeStart = txg->vertices[edge->startIx].position;
int edgeEnd = txg->vertices[edge->endIx].position;
double edgeSizeFactor = 1.0/(edgeEnd - edgeStart);
for (ev = edge->evList; ev != NULL; ev = ev->next)
    {
    double sourceWeight = sourceTypeWeights[ev->sourceId];
    int overlap = rangeIntersection(edgeStart, edgeEnd, ev->start, ev->end);
    if (overlap > 0)
        {
	double weight = sourceWeight * edgeSizeFactor * overlap;
	total += weight;
	}
    }
return total;
}

struct edgeTracker
/* Keep track of info on an edge temporarily */
   {
   struct edgeTracker *next;
   struct txEdge *edge;	/* Corresponding edge. */
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
	double threshold, double singleExonFactor, struct lm *lm)
/* Make up a structure to help walk the graph. */
{
struct edgeTracker *trackerList = NULL;
struct txEdge *edge;
int i;
for (edge = txg->edgeList, i=0; edge != NULL; edge = edge->next, i++)
    {
    struct edgeTracker *tracker;
    lmAllocVar(lm, tracker);
    tracker->edge = edge;
    double weight = edgeTotalWeight(txg, edge, sourceTypeWeights);
    if (txg->vertices[edge->startIx].type == ggSoftStart && 
    	txg->vertices[edge->endIx].type == ggSoftEnd)
	{
	weight *= singleExonFactor;
	}
    tracker->weight = weight;
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
    if (isRnaSource(source))
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

void rnaOut(struct txGraph *txg, struct slRef *trackerRefList, 
	struct txSource *source, int txId, FILE *bedFile, FILE *efFile)
/* Write out one RNA transcript. */
{
struct slRef *ref;
int minBase = BIGNUM, maxBase = -BIGNUM;
struct hash *evHash = hashNew(0);
char nameBuf[256];

/* Figure min/max. */
for (ref = trackerRefList; ref != NULL; ref = ref->next)
    {
    struct edgeTracker *tracker = ref->val;
    if (tracker->edge->type == ggExon)
	{
	minBase = min(minBase, tracker->start);
	maxBase = max(maxBase, tracker->end);
	}
    }

/* Write out bed stuff except for blocks. */
fprintf(bedFile, "%s\t%d\t%d\t", txg->tName, minBase, maxBase);
safef(nameBuf, sizeof(nameBuf), "%s.%d.%s", txg->name, txId, source->accession);
fprintf(bedFile, "%s\t%d\t%s\t", nameBuf, 0, txg->strand);

/* Pass blocks through a rangeTree to merge any ones that 
 * are overlapping.  This occassionally happens in the graph. */
struct rbTree *rangeTree = rangeTreeNew();
for (ref = trackerRefList; ref != NULL; ref = ref->next)
    {
    struct edgeTracker *tracker = ref->val;
    struct txEdge *edge = tracker->edge;
    if (edge->type == ggExon)
	{
	rangeTreeAdd(rangeTree, tracker->start, tracker->end);
	struct txEvidence *ev;
	for (ev = edge->evList; ev != NULL; ev = ev->next)
	    {
	    struct txSource *source = &txg->sources[ev->sourceId];
	    if (isRnaSource(source))
		hashStore(evHash, source->accession);
	    }
	}
    }

/* Write out exon count and block sizes */
fprintf(bedFile, "0\t0\t0\t%d\t", rangeTree->n);
struct slRef *eRef, *eRefList = rbTreeItems(rangeTree);
for (eRef = eRefList; eRef != NULL; eRef = eRef->next)
    {
    struct range *r = eRef->val;
    fprintf(bedFile, "%d,", r->end - r->start);
    }
fprintf(bedFile, "\t");

/* Write out block starts */
for (eRef = eRefList; eRef != NULL; eRef = eRef->next)
    {
    struct range *r = eRef->val;
    fprintf(bedFile, "%d,", r->start - minBase);
    }
fprintf(bedFile, "\n");

if (evFile != NULL)
    {
    struct hashEl *hel, *helList = hashElListHash(evHash);
    slSort(&helList, hashElCmp);
    fprintf(evFile, "%s\t%s\t%d\t", nameBuf, source->accession, evHash->elCount);
    for (hel = helList; hel != NULL; hel =  hel->next)
        {
	fprintf(evFile, "%s,", hel->name);
	}
    fprintf(evFile, "\n");
    }

rbTreeFree(&rangeTree);
hashFree(&evHash);
}

struct path
/* A connected set of exons. */
    {
    struct path *next;
    struct slRef *trackerRefList;
    };

boolean properSubsetOf(struct slRef *bigger, struct slRef *smaller)
/* Return TRUE if smaller is a proper subset of bigger.  */
{
struct slRef *bigRef, *smallRef;
for (smallRef = smaller; smallRef != NULL; smallRef = smallRef->next)
    {
    boolean anySame = FALSE;
    void *smallVal = smallRef->val;
    for (bigRef = bigger; bigRef != NULL; bigRef = bigRef->next)
        {
	if (bigRef->val == smallVal)
	    {
	    anySame = TRUE;
	    break;
	    }
	}
    if (!anySame)
	return FALSE;
    }
return TRUE;
}


void dumpTrackerRefList(struct slRef *refList)
{
struct slRef *ref;
for (ref = refList; ref != NULL; ref = ref->next)
    {
    printf("%p ", ref->val);
    }
}

boolean debug_properSubsetOf(struct slRef *bigger, struct slRef *smaller)
{
boolean isSubset = properSubsetOf(bigger, smaller);
printf("is:\n  ");
dumpTrackerRefList(smaller);
printf("\na subset of;\n  ");
dumpTrackerRefList(bigger);
printf("\n? %s\n", (isSubset ? "yes" : "no"));
printf("\n");
return isSubset;
}

boolean properSubsetOfAny(struct path *pathList, struct slRef *trackerRefList)
/* Return TRUE if trackerRefList is a subset of any of the paths in
 * pathList */
{
struct path *path;
for (path = pathList; path != NULL; path = path->next)
    {
    if (properSubsetOf(path->trackerRefList, trackerRefList))
        return TRUE;
    }
return FALSE;
}

struct slRef *supportedSubset(struct slRef *trackerRefList, double threshold, 
	struct lm *lm)
/* Return subset of trackerRefList that has weight over threshold */
{
struct slRef *newList = NULL, *newRef, *ref;
for (ref = trackerRefList; ref != NULL; ref = ref->next)
    {
    struct edgeTracker *tracker = ref->val;
    if (tracker->weight >= threshold)
	{
	lmAllocVar(lm, newRef);
	newRef->val = tracker;
	slAddHead(&newList, newRef);
	}
    }
slReverse(&newList);
return newList;
}

void walkOut(struct txGraph *txg, struct hash *weightHash, double threshold, 
	double singleExonFactor, FILE *bedFile, FILE *evFile)
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
struct edgeTracker *trackerList = makeTrackerList(txg, sourceTypeWeights, 
	threshold, singleExonFactor, lm);

/* Calculate txWeights for all sources. */
double sourceTxWeights[txg->sourceCount];
for (i=0; i<txg->sourceCount; ++i)
     sourceTxWeights[i] = 0;
struct txEdge *edge;
struct edgeTracker *tracker;
for (edge = txg->edgeList, tracker=trackerList; 
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
int txId=0;
struct path *path, *pathList = NULL;
for (rna = rnaList; rna != NULL; rna = rna->next)
    {
    struct slRef *trackerRefList = edgesUsedBySource[rna->id];
    struct slRef *supportedList = supportedSubset(trackerRefList, threshold, lm);
    if (supportedList != NULL && !properSubsetOfAny(pathList, supportedList))
        {
	lmAllocVar(lm, path);
	path->trackerRefList = supportedList;
	slAddHead(&pathList, path);
        rnaOut(txg, trackerRefList, &txg->sources[rna->id], ++txId, bedFile, evFile);
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
threshold -= 0.000001;	/* Allow for some rounding. */
FILE *f = mustOpen(outBed, "w");
char *row[TXGRAPH_NUM_COLS];
while (lineFileRow(lf, row))
    {
    struct txGraph *txg = txGraphLoad(row);
    verbose(3, "Processing %s\n", txg->name);
    walkOut(txg, weightHash, threshold, singleExonFactor, f, evFile);
    txGraphFree(&txg);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (optionExists("evidence"))
    evFile = mustOpen(optionVal("evidence", NULL), "w");
singleExonFactor = optionDouble("singleExonFactor", singleExonFactor);
if (argc != 5)
    usage();
txWalk(argv[1], argv[2], argv[3], argv[4]);
carefulClose(&evFile);
return 0;
}

