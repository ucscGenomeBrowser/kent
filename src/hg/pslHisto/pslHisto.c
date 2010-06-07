/* collect counts on PSL alignments for making histograms */
#include "common.h"
#include "pslTbl.h"
#include "psl.h"
#include "hash.h"
#include "options.h"

/* command line options and values */
static struct optionSpec optionSpecs[] =
{
    {"multiOnly", OPTION_BOOLEAN},
    {"nonZero", OPTION_BOOLEAN},
    {NULL, 0}
};
boolean gMultiOnly;
boolean gNonZero;

void usage(char *msg, ...)
/* usage msg and exit */
{
va_list ap;
va_start(ap, msg);
vfprintf(stderr, msg, ap);
errAbort("\n%s",
         "pslHisto [options] what inPsl outHisto\n"
         "\n"
         "Collect counts on PSL alignments for making histograms. These\n"
         "then be analyzed with R, textHistogram, etc.\n"
         "\n"
         "The 'what' argument determines what data to collect, the following\n"
         "are currently supported:\n"
         "\n"
         "  o alignsPerQuery - number of alignments per query. Output is one\n"
         "    line per query with the number of alignments.\n"
         "\n"
         "  o coverSpread - difference between the highest and lowest coverage\n"
         "    for alignments of a query.  Output line per query, with the difference.\n"
         "    Only includes queries with multiple alignments\n"
         "\n"
         "  o idSpread - difference between the highest and lowest fraction identity\n"
         "    for alignments of a query.  Output line per query, with the difference.\n"
         "\n" 
         "Options:\n"
         "   -multiOnly - omit queries with only one alignment from output.\n"
         "   -nonZero - omit queries with zero values.\n");
}

boolean shouldOutput(struct pslQuery *query)
/* should this query be written? */
{
return (!gMultiOnly) || (query->psls->next != NULL);
}

boolean shouldOutputIntVal(int val)
/* should this integer value be written? */
{
return (!gNonZero) || (val != 0);
}

boolean shouldOutputFloatVal(float val)
/* should this float value be written? */
{
return (!gNonZero) || (val != 0.0);
}

void alignsPerQuery(struct pslTbl *pslTbl, FILE *outFh)
/* collect counts of the number of alignments per query */
{
struct pslQuery *q;
struct hashCookie hc = hashFirst(pslTbl->queryHash);
while ((q = hashNextVal(&hc)) != NULL)
    if (shouldOutput(q))
        {
        int cnt =  slCount(q->psls);
        if (shouldOutputIntVal(cnt))
            fprintf(outFh, "%d\n", cnt);
        }
}

static float calcCover(struct psl *psl)
/* calculate coverage */
{
unsigned aligned = psl->match + psl->misMatch + psl->repMatch;
if (aligned == 0)
    return 0.0;
else
    return ((float)aligned)/((float)(psl->qSize));
}

float queryCoverSpread(struct pslQuery *query)
/* calculate cover difference for a query */
{
struct psl *psl = query->psls;
float minCov = calcCover(psl);
float maxCov = minCov;
for (psl = psl->next; psl != NULL; psl = psl->next)
    {
    float cov = calcCover(psl);
    minCov = min(minCov, cov);
    maxCov = max(maxCov, cov);
    }
return maxCov-minCov;
}

void coverSpread(struct pslTbl *pslTbl, FILE *outFh)
/* difference between the highest and lowest coverage for alignments of a
 * query. */
{
struct pslQuery *q;
struct hashCookie hc = hashFirst(pslTbl->queryHash);
while ((q = hashNextVal(&hc)) != NULL)
    if (shouldOutput(q))
        {
        float diff = queryCoverSpread(q);
        if (shouldOutputFloatVal(diff))
            fprintf(outFh, "%0.4g\n", diff);
        }
}

static float calcIdent(struct psl *psl)
/* get fraction ident for a psl */
{
unsigned aligned = psl->match + psl->misMatch + psl->repMatch;
if (aligned == 0)
    return 0.0;
else
    return ((float)(psl->match + psl->repMatch))/((float)(aligned));
}

float queryIdSpread(struct pslQuery *query)
/* calculate id difference for a query */
{
struct psl *psl = query->psls;
float minId = calcIdent(psl);
float maxId = minId;
for (psl = psl->next; psl != NULL; psl = psl->next)
    {
    float id = calcIdent(psl);
    minId = min(minId, id);
    maxId = max(maxId, id);
    }
return maxId-minId;
}

void idSpread(struct pslTbl *pslTbl, FILE *outFh)
/* difference between the highest and lowest ident for alignments of a
 * query. */
{
struct pslQuery *q;
struct hashCookie hc = hashFirst(pslTbl->queryHash);
while ((q = hashNextVal(&hc)) != NULL)
    if (shouldOutput(q))
        {
        float diff = queryIdSpread(q);
        if (shouldOutputFloatVal(diff))
            fprintf(outFh, "%0.4g\n", diff);
        }
}

void pslHisto(char *what, char *inPsl, char *outHisto)
/* collect counts on PSL alignments for making histograms */
{
struct pslTbl *pslTbl = pslTblNew(inPsl, NULL);
FILE *outFh = mustOpen(outHisto, "w");
if (sameString(what, "alignsPerQuery"))
    alignsPerQuery(pslTbl, outFh);
else if (sameString(what, "coverSpread"))
    coverSpread(pslTbl, outFh);
else if (sameString(what, "idSpread"))
    idSpread(pslTbl, outFh);
else
    usage("invalid what argument: %s", what);
carefulClose(&outFh);
pslTblFree(&pslTbl);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage("wrong # of args:");
gMultiOnly = optionExists("multiOnly");
gNonZero =  optionExists("nonZero");
pslHisto(argv[1], argv[2], argv[3]);
return 0;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

