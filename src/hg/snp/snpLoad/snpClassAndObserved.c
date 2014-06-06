/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* snpClassAndObserved -- new seventh step
 * Hash the UniVariation table, storing subsnp_class and var_str.
 * subsnp_class is a number from 0-8.
 * Log and skip rows where subsnp_class = 0 or subsnp_class > 8.
 * Should only be a few of these.
 * Hash the SNP table, storing univar_id as string.
 * Again, log and skip rows where univar_id = 0.
 * There are likely order(10K) of these.
 * Serially read the chrN_snpTmp tables.
 * Do lookups into snpHash and univarHash.
 * Use UCSC chromInfo. */

/* Could drop refUCSCReverseComp here. */
/* Dropping ctg_id at this point. */

#include "common.h"

#include "hash.h"
#include "hdb.h"


struct snpData
    {
    int classInt;
    char *observed;
    };

static struct hash *univarHash = NULL;
static struct hash *snpHash = NULL;

static char *snpDb = NULL;
FILE *errorFileHandle = NULL;

/* we will promote in-del to insertion if loc_type == 3 */
/* we will promote in-del to deletion if loc_type == 1 or loc_type == 2 */
/* we are hoping for no no-var and hardly any het */
char *classStrings[] = {
    "single",
    "in-del",
    "het",
    "microsatellite",
    "named",
    "no-var",
    "mixed",
    "mnp"
};

int classCount = sizeof(classStrings);

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpClassAndObserved - lookup class and observed in SNP and UniVariation tables\n"
    "check for errors in observed\n"
    "recreate chrN_snpTmp\n"
    "usage:\n"
    "    snpClassAndObserved snpDb \n");
}


struct hash *getUnivarHash()
/* read var_str and subsnp_class of UniVar into hash indexed by univar_id. */
{
struct snpData *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hash *ret = NULL;
int classInt = 0;
int rowCount = 0;
int skipCount = 0;

verbose(1, "create univarHash...\n");
ret = newHash(0);
sqlSafef(query, sizeof(query), "select univar_id, var_str, subsnp_class from UniVariation");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    rowCount++;
    classInt = sqlUnsigned(row[2]);
    if (classInt == 0)
        {
	fprintf(errorFileHandle, "subsnp_class = 0 (zero) for univar_id %s (skipping)\n", row[0]);
	skipCount++;
	continue;
	}
    if (classInt > classCount)
        {
	fprintf(errorFileHandle, "unexpected subsnp_class = %d for univar_id %s (skipping)\n", classInt, row[0]);
	skipCount++;
	continue;
	}
    AllocVar(el);
    el->observed = cloneString(row[1]);
    el->classInt = classInt;
    hashAdd(ret, cloneString(row[0]), el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "read %d rows in UniVariation\n", rowCount);
verbose(1, "skipped %d rows \n", skipCount);
return ret;
}

struct hash *getSNPHash()
/* Store univar_id from SNP into hash indexed by snp_id. */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hash *ret = NULL;
int id = 0;
int rowCount = 0;
int skipCount = 0;
struct hashEl *univarElement = NULL;
struct snpData *snpElementNew = NULL;
struct snpData *snpElementOld = NULL;

verbose(1, "creating snpHash...\n");
ret = newHash(16);
sqlSafef(query, sizeof(query), "select snp_id, univar_id from SNP");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    rowCount++;
    id = sqlUnsigned(row[1]);
    if (id == 0)
        {
	fprintf(errorFileHandle, "univar_id = 0 (zero) for %s (skipping)\n", row[0]);
	skipCount++;
	continue;
	}
    /* check that this univar_id is in univarHash */
    univarElement = hashLookup(univarHash, row[1]);
    if (univarElement == NULL)
        {
	fprintf(errorFileHandle, "univar_id = %s for %s missing from UniVariation table (leaving out of snpHash)\n", 
	        row[1], row[0]);
	skipCount++;
	continue;
	}
    /* clone */
    snpElementOld = (struct snpData *)univarElement->val;
    AllocVar(snpElementNew);
    snpElementNew->observed = cloneString(snpElementOld->observed);
    snpElementNew->classInt = snpElementOld->classInt;
    hashAdd(ret, cloneString(row[0]), snpElementNew);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "read %d rows in SNP\n", rowCount);
verbose(1, "skipped %d rows \n", skipCount);
return ret;
}

void processSnps(char *chromName)
/* read through all rows in snpTmp */
/* look up class and observed */
/* write to output file */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];
char fileName[64];
FILE *f;
struct hashEl *univarElement = NULL;
struct snpData *dataElement = NULL;
int classInt = 0;
char *classString = NULL;
int loc_type = 0;
int skipCount = 0;

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
safef(fileName, ArraySize(fileName), "%s_snpTmp.tab", chromName);
f = mustOpen(fileName, "w");

sqlSafef(query, sizeof(query), 
     "select snp_id, chromStart, chromEnd, loc_type, orientation, allele, refUCSC, refUCSCReverseComp, weight from %s ", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    loc_type = sqlUnsigned(row[3]);
    /* get univarElement from snpHash */
    univarElement = hashLookup(snpHash, row[0]);
    if (univarElement == NULL)
       {
        {
	fprintf(errorFileHandle, "no data for %s (dropping)\n", row[0]);
	skipCount++;
	continue;
	}
       }
    dataElement = (struct snpData *)univarElement->val;
    classInt = dataElement->classInt;
    // verbose(1, "classInt = %d\n", classInt);
    assert(classInt >= 1 && classInt <= classCount);
    /* lookup classString */
    classString = classStrings[classInt-1];
    /* special handling for class = in-del; split into classes of our own construction */
    if (sameString(classString, "in-del"))
        {
        if (loc_type == 3) 
	    classString = cloneString("insertion");
	if (loc_type == 1 || loc_type == 2) 
	    classString = cloneString("deletion");
	}
    fprintf(f, "%s\t%s\t%s\t%d\t%s\t", row[0], row[1], row[2], loc_type, classString);
    fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\n", row[4], row[5], row[6], row[7], dataElement->observed, row[8]);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&f);
if (skipCount > 0)
    verbose(1, "%d rows dropped\n", skipCount);
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
/* hash subsnp_class and var_str from UniVariation */
/* read SNP for univar_id, lookup into univarHash */
/* store univarHash elements in snpHash */
/* read through chrN_snpTmp, rewrite with extensions to individual chrom tables */
{
struct slName *chromList, *chromPtr;
char tableName[64];

if (argc != 2)
    usage();


snpDb = argv[1];
hSetDb(snpDb);

/* check for necessary tables */
if(!hTableExistsDb(snpDb, "SNP"))
    errAbort("missing SNP table");
if(!hTableExistsDb(snpDb, "UniVariation"))
    errAbort("missing UniVariation table");

chromList = hAllChromNamesDb(snpDb);

errorFileHandle = mustOpen("snpClassAndObserved.errors", "w");

univarHash = getUnivarHash();
snpHash = getSNPHash();

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    safef(tableName, ArraySize(tableName), "%s_snpTmp", chromPtr->name);
    if (!hTableExists(tableName)) continue;
    verbose(1, "chrom = %s\n", chromPtr->name);
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
