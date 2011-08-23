/* asObj - get and use parsed autoSQL objects describing table. */

#include "common.h"
#include "linefile.h"
#include "jksql.h"
#include "asParse.h"
#include "errCatch.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: asObj.c,v 1.4 2009/05/20 20:59:55 mikep Exp $";

static struct asObject *asForTableOrDie(struct sqlConnection *conn, char *table)
/* Get autoSQL description if any associated with table.   Abort if
 * there's a problem*/
{
struct asObject *asObj = NULL;
if (isBigBed(database, table, curTrack, ctLookupName))
    asObj = bigBedAsForTable(table, conn);
else if (isBamTable(table))
    asObj = bamAsObj();
else if (isVcfTable(table))
    asObj = vcfAsObj();
else
    {
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

struct slName *asColNames(struct asObject *as)
/* Get list of column names. */
{
struct slName *list = NULL, *el;
struct asColumn *col;
for (col = as->columnList; col != NULL; col = col->next)
    {
    el = slNameNew(col->name);
    slAddHead(&list, el);
    }
slReverse(&list);
return list;
}

struct sqlFieldType *sqlFieldTypesFromAs(struct asObject *as)
/* Convert asObject to list of sqlFieldTypes */
{
struct sqlFieldType *ft, *list = NULL;
struct asColumn *col;
for (col = as->columnList; col != NULL; col = col->next)
    {
    struct dyString *type = asColumnToSqlType(col);
    ft = sqlFieldTypeNew(col->name, type->string);
    slAddHead(&list, ft);
    dyStringFree(&type);
    }
slReverse(&list);
return list;
}

