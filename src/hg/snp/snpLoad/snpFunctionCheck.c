/* snpFunctionCheck - compare ContigLocusIdFilter.mrna_acc to refGenes. */
/* First check for entries in ContigLocusIdFilter that we don't use. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "hash.h"
#include "hdb.h"


static char *snpDb = NULL;

static struct hash *contigHash = NULL;
static struct hash *geneHash = NULL;
static struct hash *missingHash = NULL;

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
    "snpFunctionCheck - compare ContigLocusIdFilter.mrna_acc to refGenes. "
    "usage:\n"
    "    snpFunctionCheck snpDb \n");
}

void getGeneHash()
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

geneHash = newHash(0);
sqlSafef(query, sizeof(query), "select distinct(name) from refGene");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(geneHash, cloneString(row[0]), NULL);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void getContigHash()
/* only store snp_id/mrna_acc pairs if recognized mrna_acc */
{
char query[512];
char name[128];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *hel = NULL;
int missingCount = 0;
int validCount = 0;

contigHash = newHash(0);
missingHash = newHash(0);
sqlSafef(query, sizeof(query), "select snp_id, mrna_acc from ContigLocusIdFilter where mrna_acc != ''");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hel = hashLookup(geneHash, row[1]);
    if (hel == NULL)
        {
	hel = hashLookup(missingHash, row[1]);
	if (hel == NULL)
	    {
            fprintf(outputFileHandle, "%s not found in refGene\n", row[1]);
	    hashAdd(missingHash, cloneString(row[1]), NULL);
	    missingCount++;
	    }
	}
    else
        {
	safef(name, sizeof(name), "rs%s", row[0]);
        hashAdd(contigHash, name, cloneString(row[1]));
	validCount++;
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "%d distinct mrna_acc not found in refGene\n", missingCount);
verbose(1, "%d distinct valid mrna_acc found in refGene\n", validCount);
}


struct slName *readSnps(char *chromName, int start, int end)
/* change to save coords of snps */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct slName *el = NULL;
struct slName *ret = NULL;

sqlSafef(query, sizeof(query), "select name from snp125 where chrom = '%s' and chromStart >= %d and chromEnd <= %d", 
                             chromName, start, end);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = slNameNew(row[0]);
    slAddHead(&ret, el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

void scanGenes(char *chromName)
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *hel = NULL;
struct slName *snpPtr = NULL;
struct slName *snpList = NULL;
char *geneName = NULL;

sqlSafef(query, sizeof(query), "select name, txStart, txEnd from refGene where chrom = '%s'", chromName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    geneName = cloneString(row[0]);
    snpList = readSnps(chromName, sqlUnsigned(row[1]), sqlUnsigned(row[2]));
    for (snpPtr = snpList; snpPtr != NULL; snpPtr = snpPtr->next)
        {
        hel = hashLookup(contigHash, snpPtr->name);
	if (hel == NULL)
	    verbose(1, "no function for %s\n", snpPtr->name);
	else if (!sameString(hel->val, geneName))
	    verbose(1, "mismatch for %s: refGene = %s, mrna_acc = %s\n", snpPtr->name, geneName, hel->val);
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}



int main(int argc, char *argv[])
{
struct slName *chromList, *chromPtr;

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);

outputFileHandle = mustOpen("snpCheckFunction.out", "w");
getGeneHash();
getContigHash();
chromList = hAllChromNamesDb(snpDb);

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    scanGenes(chromPtr->name);

carefulClose(&outputFileHandle);

return 0;
}
