/* gbSeqCheck - check that gbExtFile references in gbSeq table are valid. */
#include "common.h"
#include "hdb.h"
#include "gbExtFile.h"

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
    "gbSeqCheck - check that extFile references in gbSeq table are valid\n"
    "usage:\n"
    "    gbSeqCheck database \n");
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

verbose(1, "reading in from gbSeq...\n");
safef(query, sizeof(query), "select distinct(gbExtFile) from gbSeq");
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
verbose(1, "%d rows found\n", count);
return list;
}



void gbSeqCheck()
/* gbSeqCheck - read in all seq, compare to extFile. */
{
struct extFileId *idList = NULL;
struct extFileId *id1 = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;

idList = readSeq();

verbose(1, "checking....\n");

for (id1 = idList; id1 != NULL; id1 = id1->next)
    {
    safef(query, sizeof(query), "select path from gbExtFile where id = %d", id1->id);
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

// gbSeqFreeList(&seqList);

}


int main(int argc, char *argv[])
/* Check args and call gbSeqCheck. */
{
if (argc != 2)
    usage();
database = argv[1];

// check for table existence
if (!hTableExists(database, "gbSeq"))
    errAbort("no gbSeq table");
if (!hTableExists(database, "gbExtFile"))
    errAbort("no gbExtFile table");

gbSeqCheck();
return 0;
}
