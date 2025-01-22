/* filter cDNA alignments in psl format */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "cDnaAligns.h"
#include "cDnaReader.h"
#include "overlapFilter.h"
#include "globalNearBestFilter.h"
#include "localNearBestFilter.h"
#include "hapRegions.h"
#include "psl.h"
#include "options.h"
#include "genbankBlackList.h"

struct blackListRange *gBlackListRanges = NULL;

static void usage()
/* usage msg and exit */
{
/* message got huge, so it's in a generate file */
#include "usage.h"
errAbort("%s", usageMsg);
}

static void prAlgo()
/* print algorithm description and exit */
{
/* message got huge, so it's in a generate file */
#include "algo.h"
fprintf(stderr, "%s", usageMsg);
exit(0);
}


/* command line options and values */
static struct optionSpec optionSpecs[] =
{
    {"algoHelp", OPTION_BOOLEAN},
    {"localNearBest", OPTION_FLOAT},
    {"globalNearBest", OPTION_FLOAT},
    {"ignoreNs", OPTION_BOOLEAN},
    {"ignoreIntrons", OPTION_BOOLEAN},
    {"minId", OPTION_FLOAT},
    {"minCover", OPTION_FLOAT},
    {"minSpan", OPTION_FLOAT},
    {"minQSize", OPTION_INT},
    {"maxAligns", OPTION_INT},
    {"maxAlignsDrop", OPTION_INT},
    {"minAlnSize", OPTION_INT},
    {"minNonRepSize", OPTION_INT},
    {"maxRepMatch", OPTION_FLOAT},
    {"repsAsMatch", OPTION_BOOLEAN},
    {"polyASizes", OPTION_STRING},
    {"usePolyTHead", OPTION_BOOLEAN},
    {"hapRegions", OPTION_STRING},
    {"bestOverlap", OPTION_BOOLEAN},
    {"dropped", OPTION_STRING},
    {"weirdOverlapped", OPTION_STRING},
    {"filterWeirdOverlapped", OPTION_BOOLEAN},
    {"noValidate", OPTION_BOOLEAN},
    {"hapRefMapped", OPTION_STRING},
    {"hapRefCDnaAlns", OPTION_STRING},
    {"hapLociAlns", OPTION_STRING},
    {"alnIdQNameMode", OPTION_BOOLEAN},
    {"uniqueMapped", OPTION_BOOLEAN},
    {"decayMinCover", OPTION_BOOLEAN},
    {"blackList", OPTION_STRING},
    {"statsOut", OPTION_STRING},
    {NULL, 0}
};

static boolean gValidate = TRUE;       /* run pslCheck validations */
static float gLocalNearBest = -1.0;    /* local near best in genome */
static float gGlobalNearBest = -1.0;   /* global near best in genome */
static unsigned gCDnaOpts = 0;         /* options for cDnaReader */
static float gMinId = 0.0;             /* minimum fraction id */
static float gMinCover = 0.0;          /* minimum coverage */
static float gMinSpan = 0.0;           /* minimum target span allowed */
static int gMinQSize = 0;              /* drop queries shorter than this */
static int gMaxAligns = -1;            /* only allow this many alignments for a query */
static int gMaxAlignsDrop = -1;        /* drop all if more than this many aligns*/
static int gMinAlnSize = 0;            /* minimum bases that must be aligned */
static int gMinNonRepSize = 0;         /* minimum non-repeat bases that must match */
static float gMaxRepMatch = 1.0;       /* maximum fraction of repeat matching */
static char *gPolyASizes = NULL;       /* polyA size file */
static char *gHapRegions = NULL;       /* haplotype regions file */
static boolean gBestOverlap = FALSE;   /* filter overlaping, keeping only the best */
static char *gDropped = NULL;          /* save dropped psls here */
static char *gWeirdOverlappped = NULL; /* save weird overlapping psls here */
static boolean gFilterWeirdOverlapped = FALSE; /* save only highest scoring of weirdly overlapped alignments. */
static int gMinLocalBestCnt = 30;      /* minimum number of bases that are over threshold
                                        * for local best */
static char *gHapRefMapped = NULL;    /* PSLs of haplotype to reference chromosome
                                       * mappings */
static char *gHapRefCDnaAlns = NULL;  /* PSLs of haplotype cDNA to reference
                                       * cDNA alignments */
static char *gHapLociAlns = NULL;     /* loci id + PSLs indicating haplotype groups */
static boolean gUniqueMapped = FALSE; /* keep only cDNAs that are uniquely
                                       * aligned after filtering */
static boolean gDecayMinCover = FALSE; /* use decay model for minCoverage */
static char *gStatsOut = NULL;  /* stats output */

struct outFiles
/* open output files */
{
    FILE *passFh;              /* filtered psls */
    FILE *dropFh;              /* dropped psls, or NULL */
    FILE *weirdOverFh;         /* weird overlap psls, or NULL */
    FILE *hapRefMappedFh;      /* hap to ref cDNA alignment mappings */
    FILE *hapRefCDnaAlnsFh;    /* cDNA to cDNA alignments. */
    FILE *hapLociAlnsFh;       /* loci groupings for haplotypes */
};

static boolean validPsl(struct psl *psl)
/* check if a psl is internally consistent */
{
static FILE *devNull = NULL;
if (devNull == NULL)
    devNull = mustOpen("/dev/null", "w");
return (pslCheck("", devNull, psl) == 0);
}

static void invalidPslFind(struct cDnaQuery *cdna)
/* find invalid PSL for verbose messages */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!validPsl(aln->psl))
        cDnaAlignVerb(2, aln, "invalid PSL");
    }
}

static void blackListFilter(struct cDnaQuery *cdna)
/* filter for black list */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop && genbankBlackListFail(aln->psl->qName, gBlackListRanges))
        cDnaAlignDrop(aln, FALSE, &cdna->stats->blackListCnts, "black listed");
    }
}

static void invalidPslFilter(struct cDnaQuery *cdna)
/* filter for invalid PSL */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop && !validPsl(aln->psl))
        cDnaAlignDrop(aln, FALSE, &cdna->stats->badDropCnts, "invalid PSL");
    }
}

static void minQSizeFilter(struct cDnaQuery *cdna)
/* filter by minimum query size */
{
struct cDnaAlign *aln;
/* note that qSize is actually the same for all alignments */
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop && (aln->psl->qSize < gMinQSize))
        cDnaAlignDrop(aln, FALSE, &cdna->stats->minQSizeDropCnts, "min query size %0.4g", aln->psl->qSize);
    }
}

static void identFilter(struct cDnaQuery *cdna)
/* filter by fraction identity */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop && (aln->ident < gMinId))
        cDnaAlignDrop(aln, FALSE, &cdna->stats->minIdDropCnts, "min ident %0.4g", aln->ident);
    }
}

static double calcDecayCoverage(int qSize)
/* calc min coverage needed based on qSize */
{
double needCoverage;

/* Make it so that smaller RNAs had to align a larger fraction of themselves.
 * RNAs larger than 250bp must align at least 25%, RNAs smaller than 25 bp
 * must align 90%, for RNAs in between there's a linear interpolation between.
 * This is backwards compatible with the hard 25% cut off we used to have on the
 * large side, and should prevent noisy alignments we're now getting from a 
 * bunch of small RNAs. */
needCoverage = 1.0 - qSize / 250.0;

if (needCoverage < 0.25)
    needCoverage = 0.25;
else if (needCoverage > 0.9)
    needCoverage = 0.9;
return needCoverage;
}

static void decayCoverFilter(struct cDnaQuery *cdna)
/* filter by decaying coverage function based on qSize */
{
struct cDnaAlign *aln;

/* drop those that are under min calced using qSize */
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    double needCoverage = calcDecayCoverage( aln->psl->qSize );
    if (!aln->drop && (aln->cover < needCoverage))
        cDnaAlignDrop(aln, FALSE, &cdna->stats->minCoverDropCnts, "decay cover %0.4g", aln->cover);
    }
}

static void coverFilter(struct cDnaQuery *cdna)
/* filter by coverage */
{
struct cDnaAlign *aln;

/* drop those that are under min */
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop && (aln->cover < gMinCover))
        cDnaAlignDrop(aln, FALSE, &cdna->stats->minCoverDropCnts, "min cover %0.4g", aln->cover);
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
    if (!aln->drop && (aln->adjAlnSize < gMinAlnSize))
        cDnaAlignDrop(aln, FALSE, &cdna->stats->minAlnSizeDropCnts, "min align size %d", aln->adjAlnSize);
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
    int nonRepSize = (aln->adjMatch - aln->alnPolyAT);
    if (nonRepSize < 0)
        nonRepSize = 0;
    if (!aln->drop && (nonRepSize < gMinNonRepSize))
        cDnaAlignDrop(aln, FALSE, &cdna->stats->minNonRepSizeDropCnts, "min non-rep size %d", nonRepSize);
    }
}

static void repMatchFilter(struct cDnaQuery *cdna)
/* filter by maxRepMatch */
{
struct cDnaAlign *aln;

/* drop those that are over max */
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop && (aln->repMatchFrac > gMaxRepMatch))
        cDnaAlignDrop(aln, FALSE, &cdna->stats->maxRepMatchDropCnts, "max repMatch %0.4g", aln->repMatchFrac);
    }
}

static boolean shouldMaxAlignsDrop(struct cDnaQuery *cdna)
/* should this set of alignments be dropped */
{
struct cDnaAlign *aln;
int cnt;
for (aln = cdna->alns, cnt = 0; (aln != NULL); aln = aln->next)
    {
    if (!aln->drop)
        cnt++;
    if (cnt == gMaxAlignsDrop)
	return TRUE;
    }
return FALSE;
}

static void maxAlignDropFilter(struct cDnaQuery *cdna)
/* filter by maximum number of alignments, drop all if number is too large */
{
struct cDnaAlign *aln;
cDnaQueryRevScoreSort(cdna);
if (shouldMaxAlignsDrop(cdna))
    {
    for (aln = cdna->alns; aln != NULL; aln = aln->next)
	{
	if (!aln->drop)
	    cDnaAlignDrop(aln, FALSE, &cdna->stats->maxAlignsDropCnts, "max aligns");
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
    {
    if (!aln->drop)
        cDnaAlignDrop(aln, FALSE, &cdna->stats->maxAlignsDropCnts, "max aligns");
    }
}

static boolean allHapSetDropped(struct cDnaAlign *aln)
/* is aln or any of the hapSet linked not dropped? */
{
if (!aln->drop)
    return FALSE;
struct cDnaAlignRef *hapAln;
for (hapAln = aln->hapAlns; hapAln != NULL; hapAln = hapAln->next)
    {
    if (!hapAln->ref->drop)
        return FALSE;
    }
return TRUE;
}

static void uniqueMappedFilter(struct cDnaQuery *cdna)
/* filter for uniquely mapped alignments */
{
int liveCnt = 0;
struct cDnaAlignRef *hapSetAln;
for (hapSetAln = cdna->hapSets; hapSetAln != NULL; hapSetAln = hapSetAln->next)
    {
    if (!allHapSetDropped(hapSetAln->ref))
        liveCnt++;
    }
if (liveCnt > 1)
    {
    for (hapSetAln = cdna->hapSets; hapSetAln != NULL; hapSetAln = hapSetAln->next)
        {
        if (!hapSetAln->ref->drop)
            cDnaAlignDrop(hapSetAln->ref, TRUE, &cdna->stats->nonUniqueMap, "non-uniquely mapped");
        }
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
    {
    if (!aln->drop && (getTargetSpan(aln) < minSpanLen))
        cDnaAlignDrop(aln, FALSE, &cdna->stats->minSpanDropCnts, "minSpan span %d, min is %d (%0.2f)",
                      getTargetSpan(aln), minSpanLen, ((float)getTargetSpan(aln))/longestSpan);
    }
}

static void filterNonComparative(struct cDnaQuery *cdna)
/* apply non-comparative filters */
{
/* n.b. order must agree with doc in algo.txt */
if (gBlackListRanges)
    blackListFilter(cdna);
if (gValidate)
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
if (gDecayMinCover)
    decayCoverFilter(cdna);
if (gBestOverlap)
    overlapFilterOverlapping(cdna);
if (gMinSpan > 0.0)
    minSpanFilter(cdna);
}

static void filterComparative(struct cDnaQuery *cdna, struct hapRegions *hapRegions)
/* apply comparative filters */
{
/* n.b. order must agree with doc in algo.txt */
if (hapRegions != NULL)
    hapRegionsLinkHaps(hapRegions, cdna);
else
    hapRegionsBuildHapSets(cdna);
if (gLocalNearBest >= 0.0)
    localNearBestFilter(cdna, gLocalNearBest, gMinLocalBestCnt);
if (gGlobalNearBest >= 0.0)
    globalNearBestFilter(cdna, gGlobalNearBest);
if (gMaxAlignsDrop >= 0)
    maxAlignDropFilter(cdna);
if (gMaxAligns >= 0)
    maxAlignFilter(cdna);
if (gUniqueMapped)
    uniqueMappedFilter(cdna);
// this is last so weird overlaps can be discarded by above tests first
overlapFilterFlagWeird(cdna);
if (gFilterWeirdOverlapped)
    overlapFilterWeirdFilter(cdna);
}

static void filterQuery(struct cDnaQuery *cdna, struct hapRegions *hapRegions,
                        struct outFiles *outFiles)
/* filter the current query set of alignments in cdna */
{
/* setup */
invalidPslFind(cdna);

filterNonComparative(cdna);
filterComparative(cdna, hapRegions);

cDnaQueryWriteKept(cdna, outFiles->passFh);
if (outFiles->dropFh != NULL)
    cDnaQueryWriteDrop(cdna, outFiles->dropFh);
if (outFiles->weirdOverFh != NULL)
    cDnaQueryWriteWeird(cdna, outFiles->weirdOverFh);
if (outFiles->hapLociAlnsFh != NULL)
    cDnaQueryWriteHaplotypePslLoci(cdna, outFiles->hapLociAlnsFh);
}

static void pslCDnaFilter(char *inPsl, char *outPsl)
/* filter cDNA alignments in psl format */
{
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
if (gHapLociAlns != NULL)
    outFiles.hapLociAlnsFh = mustOpen(gHapLociAlns, "w");
struct hapRegions *hapRegions = (gHapRegions == NULL) ? NULL
    : hapRegionsNew(gHapRegions, outFiles.hapRefMappedFh, outFiles.hapRefCDnaAlnsFh);
struct cDnaReader *reader = cDnaReaderNew(inPsl, gCDnaOpts, gPolyASizes, hapRegions);

while (cDnaReaderNext(reader))
    filterQuery(reader->cdna, hapRegions, &outFiles);

carefulClose(&outFiles.hapRefMappedFh);
carefulClose(&outFiles.hapRefCDnaAlnsFh);
carefulClose(&outFiles.hapLociAlnsFh);
carefulClose(&outFiles.dropFh);
carefulClose(&outFiles.weirdOverFh);
carefulClose(&outFiles.passFh);
if (gStatsOut != NULL)
    {
    FILE *fh = mustOpen(gStatsOut, "w");
    cDnaStatsPrint(&reader->stats, fh);
    carefulClose(&fh);
    }
else if (verboseLevel() >= 1)
    cDnaStatsPrint(&reader->stats, stderr);
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
    usage();

gLocalNearBest = optionFrac("localNearBest", gLocalNearBest);
gGlobalNearBest = optionFrac("globalNearBest", gGlobalNearBest);
if ((gLocalNearBest >= 0.0) && (gGlobalNearBest >= 0.0))
    errAbort("can only specify one of -localNearBest and -globalNearBest");
if (optionExists("usePolyTHead"))
    gCDnaOpts |= cDnaUsePolyTHead;
if (optionExists("ignoreNs"))
    gCDnaOpts |= cDnaIgnoreNs;
if (optionExists("ignoreIntrons"))
    gCDnaOpts |= cDnaIgnoreIntrons;
if (optionExists("repsAsMatch"))
    gCDnaOpts |= cDnaRepsAsMatch;
gMinId = optionFrac("minId", gMinId);
gMinCover = optionFrac("minCover", gMinCover);
gMinSpan = optionFrac("minSpan", gMinSpan);
gMinQSize = optionInt("minQSize", gMinQSize);
gMaxAligns = optionInt("maxAligns", gMaxAligns);
gMaxAlignsDrop = optionInt("maxAlignsDrop", gMaxAlignsDrop);
if ((gMaxAligns >= 0) && (gMaxAlignsDrop >= 0))
    errAbort("cannot specify both -maxAligns and -maxAlignsDrop");
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
gFilterWeirdOverlapped = optionExists("filterWeirdOverlapped");
gHapRefMapped = optionVal("hapRefMapped", NULL);
gHapRefCDnaAlns = optionVal("hapRefCDnaAlns", NULL);
gHapLociAlns = optionVal("hapLociAlns", NULL);
if (optionExists("noValidate"))
    gValidate = FALSE;
cDnaAlignsAlnIdQNameMode = optionExists("alnIdQNameMode");
if (optionExists("ignoreNs"))
    gCDnaOpts |= cDnaIgnoreNs;
gUniqueMapped = optionExists("uniqueMapped");
gDecayMinCover = optionExists("decayMinCover");
gStatsOut = optionVal("statsOut", NULL);
char *blackList = optionVal("blackList", NULL);

if (blackList != NULL)
    gBlackListRanges = genbankBlackListParse(blackList);

if ( gDecayMinCover && (gMinCover > 0.0))
    errAbort("can only specify one of -minCoverage and -decayMinCoverage");

pslCDnaFilter(argv[1], argv[2]);
return 0;
}
