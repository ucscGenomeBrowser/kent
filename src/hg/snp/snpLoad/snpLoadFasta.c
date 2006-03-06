/* snpLoadFasta - Read fasta files with flank sequences.   Load into database.  */

/* Check for:

   "SingleClassQuadAllelic" and "SingleClassTriAllelic" 
   "SingleClassWrongObserved" 
   "IndelClassTruncatedObserved"
   "IndelClassObservedWrongFormat" 
   "MixedClassTruncatedObserved"
   "MixedClassObservedWrongFormat" 
   "NamedClassObservedWrongFormat" 

*/

/* Should be checking for duplicates here. */

#include "common.h"

#include "dystring.h"
#include "hdb.h"
#include "linefile.h"

static char const rcsid[] = "$Id: snpLoadFasta.c,v 1.16 2006/03/06 23:46:23 heather Exp $";

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
FILE *exceptionFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "snpLoadFasta - Read SNP fasta files and load into database.\n"
  "usage:\n"
  "  snpLoadFasta database \n");
}

boolean triAllelic(char *observed)
{
if (sameString(observed, "A/C/G")) return TRUE;
if (sameString(observed, "A/C/T")) return TRUE;
if (sameString(observed, "A/G/T")) return TRUE;
if (sameString(observed, "C/G/T")) return TRUE;
return FALSE;
}

boolean quadAllelic(char *observed)
{
if (sameString(observed, "A/C/G/T")) return TRUE;
return FALSE;
}

boolean validSingleObserved(char *observed)
{
if (sameString(observed, "A/C")) return TRUE;
if (sameString(observed, "A/G")) return TRUE;
if (sameString(observed, "A/T")) return TRUE;
if (sameString(observed, "C/G")) return TRUE;
if (sameString(observed, "C/T")) return TRUE;
if (sameString(observed, "G/T")) return TRUE;
return FALSE;
}

void checkSingleObserved(char *chromName, char *rsId, char *observed)
/* check for exceptions in single class */
/* this is not full exceptions format */
{
if (quadAllelic(observed))
    {
    fprintf(exceptionFileHandle, "chr%s\t%s\t%s\t%s\n", chromName, rsId, "SingleClassQuadAllelic", observed);
    return;
    }

if (triAllelic(observed))
    {
    fprintf(exceptionFileHandle, "chr%s\t%s\t%s\t%s\n", chromName, rsId, "SingleClassTriAllelic", observed);
    return;
    }

if (validSingleObserved(observed)) return;
fprintf(exceptionFileHandle, "chr%s\t%s\t%s\t%s\n", chromName, rsId, "SingleClassWrongObserved", observed);
}

void checkIndelObserved(char *chromName, char *rsId, char *observed)
/* Check for exceptions in in-del class. */
/* lengthTooLong: this is temporary.  Can get from full read of rs_fasta file. */
/* First char should be dash, second char should be forward slash. */
/* To do: no IUPAC */
{
int slashCount = 0;

if (sameString(observed, "lengthTooLong"))
    {
    fprintf(exceptionFileHandle, "chr%s\t%s\t%s\n", chromName, rsId, "IndelClassMissingObserved");
    return;
    }

if (strlen(observed) < 2)
    {
    fprintf(exceptionFileHandle, "chr%s\t%s\t%s\n", chromName, rsId, "IndelClassTruncatedObserved");
    return;
    }

slashCount = chopString(observed, "/", NULL, 0);
if (slashCount > 1)
    fprintf(exceptionFileHandle, "chr%s\t%s\t%s\t%s\n", chromName, rsId, "IndelClassObservedWrongFormat", observed);

if (observed[0] != '-' || observed[1] != '/')
    fprintf(exceptionFileHandle, "chr%s\t%s\t%s\t%s\n", chromName, rsId, "IndelClassObservedWrongFormat", observed);
}

void checkMixedObserved(char *chromName, char *rsId, char *observed)
/* Check for exceptions in mixed class. */
/* should be multi-allelic */
/* lengthTooLong: this is temporary.  Can get from full read of rs_fasta file. */
/* To do: no IUPAC */
{
int slashCount = 0;

if (sameString(observed, "lengthTooLong"))
    {
    fprintf(exceptionFileHandle, "chr%s\t%s\t%s\n", chromName, rsId, "MixedClassMissingObserved");
    return;
    }

if (strlen(observed) < 2)
    {
    fprintf(exceptionFileHandle, "chr%s\t%s\t%s\n", chromName, rsId, "MixedClassTruncatedObserved");
    return;
    }

if (observed[0] != '-' || observed[1] != '/')
    {
    fprintf(exceptionFileHandle, "chr%s\t%s\t%s\t%s\n", chromName, rsId, "MixedClassObservedWrongFormat", observed);
    return;
    }

slashCount = chopString(observed, "/", NULL, 0);
if (slashCount < 2)
    fprintf(exceptionFileHandle, "chr%s\t%s\t%s\t%s\n", chromName, rsId, "MixedClassObservedWrongFormat", observed);

}

void checkNamedObserved(char *chromName, char *rsId, char *observed)
/* Check for exceptions in named class. */
/* lengthTooLong: this is temporary.  Can get from full read of rs_fasta file. */
/* Should be (name). */
{
if (sameString(observed, "lengthTooLong"))
    {
    fprintf(exceptionFileHandle, "chr%s\t%s\t%s\n", chromName, rsId, "NamedClassMissingObserved");
    return;
    }

if (observed[0] != '(')
    fprintf(exceptionFileHandle, "chr%s\t%s\t%s\t%s\n", chromName, rsId, "NamedClassObservedWrongFormat", observed);
}

void checkMicrosatObserved(char *chromName, char *rsId, char *observed)
/* Check for exceptions in microsat class. */
/* lengthTooLong: this is temporary.  Can get from full read of rs_fasta file. */
/* This is bare minimum check for now. */
{
if (sameString(observed, "lengthTooLong"))
    {
    fprintf(exceptionFileHandle, "chr%s\t%s\t%s\n", chromName, rsId, "MicrosatClassMissingObserved");
    return;
    }
}


boolean getDataFromFasta(char *chromName)
/* Parse each line in chrN.gnl, write to chrN_snpFasta.tab. */
{
char inputFileName[64], outputFileName[64];
struct lineFile *lf;
FILE *f;
char *line;
int lineSize;
char *chopAtRs;
char *chopAtMolType;
char *chopAtAllele;
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

    fprintf(f, "%s\t%s\t%s\t%s\n", rsId[0], molType[1], classStrings[classVal], allele[1]);
    if (classVal == 1)
	checkSingleObserved(chromName, rsId[0], allele[1]);
    if (classVal == 2)
	checkIndelObserved(chromName, rsId[0], allele[1]);
    if (classVal == 7)
        checkMixedObserved(chromName, rsId[0], allele[1]);
    if (classVal == 5)
        checkNamedObserved(chromName, rsId[0], allele[1]);
    if (classVal == 4)
        checkMicrosatObserved(chromName, rsId[0], allele[1]);
    }
carefulClose(&f);
// close the lineFile pointer?
return TRUE;
}

void createTable(char *chromName)
/* create a chrN_snpFasta table */
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
dyStringPrintf(dy, createString, tableName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
hFreeConn(&conn);
}

void addIndex(char *chromName)
/* add the index after table is created */
{
struct sqlConnection *conn = hAllocConn();
char tableName[64];
char *alterString = "ALTER TABLE %s add index rsId(rsId(12))";
struct dyString *dy = newDyString(512);

safef (tableName, ArraySize(tableName), "chr%s_snpFasta", chromName);
dyStringPrintf(dy, alterString, tableName);
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

exceptionFileHandle = mustOpen("snpLoadFasta.exceptions", "w");

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

carefulClose(&exceptionFileHandle);
return 0;
}
