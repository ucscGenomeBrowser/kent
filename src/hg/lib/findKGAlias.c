#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "errabort.h"
#include "kgAlias.h"

static void addKgAlias(struct sqlConnection *conn, struct dyString *query,
       struct kgAlias **pList)
/* Query database and add returned kgAlias to head of list. */
{
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct kgAlias *kl = kgAliasLoad(row);
    slAddHead(pList, kl);
    }
sqlFreeResult(&sr);
}

struct kgAlias *findKGAlias(char *dataBase, char *spec, char *mode)
{
/* findKGAlias Looks up aliases for Known Genes, given a seach spec 

	mode "E" is for Exact match
 	mode "F" is for Fuzzy match

   it returns a link list of kgAlias nodes, which contain kgID and Alias 
*/
struct sqlConnection *conn  = hAllocConn();
struct sqlResult     *sr    = NULL;
struct dyString      *ds    = newDyString(256);
struct kgAlias *kaList 	    = NULL, *kl;
char   fullTableName[256];

snprintf(fullTableName, 250, "%s.%s", dataBase, "kgAlias");
if (!sqlTableExists(conn, fullTableName))
	{
	errAbort("Table %s.kgAlias does not exist.\n", dataBase);
	}

if (strcmp(mode, "E") == 0)
    {
    dyStringPrintf(ds, "select * from %s.kgAlias where alias = '%s'", dataBase, spec);
    }
else
    {
    if (strcmp(mode, "F") == 0)
	{
    	dyStringPrintf(ds, "select * from %s.kgAlias where alias like '%%%s%%'", dataBase, spec);
	}
    else
	{
	errAbort("%s is not a valid mode for findKGAlias()\n", mode);
	}
    }

addKgAlias(conn, ds, &kaList);
hFreeConn(&conn);

return(kaList);
}
