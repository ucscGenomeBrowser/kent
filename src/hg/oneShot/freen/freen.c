/* freen - My Pet Freen. */
#include "common.h"
#include "jksql.h"
#include "browserTable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "freen - My pet freen\n"
  "usage:\n"
  "   freen hgN\n");
}

void scanBrowserTable(char *db)
/* Scan browser table and print selected fields. */
{
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr = sqlGetResult(conn, "select * from browserTable");
char **row;
struct browserTable *bt;

while ((row = sqlNextRow(sr)) != NULL)
    {
    bt = browserTableLoad(row);
    if (bt->shortLabel != NULL && bt->shortLabel[0] != 0 && bt->isPlaced)
        {
	printf("{\n");
	printf(" NULL,\n");
	printf(" \"%s\",	/* mapName */\n", bt->mapName);
	printf(" \"%s\",	/* tableName */\n", bt->tableName);
	printf(" \"%s\",	/* shortLabel */\n", bt->shortLabel);
	printf(" \"%s\",	/* longLabel */\n", bt->longLabel);
	printf(" %d,	/* visibility */\n", bt->visibility);
	printf(" %d,%d,%d,	/* color */\n", bt->colorR, bt->colorG, bt->colorB);
	printf(" %d,%d,%d,	/* altColor */\n", bt->altColorR, bt->altColorG, bt->altColorB);
	printf(" %d,	/* useScore */\n", bt->useScore);
	printf(" %d,	/* isSplit */\n", bt->isSplit);
	printf(" %d,	/* private */\n", bt->private);
	printf(" TRUE,	/* hardCoded */\n");
	printf("};\n");
	}
    browserTableFree(&bt);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2 )
    usage();
scanBrowserTable(argv[1]);
return 0;
}
