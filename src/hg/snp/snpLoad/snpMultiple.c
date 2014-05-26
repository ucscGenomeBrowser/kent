/* snpMultiple - post-processing.
 * Read snp126.
 * Report coords for all SNPs that align more than once.
 * Also report counts. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "hash.h"
#include "hdb.h"


static char *snpDb = NULL;

static struct hash *coordHash = NULL;
static struct hash *nameHash = NULL;

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
    "snpMultiple - read snp126 for rsIds that appear more than once\n"
    "usage:\n"
    "    snpMultiple snpDb \n");
}


void readSnps()
/* put all coords in coordHash */
/* put names of SNPs that appear 2x or more in nameHash */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *helCoord, *helName = NULL;
struct coords *cel = NULL;

coordHash = newHash(18);
nameHash = newHash(0);
verbose(1, "creating hashes...\n");
sqlSafef(query, sizeof(query), "select name, chrom, chromStart, chromEnd from snp126");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    /* have we already seen this snp name? */
    /* if so, save it in nameList */
    helCoord = hashLookup(coordHash, row[0]);
    if (helCoord != NULL) 
        {
	helName = hashLookup(nameHash, row[0]);
	if (helName == NULL)
	    {
	    hashAdd(nameHash, cloneString(row[0]), NULL);
	    }
	}
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


void writeResults()
/* loop through nameHash */
/* print all coords from coordHash to outputFileHandle */
/* also print count per SNP to logFileHandle */
{
struct hashCookie cookie;
struct hashEl *hel= NULL;
struct coords *cel = NULL;
int count = 0;
char *name;

verbose(1, "writing results...\n");
cookie = hashFirst(nameHash);
while ((name = hashNextName(&cookie)) != NULL)
    {
    count = 0;
    for (hel = hashLookup(coordHash, name); hel != NULL; hel= hashLookupNext(hel))
        {
	cel = (struct coords *)hel->val;
	fprintf(outputFileHandle, "%s\t%d\t%d\t%s\tMultipleAlignments\n", cel->chrom, cel->start, cel->end, name);
	count++;
	}
    fprintf(logFileHandle, "%s\t%d\n", name, count);
    }
}


int main(int argc, char *argv[])
/* Read snp126. */
/* Write coords of multiple alignments to .tab file. */
/* Write counts to .log file. */
{

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);

outputFileHandle = mustOpen("snpMultiple.tab", "w");
logFileHandle = mustOpen("snpMultiple.log", "w");

readSnps();
writeResults();

// free hashes

carefulClose(&outputFileHandle);
carefulClose(&logFileHandle);

return 0;
}
