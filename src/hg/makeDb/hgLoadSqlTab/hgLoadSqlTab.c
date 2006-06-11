/* hgLoadSqlTab - Load table into database from SQL and text files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "jksql.h"
#include "options.h"
#include "obscure.h"
#include "hgRelate.h"

static char const rcsid[] = "$Id: hgLoadSqlTab.c,v 1.2 2006/06/11 18:53:24 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
"hgLoadSqlTab - Load table into database from SQL and text files.\n"
"usage:\n"
"   hgLoadSqlTab database table file.sql file(s).tab\n"
"file.sql contains a SQL create statement for table\n"
"file.tab contains tab-separated text (rows of table)\n"
"options:\n"
"  -warn - warn or errors or warnings rather than abort\n"
"  -notOnServer - file is *not* in a directory that the mysql server can see\n"
"  -oldTable|-append - add to existing table\n"
  );
}

static struct optionSpec options[] = {
    {"warn", OPTION_BOOLEAN},
    {"notOnServer", OPTION_BOOLEAN},
    {"oldTable", OPTION_BOOLEAN},
    {"append", OPTION_BOOLEAN},
    {NULL, 0},
};

void hgLoadSqlTab(char *database, char *table, char *createFile,
		  int inCount, char *inNames[])
/* hgLoadSqlTab - Load table into database from SQL and text files. */
{
struct sqlConnection *conn = sqlConnect(database);
char comment[256];
int loadOptions = 0;
int i=0;
boolean oldTable = optionExists("oldTable") || optionExists("append");
if (optionExists("warn"))
    loadOptions |= SQL_TAB_FILE_WARN_ON_ERROR;

if (! optionExists("notOnServer"))
    loadOptions |= SQL_TAB_FILE_ON_SERVER;

if (! oldTable)
    {
    char *create = NULL;
    readInGulp(createFile, &create, NULL);
    sqlRemakeTable(conn, table, create);
    freez(&create);
    }
verbose(1, "Scanning through %d files\n", inCount);
for (i=0;  i < inCount;  i++)
    {
    verbose(2, "Loading file %s into table %s\n", inNames[i], table);
    sqlLoadTabFile(conn, inNames[i], table, loadOptions);
    }
if (oldTable)
    safef(comment, sizeof(comment),
	  "Add contents of %d text file(s) to table %s.",
	  inCount, table);
else
    safef(comment, sizeof(comment),
	  "Load table %s directly from .sql and %d text file(s).",
	  table, inCount);
hgHistoryComment(conn, comment);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 5)
    usage();
hgLoadSqlTab(argv[1], argv[2], argv[3], argc-4, argv+4);
return 0;
}
