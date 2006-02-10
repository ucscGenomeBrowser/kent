/* snpRefUCSC - sixth step in dbSNP processing.
 * Read the nib file for a chrom into memory.
 * Lookup sequence at coordinates. 
 * Enhancement: switch to using 2bit.
 * Rewrite to new chrN_snpTmp tables.  
 * Use UCSC chromInfo.  */


#include "common.h"

#include "chromInfo.h"
#include "dystring.h"
#include "hash.h"
#include "hdb.h"

#define MAX_SNP_SIZE 1024

static char const rcsid[] = "$Id: snpRefUCSC.c,v 1.3 2006/02/10 19:31:22 heather Exp $";

static char *snpDb = NULL;
static struct hash *chromHash = NULL;
FILE *errorFileHandle = NULL;
FILE *exceptionFileHandle = NULL;


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
    // verbose(1, "chrom = %s\n", row[0]);
    hashAdd(ret, el->chrom, (void *)(& el->size));
    // hashAdd(ret, cloneString(row[0]), sqlUnsigned(row[1]));
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
// do I need an include for this?
struct dnaSeq *seq;
char *seqPtr = NULL;
int start = 0;
int end = 0;
int chromSize = 0;
// figure out maximum SNP size
char refUCSC[MAX_SNP_SIZE];
int pos = 0;
int snpSize = 0;
// for short-circuit
int count = 0;

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
if (!hTableExists(tableName)) return;
verbose(3, "tableName = %s\n", tableName);
safef(fileName, ArraySize(fileName), "%s_snpTmp.tab", chromName);
verbose(3, "fileName = %s\n", fileName);
chromSize = getChromSize(chromName);
verbose(3, "chromSize = %d\n", chromSize);

f = mustOpen(fileName, "w");
hNibForChrom(chromName, nibName);
seq = hFetchSeq(nibName, chromName, 0, chromSize-1);
touppers(seq->dna);
seqPtr = seq->dna;

safef(query, sizeof(query), 
    "select snp_id, chromStart, chromEnd, loc_type, orientation, allele from %s", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    count++;
    // if (count == 100) return;
    start = atoi(row[1]);
    end = atoi(row[2]);
    snpSize = end - start;
    if (snpSize > MAX_SNP_SIZE)
        {
	verbose(1, "maximum size exceeded %s, %s:%d-%d\n", row[0], chromName, start, end);
	continue;
	}
    if (start == end)
        {
	/* copy whatever convention dbSNP uses for insertion */
        fprintf(f, "%s\t%d\t%d\t%s\t%s\t%s\t%s\t%s\n", 
                row[0], start, end, row[3], row[4], row[5], row[5], row[5]);
	continue;
	}
    verbose(5, "building refUCSC for rs%s at %s:%d-%d\n", row[0], chromName, start, end);
    for (pos = start; pos < end; pos++)
        refUCSC[pos - start] = seqPtr[pos];
    refUCSC[snpSize] = '\0';
    verbose(5, "refUCSC = %s\n", refUCSC);
    
    fprintf(f, "%s\t%d\t%d\t%s\t%s\t%s\t%s\t", row[0], start, end, row[3], row[4], row[5], refUCSC);
    reverseComplement(refUCSC, snpSize);
    fprintf(f, "%s\n", refUCSC);

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
"    chromStart int(11) not null,\n"
"    chromEnd int(11) not null,\n"
"    loc_type tinyint(4) not null,\n"
"    orientation tinyint(4) not null,\n"
"    allele blob,\n"
"    refUCSC blob,\n"
"    refUCSCReverseComp blob\n"
");\n";

struct dyString *dy = newDyString(1024);

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
dyStringPrintf(dy, createString, tableName);
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

errorFileHandle = mustOpen("snpRefUCSC.errors", "w");
exceptionFileHandle = mustOpen("snpRefUCSC.exceptions", "w");

cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    {
    verbose(1, "chrom = %s\n", chromName);
    getRefUCSC(chromName);
    recreateDatabaseTable(chromName);
    loadDatabase(chromName);
    }

carefulClose(&errorFileHandle);
carefulClose(&exceptionFileHandle);
return 0;
}
