/* hgLoadPsl - Load up a mySQL database with psl alignment tables. */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "dystring.h"
#include "psl.h"
#include "xAli.h"
#include "hdb.h"

boolean tNameIx = FALSE;
boolean noBin = FALSE;
boolean append = FALSE;
boolean xaFormat = FALSE;
boolean exportOutput = FALSE;
char *clTableName = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadPsl - Load up a mySQL database with psl alignment tables\n"
  "usage:\n"
  "   hgLoadPsl database file1.psl ... fileN.psl\n"
  "This must be run in the same directory as the .psl files\n"
  "It will create a table for each psl file.\n"
  "options:\n"
  "   -table=tableName  Explicitly set tableName.  (Defaults to file name)\n"
  "   -tNameIx  add target name index\n"
  "   -xa    Include sequence info\n"
  "   -export - create output in a manner similar to mysqlexport -T.\n"
  "      This create a sql script $table.sql and tab seperate file $table.txt\n"
  "      Useful when a psl is to be loaded in multiple databases or to avoid\n"
  "      writing an intermediate PSL file when used in a pipeline. The database is\n"
  "      is not loaded and db parameter is ignored\n"
  "   -append append data - don't drop table before loading\n"
  "   -nobin Repress binning");
}

void createTable(struct sqlConnection *conn, char* table)
{
char *sqlCmd = pslGetCreateSql(table, tNameIx, !noBin, xaFormat);
if (exportOutput)
    {
    char sqlFName[1024];
    FILE* fh;
    sprintf(sqlFName, "%s.sql", table);
    fh = mustOpen(sqlFName, "w");
    mustWrite(fh, sqlCmd, strlen(sqlCmd));
    carefulClose(&fh);
    }
else
    sqlRemakeTable(conn, table, sqlCmd);
freez(&sqlCmd);
}

void hgLoadPsl(char *database, int pslCount, char *pslNames[])
/* hgLoadPsl - Load up a mySQL database with psl alignment tables. */
{
int i;
char table[128];
char *pslName;
struct sqlConnection *conn = NULL;
struct dyString *ds = newDyString(2048);
if (!exportOutput)
    conn = sqlConnect(database);

for (i = 0; i<pslCount; ++i)
    {
    char tabFile[1024];
    pslName = pslNames[i];
    printf("Processing %s\n", pslName);
    if (clTableName != NULL)
        strcpy(table, clTableName);
    else
	splitPath(pslName, NULL, table, NULL);
    if (noBin)
        strcpy(tabFile, pslName);
    else
        {
        FILE *f;
	struct lineFile *lf = NULL;
        if (exportOutput)
            sprintf(tabFile, "%s.txt", table);
        else
            strcpy(tabFile, "psl.tab");

	f = mustOpen(tabFile, "w");
	if (xaFormat)
	    {
	    struct xAli *xa;
	    char *row[23];
	    lf = lineFileOpen(pslName, TRUE);
	    while (lineFileRow(lf, row))
	        {
		xa = xAliLoad(row);
		fprintf(f, "%u\t", hFindBin(xa->tStart, xa->tEnd));
		xAliTabOut(xa, f);
		xAliFree(&xa);
		}
	    }
	else
	    {
	    struct psl *psl;
	    lf = pslFileOpen(pslName);
	    while ((psl = pslNext(lf)) != NULL)
		{
		fprintf(f, "%u\t", hFindBin(psl->tStart, psl->tEnd));
		pslTabOut(psl, f);
		pslFree(&psl);
		}
	    lineFileClose(&lf);
	    }
	carefulClose(&f);
        }
    if (!append)
        createTable(conn, table);
    if (!exportOutput)
        {
        dyStringClear(ds);
        dyStringPrintf(ds, 
           "LOAD data local infile '%s' into table %s", tabFile, table);
        sqlUpdate(conn, ds->string);
        }
    }
if (conn != NULL)
    sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
tNameIx = optionExists("tNameIx");
clTableName = optionVal("table", NULL);
xaFormat = optionExists("xa");
append = optionExists("append");
exportOutput = optionExists("export");
noBin = optionExists("nobin");
if (noBin && exportOutput)
    errAbort("-nobin not supported with -export\n");
if (argc < 3)
    usage();
/* force progress message out immediatly */
setlinebuf(stdout);
setlinebuf(stderr);
hgLoadPsl(argv[1], argc-2, argv+2);
return 0;
}
