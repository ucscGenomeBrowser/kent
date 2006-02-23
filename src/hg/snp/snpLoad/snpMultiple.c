/* snpMultiple - eleventh step in dbSNP processing.
 * Run on hgwdev.
 * Read snp125.
 * Load unique names into hash.
 * Check count per name. 
 * Log count > 1. */

#include "common.h"

#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpMultiple.c,v 1.2 2006/02/23 06:43:11 heather Exp $";

static char *snpDb = NULL;
static struct hash *nameHash = NULL;
FILE *outputFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpMultiple - read snp125 for rsIds that appear more than once\n"
    "usage:\n"
    "    snpMultiple snpDb \n");
}


void readSnps()
/* put all distinct names in hash */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *el = NULL;

nameHash = newHash(0);
verbose(1, "getting names...\n");
safef(query, sizeof(query), "select name from snp125");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = hashLookup(nameHash, row[0]);
    if (el != NULL) continue;
    hashAdd(nameHash, cloneString(row[0]), NULL);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}


void getCount()
/* log count > 1 */
{
struct hashCookie cookie;
struct hashEl *hel;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int count = 0;
char *name = NULL;

verbose(1, "checking counts...\n");
cookie = hashFirst(nameHash);
while ((name = hashNextName(&cookie)) != NULL) 
    {
    safef(query, sizeof(query), "select count(*) from snp125 where name = %s", name);
    count = sqlQuickNum(conn, query);
    if (count == 0) continue;
    safef(query, sizeof(query), "select chrom, chromStart, chromEnd from snp125 where name = %s", name);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	fprintf(outputFileHandle, "%s\t%s\t%s\t%s\n", name, row[0], row[1], row[2]);
	verbose(1, "%s\t%s\t%s\t%s\n", name, row[0], row[1], row[2]);
	}
    verbose(1, "----------------------------\n");
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
}


int main(int argc, char *argv[])
/* get distinct names from snp125, record count > 1 */
{

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);
outputFileHandle = mustOpen("snpMultiple.tab", "w");
readSnps();
getCount();
// free hash
carefulClose(&outputFileHandle);

return 0;
}
