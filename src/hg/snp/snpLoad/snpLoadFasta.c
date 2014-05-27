/* snpLoadFasta - Read gnl header lines from fasta files. */
/* Create chrN_snpFasta tables. */
/* SNPs aligned to random chroms will be in chrN and/or chrMulti. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "dystring.h"
#include "hdb.h"
#include "linefile.h"


/* from snpFixed.SnpClassCode */
/* The vast majority are single. */
/* Expect about 500k in-del. */
/* Expect about 50k mixed. */
/* Expect about 5k named and microsatellite. */
char *classStrings[] = {
    "unknown",
    "single",
    "in-del",
    "het",
    "microsatellite",
    "named",
    "no var",
    "mixed",
    "mnp",
};

static char *database = NULL;
FILE *errorFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "snpLoadFasta - Read gnl header lines from SNP fasta files and load into database.\n"
  "usage:\n"
  "  snpLoadFasta database \n");
}



boolean getDataFromFasta(char *chromName)
/* Parse each line in chrN.gnl, write to chrN_snpFasta.tab. */
{
char inputFileName[64], outputFileName[64];
struct lineFile *lf;
FILE *f;
char *line;
int lineSize;
int wordCount9, wordCount2;
char *row[9], *rsId[2], *molType[2], *class[2], *allele[2];
int classVal = 0;

safef(inputFileName, ArraySize(inputFileName), "ch%s.gnl", chromName);
if (!fileExists(inputFileName)) return FALSE;

safef(outputFileName, ArraySize(outputFileName), "chr%s_snpFasta.tab", chromName);

lf = lineFileOpen(inputFileName, TRUE);
f = mustOpen(outputFileName, "w");

while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount9 = chopString(line, "|", row, ArraySize(row));
    wordCount2 = chopString(row[2], " ", rsId, ArraySize(rsId));
    wordCount2 = chopString(row[6], "=", molType, ArraySize(molType));
    wordCount2 = chopString(row[7], "=", class, ArraySize(class));
    wordCount2 = chopString(row[8], "=", allele, ArraySize(allele));

    stripChar(molType[1], '"');
    stripChar(allele[1], '"');
    classVal = sqlUnsigned(class[1]);

    if (sameString(allele[1], "lengthTooLong"))
        {
        fprintf(f, "%s\t%s\t%s\t%s\n", rsId[0], molType[1], classStrings[classVal], "unknown");
	fprintf(errorFileHandle, "missing observed for %s\n", rsId[0]);
	}
    else
        fprintf(f, "%s\t%s\t%s\t%s\n", rsId[0], molType[1], classStrings[classVal], allele[1]);
    }
carefulClose(&f);
// close the lineFile pointer?
return TRUE;
}

void createTable(char *chromName)
/* create a chrN_snpFasta table */
/* actually observed could also be varchar(255) */
{
struct sqlConnection *conn = hAllocConn();
char tableName[64];
char *createString =
"CREATE TABLE %s (\n"
"    rsId varchar(255) not null,\n"
"    molType varchar(255) not null,\n"
"    class varchar(255) not null,\n"
"    observed blob\n"
");\n";

struct dyString *dy = newDyString(1024);

safef(tableName, ArraySize(tableName), "chr%s_snpFasta", chromName);
sqlDyStringPrintf(dy, createString, tableName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
hFreeConn(&conn);
}

void addIndex(char *chromName)
/* add the index after table is created */
{
struct sqlConnection *conn = hAllocConn();
char tableName[64];
struct dyString *dy = newDyString(512);

safef (tableName, ArraySize(tableName), "chr%s_snpFasta", chromName);
sqlDyStringPrintf(dy, "ALTER TABLE %s add index rsId(rsId(12))", tableName);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
hFreeConn(&conn);
}


void loadDatabase(char *chromName)
/* load one table into database */
{
FILE *f;
struct sqlConnection *conn = hAllocConn();
char tableName[64], fileName[64];

safef(tableName, ArraySize(tableName), "chr%s_snpFasta", chromName);
safef(fileName, ArraySize(fileName), "chr%s_snpFasta.tab", chromName);

f = mustOpen(fileName, "r");
hgLoadTabFile(conn, ".", tableName, &f);

hFreeConn(&conn);
}


int main(int argc, char *argv[])
{
struct slName *chromList, *chromPtr;
char fileName[64];

if (argc != 2)
    usage();

database = argv[1];
hSetDb(database);
chromList = hAllChromNamesDb(database);

errorFileHandle = mustOpen("snpLoadFasta.error", "w");

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    stripString(chromPtr->name, "chr");
    safef(fileName, ArraySize(fileName), "ch%s.gnl", chromPtr->name);
    if (!fileExists(fileName)) continue;
    verbose(1, "chrom = %s\n", chromPtr->name);
    getDataFromFasta(chromPtr->name);
    createTable(chromPtr->name);
    loadDatabase(chromPtr->name);
    addIndex(chromPtr->name);
    }

getDataFromFasta("Multi");
createTable("Multi");
loadDatabase("Multi");
addIndex("Multi");

getDataFromFasta("Un");
createTable("Un");
loadDatabase("Un");
addIndex("Un");

carefulClose(&errorFileHandle);
return 0;
}
