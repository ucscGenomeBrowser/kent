/* filter cDNA alignments in psl format */
#include "common.h"
#include "cDnaAligns.h"
#include "overlapFilter.h"
#include "psl.h"
#include "options.h"

/* command line options and values */
static struct optionSpec optionSpecs[] =
{
    {"minId", OPTION_FLOAT},
    {"idNearTop", OPTION_FLOAT},
    {"minCover", OPTION_FLOAT},
    {"coverNearTop", OPTION_FLOAT},
    {"coverWeight", OPTION_FLOAT},
    {"minQSize", OPTION_INT},
    {"maxAligns", OPTION_INT},
    {"polyASizes", OPTION_STRING},
    {"dropped", OPTION_STRING},
    {"weirdOverlapped", OPTION_STRING},
    {NULL, 0}
};
char *gPolyASizes = NULL; /* polyA size file */
char *gDropped = NULL; /* save dropped psls here */
char *gWeirdOverlappped = NULL; /* save weird overlapping psls here */
float gMinId = 0.95;            /* minimum fraction id */
float gIdNearTop = 0.01;        /* keep within this fraction of the top */
float gMinCover = 0.90;         /* minimum coverage */
float gCoverNearTop = 0.1;      /* keep within this fraction of best cover */
float gCoverWeight = 0.5;       /* weight of cover vs id */
int gMinQSize = 0;              /* drop queries shorter than this */
int gMaxAligns = -1;            /* only allow this many alignments for a query
                                 * if >= 0. */

void usage(char *msg)
/* usage msg and exit */
{
errAbort("%s\n%s", msg,
         "pslCDnaFilter [options] inPsl outPsl\n"
         "\n"
         "Filter cDNA alignments in psl format.\n"
         "\n"
         "Options:\n"
         "   -minId=0.95 - only keep alignments with at least this fraction\n"
         "    identity.\n"
         "   -idNearTop=0.01 - keep alignments within this fraction\n"
         "    from top ident.\n"
         "   -minCover=0.90 - minimum fraction of query that must be aligned.\n"
         "   -coverNearTop=0.1 - keep alignments within this fraction of\n"
         "    the top coverage alignment. If -polyASizes is specified and the query\n"
         "    is in the file, the ploy-A is not included in coverage calculation.\n"
         "   -coverWeight=0.50 - weight of coverage vs identity in critera that\n"
         "    select between alignments. A value of 0.75 would put 3/4 of the\n"
         "    weight on coverage and 1/4 on identity.\n"
         "   -minQSize=0 - drop queries shorter than this size\n"
         "   -maxAligns=n - maximum number of alignments for a given query. If\n"
         "    exceeded, then alignments are sorted by weighed coverage and\n"
         "    identity an the those over this are dropped.\n"
         "   -polyASizes=file - tab separate file as output by faPolyASizes, columns are:\n"
         "        id seqSize tailPolyASize headPolyTSize\n"
         "   -dropped=psl - save psls that were dropped to this file.\n"
         "   -weirdOverlapped=psl - output weirdly overlapping PSLs to\n"
         "    this file.\n" 
         "   -verbose=1 - 0: quite\n"
         "                1: output stats\n"
         "                2: list problem alignment (weird or invalid)\n"
         "                3: list dropped alignments and reason for dropping\n"
         "                4: list kept psl and info\n"
         "                5: info about all PSLs\n"
         "\n"
         "Warning: inPsl MUST be sorted by qName\n"
         "\n"
         "This filters for the following criteria and in this order:\n"
         "  o PSLs that are not internally consistent, such has having\n"
         "    overlapping blocks, are dropped.  Use pslCheck program to\n"
         "    get the details.\n"
         "  o Drop queries less than the min size, if specified\n"
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
         "  o By maxAligns, if specified\n");
}

void minQSizeFilter(struct cDnaAligns *cdAlns)
/* filter by minimum query size */
{
struct cDnaAlign *aln;
/* note that qSize is actually the same for a alignments */
for (aln = cdAlns->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && (aln->psl->qSize < gMinQSize))
        {
        aln->drop = TRUE;
        cdAlns->minQSizeCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: min query size %g", aln->psl->qSize);
        }
}

void identFilter(struct cDnaAligns *cdAlns)
/* filter by fraction identity */
{
struct cDnaAlign *aln;
for (aln = cdAlns->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && (aln->ident < gMinId))
        {
        aln->drop = TRUE;
        cdAlns->minIdCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: min ident %g", aln->ident);
        }
}

float getMaxIdent(struct cDnaAligns *cdAlns)
/* get the maximum ident  */
{
float maxIdent = 0.0;
struct cDnaAlign *aln;
for (aln = cdAlns->alns; aln != NULL; aln = aln->next)
    if (!aln->drop)
        maxIdent = max(maxIdent, aln->ident);
return maxIdent;
}

void identNearTopFilter(struct cDnaAligns *cdAlns)
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
        cDnaAlignVerb(3, aln->psl, "drop: id near top %g, top=%g, th=%g",
                      aln->ident, maxIdent, thresh);
        }
}

void coverFilterAln(struct cDnaAligns *cdAlns,
                    struct cDnaAlign *aln)
/* filter an alignment based on coverage */
{
if (aln->cover < gMinCover)
    {
    aln->drop = TRUE;
    cdAlns->minCoverCnts.aligns++;
    cDnaAlignVerb(3, aln->psl, "drop: min cover %g", aln->cover);
    }
}

void coverFilter(struct cDnaAligns *cdAlns)
/* filter by coverage */
{
struct cDnaAlign *aln;

/* drop those that are under min */
for (aln = cdAlns->alns; aln != NULL; aln = aln->next)
    if ((!aln->drop) && (aln->cover < gMinCover))
        {
        aln->drop = TRUE;
        cdAlns->minCoverCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: min cover %g", aln->cover);
        }
}

float getMaxCover(struct cDnaAligns *cdAlns)
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

void coverNearTopFilter(struct cDnaAligns *cdAlns)
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
        cDnaAlignVerb(3, aln->psl, "drop: cover near top %g, top=%g, th=%g",
                      aln->cover, maxCover, thresh);
        }
}

struct cDnaAlign *findMaxAlign(struct cDnaAligns *cdAlns)
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
void maxAlignFilter(struct cDnaAligns *cdAlns)
/* filter by maximum number of alignments */
{
struct cDnaAlign *aln;
int cnt;
cDnaAlignsSort(cdAlns);
for (aln = findMaxAlign(cdAlns); aln != NULL; aln = aln->next)
    if (!aln->drop)
        {
        aln->drop = TRUE;
        cdAlns->maxAlignsCnts.aligns++;
        cDnaAlignVerb(3, aln->psl, "drop: max aligns");
        }
}

void filterQuery(struct cDnaAligns *cdAlns,
                 FILE *outPslFh, FILE *dropPslFh, FILE *weirdOverPslFh)
/* filter the current query set of alignments in cdAlns */
{
/* n.b. order should agree with doc */
minQSizeFilter(cdAlns);
overlapFilter(cdAlns);
identFilter(cdAlns);
coverFilter(cdAlns);
identNearTopFilter(cdAlns);
coverNearTopFilter(cdAlns);
if (gMaxAligns >= 0)
    maxAlignFilter(cdAlns);

cDnaAlignsWriteKept(cdAlns, outPslFh);
if (dropPslFh != NULL)
    cDnaAlignsWriteDrop(cdAlns, dropPslFh);
if (weirdOverPslFh != NULL)
    cDnaAlignsWriteWeird(cdAlns, weirdOverPslFh);
}

void verbStats(char *label, struct cDnaCnts *cnts)
/* output stats */
{
if (cnts->aligns > 0)
    verbose(1, "%18s:\t%d\t%d\n", label, cnts->queries, cnts->aligns);
}

void pslCDnaFilter(char *inPsl, char *outPsl)
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

verbose(1,"query/alignment counts:\n");
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
verbStats("drop maxAligns", &cdAlns->maxAlignsCnts);
cDnaAlignsFree(&cdAlns);
}

float optionFrac(char *name, float def)
/* get an option that must be in the range 0.0 and 1.0 */
{
float val = optionFloat(name, def);
if (!((0.0 <= val) && (val <= 1.0)))
    errAbort("-%s must be in the range 0.0..1.0");
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
gMinQSize = optionInt("minQSize", gMinQSize);
gMaxAligns = optionInt("maxAligns", gMaxAligns);
pslCDnaFilter(argv[1], argv[2]);
return 0;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

