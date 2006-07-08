
/* affyLookup - Find rsIds for Affy SNPs. */
#include "common.h"
#include "binRange.h"
#include "hdb.h"

static char const rcsid[] = "$Id: affyLookup.c,v 1.1 2006/07/08 00:25:43 heather Exp $";

char *database = NULL;
char *affyTable = NULL;
char *snpTable = NULL;
char *outputFile = NULL;
char *errorFile = NULL;
FILE *errorFileHandle = NULL;
int missCount = 0;
int multipleCount = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "affyLookup - find rsIds for Affy SNPs\n"
    "usage:\n"
    "    affyLookup database affyTable snpTable outputFile\n");
}



struct binKeeper *readSnps(char *chrom)
/* read snps for chrom into a binKeeper */
{
int chromSize = hChromSize(chrom);
struct binKeeper *ret = binKeeperNew(0, chromSize);
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *name = NULL;
int start = 0;
int end = 0;
char *class = NULL;
char *locType = NULL;

safef(query, sizeof(query), "select name, chromStart, chromEnd, class, locType from %s where chrom='%s' ", snpTable, chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    /* this is specific to snp124 */
    class = cloneString(row[3]);
    if (!sameString(class, "snp")) continue;
    locType = cloneString(row[4]);
    if (!sameString(locType, "exact")) continue;
    start = sqlUnsigned(row[1]);
    end = sqlUnsigned(row[2]);
    if (end != start + 1) continue;

    name = cloneString(row[0]);
    binKeeperAdd(ret, start, end, name);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}


void affyLookup(char *chromName)
{
struct binKeeper *snps = NULL;
struct binElement *elList = NULL;
char fileName[64];
FILE *fileHandle = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int start = 0;
int end = 0;

safef(fileName, sizeof(fileName), "%s.%s", outputFile, chromName);
fileHandle = mustOpen(fileName, "w");

snps = readSnps(chromName);

safef(query, sizeof(query), "select * from %s where chrom='%s' ", affyTable, chromName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    start = sqlUnsigned(row[2]);
    end = sqlUnsigned(row[3]);
    elList = binKeeperFind(snps, start, end);
    if (elList == NULL)
        {
	missCount++;
        fprintf(errorFileHandle, "Missing rsId for affy SNP %s\n", row[4]);
        fprintf(fileHandle, "%s\t%s\t%s\t%s\t", row[0], row[1], row[2], row[3]);
        fprintf(fileHandle, "%s\t%s\t%s\t%s\t", row[4], row[5], row[6], row[7]);
        fprintf(fileHandle, "%s\n", "unknown");
	continue;
	}
     /* just use the first one found */
     fprintf(fileHandle, "%s\t%s\t%s\t%s\t", row[0], row[1], row[2], row[3]);
     fprintf(fileHandle, "%s\t%s\t%s\t%s\t", row[4], row[5], row[6], row[7]);
     fprintf(fileHandle, "%s\n", (char *)elList->val);
     /* check for extras */
     if (elList->next != NULL)
        {
	multipleCount++;
        fprintf(errorFileHandle, "Multiple rsIds for affy SNP %s\n", row[4]);
	}
    }
hFreeConn(&conn);
carefulClose(&fileHandle);
}


int main(int argc, char *argv[])
/* Check args and call affyLookup. */
{
struct slName *chromList, *chromPtr;
if (argc != 5)
    usage();
database = argv[1];
if(!hDbExists(database))
    errAbort("%s does not exist\n", database);
hSetDb(database);
affyTable = argv[2];
if(!hTableExistsDb(database, affyTable))
    errAbort("no %s in %s\n", affyTable, database);
snpTable = argv[3];
if(!hTableExistsDb(database, snpTable))
    errAbort("no %s in %s\n", snpTable, database);
outputFile = argv[4];
errorFileHandle = mustOpen("affyLookup.err", "w");
chromList = hAllChromNamesDb(database);
for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    verbose(1, "chrom = %s\n", chromPtr->name);
    affyLookup(chromPtr->name);
    }
carefulClose(&errorFileHandle);
verbose(1, "missing count = %d\n", missCount);
verbose(1, "multiple count = %d\n", multipleCount);
return 0;
}

