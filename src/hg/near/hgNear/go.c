/* go - Gene Ontology stuff. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "cart.h"
#include "hdb.h"
#include "hCommon.h"
#include "goa.h"
#include "hgNear.h"

static char const rcsid[] = "$Id: go.c,v 1.1 2003/09/08 09:04:21 kent Exp $";

static boolean goExists(struct column *col, struct sqlConnection *conn)
/* This returns true if go database and goa table exists. */
{
boolean gotIt = FALSE;
col->goConn = sqlMayConnect("go");
if (col->goConn != NULL)
    gotIt = sqlTableExists(col->goConn, "goa");
return gotIt;
}

static char *goCellVal(struct column *col, struct genePos *gp, 
   	struct sqlConnection *conn)
/* Get go terms as comma separated string. */
{
struct dyString *dy = dyStringNew(256);
char *result = NULL;
struct sqlResult *sr;
char **row;
char query[256];
boolean gotOne = FALSE;

if (gp->protein != NULL && gp->protein[0] != 0)
    {
    safef(query, sizeof(query), 
	    "select goId from goa where dbObjectSymbol = '%s'", gp->protein);
    sr = sqlGetResult(col->goConn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	if (!gotOne)
	    gotOne = TRUE;
	else
	    dyStringAppendC(dy, ',');
	dyStringAppend(dy, row[0]);
	}
    }
if (gotOne)
    result = cloneString(dy->string);
dyStringFree(&dy);
return result;
}

void setupColumnGo(struct column *col, char *parameters)
/* Set up gene ontology column. */
{
col->exists = goExists;
col->cellVal = goCellVal;
// col->cellPrint = goCellPrint;
col->filterControls = lookupAdvFilterControls;
// col->advFilter = goAdvFilter;
}
