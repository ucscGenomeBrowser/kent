/* checkDbSync - Check databases on different machines are in sync. */
#include "common.h"
#include "hash.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkDbSync - Check databases on different machines are in sync\n"
  "usage:\n"
  "   checkDbSync database user password referenceHost otherHost(s)\n");
}

struct tableInfo
/* Information about one table. */
    {
    struct tableInfo *next;
    char *name;		/* Table name. */
    int itemCount;      /* Items in table. */
    };

void tableInfoFree(struct tableInfo **pEl)
/* Free up a table info. */
{
struct tableInfo *el;
if ((el = *pEl) != NULL)
    {
    freeMem(el->name);
    freez(pEl);
    }
}

void tableInfoFreeList(struct tableInfo **pList)
/* Free a list of dynamically allocated tableInfo's */
{
struct tableInfo *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    tableInfoFree(&el);
    }
*pList = NULL;
}

struct tableInfo *getTableInfo(char *database, char *host, char *user, char *password)
/* Get list of tables. */
{
struct tableInfo *list = NULL, *el;
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
char query[256];

conn = sqlConnectRemote(host, user, password, database);
sqlSafef(query, sizeof query, "show tables");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(el);
    el->name = cloneString(row[0]);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);

for (el = list; el != NULL; el = el->next)
    {
    sqlSafef(query, sizeof query, "select count(*) from %s", el->name);
    el->itemCount = sqlQuickNum(conn, query);
    }
sqlDisconnect(&conn);
return list;
}

struct hash *hashTableList(struct tableInfo *list)
/* Allocate a hash table and fill it with list. */
{
struct hash *hash = newHash(9);
struct tableInfo *el;

for (el = list; el != NULL; el = el->next)
    hashAdd(hash, el->name, el);
return hash;
}

void checkDbSync(char *database, char *user, char *password,
	char *refHost, int hostCount, char *hosts[])
/* checkDbSync - Check databases on different machines are in sync. */
{
char *otherHost;
struct hash *refHash = NULL, *otherHash = NULL;
struct tableInfo *refList = NULL, *otherList = NULL, *ti, *oti;
int i;

refList = getTableInfo(database, refHost, user, password);
refHash = hashTableList(refList);
for (i=0; i<hostCount; ++i)
    {
    /* Load up info on database from other host. */
    otherHost = hosts[i];
    otherList = getTableInfo(database, otherHost, user, password);
    otherHash = hashTableList(otherList);
    printf("Checking %s:%s vs. %s:%s\n", refHost, database, otherHost, database);

    /* Check for things only in reference host. */
    for (ti = refList; ti != NULL; ti = ti->next)
        {
	if (!hashLookup(otherHash, ti->name))
	    printf(" %s missing from %s\n", ti->name, otherHost);
	}
    
    /* Check for things that have changed from ref host. */
    for (ti = refList; ti != NULL; ti = ti->next)
        {
	if ((oti = hashFindVal(otherHash, ti->name)) != NULL)
	    {
	    if (oti->itemCount != ti->itemCount)
	        printf(" %s has %d items in %s and %d items in %s\n",
			ti->name, ti->itemCount, refHost, oti->itemCount, otherHost);
	    }
	}
    
    /* Check for things that are not in ref host. */
    for (oti = otherList; oti != NULL; oti = oti->next)
        {
	if (!hashLookup(refHash, oti->name))
	    printf(" %s in %s but not reference host %s\n", oti->name,
	    	otherHost, refHost);
	}

    freeHash(&otherHash);
    tableInfoFreeList(&otherList);
    }
freeHash(&refHash);
tableInfoFreeList(&refList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 4)
    usage();
checkDbSync(argv[1], argv[2], argv[3], argv[4], argc-5, argv+5);
return 0;
}
