/* pslStats - collect statistics from a psl file */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "obscure.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "psl.h"
#include "sqlNum.h"


/* size for query name hashs */
static int queryHashPowTwo = 22;
static boolean warnOnConflicts = FALSE;

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"queryStats", OPTION_BOOLEAN},
    {"overallStats", OPTION_BOOLEAN},
    {"queries", OPTION_STRING},
    {"warnOnConflicts", OPTION_BOOLEAN},
    {NULL, 0}
};

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslStats - collect statistics from a psl file.\n"
  "\n"
  "usage:\n"
  "   pslStats [options] psl statsOut\n"
  "\n"
  "Options:\n"
  "  -queryStats - output per-query statistics, the default is per-alignment stats\n"
  "  -overallStats - output overall statistics.\n"
  "  -queries=querySizeFile - tab separated file with of expected qNames and sizes.\n"
  "   If specified, statistic will include queries that didn't align.\n"
  "  -warnOnConflicts - warn and ignore when a two PSLs with the same qName conflict.\n"
  "   This can happen with bogus generated names.\n");
}

struct querySizeCnt
/* structure used to hold query size and a count */
{
    unsigned qSize;
    unsigned alnCnt;
};

static void querySizeConflict(char *qName, unsigned qSize, unsigned qSize2)
/* generate a error or warning on conflicting query sizes */
{
if (warnOnConflicts)
    warn("conflicting query sizes for %s: %d and %d", qName, qSize, qSize2);
else
    errAbort("conflicting query sizes for %s: %d and %d", qName, qSize, qSize2);
}

static struct querySizeCnt *querySizeCntGet(struct hash* querySizesTbl,
                                            char *qName, unsigned qSize)
/* get entry with size and alignment count, create if not present. */
{
struct hashEl *hel = hashStore(querySizesTbl, qName);
struct querySizeCnt *qs = hel->val;
if (qs != NULL)
    {
    if (qs->qSize != qSize)
        {
        querySizeConflict(qName, qs->qSize, qSize);
        return NULL;
        }
    }
else
    {
    lmAllocVar(querySizesTbl->lm, qs);
    hel->val = qs;
    qs->qSize = qSize;
    }
return qs;
}

/* read qNames and sizes into a hash of querySizeCnt objet */
static struct hash* querySizeCntLoad(char *querySizeFile)
{
struct hash* querySizesTbl = hashNew(queryHashPowTwo);
struct lineFile *lf = lineFileOpen(querySizeFile, TRUE);
char *row[2];

while (lineFileNextRowTab(lf, row, ArraySize(row)))
    querySizeCntGet(querySizesTbl, row[0], sqlUnsigned(row[1]));

lineFileClose(&lf);
return querySizesTbl;
}

struct sumStats
/* structure to hold per-query summary statistics */
{
    char *qName;        /* query name; string is in hash table, NULL if overall */
    unsigned queryCnt;  /* total number of queries, if summary */
    unsigned minQSize;  /* min/max/total sizes of queries */
    unsigned maxQSize;
    bits64 totalQSize;
    unsigned alnCnt;   /* number of alignments */
    bits64 totalAlign; /* total bases aligned */ 
    bits64 totalMatch; /* total bases matching */ 
    bits64 totalRepMatch; /* total bases matching repeats */
    float minIdent;    /* min/max fraction identity */
    float maxIndent;
    float minQCover;   /* min/max coverage of query */
    float maxQCover;
    float minTCover;   /* min/max coverage of target */
    float maxTCover;
    float minRepMatch; /* fraction that is repeat matches */
    float maxRepMatch;
};

struct sumStats *sumStatsGetForQuery(struct hash *queryStatsTbl,
                                     char *qName, unsigned qSize)
/* lookup a stats on query by name, creating if it doesn't exist */
{
struct hashEl *hel = hashStore(queryStatsTbl, qName);
struct sumStats *qs = hel->val;
if (qs == NULL)
    {
    AllocVar(qs);
    qs->qName = hel->name;  /* use string in hash */
    qs->queryCnt = 1;
    qs->minQSize = qs->maxQSize = qSize;
    hel->val = qs;
    }
else if (qs->minQSize != qSize)
    {
    querySizeConflict(qName, qs->minQSize, qSize);
    return NULL;
    }
return hel->val;
}

/* read qNames and sizes into a hash of sumStats objects */
static struct hash* sumStatsLoad(char *querySizeFile)
{
struct hash* querySizesTbl = hashNew(queryHashPowTwo);
struct lineFile *lf = lineFileOpen(querySizeFile, TRUE);
char *row[2];

while (lineFileNextRowTab(lf, row, ArraySize(row)))
    sumStatsGetForQuery(querySizesTbl, row[0], sqlUnsigned(row[1]));

lineFileClose(&lf);
return querySizesTbl;
}

static unsigned calcAligned(struct psl *psl)
/* compute the number of bases aligned */
{
return psl->match + psl->misMatch + psl->repMatch + psl->nCount;
}

static unsigned calcMatch(struct psl *psl)
/* compute the number of bases matching */
{
return psl->match + psl->repMatch;
}

static float calcIdent(struct psl *psl)
/* get fraction ident for a psl */
{
unsigned aligned = calcAligned(psl);
if (aligned == 0)
    return 0.0;
else
    return ((float)calcMatch(psl))/((float)(aligned));
}

static float calcQCover(struct psl *psl)
/* calculate query coverage */
{
if (psl->qSize == 0)
    return 0.0; /* should never happen */
else
    return ((float)calcAligned(psl))/((float)psl->qSize);
}

static float calcTCover(struct psl *psl)
/* calculate target coverage */
{
if (psl->tSize == 0)
    return 0.0; /* should never happen */
else
    return ((float)calcAligned(psl))/((float)psl->tSize);
}

static float calcRepMatch(struct psl *psl)
/* calculate fraction of aligned that is repeat match */
{
unsigned aligned = calcAligned(psl);
if (aligned == 0)
    return 0.0; /* should never happen */
else
    return ((float)psl->repMatch)/((float)aligned);
}

static float calcMeanIdent(struct sumStats *ss)
/* calculate the mean fraction identity for a set of alignments */
{
if (ss->totalAlign == 0)
    return 0.0;
else
    return (float)(((double)ss->totalMatch)/((double)ss->totalAlign));
}

static unsigned calcMeanQSize(struct sumStats *ss)
/* calculate the mean query size for a set of alignments */
{
if (ss->queryCnt == 0)
    return 0;
else
    return ss->totalQSize/ss->queryCnt;
}

static float calcMeanQCover(struct sumStats *ss)
/* calculate the mean qcover  for a set of alignments */
{
if (ss->totalQSize == 0)
    return 0.0;
else
    return (float)(((double)ss->totalAlign)/((double)(ss->alnCnt*ss->totalQSize)));
}

static float calcMeanRepMatch(struct sumStats *ss)
/* calculate the mean repmatch for a set of alignments */
{
if (ss->totalAlign == 0)
    return 0.0;
else
    return (float)(((double)ss->totalRepMatch)/((double)ss->totalAlign));
}

static void sumStatsAccumulateQuery(struct sumStats *ss, struct psl *psl)
/* accumulate stats from psl into sumStats object; must be for same query */
{
float ident = calcIdent(psl);
float qCover = calcQCover(psl);
float tCover = calcTCover(psl);
float repMatch = calcRepMatch(psl);
if (ss->alnCnt == 0)
    {
    ss->totalQSize = psl->qSize;
    ss->minQSize = ss->maxQSize = psl->qSize;
    ss->minIdent = ss->maxIndent = ident;
    ss->minQCover = ss->maxQCover = qCover;
    ss->minTCover = ss->maxTCover = tCover;
    ss->minRepMatch = ss->maxRepMatch = repMatch;
    }
else
    {
    ss->minQSize = min(ss->minQSize, psl->qSize);
    ss->maxQSize = max(ss->maxQSize, psl->qSize);
    ss->minIdent = min(ss->minIdent, ident);
    ss->maxIndent = max(ss->maxIndent, ident);
    ss->minQCover = min(ss->minQCover, qCover);
    ss->maxQCover = max(ss->maxQCover, qCover);
    ss->minTCover = min(ss->minTCover, tCover);
    ss->maxTCover = max(ss->maxTCover, tCover);
    ss->minRepMatch = min(ss->minRepMatch, repMatch);
    ss->maxRepMatch = max(ss->maxRepMatch, repMatch);
    }
ss->totalAlign += calcAligned(psl);
ss->totalMatch += calcMatch(psl);
ss->totalRepMatch += psl->repMatch;
ss->alnCnt++;
}

static void sumStatsSum(struct sumStats *ss, struct sumStats *ss2)
/* sum a sumStats object into this object */
{
if (ss->alnCnt == 0)
    {
    ss->minQSize = ss2->maxQSize;
    ss->minIdent = ss2->maxIndent;
    ss->minQCover = ss2->maxQCover;
    ss->minTCover = ss2->maxTCover;
    ss->minRepMatch = ss2->maxRepMatch;
    }
else if (ss2->alnCnt > 0)
    {
    ss->minQSize = min(ss->minQSize, ss2->minQSize);
    ss->maxQSize = max(ss->maxQSize, ss2->maxQSize);
    ss->minIdent = min(ss->minIdent, ss2->minIdent);
    ss->maxIndent = max(ss->maxIndent, ss2->maxIndent);
    ss->minQCover = min(ss->minQCover, ss2->minQCover);
    ss->maxQCover = max(ss->maxQCover, ss2->maxQCover);
    ss->minTCover = min(ss->minTCover, ss2->minTCover);
    ss->maxTCover = max(ss->maxTCover, ss2->maxTCover);
    ss->minRepMatch = min(ss->minRepMatch, ss2->minRepMatch);
    ss->maxRepMatch = max(ss->maxRepMatch, ss2->maxRepMatch);
    }
ss->queryCnt += ss2->queryCnt;
ss->totalQSize += ss2->totalQSize;
ss->totalAlign += ss2->totalAlign;
ss->totalMatch += ss2->totalMatch;
ss->totalRepMatch += ss2->totalRepMatch;
ss->alnCnt += ss2->alnCnt;
}

/* header for alignment statistics */
static char *alnStatsHdr = "#qName\t" "qSize\t" "tName\t" "tStart\t" "tEnd\t"
"ident\t" "qCover\t" "repMatch\t" "tCover\n";

/* format for alignStats output */
static char *alnStatsFmt = "%s\t%d\t%s\t%d\t%d\t%0.4f\t%0.4f\t%0.4f\t%0.4f\n";

static void alignStatsOutputUnaligned(FILE *fh, struct hash* querySizesTbl)
/* output stats on unaligned */
{
struct hashCookie cookie = hashFirst(querySizesTbl);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct querySizeCnt *qs = hel->val;
    if (qs->alnCnt == 0)
        fprintf(fh, alnStatsFmt, hel->name, qs->qSize, "", 0, 0, 0.0, 0.0, 0.0, 0.0);
    }
}

static void pslAlignStats(char *pslFile, char *statsFile, char *querySizeFile)
/* collect and output per-alignment stats */
{
struct hash* querySizesTbl = (querySizeFile != NULL)
    ? querySizeCntLoad(querySizeFile) : NULL;
struct lineFile *pslLf = pslFileOpen(pslFile);
FILE *fh = mustOpen(statsFile, "w");
struct psl* psl;

fputs(alnStatsHdr, fh);
while ((psl = pslNext(pslLf)) != NULL)
    {
    fprintf(fh, alnStatsFmt, psl->qName, psl->qSize, psl->tName, psl->tStart, psl->tEnd,
            calcIdent(psl), calcQCover(psl), calcRepMatch(psl), calcTCover(psl));
    if (querySizesTbl != NULL)
        querySizeCntGet(querySizesTbl, psl->qName, psl->qSize)->alnCnt++;
    pslFree(&psl);
    }
lineFileClose(&pslLf);

if (querySizesTbl != NULL)
    alignStatsOutputUnaligned(fh, querySizesTbl);

carefulClose(&fh);
}

/* header for query statistics */
static char *queryStatsHdr = "#qName\t" "qSize\t" "alnCnt\t" "minIdent\t" "maxIndent\t" "meanIdent\t"
"minQCover\t" "maxQCover\t" "meanQCover\t" "minRepMatch\t" "maxRepMatch\t" "meanRepMatch\t"
"minTCover\t" "maxTCover\n";

static void queryStatsOutput(FILE *fh, struct sumStats *qs)
/* output statistic on a query */
{
fprintf(fh, "%s\t%d\t%d\t" "%0.4f\t%0.4f\t%0.4f\t"
        "%0.4f\t%0.4f\t%0.4f\t"  "%0.4f\t%0.4f\t%0.4f\t" "%0.4f\t%0.4f\n",
        qs->qName, qs->minQSize, qs->alnCnt,
        qs->minIdent, qs->maxIndent, calcMeanIdent(qs),
        qs->minQCover, qs->maxQCover, calcMeanQCover(qs),
        qs->minRepMatch, qs->maxRepMatch, calcMeanRepMatch(qs),
        qs->minTCover, qs->maxTCover);
}

static void outputQueryStats(struct hash *queryStatsTbl, char *statsFile)
/* output statistics on queries */
{
struct hashCookie cookie = hashFirst(queryStatsTbl);
FILE *fh = mustOpen(statsFile, "w");
struct hashEl *hel;

fputs(queryStatsHdr, fh);
while ((hel = hashNext(&cookie)) != NULL)
    queryStatsOutput(fh, hel->val);

carefulClose(&fh);
}

static struct hash *collectQueryStats(char *pslFile, char *querySizeFile)
/* collect per-query statistics */
{
struct hash *queryStatsTbl = (querySizeFile != NULL)
    ? sumStatsLoad(querySizeFile)
    : hashNew(queryHashPowTwo);

struct lineFile *pslLf = pslFileOpen(pslFile);
struct psl* psl;

while ((psl = pslNext(pslLf)) != NULL)
    {
    struct sumStats *ss = sumStatsGetForQuery(queryStatsTbl, psl->qName, psl->qSize);
    sumStatsAccumulateQuery(ss, psl);
    pslFree(&psl);
    }
lineFileClose(&pslLf);
return queryStatsTbl;
}

static void pslQueryStats(char *pslFile, char *statsFile, char *querySizeFile)
/* collect and output per-query stats */
{
struct hash *queryStatsTbl = collectQueryStats(pslFile, querySizeFile);
outputQueryStats(queryStatsTbl, statsFile);
}

/* header for overall statistics */
static char *overallStatsHdr = "#queryCnt\t" "minQSize\t" "maxQSize\t" "meanQSize\t"
"alnCnt\t" "minIdent\t" "maxIndent\t" "meanIdent\t"
"minQCover\t" "maxQCover\t" "meanQCover\t" "minRepMatch\t" "maxRepMatch\t" "meanRepMatch\t"
"minTCover\t" "maxTCover\t" "aligned\t" "aligned1\t" "alignedN\t" "totalAlignedSize\n";

static void outputOverallStats(char *statsFile, struct sumStats *os, int aligned1, int alignedN)
/* output overall statistic */
{
FILE *fh = mustOpen(statsFile, "w");
fputs(overallStatsHdr, fh);
fprintf(fh, "%d\t%d\t%d\t%d\t%d\t" "%0.4f\t%0.4f\t%0.4f\t"
        "%0.4f\t%0.4f\t%0.4f\t"  "%0.4f\t%0.4f\t%0.4f\t" "%0.4f\t%0.4f\t"
        "%d\t%d\t%d\t%lld\n",
        os->queryCnt, os->minQSize, os->maxQSize, calcMeanQSize(os),
        os->alnCnt,
        os->minIdent, os->maxIndent, calcMeanIdent(os),
        os->minQCover, os->maxQCover, calcMeanQCover(os),
        os->minRepMatch, os->maxRepMatch, calcMeanRepMatch(os),
        os->minTCover, os->maxTCover, aligned1+alignedN, aligned1, alignedN,
        os->totalAlign);
carefulClose(&fh);
}

static struct sumStats sumOverAllStats(struct hash *queryStatsTbl, int *aligned1, int *alignedN)
/* sum statistics over all queries */
{
struct sumStats os;
ZeroVar(&os);
struct hashEl *hel;
struct hashCookie cookie = hashFirst(queryStatsTbl);
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct sumStats *ss = hel->val;
    sumStatsSum(&os, ss);
    if (ss->alnCnt == 1)
        (*aligned1)++;
    else if (ss->alnCnt > 1)
        (*alignedN)++;
    }
return os;
}

static void pslOverallStats(char *pslFile, char *statsFile,
                            char *querySizeFile)
/* collect and output overall stats */
{
struct hash *queryStatsTbl = collectQueryStats(pslFile, querySizeFile);
int aligned1 = 0, alignedN = 0;
struct sumStats os = sumOverAllStats(queryStatsTbl, &aligned1, &alignedN);
outputOverallStats(statsFile, &os, aligned1, alignedN);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
warnOnConflicts = optionExists("warnOnConflicts");
char *querySizeFile = optionVal("queries", NULL);
if (optionExists("queryStats") && optionExists("overallStats"))
    errAbort("can't specify both -queryStats and -overallStats");
if (optionExists("queryStats"))
    pslQueryStats(argv[1], argv[2], querySizeFile);
else if (optionExists("overallStats"))
    pslOverallStats(argv[1], argv[2], querySizeFile);
else
    pslAlignStats(argv[1], argv[2], querySizeFile);
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

