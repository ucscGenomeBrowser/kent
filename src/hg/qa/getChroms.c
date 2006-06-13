/* getChroms -- sample code */

#include "common.h"
#include "chromInfo.h"
#include "hdb.h"

static char const rcsid[] = "$Id: getChroms.c,v 1.1 2006/06/13 18:17:29 heather Exp $";

static char *db = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "getChroms - print chrom names\n"
    "usage:\n"
    "    getChroms database\n");
}


void getChroms()
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct chromInfo *el;

safef(query, sizeof(query), "select chrom, size from chromInfo");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = chromInfoLoad(row);
    printf("chrom = %s, size = %d\n", el->chrom, el->size);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

int main(int argc, char *argv[])
{
if (argc != 2)
    usage();

db = argv[1];
hSetDb(db);
if (!hTableExists("chromInfo"))
    errAbort("no chromInfo table in %s\n", db);
getChroms();

return 0;
}
