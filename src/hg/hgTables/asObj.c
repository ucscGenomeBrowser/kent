/* asObj - get and use parsed autoSQL objects describing table. */

#include "common.h"
#include "linefile.h"
#include "jksql.h"
#include "asParse.h"
#include "errCatch.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: asObj.c,v 1.2 2008/05/30 18:38:45 hiram Exp $";

static struct asObject *asForTableOrDie(struct sqlConnection *conn, char *table)
/* Get autoSQL description if any associated with table.   Abort if
 * there's a problem*/
{
struct asObject *asObj = NULL;
if (sqlTableExists(conn, "tableDescriptions"))
    {
    char query[256];
    char *asText = NULL;

    /* Try split table first. */
    safef(query, sizeof(query), 
    	"select autoSqlDef from tableDescriptions where tableName='chrN_%s'",
	table);
    asText = sqlQuickString(conn, query);

    /* If no result try unsplit table. */
    if (asText == NULL)
	{
	safef(query, sizeof(query), 
	    "select autoSqlDef from tableDescriptions where tableName='%s'",
	    table);
	asText = sqlQuickString(conn, query);
	}
    if (asText != NULL && asText[0] != 0)
	{
	asObj = asParseText(asText);
	}
    freez(&asText);
    }
return asObj;
}

struct asObject *asForTable(struct sqlConnection *conn, char *table)
/* Get autoSQL description if any associated with table. */
/* Wrap some error catching around asForTable. */
{
struct errCatch *errCatch = errCatchNew();
struct asObject *asObj = NULL;
if (errCatchStart(errCatch))
    {
    asObj = asForTableOrDie(conn, table);
    }
errCatchEnd(errCatch);
errCatchFree(&errCatch);
return asObj;
}

struct asColumn *asColumnFind(struct asObject *asObj, char *name)
/* Return named column. */
{
struct asColumn *asCol = NULL;
if (asObj!= NULL)
    {
    for (asCol = asObj->columnList; asCol != NULL; asCol = asCol->next)
        if (sameString(asCol->name, name))
	     break;
    }
return asCol;
}

