/* filter cDNA alignments in psl format */
#include "common.h"
#include "cDnaAligns.h"
#include "overlapFilter.h"
#include "psl.h"
#include "options.h"

static void usage(char *msg)
/* usage msg and exit */
{
/* message got huge, so it's in a generate file */
static char *usageMsg =
#include "usage.h"
    ;
errAbort("%s:  %s", msg, usageMsg);
}


/* command line options and values */
static struct optionSpec optionSpecs[] =
{
    {"localNearBest", OPTION_FLOAT},
    {"globalNearBest", OPTION_FLOAT},
    {"minId", OPTION_FLOAT},
    {"minCover", OPTION_FLOAT},
    {"minSpan", OPTION_FLOAT},
    {"minQSize", OPTION_INT},
    {"maxAligns", OPTION_INT},
    {"minNonRepLen", OPTION_INT},
    {"maxRepMatch", OPTION_FLOAT},
    {"polyASizes", OPTION_STRING},
    {"usePolyTHead", OPTION_BOOLEAN},
    {"bestOverlap", OPTION_BOOLEAN},
    {"dropped", OPTION_STRING},
    {"weirdOverlapped", OPTION_STRING},
    {"alignStats", OPTION_STRING},
    {NULL, 0}
};

static float gLocalNearBest = -1.0;    /* local near best in genome */
static float gGlobalNearBest = -1.0;   /* global near best in genome */
static float gMinId = 0.0;             /* minimum fraction id */
static float gMinCover = 0.0;          /* minimum coverage */
static float gMinSpan = 0.0;           /* minimum target span allowed */
static int gMinQSize = 0;              /* drop queries shorter than this */
static int gMaxAligns = -1;            /* only allow this many alignments for a query */
static int gMinNonRepLen = 0;          /* minimum non-repeat bases that must be aligned */
static float gMaxRepMatch = 1.0;       /* maximum repeat match/aligned */
static char *gPolyASizes = NULL;       /* polyA size file */
static boolean gUsePolyTHead = FALSE;  /* adjust bounds with longs of polyA tail or polyT head */
static boolean gBestOverlap = FALSE;   /* filter overlaping, keeping only the best */
static char *gDropped = NULL;          /* save dropped psls here */
static char *gWeirdOverlappped = NULL; /* save weird overlapping psls here */
static char *gAlignStats = NULL;       /* file for statistics output */

static int gMinLocalBestCnt = 30;      /* minimum number of bases that are over threshold
                                        * for local best */

struct outFiles
/* open output files */
{
    FILE *pslFh;               /* filtered psls */
    FILE *droppedFh;           /* dropped psls, or NULL */
    FILE *weirdOverPslFh;      /* wierd overlap psls, or NULL */
    FILE *alignStatsFh;        /* alignment stats, or NULL; */
};

static boolean validPsl(struct psl *psl)
/* check if a psl is internally consistent */
{
static FILE *devNull = NULL;
if (devNull == NULL)
    devNull = mustOpen("/dev/null", "w");
return (pslCheck("", devNull, psl) == 0);
}

static void invalidPslFilter(struct cDnaQuery *cdna)
/* filter for invalid PSL */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && !validPsl(aln->psl))
        {
        aln->drop = TRUE;
        cdna->stats->badCnts.aligns++;
        cDnaAlignVerb(2, aln->psl, "drop: invalid psl");
        }
}

static void minQSizeFilter(struct cDnaQuery *cdna)
/* filter by minimum query size */
{
struct cDnaAlign *aln;
/* note that qSize is actually the same for all alignments */
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && (aln->psl->qSize < gMinQSize))
        {
        aln->drop = TRUE;
        cdna->stats->minQSizeCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: min query size %0.4g", aln->psl->qSize);
        }
}

static void identFilter(struct cDnaQuery *cdna)
/* filter by fraction identity */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && (aln->ident < gMinId))
        {
        aln->drop = TRUE;
        cdna->stats->minIdCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: min ident %0.4g", aln->ident);
        }
}

static void coverFilter(struct cDnaQuery *cdna)
/* filter by coverage */
{
struct cDnaAlign *aln;

/* drop those that are under min */
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && (aln->cover < gMinCover))
        {
        aln->drop = TRUE;
        cdna->stats->minCoverCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: min cover %0.4g", aln->cover);
        }
}

static void nonRepLenFilter(struct cDnaQuery *cdna)
/* filter by minNonRepLen */
{
struct cDnaAlign *aln;

/* drop those that are over max */
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    /* don't included poly-A length, although we are not sure if it's aligned
     * to a repeat. */
    int nonRepAln = ((aln->psl->match + aln->psl->misMatch) - aln->alnPolyAT);
    if ((!aln->drop) && (nonRepAln < gMinNonRepLen))
        {
        aln->drop = TRUE;
        cdna->stats->minNonRepLenCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: min non-rep len %d", nonRepAln);
        }
    }
}

static void repMatchFilter(struct cDnaQuery *cdna)
/* filter by maxRepMatch */
{
struct cDnaAlign *aln;

/* drop those that are over max */
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if ((!aln->drop) && (aln->repMatch > gMaxRepMatch))
        {
        aln->drop = TRUE;
        cdna->stats->maxRepMatchCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: max repMatch %0.4g", aln->repMatch);
        }
    }
}

static struct cDnaAlign *findMaxAlign(struct cDnaQuery *cdna)
/* find first alignment over max size */
{
struct cDnaAlign *aln;
int cnt;
for (aln = cdna->alns, cnt = 0; (aln != NULL) && (cnt < gMaxAligns);
     aln = aln->next)
    {
    if (!aln->drop)
        cnt++;
    }
return aln;
}

static void maxAlignFilter(struct cDnaQuery *cdna)
/* filter by maximum number of alignments */
{
struct cDnaAlign *aln;
cDnaQueryRevScoreSort(cdna);
for (aln = findMaxAlign(cdna); aln != NULL; aln = aln->next)
    if (!aln->drop)
        {
        aln->drop = TRUE;
        cdna->stats->maxAlignsCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: max aligns");
        }
}

static int getTargetSpan(struct cDnaAlign *aln)
/* get the target span for an alignment */
{
return (aln->psl->tEnd - aln->psl->tStart);
}

static void minSpanFilter(struct cDnaQuery *cdna)
/* Filter by fraction of target length of maximum longest passing the other
 * filters.  This can be useful for removing possible retroposed genes.
 * Suggested by Jeltje van Baren.*/
{
int longestSpan = 0;
int minSpanLen;
struct cDnaAlign *aln;

/* find longest span */
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    if (!aln->drop)
        longestSpan = max(longestSpan, getTargetSpan(aln));

/* filter by under this amount  */
minSpanLen = gMinSpan*longestSpan;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && (getTargetSpan(aln) < minSpanLen))
        {
        aln->drop = TRUE;
        cdna->stats->minSpanCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: minSpan span %d, min is %d (%0.2f)",
                      getTargetSpan(aln), minSpanLen, ((float)getTargetSpan(aln))/longestSpan);
        }
}

static void baseScoreUpdate(struct cDnaQuery *cdna, struct cDnaAlign *aln,
                            float *baseScores)
/* update best local query scores for an alignment psl.*/
{
struct psl *psl = aln->psl;
int iBlk, i;

for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    struct cDnaRange q = cDnaQueryBlk(cdna, psl, iBlk);
    for (i = q.start; i < q.end; i++)
        {
        if (aln->score > baseScores[i])
            baseScores[i] = aln->score;
        }
    }
}

static float *computeLocalScores(struct cDnaQuery *cdna)
/* compute local per-base score array for a cDNA. */
{
float *baseScores;
struct cDnaAlign *aln;
AllocArray(baseScores, cdna->alns->psl->qSize);

for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop)
        baseScoreUpdate(cdna, aln, baseScores);
    }
return baseScores;
}

static boolean isLocalNearBest(struct cDnaQuery *cdna, struct cDnaAlign *aln,
                               float *baseScores)
/* check if aligned blocks pass local near-beast filter. */
{
float near = (1.0-gLocalNearBest);
struct psl *psl = aln->psl;
int iBlk, i;
int topCnt = 0;

for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    struct cDnaRange q = cDnaQueryBlk(cdna, psl, iBlk);
    for (i = q.start; i < q.end; i++)
        {
        if (aln->score >= baseScores[i]*near)
            {
            if (++topCnt >= gMinLocalBestCnt)
                return TRUE;
            }
        }
    }
return FALSE;
}

static void localNearBestFilter(struct cDnaQuery *cdna)
/* local near best in genome filter. */
{
float *baseScores = computeLocalScores(cdna);
struct cDnaAlign *aln;

for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if ((!aln->drop) && !isLocalNearBest(cdna, aln, baseScores))
        {
        aln->drop = TRUE;
        cdna->stats->localBestCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: local near best %0.4g",
                      aln->score);
        }
    }

freeMem(baseScores);
}

static float getBestScore(struct cDnaQuery *cdna)
/* find best score for alignments that have not been dropped */
{
float best = -1.0;
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && (aln->score > best))
        best = aln->score;
return best;
}

static void globalNearBestFilter(struct cDnaQuery *cdna)
/* global near best in genome filter. */
{
float best = getBestScore(cdna);
float thresh = best*(1.0-gGlobalNearBest);
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && (aln->score < thresh))
        {
        aln->drop = TRUE;
        cdna->stats->globalBestCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: global near best %0.4g, best=%0.4g, th=%0.4g",
                      aln->score, best, thresh);
        }
}

static void filterQuery(struct cDnaQuery *cdna,
                        FILE *outPslFh, FILE *dropPslFh, FILE *weirdOverPslFh)
/* filter the current query set of alignments in cdna */
{
/* n.b. order should agree with doc */
invalidPslFilter(cdna);
if (gMinQSize > 0)
    minQSizeFilter(cdna);
if (gMinNonRepLen > 0)
    nonRepLenFilter(cdna);
if (gMaxRepMatch < 1.0)
    repMatchFilter(cdna);
if (gMinId > 0.0)
    identFilter(cdna);
if (gMinCover > 0.0)
    coverFilter(cdna);
if (gBestOverlap)
    overlapFilter(cdna);
if (gMinSpan > 0.0)
    minSpanFilter(cdna);
if (gLocalNearBest >= 0.0)
    localNearBestFilter(cdna);
if (gGlobalNearBest >= 0.0)
    globalNearBestFilter(cdna);
if (gMaxAligns >= 0)
    maxAlignFilter(cdna);
cDnaQueryWriteKept(cdna, outPslFh);
if (dropPslFh != NULL)
    cDnaQueryWriteDrop(cdna, dropPslFh);
if (weirdOverPslFh != NULL)
    cDnaQueryWriteWeird(cdna, weirdOverPslFh);
}

static void verbStats(char *label, struct cDnaCnts *cnts, boolean always)
/* output stats */
{
if ((cnts->aligns > 0) || always)
    verbose(1, "%18s:\t%d\t%d\t%d\n", label, cnts->queries, cnts->aligns,
            cnts->multAlnQueries);
}

static void pslCDnaFilter(char *inPsl, char *outPsl)
/* filter cDNA alignments in psl format */
{
struct cDnaReader *reader = cDnaReaderNew(inPsl, gUsePolyTHead, gPolyASizes);
struct cDnaStats *stats = &reader->stats;
FILE *outPslFh = mustOpen(outPsl, "w");
FILE *dropPslFh = NULL;
FILE *weirdOverPslFh = NULL;
if (gDropped != NULL)
    dropPslFh = mustOpen(gDropped, "w");
if (gWeirdOverlappped != NULL)
    weirdOverPslFh = mustOpen(gWeirdOverlappped, "w");

while (cDnaReaderNext(reader))
    filterQuery(reader->cdna, outPslFh, dropPslFh, weirdOverPslFh);

carefulClose(&dropPslFh);
carefulClose(&weirdOverPslFh);
carefulClose(&outPslFh);

verbose(1,"%18s \tseqs\taligns\tmultAlnSeqs\n", "");
verbStats("total", &stats->totalCnts, FALSE);
verbStats("drop invalid", &stats->badCnts, FALSE);
verbStats("drop minNonRepLen", &stats->minNonRepLenCnts, FALSE);
verbStats("drop maxRepMatch", &stats->maxRepMatchCnts, FALSE);
verbStats("drop min size", &stats->minQSizeCnts, FALSE);
verbStats("drop minIdent", &stats->minIdCnts, FALSE);
verbStats("drop minCover", &stats->minCoverCnts, FALSE);
verbStats("weird over", &stats->weirdOverCnts, FALSE);
verbStats("kept weird", &stats->weirdKeptCnts, FALSE);
verbStats("drop overlap", &stats->overlapCnts, FALSE);
verbStats("drop minSpan", &stats->minSpanCnts, FALSE);
verbStats("drop localBest", &stats->localBestCnts, FALSE);
verbStats("drop globalBest", &stats->globalBestCnts, FALSE);
verbStats("drop maxAligns", &stats->maxAlignsCnts, FALSE);
verbStats("kept", &stats->keptCnts, TRUE);

cDnaReaderFree(&reader);
}

static float optionFrac(char *name, float def)
/* get an option that must be in the range 0.0 and 1.0, or -1.0 to disable. */
{
float val = optionFloat(name, def);
if (((val < 0.0) || (val > 1.0)) && (val != -1.0))
    errAbort("-%s must be in the range 0.0..1.0 (or -1.0 to disable)", name);
return val;
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage("wrong # of args");

gLocalNearBest = optionFrac("localNearBest", gLocalNearBest);
gGlobalNearBest = optionFrac("globalNearBest", gGlobalNearBest);
if ((gLocalNearBest >= 0.0) && (gGlobalNearBest >= 0.0))
    errAbort("can only specify one of -localNearBest and -gLocalNearBest");
gMinId = optionFrac("minId", gMinId);
gMinCover = optionFrac("minCover", gMinCover);
gMinSpan = optionFrac("minSpan", gMinSpan);
gMinQSize = optionInt("minQSize", gMinQSize);
gMaxAligns = optionInt("maxAligns", gMaxAligns);
gMinNonRepLen = optionInt("minNonRepLen", gMinNonRepLen);
gMaxRepMatch = optionFrac("maxRepMatch", gMaxRepMatch);
gPolyASizes = optionVal("polyASizes", NULL);
gUsePolyTHead = optionExists("usePolyTHead");
if (gUsePolyTHead && (gPolyASizes = NULL))
    errAbort("must specify -polyASizes with -usePolyTHead");
gBestOverlap = optionExists("bestOverlap");
gDropped = optionVal("dropped", NULL);
gWeirdOverlappped = optionVal("weirdOverlapped", NULL);
gAlignStats = optionVal("alignStats", gAlignStats);

pslCDnaFilter(argv[1], argv[2]);
return 0;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

