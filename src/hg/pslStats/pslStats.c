/* pslStats - collect statistics from a psl file */
#include "common.h"
#include "options.h"
#include "obscure.h"
#include "linefile.h"
#include "hash.h"
#include "psl.h"

static char const rcsid[] = "$Id: pslStats.c,v 1.3 2007/04/08 00:35:23 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"queryStats", OPTION_BOOLEAN},
    {"overallStats", OPTION_BOOLEAN},
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
  "  -overallStats - output overall statstics.\n"
  );
}


struct sumStats
/* structure to hold summary statistics, either per query or overall */
{
    char *qName;        /* query name; string is in hash table */
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
    float minRepMatch; /* fraction that is repeat matches */
    float maxRepMatch;
};

static unsigned calcAligned(struct psl *psl)
/* compute the number of bases aligned */
{
return psl->match + psl->misMatch + psl->repMatch;
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
    return (float)(((double)ss->totalAlign)/((double)ss->totalQSize));
}

static float calcMeanRepMatch(struct sumStats *ss)
/* calculate the mean repmatch for a set of alignments */
{
if (ss->totalAlign == 0)
    return 0.0;
else
    return (float)(((double)ss->totalRepMatch)/((double)ss->totalAlign));
}

static void sumStatsAccumulate(struct sumStats *ss, struct psl *psl)
/* accumulate stats from psl into sumStats object */
{
float ident = calcIdent(psl);
float qCover = calcQCover(psl);
float repMatch = calcRepMatch(psl);
ss->totalQSize += psl->qSize;
if (ss->alnCnt == 0)
    {
    ss->minQSize = ss->maxQSize = psl->qSize;
    ss->minIdent = ss->maxIndent = ident;
    ss->minQCover = ss->maxQCover = qCover;
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
    ss->minRepMatch = min(ss->minRepMatch, repMatch);
    ss->maxRepMatch = max(ss->maxRepMatch, repMatch);
    }
ss->totalAlign += calcAligned(psl);
ss->totalMatch += calcMatch(psl);
ss->totalRepMatch += psl->repMatch;
ss->alnCnt++;
}

/* header for alignment statistics */
static char *alnStatsHdr = "#qName\t" "qSize\t" "tName\t" "tStart\t" "tEnd\t"
"ident\t" "qCover\t" "repMatch\n";

static void alnStatsOutput(FILE *fh, struct psl *psl)
/* output statistic on a psl */
{
fprintf(fh, "%s\t%d\t%s\t%d\t%d\t%0.4f\t%0.4f\t%0.4f\n",
        psl->qName, psl->qSize, psl->tName, psl->tStart, psl->tEnd,
        calcIdent(psl), calcQCover(psl), calcRepMatch(psl));
}

static void pslAlignStats(char *pslFile, char *statsFile)
/* collect and output per-alignment stats */
{
struct lineFile *pslLf = pslFileOpen(pslFile);
FILE *fh = mustOpen(statsFile, "w");
struct psl* psl;

fputs(alnStatsHdr, fh);
while ((psl = pslNext(pslLf)) != NULL)
    {
    alnStatsOutput(fh, psl);
    pslFree(&psl);
    }
lineFileClose(&pslLf);
carefulClose(&fh);
}

struct sumStats *queryStatsGet(struct hash *queryStatsTbl,
                               struct psl *psl)
/* lookup a stats on query by name, creating if it doesn't exist */
{
struct hashEl *hel = hashStore(queryStatsTbl, psl->qName);
if (hel->val == NULL)
    {
    struct sumStats *qs;
    AllocVar(qs);
    qs->qName = hel->name;  /* use string in hash */
    qs->queryCnt = 1;
    hel->val = qs;
    }
return hel->val;
}

/* header for query statistics */
static char *queryStatsHdr = "#qName\t" "qSize\t" "alnCnt\t" "minIdent\t" "maxIndent\t" "meanIdent\t"
"minQCover\t" "maxQCover\t" "meanQCover\t" "minRepMatch\t" "maxRepMatch\t" "meanRepMatch\n";

static void queryStatsOutput(FILE *fh, struct sumStats *qs)
/* output statistic on a query */
{
fprintf(fh, "%s\t%d\t%d\t" "%0.4f\t%0.4f\t%0.4f\t"
        "%0.4f\t%0.4f\t%0.4f\t"  "%0.4f\t%0.4f\t%0.4f\n",
        qs->qName, qs->minQSize, qs->alnCnt,
        qs->minIdent, qs->maxIndent, calcMeanIdent(qs),
        qs->minQCover, qs->maxQCover, calcMeanQCover(qs),
        qs->minRepMatch, qs->maxRepMatch, calcMeanRepMatch(qs));
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

static void pslQueryStats(char *pslFile, char *statsFile)
/* collect and output per-query stats */
{
struct hash *queryStatsTbl = hashNew(22);
struct lineFile *pslLf = pslFileOpen(pslFile);
struct psl* psl;

while ((psl = pslNext(pslLf)) != NULL)
    {
    sumStatsAccumulate(queryStatsGet(queryStatsTbl, psl), psl);
    pslFree(&psl);
    }
lineFileClose(&pslLf);
outputQueryStats(queryStatsTbl, statsFile);
}

/* header for overall statistics */
static char *overallStatsHdr = "#queryCnt\t" "minQSize\t" "maxQSize\t" "meanQSize\t"
"alnCnt\t" "minIdent\t" "maxIndent\t" "meanIdent\t"
"minQCover\t" "maxQCover\t" "meanQCover\t" "minRepMatch\t" "maxRepMatch\t" "meanRepMatch\n";

static void outputOverallStats(char *statsFile, struct sumStats *os)
/* output overall statistic */
{
FILE *fh = mustOpen(statsFile, "w");
fputs(overallStatsHdr, fh);
fprintf(fh, "%d\t%d\t%d\t%d\t%d\t" "%0.4f\t%0.4f\t%0.4f\t"
        "%0.4f\t%0.4f\t%0.4f\t"  "%0.4f\t%0.4f\t%0.4f\n",
        os->queryCnt, os->minQSize, os->maxQSize, calcMeanQSize(os),
        os->alnCnt,
        os->minIdent, os->maxIndent, calcMeanIdent(os),
        os->minQCover, os->maxQCover, calcMeanQCover(os),
        os->minRepMatch, os->maxRepMatch, calcMeanRepMatch(os));
carefulClose(&fh);
}

static void pslOverallStats(char *pslFile, char *statsFile)
/* collect and output overall stats */
{
struct hash *qNameTbl = hashNew(22); // each qName to int value of 1
struct sumStats os;
ZeroVar(&os);
struct lineFile *pslLf = pslFileOpen(pslFile);
struct psl* psl;

while ((psl = pslNext(pslLf)) != NULL)
    {
    hashStore(qNameTbl, psl->qName)->val = intToPt(1);
    sumStatsAccumulate(&os, psl);
    pslFree(&psl);
    }
lineFileClose(&pslLf);
os.queryCnt = hashIntSum(qNameTbl);
outputOverallStats(statsFile, &os);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
if (optionExists("queryStats") && optionExists("overallStats"))
    errAbort("can't specify both -queryStats and -overallStats");
if (optionExists("queryStats"))
    pslQueryStats(argv[1], argv[2]);
else if (optionExists("overallStats"))
    pslOverallStats(argv[1], argv[2]);
else
    pslAlignStats(argv[1], argv[2]);
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

