/* hgNear - gene family browser. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "obscure.h"
#include "portable.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "htmshell.h"
#include "subText.h"
#include "cart.h"
#include "hdb.h"
#include "hui.h"
#include "web.h"
#include "ra.h"
#include "hgNear.h"

static char const rcsid[] = "$Id: hgNear.c,v 1.63 2003/09/09 00:50:05 kent Exp $";

char *excludeVars[] = { "submit", "Submit", confVarName, colInfoVarName,
	defaultConfName, hideAllConfName, showAllConfName,
	saveCurrentConfName, useSavedConfName, 
	filSaveCurrentVarName, filUseSavedVarName,
	getSeqVarName, getSeqPageVarName, getGenomicSeqVarName, getTextVarName, 
	advFilterVarName, advFilterClearVarName, advFilterBrowseVarName,
	advFilterListVarName, idVarName, idPosVarName, NULL }; 
/* The excludeVars are not saved to the cart. */

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
char *organism;		/* Name of organism - mouse, human, etc. */
char *groupOn;		/* Current grouping strategy. */
int displayCount;	/* Number of items to display. */
char *displayCountString; /* Ascii version of display count, including 'all'. */

struct genePos *curGeneId;	  /* Identity of current gene. */

/* ---- General purpose helper routines. ---- */

int genePosCmpName(const void *va, const void *vb)
/* Sort function to compare two genePos by name. */
{
const struct genePos *a = *((struct genePos **)va);
const struct genePos *b = *((struct genePos **)vb);
return strcmp(a->name, b->name);
}

void genePosFillFrom5(struct genePos *gp, char **row)
/* Fill in genePos from row containing ascii version of
 * name/chrom/start/end/protein. */
{
gp->name = cloneString(row[0]);
gp->chrom = cloneString(row[1]);
gp->start = sqlUnsigned(row[2]);
gp->end = sqlUnsigned(row[3]);
gp->protein = cloneString(row[4]);
gp->distance = genePosTooFar;
}

int genePosCmpDistance(const void *va, const void *vb)
/* Compare to sort based on distance. */
{
const struct genePos *a = *((struct genePos **)va);
const struct genePos *b = *((struct genePos **)vb);
float diff = a->distance - b->distance;
if (diff > 0)
    return 1;
else if (diff < 0)
    return -1;
else
    return 0;
}

boolean wildMatchAny(char *word, struct slName *wildList)
/* Return TRUE if word matches any thing in wildList. */
{
struct slName *w;
for (w = wildList; w != NULL; w = w->next)
    if (wildMatch(w->name, word) )
        return TRUE;
return FALSE;
}

boolean wildMatchAll(char *word, struct slName *wildList)
/* Return TRUE if word matches all things in wildList. */
{
struct slName *w;
for (w = wildList; w != NULL; w = w->next)
    if (!wildMatch(w->name, word) )
        return FALSE;
return TRUE;
}

boolean wildMatchList(char *word, struct slName *wildList, boolean orLogic)
/* Return TRUE if word matches things in wildList. */
{
if (orLogic)
   return wildMatchAny(word, wildList);
else
   return wildMatchAll(word, wildList);
}

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

void hPrintNonBreak(char *s)
/* Print out string but replace spaces with &nbsp; */
{
char c;
while (c = *s++)
    {
    if (c == ' ')
	fputs("&nbsp;", stdout);
    else
        putchar(c);
    }
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

char *columnSetting(struct column *col, char *name, char *defaultVal)
/* Return value of named setting in column, or default if it doesn't exist. */
{
char *result = hashFindVal(col->settings, name);
if (result == NULL)
    result = defaultVal;
return result;
}

int columnSettingInt(struct column *col, char *name, int defaultVal)
/* Return value of named integer setting or default if it doesn't exist. */
{
char *result = hashFindVal(col->settings, name);
if (result == NULL)
    return defaultVal;
return atoi(result);
}

boolean columnSettingExists(struct column *col, char *name)
/* Return TRUE if setting exists in column. */
{
return hashFindVal(col->settings, name) != NULL;
}

char *colVarName(struct column *col, char *prefix)
/* Return variable name prefix.col->name. This is just a static
 * variable, so don't nest these calls*/
{
static char name[64];
safef(name, sizeof(name), "%s%s", prefix, col->name);
return name;
}

void colButton(struct column *col, char *prefix, char *label)
/* Make a button named prefix.col->name with given label. */
{
static char name[64];
safef(name, sizeof(name), "%s%s", prefix, col->name);
cgiMakeButton(name, label);
}

struct column *colButtonPressed(struct column *colList, char *prefix)
/* See if a button named prefix.column is pressed for some
 * column, and if so return the column, else NULL. */
{
static char pattern[64];
char colName[64];
char *match;
safef(pattern, sizeof(pattern), "%s*", prefix);
match = cartFindFirstLike(cart, pattern);
if (match == NULL)
    return NULL;

/* Construct column name.  If variable is from an file upload
 * there __filename suffix attached that we need to remove. */
safef(colName, sizeof(colName), "%s", match + strlen(prefix));
if (endsWith(colName, "__filename"))
    {
    int newLen = strlen(colName) - strlen("__filename");
    colName[newLen] = 0;
    }
return findNamedColumn(colList, colName);
}

struct hash *keyFileHash(struct column *col)
/* Make up a hash from key file for this column. 
 * Return NULL if no key file. */
{
char *fileName = advFilterVal(col, "keyFile");
if (fileName == NULL)
    return NULL;
if (!fileExists(fileName))
    {
    cartRemove(cart, advFilterName(col, "keyFile"));
    return NULL;
    }
return hashWordsInFile(fileName, 16);
}

char *cellLookupVal(struct column *col, struct genePos *gp, struct sqlConnection *conn);
/* Get a field in a table defined by col->table, col->keyField, col->valField. */

char *cellLookupVal(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Get a field in a table defined by col->table, col->keyField, 
 * col->valField. */
{
char query[512];
struct sqlResult *sr;
char **row;
char *res = NULL;
safef(query, sizeof(query), "select %s from %s where %s = '%s'",
	col->valField, col->table, col->keyField, gp->name);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    res = cloneString(row[0]);
sqlFreeResult(&sr);
return res;
}

void cellSimplePrint(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* This just prints one field from table. */
{
char *s = col->cellVal(col, gp, conn);
hPrintf("<TD>");
if (s == NULL) 
    {
    hPrintf("n/a");
    }
else
    {
    hPrintNonBreak(s);
    freeMem(s);
    }
hPrintf("</TD>");
}

static void hPrintSpaces(int count)
/* Print count number of spaces. */
{
while (--count >= 0)
    hPrintf(" ");
}

void colInfoAnchor(struct column *col)
/* Print anchor tag that leads to column info page. */
{
hPrintf("<A HREF=\"../cgi-bin/hgNear?%s&%s=%s\">", 
	cartSidUrlString(cart), colInfoVarName, col->name);
}

void colInfoLink(struct column *col)
/* Print link to column. */
{
colInfoAnchor(col);
hPrintf("%s</A>", col->shortLabel);
}

void labelSimplePrint(struct column *col)
/* This just prints cell->shortLabel.  If colWidth is
 * set it will add spaces, center justifying it.  */
{
int colWidth = columnSettingInt(col, "colWidth", 0);

hPrintf("<TH VALIGN=BOTTOM><B><PRE>");
/* The <PRE> above helps Internet Explorer avoid wrapping
 * in the label column, which helps us avoid wrapping in 
 * the data columns below.  Wrapping in the data columns
 * makes the expression display less effective so we try
 * to minimize it.  -jk */
if (colWidth == 0)
    {
    colInfoLink(col);
    }
else
    {
    int labelLen = strlen(col->shortLabel);
    int diff = colWidth - labelLen;
    if (diff < 0) diff = 0;
    hPrintSpaces(diff/2);
    colInfoLink(col);
    hPrintSpaces((diff+1)/2);
    }
hPrintf("</PRE></B></TH>");
}

void selfAnchorSearch(struct genePos *gp)
/* Print self anchor to given search term. */
{
hPrintf("<A HREF=\"../cgi-bin/hgNear?%s&%s=%s\">", 
	cartSidUrlString(cart), searchVarName, gp->name);
}

void selfAnchorId(struct genePos *gp)
/* Print self anchor to given id. */
{
hPrintf("<A HREF=\"../cgi-bin/hgNear?%s&%s=%s", 
	cartSidUrlString(cart), idVarName, gp->name);
if (gp->chrom != NULL)
    hPrintf("&%s=%s:%d-%d", idPosVarName, gp->chrom, gp->start+1, gp->end);
hPrintf("\">");
}

void cellSelfLinkPrint(struct column *col, struct genePos *gp,
	struct sqlConnection *conn)
/* Print self and hyperlink to make this the search term. */
{
char *s = col->cellVal(col, gp, conn);
if (s == NULL) 
    s = cloneString("n/a");
hPrintf("<TD>");
selfAnchorId(gp);
hPrintf("%s</A></TD>", s);
freeMem(s);
}

struct genePos *weedUnlessInHash(struct genePos *inList, struct hash *hash)
/* Return input list with stuff not in hash removed. */
{
struct genePos *outList = NULL, *gp, *next;
for (gp = inList; gp != NULL; gp = next)
    {
    next = gp->next;
    if (hashLookup(hash, gp->name))
        {
	slAddHead(&outList, gp);
	}
    }
slReverse(&outList);
return outList;
}

static boolean alwaysExists(struct column *col, struct sqlConnection *conn)
/* We always exist. */
{
return TRUE;
}

boolean simpleTableExists(struct column *col, struct sqlConnection *conn)
/* This returns true if col->table exists. */
{
return sqlTableExists(conn, col->table);
}

static char *noVal(struct column *col, struct genePos *gp, struct sqlConnection *conn)
/* Return not-available value. */
{
return cloneString("n/a");
}

static int oneColumn(struct column *col)
/* Return that we have single column. */
{
return 1;
}

void columnDefaultMethods(struct column *col)
/* Set up default methods. */
{
col->exists = alwaysExists;
col->cellVal = noVal;
col->cellPrint = cellSimplePrint;
col->labelPrint = labelSimplePrint;
col->tableColumns = oneColumn;
}

/* ---- Accession column ---- */

static char *accVal(struct column *col, struct genePos *gp, struct sqlConnection *conn)
/* Return clone of geneId */
{
return cloneString(gp->name);
}

struct genePos *accAdvFilter(struct column *col, 
	struct sqlConnection *conn, struct genePos *list)
/* Do advanced filter on accession. */
{
char *wild = advFilterVal(col, "wild");
struct hash *keyHash = keyFileHash(col);
if (keyHash != NULL)
    {
    struct genePos *newList = NULL, *next, *gp;
    for (gp = list; gp != NULL; gp = next)
        {
	next = gp->next;
	if (hashLookup(keyHash, gp->name))
	    {
	    slAddHead(&newList, gp);
	    }
	}
    slReverse(&newList);
    list = newList;
    }
if (wild != NULL)
    {
    boolean orLogic = advFilterOrLogic(col, "logic", TRUE);
    struct genePos *newList = NULL, *next, *gp;
    struct slName *wildList = stringToSlNames(wild);
    for (gp = list; gp != NULL; gp = next)
        {
	next = gp->next;
	if (wildMatchList(gp->name, wildList, orLogic))
	    {
	    slAddHead(&newList, gp);
	    }
	}
    slReverse(&newList);
    list = newList;
    }
return list;
}


void setupColumnAcc(struct column *col, char *parameters)
/* Set up a column that displays the geneId (accession) */
{
columnDefaultMethods(col);
col->cellVal = accVal;
col->filterControls = lookupAdvFilterControls;
col->advFilter = accAdvFilter;
}

/* ---- Number column ---- */

static char *numberVal(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Return incrementing number. */
{
static int ix = 0;
char buf[15];
++ix;
safef(buf, sizeof(buf), "%d", ix);
return cloneString(buf);
}

void setupColumnNum(struct column *col, char *parameters)
/* Set up column that displays index in displayed list. */
{
columnDefaultMethods(col);
col->cellVal = numberVal;
col->cellPrint = cellSelfLinkPrint;
}

/* ---- Simple table lookup type columns ---- */

static struct searchResult *lookupTypeSimpleSearch(struct column *col, 
    struct sqlConnection *conn, char *search)
/* Search lookup type column. */
{
struct dyString *query = dyStringNew(512);
char *searchHow = columnSetting(col, "search", "exact");
struct sqlConnection *searchConn = hAllocConn();
struct sqlResult *sr;
char **row;
struct searchResult *resList = NULL, *res;

dyStringPrintf(query, "select %s from %s where %s ", 
	col->keyField, col->table, col->valField);
if (sameString(searchHow, "fuzzy"))
    dyStringPrintf(query, "like '%%%s%%'", search);
else
    dyStringPrintf(query, " = '%s'", search);
sr = sqlGetResult(searchConn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    res = knownGeneSearchResult(conn, row[0], NULL);
    slAddHead(&resList, res);
    }

/* Clean up and go home. */
sqlFreeResult(&sr);
hFreeConn(&searchConn);
dyStringFree(&query);
slReverse(&resList);
return resList;
}

void lookupAdvFilterControls(struct column *col, struct sqlConnection *conn)
/* Print out controls for advanced filter. */
{
char *fileName = advFilterVal(col, "keyFile");
hPrintf("%s search (including * and ? wildcards):", col->shortLabel);
advFilterRemakeTextVar(col, "wild", 18);
hPrintf("<BR>\n");
hPrintf("Include if ");
advFilterAnyAllMenu(col, "logic", TRUE);
hPrintf("words in search term match.<BR>");
if (!columnSetting(col, "noKeys", NULL))
    {
    hPrintf("Limit to items in list: ");
    advFilterKeyPasteButton(col);
    hPrintf(" ");
    advFilterKeyUploadButton(col);
    hPrintf(" ");
    if (fileName != NULL)
	{
	if (fileExists(fileName))
	    {
	    advFilterKeyClearButton(col);
	    hPrintf("<BR>\n");
	    hPrintf("(There are currently %d items in list.)",
		    countWordsInFile(fileName));
	    }
	else
	    {
	    cartRemove(cart, advFilterName(col, "keyFile"));
	    }
       }
    }
}

struct genePos *lookupAdvFilter(struct column *col, 
	struct sqlConnection *conn, struct genePos *list)
/* Do advanced filter on position. */
{
char *wild = advFilterVal(col, "wild");
struct hash *keyHash = keyFileHash(col);
if (wild != NULL || keyHash != NULL)
    {
    boolean orLogic = advFilterOrLogic(col, "logic", TRUE);
    struct hash *hash = newHash(17);
    char query[256];
    struct sqlResult *sr;
    char **row;
    struct slName *wildList = stringToSlNames(wild);
    safef(query, sizeof(query), "select %s,%s from %s",
    	col->keyField, col->valField, col->table);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	if (keyHash == NULL || hashLookup(keyHash, row[1]))
	    {
	    if (wildList == NULL || wildMatchList(row[1], wildList, orLogic))
		hashAdd(hash, row[0], NULL);
	    }
	}
    list = weedUnlessInHash(list, hash);
    sqlFreeResult(&sr);
    hashFree(&hash);
    }
return list;
}

void lookupTypeMethods(struct column *col, char *table, char *key, char *val)
/* Set up the methods for a simple lookup column. */
{
col->table = cloneString(table);
col->keyField = cloneString(key);
col->valField = cloneString(val);
col->exists = simpleTableExists;
col->cellVal = cellLookupVal;
if (columnSetting(col, "search", NULL))
    {
    col->simpleSearch = lookupTypeSimpleSearch;
    }
col->filterControls = lookupAdvFilterControls;
col->advFilter = lookupAdvFilter;
}

void setupColumnLookup(struct column *col, char *parameters)
/* Set up column that just looks up one field in a table
 * keyed by the geneId. */
{
char *table = nextWord(&parameters);
char *keyField = nextWord(&parameters);
char *valField = nextWord(&parameters);
if (valField == NULL)
    errAbort("Not enough fields in type lookup for %s", col->name);
lookupTypeMethods(col, table, keyField, valField);
}

/* ---- Distance table type columns ---- */

static char *cellDistanceVal(struct column *col, struct genePos *gp, struct sqlConnection *conn)
/* Get a field in a table defined by col->table, col->keyField, col->valField. */
{
char query[512];
struct sqlResult *sr;
char **row;
char *res = NULL;
safef(query, sizeof(query), "select %s from %s where %s = '%s' and %s = '%s'",
	col->valField, col->table, col->keyField, gp->name, col->curGeneField, curGeneId->name);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    res = cloneString(row[0]);
sqlFreeResult(&sr);
return res;
}

void distanceFilterControls(struct column *col, struct sqlConnection *conn)
/* Print out controls for advanced filter. */
{
hPrintf("minimum: ");
advFilterRemakeTextVar(col, "min", 8);
hPrintf(" maximum: ");
advFilterRemakeTextVar(col, "max", 8);
}

struct genePos *distanceAdvFilter(struct column *col, 
	struct sqlConnection *conn, struct genePos *list)
/* Do advanced filter on distance type. */
{
char *minString = advFilterVal(col, "min");
char *maxString = advFilterVal(col, "max");
if (minString != NULL || maxString != NULL)
    {
    struct hash *passHash = newHash(16);  /* Hash of genes that pass. */
    struct sqlResult *sr;
    char **row;
    struct dyString *dy = newDyString(512);
    dyStringPrintf(dy, "select %s from %s where", col->keyField, col->table);
    dyStringPrintf(dy, " %s='%s'", col->curGeneField, curGeneId->name);
    if (minString)
         dyStringPrintf(dy, " and %s >= %s", col->valField, minString);
    if (maxString)
         {
	 if (minString)
	     dyStringPrintf(dy, " and ");
         dyStringPrintf(dy, " and %s <= %s", col->valField, maxString);
	 }
    sr = sqlGetResult(conn, dy->string);
    while ((row = sqlNextRow(sr)) != NULL)
        hashAdd(passHash, row[0], NULL);
    list = weedUnlessInHash(list, passHash);
    sqlFreeResult(&sr);
    dyStringFree(&dy);
    hashFree(&passHash);
    }
return list;
}

void distanceTypeMethods(struct column *col, char *table, 
	char *curGene, char *otherGene, char *valField)
/* Set up a column that looks up a field in a distance matrix
 * type table such as the expression or homology tables. */
{
col->table = cloneString(table);
col->keyField = cloneString(otherGene);
col->valField = cloneString(valField);
col->curGeneField = cloneString(curGene);
col->exists = simpleTableExists;
col->cellVal = cellDistanceVal;
col->filterControls = distanceFilterControls;
col->advFilter = distanceAdvFilter;
}

void setupColumnDistance(struct column *col, char *parameters)
/* Set up a column that looks up a field in a distance matrix
 * type table such as the expression or homology tables. */
{
char *table = nextWord(&parameters);
char *curGene = nextWord(&parameters);
char *otherGene = nextWord(&parameters);
char *valField = nextWord(&parameters);
if (valField == NULL)
    errAbort("Not enough fields in type distance for %s", col->name);
distanceTypeMethods(col, table, curGene, otherGene, valField);
}

/* ---- Page/Form Making stuff ---- */

void controlPanel(struct genePos *gp)
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
    {
    static char *menu[] = {"expression", "homology", "name", "position", };
    int menuSize = ArraySize(menu);
    hPrintf("group by ");
    cgiMakeDropList(groupVarName, menu, menuSize, groupOn);
    }

/* Do items to display drop-down */
    {
    static char *menu[] = {"25", "50", "100", "200", "500", "1000", "all"};
    hPrintf(" display ");
    cgiMakeDropList(countVarName, menu, ArraySize(menu), displayCountString);
    }

/* Do search box. */
    {
    char *search = "";
    if (gp != NULL) search = gp->name;
    hPrintf(" search ");
    cgiMakeTextVar(searchVarName,  search, 25);
    }

/* Do go button. */
    {
    hPrintf(" ");
    printf("<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\" TABINDEX=0>", "submit", "Go!");
    }

hPrintf("</TD></TR>\n<TR><TD ALIGN=CENTER>");

/* Do genome drop down (just fake for now) */
    {
    static char *menu[] = {"Human"};
    hPrintf("genome: ");
    cgiMakeDropList("near.genome", menu, ArraySize(menu), menu[0]);
    }

/* Do assembly drop down (again just fake) */
    {
    static char *menu[] = {"April 2003"};
    hPrintf(" assembly: ");
    cgiMakeDropList("near.assembly", menu, ArraySize(menu), menu[0]);
    }

/* Make getDna, getText, advFilter buttons */
    {
    hPrintf(" ");
    cgiMakeButton(getSeqPageVarName, "as sequence");
    hPrintf(" ");
    cgiMakeButton(getTextVarName, "as text");
    hPrintf(" ");
    if (gotAdvFilter())
	cgiMakeButton(advFilterVarName, "filter (now on)");
     else
	cgiMakeButton(advFilterVarName, "filter (now off)");
    }


hPrintf("</TD></TR></TABLE>");
}

#ifdef OLD
static struct genePos *genePosSimple(char *name)
/* Return a simple gene pos - with no chrom info. */
{
struct genePos *gp;
AllocVar(gp);
gp->name = cloneString(name);
gp->distance = genePosTooFar;
return gp;
}
#endif /* OLD */

#ifdef OLD
static struct genePos *genePosFull(char *name, char *chrom, int start, int end)
/* Return full gene position */
{
struct genePos *gp = genePosSimple(name);
gp->chrom = cloneString(chrom);
gp->start = start;
gp->end = end;
return gp;
}
#endif /* OLD */

static void flipDistances(struct genePos *list)
/* Subtract distance values that are set from genePosTooFar. 
 * This lets us convert a score (close == high) to a distance. */
{
struct genePos *gp;
for (gp = list; gp != NULL; gp = gp->next)
    if (gp->distance != genePosTooFar)
        gp->distance = genePosTooFar - gp->distance;
}

static void neighborhoodList(struct sqlConnection *conn, 
	char *query, struct hash *geneHash, int maxCount)
/* Fill in distance values based on query. */
{
struct sqlResult *sr;
char **row;
int count = 0;
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct hashEl *hel = hashLookup(geneHash, row[0]);
    while (hel != NULL)
        {
	struct genePos *gp = hel->val;
	gp->distance = atof(row[1]);
	hel = hashLookupNext(hel);
	++count;
	}
    if (count >= maxCount)
	break;
    }
sqlFreeResult(&sr);
}

static void getExpressionNeighbors(struct sqlConnection *conn,
	struct genePos *geneList, struct hash *geneHash, int maxCount)
/* Fill in distances for expression neighborhood. */
{
struct dyString *query = dyStringNew(1024);

/* Look for matchers.  Look for a few more than they ask for to
 * account for dupes. */
dyStringPrintf(query, 
	"select target,distance from knownExpDistance where query='%s'", 
	curGeneId->name);
neighborhoodList(conn, query->string, geneHash, maxCount);
dyStringFree(&query);
}

static void getHomologyNeighbors(struct sqlConnection *conn,
	struct genePos *geneList, struct hash *geneHash, int maxCount)
/* Fill in distances for homology neighborhood. */
{
struct dyString *query = dyStringNew(1024);
struct genePos *list;

/* Look for matchers.  Look for a few more than they ask for to
 * account for dupes. */
dyStringPrintf(query, 
	"select target,eValue from knownBlastTab where query='%s'", 
	curGeneId->name);
neighborhoodList(conn, query->string, geneHash, maxCount);
dyStringFree(&query);
}


static void getGenomicNeighbors(struct sqlConnection *conn, 
	struct genePos *curGp, struct hash *geneHash, int maxCount)
/* Get fill in distance values based on genome position. */
{
char query[256];
struct sqlResult *sr;
char **row;

/* Get list of all genes in chromosome */
safef(query, sizeof(query), 
	"select name,txStart from knownGene where chrom='%s' "
	, curGp->chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct hashEl *hel = hashLookup(geneHash, row[0]);
    while (hel != NULL)
        {
	struct genePos *gp = hel->val;
	int start = sqlUnsigned(row[1]);
	gp->distance = abs(start - curGp->start);
	hel = hashLookupNext(hel);
	}
    }
sqlFreeResult(&sr);
}


static void getPositionNeighbors(struct sqlConnection *conn, 
	struct genePos *geneList, struct hash *geneHash, int maxCount)
/* Get genes in genomic neighborhood. */
{
getGenomicNeighbors(conn, curGeneId, geneHash, maxCount);
}

#ifdef OLD
struct scoredIdName
/* A structure that stores a key, a name, and a score. */
    {
    struct scoredIdName *next;
    char *id;		/* GeneID */
    char *name;		/* Gene name. */
    int score;		/* Big is better. */
    };
#endif /* OLD */

#ifdef OLD
int scoredIdNameCmpScore(const void *va, const void *vb)
/* Compare to sort based on score. */
{
const struct scoredIdName *a = *((struct scoredIdName **)va);
const struct scoredIdName *b = *((struct scoredIdName **)vb);
int diff = b->score - a->score;
if (diff == 0)
    diff = strcmp(a->name, b->name);
return diff;
}
#endif /* OLD */

int countStartSame(char *prefix, char *name)
/* Return how many letters of prefix match first characters of name. */
{
int i;
char c;
for (i=0; ;++i)
    {
    c = prefix[i];
    if (c == 0 || c != name[i])
        break;
    }
return i;
}

#ifdef OLD
struct scoredIdName *scorePrefixMatch(char *prefix, char *id, char *name)
/* Return scoredIdName structure based on name and how many
 * characters match prefix. */
{
struct scoredIdName *sin;
AllocVar(sin);
sin->id = cloneString(id);
sin->name = cloneString(name);
sin->score = countStartSame(prefix, name);
return sin;
}
#endif /* OLD */

static void getNameborhood(struct sqlConnection *conn,
	char *curName, char *table, char *idField, char *nameField,
	struct hash *geneHash, int maxCount)
/* Fill in distance values based on how many letters match
 * current name. */
{
struct sqlResult *sr;
char **row;
char query[512];

/* Scan table for names and sort by similarity. */
safef(query, sizeof(query), "select %s,%s from %s", idField, nameField, table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct hashEl *hel = hashLookup(geneHash, row[0]);
    while (hel != NULL)
        {
	struct genePos *gp = hel->val;
	gp->distance = 1000 - countStartSame(curName,row[1]);	
	hel = hashLookupNext(hel);
	}
    }
sqlFreeResult(&sr);
}

static void getNameNeighbors(struct sqlConnection *conn,
	struct genePos *geneList, struct hash *geneHash, int maxCount)
/* Get genes in genomic neighborhood. */
{
char query[256];
char name[128];
safef(query, sizeof(query), 
	"select geneSymbol from kgXref where kgID = '%s'", curGeneId->name);
if (sqlQuickQuery(conn, query, name, sizeof(name)))
    {
    getNameborhood(conn, name, "kgXref", "kgID", "geneSymbol", 
    	geneHash, maxCount);
    }
else
    {
    warn("Couldn't find name for %s", curGeneId->name);
    }
}

#ifdef OLD
struct hash *cannonicalHash(struct sqlConnection *conn)
/* Get cannonicalHash if necessary, otherwise return NULL. */
{
if (showOnlyCannonical())
    return knownCannonicalHash(conn);
else
    return NULL;
}
#endif /* OLD */

static void trimFarGenes(struct genePos **pList)
/* Remove genes that are genePosTooFar or farther from list. */
{
struct genePos *newList = NULL, *gp, *next;
for (gp = *pList; gp != NULL; gp = next)
    {
    next = gp->next;
    if (gp->distance < genePosTooFar)
        {
	slAddHead(&newList, gp);
	}
    }
slReverse(&newList);
*pList = newList;
}

struct genePos *getNeighbors(struct column *colList, struct sqlConnection *conn)
/* Return sorted list of gene neighbors. */
{
struct genePos *geneList = advFilterResults(colList, conn);
struct hash *geneHash = newHash(16);
struct genePos *gp;

/* Make hash of all genes. */
for (gp = geneList; gp != NULL; gp = gp->next)
    hashAdd(geneHash, gp->name, gp);

if (sameString(groupOn, "expression"))
    getExpressionNeighbors(conn, geneList, geneHash, displayCount);
else if (sameString(groupOn, "homology"))
    getHomologyNeighbors(conn, geneList, geneHash, displayCount);
else if (sameString(groupOn, "position"))
    getPositionNeighbors(conn, geneList, geneHash, displayCount);
else if (sameString(groupOn, "name"))
    getNameNeighbors(conn, geneList, geneHash, displayCount);
else
    {
    warn("Unknown sort value %s", groupOn);
    getExpressionNeighbors(conn, geneList, geneHash, displayCount);
    }

if (!gotAdvFilter())
    {
    trimFarGenes(&geneList);
    }
slSort(&geneList, genePosCmpDistance);

/* Trim list to max number. */
gp = slElementFromIx(geneList, displayCount-1);
if (gp != NULL)
    gp->next = NULL;

// for (gp = geneList; gp !=NULL; gp = gp->next) uglyf("%s %f<BR>\n", gp->name, gp->distance);
return geneList;
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
    safef(varName, sizeof(varName), "%s%s", colConfigPrefix, col->name);
    val = cartOptionalString(cart, varName);
    if (val != NULL)
	col->on = sameString(val, "on");
    }
}

char *mustFindInRaHash(struct lineFile *lf, struct hash *raHash, char *name)
/* Look up in ra hash or die trying. */
{
char *val = hashFindVal(raHash, name);
if (val == NULL)
    errAbort("Missing required %s field in record ending line %d of %s",
    	name, lf->lineIx, lf->fileName);
return val;
}

void setupColumnType(struct column *col)
/* Set up methods and column-specific variables based on
 * track type. */
{
char *dupe = cloneString(col->type);	
char *s = dupe;
char *type = nextWord(&s);

if (type == NULL)
    warn("Missing type value for column %s", col->name);
if (sameString(type, "num"))
    setupColumnNum(col, s);
else if (sameString(type, "lookup"))
    setupColumnLookup(col, s);
else if (sameString(type, "acc"))
    setupColumnAcc(col, s);
else if (sameString(type, "distance"))
    setupColumnDistance(col, s);
else if (sameString(type, "knownPos"))
    setupColumnKnownPos(col, s);
else if (sameString(type, "knownDetails"))
    setupColumnKnownDetails(col, s);
else if (sameString(type, "knownName"))
    setupColumnKnownName(col, s);
else if (sameString(type, "expRatio"))
    setupColumnExpRatio(col, s);
else if (sameString(type, "swissProt"))
    setupColumnSwissProt(col, s);
else if (sameString(type, "go"))
    setupColumnGo(col, s);
else
    errAbort("Unrecognized type %s for %s", col->type, col->name);
freez(&dupe);
}


struct column *getColumns(struct sqlConnection *conn)
/* Return list of columns for big table. */
{
char *raName = "hgNearData/columnDb.ra";
struct lineFile *lf = lineFileOpen(raName, TRUE);
struct column *col, *colList = NULL;
struct hash *raHash;

while ((raHash = raNextRecord(lf)) != NULL)
    {
    AllocVar(col);
    col->name = mustFindInRaHash(lf, raHash, "name");
    col->shortLabel = mustFindInRaHash(lf, raHash, "shortLabel");
    col->longLabel = mustFindInRaHash(lf, raHash, "longLabel");
    col->priority = atof(mustFindInRaHash(lf, raHash, "priority"));
    col->on = col->defaultOn = sameString(mustFindInRaHash(lf, raHash, "visibility"), "on");
    col->type = mustFindInRaHash(lf, raHash, "type");
    col->settings = raHash;
    columnDefaultMethods(col);
    setupColumnType(col);
    if (col->exists(col, conn))
	slAddHead(&colList, col);
    }
lineFileClose(&lf);
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

struct column *findNamedColumn(struct column *colList, char *name)
/* Return column of given name from list or NULL if none. */
{
struct column *col;
for (col = colList; col != NULL; col = col->next)
    {
    if (sameString(col->name, name))
        return col;
    }
return NULL;
}

int totalHtmlColumns(struct column *colList)
/* Count up columns in big-table html. */
{
int count = 0;
struct column *col;

for (col = colList; col != NULL; col = col->next)
    {
    if (col->on)
         count += col->tableColumns(col);
    }
return count;
}

void bigTable(struct sqlConnection *conn, struct column *colList, 
	struct genePos *geneList)
/* Put up great big table. */
{
struct column *col;
struct genePos *gene;

if (geneList == NULL)
    return;
hPrintf("<TABLE BORDER=1 CELLSPACING=1 CELLPADDING=1 COLS=%d>\n", 
	totalHtmlColumns(colList));

/* Print label row. */
hPrintf("<TR BGCOLOR=\"#E0E0FF\">");
for (col = colList; col != NULL; col = col->next)
    {
    char *colName = col->shortLabel;
    if (col->on)
	{
	col->labelPrint(col);
	}
    }
hPrintf("</TR>\n");

/* Print other rows. */
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    if (sameString(gene->name, curGeneId->name))
        hPrintf("<TR BGCOLOR=\"#D0FFD0\">");
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
		col->cellPrint(col,gene,conn);
	    }
	}
    hPrintf("</TR>\n");
    }

hPrintf("</TABLE>");
}

void doGetText(struct sqlConnection *conn, struct column *colList, 
	struct genePos *geneList)
/* Put up great big table. */
{
struct genePos *gene;
struct column *col;
boolean first = TRUE;

if (geneList == NULL)
    {
    hPrintf("empty table");
    return;
    }
hPrintf("<TT><PRE>");

/* Print labels. */
hPrintf("#");
for (col = colList; col != NULL; col = col->next)
    {
    if (first)
	first = FALSE;
    else
	hPrintf("\t");
    if (col->on)
	hPrintf("%s", col->name);
    }
hPrintf("\n");
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    first = TRUE;
    for (col = colList; col != NULL; col = col->next)
	{
	if (col->on)
	    {
	    char *val = col->cellVal(col, gene, conn);
	    if (first)
	        first = FALSE;
	    else
		hPrintf("\t");
	    if (val == NULL)
		hPrintf("n/a", val);
	    else
		hPrintf("%s", val);
	    freez(&val);
	    }
	}
    hPrintf("\n");
    }
hPrintf("</PRE></TT>");
}

void doMainDisplay(struct sqlConnection *conn, struct column *colList, 
	struct genePos *geneList)
/* Put up the main family browser display - a control panel followed by 
 * a big table. */
{
char buf[128];
safef(buf, sizeof(buf), "UCSC %s Gene Family Browser", organism);
makeTitle(buf, "hgNear.html");
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=GET>\n");
cartSaveSession(cart);
controlPanel(curGeneId);
if (geneList != NULL)
    bigTable(conn, colList,geneList);
hPrintf("</FORM>");
}

void displayData(struct sqlConnection *conn, struct column *colList, 
	struct genePos *gp)
/* Display data in neighborhood of gene. */
{
struct genePos *geneList = NULL;
curGeneId = gp;
if (gp)
    geneList = getNeighbors(colList, conn);
if (cartVarExists(cart, getTextVarName))
    doGetText(conn, colList, geneList);
else if (cartVarExists(cart, getSeqVarName))
    doGetSeq(conn, colList, geneList, cartString(cart, getSeqHowVarName));
else if (cartVarExists(cart, getGenomicSeqVarName))
    doGetGenomicSeq(conn, colList, geneList);
else
    doMainDisplay(conn, colList, geneList);
}

struct genePos *curGenePos()
/* Return current gene pos from cart variables. */
{
struct genePos *gp;
AllocVar(gp);
gp->name = cloneString(cartString(cart, idVarName));
if (cartVarExists(cart, idPosVarName))
    {
    hgParseChromRange(cartString(cart, idPosVarName),
    	&gp->chrom, &gp->start, &gp->end);
    gp->chrom = cloneString(gp->chrom);
    }
return gp;
}

void doFixedId(struct sqlConnection *conn, struct column *colList)
/* Put up the main page based on id/idPos. */
{
displayData(conn, colList, curGenePos());
}


void doExamples(struct sqlConnection *conn, struct column *colList)
/* Put up controls and then some helpful text and examples.
 * Called when search box is empty. */
{
displayData(conn, colList, NULL);
htmlHorizontalLine();
hPrintf("%s",
 "<P>This program displays a table of genes that are related to "
 "each other.  The relationship can be of several types including "
 "protein level homology, similarity of gene expression profiles, or "
 "genomic proximity.  The 'group by' drop-down controls "
 "which type of relationship is used.</P>"
 "<P>To use this tool please type something into the search column and "
 "hit the 'Go' button. You can search for many types of things including "
 "the gene name, the SwissProt protein name, a word or phrase "
 "that occurs in the description of a gene, or a GenBank mRNA accession. "
 "Some examples of search terms are 'FOXA1' 'HOXA9' and 'MAP kinase.' </P>"
 "<P>After the search a table appears containing the gene and it's relatives, "
 "one gene per row.  The gene matching the search will be hilighted in light "
 "green.  In the case of search by genome position, the hilighted gene will "
 "be in the middle of the table. In other cases it will be the first row. "
 "Some of the columns in the table including the BLAST 'E-value' and "
 "'%ID' columns will be calculated relative to the hilighted gene.  You can "
 "select a different gene in the list by clicking on the gene's name. "
 "Clicking on the 'Genome Position' will open the Genome Browser on that "
 "gene.  Clicking on the 'Description' will open a details page on the "
 "gene.</P>"
 "<P>To control which columns are displayed in the table use the 'configure' "
 "button. To control the number of rows displayed use the 'display' drop "
 "down. The 'as sequence' button will fetch protein, mRNA, promoter, or "
 "genomic sequence associated with the genes in the table.  The 'as text' "
 "button fetches the table in a simple tab-delimited format suitable for "
 "import into a spreadsheet or relational database. The advanced filter "
 "button allows you to select which genes are displayed in the table "
 "in a very detailed and flexible fashion.</P>"
 "<P>The UCSC Gene Family Browser was designed and implemented by Jim Kent, "
 "Fan Hsu, David Haussler, and the UCSC Genome Bioinformatics Group. This "
 "work is supported by a grant from the National Human Genome Research "
 "Institute and by the Howard Hughes Medical Institute.</P>"
 );
}

static char *colHtmlFileName(struct column *col)
/* Return html file associated with column.  You can
 * freeMem this when done. */
{
char name[PATH_LEN];
safef(name, sizeof(name), "hgNearData/%s.html", col->name);
return cloneString(name);
}

void doColInfo(struct sqlConnection *conn, struct column *colList,
	char *colName)
/* Put up info page on column. */
{
struct column *col = findNamedColumn(colList, colName);
char *htmlFileName = colHtmlFileName(col);
if (col == NULL)
    errAbort("Can't find column '%s'", colName);
hPrintf("<H2>Column %s - %s</H2>\n", col->shortLabel, col->longLabel);
if (fileExists(htmlFileName))
    {
    char *raw, *cooked;
    struct subText *subs = NULL;
    hAddDbSubVars("", database, &subs);
    readInGulp(htmlFileName, &raw, NULL);
    cooked = subTextString(subs, raw);
    hPrintf("%s", cooked);
    freez(&cooked);
    freez(&raw);
    subTextFreeList(&subs);
    }
else
    {
    hPrintf("No additional info available on %s column", col->shortLabel);
    }
freeMem(htmlFileName);
}


void doMiddle(struct cart *theCart)
/* Write the middle parts of the HTML page. 
 * This routine sets up some globals and then
 * dispatches to the appropriate page-maker. */
{
char *var = NULL;
struct sqlConnection *conn;
struct column *colList, *col;
cart = theCart;

#ifdef SOON
getDbAndGenome(cart, &database, &organism);
#else
database = "hg15";
cartSetString(cart, "db", database);
organism = "Human";
#endif /* SOON */
hSetDb(database);
conn = hAllocConn();

/* Get groupOn.  Revert to default if no advanced filter. */
groupOn = cartUsualString(cart, groupVarName, "expression");

displayCountString = cartUsualString(cart, countVarName, "50");
if (sameString(displayCountString, "all")) 
    displayCount = BIGNUM;
else
    displayCount = atoi(displayCountString);
colList = getColumns(conn);
if (cartVarExists(cart, confVarName))
    doConfigure(conn, colList, NULL);
else if (cartVarExists(cart, colInfoVarName))
    doColInfo(conn, colList, cartString(cart, colInfoVarName));
else if ((var = cartFindFirstLike(cart, "near.up.*")) != NULL)
    {
    doConfigure(conn, colList, var);
    cartRemovePrefix(cart, "near.up.");
    }
else if ((var = cartFindFirstLike(cart, "near.down.*")) != NULL)
    {
    doConfigure(conn, colList, var);
    cartRemovePrefix(cart, "near.down.");
    }
else if (cartVarExists(cart, defaultConfName))
    doDefaultConfigure(conn, colList);
else if (cartVarExists(cart, hideAllConfName))
    doConfigHideAll(conn, colList);
else if (cartVarExists(cart, showAllConfName))
    doConfigShowAll(conn, colList);

else if (cartVarExists(cart, saveCurrentConfName))
    doNameCurrentColumns();
else if (cartVarExists(cart, savedCurrentConfName))
    doSaveCurrentColumns(conn, colList);
else if (cartVarExists(cart, useSavedConfName))
    doConfigUseSaved(conn, colList);

else if (cartVarExists(cart, filSaveCurrentVarName))
    doNameCurrentFilters();
else if (cartVarExists(cart, filSavedCurrentVarName))
    doSaveCurrentFilters(conn, colList);
else if (cartVarExists(cart, filUseSavedVarName))
    doUseSavedFilters(conn, colList);

else if (cartVarExists(cart, advFilterVarName))
    doAdvFilter(conn, colList);
else if (cartVarExists(cart, advFilterClearVarName))
    doAdvFilterClear(conn, colList);
else if (cartVarExists(cart, advFilterBrowseVarName))
    doAdvFilterBrowse(conn, colList);
else if (cartVarExists(cart, advFilterListVarName))
    doAdvFilterList(conn, colList);
else if (cartVarExists(cart, getSeqPageVarName))
    doGetSeqPage(conn, colList);
else if (cartVarExists(cart, idVarName))
    doFixedId(conn, colList);
else if ((col = advFilterKeyPastePressed(colList)) != NULL)
    doAdvFilterKeyPaste(conn, colList, col);
else if ((col = advFilterKeyPastedPressed(colList)) != NULL)
    doAdvFilterKeyPasted(conn, colList, col);
else if ((col = advFilterKeyUploadPressed(colList)) != NULL)
    doAdvFilterKeyUpload(conn, colList, col);
else if ((col = advFilterKeyClearPressed(colList)) != NULL)
    doAdvFilterKeyClear(conn, colList, col);
else if (cartNonemptyString(cart, searchVarName))
    doSearch(conn, colList);
else
    doExamples(conn, colList);
hFreeConn(&conn);
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
