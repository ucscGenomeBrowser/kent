/* hgNear - Similarity neighborhood gene browser. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "htmshell.h"
#include "cart.h"
#include "hdb.h"
#include "hui.h"
#include "web.h"
#include "hgNear.h"

static char const rcsid[] = "$Id: hgNear.c,v 1.2 2003/06/18 03:26:41 kent Exp $";

/* ---- Cart Variables ---- */
#define confVarName "near.configure"	/* Configuration button */
#define countVarName "near.count"	/* How many items to display. */
#define searchVarName "near.search"	/* Search term. */
#define geneIdVarName "near.geneId"     /* Purely cart, not cgi. Last valid geneId */
#define sortVarName "near.sort"		/* Sort scheme. */

char *excludeVars[] = { "submit", "Submit", confVarName, NULL }; 
/* The excludeVars are not saved to the cart. */

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
char *organism;		/* Name of organism - mouse, human, etc. */
char *curGeneId;	/* Identity of current gene. */
char *sortOn;		/* Current sort strategy. */
int displayCount;	/* Number of items to display. */


/* ---- Some general purpose helper routines. ---- */

void hvPrintf(char *format, va_list args)
/* Print out some html. */
{
vprintf(format, args);
}

void hPrintf(char *format, ...)
/* Print out some html. */
{
va_list(args);
va_start(args, format);
hvPrintf(format, args);
va_end(args);
}

/* ---- Some helper routines for column methods. ---- */

char *cellSimpleVal(struct column *col, char *geneId, struct sqlConnection *conn)
/* Get a field in a table defined by col->table, col->keyField, col->valField. */
{
char query[512];
struct sqlResult *sr;
char **row;
char *res = "n/a";
safef(query, sizeof(query), "select %s from %s where %s = '%s'",
	col->valField, col->table, col->keyField, geneId);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    res = row[0];
    }
res = cloneString(res);
sqlFreeResult(&sr);
return res;
}

void cellSimplePrint(struct column *col, char *geneId, struct sqlConnection *conn)
/* This just prints one field from table. */
{
char *s = col->cellVal(col, geneId, conn);
hPrintf("<TD>%s</TD>", s);
freeMem(s);
}

void simpleMethods(struct column *col, char *table, char *key, char *val)
/* Set up the simplest type of methods for column. */
{
col->table = table;
col->keyField = key;
col->valField = val;
col->cellVal = cellSimpleVal;
col->cellPrint = cellSimplePrint;
}

/* ---- Accession column ---- */

char *accVal(struct column *col, char *geneId, struct sqlConnection *conn)
/* Return clone of geneId */
{
return cloneString(geneId);
}

void accMethods(struct column *col)
/* Set up methods for accession column. */
{
col->cellVal = accVal;
col->cellPrint = cellSimplePrint;
}

/* ---- Page/Form Making stuff ---- */


void makeTitle()
/* Make title bar. */
{
hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#536ED3\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"2\"><TR>\n");
hPrintf("<TR>");
hPrintf("<TD ALIGN=LEFT><A HREF=\"/index.html\">%s</A></TD>", wrapWhiteFont("Home"));
hPrintf("<TD ALIGN=CENTER><FONT COLOR=\"#FFFFFF\" SIZE=5>UCSC %s %s</FONT></TD>", 
	organism, "Gene Similarity Browser");
hPrintf("<TD ALIGN=Right><A HREF=\"../goldenPath/help/hgNearHelp.html\">%s</A></TD>", 
	wrapWhiteFont("Help"));
hPrintf("</TR></TABLE>");
}

void controlPanel()
/* Make control panel. */
{
hPrintf("<TABLE WIDTH=\"100%%\" BORDER=0 CELLSPACING=1 CELLPADDING=1>\n");
hPrintf("<TR><TD ALIGN=CENTER>");

/* Do sort by drop-down */
sortOn = cartUsualString(cart, sortVarName, "homology");
    {
    static char *menu[] = {"homology", "expression", "chromosome"};
    hPrintf("sort on ");
    cgiMakeDropList(sortVarName, menu, ArraySize(menu), sortOn);
    }

/* Do items to display drop-down */
    {
    char *count = cartUsualString(cart, countVarName, "50");
    static char *menu[] = {"25", "50", "100", "200", "500", "1000"};
    hPrintf(" display ");
    cgiMakeDropList(countVarName, menu, ArraySize(menu), count);
    displayCount = atoi(count);
    }

/* Do configure button. */
    {
    hPrintf(" ");
    cgiMakeButton(confVarName, "configure");
    }

/* Do search box. */
    {
    char *search = cartUsualString(cart, searchVarName, "");
    hPrintf(" search ");
    cgiMakeTextVar(searchVarName,  search, 25);
    }

/* Do go button. */
    {
    hPrintf(" ");
    cgiMakeButton("submit", "go");
    }

hPrintf("</TD></TR></TABLE>");
}

char *findGeneId()
/* Find out gene ID from search term if available or
 * last gene ID if not. Returns NULL if not found. */
{
char *search = cartUsualString(cart, searchVarName, "");
char query[256];
char *result = NULL;
search = trimSpaces(search);
if (sameString(search, ""))
    search = cartOptionalString(cart, geneIdVarName);
if (search != NULL)
    {
    struct sqlConnection *conn = hAllocConn();
    safef(query, sizeof(query), 
    	"select name from knownGene where name = '%s'", search);
    if (sqlExists(conn, query))
	result = cloneString(search);
    else
	warn("%s not found", search);
    hFreeConn(&conn);
    }
return result;
}

struct slName *getHomologyNeighbors(struct sqlConnection *conn)
/* Get homology neighborhood. */
{
struct sqlResult *sr;
char **row;
struct dyString *query = dyStringNew(1024);
struct slName *list = NULL, *name;

dyStringPrintf(query, "select target from knownBlastTab where query = '%s'", 
	curGeneId);
dyStringPrintf(query, " order by bitScore desc limit %d", displayCount);
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    name = slNameNew(row[0]);
    slAddHead(&list, name);
    }
dyStringFree(&query);
slReverse(&list);
return list;
}

struct slName *getNeighbors(struct sqlConnection *conn)
/* Return gene neighbors. */
{
if (sameString(sortOn, "homology"))
    return getHomologyNeighbors(conn);
else
    {
    errAbort("Unknown sort value %s", sortOn);
    return NULL;
    }
}

struct column *getColumns()
/* Return list of columns for big table. */
{
struct column *list = NULL, *col;

AllocVar(col);
col->name = "acc";
col->label = "Accession";
accMethods(col);
slAddHead(&list, col);

AllocVar(col);
col->name = "eVal";
col->label = "E Value";
eValMethods(col);
slAddHead(&list, col);

AllocVar(col);
col->name = "percentId";
col->label = "%ID";
percentIdMethods(col);
slAddHead(&list, col);

slReverse(&list);
return list;
}

void bigTable()
/* Put up great big table. */
{
struct sqlConnection *conn = hAllocConn();
struct slName *geneList = getNeighbors(conn), *gene;
struct column *colList = getColumns(), *col;

hPrintf("<TABLE BORDER=1 CELLSPACING=1 CELLPADDING=1>\n");

/* Print label row. */
hPrintf("<TR BGCOLOR=\"#EFEFFF\">");
for (col = colList; col != NULL; col = col->next)
    {
    char *colName = col->label;
    hPrintf("<TD><B>%s</B></TD>", colName); 
    }
hPrintf("</TR>\n");

/* Print other rows. */
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    char *geneId = gene->name;
    if (sameString(geneId, curGeneId))
        hPrintf("<TR BGCOLOR=\"#FFEFEF\">");
    else
        hPrintf("<TR>");
    for (col = colList; col != NULL; 
    		col = col->next)
        {
	if (col->cellPrint == NULL)
	    hPrintf("<TD></TD>");
	else
	    col->cellPrint(col,geneId,conn);
	}
    hPrintf("</TR>");
    }

hPrintf("</TABLE>");
hFreeConn(&conn);
}

void doMain()
/* The main page. */
{
makeTitle();
controlPanel();
curGeneId = findGeneId();
if (curGeneId != NULL)
    {
    bigTable();
    }
}

void doConfigure()
/* Configuration page. */
{
hPrintf("<H2>Configuration Coming Someday</H2>");
}

void doMiddle(struct cart *theCart)
/* Write the middle parts of the HTML page. 
 * This routine sets up some globals and then
 * dispatches to the appropriate page-maker. */
{
cart = theCart;
getDbAndGenome(cart, &database, &organism);
hSetDb(database);
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=GET>\n");
cartSaveSession(cart);
if (cgiVarExists(confVarName))
    doConfigure();
else
    doMain();
hPrintf("</FORM>");
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgNear - Similarity neighborhood gene browser - a cgi script\n"
  "usage:\n"
  "   hgNear\n"
  );
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartHtmlShell("Gene Similarity v1", doMiddle, hUserCookie(), excludeVars, NULL);
return 0;
}
