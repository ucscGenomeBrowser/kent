/* hgLoadSqlTab - Load table into database from SQL and text files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "jksql.h"
#include "options.h"
#include "obscure.h"
#include "hgRelate.h"
#include "hdb.h"

static char const rcsid[] = "$Id: hgLoadSqlTab.c,v 1.6.24.1 2008/08/14 01:29:50 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
"hgLoadSqlTab - Load table into database from SQL and text files.\n"
"usage:\n"
"   hgLoadSqlTab database table file.sql file(s).tab\n"
"file.sql contains a SQL create statement for table\n"
"file.tab contains tab-separated text (rows of table)\n"
"The actual table name will come from the command line, not the sql file.\n"
"options:\n"
"  -warn - warn instead of abort on mysql errors or warnings\n"
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

struct dyString *readAndReplaceTableName(char *fileName, char *table)
/* Read file into string.  While doing so strip any leading comments
 * and insist that the first non-comment line contain the words
 * "create table" followed by a table name.  Replace the table name,
 * and copy the rest of the file verbatem. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct dyString *dy = dyStringNew(0);
char *line, *word;
if (!lineFileNextReal(lf, &line))
    errAbort("No real lines in %s\n", fileName);
word = nextWord(&line);
if (!sameWord(word, "create"))
    errAbort("Expecting first word in file to be CREATE. Got %s", word);
word = nextWord(&line);
if (word == NULL || !sameWord(word, "table"))
    errAbort("Expecting second word in file to be table. Got %s", emptyForNull(word));
word = nextWord(&line);
if (word == NULL)
    errAbort("Expecting table name on same line as CREATE TABLE");
dyStringPrintf(dy, "CREATE TABLE %s ", table);
dyStringAppend(dy, line);
dyStringAppendC(dy, '\n');
while (lineFileNext(lf, &line, NULL))
    {
    dyStringAppend(dy, line);
    dyStringAppendC(dy, '\n');
    }
lineFileClose(&lf);
return dy;
}

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
    struct dyString *dy = readAndReplaceTableName(createFile, table);
    sqlRemakeTable(conn, table, dy->string);
    dyStringFree(&dy);
    }
verbose(1, "Scanning through %d files\n", inCount);
for (i=0;  i < inCount;  i++)
    {
    verbose(2, "Loading file %s into table %s\n", inNames[i], table);
    if (sameString("stdin", inNames[i]))
	sqlLoadTabFile(conn, "/dev/stdin", table,
		       (loadOptions & ~SQL_TAB_FILE_ON_SERVER));
    else
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
