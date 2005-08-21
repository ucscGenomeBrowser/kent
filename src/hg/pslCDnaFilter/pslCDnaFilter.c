/* filter cDNA alignments in psl format */
#include "common.h"
#include "cDnaAligns.h"
#include "overlapFilter.h"
#include "psl.h"
#include "options.h"

void usage(char *msg)
/* usage msg and exit */
{
errAbort("%s\n%s", msg,
         "pslCDnaFilter [options] inPsl outPsl\n"
         "\n"
         "Filter cDNA alignments in psl format.  Filtering parameters are\n"
         "comparative and non-comparative.  Comparative filters select the\n"
         "best alignments for each query and REQUIRES THE INPUT TO BE SORTED\n"
         "BY QUERY NAME.  Non-comparative filters are based only on the quality\n"
         "of an individual alignment.\n"
         "\n"
         "The default values don't do any filtering, so if no filtering criteria\n"
         "are specified, all PSL should be passed though, except those that are\n"
         "inconsistent.\n"
         "\n"
         "Options:\n"
         "   -nonComparative - only do non-comparative filtering.  This is\n"
         "    use to prefilter before all of the alignments for a query have\n"
         "    been combined.\n"
         "   -minId=0.0 - only keep alignments with at least this fraction\n"
         "    identity.\n"
         "   -idNearTop=1.0 - keep alignments within this fraction\n"
         "    from top ident.\n"
         "   -minCover=0.0 - minimum fraction of query that must be aligned.\n"
         "   -coverNearTop=1.0 - keep alignments within this fraction of\n"
         "    the top coverage alignment. If -polyASizes is specified and the query\n"
         "    is in the file, the ploy-A is not included in coverage calculation.\n"
         "   -coverWeight=0.5 - weight of coverage vs identity in critera that\n"
         "    select between alignments. A value of 0.75 would put 3/4 of the\n"
         "    weight on coverage and 1/4 on identity.\n"
         "   -minSpan=0.0 - Keep only alignments whose target length are at least this fraction of the\n"
         "    longest alignment passing the other filters.  This can be useful for removing possible\n"
         "    retroposed genes.\n"
         "   -minQSize=0 - drop queries shorter than this size\n"
         "   -maxAligns=-1 - maximum number of alignments for a given query. If\n"
         "    exceeded, then alignments are sorted by weighed coverage and\n"
         "    identity an the those over this are dropped, A value of -1\n"
         "    disables (default)\n"
         "   -maxRepMatch=1.0 - Maximum fraction of matching aligned bases that\n"
         "    are repeats.  Must use -repeats on BLAT if doing unmasked alignments.\n"
         "   -polyASizes=file - tab separate file as output by faPolyASizes, columns are:\n"
         "        id seqSize tailPolyASize headPolyTSize\n"
         "   -bestOverlap - filter overlapping alignments, keeping the best of aligments that\n"
         "    are similar, but not discarding ones with weird overlap.\n"
         "   -dropped=psl - save psls that were dropped to this file.\n"
         "   -weirdOverlapped=psl - output weirdly overlapping PSLs to\n"
         "    this file.\n"
         "   -alignStats=file - output the per-alignment statistics to this file\n"
         "   -verbose=1 - 0: quite\n"
         "                1: output stats\n"
         "                2: list problem alignment (weird or invalid)\n"
         "                3: list dropped alignments and reason for dropping\n"
         "                4: list kept psl and info\n"
         "                5: info about all PSLs\n"
         "\n"
         "Warning: inPsl MUST be sorted by qName if comparative filters\n"
         "are used.\n"
         "\n"
         "This filters for the following criteria and in this order:\n"
         "  o PSLs that are not internally consistent, such has having\n"
         "    overlapping blocks, are dropped.  Use pslCheck program to\n"
         "    get the details.\n"
         "  o Drop queries less than the min size, if specified\n"
         "  o Drop if repMatch/totalMatch is greater than maxRepMatch.\n"
         "  o Overlapping alignments, trying to keep the one with the most\n"
         "    coverage, but not dropping weird overlaps. These are sets of\n"
         "    alignments for a given query that overlap, however the same\n"
         "    query bases align different target bases.  These will be\n"
         "    included in the output unless dropped by another filter criteria.\n"
         "    These are often caused by tandem repeats.  A small fraction\n"
         "    of differently aligned bases are allowed to handle different block\n"
         "    boundaries.\n"
         "  o By minimum identity to the target sequence.\n"
         "  o By minimum coverage, if polyA sizes are supplied, the polyA tail\n"
         "    is not included in coverage calculation.\n"
         "  o By identity near top, only keeping alignments with idNearTop of\n"
         "    highest identity alignment.\n"
         "  o By coverage near top, only keeping alignments with coverNearTop of\n"
         "    highest coverage alignment.\n"
         "  o By minSpan, if specified\n"
         "  o By maxAligns, if specified\n"
         "\n"
         "THE INPUT MUST BE BE SORTED BY QUERY for the comparitive filters.\n");
}

/* command line options and values */
static struct optionSpec optionSpecs[] =
{
    {"nonComparative", OPTION_BOOLEAN},
    {"minId", OPTION_FLOAT},
    {"idNearTop", OPTION_FLOAT},
    {"minCover", OPTION_FLOAT},
    {"coverNearTop", OPTION_FLOAT},
    {"coverWeight", OPTION_FLOAT},
    {"maxRepMatch", OPTION_FLOAT},
    {"minQSize", OPTION_INT},
    {"maxAligns", OPTION_INT},
    {"minSpan", OPTION_FLOAT},
    {"bestOverlap", OPTION_BOOLEAN},
    {"polyASizes", OPTION_STRING},
    {"dropped", OPTION_STRING},
    {"alignStats", OPTION_STRING},
    {"weirdOverlapped", OPTION_STRING},
    {NULL, 0}
};

/* options that are comparative */
char *comparativeOpts[] = {
    "idNearTop", "coverNearTop", "coverWeight", "minSpan",
    "maxAligns", "bestOverlap", "weirdOverlapped", 
    NULL
};

char *gPolyASizes = NULL;       /* polyA size file */
char *gDropped = NULL;          /* save dropped psls here */
char *gWeirdOverlappped = NULL; /* save weird overlapping psls here */
float gMinId = 0.0;             /* minimum fraction id */
float gIdNearTop = 1.0;         /* keep within this fraction of the top */
float gMinCover = 0.0;          /* minimum coverage */
float gCoverNearTop = 1.0;      /* keep within this fraction of best cover */
float gCoverWeight = 0.5;       /* weight of cover vs id */
float gMaxRepMatch = 1.0;       /* maximum repeat match/aligned */
float gMinSpan = 0.0;           /* minimum target span allowed */
int gMinQSize = 0;              /* drop queries shorter than this */
int gMaxAligns = -1;            /* only allow this many alignments for a query
                                 * -1 disables check. */
boolean gBestOverlap = FALSE;   /* filter overlaping, keeping only the best */
char *gAlignStats = NULL;       /* file for statistics output */


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

static void invalidPslFilter(struct cDnaAligns *cdAlns)
/* filter for invalid PSL */
{
struct cDnaAlign *aln;
for (aln = cdAlns->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && !validPsl(aln->psl))
        {
        aln->drop = TRUE;
        cdAlns->badCnts.aligns++;
        cDnaAlignVerb(2, aln->psl, "drop: invalid psl");
        }
}

static void minQSizeFilter(struct cDnaAligns *cdAlns)
/* filter by minimum query size */
{
struct cDnaAlign *aln;
/* note that qSize is actually the same for all alignments */
for (aln = cdAlns->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && (aln->psl->qSize < gMinQSize))
        {
        aln->drop = TRUE;
        cdAlns->minQSizeCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: min query size %0.4g", aln->psl->qSize);
        }
}

static void identFilter(struct cDnaAligns *cdAlns)
/* filter by fraction identity */
{
struct cDnaAlign *aln;
for (aln = cdAlns->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && (aln->ident < gMinId))
        {
        aln->drop = TRUE;
        cdAlns->minIdCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: min ident %0.4g", aln->ident);
        }
}

static float getMaxIdent(struct cDnaAligns *cdAlns)
/* get the maximum ident  */
{
float maxIdent = 0.0;
struct cDnaAlign *aln;
for (aln = cdAlns->alns; aln != NULL; aln = aln->next)
    if (!aln->drop)
        maxIdent = max(maxIdent, aln->ident);
return maxIdent;
}

static void identNearTopFilter(struct cDnaAligns *cdAlns)
/* filter by fraction identity near the top */
{
float maxIdent = getMaxIdent(cdAlns);
float thresh = maxIdent*(1.0-gIdNearTop);
struct cDnaAlign *aln;
for (aln = cdAlns->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && (aln->ident < thresh))
        {
        aln->drop = TRUE;
        cdAlns->idTopCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: id near top %0.4g, top=%0.4g, th=%0.4g",
                      aln->ident, maxIdent, thresh);
        }
}

static void coverFilter(struct cDnaAligns *cdAlns)
/* filter by coverage */
{
struct cDnaAlign *aln;

/* drop those that are under min */
for (aln = cdAlns->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && (aln->cover < gMinCover))
        {
        aln->drop = TRUE;
        cdAlns->minCoverCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: min cover %0.4g", aln->cover);
        }
}

static float getMaxCover(struct cDnaAligns *cdAlns)
/* get the maximum coverage */
{
float maxCover = 0.0;
struct cDnaAlign *aln;
for (aln = cdAlns->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop)
        maxCover = max(maxCover, aln->cover);
    }
return maxCover;
}

static void coverNearTopFilter(struct cDnaAligns *cdAlns)
/* filter by coverage */
{
float maxCover = getMaxCover(cdAlns);
float thresh = maxCover*(1.0-gCoverNearTop);
struct cDnaAlign *aln;

for (aln = cdAlns->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && (aln->cover < thresh))
        {
        aln->drop = TRUE;
        cdAlns->coverTopCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: cover near top %0.4g, top=%0.4g, th=%0.4g",
                      aln->cover, maxCover, thresh);
        }
}

static void repMatchFilter(struct cDnaAligns *cdAlns)
/* filter by maxRepMatch */
{
struct cDnaAlign *aln;

/* drop those that are over max */
for (aln = cdAlns->alns; aln != NULL; aln = aln->next)
    {
    if ((!aln->drop) && (aln->repMatch > gMaxRepMatch))
        {
        aln->drop = TRUE;
        cdAlns->maxRepMatchCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: max repMatch %0.4g", aln->repMatch);
        }
    }
}

static struct cDnaAlign *findMaxAlign(struct cDnaAligns *cdAlns)
/* find first alignment over max size */
{
struct cDnaAlign *aln;
int cnt;
for (aln = cdAlns->alns, cnt = 0; (aln != NULL) && (cnt < gMaxAligns);
     aln = aln->next)
    {
    if (!aln->drop)
        cnt++;
    }
return aln;
}

static void maxAlignFilter(struct cDnaAligns *cdAlns)
/* filter by maximum number of alignments */
{
struct cDnaAlign *aln;
cDnaAlignsRevScoreSort(cdAlns);
for (aln = findMaxAlign(cdAlns); aln != NULL; aln = aln->next)
    if (!aln->drop)
        {
        aln->drop = TRUE;
        cdAlns->maxAlignsCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: max aligns");
        }
}

static int getTargetSpan(struct cDnaAlign *aln)
/* get the target span for an alignment */
{
return (aln->psl->tEnd - aln->psl->tStart);
}

static void minSpanFilter(struct cDnaAligns *cdAlns)
/* Filter by fraction of target length of maximum longest passing the other
 * filters.  This can be useful for removing possible retroposed genes.
 * Suggested by Jeltje van Baren.*/
{
int longestSpan = 0;
int minSpanLen;
struct cDnaAlign *aln;

/* find longest span */
for (aln = cdAlns->alns; aln != NULL; aln = aln->next)
    if (!aln->drop)
        longestSpan = max(longestSpan, getTargetSpan(aln));

/* filter by under this amount  */
minSpanLen = gMinSpan*longestSpan;
for (aln = cdAlns->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && (getTargetSpan(aln) < minSpanLen))
        {
        aln->drop = TRUE;
        cdAlns->minSpanCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: minSpan span %d, min is %d (%0.2f)",
                      getTargetSpan(aln), minSpanLen, ((float)getTargetSpan(aln))/longestSpan);
        }
}

static void filterQuery(struct cDnaAligns *cdAlns,
                        FILE *outPslFh, FILE *dropPslFh, FILE *weirdOverPslFh)
/* filter the current query set of alignments in cdAlns */
{
/* n.b. order should agree with doc */
invalidPslFilter(cdAlns);
if (gMinQSize > 0)
    minQSizeFilter(cdAlns);
if (gBestOverlap)
    overlapFilter(cdAlns);
if (gMaxRepMatch < 1.0)
    repMatchFilter(cdAlns);
if (gMinId > 0.0)
    identFilter(cdAlns);
if (gMinCover > 0.0)
    coverFilter(cdAlns);
if (gIdNearTop < 1.0)
    identNearTopFilter(cdAlns);
if (gCoverNearTop < 1.0)
    coverNearTopFilter(cdAlns);
if (gMinSpan > 0.0)
    minSpanFilter(cdAlns);
if (gMaxAligns >= 0)
    maxAlignFilter(cdAlns);
cDnaAlignsWriteKept(cdAlns, outPslFh);
if (dropPslFh != NULL)
    cDnaAlignsWriteDrop(cdAlns, dropPslFh);
if (weirdOverPslFh != NULL)
    cDnaAlignsWriteWeird(cdAlns, weirdOverPslFh);
}

static void verbStats(char *label, struct cDnaCnts *cnts)
/* output stats */
{
if (cnts->aligns > 0)
    verbose(1, "%18s:\t%d\t%d\t%d\n", label, cnts->queries, cnts->aligns,
            cnts->multAlnQueries);
}

static void pslCDnaFilter(char *inPsl, char *outPsl)
/* filter cDNA alignments in psl format */
{
struct cDnaAligns *cdAlns = cDnaAlignsNew(inPsl, gCoverWeight, gPolyASizes);
FILE *outPslFh = mustOpen(outPsl, "w");
FILE *dropPslFh = NULL;
FILE *weirdOverPslFh = NULL;
if (gDropped != NULL)
    dropPslFh = mustOpen(gDropped, "w");
if (gWeirdOverlappped != NULL)
    weirdOverPslFh = mustOpen(gWeirdOverlappped, "w");

while (cDnaAlignsNext(cdAlns))
    filterQuery(cdAlns, outPslFh, dropPslFh, weirdOverPslFh);

carefulClose(&dropPslFh);
carefulClose(&weirdOverPslFh);
carefulClose(&outPslFh);

verbose(1,"%18s \tseqs\taligns\tmultAlnSeqs\n", "");
verbStats("total", &cdAlns->totalCnts);
verbStats("kept", &cdAlns->keptCnts);
verbStats("drop invalid", &cdAlns->badCnts);
verbStats("drop min size", &cdAlns->minQSizeCnts);
verbStats("weird over", &cdAlns->weirdOverCnts);
verbStats("kept weird", &cdAlns->weirdKeptCnts);
verbStats("drop overlap", &cdAlns->overlapCnts);
verbStats("drop minIdent", &cdAlns->minIdCnts);
verbStats("drop idNearTop", &cdAlns->idTopCnts);
verbStats("drop minCover", &cdAlns->minCoverCnts);
verbStats("drop coverNearTop", &cdAlns->coverTopCnts);
verbStats("drop maxRepMatch", &cdAlns->maxRepMatchCnts);
verbStats("drop minSpan", &cdAlns->minSpanCnts);
verbStats("drop maxAligns", &cdAlns->maxAlignsCnts);
cDnaAlignsFree(&cdAlns);
}

static float optionFrac(char *name, float def)
/* get an option that must be in the range 0.0 and 1.0 */
{
float val = optionFloat(name, def);
if (!((0.0 <= val) && (val <= 1.0)))
    errAbort("-%s must be in the range 0.0..1.0", name);
return val;
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage("wrong # of args:");
gPolyASizes = optionVal("polyASizes", NULL);
gDropped = optionVal("dropped", NULL);
gWeirdOverlappped = optionVal("weirdOverlapped", NULL);
gMinId = optionFrac("minId", gMinId);
gIdNearTop = optionFrac("idNearTop", gIdNearTop);
gMinCover = optionFrac("minCover", gMinCover);
gCoverNearTop = optionFrac("coverNearTop", gCoverNearTop);
gCoverWeight = optionFrac("coverWeight", gCoverWeight);
gMaxRepMatch = optionFrac("maxRepMatch", gMaxRepMatch);
gMinQSize = optionInt("minQSize", gMinQSize);
gMinSpan = optionFrac("minSpan", gMinSpan);
gMaxAligns = optionInt("maxAligns", gMaxAligns);
gBestOverlap = optionExists("bestOverlap");
gAlignStats = optionVal("alignStats", gAlignStats);

if (optionExists("nonComparative"))
    {
    int i;
    for (i = 0; comparativeOpts[i] != NULL; i++)
        if (optionExists(comparativeOpts[i]))
            errAbort("option -%s is not compatible with -nonComparative",
                     comparativeOpts[i]);
    gIdNearTop = 1.0;
    gCoverNearTop = 1.0;
    }

pslCDnaFilter(argv[1], argv[2]);
return 0;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

