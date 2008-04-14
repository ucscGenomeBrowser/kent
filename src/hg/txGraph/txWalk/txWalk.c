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
struct hash *sizeHash = NULL;
boolean doDefrag = FALSE;
double defragSize = 0.25;

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
  "   -sizes=sizes.tab - tell program about size of RNA.  Format of file is\n"
  "                      Two column - accession, size.  Enables -defrag\n"
  "   -defrag=0.N - minimum size fragment of a gene to output relative to input\n"
  "                 RNA size.  Requires sizes option. Suggested value 0.25\n"
  , singleExonFactor
  );
}

static struct optionSpec options[] = {
   {"evidence", OPTION_STRING},
   {"singleExonFactor", OPTION_DOUBLE},
   {"sizes", OPTION_STRING},
   {"defrag", OPTION_DOUBLE},
   {NULL, 0},
};

struct weight
/* A named weight. */
    {
    char *type;		/* Not allocated here */
    double value;	/* Weight value */
    };

boolean isAccessionedSource(struct txSource *source)
/* Return TRUE if it's some sort of source we want to keep track of. */
{
return sameString(source->type, "refSeq") || sameString(source->type, "mrna") || sameString(source->type, "ccds");
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

struct hash *hashKeyIntFile(char *fileName)
/* Read in a two column file with first column a key, second a number,
 * and return an integer valued hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = hashNew(20);
char *row[2];
while (lineFileRow(lf, row))
    {
    int val = lineFileNeedNum(lf, row, 1);
    hashAddInt(hash, row[0], val);
    }
lineFileClose(&lf);
return hash;
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
    if (isAccessionedSource(source))
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

boolean subsetExceptForEnds(struct rbTree *aTree, struct rbTree *bTree,
	struct slRef *bRangeRefList)
/* Return TRUE if bTree is a subset of aTree are the same except for perhaps
 * the start of the first exon and end of last exon.   By this I mean every
 * exon is the same, not just enclosed. Pass in the refList for b, just as a 
 * minor speed tweak. */
{
struct slRef *aRangeRefList = rbTreeItems(aTree);
struct slRef *aRef, *bRef;
struct range *a, *b;

/* Loop through aTree until find beginning exon that is same as first
 * exon in bTree. */
bRef = bRangeRefList;
b = bRef->val;
for (aRef = aRangeRefList; aRef != NULL; aRef = aRef->next)
    {
    a = aRef->val;
    if (a->end == b->end)
        break;
    if (a->end > b->end)
        return FALSE;
    }
if (aRef == NULL)
    return FALSE;
if (a->start > b->start)
    return FALSE;

/* Advance B.  Check for single exon B. */
bRef = bRef->next;
if (bRef == NULL)
    return TRUE;

/* Advance A.  Check for single exon A */
aRef = aRef->next;
if (aRef == NULL)
    return FALSE;

/* Loop through until get to last exon in b, making sure we have
 * an exact match on both ends. */
for (;; aRef = aRef->next, bRef = bRef->next)
    {
    if (bRef->next == NULL)
        break;
    if (aRef->next == NULL)
        return FALSE;
    a = aRef->val;
    b = bRef->val;
    if (a->start != b->start || a->end != b->end)
        return FALSE;
    }

/* On last exon just check start. */
assert(aRef != NULL && bRef != NULL);
a = aRef->val;
b = bRef->val;
if (a->start != b->start)
    return FALSE;
if (a->end < b->end)
    return FALSE;
return TRUE;
}

struct rbTree *rnaOut(struct txGraph *txg, struct lm *lm, struct rbTreeNode *stack[128],
	struct rbTree *existingList, struct slRef *trackerRefList, 
	struct txSource *source, int txId, FILE *bedFile, FILE *efFile)
/* Write out one RNA transcript. */
{
struct slRef *ref;
struct hash *evHash = hashNew(8);
char nameBuf[256];

/* Pass blocks through a rangeTree to merge any ones that 
 * are overlapping.  This occassionally happens in the graph. */
struct rbTree *rangeTree = rangeTreeNewDetailed(lm, stack);
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
	    if (isAccessionedSource(source))
		{
		if (!hashLookup(evHash, source->accession))
		    hashAdd(evHash, source->accession, source);
		}
	    }
	}
    }

/* Get list of evidence.  We built this up in a hash
 * along with the range tree above. */
struct hashEl *hel, *evList = hashElListHash(evHash);
slSort(&evList, hashElCmp);

/* Check that not a proper subset at the exon level from something
 * that is already output.  The conditions that lead to this
 * are rare, but they do happen.  By subset in this context we
 * mean every exon identical except perhaps for the start of the
 * first exon and the end of the last exon. */
struct slRef *eRef, *eRefList = rbTreeItems(rangeTree);
struct rbTree *existing;
for (existing = existingList; existing != NULL; existing = existing->next)
     {
     if (subsetExceptForEnds(existing, rangeTree, eRefList))
         return NULL;
     }

/* Figure min/max and totalSize. */
int totalSize = 0;
int minBase = BIGNUM, maxBase = -BIGNUM;
for (eRef = eRefList; eRef != NULL; eRef = eRef->next)
    {
    struct range *r = eRef->val;
    minBase = min(minBase, r->start);
    maxBase = max(maxBase, r->end);
    totalSize += (r->end - r->start);
    }

/* Optionally filter out small fragments. */
if (doDefrag)
    {
    int size = hashIntValDefault(sizeHash, source->accession, 0);
    if (!size)
	{
	if (!sameString(source->type, "ccds"))
	    verbose(1, "%s not in sizes tab file\n", source->accession);
	}
    if (totalSize < size * defragSize)
        return NULL;
    }

/* Write out bed stuff except for blocks. */
fprintf(bedFile, "%s\t%d\t%d\t", txg->tName, minBase, maxBase);
safef(nameBuf, sizeof(nameBuf), "%s.%d.%s", txg->name, txId, source->accession);
fprintf(bedFile, "%s\t%d\t%s\t", nameBuf, 0, txg->strand);

/* Write out exon count and block sizes */
fprintf(bedFile, "0\t0\t0\t%d\t", rangeTree->n);
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

/* Optionally write out evidence list for each accession. */
if (evFile != NULL)
    {
    fprintf(evFile, "%s\t%s\t%d\t", nameBuf, source->accession, evHash->elCount);
    for (hel = evList; hel != NULL; hel =  hel->next)
        {
	fprintf(evFile, "%s,", hel->name);
	}
    fprintf(evFile, "\n");
    }
slFreeList(&evList);
hashFree(&evHash);
slFreeList(&eRefList);
return rangeTree;
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
for (bigRef = bigger; bigRef != NULL; bigRef = bigRef->next)
    {
    if (bigRef->val == smaller->val)
        break;
    }
if (bigRef == NULL)
    return FALSE;
for (smallRef = smaller; smallRef != NULL && bigRef != NULL; smallRef = smallRef->next, bigRef=bigRef->next)
    {
    if (smallRef->val != bigRef->val)
        return FALSE;
    }
return smallRef == NULL;
}

#ifdef DEBUG
void dumpTrackerRef(struct slRef *ref)
/* Dump info from a single reference to an edge tracker. */
{
struct edgeTracker *tracker = ref->val;
struct txEdge *edge = tracker->edge;
if (edge->type == 0)
    printf("[%d-%d]  ", tracker->start, tracker->end);
else
    printf("]%d-%d[  ", tracker->start, tracker->end);
}

void dumpTrackerRefList(struct slRef *refList)
/* Dump out information on a list of tracked edges. */
{
struct slRef *ref;
for (ref = refList; ref != NULL; ref = ref->next)
    {
    dumpTrackerRef(ref);
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
return isSubset;
}
#endif /* DEBUG */

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
struct rbTreeNode **stack;
lmAllocArray(lm, stack, 128);

/* Look up type weights for all sources. */
double sourceTypeWeights[txg->sourceCount];
int i;
for (i=0; i<txg->sourceCount; ++i)
    {
    sourceTypeWeights[i] = weightFromHash(weightHash, txg->sources[i].type);
    verbose(3, "weight for %s %s (index %d) is %f\n", txg->sources[i].type, txg->sources[i].accession, i, sourceTypeWeights[i]);
    }

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
    edgesUsedBySource[i] = NULL;
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
struct rbTree *existingList = NULL, *exonTree;
for (rna = rnaList; rna != NULL; rna = rna->next)
    {
    struct slRef *trackerRefList = edgesUsedBySource[rna->id];
    struct slRef *supportedList = supportedSubset(trackerRefList, threshold, lm);
    verbose(3, "rna %s, supportedBy %d, trackedBy %d\n", txg->sources[rna->id].accession, slCount(supportedList), slCount(trackerRefList));
    if (supportedList != NULL && !properSubsetOfAny(pathList, supportedList))
        {
	verbose(3, "   outputting %s\n", txg->sources[rna->id].accession);
	lmAllocVar(lm, path);
	path->trackerRefList = supportedList;
	slAddHead(&pathList, path);
        exonTree = rnaOut(txg, lm, stack, existingList, trackerRefList, 
		&txg->sources[rna->id], ++txId, bedFile, evFile);
	if (exonTree != NULL)
	    slAddHead(&existingList, exonTree);
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
    verbose(2, "Processing %s\n", txg->name);
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
char *sizeFile = optionVal("sizes", NULL);
if (sizeFile)
    sizeHash = hashKeyIntFile(sizeFile);
if (optionExists("defrag"))
    {
    if (sizeFile == NULL)
        errAbort("-sizes option required with -defrag option.");
    doDefrag = TRUE;
    defragSize = optionDouble("defrag", defragSize);
    }
if (argc != 5)
    usage();
txWalk(argv[1], argv[2], argv[3], argv[4]);
carefulClose(&evFile);
return 0;
}

