/* snpCheckClassAndObserved
 *
 * Log exceptions:
 *
 * 1) SingleClassWrongLocType
 *    loc_type = 3 (between)
 *    and class = 'single'
 *
 * 2) DeletionClassWrongObservedSize
 *    for class = 'deletion', compare the length 
 *    of the observed string to chromRange
 *
 * 3) NamedClassWrongLocType
 *    for class = 'named'
 *    if observed like "LARGEDELETION" then expect loc_type = 1
 *    if observed like "LARGEINSERTION" then expect loc_type = 3
 *
 * Use UCSC chromInfo.  */


#include "common.h"

#include "chromInfo.h"
#include "dystring.h"
#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpCheckClassAndObserved.c,v 1.1 2006/02/14 19:48:27 heather Exp $";

static char *snpDb = NULL;
static struct hash *chromHash = NULL;
FILE *exceptionFileHandle = NULL;


void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpCheckClassAndObserved - log exceptions with class and observed\n"
    "usage:\n"
    "    snpCheckClassAndObserved snpDb \n");
}


struct hash *loadChroms()
/* hash from UCSC chromInfo */
/* not using size */
{
struct hash *ret;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *randomSubstring = NULL;
struct chromInfo *el;
char tableName[64];

ret = newHash(0);
safef(query, sizeof(query), "select chrom, size from chromInfo");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    randomSubstring = strstr(row[0], "random");
    if (randomSubstring != NULL) continue;
    safef(tableName, ArraySize(tableName), "%s_snpTmp", row[0]);
    if (!hTableExists(tableName)) continue;
    el = chromInfoLoad(row);
    hashAdd(ret, el->chrom, (void *)(& el->size));
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

void writeToExceptionFile(char *chrom, char *start, char *end, char *name, char *exception)
{
fprintf(exceptionFileHandle, "%s\t%s\t%s\t%s\t%s\n", chrom, start, end, name, exception);
}



void doCheck(char *chromName)
/* check each row for exceptions */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];
int loc_type = 0;
int alleleLen = 0;
int span = 0;
char *subString = NULL;

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
if (!hTableExists(tableName)) return;

verbose(1, "chrom = %s\n", chromName);
safef(query, sizeof(query), 
    "select snp_id, chromStart, chromEnd, loc_type, class, observed from %s", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    loc_type = sqlUnsigned(row[3]);

    /* SingleClassWrongLocType */
    if (loc_type == 3 && sameString(row[4], "single"))
        writeToExceptionFile(chromName, row[1], row[2], row[0], "SingleClassWrongLocType");

    /* DeletionClassWrongObservedSize */
    if (sameString(row[4], "deletion"))
        {
	alleleLen = strlen(row[5]);
	alleleLen = alleleLen - 2;
	span = sqlUnsigned(row[2]) - sqlUnsigned(row[1]);
	if (alleleLen != span)
            writeToExceptionFile(chromName, row[1], row[2], row[0], "DeletionClassWrongObservedSize");
	}

    /* NamedClassWrongLocType */
    if (sameString(row[4], "named"))
        {
	subString = strstr(row[5], "LARGEINSERTION");
	if (subString != NULL && loc_type != 3)
            writeToExceptionFile(chromName, row[1], row[2], row[0], "NamedClassWrongLocType");
        subString = strstr(row[5], "LARGEDELETION");
	if (subString != NULL && loc_type != 1)
            writeToExceptionFile(chromName, row[1], row[2], row[0], "NamedClassWrongLocType");
        }
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* read chrN_snpTmp, log exceptions */
{
struct hashCookie cookie;
struct hashEl *hel;
char *chromName;

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);

chromHash = loadChroms();
if (chromHash == NULL) 
    {
    verbose(1, "couldn't get chrom info\n");
    return 1;
    }

exceptionFileHandle = mustOpen("snpCheckClassAndObserved.exceptions", "w");

cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    doCheck(chromName);

carefulClose(&exceptionFileHandle);
return 0;
}
