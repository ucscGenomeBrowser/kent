/* positionalTblCheck - check that positional tables are sorted. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "verbose.h"
#include "hdb.h"

static char const rcsid[] = "$Id: positionalTblCheck.c,v 1.1 2008/07/18 04:37:03 markd Exp $";

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "positionalTblCheck - check that positional tables are sorted\n"
  "usage:\n"
  "   positionalTblCheck db table\n"
  "\n"
  "options:\n"
  "  -verbose=n  n>=2, print tables as checked"
  "\n"
  "This will check sorting of a table in a variety of formats.\n"
  "It looks for commonly used names for chrom and chrom start\n"
  "columns.  It also handles split tables\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

static boolean nextRow(struct sqlResult *sr, char **chromRet, int *startRet)
/* read the next row */
{
char **row = sqlNextRow(sr);
if (row == NULL)
    return FALSE;
*chromRet = row[0];
*startRet = sqlUnsigned(row[1]);
return TRUE;
}

static void checkTblOrder(struct sqlConnection *conn,
                          char *tbl, char *chromFld, char *startFld)
/* check sorting of a positional table */
{
verbose(2, "checking %s.%s\n", sqlGetDatabase(conn), tbl);
char prevChrom[256], *chrom;
prevChrom[0] = '\0';
int prevStart = 0, start;
off_t irow = 0;

char query[512];
safef(query, sizeof(query), "SELECT %s,%s FROM %s", chromFld, startFld, tbl);
struct sqlResult *sr = sqlGetResult(conn, query);
while (nextRow(sr, &chrom, &start))
    {
    if (!sameString(chrom, prevChrom))
        {
        safecpy(prevChrom, sizeof(prevChrom), chrom);
        prevStart = start;
        }
    else
        {
        if (start < prevStart)
            errAbort("table %s.%s not sorted starting at row %lld: %s:%d",
                     sqlGetDatabase(conn), tbl, (long long)irow, chrom, start);
        prevStart = start;
        }
    irow++;
    }
sqlFreeResult(&sr);
}

static void positionalTblCheck(char *db, char *table)
/* positionalTblCheck - check that positional tables are sorted. */
{
hSetDb(db);
struct hTableInfo *tblInfo = hFindTableInfoDb(db, NULL, table);
if (tblInfo == NULL)
    errAbort("cant find table %s.%s or %s.*_%s", db, table, db, table);
if (!tblInfo->isPos)
    errAbort("%s.%s does not appear to be a positional table", db, table);
struct slName *tbl, *tbls = hSplitTableNames(table);
struct sqlConnection *conn = hAllocConn();
for (tbl = tbls; tbl != NULL; tbl = tbl->next)
    checkTblOrder(conn, tbl->name, tblInfo->chromField, tblInfo->startField);
hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
positionalTblCheck(argv[1], argv[2]);
return 0;
}
