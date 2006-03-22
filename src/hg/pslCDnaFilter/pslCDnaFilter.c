/* filter cDNA alignments in psl format */
#include "common.h"
#include "cDnaAligns.h"
#include "cDnaReader.h"
#include "overlapFilter.h"
#include "hapRegions.h"
#include "psl.h"
#include "options.h"

static void usage(char *msg)
/* usage msg and exit */
{
/* message got huge, so it's in a generate file */
static char *usageMsg =
#include "usage.msg"
    ;
errAbort("%s:  %s", msg, usageMsg);
}

static void prAlgo()
/* print algorithm description and exit */
{
/* message got huge, so it's in a generate file */
static char *algoMsg =
#include "algo.msg"
    ;
fprintf(stderr, "%s", algoMsg);
exit(0);
}


/* command line options and values */
static struct optionSpec optionSpecs[] =
{
    {"algoHelp", OPTION_BOOLEAN},
    {"localNearBest", OPTION_FLOAT},
    {"globalNearBest", OPTION_FLOAT},
    {"ignoreNs", OPTION_BOOLEAN},
    {"minId", OPTION_FLOAT},
    {"minCover", OPTION_FLOAT},
    {"minSpan", OPTION_FLOAT},
    {"minQSize", OPTION_INT},
    {"maxAligns", OPTION_INT},
    {"minAlnSize", OPTION_INT},
    {"minNonRepSize", OPTION_INT},
    {"maxRepMatch", OPTION_FLOAT},
    {"polyASizes", OPTION_STRING},
    {"usePolyTHead", OPTION_BOOLEAN},
    {"hapRegions", OPTION_STRING},
    {"bestOverlap", OPTION_BOOLEAN},
    {"dropped", OPTION_STRING},
    {"weirdOverlapped", OPTION_STRING},
    {"alignStats", OPTION_STRING},
    {"hapRefMapped", OPTION_STRING},
    {"hapRefCDnaAlns", OPTION_STRING},
    {"alnIdQNameMode", OPTION_BOOLEAN},
    {NULL, 0}
};

static float gLocalNearBest = -1.0;    /* local near best in genome */
static float gGlobalNearBest = -1.0;   /* global near best in genome */
static unsigned gCDnaOpts = 0;         /* options for cDnaReader */
static float gMinId = 0.0;             /* minimum fraction id */
static float gMinCover = 0.0;          /* minimum coverage */
static float gMinSpan = 0.0;           /* minimum target span allowed */
static int gMinQSize = 0;              /* drop queries shorter than this */
static int gMaxAligns = -1;            /* only allow this many alignments for a query */
static int gMinAlnSize = 0;            /* minimum bases that must be aligned */
static int gMinNonRepSize = 0;         /* minimum non-repeat bases that must match */
static float gMaxRepMatch = 1.0;       /* maximum fraction of repeat matching */
static char *gPolyASizes = NULL;       /* polyA size file */
static char *gHapRegions = NULL;       /* haplotype regions file */
static boolean gBestOverlap = FALSE;   /* filter overlaping, keeping only the best */
static char *gDropped = NULL;          /* save dropped psls here */
static char *gWeirdOverlappped = NULL; /* save weird overlapping psls here */
static int gMinLocalBestCnt = 30;      /* minimum number of bases that are over threshold
                                        * for local best */
#if 0 // FIXME:
static float gHapRefNearTop = 0.01;    /* near-top cut off for hap-ref alignment
                                        * association */
#endif
static char *gHapRefMapped = NULL;    /* PSLs of haplotype to reference chromosome
                                       * mappings */
static char *gHapRefCDnaAlns = NULL;  /* PSLs of haplotype cDNA to reference
                                       * cDNA alignments */

struct outFiles
/* open output files */
{
    FILE *passFh;              /* filtered psls */
    FILE *dropFh;              /* dropped psls, or NULL */
    FILE *weirdOverFh;         /* wierd overlap psls, or NULL */
    FILE *hapRefMappedFh;      /* hap to ref cDNA alignment mappings */
    FILE *hapRefCDnaAlnsFh;    /* cDNA to cDNA alignments. */
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
    if (!aln->drop && !validPsl(aln->psl))
        {
        cDnaAlignDrop(aln, &cdna->stats->badCnts);
        cDnaAlignVerb(2, aln, "drop: invalid psl");
        }
}

static void minQSizeFilter(struct cDnaQuery *cdna)
/* filter by minimum query size */
{
struct cDnaAlign *aln;
/* note that qSize is actually the same for all alignments */
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    if (!aln->drop && (aln->psl->qSize < gMinQSize))
        {
        cDnaAlignDrop(aln, &cdna->stats->minQSizeCnts);
        cDnaAlignVerb(3, aln, "drop: min query size %0.4g", aln->psl->qSize);
        }
}

static void identFilter(struct cDnaQuery *cdna)
/* filter by fraction identity */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    if (!aln->drop && (aln->ident < gMinId))
        {
        cDnaAlignDrop(aln, &cdna->stats->minIdCnts);
        cDnaAlignVerb(3, aln, "drop: min ident %0.4g", aln->ident);
        }
}

static void coverFilter(struct cDnaQuery *cdna)
/* filter by coverage */
{
struct cDnaAlign *aln;

/* drop those that are under min */
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    if (!aln->drop && (aln->cover < gMinCover))
        {
        cDnaAlignDrop(aln, &cdna->stats->minCoverCnts);
        cDnaAlignVerb(3, aln, "drop: min cover %0.4g", aln->cover);
        }
}

static void alnSizeFilter(struct cDnaQuery *cdna)
/* filter by alnRepSize */
{
struct cDnaAlign *aln;

/* drop those that are over max */
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    /* don't included poly-A length. */
    struct psl *psl = aln->psl;
    int alnSize = ((psl->match + psl->repMatch + aln->adjMisMatch) - aln->alnPolyAT);
    if (!aln->drop && (alnSize < gMinAlnSize))
        {
        cDnaAlignDrop(aln, &cdna->stats->minAlnSizeCnts);
        cDnaAlignVerb(3, aln, "drop: min align size %d", alnSize);
        }
    }
}

static void nonRepSizeFilter(struct cDnaQuery *cdna)
/* filter by minNonRepSize */
{
struct cDnaAlign *aln;

/* drop those that are over max */
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    /* don't included poly-A length, although we are not sure if it's aligned
     * to a repeat. Also, don't include mismatches, as these are not reported
     * by blat */
    int nonRepSize = (aln->psl->match - aln->alnPolyAT);
    if (nonRepSize < 0)
        nonRepSize = 0;
    if (!aln->drop && (nonRepSize < gMinNonRepSize))
        {
        cDnaAlignDrop(aln, &cdna->stats->minNonRepSizeCnts);
        cDnaAlignVerb(3, aln, "drop: min non-rep size %d", nonRepSize);
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
    if (!aln->drop && (aln->repMatch > gMaxRepMatch))
        {
        cDnaAlignDrop(aln, &cdna->stats->maxRepMatchCnts);
        cDnaAlignVerb(3, aln, "drop: max repMatch %0.4g", aln->repMatch);
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
        cDnaAlignDrop(aln, &cdna->stats->maxAlignsCnts);
        cDnaAlignVerb(3, aln, "drop: max aligns");
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
    if (!aln->drop && (getTargetSpan(aln) < minSpanLen))
        {
        cDnaAlignDrop(aln, &cdna->stats->minSpanCnts);
        cDnaAlignVerb(3, aln, "drop: minSpan span %d, min is %d (%0.2f)",
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
    struct range q = cDnaQueryBlk(cdna, psl, iBlk);
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

static void localNearBestVerb(struct cDnaAlign *aln, boolean pass,
                              float nearScore, int topCnt, int chkCnt,
                              float minBest, float maxBest)
/* verbose tracing for isLocalNearBest */
{
cDnaAlignVerb(5, aln, "isLocalNearBest: %s nearScore: %0.3f %d in %d best: %0.3f .. %0.3f",
              (pass ? "yes" : "no "), nearScore, topCnt, chkCnt, 
              minBest, maxBest);
}

static boolean isLocalNearBest(struct cDnaQuery *cdna, struct cDnaAlign *aln,
                               float *baseScores)
/* check if aligned blocks pass local near-beast filter. */
{
float nearScore = aln->score * (1.0+gLocalNearBest);
struct psl *psl = aln->psl;
int iBlk, i;
int chkCnt = 0, topCnt = 0;
float minBest = 0.0, maxBest = 0.0;

for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    struct range q = cDnaQueryBlk(cdna, psl, iBlk);
    for (i = q.start; i < q.end; i++)
        {
        if (chkCnt == 0)
            minBest = maxBest = baseScores[i];
        else
            {
            minBest = min(minBest, baseScores[i]);
            maxBest = max(maxBest, baseScores[i]);
            }
        chkCnt++;
        if (nearScore >= baseScores[i])
            {
            if (++topCnt >= gMinLocalBestCnt)
                {
                localNearBestVerb(aln, TRUE, nearScore, topCnt, chkCnt, minBest, maxBest);
                return TRUE;
                }
            }
        }
    }
localNearBestVerb(aln, FALSE, nearScore, topCnt, chkCnt, minBest, maxBest);
return FALSE;
}

static void localNearBestFilter(struct cDnaQuery *cdna)
/* Local near best in genome filter. Algorithm is based on
 * pslReps.  This avoids dropping exons in drafty genomes.
 *
 * 1) Assign a score to each alignment that is weight heavily by percent id
 *    and has a bonus for apparent introns.
 * 2) Create a per-mRNA base vector of near-best scores:
 *    - foreach alignment:
 *      - foreach base of the alignment:
 *        - if the alignment's score is greater than the score at the position
 *          in the vector, replaced the vector score.
 * 3) Foreach alignment:
 *    - count the number of bases within the near-top range of the best
 *     score for the base.
 * 4) If an alignment has at least 30 of it's bases within near-top, keep the
 *    alignment.
 */
{
float *baseScores = computeLocalScores(cdna);
struct cDnaAlign *aln;

for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop && !isLocalNearBest(cdna, aln, baseScores))
        {
        cDnaAlignDrop(aln, &cdna->stats->localBestCnts);
        cDnaAlignVerb(3, aln, "drop: local near best %0.4g",
                      aln->score);
        }
    }

freeMem(baseScores);
}

static boolean isLinkedHap(struct cDnaAlign *aln)
/* determine if a cDNA is a haplotype linked to a reference sequence */
{
return (aln->isHapChrom && aln->refLinkCount > 0);
}

static float getAlnScore(struct cDnaAlign *aln)
/* get the score for an alignment, getting best score for a refAln that is
 * linked, */
{
float best = aln->score;
if (aln->hapAlns != NULL)
    {
    struct cDnaHapAln *hapAln;
    for (hapAln = aln->hapAlns; hapAln != NULL; hapAln = hapAln->next)
        best = max(best, hapAln->hapAln->score);
    }
return best;
}

static float getBestScore(struct cDnaQuery *cdna)
/* find best score for alignments that have not been dropped */
{
float best = -1.0;
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop && !isLinkedHap(aln))
        {
        float score = getAlnScore(aln);
        best = max(score, best);
        }
    }
return best;
}

static void globalNearBestDrop(struct cDnaAlign *aln, float score, float best,
                               float thresh)
/* drop alignment for global near best */
{
cDnaAlignDrop(aln, &aln->cdna->stats->globalBestCnts);
cDnaAlignVerb(3, aln, "drop: global near best %0.4g, best=%0.4g, th=%0.4g%s",
              score, best, thresh, ((aln->hapAlns != NULL) ? " refLinked" : ""));

/* if there are referenced alignments, decrement ref count, maybe dropping */
if (aln->hapAlns != NULL)
    {
    struct cDnaHapAln *hapAln;
    for (hapAln = aln->hapAlns; hapAln != NULL; hapAln = hapAln->next)
        if (--hapAln->hapAln->refLinkCount == 0)
            {
            cDnaAlignDrop(hapAln->hapAln, &hapAln->hapAln->cdna->stats->globalBestCnts);
            cDnaAlignVerb(3, hapAln->hapAln, "drop: global near best hapLinked %0.4g ",
                          hapAln->hapAln->score);
            }
    }
}

static void globalNearBestFilter(struct cDnaQuery *cdna)
/* global near best in genome filter. */
{
float best = getBestScore(cdna);
float thresh = best*(1.0-gGlobalNearBest);
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    if (!aln->drop && !isLinkedHap(aln))
        {
        float score = getAlnScore(aln);
        if (score < thresh)
            globalNearBestDrop(aln, score, best, thresh);
        }
}

static void filterQuery(struct cDnaQuery *cdna, struct hapRegions *hapRegions,
                        struct outFiles *outFiles)
/* filter the current query set of alignments in cdna */
{
/* n.b. order should agree with doc */
invalidPslFilter(cdna);
if (gMinQSize > 0)
    minQSizeFilter(cdna);
if (gMinAlnSize > 0)
    alnSizeFilter(cdna);
if (gMinNonRepSize > 0)
    nonRepSizeFilter(cdna);
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
/* begin comparative filters */
if (hapRegions != NULL)
    hapRegionsLink(hapRegions, cdna, outFiles->hapRefMappedFh, outFiles->hapRefCDnaAlnsFh);
if (gLocalNearBest >= 0.0)
    localNearBestFilter(cdna);
if (gGlobalNearBest >= 0.0)
    globalNearBestFilter(cdna);
if (gMaxAligns >= 0)
    maxAlignFilter(cdna);
cDnaQueryWriteKept(cdna, outFiles->passFh);
if (outFiles->dropFh != NULL)
    cDnaQueryWriteDrop(cdna, outFiles->dropFh);
if (outFiles->weirdOverFh != NULL)
    cDnaQueryWriteWeird(cdna, outFiles->weirdOverFh);
}

static void pslCDnaFilter(char *inPsl, char *outPsl)
/* filter cDNA alignments in psl format */
{
struct hapRegions *hapRegions = (gHapRegions == NULL) ? NULL : hapRegionsNew(gHapRegions);
struct cDnaReader *reader = cDnaReaderNew(inPsl, gCDnaOpts, gPolyASizes, hapRegions);
struct outFiles outFiles;
ZeroVar(&outFiles);
outFiles.passFh = mustOpen(outPsl, "w");
if (gDropped != NULL)
    outFiles.dropFh = mustOpen(gDropped, "w");
if (gWeirdOverlappped != NULL)
    outFiles.weirdOverFh = mustOpen(gWeirdOverlappped, "w");
if (gHapRefMapped != NULL)
    outFiles.hapRefMappedFh = mustOpen(gHapRefMapped, "w");
if (gHapRefCDnaAlns != NULL)
    outFiles.hapRefCDnaAlnsFh = mustOpen(gHapRefCDnaAlns, "w");

while (cDnaReaderNext(reader))
    filterQuery(reader->cdna, hapRegions, &outFiles);

carefulClose(&outFiles.hapRefMappedFh);
carefulClose(&outFiles.hapRefCDnaAlnsFh);
carefulClose(&outFiles.dropFh);
carefulClose(&outFiles.weirdOverFh);
carefulClose(&outFiles.passFh);
cDnaStatsPrint(&reader->stats, 1);
hapRegionsFree(&hapRegions);
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
if (optionExists("algoHelp"))
    prAlgo();
if (argc != 3)
    usage("wrong # of args");

gLocalNearBest = optionFrac("localNearBest", gLocalNearBest);
gGlobalNearBest = optionFrac("globalNearBest", gGlobalNearBest);
if ((gLocalNearBest >= 0.0) && (gGlobalNearBest >= 0.0))
    errAbort("can only specify one of -localNearBest and -globalNearBest");
if (optionExists("usePolyTHead"))
    gCDnaOpts |= cDnaUsePolyTHead;
if (optionExists("ignoreNs"))
    gCDnaOpts |= cDnaIgnoreNs;
gMinId = optionFrac("minId", gMinId);
gMinCover = optionFrac("minCover", gMinCover);
gMinSpan = optionFrac("minSpan", gMinSpan);
gMinQSize = optionInt("minQSize", gMinQSize);
gMaxAligns = optionInt("maxAligns", gMaxAligns);
gMinAlnSize = optionInt("minAlnSize", gMinAlnSize);
gMinNonRepSize = optionInt("minNonRepSize", gMinNonRepSize);
gMaxRepMatch = optionFrac("maxRepMatch", gMaxRepMatch);
gPolyASizes = optionVal("polyASizes", NULL);
if (optionExists("usePolyTHead") && (gPolyASizes == NULL))
    errAbort("must specify -polyASizes with -usePolyTHead");
gHapRegions = optionVal("hapRegions", NULL);
gBestOverlap = optionExists("bestOverlap");
gDropped = optionVal("dropped", NULL);
gWeirdOverlappped = optionVal("weirdOverlapped", NULL);
gHapRefMapped = optionVal("hapRefMapped", NULL);
gHapRefCDnaAlns = optionVal("hapRefCDnaAlns", NULL);
alnIdQNameMode = optionExists("alnIdQNameMode");

pslCDnaFilter(argv[1], argv[2]);
return 0;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

