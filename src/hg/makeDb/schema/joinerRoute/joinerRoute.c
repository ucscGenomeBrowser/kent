/* joinerRoute - Display the results of joinerFindRouteThroughAll on a list of tables. */
#include "common.h"
#include "options.h"
#include "joiner.h"
#include "portable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "joinerRoute - Display the results of joinerFindRouteThroughAll on a list of tables\n"
  "usage:\n"
  "   joinerRoute /path/to/all.joiner db table1 table2 [...tableN]\n"
//  "options:\n"
//  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct joinerDtf *tablesToDtList(char *db, char *tables[], int tableCount)
/* Convert an array of table names to a joinerDtf list. */
{
struct joinerDtf *tableList = NULL;
int i;
for (i = 0;  i < tableCount;  i++)
    {
    char *table = tables[i];
    char *dot = strchr(table, '.');
    if (dot)
        {
        // It's a db.table not just table -- use the specified db instead of default db:
        char thisDb[dot-table+1];
        safencpy(thisDb, sizeof(thisDb), table, dot-table);
        slAddHead(&tableList, joinerDtfNew(thisDb, dot+1, NULL));
        }
    else
        slAddHead(&tableList, joinerDtfNew(db, table, NULL));
    }
slReverse(&tableList);
return tableList;
}

void joinerRoute(char *joinerFilePath, char *db, char *tables[], int tableCount)
/* joinerRoute - Display the results of joinerFindRouteThroughAll on a list of tables. */
{
struct joinerDtf *tableList = tablesToDtList(db, tables, tableCount);
long startTime = clock1000();
struct joiner *joiner = joinerRead(joinerFilePath);
long readTime = clock1000();
struct joinerPair *routeList = joinerFindRouteThroughAll(joiner, tableList);
long routeTime = clock1000();
joinerPairDump(routeList, stdout);
printf("joinerRead time: %ldms\n", readTime - startTime);
printf("joinerFindRouteThroughAll time: %ldms\n", routeTime - readTime);
printf("Tree version:\n");
joinerPairListToTree(routeList);
joinerPairDump(routeList, stdout);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 5)
    usage();
joinerRoute(argv[1], argv[2], argv+3, argc-3);
return 0;
}
