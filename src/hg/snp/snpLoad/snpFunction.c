/* snpFunction: eighth step in dbSnp processing.
 * Add fxn_class values (can be more than one) to chrN_snpTmp.
 * Create a hash from ContigLocusIdCondense table. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"

#include "hash.h"
#include "hdb.h"


static char *snpDb = NULL;
static struct hash *functionHash = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpFunction - add function to chrN_snpTmp tables\n"
    "usage:\n"
    "    snpFunction snpDb \n");
}



struct hash *createFunctionHash()
{
struct hash *ret;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *snpString = NULL;
char *functionString = NULL;

verbose(1, "building function hash...\n");
ret = newHash(18);
sqlSafef(query, sizeof(query), "select snp_id, fxn_class from ContigLocusIdCondense");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    snpString = cloneString(row[0]);
    functionString = cloneString(row[1]);
    // hashAdd(ret, snpString, (void *)(&functionString));
    hashAdd(ret, snpString, functionString);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}


void addFunction(char *chromName)
/* Look up function for each row */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *el1;
FILE *f;
char fileName[64];
char tableName[64];

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
if (!hTableExists(tableName)) return;
safef(fileName, ArraySize(fileName), "%s_snpTmp.tab", chromName);

f = mustOpen(fileName, "w");

sqlSafef(query, sizeof(query),
    "select snp_id, chromStart, chromEnd, loc_type, class, orientation, "
    "allele, refUCSC, refUCSCReverseComp, observed, weight from %s", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\t", row[0], row[1], row[2], row[3], row[4], row[5]);
    el1 = hashLookup(functionHash,row[0]);
    if (el1 == NULL)
	fprintf(f, "unknown\t");
    else
        fprintf(f, "%s\t", (char *)el1->val);
    fprintf(f, "%s\t%s\t%s\t%s\t%s\n", row[6], row[7], row[8], row[9], row[10]);
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
"    fxn_class varchar(255) not null,\n"
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
hgLoadTabFile(conn, ".", tableName, &f);

hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* Read chromInfo into hash. */
/* Read ContigLocusIdCondense into hash. */
/* Recreate chrN_snpTmp, adding fxn_classes. */
{
struct slName *chromList, *chromPtr;
char tableName[64];

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);
chromList = hAllChromNamesDb(snpDb);

/* check for needed tables */
if(!hTableExistsDb(snpDb, "ContigLocusIdCondense"))
    errAbort("no ContigLocusIdCondense table in %s\n", snpDb);
if(!hTableExistsDb(snpDb, "chromInfo"))
    errAbort("no chromInfo table in %s\n", snpDb);

functionHash = createFunctionHash();

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    safef(tableName, ArraySize(tableName), "%s_snpTmp", chromPtr->name);
    if (!hTableExists(tableName)) continue;
    verbose(1, "chrom = %s\n", chromPtr->name);
    addFunction(chromPtr->name);
    }

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    safef(tableName, ArraySize(tableName), "%s_snpTmp", chromPtr->name);
    if (!hTableExists(tableName)) continue;
    verbose(1, "loading chrom = %s\n", chromPtr->name);
    recreateDatabaseTable(chromPtr->name);
    loadDatabase(chromPtr->name);
    }

return 0;
}
