/* tabTable - Produce a tab-delimited table from database */
#include "common.h"
#include "dystring.h"
#include "jksql.h"

int main(int argc, char *argv[])
{
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
int modSize = 100000;
int mod = modSize;
int cumTotal = 0;
char *database, *table, *outName;
FILE *out;
char query[256];
int fieldCount;
int lastField;
int sep;
int i;
struct dyString *ds = newDyString(256);

if (argc < 4)
    {
    errAbort("tabTable - creates a tab-delimited version of mySql table\n"
             "usage\n"
	     "    tabTable database table output [start] [end]");
    }
database = argv[1];
table = argv[2];
outName = argv[3];
conn = sqlConnect(database);
sqlDyStringPrintf(ds, "select * from %s", table);
if (argc > 4)
    dyStringPrintf(ds, " where id >= %s", argv[4]);
if (argc > 5)
    dyStringPrintf(ds, " and id < %s", argv[5]);
sr = sqlGetResult(conn, ds->string);
out = mustOpen(outName, "w");
fieldCount = sqlFieldCount(sr);
lastField = fieldCount-1;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (--mod <= 0)
	{
	cumTotal += modSize;
	printf("Processing row %d\n", cumTotal);
	mod = modSize;
	}
    for (i=0; i<fieldCount; ++i)
	{
	char *field;
	if ((field = row[i]) == 0)
	    field = ".";
	sep = (i == lastField ? '\n' : '\t');
	fputs(field, out);
	fputc(sep, out);
	}
    }
}
