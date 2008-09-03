/* findKGProtAlias finds Known Gene entries using protein alias table */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "errabort.h"
#include "kgProtAlias.h"

static void addKGProtAlias(struct sqlConnection *conn, struct dyString *query,
       struct kgProtAlias **pList)
/* Query database and add returned kgProtAlias to head of list. */
{
struct sqlResult *sr = sqlGetResult(conn, query->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct kgProtAlias *kl = kgProtAliasLoad(row);
    slAddHead(pList, kl);
    }
sqlFreeResult(&sr);
}

struct kgProtAlias *findKGProtAlias(char *dataBase, char *spec, char *mode)
{
/* findKGProtAlias looks up protein aliases for Known Genes, given a seach spec 

	mode "E" is for Exact match
 	mode "F" is for Fuzzy match
 	mode "P" is for Prefix match

   it returns a link list of kgProtAlias nodes, which contain kgID, displayID, and alias 
*/
struct sqlConnection *conn  = hAllocConn(dataBase);
struct dyString      *ds    = newDyString(256);
struct kgProtAlias *kapList = NULL;
char   fullTableName[256];

snprintf(fullTableName, 250, "%s.%s", dataBase, "kgProtAlias");
if (!sqlTableExists(conn, fullTableName))
    {
    errAbort("Table %s.kgProtAlias does not exist.\n", dataBase);
    }

if (sameString(mode, "E"))
    {
    dyStringPrintf(ds, "select * from %s.kgProtAlias where alias = '%s'", dataBase, spec);
    }
else if (sameString(mode, "F"))
    {
    dyStringPrintf(ds, "select * from %s.kgProtAlias where alias like '%%%s%%'", dataBase, spec);
    }
else if (sameString(mode, "P"))
    {
    dyStringPrintf(ds, "select * from %s.kgProtAlias where alias like '%s%%'", dataBase, spec);
    }
else
    {
    errAbort("%s is not a valid mode for findKGAlias()\n", mode);
    }

addKGProtAlias(conn, ds, &kapList);
hFreeConn(&conn);

return(kapList);
}
