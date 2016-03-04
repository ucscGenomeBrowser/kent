/* makeTableDescriptions - Add table descriptions to database.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "jksql.h"
#include "hgRelate.h"
#include "asParse.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "makeTableDescriptions - Add table descriptions to database.\n"
  "usage:\n"
  "   makeTableDescriptions database tables.as\n"
  "This is for use on simple, non-genome, databases where\n"
  "there is just a single table of each type in a single .as file\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void makeTableDescriptions(char *database, char *asFile)
/* makeTableDescriptions - Add table descriptions to database.. */
{
struct sqlConnection *conn = sqlConnect(database);
struct lineFile *lf = lineFileOpen(asFile, TRUE);
FILE *f = hgCreateTabFile(".", "tableDescriptions");
/* Open a tab file with name corresponding to tableName in tmpDir. */
char *line;

/* struct asObject *asList = */ asParseFile(asFile);	/* Just to check syntax */

if (sqlTableExists(conn, "chromInfo"))
    errAbort("%s looks like a genome database, has chromInfo, aborting", 
    	database);

sqlRemakeTable(conn, "tableDescriptions",
   NOSQLINJ "CREATE TABLE tableDescriptions (\n"
   "  tableName varchar(255) not null,\n"
   "  autoSqlDef longblob not null,\n"
   "  gbdAnchor varchar(255) not null,\n"
   "  PRIMARY KEY(tableName(32))\n"
   ")" );

while (lineFileNextReal(lf, &line))
    {
    if (startsWith("table", line))
        {
	struct dyString *as = dyStringNew(0);
	char *name = trimSpaces(line + 6);	/* Skip over table. */
	char *escaped = NULL;

	fprintf(f, "%s\t", name);

	/* Putting lines into as. */
	for (;;)
	    {
	    char *s;
	    dyStringAppend(as, line);
	    dyStringAppendC(as, '\n');
	    s = skipLeadingSpaces(line);
	    if (s[0] == ')')
	        break;
	    if (!lineFileNext(lf, &line, NULL))
	        errAbort("Unexpected end of file, missing closing paren in %s",
			lf->fileName);
	    }
	escaped = needMem(2*as->stringSize+1);
	fprintf(f, "%s\t", sqlEscapeTabFileString2(escaped, as->string));
	fprintf(f, "\n");

	freez(&escaped);
	dyStringFree(&as);
	}
    else
        errAbort("Expecting table line %d of %s", lf->lineIx, lf->fileName);
    }
hgLoadTabFile(conn, ".", "tableDescriptions", &f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
makeTableDescriptions(argv[1], argv[2]);
return 0;
}
