/* cnpLookup - lookup BACs from cnpSharp and cnpIafrate. */
/* start with temporary CNP table */
/* log if chrom changes */
/* if BAC is missing, output to lift file */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


#include "common.h"

#include "hash.h"
#include "hdb.h"


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
    "cnpLookup - get coords from BAC ends\n"
    "usage:\n"
    "    cnpLookup db bacTable cnpTable outputFile liftFile logFile\n");
}


struct hash *storeBacs(char *tableName)
/* store BAC coords in a hash */
{
struct hash *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct coords *cel = NULL;

verbose(1, "creating BAC hash...\n");
ret = newHash(0);
sqlSafef(query, sizeof(query), 
      "select name, chrom, chromStart, chromEnd from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(cel);
    cel->chrom = cloneString(row[1]);
    cel->start = sqlUnsigned(row[2]);
    cel->end = sqlUnsigned(row[3]);
    hashAdd(ret, cloneString(row[0]), cel);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}



void processCnps(struct hash *bacHash, char *cnpTable, char *fileName, char *liftFileName, char *logFileName)
/* read cnpTable */
/* lookup position in bacHash */
/* log change in chrom */
/* write to lift table if missing from bacHash */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *hel = NULL;
struct coords *cel = NULL;
FILE *fileHandle = mustOpen(fileName, "w");
FILE *logFileHandle = mustOpen(logFileName, "w");
FILE *liftFileHandle = mustOpen(liftFileName, "w");
char *cnpName = NULL;
char variantSignal;

verbose(1, "process CNPs...\n");
sqlSafef(query, sizeof(query), "select * from %s", cnpTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    cnpName = cloneString(row[4]);
    variantSignal = lastChar(cnpName);
    if (variantSignal == '*')
       stripChar(cnpName, '*');
    if (variantSignal == '?')
       stripChar(cnpName, '?');
    if (variantSignal == '#')
        stripChar(cnpName, '#');

    hel = hashLookup(bacHash, cnpName);
    if (hel == NULL) 
        {
	fprintf(liftFileHandle, "%s\t%s\t%s\t%s\t", row[1], row[2], row[3], row[4]);
	fprintf(liftFileHandle, "%s\t%s\t%s\t%s\t%s\t", row[5], row[6], row[7], row[8], row[9]);
	fprintf(liftFileHandle, "%s\t%s\t%s\t%s\t%s\n", row[10], row[11], row[12], row[13], row[14]);
	continue;
	}
    cel = (struct coords *)hel->val;

    if (!sameString(cel->chrom, row[1]))
	fprintf(logFileHandle, "chrom mismatch for %s: cnp on %s, BAC on %s\n", row[4], row[1], cel->chrom);

    fprintf(fileHandle, "%s\t%d\t%d\t%s\t", cel->chrom, cel->start, cel->end, row[4]);
    fprintf(fileHandle, "%s\t%s\t%s\t%s\t%s\t", row[5], row[6], row[7], row[8], row[9]);
    fprintf(fileHandle, "%s\t%s\t%s\t%s\t%s\n", row[10], row[11], row[12], row[13], row[14]);
    }

carefulClose(&fileHandle);
carefulClose(&logFileHandle);
carefulClose(&liftFileHandle);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


int main(int argc, char *argv[])
{
char *db = NULL;
char *bacTableName = NULL;
char *cnpTableName = NULL;
char fileName[64];
char liftFileName[64];
char logFileName[64];
struct hash *bacHash = NULL;

if (argc != 7)
    usage();

db = argv[1];
bacTableName = argv[2];
cnpTableName = argv[3];
safef(fileName, ArraySize(fileName), "%s", argv[4]);
safef(liftFileName, ArraySize(liftFileName), "%s", argv[5]);
safef(logFileName, ArraySize(logFileName), "%s", argv[6]);

/* process args */
hSetDb(db);
if (!hTableExists(bacTableName))
    errAbort("no %s table in %s\n", bacTableName, db);
if (!hTableExists(cnpTableName))
    errAbort("no %s table in %s\n", cnpTableName, db);

bacHash = storeBacs(bacTableName);
processCnps(bacHash, cnpTableName, fileName, liftFileName, logFileName);

return 0;
}
