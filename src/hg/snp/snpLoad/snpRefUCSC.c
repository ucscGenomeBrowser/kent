/* snpRefUCSC - sixth step in dbSNP processing.
 * Read the nib file for a chrom into memory.
 * Lookup sequence at coordinates. 
 * Rewrite to new chrN_snpTmp tables.  
 * Rename chrMT_snpTmp to chrM_snpTmp before running this.
 * Use UCSC chromInfo.  */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* Check for coords larger than chromSize; log as errors. */

#include "common.h"

#include "chromInfo.h"
#include "dystring.h"
#include "hash.h"
#include "hdb.h"

/* errAbort if larger SNP found */
#define MAX_SNP_SIZE 1024


static char *snpDb = NULL;
static struct hash *chromHash = NULL;
FILE *errorFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpRefUCSC - read nib files\n"
    "usage:\n"
    "    snpRefUCSC snpDb\n");
}


struct hash *loadChroms()
/* hash from UCSC chromInfo */
{
struct hash *ret;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct chromInfo *el;
char tableName[64];

ret = newHash(0);
sqlSafef(query, sizeof(query), "select chrom, size from chromInfo");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    safef(tableName, ArraySize(tableName), "%s_snpTmp", row[0]);
    if (!hTableExists(tableName)) continue;
    el = chromInfoLoad(row);
    hashAdd(ret, el->chrom, (void *)(& el->size));
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

unsigned getChromSize(char *chrom)
/* Return size of chrom.  */
{
struct hashEl *el = hashLookup(chromHash,chrom);

if (el == NULL)
    errAbort("Couldn't find size of chrom %s", chrom);
return *(unsigned *)el->val;
}


void getRefUCSC(char *chromName)
/* get sequence for each row */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];
char fileName[64];
char nibName[HDB_MAX_PATH_STRING];
FILE *f;
struct dnaSeq *seq;
char *seqPtr = NULL;
int start = 0;
int end = 0;
int chromSize = 0;
char refUCSC[MAX_SNP_SIZE];
int pos = 0;
int snpSize = 0;

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
if (!hTableExists(tableName)) return;
safef(fileName, ArraySize(fileName), "%s_snpTmp.tab", chromName);
chromSize = getChromSize(chromName);

f = mustOpen(fileName, "w");
hNibForChrom(chromName, nibName);
seq = hFetchSeq(nibName, chromName, 0, chromSize-1);
touppers(seq->dna);
seqPtr = seq->dna;

sqlSafef(query, sizeof(query), 
    "select snp_id, ctg_id, chromStart, chromEnd, loc_type, orientation, allele, weight from %s", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    start = sqlUnsigned(row[2]);
    end = sqlUnsigned(row[3]);
    if (start > chromSize)
        {
        fprintf(errorFileHandle, "rs%s at %s:%d-%d; chromSize=%d\n", row[0], chromName, start, end, chromSize);
	continue;
	}

    snpSize = end - start;
    if (snpSize > MAX_SNP_SIZE)
	errAbort("maximum size exceeded %s, %s:%d-%d\n", row[0], chromName, start, end);
    if (start == end)
        {
	/* copy whatever convention dbSNP uses for insertion */
        fprintf(f, "%s\t%s\t%d\t%d\t%s\t%s\t%s\t%s\t%s\t%s\n", 
                row[0], row[1], start, end, row[4], row[5], row[6], row[6], row[6], row[7]);
	continue;
	}
    verbose(5, "building refUCSC for rs%s at %s:%d-%d\n", row[0], chromName, start, end);
    // could potentially use memcpy here
    for (pos = start; pos < end; pos++)
        refUCSC[pos - start] = seqPtr[pos];
    refUCSC[snpSize] = '\0';
    verbose(5, "refUCSC = %s\n", refUCSC);
    
    fprintf(f, "%s\t%s\t%d\t%d\t%s\t%s\t%s\t%s\t", row[0], row[1], start, end, row[4], row[5], row[6], refUCSC);
    reverseComplement(refUCSC, snpSize);
    fprintf(f, "%s\t%s\n", refUCSC, row[7]);

    }
sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&f);
}




void recreateDatabaseTable(char *chromName)
/* create a new chrN_snpTmp table with new definition */
/* could use enum for loc_type here */
{
struct sqlConnection *conn = hAllocConn();
char tableName[64];
char *createString =
"CREATE TABLE %s (\n"
"    snp_id int(11) not null,\n"
"    ctg_id int(11) not null,\n"
"    chromStart int(11) not null,\n"
"    chromEnd int(11) not null,\n"
"    loc_type tinyint(4) not null,\n"
"    orientation tinyint(4) not null,\n"
"    allele blob,\n"
"    refUCSC blob,\n"
"    refUCSCReverseComp blob,\n"
"    weight int\n"
");\n";

struct dyString *dy = newDyString(1024);

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
sqlDyStringPrintf(dy, createString, tableName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
hFreeConn(&conn);
}


void loadDatabase(char *chromName)
/* load one table into database */
{
FILE *f;
struct sqlConnection *conn = hAllocConn();
char tableName[64], fileName[64];

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
safef(fileName, ArraySize(fileName), "%s_snpTmp.tab", chromName);

f = mustOpen(fileName, "r");
hgLoadTabFile(conn, ".", tableName, &f);

hFreeConn(&conn);
}


int main(int argc, char *argv[])
/* read chrN_snpTmp, look up sequence, rewrite to individual chrom tables */
{
struct hashCookie cookie;
char *chromName;
char tableName[64];

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

errorFileHandle = mustOpen("snpRefUCSC.errors", "w");

cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    {
    safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
    if (!hTableExists(tableName)) continue;
    verbose(1, "chrom = %s\n", chromName);
    getRefUCSC(chromName);
    recreateDatabaseTable(chromName);
    loadDatabase(chromName);
    }

carefulClose(&errorFileHandle);
return 0;
}
