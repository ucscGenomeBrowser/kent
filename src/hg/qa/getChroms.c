/* getChroms -- sample code */

#include "common.h"
#include "chromInfo.h"
#include "hdb.h"

static char const rcsid[] = "$Id: getChroms.c,v 1.1.106.1 2008/07/31 05:21:41 markd Exp $";

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
struct sqlConnection *conn = hAllocConn(db);
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
if (!hTableExists(db, "chromInfo"))
    errAbort("no chromInfo table in %s\n", db);
getChroms();

return 0;
}
