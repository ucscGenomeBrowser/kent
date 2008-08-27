/* dbFindFieldsWith - Look through database and find fields that have elements matching a certain pattern in the first N rows.. */
#include "common.h"
#include <regex.h>
#include "linefile.h"
#include "jksql.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: dbFindFieldsWith.c,v 1.2 2008/08/27 19:06:12 kent Exp $";

int maxRows = 100;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dbFindFieldsWith - Look through database and find fields that have elements matching a certain "
  "regular expression in the first N rows.\n"
  "usage:\n"
  "   dbFindFieldsWith db regExp outputFile\n"
  "options:\n"
  "   -maxRows=%d\n"
  "Example - to find tables in go that contain _exactly_ the word 'kinase'\n"
  "    dbFindFieldsWith go '^kinase$' outputFile\n"
  "Example - to find tables in hg18 that contain a known gene id\n"
  "    dbFindFieldsWith hg18 '^uc[0-9][0-9][0-9][a-z][a-z][a-z].[0-9]+$' output\n"
  , maxRows
  );
}

static struct optionSpec options[] = {
   {"maxRows", OPTION_INT},
   {NULL, 0},
};

char *sqlColumnName(struct sqlResult *sr, int colIx)
/* REturn name of column of given index.  You can only use this once per result! */
{
int i;
for (i=0; i<colIx; ++i)
    sqlFieldName(sr);
return sqlFieldName(sr);
}

void dbFindFieldsWith(char *database, char *regExp, char *output)
/* dbFindFieldsWith - Look through database and find fields that have elements matching a certain pattern in the first N rows.. */
{
regex_t re;
int err = regcomp(&re, regExp, REG_NOSUB|REG_EXTENDED);
if (err < 0)
   errAbort("regcomp failed code %d", err);
struct sqlConnection *conn = sqlConnect(database);
struct slName *table, *tableList = sqlQuickList(conn, "show tables");
FILE *f = mustOpen(output, "w");
for (table = tableList; table != NULL; table = table->next)
    {
    char query[256];
    safef(query, sizeof(query), "select * from %s limit %d", table->name, maxRows);
    verbose(2, "%s.%s\n", database, table->name);
    struct sqlResult *sr = sqlGetResult(conn, query);
    if (sr != NULL)
	{
	int colCount = sqlCountColumns(sr);

	/* Get labels for columns */
	char **labels;
	AllocArray(labels, colCount);
	int i;
	for (i=0; i<colCount; ++i)
	    labels[i] = sqlFieldName(sr);

	/* Get flags that say which fields we've reported. */
	bool *flags;
	AllocArray(flags, colCount);
	char **row;
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    int i;
	    for (i=0; i<colCount; ++i)
	         {
		 char *field = row[i];
		 if (field != NULL && field[0] != 0)
		     {
		     if (regexec(&re, row[i], 0, NULL, 0) == 0)
			 {
			 if (!flags[i])
			     {
			     flags[i] = TRUE;
			     fprintf(f, "%s\t%s\t%s\n", table->name, labels[i], row[i]);
			     }
			 }
		     }
		 }
	    }
	sqlFreeResult(&sr);
	freez(&flags);
	freez(&labels);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
dbFindFieldsWith(argv[1], argv[2], argv[3]);
return 0;
}
