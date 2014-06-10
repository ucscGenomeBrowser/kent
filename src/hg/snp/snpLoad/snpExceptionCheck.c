/* snpExceptionCheck - make sure that all exceptions have coords that match the main table.
 * Read snp125. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "hash.h"
#include "hdb.h"


static char *snpDb = NULL;

static struct hash *coordHash = NULL;

FILE *outputFileHandle = NULL;
FILE *logFileHandle = NULL;

struct coords 
    {
    char *chrom;
    int start;
    int end;
    };


void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpExceptionCheck - make sure that all exceptions have coords that match the main table\n"
    "usage:\n"
    "    snpExceptionCheck snpDb \n");
}


void readSnps()
/* put all coords in coordHash */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *helCoord, *helName = NULL;
struct coords *cel = NULL;

coordHash = newHash(18);
verbose(1, "creating hash...\n");
sqlSafef(query, sizeof(query), "select name, chrom, chromStart, chromEnd from snp125");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    /* store all coords */
    AllocVar(cel);
    cel->chrom = cloneString(row[1]);
    cel->start = sqlUnsigned(row[2]);
    cel->end = sqlUnsigned(row[3]);
    hashAdd(coordHash, cloneString(row[0]), cel);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}


void checkExceptions()
{
struct hashEl *hel= NULL;
struct coords *cel = NULL;
char *name;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
boolean matchFound = FALSE;
char *chrom;
int start = 0;
int end = 0;

verbose(1, "checking exceptions...\n");
sqlSafef(query, sizeof(query), "select name, chrom, chromStart, chromEnd from snp125Exceptions");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    matchFound = FALSE;
    name = cloneString(row[0]);
    chrom = cloneString(row[1]);
    start = sqlUnsigned(row[2]);
    end = sqlUnsigned(row[3]);
    for (hel = hashLookup(coordHash, name); hel != NULL; hel= hashLookupNext(hel))
        {
	cel = (struct coords *)hel->val;
	if (sameString(cel->chrom, chrom) && cel->start == start && cel->end == end)
	    {
	    matchFound = TRUE;
	    break;
	    }
	}
    if (!matchFound)
        verbose(1, "no match found for %s at %s:%d-%d\n", name, chrom, start, end);
    }
}


int main(int argc, char *argv[])
/* Read snp125. */
{

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);
readSnps();
checkExceptions();
return 0;
}
