/* colKnownGene - columns having to do with knownGene and closely tables. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "dystring.h"
#include "jksql.h"
#include "cart.h"
#include "hdb.h"
#include "hCommon.h"
#include "hgNear.h"
#include "kgAlias.h"
#include "findKGAlias.h"

static char const rcsid[] = "$Id: knownGene.c,v 1.7 2003/06/25 02:56:45 kent Exp $";

static char *posFromRow3(char **row)
/* Convert chrom/start/end row to position. */
{
char buf[128];
char *chrom;
int start,end;

chrom = row[0];
start = sqlUnsigned(row[1]);
end = sqlUnsigned(row[2]);
safef(buf, sizeof(buf), "%s:%d-%d", chrom, start+1, end);
return cloneString(buf);
}


static char *genePredPosFromTable(char *table, struct genePos *gp, 
	struct sqlConnection *conn)
/* Get genePred position from table.  Well, actually just get it from gp. */
{
if (gp->chrom != NULL)
    {
    char pos[256];
    safef(pos, sizeof(pos), "%s:%d-%d", gp->chrom, gp->start+1, gp->end);
    return cloneString(pos);
    }
else
    {
    char query[256];
    struct sqlResult *sr;
    char *pos = NULL;
    char **row;

    safef(query, sizeof(query), 
    	"select chrom,txStart,txEnd from %s where name = '%s'",
	table, gp->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        pos = posFromRow3(row);
    sqlFreeResult(&sr);
    return pos;
    }
}

static char *genePredPosVal(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Get genome position of genePred type table. */
{
return genePredPosFromTable(col->table, gp, conn);
}

static void genePredPosPrint(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Print genome position with hyperlink to browser. */
{
char *pos = col->cellVal(col, gp, conn);
char *chrom;
int start, end;

hPrintf("<TD>");
if (pos == NULL)
    hPrintf("n/a");
else
    {
    char numBuf[32];
    hgParseChromRange(pos, &chrom, &start, &end);
    sprintLongWithCommas(numBuf, (start+end+1)/2);
    hPrintf("<A HREF=\"%s?db=%s&position=%s&%s\">",
	    hgTracksName(), database, pos, cartSidUrlString(cart));
    hPrintf("%s&nbsp;%s</A>", chrom, numBuf);
    freeMem(pos);
    }
hPrintf("</TD>");
}

static void genePredPosMethods(struct column *col, char *table)
/* Put in methods for genePred based positions. */
{
col->table = table;
col->exists = simpleTableExists;
col->cellVal = genePredPosVal;
col->cellPrint = genePredPosPrint;
}

char *knownPosVal(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Get genome position of knownPos table.  Ok to have col NULL. */
{
char *pos = genePredPosFromTable("knownGene", gp, conn);
if (pos == NULL)
    {
    char query[256];
    char buf[128];
    struct sqlResult *sr;
    char **row;
    safef(query, sizeof(query), "select tName, tStart, tEnd from %s where qName = '%s'",
	"all_mrna", gp->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	pos = posFromRow3(row);
    sqlFreeResult(&sr);
    }
return pos;
}

void setupColumnKnownPos(struct column *col, char *parameters)
/* Set up column that links to genome browser based on known gene
 * position. */
{
genePredPosMethods(col, "knownGene");
col->cellVal = knownPosVal;
}

void printKnownDetailsLink(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Print a link to known genes details page. */
{
char *s = col->cellVal(col, gp, conn);
if (s == NULL) 
    {
    hPrintf("<TD>n/a</TD>", s);
    }
else
    {
    hPrintf("<TD>");
    fillInKnownPos(gp, conn);
    hPrintf("<A HREF=\"../cgi-bin/hgc?%s&g=knownGene&i=%s&c=%s&l=%d&r=%d\">",
	    cartSidUrlString(cart), gp->name, gp->chrom, gp->start, gp->end);
    hPrintf("%s</A></TD>", s);
    freeMem(s);
    }
}

struct searchResult *knownGeneSearchResult(struct sqlConnection *conn, 
	char *kgID, char *alias)
/* Return a searchResult for a known gene. */
{
struct searchResult *sr;
struct dyString *dy = dyStringNew(1024);
char description[512];
char query[256];
char name[64];

/* Allocate and fill in with geneID. */
AllocVar(sr);
sr->gp.name = cloneString(kgID);

/* Get gene symbol into short label if possible. */
safef(query, sizeof(query),
	"select geneSymbol from kgXref where kgID = '%s'", kgID);
if (sqlQuickQuery(conn, query, name, sizeof(name)))
    sr->shortLabel = cloneString(name);
else
    sr->shortLabel = cloneString(kgID);

/* Add alias to long label if need be */
if (alias != NULL && !sameWord(name, alias))
    dyStringPrintf(dy, "(aka %s) ", alias);

/* Add description to long label. */
safef(query, sizeof(query), 
    "select description from kgXref where kgID = '%s'", kgID);
if (sqlQuickQuery(conn, query, description, sizeof(description)))
    dyStringAppend(dy, description);
sr->longLabel = cloneString(dy->string);

/* Cleanup and go home. */
dyStringFree(&dy);
return sr;
}

void fillInKnownPos(struct genePos *gp, struct sqlConnection *conn)
/* If gp->chrom is not filled in go look it up. */
{
if (gp->chrom == NULL)
    {
    char *pos = knownPosVal(NULL, gp, conn);
    char *chrom;
    hgParseChromRange(pos, &chrom, &gp->start, &gp->end);
    gp->chrom = cloneString(chrom);
    }
}

struct searchResult *knownGeneSimpleSearch(struct column *col, 
    struct sqlConnection *conn, char *search)
/* Look for matches to known genes. */
{
struct kgAlias *ka, *kaList;
struct searchResult *srList = NULL, *sr;
kaList = findKGAlias(database, search, "F");
for (ka = kaList; ka != NULL; ka = ka->next)
    {
    sr = knownGeneSearchResult(conn, ka->kgID, ka->alias);
    slAddHead(&srList, sr);
    }
return srList;
}

void setupColumnLookupKnown(struct column *col, char *parameters)
/* Set up a column that links to details page for known genes. */
{
setupColumnLookup(col, parameters);
col->cellPrint = printKnownDetailsLink;
col->simpleSearch = knownGeneSimpleSearch;
}

