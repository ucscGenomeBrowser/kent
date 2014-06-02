/* snpCheckLocTypeSize - fifth step in dbSNP processing.
 * Read the chrN_snpTmp database tables created by snpExpandAllele and check size.
 * Write exceptions.  No need to rewrite to new chrN_snpTmp tables.  
 * Get chromInfo from ContigInfo.  */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "hash.h"
#include "hdb.h"


char *snpDb = NULL;
char *contigGroup = NULL;
static struct hash *chromHash = NULL;
FILE *exceptionFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpCheckLocTypeSize - check size based on locType in chrN_snpTmp\n"
    "usage:\n"
    "    snpExpandAllele snpDb contigGroup\n");
}


struct hash *loadChroms(char *contigGroup)
/* hash all chromNames that match contigGroup */
{
struct hash *ret;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *chromName;

ret = newHash(0);
sqlSafef(query, sizeof(query), "select distinct(contig_chr) from ContigInfo where group_term = '%s'", contigGroup);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    chromName = cloneString(row[0]);
    hashAdd(ret, chromName, NULL);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

void writeToExceptionFile(char *chrom, int start, int end, char *name, char *exception)
{
fprintf(exceptionFileHandle, "%s\t", chrom);
fprintf(exceptionFileHandle, "%d\t", start);
fprintf(exceptionFileHandle, "%d\t", end);
fprintf(exceptionFileHandle, "%s\t", name);
fprintf(exceptionFileHandle, "%s\n", exception);
}

void doCheckSize(char *chromName)
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];
char *allele = NULL;
int count = 0;
int errorCount = 0;
int locType = 0;
int start = 0;
int end = 0;

strcpy(tableName, "chr");
strcat(tableName, chromName);
strcat(tableName, "_snpTmp");

sqlSafef(query, sizeof(query), "select snp_id, chromStart, chromEnd, loc_type, orientation, allele from %s ", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    start = atoi(row[1]);
    end = atoi(row[2]);
    locType = atoi(row[3]);

    /* exact */
    if (locType == 2)
        {
	if (end != start + 1) 
	    writeToExceptionFile(chromName, start, end, row[0], "ExactLocTypeWrongSize");
	continue;
	}

    /* between */
    if (locType == 3)
        {
	if (end != start)
	    writeToExceptionFile(chromName, start, end, row[0], "BetweenLocTypeWrongSize");
	continue;
	}


    count++;
    // short-circuit
    // if (count == 1000) 
        // {
        // sqlFreeResult(&sr);
        // hFreeConn(&conn);
        // fclose(f);
        // return;
	// }
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}


int main(int argc, char *argv[])
/* read chrN_snpTmp, check size based on locType */
{
struct hashCookie cookie;
struct hashEl *hel;
char *chromName;

if (argc != 3)
    usage();

snpDb = argv[1];
contigGroup = argv[2];
hSetDb(snpDb);

chromHash = loadChroms(contigGroup);
if (chromHash == NULL) 
    {
    verbose(1, "couldn't get chrom info\n");
    return 0;
    }

exceptionFileHandle = mustOpen("snpCheckLocTypeSize.exceptions", "w");

// doCheckSize("22");
// return 0;

cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    {
    verbose(1, "chrom = %s\n", chromName);
    doCheckSize(chromName);
    }

fclose(exceptionFileHandle);
return 0;
}
