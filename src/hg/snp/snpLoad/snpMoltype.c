/* snpMoltype - tenth step in dbSNP processing.
 * Hash moltype from snpFasta (by chrom, and also chrMulti).
 * Serially read the chrN_snpTmp tables.
 * Do lookups into snpFasta hashes for molType. 
 * Use UCSC chromInfo. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* Look in chrMulti_snpFasta if no match found in chrN_snpFasta. */
/* Write to error file if no match found in chrN_snpFasta or chrMulti_snpFasta. */
/* Also write to error file if matches found in chrN_snpFasta AND chrMulti_snpFasta. */
/* Give preference to chrMulti_snpFasta data. (Don't bother checking for difference). */

#include "common.h"

#include "hash.h"
#include "hdb.h"


static struct hash *multiFastaHash = NULL;
static struct hash *chromFastaHash = NULL;

static char *snpDb = NULL;
FILE *errorFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpReadFasta - lookup molType in chrN_snpFasta and chrMulti_snpFasta\n"
    "recreate chrN_snpTmp\n"
    "usage:\n"
    "    snpReadFasta snpDb \n");
}


struct hash *readFasta(char *chromName)
/* read moltype from chrN_fasta into hash. */
/* Read again for random */
/* Also called to create the chrN_multi hash. */
{
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

sqlSafef(query, sizeof(query), "select rsId, molType from %s", fastaTableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    /* could check for duplicates here */
    hashAdd(newChromHash, cloneString(row[0]), cloneString(row[1]));

sqlFreeResult(&sr);
hFreeConn(&conn);

return newChromHash;
}

char *getMoltype(char *snp_id, char *chromName)
/* look first in the chromFastaHash */
/* also look in the multiFastaHash */
{
char *moltypeChrom = NULL;
char *moltypeMulti = NULL;
char snpName[32];

safef(snpName, sizeof(snpName), "rs%s", snp_id);
moltypeChrom = hashFindVal(chromFastaHash, snpName);
moltypeMulti = hashFindVal(multiFastaHash, snpName);

if (moltypeChrom == NULL && moltypeMulti == NULL)
    {
    fprintf(errorFileHandle, "no moltype for %s in %s\n", snpName, chromName);
    return NULL;
    }
    
if (moltypeChrom != NULL && moltypeMulti != NULL)
    {
    fprintf(errorFileHandle, "duplicate moltype for %s in %s\n", snpName, chromName);
    return moltypeMulti;
    }

if (moltypeChrom != NULL) 
    return moltypeChrom;

return moltypeMulti;

}

void processSnps(char *chromName)
/* read through all rows in snpTmp */
/* look up molType */
/* write to output file */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];
char fileName[64];
FILE *f;
char *molType = NULL;

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
safef(fileName, ArraySize(fileName), "%s_snpTmp.tab", chromName);
f = mustOpen(fileName, "w");

sqlSafef(query, sizeof(query), 
     "select snp_id, chromStart, chromEnd, loc_type, class, orientation, fxn_class, "
     "validation_status, avHet, avHetSE, allele, refUCSC, refUCSCReverseComp, observed, weight from %s ", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    molType = getMoltype(row[0], chromName);
    if (molType == NULL) 
        molType = cloneString("unknown");
    fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\t", row[0], row[1], row[2], row[3], row[4], row[5]);
    fprintf(f, "%s\t", molType);
    fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", row[6], row[7], row[8], row[9], row[10], row[11], row[12], row[13], row[14]);
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
"    fxn_class varchar(255) not null,\n"
"    validation_status tinyint(4) not null,\n"
"    avHet float not null, \n"
"    avHetSE float not null, \n"
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

errorFileHandle = mustOpen("snpMoltype.errors", "w");

multiFastaHash = readFasta("chrMulti");

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    safef(tableName, ArraySize(tableName), "%s_snpTmp", chromPtr->name);
    if (!hTableExists(tableName)) continue;
    verbose(1, "chrom = %s\n", chromPtr->name);
    chromFastaHash = readFasta(chromPtr->name);
    processSnps(chromPtr->name);
    }

carefulClose(&errorFileHandle);

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    safef(tableName, ArraySize(tableName), "%s_snpTmp", chromPtr->name);
    if (!hTableExists(tableName)) continue;
    recreateDatabaseTable(chromPtr->name);
    verbose(1, "loading chrom = %s\n", chromPtr->name);
    loadDatabase(chromPtr->name);
    }

return 0;
}
