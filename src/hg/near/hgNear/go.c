/* go - Gene Ontology stuff. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "cart.h"
#include "hdb.h"
#include "hCommon.h"
#include "obscure.h"
#include "hgNear.h"
#include "spDb.h"


static boolean goExists(struct column *col, struct sqlConnection *conn)
/* This returns true if go database and goaPart table exists. */
{
boolean gotIt = FALSE;
col->goConn = sqlMayConnect("go");
if (col->goConn != NULL)
    {
    gotIt = sqlTableExists(col->goConn, "goaPart");
    if (gotIt)
	{
    	col->uniProtConn = sqlMayConnect(UNIPROT_DB_NAME);
	if (col->uniProtConn == NULL)
	    gotIt = FALSE;
	}
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
    char *proteinAcc = NULL;
    
    if (kgVersion == KG_III)
    	{
    	proteinAcc = spFindAcc(col->uniProtConn, lookupProtein(conn, gp->name));
	}
    else
    	{
    	proteinAcc = spFindAcc(col->uniProtConn, gp->protein);
        }
    
    if (proteinAcc)
	{	    
	sqlSafef(query, sizeof(query), 
		"select term.name from goaPart,term where goaPart.%s = '%s' and goaPart.goId = term.acc", col->goaIdColumn, proteinAcc);
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
    char *proteinAcc = NULL;
    
    if (kgVersion == KG_III)
    	{
    	proteinAcc = spFindAcc(col->uniProtConn, lookupProtein(conn, gp->name));
	}
    else
    	{
    	proteinAcc = spFindAcc(col->uniProtConn, gp->protein);
        }
    if (proteinAcc)
	{	    
	sqlSafef(query, sizeof(query), 
		"select term.name,term.acc from goaPart,term "
		"where goaPart.%s = '%s' "
		"and goaPart.goId = term.acc", 
		col->goaIdColumn, proteinAcc);
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
		
		hPrintf("<A HREF=\"http://amigo.geneontology.org/cgi-bin/amigo/go.cgi?view=details&search_constraint=terms&depth=0&query=%s\" TARGET=_blank>", row[1]);
		// hPrintf("<A HREF=\"http://www.ebi.ac.uk/ego/GSearch?query=%s&mode=id\" TARGET=_blank>", row[1]);
		// hPrintf("<A HREF=\"http://www.ebi.ac.uk/ego/DisplayGoTerm?id=%s&viz=tree\" TARGET=_blank>", row[1]);

		hPrintEncodedNonBreak(row[0]);
		hPrintf("</A>");
		hPrintf("'");
		}
	    }
	sqlFreeResult(&sr);
	}
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
    struct hash *proteinHash = newHash(16); /* protein IDs of matching terms. */
    struct hash *prevHash = NULL;
    struct genePos *newList = NULL, *gp, *next;

    /* First make hash of protein's of terms that match. */
    for (term = termList; term != NULL; term = term->next)
	{
	if (startsWith("GO:", term->name))
	    {
	    sqlSafef(query, sizeof(query),
		"select %s from goaPart "
		"where goId = '%s'", col->goaIdColumn, term->name);
	    }
	else
	    {
	    sqlSafef(query, sizeof(query), 
		    "select goaPart.%s from goaPart,term "
		    "where term.name = '%s' and term.acc = goaPart.goId"
		    , col->goaIdColumn, term->name);
	    }
	sr = sqlGetResult(col->goConn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    if (prevHash == NULL || hashLookup(prevHash, row[0]) != NULL)
                {
		hashStore(proteinHash, row[0]);
		}
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
        char *proteinAcc;
    
        if (kgVersion == KG_III)
    	    {
    	    proteinAcc = spFindAcc(col->uniProtConn, lookupProtein(conn, gp->name));
	    }
        else
    	    {
    	    proteinAcc = spFindAcc(col->uniProtConn, gp->protein);
            }

        if (proteinAcc && hashLookup(proteinHash, proteinAcc))
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
hPrintf("Gene Ontology</A> search.  Enclose term in single quotes if it "
        "contains multiple words.<BR>You may search with IDs (<em>e.g.</em> "
	"GO:0005884) as well as terms (<em>e.g.</em> 'actin filament').<BR>");
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

