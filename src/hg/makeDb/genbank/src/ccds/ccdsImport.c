/* ccdsImport - import NCBI CCDS DB table dumps into a MySQL database */

#include "common.h"
#include "options.h"
#include "sqlNum.h"
#include "jksql.h"
#include "errCatch.h"
#include "linefile.h"
#include "hgRelate.h"
#include "verbose.h"

static char const rcsid[] = "$Id: ccdsImport.c,v 1.2.4.2 2005/08/21 04:37:54 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"keep", OPTION_BOOLEAN},
    {NULL, 0}
};
boolean keep = FALSE;  /* keep tab files after load */

void usage()
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

static void delimErr(char expectDel, int pos)
/* generate an error about an expected delimiter inside of a column value */
{
if (expectDel == ' ')
    errAbort("expected space at character %d of column", pos);
else
    errAbort("expected '%c' at character %d of column", expectDel, pos);
}

static void delimStr(char *colVal, char expectDel, int pos)
/* replace a delimiter with a zero byte, validating the delimiter */
{
if (colVal[pos] != expectDel)
    delimErr(' ', pos);
colVal[pos] = '\0';
}

int convertInt(char *colVal)
/* convert an integer, ignoring leading white space */
{
return sqlSigned(skipLeadingSpaces(colVal));
}

int convertMonth(char *monthStr, char *colVal)
/* convert a month string */
{
char *months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL
};
int i;

for (i = 0; months[i] != NULL; i++)
    {
    if (sameString(monthStr, months[i]))
        return i+1;
    }

errAbort("invalid month \"%s\"", monthStr);
return 0;
}

void convertDateTimeCol(char *colVal, char *sep, FILE *loadFh)
/* convert and output a datetime column */
{
/* sybase format: Feb  8 2005  3:35:01:270PM
 * mysql format: YYYY-MM-DD HH:MM:SS */
#define IMPORT_DATETIME_LEN 
char colCp[27]; /* fix length of string, plus one */
struct tm tm;
ZeroVar(&tm);

/* don't corrupt original for error messages */
if (strlen(colVal) != sizeof(colCp)-1)
    errAbort("expected date/time strlen of %lld, got %lld", (long long)sizeof(colCp)-1, (long long)strlen(colVal));
strcpy(colCp, colVal);

/* Feb  8 2005  3:35:01:270PM
 * 01234567890123456789012345
 * 0         1         2      */

/* month */
delimStr(colCp, ' ', 3);
tm.tm_mon = convertMonth(colCp+0, colVal);

/* day */
delimStr(colCp, ' ', 6);
tm.tm_mday = convertInt(colCp+4);

/* year */
delimStr(colCp, ' ', 11);
tm.tm_year = convertInt(colCp+7);

/* hour */
delimStr(colCp, ':', 14);
tm.tm_hour = convertInt(colCp+12);

/* minute */
delimStr(colCp, ':', 17);
tm.tm_min = convertInt(colCp+15);

/* seconds */
delimStr(colCp, ':', 20);
tm.tm_sec = convertInt(colCp+18);

/* AM/PM, convert to 24hr */
if (sameString(colCp+24, "PM"))
    {
    if (tm.tm_hour < 12)
        tm.tm_hour += 12;
    }
else if (!sameString(colCp+24, "AM"))
    errAbort("expected \"AM\" or \"PM\", got \"%s\"",colCp+24);

/* write as mysql time  */
fprintf(loadFh, "%s%04d-%02d-%02d %02d:%02d:%02d", sep,
        tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min,tm.tm_sec);
}

void doConvertCol(char *colVal, struct sqlFieldInfo *fi, char *sep, FILE *loadFh)
/* convert one column and output to file */
{
if (fi->allowsNull && (colVal[0] == '\0'))
    fprintf(loadFh, "%s\\N", sep);
else if (sameString(fi->type, "datetime"))
    convertDateTimeCol(colVal, sep, loadFh);
else
    {
    char *escVal = needMem(2*strlen(colVal)+1);
    fprintf(loadFh, "%s%s", sep, sqlEscapeTabFileString2(escVal, colVal));
    freeMem(escVal);
    }
}

void convertCol(char *colVal, struct sqlFieldInfo *fi, char *sep, struct lineFile *dumpLf, FILE *loadFh)
/* convert one column and output to file. Catch errors and add more info  */
{
struct errCatch *except = errCatchNew();
if (errCatchStart(except))
    doConvertCol(colVal, fi, sep, loadFh);
errCatchEnd(except);
/* FIXME: memory leak if we actually continued */
if (except->gotError)
    errAbort("%s:%d: error converting column %s: \"%s\": %s",
             dumpLf->fileName, dumpLf->lineIx, fi->field, colVal, except->message->string);

errCatchFree(&except); 
}


void convertRow(char **row, int numCols, struct sqlFieldInfo *fieldInfoList,
                struct lineFile* dumpLf, char *table, FILE *loadFh)
/* convert one row and output to file */
{
int iCol;
char *sep = "";
struct sqlFieldInfo *fi;

/* for some reason, the dump files have an extra tab at the end, so we 
 * skip this bogus column */
if (row[numCols-1][0] != '\0')
    errAbort("%s:%d: expected empty column at end of row",
             dumpLf->fileName, dumpLf->lineIx);
numCols--;

if (slCount(fieldInfoList) != numCols)
    errAbort("%s:%d: table %s has %d columns, import file has %d columns",
             dumpLf->fileName, dumpLf->lineIx, table, slCount(fieldInfoList),
             numCols);

/* field info parallels columns */
for (iCol = 0, fi = fieldInfoList; iCol < numCols; iCol++, fi = fi->next)
    {
    convertCol(row[iCol], fi, sep, dumpLf, loadFh);
    sep = "\t";
    }
fprintf(loadFh, "\n");
assert(fi == NULL); /* should have reached end of list */
}

void importTable(struct sqlConnection *conn, char *dumpFile)
{
char table[128], *row[128];
struct lineFile *dumpLf;
FILE *loadFh;
int numCols;
struct sqlFieldInfo *fieldInfoList;

splitPath(dumpFile, NULL, table, NULL);
verbose(1, "loading table %s\n", table);

fieldInfoList = sqlFieldInfoGet(conn, table);
dumpLf = lineFileOpen(dumpFile, TRUE);
loadFh = hgCreateTabFile(".", table);

while ((numCols = lineFileChopTab(dumpLf, row)) > 0)
    convertRow(row, numCols, fieldInfoList, dumpLf, table, loadFh);
    
lineFileClose(&dumpLf);

hgLoadTabFileOpts(conn, ".", table, SQL_TAB_FILE_ON_SERVER, &loadFh);
if (!keep)
    hgRemoveTabFile(".", table);
}

void ccdsImport(char *db, int numDumpFiles, char **dumpFiles)
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

