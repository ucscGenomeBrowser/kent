/* subSnpCondense
 * Condense the SNPSubSNPLink table to contain one row per snp_id,
   with comma separated subsnp_id and build_id values. */
/* This assumes that the SNPs are in order!!! 
   errAbort if this is violated. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"

#include "hdb.h"
#include "dystring.h"


static char *snpDb = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "subSnpCondense - condense the SNpSubSNPLink table to contain one row per snp_id\n"
    "usage:\n"
    "    subSnpCondense snpDb\n");
}

void condenseValues()
/* combine values for single snp */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
FILE *f;
struct dyString *ssList = newDyString(255);
struct dyString *buildList = newDyString(255);
char *currentSnpString = NULL;
int currentSnpNum = 0;
int count = 0;
char firstBuild[32];
char lastBuild[32];

f = hgCreateTabFile(".", "SNPSubSNPLinkCondense");

sqlSafef(query, sizeof(query), "select snp_id, subsnp_id, build_id from SNPSubSNPLink");

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (currentSnpString == NULL) 
        {
        currentSnpString = cloneString(row[0]);
	currentSnpNum = sqlUnsigned(row[0]);
	dyStringPrintf(ssList, "%s", row[1]);
	dyStringPrintf(buildList, "%s", row[2]);
	safef(firstBuild, sizeof(firstBuild), row[2]);
	safef(lastBuild, sizeof(firstBuild), row[2]);
	}
    else if (!sameString(row[0], currentSnpString))
        {
	fprintf(f, "%s\t%s\t%s\t%s\t%s\t%d\n", 
	           currentSnpString, ssList->string, buildList->string, firstBuild, lastBuild, count);
	if (currentSnpNum > sqlUnsigned(row[0]))
	    errAbort("snps out of order: %d before %s\n", currentSnpNum, row[0]);
	currentSnpString = cloneString(row[0]);
	currentSnpNum = sqlUnsigned(row[0]);
	dyStringClear(ssList);
	dyStringPrintf(ssList, "%s", row[1]);
	dyStringClear(buildList);
	dyStringPrintf(buildList, "%s", row[2]);
	safef(firstBuild, sizeof(firstBuild), row[2]);
	safef(lastBuild, sizeof(lastBuild), row[2]);
	count = 1;
	}
    else
        {
	count++;
	dyStringAppend(ssList, ",");
	dyStringAppend(ssList, row[1]);
	dyStringAppend(buildList, ",");
	dyStringAppend(buildList, row[2]);
	safef(lastBuild, sizeof(lastBuild), row[2]);
	}
    }
fprintf(f, "%s\t%s\t%s\t%s\t%s\t%d\n", currentSnpString, ssList->string, buildList->string, 
                                       firstBuild, lastBuild, count);
sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&f);
}


void createTable()
/* create a SNPSubSNPLinkCondense table */
{
struct sqlConnection *conn = hAllocConn();
char *createString =
NOSQLINJ "CREATE TABLE SNPSubSNPLinkCondense (\n"
"    snp_id int(11) not null,       \n"
"    subsnpIds blob not null,\n"
"    buildIds blob not null,\n"
"    firstBuild int(11) not null,\n"
"    lastBuild int(11) not null,\n"
"    count int(4) not null\n"
");\n";

sqlRemakeTable(conn, "SNPSubSNPLinkCondense", createString);
}


void loadDatabase()
{
struct sqlConnection *conn = hAllocConn();
FILE *f = mustOpen("SNPSubSNPLinkCondense.tab", "r");
hgLoadTabFile(conn, ".", "SNPSubSNPLinkCondense", &f);
hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* Condense SNPSubSNPLinkand write to SNPSubSNPLinkCondense. */
{

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);

/* check for needed tables */
if(!hTableExistsDb(snpDb, "SNPSubSNPLink"))
    errAbort("no SNPSubSNPLink table in %s\n", snpDb);


condenseValues();
createTable();
loadDatabase();

return 0;
}
