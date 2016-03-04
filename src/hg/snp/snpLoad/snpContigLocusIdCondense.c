/* snpContigLocusIdCondense 
 * Condense the ContigLocusIdFilter table to contain one row per snp_id,
   with unique, comma separated fxn_class values. */
/* This assumes that the SNPs are in order!!! 
   errAbort if this is violated. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"

#include "hdb.h"

char *functionStrings[] = {
/* From snpFixed.SnpFunctionCode. */
"unknown",
"locus",
"coding", // not used
"coding-synon",
"coding-nonsynon",
"untranslated",
"intron",
"splice-site",
// "cds-reference", not useful
};

boolean functionFound[ArraySize(functionStrings)];


static char *snpDb = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpContigLocusIdCondense - condense the ContigLocusIdFilter table to contain one row per snp_id\n"
    "usage:\n"
    "    snpContigLocusIdCondense snpDb\n");
}

void initArray()
{
int i;
for (i=0; i<ArraySize(functionStrings); i++)
    functionFound[i] = FALSE;
}

void printArray(FILE *f)
{
int i;
boolean first = TRUE;
/* special case for UTR/intron */
// if (functionFound[5] && functionFound[6])
    // functionFound[5] = FALSE;
for (i=0; i<ArraySize(functionStrings); i++)
    if (functionFound[i])
        {
        if (!first) fprintf(f, ",");
        fprintf(f, "%s", functionStrings[i]);
	first = FALSE;
	}
fprintf(f, "\n");
}

void condenseFunctionValues()
/* combine function values for single snp */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
FILE *f;
char *currentSnpString = NULL;
int currentSnpNum = 0;
int functionIndex = 0;

f = hgCreateTabFile(".", "ContigLocusIdCondense");

sqlSafef(query, sizeof(query), "select snp_id, fxn_class from ContigLocusIdFilter");

initArray();
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    functionIndex = sqlUnsigned(row[1]);
    if (functionIndex == 8) continue;
    if (functionIndex > 8) 
        {
	verbose(1, "unexpected functionIndex %d\n", functionIndex);
	continue;
	}
    if (currentSnpString == NULL) 
        {
        currentSnpString = cloneString(row[0]);
	currentSnpNum = sqlUnsigned(row[0]);
	}
    if (!sameString(row[0], currentSnpString))
        {
	fprintf(f, "%s\t", currentSnpString);
	printArray(f);
	initArray();
	if (currentSnpNum > sqlUnsigned(row[0]))
	    errAbort("snps out of order: %d before %s\n", currentSnpNum, row[0]);
	currentSnpString = cloneString(row[0]);
	currentSnpNum = sqlUnsigned(row[0]);
	}
    functionFound[functionIndex] = TRUE;
    }
fprintf(f, "%s\t", currentSnpString);
printArray(f);
sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&f);
}


void createTable()
/* create a ContigLocusIdCondense table */
{
struct sqlConnection *conn = hAllocConn();
char *createString =
NOSQLINJ "CREATE TABLE ContigLocusIdCondense (\n"
"    snp_id int(11) not null,       \n"
"    fxn_class varchar(255) not null\n"
");\n";

sqlRemakeTable(conn, "ContigLocusIdCondense", createString);
}


void loadDatabase()
{
struct sqlConnection *conn = hAllocConn();
FILE *f = mustOpen("ContigLocusIdCondense.tab", "r");
hgLoadTabFile(conn, ".", "ContigLocusIdCondense", &f);
hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* Condense ContigLocusIdFilter and write to ContigLocusIdCondense. */
{

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);

/* check for needed tables */
if(!hTableExistsDb(snpDb, "ContigLocusIdFilter"))
    errAbort("no ContigLocusIdFilter table in %s\n", snpDb);


condenseFunctionValues();
createTable();
loadDatabase();

return 0;
}
