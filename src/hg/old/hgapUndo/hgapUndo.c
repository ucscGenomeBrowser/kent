/* hgapUndo - help undo changes to hgap database. */
#include "common.h"
#include "jksql.h"
#include "hgRelate.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
 "hgapUndo - help undo changes to hgap database.\n"
 "usage:\n"
 "   hgapUndo last tables(s)\n"
 "Deletes rows with id's in the last history entry from tables\n"
 "   hgapUndo hist# tables(s)\n"
 "Deletes rows corresponding to given history #");
}

void undoTables(int hIx, char *tableNames[], int tableCount)
/* Remove id's in range specified by history table ix from
 * tables. */
{
struct sqlConnection *conn = hgStartUpdate();
struct sqlResult *sr;
char **row;
int lastIx;
char query[256];
unsigned startId, endId;
int i;

lastIx = sqlQuickNum(conn, 
    "SELECT MAX(ix) from history");
if (hIx <= 0)
    hIx = lastIx;
if (hIx > lastIx)
    errAbort("Asked to undo %d but only %d elements in history", hIx, lastIx);
sprintf(query, "select startId,endId from history where ix=%d", hIx);
sr = sqlMustGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't find ix %d in history table", hIx);
startId = sqlUnsigned(row[0]);
endId = sqlUnsigned(row[1]);
sqlFreeResult(&sr);
printf("Deleting id's between %u and %u\n", startId, endId);
for (i=0; i<tableCount; ++i)
    {
    char *table = tableNames[i];
    printf("Processing table %s\n", table);
    sprintf(query, "delete from %s where id >= %u and id < %u", table, 
    	startId, endId);
    sqlUpdate(conn, query);
    }
hgEndUpdate(&conn, "Undo #%d (%d items in %d tables including %s)", 
   hIx, endId-startId, tableCount, tableNames[0]);
}


int main(int argc, char *argv[])
{
char *ixString;
int ix;

if (argc < 3)
    usage();
ixString = argv[1];
if (isdigit(ixString[0]))
    ix = sqlUnsigned(ixString);
else if (sameWord(ixString, "last"))
    ix = -1;
else
    usage();
undoTables(ix, argv+2, argc-2);
}
