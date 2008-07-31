/* hgLoadPsl - Load up a mySQL database with psl alignment tables. */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "dystring.h"
#include "psl.h"
#include "xAli.h"
#include "hdb.h"
#include "hgRelate.h"

static char const rcsid[] = "$Id: hgLoadPsl.c,v 1.32.160.1 2008/07/31 02:24:37 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"noTNameIx", OPTION_BOOLEAN},
    {"tNameIx", OPTION_BOOLEAN},  /* default now, but allow for backw.compat.*/
    {"nobin", OPTION_BOOLEAN},
    {"xa", OPTION_BOOLEAN},
    {"fastLoad", OPTION_BOOLEAN},
    {"onServer", OPTION_BOOLEAN},   /* this is now the default, leave in for compat */
    {"clientLoad", OPTION_BOOLEAN},
    {"append", OPTION_BOOLEAN},
    {"keep", OPTION_BOOLEAN},
    {"table", OPTION_STRING},
    {NULL, 0}
};

unsigned pslCreateOpts = 0;
unsigned pslLoadOpts = 0;
boolean append = FALSE;
boolean keep = FALSE;
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
  "If you get mySQL error 1085 check your umask -\n"
  " tab file must be world readable (path to, and psl file can have any perms)\n"
  "options:\n"
  "   -table=tableName  Explicitly set tableName.  (Defaults to file name)\n"
  "                     Only one file maybe specified with this option\n"
  "   -noTNameIx  Don't add target name index\n"
  "   -xa         Include sequence info\n"
  "   -fastLoad   Browser will lock up if someone opens this table during loading\n"
  "   -clientLoad If the SQL server complains about not being able to read the\n"
  "               load file, use this.  It will load via the client API, which\n"
  "               is slower.  This can also load remotely\n"
  "   -append  Append data, don't drop tables before loading\n"
  "   -nobin Repress binning\n"
  "   -keep  Don't remove intermediate tab file/s after load\n"
);
}

boolean hasBinColumn(struct sqlConnection *conn, char* table)
/* check if a table has a bin column */
{
struct slName *fields = sqlListFields(conn, table);
struct slName *f;
boolean found = FALSE;
for (f = fields; (f != NULL) && (!found); f = f->next)
    found = sameString(f->name, "bin");

slFreeList(&fields);
return found;
}

void checkBinConsistent(struct sqlConnection *conn, char* table)
/* check if bin column is consistent when appending */
{
if (sqlTableExists(conn, table))
    {
    /* make sure bin is consistent */
    if (pslCreateOpts & PSL_WITH_BIN)
        {
        if (!hasBinColumn(conn, table))
            errAbort("can't append to existing table %s, bin column requested,\n"
                     "however the table does not have a bin column", table);
        }
    else if (hasBinColumn(conn, table))
        {
        errAbort("can't append to existing table %s, -nobin specified,\n"
                 "however the table has a bin column", table);
        }
    }
}

void setupTable(char *database, struct sqlConnection *conn, char* table)
/* create a psl table as needed */
{
int minLength = hGetMinIndexLength(database);
char *sqlCmd = pslGetCreateSql(table, pslCreateOpts,
                               (pslCreateOpts & PSL_TNAMEIX) ?  minLength : 0);
if (append)
    {
    checkBinConsistent(conn, table);
    sqlMaybeMakeTable(conn, table, sqlCmd);
    }
else
    sqlRemakeTable(conn, table, sqlCmd);
freez(&sqlCmd);
}

void copyPslToTab(char *pslFile, char *tabFile)
/* copy a single PSL to the tab file */
{
struct psl *psl;
struct lineFile *lf = pslFileOpen(pslFile);
FILE *tabFh = mustOpen(tabFile, "w");
while ((psl = pslNext(lf)) != NULL)
    {
    if (pslCreateOpts & PSL_WITH_BIN)
        fprintf(tabFh, "%u\t", hFindBin(psl->tStart, psl->tEnd));
    pslTabOut(psl, tabFh);
    pslFree(&psl);
    }
lineFileClose(&lf);
carefulClose(&tabFh);
}

void copyPslXaToTab(char *pslFile, char *tabFile)
/* copy a single PSL XA to the tab file */
{
struct xAli *xa;
char *row[23];
struct lineFile *lf = lineFileOpen(pslFile, TRUE);
FILE *tabFh = mustOpen(tabFile, "w");
while (lineFileRow(lf, row))
    {
    xa = xAliLoad(row);
    if (pslCreateOpts & PSL_WITH_BIN)
        fprintf(tabFh, "%u\t", hFindBin(xa->tStart, xa->tEnd));
    xAliTabOut(xa, tabFh);
    xAliFree(&xa);
    }
carefulClose(&tabFh);
lineFileClose(&lf);
}

void loadPslTable(char *database, struct sqlConnection *conn, char *pslFile)
/* load one psl table */
{
char table[128], comment[256];
char *tabFile;
boolean indirectLoad = FALSE;

verbose(1, "Processing %s\n", pslFile);

/* determine table name to use */
if (clTableName != NULL)
    safef(table, sizeof(table), "%s", clTableName);
else
    {
    if (endsWith(pslFile, ".gz"))
	{
	char *stripGz;
	stripGz = cloneString(pslFile);
	chopSuffix(stripGz);
	splitPath(stripGz, NULL, table, NULL);
	freeMem(stripGz);
	}
    else
	splitPath(pslFile, NULL, table, NULL);
    }

setupTable(database, conn, table);

/* if a bin column is being added or if the input file is
 * compressed, we must copy to an intermediate tab file */
indirectLoad = ((pslCreateOpts & PSL_WITH_BIN) != 0) || endsWith(pslFile, ".gz");

if (indirectLoad)
    {
    tabFile = "psl.tab";
    if (pslCreateOpts & PSL_XA_FORMAT)
        copyPslXaToTab(pslFile, tabFile);
    else
        copyPslToTab(pslFile, tabFile);
    }
else
    tabFile = pslFile;

sqlLoadTabFile(conn, tabFile, table, pslLoadOpts);

/* add a comment and ids to the history table */
safef(comment, sizeof(comment), "Add psl alignments to %s table", table);
hgHistoryComment(conn, comment);

if (indirectLoad && !keep)
    unlink(tabFile);
}

void hgLoadPsl(char *database, int pslCount, char *pslFiles[])
/* hgLoadPsl - Load up a mySQL database with psl alignment tables. */
{
struct sqlConnection *conn = NULL;
int i;

conn = sqlConnect(database);
for (i = 0; i < pslCount; i++)
    loadPslTable(database, conn, pslFiles[i]);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (! optionExists("noTNameIx"))
    pslCreateOpts |= PSL_TNAMEIX;
if (!optionExists("nobin"))
    pslCreateOpts |= PSL_WITH_BIN;
if (optionExists("xa"))
    pslCreateOpts |= PSL_XA_FORMAT;
if (!optionExists("fastLoad"))
    pslLoadOpts |= SQL_TAB_FILE_CONCURRENT;
if (optionExists("clientLoad") && optionExists("onServer"))
    errAbort("can't specify both -clientLoad and -onServer");
if (!optionExists("clientLoad"))
    pslLoadOpts |= SQL_TAB_FILE_ON_SERVER;
clTableName = optionVal("table", NULL);
append = optionExists("append");
keep = optionExists("keep");
if (argc < 3)
    usage();
if ((clTableName != NULL) && (argc > 3))
    errAbort("may only specify one psl file with -tableName");

hgLoadPsl(argv[1], argc-2, argv+2);
return 0;
}
