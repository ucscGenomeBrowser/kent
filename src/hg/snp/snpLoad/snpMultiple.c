/* snpMultiple - eleventh step in dbSNP processing.
 * Run on hgwdev.
 * Read snp125.
 * Load unique names into hash.
 * Check count per name. 
 * Log count > 1. */

#include "common.h"

#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpMultiple.c,v 1.7 2006/02/24 21:15:31 heather Exp $";

static char *snpDb = NULL;
static struct hash *snpHash = NULL;
static struct slName *nameList = NULL;
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
    "snpMultiple - read snp125 for rsIds that appear more than once\n"
    "usage:\n"
    "    snpMultiple snpDb \n");
}


void readSnps()
/* put all coords in hash */
/* use hash to generate list of snp names with multiple hashes (store in nameList) */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *hel = NULL;
struct slName *sel = NULL;
struct coords *cel = NULL;

snpHash = newHash(22);
verbose(1, "getting name list and coords hash...\n");
safef(query, sizeof(query), "select name, chrom, chromStart, chromEnd from snp125");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    /* have we already seen this snp name? */
    /* if so, save it in nameList */
    hel = hashLookup(snpHash, row[0]);
    if (hel != NULL) 
        {
	sel = newSlName(row[0]);
	slAddHead(&nameList, sel);
	}
    /* store all coords */
    AllocVar(cel);
    cel->chrom = cloneString(row[1]);
    cel->start = sqlUnsigned(row[2]);
    cel->end = sqlUnsigned(row[3]);
    hashAdd(snpHash, cloneString(row[0]), cel);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}


void getCount()
/* loop through list of names with multiple matches */
/* print all coords from snpHash to outputFileHandle */
/* also print count per SNP to logFileHandle */
{
struct slName *namePtr = NULL;
struct hashEl *hel = NULL;
struct coords *cel = NULL;
int count = 0;

verbose(1, "checking counts...\n");
for (namePtr = nameList; namePtr != NULL; namePtr = namePtr->next)
    {
    count = 0;
    for (hel = hashLookup(snpHash, namePtr->name); hel != NULL; hel = hashLookupNext(hel))
        {
	cel = (struct coords *)hel->val;
	fprintf(outputFileHandle, "%s\t%s\t%d\t%d\n", 
	        namePtr->name, cel->chrom, cel->start, cel->end);
	count++;
	}
    fprintf(logFileHandle, "%s\t%d\n", namePtr->name, count);
    }
}


int main(int argc, char *argv[])
/* Read snp125. */
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
getCount();
// free hash
carefulClose(&outputFileHandle);
carefulClose(&logFileHandle);

return 0;
}
