/* colBlast - some columns based on blast homology. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "htmshell.h"
#include "cart.h"
#include "hgNear.h"

static char *blastVal(struct column *col, char *geneId, struct sqlConnection *conn)
/* Get a field in a table defined by col->table, col->keyField, col->valField. */
{
char query[512];
struct sqlResult *sr;
char **row;
char *res = NULL;
safef(query, sizeof(query), "select %s from %s where target = '%s' and query = '%s'",
	col->valField, col->table, curGeneId, geneId);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    res = cloneString(row[0]);
sqlFreeResult(&sr);
return res;
}

static void blastMethods(struct column *col, char *valField)
/* Specify methods for blast columns. */
{
columnDefaultMethods(col);
col->table = "knownBlastTab";
col->valField = valField;
col->exists = simpleTableExists;
col->cellVal = blastVal;
col->cellPrint = cellSimplePrint;
}

void percentIdMethods(struct column *col)
/* Set up methods for percentage ID column. */
{
blastMethods(col, "identity");
}

void eValMethods(struct column *col)
/* Set up methods for e value column. */
{
blastMethods(col, "eValue");
}

void bitScoreMethods(struct column *col)
/* Set up methods for bit score column. */
{
blastMethods(col, "bitScore");
}


