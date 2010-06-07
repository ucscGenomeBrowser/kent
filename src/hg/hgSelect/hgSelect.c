/* hgSelect - select from genome tables, handling split tables and bin column */
#include "common.h"
#include "options.h"
#include "dystring.h"
#include "hdb.h"
#include "jksql.h"
#include "verbose.h"

static char const rcsid[] = "$Id: hgSelect.c,v 1.2 2008/09/03 19:18:57 markd Exp $";

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort(
    "%s\n"
    "hgSelect - select from genome tables, handling split tables and\n"
    "           bin column\n"
    "\n"
    "usage:\n"
    "   hgSelect [options] db table outFile\n"
    "\n"
    "Select rows from a table, handling select from split tables and dropping\n"
    "a bin column. \n"
    "\n"
    "Options:\n"
    "   -noRandom - Exclude *_random pseudo-chromsomes\n"
    "   -noHap - Exclude *_hap* pseudo-chromsomes\n"
    "   -where=where \n"
    "   -joinTbls=tbls - comman seperated list other tables, if required\n"
    "    by -where\n"
    "   -verbose=n - 2 outputs SQL\n",
    msg);
}

/* command line */
static struct optionSpec optionSpec[] = {
    {"noRandom", OPTION_BOOLEAN},
    {"noHap", OPTION_BOOLEAN},
    {"where", OPTION_STRING},
    {"joinTbls", OPTION_STRING},
    {NULL, 0}
};

static boolean noRandom;
static boolean noHap;
static char *where;
static char *joinTbls;

static boolean inclChrom(char *chrom)
/* check if rows for chromosome should be included */
{
if (noRandom && (strstr(chrom, "_random") != NULL))
    return FALSE;
if (noHap && (strstr(chrom, "_hap") != NULL))
    return FALSE;
return TRUE;
}

static void outputRow(FILE *outFh, struct hTableInfo *tblInfo, int numCols, char **row)
/* output a row */
{
int startCol = (tblInfo->hasBin ? 1 : 0);
int i;
for (i = startCol; i < numCols; i++)
    {
    if (i > startCol)
        fputc('\t', outFh);
    fputs(row[i], outFh);
    }
fputc('\n', outFh);
}

static void addWhereOrAnd(struct dyString *query, int clauseCnt)
/* add a `where' or `and' to the query as indicated by clauseCnt  */
{
if (clauseCnt == 0)
    dyStringAppend(query, " where");
else
    dyStringAppend(query, " and");
}

static void addWhereClause(struct hTableInfo *tblInfo,
                           struct dyString *query)
/* Build up where clause. If this isn't a split table and there are chrom
 * restrictions, include chrom restiction in the where. */
{

int clauseCnt = 0;
if (where != NULL)
    {
    addWhereOrAnd(query, clauseCnt++);
    dyStringPrintf(query, " %s", where);
    ;
    }
if (!tblInfo->isSplit && noRandom)
    {
    addWhereOrAnd(query, clauseCnt++);
    dyStringPrintf(query, " (%s not like \"%%__random\")", tblInfo->chromField);
    }
if (!tblInfo->isSplit && noHap)
    {
    addWhereOrAnd(query, clauseCnt++);
    dyStringPrintf(query, " (%s not like \"%%__hap%%\")", tblInfo->chromField);
    }
}

static void selectFromTable(char *table, struct hTableInfo *tblInfo,
                            struct sqlConnection *conn, FILE *outFh)
/* select from a table and output rows */
{
struct dyString *query = dyStringNew(0);
dyStringPrintf(query, "SELECT %s.* FROM %s", table, table);
if (joinTbls != NULL)
    dyStringPrintf(query, ",%s", joinTbls);
addWhereClause(tblInfo, query);
verbose(2, "query: %s\n", query->string);

struct sqlResult *sr = sqlGetResult(conn, query->string);
int numCols = sqlCountColumns(sr);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    outputRow(outFh, tblInfo, numCols, row);
sqlFreeResult(&sr);
dyStringFree(&query);
}

static void selectFromSplitTable(char *db, char *table, struct hTableInfo *tblInfo,
                                 struct sqlConnection *conn, FILE *outFh)
/* select from a split table */
{
struct slName *chroms = hAllChromNames(db);
struct slName *chrom;
char chromTable[256];
for (chrom = chroms; chrom != NULL ; chrom = chrom->next)
    if (inclChrom(chrom->name))
        {
        safef(chromTable, sizeof(chromTable), "%s_%s", chrom->name, table);
        selectFromTable(chromTable, tblInfo, conn, outFh);
        }
}

void hgSelect(char *db, char *table, char *outFile)
/* select from genome tables, handling split tables and bin column */
{
struct hTableInfo *tblInfo;

/* get table info upfront so don't have to wait long find for error */
tblInfo = hFindTableInfo(db, NULL, table);
if (tblInfo == NULL)
    errAbort("Error: no table: %s or *_%s", table, table);

struct sqlConnection *conn = hAllocConn(db);
FILE* outFh = mustOpen(outFile, "w");
if (tblInfo->isSplit)
    selectFromSplitTable(db, table, tblInfo, conn, outFh);
else
    selectFromTable(table, tblInfo, conn, outFh);

carefulClose(&outFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpec);
if (argc != 4)
    usage("wrong # of args");
noRandom = optionExists("noRandom");
noHap = optionExists("noHap");
where = optionVal("where", NULL);
joinTbls = optionVal("joinTbls", NULL);

hgSelect(argv[1], argv[2], argv[3]);
return 0;
}
