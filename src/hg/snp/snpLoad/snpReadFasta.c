/* snpReadFasta - seventh step in dbSNP processing.
 * Hash the snpFasta contents (by chrom, and also chrMulti).
 * Serially read the chrN_snpTmp tables.
 * Do lookups into snpFasta hashes for class, observed, molType. 
 * Use UCSC chromInfo. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* Could drop refUCSCReverseComp here. */
/* Dropping ctg_id at this point. */

/* Look in chrMulti_snpFasta if no match found in chrN_snpFasta. */
/* Write to error file if no match found in chrN_snpFasta or chrMulti_snpFasta. */
/* Also write to error file if matches found in chrN_snpFasta AND chrMulti_snpFasta. */
/* Give preference to chrMulti_snpFasta data. (Don't bother checking for difference). */

#include "common.h"

#include "hash.h"
#include "hdb.h"


struct snpFasta
    {
    /* I don't think I need next */
    struct snpFasta *next;
    char *molType;
    char *class;
    char *observed;
    };

static struct hash *multiFastaHash = NULL;
static struct hash *chromFastaHash = NULL;

static char *snpDb = NULL;
FILE *errorFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpReadFasta - lookup molType, class and observed in chrN_snpFasta and chrMulti_snpFasta\n"
    "check for errors in observed\n"
    "recreate chrN_snpTmp\n"
    "usage:\n"
    "    snpReadFasta snpDb \n");
}


struct hash *readFasta(char *chromName)
/* read contents of chrN_fasta into hash. */
/* Read again for random */
/* Also called to create the chrN_multi hash. */
{
struct snpFasta *fel;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char fastaTableName[64];
struct hash *newChromHash = NULL;
char *adjustedChromName = cloneString(chromName);
char *randomSubstring = NULL;

newChromHash = newHash(0);

randomSubstring = strstr(chromName, "random");
if (randomSubstring != NULL) 
    stripString(adjustedChromName, "_random");

safef(fastaTableName, ArraySize(fastaTableName), "%s_snpFasta", adjustedChromName);
if(!hTableExistsDb(snpDb, fastaTableName)) 
    errAbort("can't get table %s\n", fastaTableName);

sqlSafef(query, sizeof(query), "select rsId, molType, class, observed from %s", fastaTableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(fel);
    fel->molType = cloneString(row[1]);
    fel->class = cloneString(row[2]);
    fel->observed = cloneString(row[3]);
    /* could check for duplicates here */
    hashAdd(newChromHash, cloneString(row[0]), fel);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);

return newChromHash;
}

struct snpFasta *getFastaElement(char *snp_id, char *chromName)
/* look first in the chromFastaHash */
/* also look in the multiFastaHash */
{
struct snpFasta *felChrom = NULL;
struct snpFasta *felMulti = NULL;
char snpName[32];

safef(snpName, sizeof(snpName), "rs%s", snp_id);
felChrom = hashFindVal(chromFastaHash, snpName);
felMulti = hashFindVal(multiFastaHash, snpName);

if (felChrom == NULL && felMulti == NULL)
    {
    fprintf(errorFileHandle, "no snpFasta for %s in %s\n", snpName, chromName);
    return NULL;
    }
    
if (felChrom != NULL && felMulti != NULL)
    {
    fprintf(errorFileHandle, "duplicate snpFasta for %s in %s\n", snpName, chromName);
    return felMulti;
    }

if (felChrom != NULL) 
    return felChrom;

return felMulti;

}

void processSnps(char *chromName)
/* read through all rows in snpTmp */
/* look up molType/class/observed */
/* write to output file */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];
char fileName[64];
char *classString = NULL;
FILE *f;
struct snpFasta *fel = NULL;
int loc_type = 0;

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
safef(fileName, ArraySize(fileName), "%s_snpTmp.tab", chromName);
f = mustOpen(fileName, "w");

sqlSafef(query, sizeof(query), 
     "select snp_id, chromStart, chromEnd, loc_type, orientation, allele, refUCSC, refUCSCReverseComp, weight from %s ", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    loc_type = sqlUnsigned(row[3]);
    fel = getFastaElement(row[0], chromName);
    if (fel == NULL)
        {
        fprintf(f, "%s\t%s\t%s\t%d\t", row[0], row[1], row[2], loc_type);
	fprintf(f, "%s\t%s\t%s\t%s\t", "unknown", row[4], "unknown", row[5]);
	fprintf(f, "%s\t%s\t%s\t%s\n", row[6], row[7], "n/a", row[8]);
	continue;
	}
    classString = fel->class;
    /* special handling for class = in-del; split into classes of our own construction */
    if (sameString(classString, "in-del"))
        {
        if (loc_type == 3) 
	    classString = cloneString("insertion");
	if (loc_type == 1 || loc_type == 2) 
	    classString = cloneString("deletion");
	}
    fprintf(f, "%s\t%s\t%s\t%d\t", row[0], row[1], row[2], loc_type);
    fprintf(f, "%s\t%s\t%s\t%s\t", classString, row[4], fel->molType, row[5]);
    fprintf(f, "%s\t%s\t%s\t%s\n", row[6], row[7], fel->observed, row[8]);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&f);
}


void recreateDatabaseTable(char *chromName)
/* create a new chrN_snpTmp table with new definition */
{
struct sqlConnection *conn = hAllocConn();
char tableName[64];
char *createString =
"CREATE TABLE %s (\n"
"    snp_id int(11) not null,\n"
"    chromStart int(11) not null,\n"
"    chromEnd int(11) not null,\n"
"    loc_type tinyint(4) not null,\n"
"    class varchar(255) not null,\n"
"    orientation tinyint(4) not null,\n"
"    molType varchar(255) not null,\n"
"    allele blob,\n"
"    refUCSC blob,\n"
"    refUCSCReverseComp blob,\n"
"    observed blob,\n"
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
/* hgLoadTabFile closes the file handle */
hgLoadTabFile(conn, ".", tableName, &f);

hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* hash snpFasta, read through chrN_snpTmp, rewrite with extensions to individual chrom tables */
{
struct slName *chromList, *chromPtr;
char tableName[64];

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);
chromList = hAllChromNamesDb(snpDb);

errorFileHandle = mustOpen("snpReadFasta.errors", "w");

multiFastaHash = readFasta("chrMulti");

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    safef(tableName, ArraySize(tableName), "%s_snpTmp", chromPtr->name);
    if (!hTableExists(tableName)) continue;
    verbose(1, "chrom = %s\n", chromPtr->name);
    chromFastaHash = readFasta(chromPtr->name);
    processSnps(chromPtr->name);
    }

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    safef(tableName, ArraySize(tableName), "%s_snpTmp", chromPtr->name);
    if (!hTableExists(tableName)) continue;
    recreateDatabaseTable(chromPtr->name);
    verbose(1, "loading chrom = %s\n", chromPtr->name);
    loadDatabase(chromPtr->name);
    }

carefulClose(&errorFileHandle);

return 0;
}
