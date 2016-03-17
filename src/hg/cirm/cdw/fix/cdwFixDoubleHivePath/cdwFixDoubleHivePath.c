/* cdwFixDoubleHivePath - Fix double inclusion of hive path in cdwSubmit URLs.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwFixDoubleHivePath - Fix double inclusion of hive path in cdwSubmit URLs.\n"
  "usage:\n"
  "   cdwFixDoubleHivePath out.sql\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void cdwFixDoubleHivePath(char *sqlFile)
/* cdwFixDoubleHivePath - Fix double inclusion of hive path in cdwSubmit URLs.. */
{
FILE *f = mustOpen(sqlFile, "w");
struct sqlConnection *conn = sqlConnect("cdw");
char query[512];
sqlSafef(query, sizeof(query), "select id,url from cdwSubmit");
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
char *pattern = "//hive/";
int patLen = strlen(pattern);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *id = row[0];
    char *url = row[1];
    if (url != NULL)
	{
	char *first = stringIn(pattern, url);
	if (first != NULL)
	    {
	    char *second = stringIn(pattern, first+patLen);
	    if (second != NULL)
		{
		strcpy(first, second);
		fprintf(f, "update cdwSubmit set url='%s' where id=%s;\n", url, id);
		}
	    }
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cdwFixDoubleHivePath(argv[1]);
return 0;
}
