/* hgNear - gene family browser. */
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

static char const rcsid[] = "$Id: hgNear.c,v 1.5 2003/06/18 17:44:49 kent Exp $";

char *excludeVars[] = { "submit", "Submit", confVarName, defaultConfName,
	resetConfName, NULL }; 
/* The excludeVars are not saved to the cart. */

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
char *organism;		/* Name of organism - mouse, human, etc. */
char *curGeneId;	/* Identity of current gene. */
char *groupOn;		/* Current grouping strategy. */
int displayCount;	/* Number of items to display. */


/* ---- Some html helper routines. ---- */

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

void makeTitle(char *title, char *helpName)
/* Make title bar. */
{
hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#536ED3\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"2\"><TR>\n");
hPrintf("<TD ALIGN=LEFT><A HREF=\"/index.html\">%s</A></TD>", wrapWhiteFont("Home"));
hPrintf("<TD ALIGN=CENTER><FONT COLOR=\"#FFFFFF\" SIZE=5>%s</FONT></TD>", title);
hPrintf("<TD ALIGN=Right><A HREF=\"../goldenPath/help/%s\">%s</A></TD>", 
	helpName, wrapWhiteFont("Help"));
hPrintf("</TR></TABLE>");
}


/* ---- Some helper routines for column methods. ---- */

char *cellSimpleVal(struct column *col, char *geneId, struct sqlConnection *conn)
/* Get a field in a table defined by col->table, col->keyField, col->valField. */
{
char query[512];
struct sqlResult *sr;
char **row;
char *res = NULL;
safef(query, sizeof(query), "select %s from %s where %s = '%s'",
	col->valField, col->table, col->keyField, geneId);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    res = cloneString(row[0]);
sqlFreeResult(&sr);
return res;
}

void cellSimplePrint(struct column *col, char *geneId, struct sqlConnection *conn)
/* This just prints one field from table. */
{
char *s = col->cellVal(col, geneId, conn);
if (s == NULL) 
    {
    hPrintf("<TD>n/a</TD>", s);
    }
else
    {
    hPrintf("<TD>%s</TD>", s);
    freeMem(s);
    }
}

static boolean alwaysExists(struct column *col, struct sqlConnection *conn)
/* We don't exist ever. */
{
return TRUE;
}

boolean simpleTableExists(struct column *col, struct sqlConnection *conn)
/* This returns true if col->table exists. */
{
return sqlTableExists(conn, col->table);
}

static char *noVal(struct column *col, char *geneId, struct sqlConnection *conn)
/* Return not-available value. */
{
return cloneString("n/a");
}

void columnDefaultMethods(struct column *col)
/* Set up default methods. */
{
col->exists = alwaysExists;
col->cellVal = noVal;
col->cellPrint = cellSimplePrint;
}

void simpleMethods(struct column *col, char *table, char *key, char *val)
/* Set up the simplest type of methods for column. */
{
columnDefaultMethods(col);
col->table = table;
col->keyField = key;
col->valField = val;
col->cellVal = cellSimpleVal;
}

/* ---- Accession column ---- */

static char *accVal(struct column *col, char *geneId, struct sqlConnection *conn)
/* Return clone of geneId */
{
return cloneString(geneId);
}

void accMethods(struct column *col)
/* Set up methods for accession column. */
{
columnDefaultMethods(col);
col->cellVal = accVal;
}

/* ---- Number column ---- */

static char *numberVal(struct column *col, char *geneId, struct sqlConnection *conn)
/* Return incrementing number. */
{
static int ix = 0;
char buf[15];
++ix;
safef(buf, sizeof(buf), "%d", ix);
return cloneString(buf);
}

void numberMethods(struct column *col)
/* Set up methods for accession column. */
{
columnDefaultMethods(col);
col->cellVal = numberVal;
}


/* ---- Page/Form Making stuff ---- */

void controlPanel()
/* Make control panel. */
{
hPrintf("<TABLE WIDTH=\"100%%\" BORDER=0 CELLSPACING=1 CELLPADDING=1>\n");
hPrintf("<TR><TD ALIGN=CENTER>");

/* Do configure button. */
    {
    cgiMakeButton(confVarName, "configure");
    hPrintf(" ");
    }

/* Do sort by drop-down */
groupOn = cartUsualString(cart, groupVarName, "homology");
    {
    static char *menu[] = {"homology", "position"};
    hPrintf("group by ");
    cgiMakeDropList(groupVarName, menu, ArraySize(menu), groupOn);
    }

/* Do items to display drop-down */
    {
    char *count = cartUsualString(cart, countVarName, "50");
    static char *menu[] = {"25", "50", "100", "200", "500", "1000"};
    hPrintf(" display ");
    cgiMakeDropList(countVarName, menu, ArraySize(menu), count);
    displayCount = atoi(count);
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

struct slName *getGenomicNeighbors(struct sqlConnection *conn, char *geneId,
	char *chrom, int start, int end)
/* Get neighbors in genome. */
{
struct genePos
/* A gene position (a little local helper struct) */
    {
    struct genePos *next;
    char *name;
    int start;
    int end;
    } *gpList = NULL, *gp;
char query[256];
struct sqlResult *sr;
char **row;
int i, ix = 0, chosenIx = -1;
int startIx, endIx, listSize;
int geneCount = 0;
struct slName *geneList = NULL, *gene;

/* Get list of all genes in chromosome */
safef(query, sizeof(query), 
	"select name,txStart,txEnd from knownGene where chrom='%s'", chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(gp);
    gp->name = cloneString(row[0]);
    gp->start = sqlUnsigned(row[1]);
    gp->end = sqlUnsigned(row[2]);
    slAddHead(&gpList, gp);
    if (start == gp->start && end == gp->end && sameString(geneId, gp->name) )
        chosenIx = ix;
    ++ix;
    ++geneCount;
    }
sqlFreeResult(&sr);
slReverse(&gpList);

/* If not already found, find gene that we should try to put in middle of
 * our list. */
if (chosenIx < 0)
    {
    int middle = (start+end)/2, mid;
    int distance, bestDistance = BIGNUM;
    for (gp = gpList,ix=0; gp != NULL; gp = gp->next,++ix)
        {
	mid = (gp->start + gp->end)/2;
	distance = middle - mid;
	if (distance < 0) distance = -distance;
	if (distance < bestDistance)
	    {
	    bestDistance = distance;
	    chosenIx = ix;
	    }
	}
    warn("%s is not mapped", geneId);
    }

/* Figure out start and ending index of genes we want. */
startIx = chosenIx - displayCount/2;
endIx = startIx + displayCount;
if (startIx < 0) startIx = 0;
if (endIx > geneCount) endIx = geneCount;
listSize = endIx - startIx;

gp = slElementFromIx(gpList, startIx);
for (i=0; i<listSize; ++i, gp=gp->next)
    {
    gene = slNameNew(gp->name);
    slAddHead(&geneList, gene);
    }
slReverse(&geneList);

/* Clean up. */
for (gp = gpList; gp != NULL; gp = gp->next)
    freez(&gp->name);
slFreeList(&gpList);

return geneList;
}


struct slName *getPositionNeighbors(struct sqlConnection *conn)
/* Get genes in genomic neighborhood. */
{
char *pos = knownPosVal(NULL, curGeneId, conn);
if (pos == NULL)
    {
    warn("no position associated with %s", curGeneId);
    return NULL;
    }
else
    {
    char *chrom;
    int start,end;
    hgParseChromRange(pos, &chrom, &start, &end);
    return getGenomicNeighbors(conn, curGeneId, chrom, start, end);
    }
}

struct slName *getNeighbors(struct sqlConnection *conn)
/* Return gene neighbors. */
{
if (sameString(groupOn, "homology"))
    return getHomologyNeighbors(conn);
else if (sameString(groupOn, "position"))
    return getPositionNeighbors(conn);
else
    {
    errAbort("Unknown sort value %s", groupOn);
    return NULL;
    }
}

int columnCmpPriority(const void *va, const void *vb)
/* Compare to sort columns based on priority. */
{
const struct column *a = *((struct column **)va);
const struct column *b = *((struct column **)vb);
float dif = a->priority - b->priority;
if (dif < 0)
    return -1;
else if (dif > 0)
    return 1;
else
    return 0;
}

static void refinePriorities(struct column *colList)
/* Consult colOrderVar if it exists to reorder priorities. */
{
char *orig = cartOptionalString(cart, colOrderVar);
if (orig != NULL)
    {
    char *dupe = cloneString(orig);
    char *s = dupe;
    char *name, *val;
    struct hash *colHash = hashColumns(colList);
    struct column *col;
    while ((name = nextWord(&s)) != NULL)
        {
	if ((val = nextWord(&s)) == NULL || !isdigit(val[0]))
	    {
	    warn("Bad priority list: %s", orig);
	    cartRemove(cart, colOrderVar);
	    break;
	    }
	col = hashFindVal(colHash, name);
	if (col != NULL)
	    col->priority = atof(val);
	}
    hashFree(&colHash);
    freez(&dupe);
    }
}

void refineVisibility(struct column *colList)
/* Consult cart to set on/off visibility. */
{
char varName[128], *val;
struct column *col;

for (col = colList; col != NULL; col = col->next)
    {
    safef(varName, sizeof(varName), "near.col.%s", col->name);
    val = cartOptionalString(cart, varName);
    if (val != NULL)
	col->on = sameString(val, "on");
    }
}

struct column *getColumns(struct sqlConnection *conn)
/* Return list of columns for big table. */
{
struct column *colList = NULL, *col;

AllocVar(col);
col->name = "num";
col->label = "#";
col->priority = 1;
col->on = FALSE;
numberMethods(col);
if (col->exists(col, conn))
    slAddHead(&colList, col);

AllocVar(col);
col->name = "acc";
col->label = "Accession";
col->priority = 2;
col->on = TRUE;
accMethods(col);
if (col->exists(col, conn))
    slAddHead(&colList, col);

AllocVar(col);
col->name = "bitScore";
col->label = "Bits";
col->priority = 3;
col->on = TRUE;
bitScoreMethods(col);
if (col->exists(col, conn))
    slAddHead(&colList, col);

AllocVar(col);
col->name = "eVal";
col->label = "E Value";
col->priority = 4;
col->on = FALSE;
eValMethods(col);
if (col->exists(col, conn))
    slAddHead(&colList, col);

AllocVar(col);
col->name = "percentId";
col->label = "%ID";
col->priority = 5;
col->on = TRUE;
percentIdMethods(col);
if (col->exists(col, conn))
    slAddHead(&colList, col);

AllocVar(col);
col->name = "knownPos";
col->label = "Genome Position";
knownPosMethods(col);
col->priority = 6;
col->on = TRUE;
if (col->exists(col, conn))
    slAddHead(&colList, col);

refinePriorities(colList);
refineVisibility(colList);

slSort(&colList, columnCmpPriority);
return colList;
}

struct hash *hashColumns(struct column *colList)
/* Return a hash of columns keyed by name. */
{
struct column *col;
struct hash *hash = hashNew(8);
for (col = colList; col != NULL; col = col->next)
    {
    if (hashLookup(hash, col->name))
        warn("duplicate %s in column list", col->name);
    hashAdd(hash, col->name, col);
    }
return hash;
}

void bigTable()
/* Put up great big table. */
{
struct sqlConnection *conn = hAllocConn();
struct slName *geneList = getNeighbors(conn), *gene;
struct column *colList = getColumns(conn), *col;

hPrintf("<TABLE BORDER=1 CELLSPACING=1 CELLPADDING=1>\n");

/* Print label row. */
hPrintf("<TR BGCOLOR=\"#EFEFFF\">");
for (col = colList; col != NULL; col = col->next)
    {
    char *colName = col->label;
    if (col->on)
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
	if (col->on)
	    {
	    if (col->cellPrint == NULL)
		hPrintf("<TD></TD>");
	    else
		col->cellPrint(col,geneId,conn);
	    }
	}
    hPrintf("</TR>");
    }

hPrintf("</TABLE>");
hFreeConn(&conn);
}

void doMain()
/* The main page. */
{
char buf[128];
safef(buf, sizeof(buf), "UCSC %s Gene Family Browser", organism);
makeTitle(buf, "hgNear.html");
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=GET>\n");
controlPanel();
curGeneId = findGeneId();
if (curGeneId != NULL)
    {
    bigTable();
    }
}

void doMiddle(struct cart *theCart)
/* Write the middle parts of the HTML page. 
 * This routine sets up some globals and then
 * dispatches to the appropriate page-maker. */
{
char *var = NULL;
cart = theCart;
getDbAndGenome(cart, &database, &organism);
hSetDb(database);
if (cgiVarExists(confVarName))
    doConfigure(NULL);
else if ((var = cartFindLike(cart, "near.up.*")) != NULL)
    doConfigure(var);
else if ((var = cartFindLike(cart, "near.down.*")) != NULL)
    doConfigure(var);
else if (cgiVarExists(defaultConfName))
    {
    doDefaultConfigure();
    }
else
    doMain();
cartRemoveLike(cart, "near.up.*");
cartRemoveLike(cart, "near.down.*");
cartSaveSession(cart);
hPrintf("</FORM>");
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgNear - gene family browser - a cgi script\n"
  "usage:\n"
  "   hgNear\n"
  );
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartHtmlShell("Gene Family v1", doMiddle, hUserCookie(), excludeVars, NULL);
return 0;
}
