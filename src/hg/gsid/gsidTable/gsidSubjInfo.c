/* col gsidSubjInfo - columns having to do with subjInfo and closely related tables. */

#include "common.h"
//#include "linefile.h"
//#include "hash.h"
//#include "obscure.h"
#include "dystring.h"
#include "jksql.h"
//#include "cheapcgi.h"
//#include "cart.h"
//#include "hdb.h"
//#include "hCommon.h"
//#include "kgAlias.h"
//#include "findKGAlias.h"

#include "gsidTable.h"

static char const rcsid[] = "$Id: gsidSubjInfo.c,v 1.1 2007/01/14 23:11:46 galt Exp $";

struct subjInfo *readAllSubjInfo(struct sqlConnection *conn, struct column *columns)
/* Get all main table gsidSubjInfo columns in use. */
{
struct sqlResult *sr;
char **row;
struct subjInfo *siList = NULL, *si;
struct column *column = NULL;
int colCount = 0;
struct dyString *query = dyStringNew(256);
char *sep="";
if (!sameString(columns->name,"subjId"))
    errAbort("subjId must be the first column in columnDb.ra");
dyStringAppend(query,"select ");
for (column=columns;column;column=column->next)
    {
    // skip non-main table columns which have "query" setting
    // automatically add first column (subjId) whether it's on or not.
    if ((column->on || column->filterOn || column==columns) && (!column->query))  
	{
	dyStringPrintf(query,"%s%s\n", sep, column->name);
	column->colNo = colCount++;
	sep = ",";
	}
    }
dyStringAppend(query," from gsidSubjInfo\n");

sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    int i=0;
    AllocVar(si);
    si->fields = needMem(sizeof(char *)*colCount);
    for(i=0;i<colCount;++i)
	si->fields[i] = cloneString(row[i]);
    slAddHead(&siList, si);
    }
sqlFreeResult(&sr);
dyStringFree(&query);
return siList;
}





