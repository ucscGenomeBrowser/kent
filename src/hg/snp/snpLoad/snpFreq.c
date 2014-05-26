/* snpFreq - create snpFreq table from SNPAlleleFreq and Allele tables. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "dystring.h"
#include "hash.h"
#include "hdb.h"
#include "jksql.h"
#include "linefile.h"
#include "snp125.h"
#include "snp125Exceptions.h"


static char *snpDb = NULL;
static char *snpTable = NULL;
static struct hash *alleleHash = NULL;
static struct hash *snpHash = NULL;

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
    "snpFreq - create snpFreq table from SNPAlleleFreq and Allele tables.\n"
    "usage:\n"
    "    snpFinalTable snpDb snpTable\n");
}


void createAlleleHash()
/* load the Allele table into global alleleHash */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

alleleHash = newHash(0);
sqlSafef(query, sizeof(query), "select allele_id, allele from Allele");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(alleleHash, cloneString(row[0]), cloneString(row[1]));
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void createSnpHash(char *snpTable)
/* load the snps into global snpHash */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct coords *coordsInstance = NULL;
int count = 0;

snpHash = newHash(0);
sqlSafef(query, sizeof(query), "select chrom, chromStart, chromEnd, name from %s", snpTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    count++;
    AllocVar(coordsInstance);
    coordsInstance->chrom = cloneString(row[0]);
    coordsInstance->start = sqlUnsigned(row[1]);
    coordsInstance->end = sqlUnsigned(row[2]);
    hashAdd(snpHash, cloneString(row[3]), coordsInstance);
    }
verbose(1, "%d elements in snpHash\n", count);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

char *getAllele(int allele_id)
/* simple lookup, can return NULL */
{
char *ret = NULL;
char charId[8]; 

safef(charId, sizeof(charId), "%d", allele_id);
ret = (char *)hashLookup(alleleHash, charId);
return ret;
}


void readFreq()
{
FILE *outputFileHandle = mustOpen("snpFreq.tab", "w");
FILE *logFileHandle = mustOpen("snpFreq.log", "w");
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *alleleHashEl = NULL;
struct hashEl *snpHashEl = NULL;
struct coords *coordsInstance = NULL;
char snpName[32];
int bin = 0;

sqlSafef(query, sizeof(query), "select snp_id, allele_id, freq from SNPAlleleFreq");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    alleleHashEl = hashLookup(alleleHash, row[1]);
    if (alleleHashEl == NULL)
        {
        fprintf(logFileHandle, "couldn't find allele_id %s\n", row[1]);
	continue;
	}
    safef(snpName, sizeof(snpName), "rs%s", row[0]);
    snpHashEl = hashLookup(snpHash, snpName);
    if (snpHashEl == NULL)
        {
	fprintf(logFileHandle, "skipping snp_id %s\n", row[0]);
	continue;
	}
    coordsInstance = (struct coords *)snpHashEl->val;
    /* could add bin here */
    bin = hFindBin(coordsInstance->start, coordsInstance->end);
    fprintf(outputFileHandle, "%d\t%s\t%d\t%d\t%s\t%s\t%f\n",
            bin, coordsInstance->chrom, coordsInstance->start, coordsInstance->end,
	    snpName, (char *)alleleHashEl->val, sqlFloat(row[2]));
    }
carefulClose(&outputFileHandle);
carefulClose(&logFileHandle);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


int main(int argc, char *argv[])
{
if (argc != 3)
    usage();

snpDb = argv[1];
snpTable = argv[2];

hSetDb(snpDb);
createAlleleHash();
createSnpHash(snpTable);
readFreq();
 
// freeHash(&alleleHash);
return 0;
}
