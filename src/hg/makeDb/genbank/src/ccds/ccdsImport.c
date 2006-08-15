/* ccdsImport - import NCBI CCDS DB table dumps into a MySQL database */

#include "common.h"
#include "options.h"
#include "sqlNum.h"
#include "jksql.h"
#include "errCatch.h"
#include "linefile.h"
#include "hgRelate.h"
#include "verbose.h"

static char const rcsid[] = "$Id: ccdsImport.c,v 1.4 2006/08/15 03:10:24 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"keep", OPTION_BOOLEAN},
    {NULL, 0}
};
boolean keep = FALSE;  /* keep tab files after load */

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "ccdsImport - import NCBI CCDS DB table dumps into a MySQL database.\n"
  "\n"
  "Usage:\n"
  "   ccdsImport [options] db dumpFiles\n"
  "\n"
  "The database should have been created  aand createTables.sql and\n"
  "createKeys.sql (normally from /cluster/data/genbank/etc/ should\n"
  "have already been run. The root file name of each dump file should\n"
  "specify the table name.  This handles reformating the dump files\n"
  "to deal with NULL columns and date format differences.\n"
  "\n"
  "Options:\n"
  "  -keep - keep tmp tab file used to load database\n"
  "  -verbose=n\n"
  );
}

static void doConvertCol(char *colVal, struct sqlFieldInfo *fi, char *sep, FILE *loadFh)
/* convert one column and output to file */
{
if (fi->allowsNull && (colVal[0] == '\0'))
    fprintf(loadFh, "%s\\N", sep);
else
    {
    char *escVal = needMem(2*strlen(colVal)+1);
    fprintf(loadFh, "%s%s", sep, sqlEscapeTabFileString2(escVal, colVal));
    freeMem(escVal);
    }
}

static void convertCol(char *colVal, struct sqlFieldInfo *fi, char *sep, char *dumpFile, FILE *loadFh)
/* convert one column and output to file. Catch errors and add more info  */
{
struct errCatch *except = errCatchNew();
if (errCatchStart(except))
    doConvertCol(colVal, fi, sep, loadFh);
errCatchEnd(except);
/* FIXME: memory leak if we actually continued */
if (except->gotError)
    errAbort("%s: error converting column %s: \"%s\": %s",
             dumpFile, fi->field, colVal, except->message->string);

errCatchFree(&except); 
}


static void convertRow(char **row, int numCols, struct sqlFieldInfo *fieldInfoList,
                char *dumpFile, char *table, FILE *loadFh)
/* convert one row and output to file */
{
int iCol;
char *sep = "";
struct sqlFieldInfo *fi;

/* for some reason, the dump files have an extra tab at the end, so we 
 * skip this bogus column */
if (row[numCols-1][0] != '\0')
    errAbort("%s: expected empty column at end of row", dumpFile);
numCols--;

if (slCount(fieldInfoList) != numCols)
    errAbort("%s: table %s has %d columns, import file has %d columns",
             dumpFile, table, slCount(fieldInfoList), numCols);

/* field info parallels columns */
for (iCol = 0, fi = fieldInfoList; iCol < numCols; iCol++, fi = fi->next)
    {
    convertCol(row[iCol], fi, sep, dumpFile, loadFh);
    sep = "\t";
    }
fprintf(loadFh, "\n");
assert(fi == NULL); /* should have reached end of list */
}

static int readLogical(FILE *fh, char *dumpFile, struct dyString *rowBuf)
/* read a line into rowBuf, handling quotes and escapes. Mark end of columns
 * with zero bytes in the rowBuf.  Can't save pointers until the whole line is
 * read as it rowBuf might resize */
{
boolean inQuotes = FALSE;
char prevChar = '\0';  /* used to find escapes */
int colCnt = 0;
int c;

/* when a new column is found, add a zero byte to the row buffer. Can't
 * save pointers in row until the whole line is read as it might resize */
dyStringClear(rowBuf);
while ((c = fgetc_unlocked(fh)) != EOF)
    {
    if ((c == '\n') && !inQuotes && (prevChar != '\\'))
        break;  /* reached end of line */
    else if (prevChar == '\\')
        {
        if (c == 't')
            dyStringAppendC(rowBuf, '\t');
        else 
            dyStringAppendC(rowBuf, c);
        }
    else if (c == '\\')
        prevChar = c; /* escape next */
    else if ((c == '\t') && !inQuotes)
        {
        colCnt++;
        dyStringAppendC(rowBuf, '\0');
        }
    else if (c == '"')
        inQuotes = !inQuotes;
    else
        dyStringAppendC(rowBuf, c);
    prevChar = c;
    }
if (inQuotes)
    errAbort("EOF in quoted string in %s", dumpFile);
if (rowBuf->stringSize > 0)
    {
    /* got at least on column, so count and flag last */
    colCnt++;
    dyStringAppendC(rowBuf, '\0');
    }
return colCnt;
}

static void saveColumns(struct dyString *rowBuf, char **row, int colCnt)
/* save pointers to each column in row array */
{
/* column values are all zero terminated now */
char *p = rowBuf->string;
int i;
for (i = 0; i < colCnt; i++)
    {
    row[i] = p;
    p += strlen(p)+1;
    }
}

static int readRow(FILE *fh, char *dumpFile, struct dyString *rowBuf, char **row, int maxCols)
/* read and parse a row, this handles quoted columns, including those with
 * with newlines.  Quotes are removed and the next line is read if a newline
 * is in the quotes. */
{
int colCnt = readLogical(fh, dumpFile, rowBuf);
if (colCnt > 0)
    {
    if (colCnt > maxCols)
        errAbort("%d columns exceeeds max of %d in %s", colCnt, maxCols, dumpFile);
    saveColumns(rowBuf, row, colCnt);
    }
return colCnt;
}

static void convertDumpFile(char *dumpFile, struct sqlFieldInfo *fieldInfoList, char *table, FILE *loadFh)
/* convert a dump file to a tab file suitable for loading by MySQL */
{
char *row[128];
FILE *dumpFh = mustOpen(dumpFile, "r");
struct dyString *rowBuf = dyStringNew(0);
int numCols;
while ((numCols = readRow(dumpFh, dumpFile, rowBuf, row, ArraySize(row))) > 0)
    convertRow(row, numCols, fieldInfoList, dumpFile, table, loadFh);

carefulClose(&dumpFh);
dyStringFree(&rowBuf);
}

static void importTable(struct sqlConnection *conn, char *dumpFile)
/* import a table from sybase dumps */
{
char table[128], query[128];
splitPath(dumpFile, NULL, table, NULL);
verbose(1, "loading table %s\n", table);

struct sqlFieldInfo *fieldInfoList = sqlFieldInfoGet(conn, table);
FILE *loadFh = hgCreateTabFile(".", table);

convertDumpFile(dumpFile, fieldInfoList, table, loadFh);
    
safef(query, sizeof(query), "truncate table %s", table);
sqlUpdate(conn, query);
hgLoadTabFileOpts(conn, ".", table, SQL_TAB_FILE_ON_SERVER, &loadFh);
if (!keep)
    hgRemoveTabFile(".", table);
}

static void ccdsImport(char *db, int numDumpFiles, char **dumpFiles)
/* import NCBI CCDS DB table dumps into a MySQL database */
{
struct sqlConnection *conn = sqlConnect(db);
int i;

for (i = 0; i < numDumpFiles; i++)
    importTable(conn, dumpFiles[i]);

sqlDisconnect(&conn);
verbose(1, "done\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 3)
    usage();
keep = optionExists("keep");
ccdsImport(argv[1], argc-2, argv+2);
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

