/* snpContigLocusIdCondense 
 * Condense the ContigLocusIdFilter table to contain one row per snp_id,
   with unique, comma separated fxn_class values. */
#include "common.h"

#include "dystring.h"
#include "hash.h"
#include "hdb.h"

char *functionStrings[] = {
/* From snpFixed.SnpFunctionCode. */
"unknown",
"locus",
"coding",
"coding-synon",
"coding-nonsynon",
"untranslated",
"intron",
"splice-site",
"cds-reference",
};

boolean functionFound[ArraySize(functionStrings)];

static char const rcsid[] = "$Id: snpContigLocusIdCondense.c,v 1.2 2006/02/20 19:53:18 heather Exp $";

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
for (i=0; i<ArraySize(functionStrings); i++)
    if (functionFound[i])
        {
        if (!first) fprintf(f, ",");
        fprintf(f, "%d", i);
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
struct dyString *functionList = newDyString(255);
char *currentSnp = NULL;
int len = 0;
char *finalString = NULL;

f = hgCreateTabFile(".", "ContigLocusIdCondense");

safef(query, sizeof(query), "select snp_id, fxn_class from ContigLocusIdFilter");

initArray();
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (currentSnp == NULL) 
        currentSnp = cloneString(row[0]);
    if (!sameString(row[0], currentSnp))
        {
	fprintf(f, "%s\t", currentSnp);
	printArray(f);
	initArray();
	currentSnp = cloneString(row[0]);
	}
    functionFound[sqlUnsigned(row[1])] = TRUE;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&f);
}


void createTable()
/* create a ContigLocusIdCondense table */
{
struct sqlConnection *conn = hAllocConn();
char *createString =
"CREATE TABLE ContigLocusIdCondense (\n"
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
