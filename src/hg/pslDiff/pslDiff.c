/* pslDiff - compare psl files */
#include "common.h"
#include "pslTbl.h"
#include "psl.h"
#include "hash.h"
#include "options.h"

/* command line options and values */
static struct optionSpec optionSpecs[] =
{
    {"details", OPTION_STRING},
    {"setNames", OPTION_BOOLEAN},
    {NULL, 0}
};

char *diffType = "counts";     /* type of diff to perform */
boolean haveSetNames = FALSE;  /* set names in args */

void usage(char *msg, ...)
/* usage msg and exit */
{
va_list ap;
va_start(ap, msg);
vfprintf(stderr, msg, ap);
errAbort("\n%s",
         "pslHisto [options] psl1 psl2 ...\n"
         "pslHisto [options] -setNames setName1 pslFile1 setName2 pslFile2 ...\n"
         "\n"
         "Compare queries in two or more psl files.\n"
         "Default criteria is to compare the number of alignments,\n"
         "\n"
         "Options:\n"
         "   -details=file - write details of psls that differ to this file\n"
         );
}

int queryAlignCount(struct pslQuery *query)
/* count alignments for a query, allow query to be NULL */
{
if (query == NULL)
    return 0;
else
    return slCount(query->psls);
}

struct pslTbl *loadPslTbls(int numPslSpecs, char **pslSpecs)
/* load list of psl files into a list of pslTbl objects */
{
struct pslTbl *pslTblList = NULL;
int i;
if (haveSetNames)
    {
    for (i = 0; i < numPslSpecs; i += 2)
        slSafeAddHead(&pslTblList, pslTblNew(pslSpecs[i+1], pslSpecs[i]));
    }
else
    {
    for (i = 0; i < numPslSpecs; i++)
        slSafeAddHead(&pslTblList, pslTblNew(pslSpecs[i], NULL));
    }
slReverse(&pslTblList);
return pslTblList;
}

void addQueryNames(struct hash *qNameHash, struct pslTbl *pslTbl)
/* add query names to the hash table if they are not already there. */
{
struct hashCookie hc = hashFirst(pslTbl->queryHash);
struct pslQuery *q;
while ((q = hashNextVal(&hc)) != NULL)
    hashStore(qNameHash, q->qName);  /* add if not there */
}

struct slName *getQueryNames(struct pslTbl *pslTblList)
/* get list of all query names*/
{
struct slName *qNames = NULL;
struct hash *qNameHash = hashNew(21);
struct pslTbl *pslTbl;
struct hashCookie hc;
struct hashEl *hel;

for (pslTbl = pslTblList; pslTbl != NULL; pslTbl = pslTbl->next)
    addQueryNames(qNameHash, pslTbl);

/* copy to a list */
hc = hashFirst(qNameHash);
while ((hel = hashNext(&hc)) != NULL)
    slSafeAddHead(&qNames, slNameNew(hel->name));
hashFree(&qNameHash);
slNameSort(&qNames);
return qNames;
}

struct pslTblQuery
/* Object with pslTbl and pslQuery for a give qName.  A list of these buildt
 * and then the pslQuery field filled in for each comparison.. */
{
    struct pslTblQuery *next;
    struct pslTbl *pslTbl;      /* table for this query */
    struct pslQuery *pslQuery;  /* query, or NULL if not in table */
};

struct pslTblQuery *allocPslTblQueries(struct pslTbl *pslTblList)
/* Get list of pslQuerySet objects paralleling pslTbl */
{
struct pslTblQuery *pslTblQueries = NULL;
struct pslTbl *pslTbl;

for (pslTbl = pslTblList; pslTbl != NULL; pslTbl = pslTbl->next)
    {
    struct pslTblQuery *ptq;
    AllocVar(ptq);
    ptq->pslTbl = pslTbl;
    slAddHead(&pslTblQueries, ptq);
    }
slReverse(&pslTblQueries);
return pslTblQueries;
}

void getPslQueries(char *qName, struct pslTblQuery *pslTblQueries)
/* fill in pslQuery results */
{
struct pslTblQuery *ptq;
for (ptq = pslTblQueries; ptq != NULL; ptq = ptq->next)
    ptq->pslQuery = hashFindVal(ptq->pslTbl->queryHash, qName);
}

/* header for details file */
char *detailsHdr = "#setName\t"
"qName\t" "qStart\t" "qEnd\t"
"tName\t" "tStart\t" "tEnd\t"
"strand\t" "nExons\t"
"ident\t" "repMatch\t" "cover\n" 
;

void writePslDetails(char *setName, struct psl *psl, FILE *detailsFh)
/* write details of a psl to a file */
{
unsigned aligned = psl->match + psl->misMatch + psl->repMatch;
float ident = ((float)(psl->match + psl->repMatch))/((float)(aligned));
float repMatch = ((float)psl->repMatch)/((float)(psl->match+psl->repMatch));
float cover = ((float)aligned)/((float)psl->qSize);

fprintf(detailsFh, "%s\t%s\t%d\t%d\t%s\t%d\t%d\t%s\t%d\t%0.3g\t%0.3g\t%0.3g\n",
        setName, psl->qName, psl->qStart, psl->qEnd,
        psl->tName, psl->tStart, psl->tEnd,
        psl->strand, psl->blockCount,
        ident, repMatch, cover);
}

void writeDetails(char *setName, struct pslQuery *query, FILE *detailsFh)
/* write details of queries to a file */
{
struct psl *psl;
for (psl = query->psls; psl != NULL; psl = psl->next)
    writePslDetails(setName, psl, detailsFh);
}

void writeSetDetails(struct pslTblQuery *pslTblQueries, FILE *detailsFh)
/* write details of queries from all sets */
{
struct pslTblQuery *ptq;
for (ptq = pslTblQueries; ptq != NULL; ptq = ptq->next)
    if (ptq->pslQuery != NULL)
        writeDetails(ptq->pslTbl->setName, ptq->pslQuery, detailsFh);
}

boolean diffCounts(char *qName, struct pslTblQuery *pslTblQueries, FILE *outFh)
/* basic diff on number of query psls */
{
struct pslTblQuery *ptq;
int cnt0 = queryAlignCount(pslTblQueries->pslQuery);
boolean same = TRUE;

for (ptq = pslTblQueries->next; same && (ptq != NULL); ptq = ptq->next)
    same = (queryAlignCount(ptq->pslQuery) == cnt0);

if (!same)
    {
    fputs(qName, outFh);
    for (ptq = pslTblQueries; ptq != NULL; ptq = ptq->next)
        fprintf(outFh, "\t%d", queryAlignCount(ptq->pslQuery));
    fputs("\n", outFh);
    }
return same;
}

void queryDiff(char *qName, struct pslTblQuery *pslTblQueries,
               FILE *outFh, FILE *detailsFh)
/* do requested diff of a query */
{
boolean same = FALSE;
getPslQueries(qName, pslTblQueries);
if (sameString(diffType, "counts"))
    same = diffCounts(qName, pslTblQueries, outFh);
else
    errAbort("invalid diff type: %s", diffType);
if (!same && (detailsFh != NULL))
    writeSetDetails(pslTblQueries, detailsFh);
}

void writeHeader(FILE *outFh, struct pslTblQuery *pslTblQueries)
/* write header, with a column for each psl */
{
struct pslTblQuery *ptq;
fputs("#qName", outFh);

for (ptq = pslTblQueries; ptq != NULL; ptq = ptq->next)
    fprintf(outFh, "\t%s", ptq->pslTbl->setName);
fputs("\n", outFh);
}

void pslDiff(int numPslSpecs, char **pslSpecs, char *detailsFile)
/* compare psl files */
{
struct pslTbl *pslTblList = loadPslTbls(numPslSpecs, pslSpecs);
struct slName *qNames = getQueryNames(pslTblList);
struct pslTblQuery *pslTblQueries = allocPslTblQueries(pslTblList);
struct slName *qName;
FILE *detailsFh = NULL;

writeHeader(stdout, pslTblQueries);

if (detailsFile != NULL)
    {
    detailsFh = mustOpen(detailsFile, "w");
    fputs(detailsHdr, detailsFh);
    }

for (qName = qNames; qName != NULL; qName = qName->next)
    queryDiff(qName->name, pslTblQueries, stdout, detailsFh);

carefulClose(&detailsFh);
pslTblFreeList(&pslTblList);
slFreeList(&qNames);
}

int main(int argc, char *argv[])
/* Process command line */
{
char *detailsFile;
optionInit(&argc, argv, optionSpecs);
haveSetNames = optionExists("setNames");
if (((haveSetNames) && (argc < 5))
    || ((!haveSetNames) && (argc < 3)))
    usage("wrong # of args:");

if (haveSetNames && ((argc-1)&1)) /* must have even number */
    usage("-setNames requires pairs of setName and pslFile");

detailsFile = optionVal("details", NULL);
pslDiff(argc-1, argv+1, detailsFile);
return 0;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

