/* colTemplate - example template for a new type of column. To convert to 
 * your own column type as a starting point replace "xyz" with your column
 * type name.  The code here is a simpler version of the "lookup" column,
 * which just looks up a gene in a database table. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "cheapcgi.h"
#include "localmem.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"
#include "hdb.h"
#include "hgNear.h"

boolean xyzExists(struct column *col, struct sqlConnection *conn)
/* This returns true if all tables this depends on exists. */
{
return sqlTableExists(conn, col->table);
}

char *xyzCellVal(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Make comma separated list of matches to association table. */
{
char query[1000];
sqlSafef(query, sizeof(query), "select %s from %s where %s = '%s'",
      col->valField, col->table, col->keyField, gp->name);
return sqlQuickString(conn, query);
}

static void xyzCellPrint(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Print cell in xyz table. */
{
char *string = xyzCellVal(col, gp, conn);
hPrintf("<TD>");
if (string == NULL) 
    {
    hPrintf("n/a");
    }
else if (col->xyzItalics)
    {
    hPrintf("<I>");
    hPrintEncodedNonBreak(string);
    hPrintf("</I>");
    }
else
    hPrintEncodedNonBreak(string);
hPrintf("</TD>");
freeMem(string);
}

static void xyzConfigControls(struct column *col)
/* Print out configuration column */
{
hPrintf("<TD>");
char *italicsVarName = configVarName(col, "italics");
cgiMakeCheckBox(italicsVarName, cartUsualBoolean(cart, italicsVarName, TRUE));
hPrintf("italics");
hPrintf("</TD>");
}

struct searchResult *xyzSimpleSearch(struct column *col, 
    struct sqlConnection *conn, char *search)
/* Search xyz type column. */
{
struct searchResult *resList = NULL, *res;

/* Construct query */
char query[1000];
sqlSafef(query, sizeof(query), "select %s,%s from %s where %s='%s'", 
	col->keyField, col->valField, col->table, col->valField, search);

/* Get results from database and store in results list. */
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(res);
    res->gp.name = cloneString(row[0]);
    res->matchingId = cloneString(row[1]);
    slAddHead(&resList, res);
    }
sqlFreeResult(&sr);

/* Clean up and go home. */
slReverse(&resList);
return resList;
}


static void xyzFilterControls(struct column *col, 
	struct sqlConnection *conn)
/* Print out controls for advanced filter. */
{
hPrintf("Term to match exactly: ");
advFilterRemakeTextVar(col, "term", 20);
}

static struct genePos *xyzAdvFilter(struct column *col, 
	struct sqlConnection *conn, struct genePos *list)
/* Do advanced filter on xyz type column. */
{
char *term = advFilterVal(col, "term");
if (term != NULL)
    {
    /* Construct a hash of all genes that pass filter. */
    struct hash *passHash = newHash(17);
    char query[1000];
    char **row;
    struct sqlResult *sr;
    sqlSafef(query, sizeof(query), "select %s from %s where %s='%s'", 
	col->keyField, col->table, col->valField, term);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	hashAdd(passHash, row[0], NULL);
    sqlFreeResult(&sr);

    /* Remove non-passing genes. */
    list = weedUnlessInHash(list, passHash);
    hashFree(&passHash);
    }
return list;
}

void setupColumnXyz(struct column *col, char *parameters)
/* Set up a xyz type column. */
{
/* Read configuration variables and store in column. */
char *varName = configVarName(col, "italics");
col->xyzItalics = cartUsualBoolean(cart, varName, TRUE);

col->table = cloneString(nextWord(&parameters));  // Assume have a table parameter first
col->keyField = cloneString(nextWord(&parameters));  // Then key field
col->valField = cloneString(nextWord(&parameters));  // Then value field
if (col->valField == NULL)
    errAbort("Not enough fields in type for %s", col->name);
col->exists = xyzExists;		    // Required for all columns
col->cellVal = xyzCellVal;		    // Required for all columns

// The rest of these methods are optional.  Typically the system will do something
// sensible if they are missing.  Generally you do want to implement the filters.
col->cellPrint = xyzCellPrint;
// col->labelPrint = xyzLabelPrint;
// col->tableColumns = xyzLabelColumns;
col->configControls = xyzConfigControls;
col->simpleSearch = xyzSimpleSearch;
col->filterControls = xyzFilterControls;
col->advFilter = xyzAdvFilter;
}
