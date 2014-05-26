/* getChroms -- sample code */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "chromInfo.h"
#include "hdb.h"


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

sqlSafef(query, sizeof(query), "select chrom, size from chromInfo");
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
