/* go - Gene Ontology stuff. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "cart.h"
#include "hdb.h"
#include "hCommon.h"
#include "obscure.h"
#include "hgNear.h"

static char const rcsid[] = "$Id: go.c,v 1.12 2003/11/12 18:51:10 kent Exp $";

static boolean goExists(struct column *col, struct sqlConnection *conn)
/* This returns true if go database and goaPart table exists. */
{
boolean gotIt = FALSE;
col->goConn = sqlMayConnect("go");
if (col->goConn != NULL)
    {
    gotIt = sqlTableExists(col->goConn, "goaPart");
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
	    "select term.name from goaPart,term where goaPart.%s = '%s' and goaPart.goId = term.acc", col->goaIdColumn, gp->protein);
    sr = sqlGetResult(col->goConn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	char *name = row[0];
	if (!hashLookup(hash, name))
	    {
	    hashAdd(hash, name, NULL);
	    gotOne = TRUE;
	    dyStringAppend(dy, "'");
	    dyStringAppend(dy, name);
	    dyStringAppend(dy, "'");
	    dyStringAppendC(dy, ',');
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
	    "select term.name,term.acc from goaPart,term "
	    "where goaPart.%s = '%s' "
	    "and goaPart.goId = term.acc", 
	    col->goaIdColumn, gp->protein);
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
		hPrintf("&nbsp;");
	    hPrintf("'");
	    hPrintf("<A HREF=\"http://www.godatabase.org/cgi-bin/go.cgi?query=%s&view=details\" TARGET=_blank>", row[1]);
	    // hPrintf("<A HREF=\"http://www.ebi.ac.uk/ego/GSearch?query=%s&mode=id\" TARGET=_blank>", row[1]);
	    // hPrintf("<A HREF=\"http://www.ebi.ac.uk/ego/DisplayGoTerm?id=%s&viz=tree\" TARGET=_blank>", row[1]);

	    hPrintNonBreak(row[0]);
	    hPrintf("</A>");
	    hPrintf("'");
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
		"select %s from goaPart "
		"where goId = '%s'", col->goaIdColumn, term->name);
	    }
	else
	    {
	    safef(query, sizeof(query), 
		    "select goaPart.%s from goaPart,term "
		    "where term.name = '%s' and term.acc = goaPart.goId"
		    , col->goaIdColumn, term->name);
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
hPrintf("Gene Ontology</A> search.  Please enclose term in single quotes if it "
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
col->goaIdColumn = columnRequiredSetting(col, "goaIdColumn");
}

#ifdef OLD

static boolean goOrderExists(struct order *ord, struct sqlConnection *ignore)
/* This returns true if go database and goaPart table exists. */
{
boolean gotIt = FALSE;
struct sqlConnection *conn = sqlMayConnect("go");
if (conn != NULL)
    {
    gotIt = sqlTableExists(conn, "goaPart");
    sqlDisconnect(&conn);
    }
return gotIt;
}

static void goCalcDistances(struct order *ord, 
	struct sqlConnection *ignore, /* connection to main database. */
	struct genePos **pGeneList, struct hash *geneHash, int maxCount)
/* Fill in distance fields in geneList. */
{
struct sqlConnection *conn = sqlConnect("go");
struct sqlResult *sr;
char **row;
struct hash *curTerms = newHash(8);
struct hash *protHash = newHash(17);
char query[512];
struct genePos *gp;

/* Build up hash of genes keyed by protein names. (The geneHash
 * passed in is keyed by the mrna name. */
for (gp = *pGeneList; gp != NULL; gp = gp->next)
    {
    hashAdd(protHash, gp->protein, gp);
    }

/* Build up hash full of all go IDs associated with protName. */

if (curGeneId->protein != NULL)
    {
    safef(query, sizeof(query), 
	    "select goId from goaPart where %s = '%s'", ord->keyField,
	    	curGeneId->protein);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	hashAdd(curTerms, row[0], NULL);
	}
    sqlFreeResult(&sr);
    }

/* Stream through association table counting matches. */
safef(query, sizeof(query), "select %s,goId from goaPart", ord->keyField);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (hashLookup(curTerms, row[1]))
	{
	struct hashEl *hel = hashLookup(protHash, row[0]);
	while (hel != NULL)
	    {
	    gp = hel->val;
	    gp->count += 1;
	    hel = hashLookupNext(hel);
	    }
	}
    }
sqlFreeResult(&sr);

/* Go through list translating non-zero counts to distances. */
for (gp = *pGeneList; gp != NULL; gp = gp->next)
    {
    if (gp->count > 0)
        {
	gp->distance = 1.0/gp->count;
	gp->count = 0;
	}
    if (sameString(gp->name, curGeneId->name))	/* Force self to top of list. */
        gp->distance = 0;
    }

hashFree(&protHash);
hashFree(&curTerms);
sqlDisconnect(&conn);
}

void goSimilarityMethods(struct order *ord, char *parameters)
/* Fill in goSimilarity methods. */
{
ord->exists = goOrderExists;
ord->calcDistances = goCalcDistances;
ord->keyField = hashFindVal(ord->settings, "goaIdColumn");
if (ord->keyField == NULL)
    errAbort("Missing goaIdColumn field in order.ra for go");
}
#endif /* OLD */
