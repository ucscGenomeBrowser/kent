/* go - Gene Ontology stuff. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "cart.h"
#include "hdb.h"
#include "hCommon.h"
#include "obscure.h"
#include "goa.h"
#include "hgNear.h"

static char const rcsid[] = "$Id: go.c,v 1.4 2003/09/09 08:11:11 kent Exp $";

static boolean goExists(struct column *col, struct sqlConnection *conn)
/* This returns true if go database and goa table exists. */
{
boolean gotIt = FALSE;
col->goConn = sqlMayConnect("go");
if (col->goConn != NULL)
    {
    gotIt = sqlTableExists(col->goConn, "goa");
    }
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
struct hash *hash = newHash(6);

if (gp->protein != NULL && gp->protein[0] != 0)
    {
    safef(query, sizeof(query), 
	    "select term.name from goa,term where goa.dbObjectSymbol = '%s' and goa.goId = term.acc", gp->protein);
    sr = sqlGetResult(col->goConn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	char *name = row[0];
	if (!hashLookup(hash, name))
	    {
	    hashAdd(hash, name, NULL);
	    if (!gotOne)
		gotOne = TRUE;
	    else
		dyStringAppend(dy, ", ");
	    dyStringAppend(dy, name);
	    }
	}
    sqlFreeResult(&sr);
    }
if (gotOne)
    result = cloneString(dy->string);
dyStringFree(&dy);
return result;
}

static void goCellPrint(struct column *col, struct genePos *gp, 
   	struct sqlConnection *conn)
/* Get go terms as comma separated string. */
{
struct sqlResult *sr;
char **row;
char query[256];
boolean gotOne = FALSE;
struct hash *hash = newHash(6);

hPrintf("<TD>");
if (gp->protein != NULL && gp->protein[0] != 0)
    {
    safef(query, sizeof(query), 
	    "select term.name,term.acc from goa,term "
	    "where goa.dbObjectSymbol = '%s' "
	    "and goa.goId = term.acc", 
	    gp->protein);
    sr = sqlGetResult(col->goConn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	char *name = row[0];
	if (!hashLookup(hash, name))
	    {
	    hashAdd(hash, name, NULL);
	    if (!gotOne)
		gotOne = TRUE;
	    else
		hPrintNonBreak(", ");
	    hPrintf("<A HREF=\"http://www.godatabase.org/cgi-bin/go.cgi?query=%s&view=details\" TARGET=_blank>", row[1]);
	    hPrintNonBreak(row[0]);
	    hPrintf("</A>");
	    }
	}
    sqlFreeResult(&sr);
    }
if (!gotOne)
    hPrintf("n/a");
hPrintf("</TD>");
}

static struct genePos *goAdvFilter(struct column *col, 
	struct sqlConnection *conn, struct genePos *list)
/* Do advanced filter on position. */
{
char *searchString = advFilterVal(col, "terms");
if (searchString != NULL )
    {
    char query[256];
    struct sqlResult *sr;
    char **row;
    boolean orLogic = advFilterOrLogic(col, "logic", FALSE);
    struct slName *term, *termList = stringToSlNames(searchString);
    struct hash *proteinHash = newHash(16); /* protein ID's of matching terms. */
    struct hash *prevHash = NULL;
    struct genePos *newList = NULL, *gp, *next;

    /* First make hash of protein's of terms that match. */
    for (term = termList; term != NULL; term = term->next)
	{
	if (startsWith("GO:", term->name))
	    {
	    safef(query, sizeof(query),
		"select dbObjectSymbol from goa "
		"where goId = '%s'", term->name);
	    }
	else
	    {
	    safef(query, sizeof(query), 
		    "select goa.dbObjectSymbol from goa,term "
		    "where term.name = '%s' and term.acc = goa.goId"
		    , term->name);
	    }
	sr = sqlGetResult(col->goConn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    if (prevHash == NULL || hashLookup(prevHash, row[0]) != NULL)
		hashStore(proteinHash, row[0]);
	    }
	sqlFreeResult(&sr);

	if (!orLogic)
	    {
	    hashFree(&prevHash);
	    if (term->next != NULL)
		{
		prevHash = proteinHash;
		proteinHash = newHash(17);
		}
	    }
	}

    /* Now whittle down list to only include those with correct protein. */
    for (gp = list; gp != NULL; gp = next)
	{
	next = gp->next;
	if (hashLookup(proteinHash, gp->protein))
	     {
	     slAddHead(&newList, gp);
	     }
	}
    slReverse(&newList);
    list = newList;
    hashFree(&prevHash);
    hashFree(&proteinHash);
    slFreeList(&termList);
    }
return list;
}

static void goFilterControls(struct column *col, struct sqlConnection *conn)
/* Print out controls for advanced filter. */
{
hPrintf("<A HREF=\"%s\">", "http://www.geneontology.org");
hPrintf("Gene Ontology</A> search.  Please enclose term in quotes if it "
        "contains multiple words.<BR>You can search with ID's (such as "
	"GO:0005884) as well as terms (such as 'actin filament').<BR>");
hPrintf("Term(s): ");
advFilterRemakeTextVar(col, "terms", 35);
hPrintf(" Include if ");
advFilterAnyAllMenu(col, "logic", FALSE);
hPrintf("terms match");
}

void setupColumnGo(struct column *col, char *parameters)
/* Set up gene ontology column. */
{
col->exists = goExists;
col->cellVal = goCellVal;
col->cellPrint = goCellPrint;
col->filterControls = goFilterControls;
col->advFilter = goAdvFilter;
}
