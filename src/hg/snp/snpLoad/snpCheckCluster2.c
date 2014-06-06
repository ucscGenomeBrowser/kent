/* snpCheckCluster2 -- check for clustering errors using binRange. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "binRange.h"
#include "dystring.h"
#include "hdb.h"


static char *database = NULL;
static char *snpTable = NULL;
static FILE *outputFileHandle = NULL;
static struct binKeeper *snps = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpCheckCluster2 - check for clustering errors using binRange\n"
    "usage:\n"
    "    snpCheckCluster2 database snpTable\n");
}


void getBinKeeper(char *chromName)
/* put SNPs in binKeeper */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

int start = 0;
int end = 0;
char *rsId = NULL;

int chromSize = hChromSize(chromName);

verbose(1, "constructing binKeeper...\n");
snps = binKeeperNew(0, chromSize);
sqlSafef(query, sizeof(query), 
      "select chromStart, chromEnd, name from %s where chrom = '%s'", snpTable, chromName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    start = sqlUnsigned(row[0]);
    end = sqlUnsigned(row[1]);
    rsId = cloneString(row[2]);
    binKeeperAdd(snps, start, end, rsId);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
}

void checkForClusters(char *chromName)
/* describe collisions */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

int start = 0;
int end = 0;
char *rsId = NULL;

struct binElement *el, *elList = NULL;

char *matchName = NULL;
int candidateCount = 0;
int matchCount = 0;

verbose(1, "checking for collisions...\n");
sqlSafef(query, sizeof(query), 
      "select chromStart, chromEnd, name from %s where chrom = '%s'", snpTable, chromName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    candidateCount++;
    start = sqlUnsigned(row[0]);
    end = sqlUnsigned(row[1]);
    rsId = cloneString(row[2]);

    elList = binKeeperFind(snps, start, end);
    for (el = elList; el != NULL; el = el->next)
        {
        matchName = cloneString((char *)el->val);
        /* skip self hits */
        if (sameString(matchName, rsId)) continue;
        fprintf(outputFileHandle, "%s\t%d\t%d\t%s\n", chromName, start, end, (char *)el->val);
	matchCount++;
	}
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "  candidate count = %d\n", candidateCount);
verbose(1, "  match count = %d\n", matchCount);
}

int main(int argc, char *argv[])
/* read snpTable, report positions with more than annotation */
{
if (argc != 3)
    usage();

database = argv[1];
hSetDb(database);

snpTable = argv[2];
if (!hTableExists(snpTable)) 
    errAbort("no %s table\n", snpTable);

// chromList = hAllChromNames();

// outputFileHandle = mustOpen("snpCheckCluster2.out", "w");
// for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    // {
    // verbose(1, "chrom = %s\n", chromPtr->name);
    // getBinKeeper(chromPtr->name);
    // checkForClusters(chromPtr->name);
    // }

// carefulClose(&outputFileHandle);

outputFileHandle = mustOpen("snpCheckCluster2.chr22", "w");
getBinKeeper("chr22");
checkForClusters("chr22");
carefulClose(&outputFileHandle);
return 0;
}
