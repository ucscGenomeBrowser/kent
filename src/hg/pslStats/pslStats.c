/* pslStats - collect statistics from a psl file */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
#include "localmem.h"
#include "psl.h"

static char const rcsid[] = "$Id: pslStats.c,v 1.1 2005/02/24 18:13:27 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslStats - collect statistics from a psl file.\n"
  "\n"
  "usage:\n"
  "   pslStats [options] pslFile alnStats queryStats\n"
  "\n"
  "Options:\n"
  );
}

struct alnStats
/* stats on an indivdual psl */
{
    float ident;     /* fraction identity */
    float qCover;    /* coverage of query */
    float repMatch;  /* fraction that is repeat matches */
};

float calcIdent(struct psl *psl)
/* get fraction ident for a psl */
{
unsigned aligned = psl->match + psl->misMatch + psl->repMatch;
if (aligned == 0)
    return 0.0;
else
    return ((float)(psl->match + psl->repMatch))/((float)(aligned));
}

float calcQCover(struct psl *psl)
/* calculate query coverage */
{
unsigned aligned = psl->match + psl->misMatch + psl->repMatch;
if (psl->qSize == 0)
    return 0.0; /* should never happen */
else
    return ((float)aligned)/((float)psl->qSize);
}

float calcRepMatch(struct psl *psl)
/* calculate fraction of aligned that is repeat match */
{
unsigned aligned = psl->match + psl->misMatch + psl->repMatch;
if (aligned == 0)
    return 0.0; /* should never happen */
else
    return ((float)psl->repMatch)/((float)aligned);
}

struct alnStats alnStatsCompute(struct psl *psl)
/* compute statistics for a PSL */
{
struct alnStats ps;
ps.ident = calcIdent(psl);
ps.qCover = calcQCover(psl);
ps.repMatch = calcRepMatch(psl);
return ps;
}

/* header for alignment statistics */
char *alnStatsHdr = "#qName\t" "qSize\t" "tName\t" "tStart\t" "tEnd\t"
"ident\t" "qCover\t" "repMatch\n";

void alnStatsOutput(FILE *fh, struct psl *psl, struct alnStats *as)
/* output statistic on a psl */
{
fprintf(fh, "%s\t%d\t%s\t%d\t%d\t%0.4f\t%0.4f\t%0.4f\n",
        psl->qName, psl->qSize, psl->tName, psl->tStart, psl->tEnd,
        as->ident, as->qCover, as->repMatch);
}

struct queryStats
/* structure to hold query statistics */
{
    char *qName;       /* query; string is in hash table */
    int qSize;         /* size of query */
    int alnCnt;        /* number of alignments */
    float minIdent;    /* min/max fraction identity */
    float maxIndent;
    float minQCover;   /* min/max coverage of query */
    float maxQCover;
    float minRepMatch; /* fraction that is repeat matches */
    float maxRepMatch;
};

struct queryStats *queryStatsGet(struct hash *queryStatsTbl,
                                 struct psl *psl)
/* lookup a queryStats, creating if it doesn't exist */
{
struct hashEl *hel = hashStore(queryStatsTbl, psl->qName);
if (hel->val == NULL)
    {
    struct queryStats *qs;
    AllocVar(qs);
    qs->qName = hel->name;  /* use string in hash */
    qs->qSize = psl->qSize;
    hel->val = qs;
    }
return hel->val;
}

void queryStatsCollect(struct hash *queryStatsTbl, struct psl *psl,
                       struct alnStats *as)
/* collect query statistics for a psl */
{
struct queryStats *qs = queryStatsGet(queryStatsTbl, psl);
if (qs->alnCnt == 0)
    {
    qs->minIdent = qs->maxIndent = as->ident;
    qs->minQCover = qs->maxQCover = as->qCover;
    qs->minRepMatch = qs->maxRepMatch = as->repMatch;
    }
else
    {
    qs->minIdent = min(qs->minIdent, as->ident);
    qs->maxIndent = max(qs->maxIndent, as->ident);
    qs->minQCover = min(qs->minQCover, as->qCover);
    qs->maxQCover = max(qs->maxQCover, as->qCover);
    qs->minRepMatch = min(qs->minRepMatch, as->repMatch);
    qs->maxRepMatch = max(qs->maxRepMatch, as->repMatch);
    }
qs->alnCnt++;
}

/* header for query statistics */
char *queryStatsHdr = "#qName\t" "qSize\t" "alnCnt\t" "minIdent\t" "maxIndent\t"
"minQCover\t" "maxQCover\t" "minRepMatch\t" "maxRepMatch\n";

void queryStatsOutput(FILE *fh, struct queryStats *qs)
/* output statistic on a query */
{
fprintf(fh, "%s\t%d\t%d\t%0.4f\t%0.4f\t%0.4f\t%0.4f\t%0.4f\t%0.4f\n",
        qs->qName, qs->qSize, qs->alnCnt, qs->minIdent, qs->maxIndent,
        qs->minQCover, qs->maxQCover, qs->minRepMatch, qs->maxRepMatch);
}

void collectStats(struct psl* psl, struct hash *queryStatsTbl, FILE *alnStatsFh)
/* collect statistics on a psl */
{
struct alnStats as = alnStatsCompute(psl);
alnStatsOutput(alnStatsFh, psl, &as);
queryStatsCollect(queryStatsTbl, psl, &as);
}

void outputQueryStats(struct hash *queryStatsTbl, char *queryStatsFile)
/* output statistics on queries */
{
struct hashCookie cookie = hashFirst(queryStatsTbl);
FILE *queryStatsFh = mustOpen(queryStatsFile, "w");
struct hashEl *hel;

fputs(queryStatsHdr, queryStatsFh);
while ((hel = hashNext(&cookie)) != NULL)
    queryStatsOutput(queryStatsFh, hel->val);

carefulClose(&queryStatsFh);
}

void pslStats(char *pslFile, char *alnStatsFile, char *queryStatsFile)
/* collect statistics from a psl file */
{
struct hash *queryStatsTbl = hashNew(20);
struct lineFile *pslLf = pslFileOpen(pslFile);
FILE *alnStatsFh = mustOpen(alnStatsFile, "w");
struct psl* psl;

fputs(alnStatsHdr, alnStatsFh);

while ((psl = pslNext(pslLf)) != NULL)
    {
    collectStats(psl, queryStatsTbl, alnStatsFh);
    pslFree(&psl);
    }
lineFileClose(&pslLf);

carefulClose(&alnStatsFh);
outputQueryStats(queryStatsTbl, queryStatsFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage();
pslStats(argv[1], argv[2], argv[3]);
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

