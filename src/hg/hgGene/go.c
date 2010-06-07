/* go - Gene Ontology annotations. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "hgGene.h"
#include "hdb.h"

static char const rcsid[] = "$Id: go.c,v 1.9 2008/09/03 19:18:49 markd Exp $";

static boolean goExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if GO database exists and has something
 * on this one. */
{
char query[512];
char *fbAcc = getFlyBaseId(conn, geneId);
boolean useFbGo = (isFly() && fbAcc != NULL && sqlTableExists(conn, "fbGo"));
if (!sqlDatabaseExists("go"))
    return(FALSE);
if (useFbGo)
    {
    safef(query, sizeof(query),
	  "select count(*) from fbGo where geneId = '%s'",
	  fbAcc);
    return sqlQuickNum(conn, query) > 0;
    }
else
    {
    if (swissProtAcc == NULL || !sqlTableExists(conn, "go.goaPart"))
	return FALSE;
    safef(query, sizeof(query),
	  "select count(*) from go.goaPart where dbObjectId = '%s'",
	  swissProtAcc);
    return sqlQuickNum(conn, query) > 0;
    }
}

static void goPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out GO annotations. */
{
struct sqlConnection *goConn = hAllocConn("go");
char *fbAcc = getFlyBaseId(conn, geneId);
boolean useFbGo = (isFly() && fbAcc != NULL && sqlTableExists(conn, "fbGo"));
char *acc = useFbGo ? fbAcc : swissProtAcc;
char query[512];
struct sqlResult *sr;
char **row;
static char *aspects[3] = {"F", "P", "C"};
static char *aspectNames[3] = {
    "Molecular Function",
    "Biological Process",
    "Cellular Component",
};
int aspectIx;

for (aspectIx = 0; aspectIx < ArraySize(aspects); ++aspectIx)
    {
    boolean hasFirst = FALSE;
    if (useFbGo)
	safef(query, sizeof(query),
	      "select term.acc,term.name"
	      " from %s.fbGo,term"
	      " where %s.fbGo.geneId = '%s'"
	      " and %s.fbGo.goId = term.acc"
	      " and %s.fbGo.aspect = '%s'",
	      database, database, acc, database, database, aspects[aspectIx]);
    else
	safef(query, sizeof(query),
	      "select term.acc,term.name"
	      " from goaPart,term"
	      " where goaPart.dbObjectId = '%s'"
	      " and goaPart.goId = term.acc"
	      " and goaPart.aspect = '%s'"
	      , acc, aspects[aspectIx]);
    sr = sqlGetResult(goConn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	char *goID = row[0];
	char *goTermName = row[1];
	if (!hasFirst)
	    {
	    hPrintf("<B>%s:</B><BR>", aspectNames[aspectIx]);
	    hasFirst = TRUE;
	    }
        hPrintf("<A HREF = \"");
	hPrintf("http://amigo.geneontology.org/cgi-bin/amigo/go.cgi?view=details&search_constraint=terms&depth=0&query=%s", goID);
	hPrintf("\" TARGET=_blank>%s</A> %s<BR>\n", goID, goTermName);
	}
    if (hasFirst)
        hPrintf("<BR>");
    sqlFreeResult(&sr);
    }
hFreeConn(&goConn);
}

struct section *goSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create GO annotations section. */
{
struct section *section = sectionNew(sectionRa, "go");
section->exists = goExists;
section->print = goPrint;
return section;
}

