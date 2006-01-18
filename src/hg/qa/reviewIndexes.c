/* reviewIndexes - check indexes. */
#include "common.h"
#include "hdb.h"

char *database = NULL;

struct table
{
    struct table *next;
    char *name;
};
		
struct table *tableList = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "reviewIndexes - check indexes\n"
    "usage:\n"
    "    reviewIndexes database \n");
}

struct table *getTables()
/* Get results from 'show tables' */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int count = 0;
struct table *table, *list = NULL;

verbose(2, "show tables...\n");
safef(query, sizeof(query), "show tables");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    count++;
    // short-circuit
    // if (count == 100) return list;
    AllocVar(table);
    table->name = cloneString(row[0]);
    table->next = list;
    list = table;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
verbose(2, "%d tables found\n", count);
return list;
}



void reviewIndexes()
/* reviewIndexes - look at index for each table. */
{
struct table *table1 = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

verbose(2, "checking....\n");

for (table1 = tableList; table1 != NULL; table1 = table1->next)
    {
    safef(query, sizeof(query), "show index from %s", table1->name);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	if (strstr(row[4], "start") || strstr(row[4], "Start"))
             printf("alter table %s drop index %s;\n", table1->name, row[2]);
	if (strstr(row[4], "end") || strstr(row[4], "End"))
             printf("alter table %s drop index %s;\n", table1->name, row[2]);
	} 
    sqlFreeResult(&sr);
    }

// freeList(&tableList);

}


int main(int argc, char *argv[])
/* Check args and call reviewIndexes. */
{
if (argc != 2)
    usage();
database = argv[1];
hSetDb(database);
tableList = getTables();
reviewIndexes();
return 0;
}
