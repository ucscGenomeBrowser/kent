/* Handle expression ratio type microarray data. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "jksql.h"
#include "cart.h"
#include "hdb.h"
#include "hCommon.h"
#include "hgNear.h"

static char const rcsid[] = "$Id: expRatio.c,v 1.1 2003/06/21 05:54:42 kent Exp $";


static char *expRatioCellVal(struct column *col, char *geneId, 
	struct sqlConnection *conn)
/* Get comma separated list of values. */
{
return cloneString("coming soon");
}

boolean expRatioExists(struct column *col, struct sqlConnection *conn)
/* This returns true if relevant tables exist. */
{
boolean tableOk = sqlTableExists(conn, col->table);
boolean posTableOk = sqlTableExists(conn, col->posTable);
boolean expTableOk = sqlTableExists(conn, col->expTable);
return tableOk && posTableOk && expTableOk;
}


void setupColumnExpRatio(struct column *col, char *parameters)
/* Set up expression ration type column. */
{
/* Hash up name value pairs and extract some we care about. */
char *repList = hashFindVal(col->settings, "representatives");
char *expMax = hashFindVal(col->settings, "max");

col->table = cloneString(nextWord(&parameters));
col->posTable = cloneString(nextWord(&parameters));
col->expTable = cloneString(nextWord(&parameters));
if (col->posTable == NULL)
    errAbort("missing parameters from type line of %s", col->name);    


/* Convert list of ascii number in repList to an array of binary
 * numbers in col->representatives. */
if (repList != NULL)
    {
    char *repStrings[256];
    char *dupe = cloneString(repList);
    int repCount = chopString(dupe, ",", repStrings, ArraySize(repStrings));
    int *reps, i;
    AllocArray(reps, repCount);
    for (i=0; i<repCount; ++i)
        reps[i] = sqlUnsigned(repStrings[i]);
    col->representativeCount = repCount;
    col->representatives = reps;
    freez(&dupe);
    }

if (expMax != NULL)
    col->expMax = atof(expMax);
else
    col->expMax = 3.0;

col->exists = expRatioExists;
col->cellVal = expRatioCellVal;
}
