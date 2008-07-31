/* seqCheck - check that extFile references in seq table are valid. */
#include "common.h"
#include "hdb.h"

char *database = NULL;

struct extFileId
{
    struct extFileId *next;
    int id;
};


void usage()
/* Explain usage and exit. */
{
errAbort(
    "seqCheck - check that extFile references in seq table are valid\n"
    "usage:\n"
    "    seqCheck database \n");
}

struct extFileId *readSeq()
/* Slurp in the rows */
{
struct extFileId *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int count = 0;

verbose(2, "reading in from seq...\n");
safef(query, sizeof(query), "select distinct(extFile) from seq");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    count++;
    AllocVar(el);
    el->id = atoi(row[0]);
    el->next = list;
    list = el;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
verbose(2, "%d rows found\n", count);
return list;
}



void seqCheck()
/* seqCheck - read in all seq, compare to extFile. */
{
struct extFileId *idList = NULL;
struct extFileId *id1 = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;

idList = readSeq();

verbose(2, "checking....\n");

for (id1 = idList; id1 != NULL; id1 = id1->next)
    {
    safef(query, sizeof(query), "select path from extFile where id = %d", id1->id);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row == NULL) 
        {
	verbose(1, "no matches for %d\n", id1->id);
	continue;
	}

    /* check here for multiple matches */
    while ((row = sqlNextRow(sr)) != NULL)
        {
	}
    sqlFreeResult(&sr);
    }

// seqFreeList(&seqList);

}


int main(int argc, char *argv[])
/* Check args and call seqCheck. */
{
if (argc != 2)
    usage();
database = argv[1];

// check for table existence
if (!hTableExists(database, "seq"))
    errAbort("no seq table");
if (!hTableExists(database, "extFile"))
    errAbort("no extFile table");

seqCheck();
return 0;
}
