/* snpGetInsertions -- generate a filtered set of insertions. */
/* hash in exceptions for mixed observed and multiple alignments */
/* filtering on exceptions and weight */
/* write to insertions.tab */
/* use a zero score column to preserve BED 6 */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* assuming input already filtered on locType = 'between' and class = 'insertion' */
/* assert end == start */
/* checking format of observed string */

/* not loading to database table */
/* could load to a table called snpTableFiltered */

/* not splitting by chrom */
/* not generating a bin */

/* not checking for position < chromSize; seqWithInsertions does this */

#include "common.h"
#include "hdb.h"


static char *database = NULL;
static char *snpTable = NULL;
static char *exceptionsTable = NULL;
static struct hash *exceptionsHash = NULL;
static FILE *outputFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpGetInsertions - generate file of filtered insertions\n"
    "usage:\n"
    "    snpGetInsertions database snpTable exceptionsTable\n");
}

void getExceptions()
/* create a hash of exceptions so we can exclude them */
{   
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *hel = NULL;
char *snpId = NULL;
char *exception = NULL;

exceptionsHash = newHash(0);
sqlSafef(query, sizeof(query), "select name, exception from %s", exceptionsTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    snpId = cloneString(row[0]);
    exception = cloneString(row[1]);
    if (sameString(exception, "MultipleAlignments") || sameString(exception, "MixedObserved")) 
        {
        hel = hashLookup(exceptionsHash, snpId);
        if (hel == NULL)
            hashAdd(exceptionsHash, snpId, NULL);
	}
    }
}

boolean isValidObserved(char *observed)
{
int slashCount = 0;

if (strlen(observed) < 2)
    return FALSE;

if (observed[0] != '-')
    return FALSE;

if (observed[1] != '/')
    return FALSE;

slashCount = chopString(observed, "/", NULL, 0);
if (slashCount > 2)
    return FALSE;

return TRUE;
}


void getInsertions()
/* assume snpTable already filtered on locType = 'between' and class = 'insertions' */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

int start = 0;
int end = 0;
int weight = 0;
int candidateCount = 0;
int matchCount = 0;

char *snpChrom = NULL;
char *rsId = NULL;
char *strand = NULL;
char *observed = NULL;

struct hashEl *hel;

sqlSafef(query, sizeof(query), 
    "select chrom, chromStart, chromEnd, name, strand, observed, weight from %s", snpTable);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    candidateCount++;
    snpChrom = cloneString(row[0]);
    if (!sameString(snpChrom, "chrY")) continue;
    start = sqlUnsigned(row[1]);
    end = sqlUnsigned(row[2]);
    rsId = cloneString(row[3]);
    strand = cloneString(row[4]);
    observed = cloneString(row[5]);
    weight = sqlUnsigned(row[6]);

    if (weight != 1) continue;

    assert (end == start);

    if (!isValidObserved(observed)) continue;

    hel = hashLookup(exceptionsHash, rsId);
    if (hel != NULL) continue;

    fprintf(outputFileHandle, "%s\t%s\t%s\t%s\t0\t%s\t%s\n", row[0], row[1], row[2], row[3], row[4], row[5]); 
    matchCount++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "   candidateCount = %d\n", candidateCount);
verbose(1, "   matchCount = %d\n", matchCount);
}


int main(int argc, char *argv[])
/* read snpTable, output insertions that pass input filtering */
{
struct slName *chromList = NULL;

if (argc != 4)
    usage();

database = argv[1];
hSetDb(database);

snpTable = argv[2];
if (!hTableExists(snpTable)) 
    errAbort("no %s table\n", snpTable);

exceptionsTable = argv[3];
if (!hTableExists(exceptionsTable)) 
    errAbort("no %s table\n", exceptionsTable);

verbose(1, "loading exceptions...\n");
getExceptions();

chromList = hAllChromNames();

outputFileHandle = mustOpen("insertionsChrY.tab", "w");
getInsertions();
carefulClose(&outputFileHandle);

return 0;
}
