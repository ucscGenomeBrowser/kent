/* hgNear - gene sorter. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "obscure.h"
#include "portable.h"
#include "cheapcgi.h"
#include "memalloc.h"
#include "jksql.h"
#include "htmshell.h"
#include "subText.h"
#include "cart.h"
#include "dbDb.h"
#include "hdb.h"
#include "hui.h"
#include "web.h"
#include "ra.h"
#include "hgColors.h"
#include "hgNear.h"
#include "versionInfo.h"
#include "hPrint.h"

static char const rcsid[] = "$Id: hgNear.c,v 1.179.6.1 2008/07/31 02:24:44 markd Exp $";

char *excludeVars[] = { "submit", "Submit", idPosVarName, NULL }; 
/* The excludeVars are not saved to the cart. (We also exclude
 * any variables that start "near.do.") */

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
struct hash *oldVars = NULL; /* The cart vars before new cgi stuff added. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
char *genome;		/* Name of genome - mouse, human, etc. */
char *groupOn;		/* Current grouping strategy. */
int displayCount;	/* Number of items to display. */
char *displayCountString; /* Ascii version of display count, including 'all'. */
struct hash *genomeSettings;  /* Genome-specific settings from settings.ra. */
struct hash *columnHash;  /* Hash of active columns keyed by name. */

struct genePos *curGeneId;	  /* Identity of current gene. */
int  kgVersion = KG_UNKNOWN;      /* KG version */

/* ---- General purpose helper routines. ---- */

int genePosCmpName(const void *va, const void *vb)
/* Sort function to compare two genePos by name. */
{
const struct genePos *a = *((struct genePos **)va);
const struct genePos *b = *((struct genePos **)vb);
return strcmp(a->name, b->name);
}

int genePosCmpPos(const void *va, const void *vb)
/* Sort function to compare two genePos by chrom,start. */
{
const struct genePos *a = *((struct genePos **)va);
const struct genePos *b = *((struct genePos **)vb);
int diff =  strcmp(a->chrom, b->chrom);
if (diff == 0)
    diff = a->start - b->start;
return diff;
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
    {
    return strcmp(a->protein, b->protein);
    }
}

boolean anyWild(char *s)
/* Return TRUE if there are '?' or '*' characters in s. */
{
return strchr(s, '?') != NULL || strchr(s, '*') != NULL;
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

boolean anyRealInCart(struct cart *cart, char *wild)
/* Return TRUE if variables are set matching wildcard. */
{
struct hashEl *varList = NULL, *var;
boolean ret = FALSE;

varList = cartFindLike(cart, wild);
for (var = varList; var != NULL; var = var->next)
    {
    char *s = var->val;
    if (s != NULL)
	{
	s = trimSpaces(s);
	if (s[0] != 0)
	    {
	    ret = TRUE;
	    break;
	    }
	}
    }
hashElFreeList(&varList);
return ret;
}

void fixSafariSpaceInQuotes(char *s)
/* Safari on the Mac changes a space (ascii 32) to a
 * ascii 160 if it's inside of a single-quote in a
 * text input box!?  This tuns it back to a 32. */
{
unsigned char c;
while ((c = *s) != 0)
    {
    if (c == 160)
        *s = 32;
    ++s;
    }
}

void makeTitle(char *title, char *helpName)
/* Make title bar. */
{
hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#"HG_COL_HOTLINKS"\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"2\"><TR>\n");
hPrintf("<TD ALIGN=LEFT><A HREF=\"../index.html\">%s</A></TD>", wrapWhiteFont("Home"));
hPrintf("<TD ALIGN=CENTER><FONT COLOR=\"#FFFFFF\" SIZE=4>%s</FONT></TD>", title);
hPrintf("<TD ALIGN=Right><A HREF=\"../goldenPath/help/%s\">%s</A></TD>", 
	helpName, wrapWhiteFont("Help"));
hPrintf("</TR></TABLE>");
}

/* ---- Some helper routines for order methods. ---- */


char *orderSetting(struct order *ord, char *name, char *defaultVal)
/* Return value of named setting in order, or default if it doesn't exist. */
{
char *result = hashFindVal(ord->settings, name);
if (result == NULL)
    result = defaultVal;
return result;
}

char *orderRequiredSetting(struct order *ord, char *name)
/* Return value of named setting.  Abort if it doesn't exist. */
{
char *result = hashFindVal(ord->settings, name);
if (result == NULL)
    errAbort("Missing required %s field in %s record of orderDb.ra",
    	name, ord->name);
return result;
}

int orderIntSetting(struct order *ord, char *name, int defaultVal)
/* Return value of named integer setting or default if it doesn't exist. */
{
char *result = hashFindVal(ord->settings, name);
if (result == NULL)
    return defaultVal;
return atoi(result);
}

boolean orderSettingExists(struct order *ord, char *name)
/* Return TRUE if setting exists in column. */
{
return hashFindVal(ord->settings, name) != NULL;
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

char *columnRequiredSetting(struct column *col, char *name)
/* Return value of named setting.  Abort if it doesn't exist. */
{
char *result = hashFindVal(col->settings, name);
if (result == NULL)
    errAbort("Missing required %s field in %s record of columnDb.ra",
    	name, col->name);
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

struct sqlConnection *hgFixedConn()
/* Return connection to hgFixed database. 
 * This is effectively a global, but not
 * opened until needed. */
{
static struct sqlConnection *conn = NULL;
if (conn == NULL)
    conn = sqlConnect("hgFixed");
return conn;
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
return findNamedColumn(colName);
}

static char *keyFileName(struct column *col)
/* Return key file name for this column.  Return
 * NULL if no key file. */
{
char *fileName = advFilterVal(col, "keyFile");
if (fileName == NULL)
    return NULL;
if (!fileExists(fileName))
    {
    cartRemove(cart, advFilterName(col, "keyFile"));
    return NULL;
    }
return fileName;
}

struct slName *keyFileList(struct column *col)
/* Make up list from key file for this column.
 * return NULL if no key file. */
{
char *fileName = keyFileName(col);
char *buf;
struct slName *list;

if (fileName == NULL)
    return NULL;
readInGulp(fileName, &buf, NULL);
list = stringToSlNames(buf);
freez(&buf);
return list;
}

static struct hash *upcHashWordsInFile(char *fileName, int hashSize)
/* Create a hash of space delimited uppercased words in file. */
{
struct hash *hash = newHash(hashSize);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;
while (lineFileNext(lf, &line, NULL))
    {
    while ((word = nextWord(&line)) != NULL)
	{
	touppers(word);
        hashAdd(hash, word, NULL);
	}
    }
lineFileClose(&lf);
return hash;
}

struct hash *keyFileHash(struct column *col)
/* Make up a hash from key file for this column. 
 * Return NULL if no key file. */
{
char *fileName = keyFileName(col);
if (fileName == NULL)
    return NULL;
return upcHashWordsInFile(fileName, 16);
}

char *cellLookupVal(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Get a field in a table defined by col->table, col->keyField, 
 * col->valField.  If an xrefLookup is specified in col->settings,
 * use that to look up an alternate name for the result. */
{
char *xrefDb = hashFindVal(col->settings, "xrefDb");
char *xrefTable = hashFindVal(col->settings, "xrefTable");
char *xrefNameField = hashFindVal(col->settings, "xrefNameField");
char *xrefAliasField = hashFindVal(col->settings, "xrefAliasField");
char query[1024];
if (xrefDb)
    safef(query, sizeof(query),
	  "select %s.%s.%s "
	  "from %s.%s, %s "
	  "where %s.%s = '%s' "
	  "and %s.%s = %s.%s.%s;",
	  xrefDb, xrefTable, xrefAliasField,
	  xrefDb, xrefTable,   col->table,
	  col->table, col->keyField,   gp->name,
	  col->table, col->valField,   xrefDb, xrefTable, xrefNameField);
else
    safef(query, sizeof(query), "select %s from %s where %s = '%s'",
	  col->valField, col->table, col->keyField, gp->name);
return sqlQuickString(conn, query);
}

char *lookupItemUrlVal(struct column *col, char *sVal,
        struct sqlConnection *conn)
{
char query[512];
safef(query, sizeof(query), col->itemUrlQuery, sVal);
return sqlQuickString(conn, query);
}

void cellSimplePrintExt(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn, boolean lookupForUrl)
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
    if (col->selfLink)
        {
	selfAnchorId(gp);
	hPrintNonBreak(s);
        hPrintf("</A>");
	}
    else if (col->itemUrl != NULL)
        {
	hPrintf("<A HREF=\"");
	char *sVal = gp->name;
	if (lookupForUrl)
	    sVal = s;
	if (col->itemUrlQuery)
	    sVal = lookupItemUrlVal(col, sVal, conn);
	hPrintf(col->itemUrl, sVal);
	if (col->itemUrlQuery)
	    freez(&sVal);
	if (col->useHgsid)
	    hPrintf("&%s", cartSidUrlString(cart));
	if (col->urlChromVar)
	    hPrintf("&%s=%s", col->urlChromVar, gp->chrom);
	if (col->urlStartVar)
	    hPrintf("&%s=%d", col->urlStartVar, gp->start);
	if (col->urlEndVar)
	    hPrintf("&%s=%d", col->urlEndVar, gp->end);
	if (col->urlOtherGeneVar)
	    hPrintf("&%s=%s", col->urlOtherGeneVar, curGeneId->name);
	hPrintf("\"");
	if (!col->useHgsid)
	    hPrintf(" TARGET=_blank");
	hPrintf(">");
	hPrintNonBreak(s);
        hPrintf("</A>");
	}
    else
        {
	hPrintNonBreak(s);
	}
    freeMem(s);
    }
hPrintf("</TD>");
}

void cellSimplePrint(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* This just prints one field from table. */
{
cellSimplePrintExt(col, gp, conn, TRUE);
}

void cellSimplePrintNoLookupUrl(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* This just prints one field from table using gp->name for
 * itemUrl. */
{
cellSimplePrintExt(col, gp, conn, FALSE);
}

static void hPrintSpaces(int count)
/* Print count number of spaces. */
{
while (--count >= 0)
    hPrintf(" ");
}

char *colInfoUrl(struct column *col)
/* Return URL to get column info.  freeMem this when done. */
{
char *labelUrl;
if ((labelUrl = columnSetting(col, "labelUrl", NULL)) != NULL)
    return labelUrl;
else
    {
    char url[512];
    safef(url, sizeof(url), "../cgi-bin/hgNear?%s&%s=%s",
	    cartSidUrlString(cart), colInfoVarName, col->name);
    return cloneString(url);
    }
}

void colInfoAnchor(struct column *col)
/* Print anchor tag that leads to column info page. */
{
char *url = colInfoUrl(col);
hPrintf("<A HREF=\"%s\">", url);
freeMem(url);
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

hPrintf("<TH ALIGN=LEFT VALIGN=BOTTOM><B><PRE>");
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
    colInfoLink(col);
    hPrintSpaces(diff);
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

/* ---- Debugging column ---- */

static boolean debugExists(struct column *col, struct sqlConnection *conn)
/* Return TRUE if we are on hgwdev. */
{
char *host = getHost();
return startsWith("hgwdev", host);
}

char *debugCellVal(struct column *col, struct genePos *gp, 
	struct sqlConnection *conn)
/* Return value for debugging column. */
{
char buf[32];
safef(buf, sizeof(buf), "%f", gp->distance);
return cloneString(buf);
}

void debugCellPrint(struct column *col, struct genePos *gp,
	struct sqlConnection *conn)
/* Print value including favorite hyperlink in debug column. */
{
hPrintf("<TD><A HREF=\"../cgi-bin/hgc?%s&g=knownGene&i=%s&c=%s&l=%d&r=%d\">",
	cartSidUrlString(cart), gp->name, gp->chrom, gp->start, gp->end);
hPrintf("%f", gp->distance);
hPrintf("</A></TD>");
}

void setupColumnDebug(struct column *col, char *parameters)
/* Set up a column that displays the geneId (accession) */
{
col->exists = debugExists;
col->cellVal = debugCellVal;
col->cellPrint = debugCellPrint;
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
	if (hashLookupUpperCase(keyHash, gp->name))
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
hashFree(&keyHash);
return list;
}


void setupColumnAcc(struct column *col, char *parameters)
/* Set up a column that displays the geneId (accession) */
{
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
col->cellVal = numberVal;
col->cellPrint = cellSelfLinkPrint;
}

/* ---- Simple table lookup type columns ---- */


struct searchResult *lookupTypeSimpleSearch(struct column *col, 
    struct sqlConnection *conn, char *search)
/* Search lookup type column. */
{
struct dyString *query = dyStringNew(512);
char *searchHow = columnSetting(col, "search", "exact");
struct sqlConnection *searchConn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct searchResult *resList = NULL, *res;

dyStringPrintf(query, "select %s,%s from %s where %s ", 
	col->keyField, col->valField, col->table, col->valField);
if (sameString(searchHow, "fuzzy"))
    dyStringPrintf(query, "like '%%%s%%'", search);
else if (sameString(searchHow, "prefix"))
    dyStringPrintf(query, "like '%s%%'", search);
else
    dyStringPrintf(query, " = '%s'", search);
sr = sqlGetResult(searchConn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(res);
    res->gp.name = cloneString(row[0]);
    if (!sameString(searchHow, "fuzzy"))
	res->matchingId = cloneString(row[1]);
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
/* Print out controls for advanced filter on lookup column. */
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
    hPrintf("Limit to items (no wildcards) in list: ");
    advFilterKeyPasteButton(col);
    hPrintf(" ");
    advFilterKeyUploadButton(col);
    hPrintf(" ");
    if (fileName != NULL)
	{
	if (fileExists(fileName))
	    {
	    int count = countWordsInFile(fileName);
	    advFilterKeyClearButton(col);
	    hPrintf("<BR>\n");
	    if (count == 1)
		hPrintf("(There is currently 1 item in the list.)");
	    else
		hPrintf("(There are currently %d items in the list.)", count);
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
	if (keyHash == NULL || hashLookupUpperCase(keyHash, row[1]))
	    {
	    if (wildList == NULL || wildMatchList(row[1], wildList, orLogic))
		hashAdd(hash, row[0], NULL);
	    }
	}
    list = weedUnlessInHash(list, hash);
    sqlFreeResult(&sr);
    hashFree(&hash);
    }
hashFree(&keyHash);
return list;
}

void setupColumnLookup(struct column *col, char *parameters)
/* Set up column that just looks up one field in a table
 * keyed by the geneId. */
{
char *xrefLookup = cloneString(hashFindVal(col->settings, "xrefLookup"));
col->table = cloneString(nextWord(&parameters));
col->keyField = cloneString(nextWord(&parameters));
col->valField = cloneString(nextWord(&parameters));
if (col->valField == NULL)
    errAbort("Not enough fields in type lookup for %s", col->name);
col->exists = simpleTableExists;
col->cellVal = cellLookupVal;
if (columnSetting(col, "search", NULL))
    col->simpleSearch = lookupTypeSimpleSearch;
col->filterControls = lookupAdvFilterControls;
col->advFilter = lookupAdvFilter;
if (isNotEmpty(xrefLookup))
    {
    char *xrefTable = nextWord(&xrefLookup);
    char *xrefNameField = nextWord(&xrefLookup);
    char *xrefAliasField = nextWord(&xrefLookup);
    if (isNotEmpty(xrefAliasField))
	{
	char *xrefOrg = hashFindVal(col->settings, "xrefOrg");
	char *xrefDb;
	if (xrefOrg)
	    xrefDb = hDefaultDbForGenome(xrefOrg);
	else
	    xrefDb = cloneString(database);
	struct sqlConnection *xrefConn = hAllocConn(xrefDb);
	if (sqlTableExists(xrefConn, xrefTable))
	    {
	    /* These are the column settings that will be used by 
	     * cellLookupVal, so it doesn't have to parse xrefLookup and 
	     * query for table existence for each cell. */
	    hashAdd(col->settings, "xrefDb", xrefDb);
	    hashAdd(col->settings, "xrefTable", xrefTable);
	    hashAdd(col->settings, "xrefNameField", xrefNameField);
	    hashAdd(col->settings, "xrefAliasField", xrefAliasField);
	    }
	hFreeConn(&xrefConn);
	}
    }
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

void minMaxAdvFilterControls(struct column *col, struct sqlConnection *conn)
/* Print out controls for min/max advanced filter. */
{
hPrintf("minimum: ");
advFilterRemakeTextVar(col, "min", 8);
hPrintf(" maximum: ");
advFilterRemakeTextVar(col, "max", 8);
}

struct genePos *advFilterFromQuery(struct sqlConnection *conn, char *query,
	struct genePos *list)
/* Return list of genes from list that are returned by query */
{
struct hash *passHash = newHash(16);  /* Hash of genes that pass. */
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(passHash, row[0], NULL);
list = weedUnlessInHash(list, passHash);
sqlFreeResult(&sr);
hashFree(&passHash);
return list;
}

struct genePos *distanceAdvFilter(struct column *col, 
	struct sqlConnection *conn, struct genePos *list)
/* Do advanced filter on distance type. */
{
char *minString = advFilterVal(col, "min");
char *maxString = advFilterVal(col, "max");
char *name = NULL;

if (curGeneId)
    {
    name = curGeneId->name;
    }
else
    {
    name = cartString(cart, searchVarName);
    }

if (minString != NULL || maxString != NULL)
    {
    struct dyString *dy = newDyString(512);
    dyStringPrintf(dy, "select %s from %s where", col->keyField, col->table);
    dyStringPrintf(dy, " %s='%s'", col->curGeneField, name);
    if (minString)
         dyStringPrintf(dy, " and %s >= %s", col->valField, minString);
    if (maxString)
         dyStringPrintf(dy, " and %s <= %s", col->valField, maxString);
    list = advFilterFromQuery(conn, dy->string, list);
    dyStringFree(&dy);
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
col->cellPrint = cellSimplePrintNoLookupUrl;
col->filterControls = minMaxAdvFilterControls;
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

/* ---------- Lookup floating point number column ------------- */

struct genePos *floatAdvFilter(struct column *col, 
	struct sqlConnection *conn, struct genePos *list)
/* Do advanced filter on float type. */
{
char *minString = advFilterVal(col, "min");
char *maxString = advFilterVal(col, "max");
if (minString != NULL || maxString != NULL)
    {
    struct dyString *dy = newDyString(512);
    dyStringPrintf(dy, "select %s from %s where ", col->keyField, col->table);
    if (minString && maxString)
       dyStringPrintf(dy, "%s >= %s and %s <= %s",
       		col->valField, minString, col->valField, maxString);
    else if (minString)
       dyStringPrintf(dy, "%s >= %s", col->valField, minString);
    else
       dyStringPrintf(dy, "%s <= %s", col->valField, maxString);
    list = advFilterFromQuery(conn, dy->string, list);
    dyStringFree(&dy);
    }
return list;
}


void setupColumnFloat(struct column *col, char *parameters)
/* Set up column that just looks up one floating point field
 * in a table keyed by the geneId. */
{
setupColumnLookup(col, parameters);
col->simpleSearch = NULL;
col->cellPrint = cellSimplePrintNoLookupUrl;
col->filterControls = minMaxAdvFilterControls;
col->advFilter = floatAdvFilter;
}

#ifdef OLD
void setupColumnFloat(struct column *col, char *parameters)
/* Set up column that looks up one numerical field in a table
 * keyed by the geneId. */
{
col->table = cloneString(nextWord(&parameters));
col->keyField = cloneString(nextWord(&parameters));
col->valField = cloneString(nextWord(&parameters));
if (col->valField == NULL)
    errAbort("Not enough fields in type float for %s", col->name);
col->exists = simpleTableExists;
col->cellVal = cellLookupVal;
col->cellPrint = cellSimplePrintNoLookupUrl;
col->filterControls = minMaxAdvFilterControls;
col->advFilter = floatAdvFilter;
}
#endif /* OLD */


/* ---- Page/Form Making stuff ---- */

static void makeGenomeAssemblyControls()
/* Query database to figure out which ones
 * support gene sorter. */
{
/* Make up a list of genome that have a gene
 * sorter, and a list of assemblies that have a
 * gene sorter for the current genome. */
struct slRef *as, *asList = NULL;
struct slRef *org, *orgList = NULL;
struct dbDb *db, *dbList = hGetIndexedDatabases();
char *ourOrg = hGenome(database);
struct hash *orgHash = newHash(8);
for (db = dbList; db != NULL; db = db->next)
    {
    if (db->hgNearOk)
	{
	if (!hashLookup(orgHash, db->genome))
	    {
	    hashAdd(orgHash, db->genome, db);
	    refAdd(&orgList, db);
	    }
	if (sameString(ourOrg, db->genome))
	    refAdd(&asList, db);
        }
    }
slReverse(&asList);
slReverse(&orgList);

/* Make genome drop-down. */
hPrintf("genome ");
hPrintf("<SELECT NAME=\"%s\" ", orgVarName);
hPrintf("onchange='%s'",
  "document.orgForm.org.value=document.mainForm.org.options[document.mainForm.org.selectedIndex].value;"
  "document.orgForm.db.value=0;"
  "document.orgForm.submit();");
hPrintf(">\n");
for (org = orgList; org != NULL; org = org->next)
    {
    struct dbDb *db = org->val;
    char *genome = db->genome;
    hPrintf("<OPTION VALUE=\"%s\"", genome);
    if (sameString(ourOrg, genome))
	hPrintf(" SELECTED");
    hPrintf(">%s\n", genome);
    }
hPrintf("</SELECT>");

/* Make assembly drop-down. */
hPrintf(" assembly ");
hPrintf("<SELECT NAME=\"%s\" ", dbVarName);
hPrintf("onchange='%s'",
  "document.orgForm.db.value = document.mainForm.db.options[document.mainForm.db.selectedIndex].value;"
  "document.orgForm.submit();");
hPrintf(">\n");
for (as = asList; as != NULL; as = as->next)
    {
    struct dbDb *db = as->val;
    hPrintf("<OPTION VALUE=\"%s\"", db->name);
    if (sameString(database, db->name))
	hPrintf(" SELECTED");
    hPrintf(">%s\n", db->description);
    }
hPrintf("</SELECT>");

/* Clean up time. */
slFreeList(&asList);
slFreeList(&orgList);
hashFree(&orgHash);
dbDbFree(&dbList);
}

void controlPanelStart()
/* Put up start of tables around a control panel. */
{
hPrintf("<TABLE WIDTH=\"100%%\" BORDER=0 CELLSPACING=0 CELLPADDING=4><TR><TD ALIGN=CENTER>");
hPrintf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0 BGCOLOR=\"#"HG_COL_BORDER"\"><TR><TD>");
hPrintf("<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=2 BGCOLOR=\""HG_COL_INSIDE"\"><TR><TD>\n");
hPrintf("<TABLE BORDER=0 CELLSPACING=1 CELLPADDING=1><TR><TD>");
}

void controlPanelEnd()
/* Put up end of tables around a control panel. */
{
hPrintf("</TD></TR></TABLE>");
hPrintf("</TD></TR></TABLE>");
hPrintf("</TD></TR></TABLE>");
hPrintf("</TD></TR></TABLE>");
}

static void mainControlPanel(struct genePos *gp, 
	struct order *curOrd, struct order *ordList)
/* Make control panel. */
{
controlPanelStart();

makeGenomeAssemblyControls();

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
    printf("<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\">", "submit", "Go!");
    }

hPrintf("</TD></TR>\n<TR><TD>");


/* Do sort by drop-down */
    {
    struct order *ord;

    hPrintf("<A HREF=\"");
    hPrintf("../cgi-bin/hgNear?%s=on&%s", 
    	orderInfoDoName, cartSidUrlString(cart));
    hPrintf("\">");
    hPrintf("sort by");
    hPrintf("</A> ");
    hPrintf("<SELECT NAME=\"%s\"", orderVarName);
    hPrintf(" onchange=\""
	"document.orgForm.%s.value = document.mainForm.%s.options[document.mainForm.%s.selectedIndex].value;"
      	"document.orgForm.submit();"
	"\"", 
	orderVarName,
	orderVarName,
	orderVarName);
    hPrintf(">\n");
    for (ord = ordList; ord != NULL; ord = ord->next)
        {
	hPrintf("<OPTION VALUE=\"%s\"", ord->name);
	if (ord == curOrd)
	    hPrintf(" SELECTED");
	hPrintf(">%s\n", ord->shortLabel);
	}
    hPrintf("</SELECT>\n");
    }

/* advFilter, configure buttons */
    {
    cgiMakeButton(confVarName, "configure");
    hPrintf(" ");
    if (gotAdvFilter())
	cgiMakeButton(advFilterVarName, "filter (now on)");
     else
	cgiMakeButton(advFilterVarName, "filter (now off)");
    hPrintf(" ");
    }

/* Do items to display drop-down */
    {
    static char *menu[] = {"25", "50", "100", "200", "500", "1000", "all"};
    hPrintf(" display ");
    cgiMakeDropList(countVarName, menu, ArraySize(menu), displayCountString);
    }


/* Make getDna, getText buttons */ 
    {
    hPrintf(" output ");
    cgiMakeOptionalButton(getSeqPageVarName, "sequence", gp == NULL);
    hPrintf(" ");
    cgiMakeOptionalButton(getTextVarName, "text", gp == NULL);
    }
controlPanelEnd();
}

static void trimFarGenes(struct genePos **pList)
/* Remove genes that are genePosTooFar or farther from list. */
{
struct genePos *newList = NULL, *gp, *next;
for (gp = *pList; gp != NULL; gp = next)
    {
    next = gp->next;
    if (gp->distance < genePosTooFar || sameString(gp->name, curGeneId->name))
        {
	slAddHead(&newList, gp);
	}
    }
slReverse(&newList);
*pList = newList;
}

struct genePos *getOrderedList(struct order *ord,
	struct column *colList, struct sqlConnection *conn, 
	int maxCount)
/* Return sorted list of gene neighbors. */
{
struct genePos *geneList = advFilterResults(colList, conn);
struct hash *geneHash = newHash(16);
struct genePos *gp;

/* Make hash of all genes. */
for (gp = geneList; gp != NULL; gp = gp->next)
    {
    hashAdd(geneHash, gp->name, gp);
    }

/* Calculate distances, trim unset distances, and sort. */
if (ord->calcDistances)
    ord->calcDistances(ord, conn, &geneList, geneHash, maxCount);
if (!gotAdvFilter())
    trimFarGenes(&geneList);
slSort(&geneList, genePosCmpDistance);

/* Trim list to max number. */
gp = slElementFromIx(geneList, maxCount-1);
if (gp != NULL)
    gp->next = NULL;

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

void refinePriorities(struct hash *colHash)
/* Consult colOrderVar if it exists to reorder priorities. */
{
char *orig = cartOptionalString(cart, colOrderVar);
if (orig != NULL)
    {
    char *dupe = cloneString(orig);
    char *s = dupe;
    char *name, *val;
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
    safef(varName, sizeof(varName), "%s%s.vis", colConfigPrefix, col->name);
    val = cartOptionalString(cart, varName);
    if (val != NULL)
	col->on = sameString(val, "1");
    }
}

char *mustFindInRaHash(char *fileName, struct hash *raHash, char *name)
/* Look up in ra hash or die trying. */
{
char *val = hashFindVal(raHash, name);
if (val == NULL)
    errAbort("Missing required %s field in %s", name, fileName);
return val;
}

void setupColumnType(struct column *col)
/* Set up methods and column-specific variables based on
 * track type. */
{
char *dupe = cloneString(col->type);	
char *s = dupe;
char *type = nextWord(&s);

columnDefaultMethods(col);
if (type == NULL)
    warn("Missing type value for column %s", col->name);
if (sameString(type, "num"))
    setupColumnNum(col, s);
else if (sameString(type, "debug"))
    setupColumnDebug(col, s);
else if (sameString(type, "lookup"))
    setupColumnLookup(col, s);
else if (sameString(type, "association"))
    setupColumnAssociation(col, s);
else if (sameString(type, "float"))
    setupColumnFloat(col, s);
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
else if (sameString(type, "expMulti"))
    setupColumnExpMulti(col, s);
else if (sameString(type, "expMax"))
    setupColumnExpMax(col, s);
else if (sameString(type, "go"))
    setupColumnGo(col, s);
else if (sameString(type, "pfam"))
    setupColumnPfam(col, s);
else if (sameString(type, "flyBdgp"))
    setupColumnFlyBdgp(col, s); 
else if (sameString(type, "intronSize"))
    setupColumnIntronSize(col, s);
else if (sameString(type, "xyz"))
    setupColumnXyz(col, s);
else if (sameString(type, "custom"))
    setupColumnCustom(col, s);
else
    errAbort("Unrecognized type %s for %s", col->type, col->name);
freez(&dupe);
}

static char *rootDir = "hgNearData";

struct hash *readRa(char *rootName)
/* Read in ra in root, root/org, and root/org/database. */
{
return hgReadRa(genome, database, rootDir, rootName, NULL);
}

static void getGenomeSettings()
/* Set up genome settings hash */
{
struct hash *hash = readRa("genome.ra");
char *name;
if (hash == NULL)
    errAbort("Can't find anything in genome.ra");
name = hashMustFindVal(hash, "name");
if (!sameString(name, "global"))
    errAbort("Can't find global ra record in genome.ra");
genomeSettings = hash;
}


char *genomeSetting(char *name)
/* Return genome setting value.   Aborts if setting not found. */
{
return hashMustFindVal(genomeSettings, name);
}

char *genomeOptionalSetting(char *name)
/* Return genome setting value.  Returns NULL if not found. */
{
return hashFindVal(genomeSettings, name);
}

char *protToGeneId(struct sqlConnection *conn, char *protId)
/* Convert from protein to gene ID. */
{
char *table = genomeSetting("geneTable");
char query[256];
safef(query, sizeof(query), "select name from %s where proteinId='%s'",
	table, protId);
return sqlQuickString(conn, query);
}

void columnVarsFromSettings(struct column *col, char *fileName)
/* Grab a bunch of variables from col->settings and
 * move them into col proper. */
{
struct hash *settings = col->settings;
char *selfLink;
col->name = mustFindInRaHash(fileName, settings, "name");
spaceToUnderbar(col->name);
col->shortLabel = mustFindInRaHash(fileName, settings, "shortLabel");
col->longLabel = mustFindInRaHash(fileName, settings, "longLabel");
col->priority = atof(mustFindInRaHash(fileName, settings, "priority"));
col->on = col->defaultOn = 
	sameString(mustFindInRaHash(fileName, settings, "visibility"), "on");
col->type = mustFindInRaHash(fileName, settings, "type");
col->itemUrl = hashFindVal(settings, "itemUrl");
col->itemUrlQuery = hashFindVal(settings, "itemUrlQuery");
col->useHgsid = columnSettingExists(col, "hgsid");
col->urlChromVar = hashFindVal(settings, "urlChromVar");
col->urlStartVar = hashFindVal(settings, "urlStartVar");
col->urlEndVar = hashFindVal(settings, "urlEndVar");
col->urlOtherGeneVar = hashFindVal(settings, "urlOtherGeneVar");
selfLink = hashFindVal(settings, "selfLink");
if (selfLink != NULL && selfLink[0] != '0')
    col->selfLink = TRUE;
}

static struct hash *hashColumns(struct column *colList)
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

struct column *getColumns(struct sqlConnection *conn)
/* Return list of columns for big table. */
{
char *raName = "columnDb.ra";
struct column *col, *next, *customList, *colList = NULL;
struct hash *raList = readRa(raName), *raHash = NULL;

/* Create built-in columns. */
if (raList == NULL)
    errAbort("Couldn't find anything from %s", raName);
for (raHash = raList; raHash != NULL; raHash = raHash->next)
    {
    AllocVar(col);
    col->settings = raHash;
    columnVarsFromSettings(col, raName);
    if (!hashFindVal(raHash, "hide"))
	{
	setupColumnType(col);
	if (col->exists(col, conn))
	    {
	    slAddHead(&colList, col);
	    }
	}
    }

/* Create custom columns. */
customList = customColumnsRead(conn, genome, database);
for (col = customList; col != NULL; col = next)
    {
    next = col->next;
    setupColumnType(col);
    if (col->exists(col, conn))
	{
	slAddHead(&colList, col);
	}
    }

/* Put columns in hash */
columnHash = hashColumns(colList);

/* Tweak ordering and visibilities as per user settings. */
refinePriorities(columnHash);
refineVisibility(colList);
slSort(&colList, columnCmpPriority);
return colList;
}

struct column *findNamedColumn(char *name)
/* Return column of given name from list or NULL if none. */
{
if (columnHash == NULL)
    internalErr();
return hashFindVal(columnHash, name);
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
    {
    if (gotAdvFilter())
        {
	warn("No genes passed filter.");
	}
    return;
    }
hPrintf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=1 COLS=%d BGCOLOR=\"#"HG_COL_INSIDE"\">\n", 
	totalHtmlColumns(colList));

/* Print label row. */
hPrintf("<TR BGCOLOR=\"#"HG_COL_HEADER"\">");
for (col = colList; col != NULL; col = col->next)
    {
    if (col->on)
	{
	col->labelPrint(col);
	}
    }
hPrintf("</TR>\n");

/* Print other rows. */
hPrintf("<!-- Start Rows -->");
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
    hPrintf("</TR><!-- Row -->\n");
    if (ferror(stdout))
        errAbort("Write error to stdout");
    }
hPrintf("<!-- End Rows -->");

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
    if (col->on)
	{
	if (first)
	    first = FALSE;
	else
	    hPrintf("\t");
	hPrintf("%s", col->name);
	}
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
		hPrintf("n/a");
	    else
		hPrintf("%s", val);
	    freez(&val);
	    }
	}
    hPrintf("\n");
    }
hPrintf("</PRE></TT>");
}

void doMainDisplay(struct sqlConnection *conn, 
	struct order *ord, struct order *ordList,
	struct column *colList, struct genePos *geneList)
/* Put up the main gene sorter display - a control panel followed by 
 * a big table. */
{
char buf[128];
safef(buf, sizeof(buf), "UCSC %s Gene Sorter", genome);
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" NAME=\"mainForm\" METHOD=GET>\n");
makeTitle(buf, "hgNearHelp.html");
cartSaveSession(cart);
mainControlPanel(curGeneId, ord, ordList);
if (geneList != NULL)
    bigTable(conn, colList,geneList);
hPrintf("</FORM>\n");

hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=\"GET\" NAME=\"orgForm\">\n");
hPrintf("<input type=\"hidden\" name=\"org\" value=\"%s\">\n", genome);
hPrintf("<input type=\"hidden\" name=\"db\" value=\"%s\">\n", database);
hPrintf("<input type=\"hidden\" name=\"%s\" value=\"%s\">\n", orderVarName,
	cartUsualString(cart, orderVarName, ""));
cartSaveSession(cart);
puts("</FORM>");
}

struct order *curOrder(struct order *ordList)
/* Get ordering currently selected by user, or default
 * (first in list) if none selected. */
{
char *selName;
struct order *ord;
if (ordList == NULL)
    errAbort("No orderings available");
selName = cartUsualString(cart, orderVarName, ordList->name);
for (ord = ordList; ord != NULL; ord = ord->next)
    {
    if (sameString(ord->name, selName))
        return ord;
    }
return ordList;
}

char *lookupProtein(struct sqlConnection *conn, char *mrnaName)
/* Given mrna name look up protein.  FreeMem result when done. */
{
char query[256];
char buf[64];

if (kgVersion == KG_III)
    {
    safef(query, sizeof(query), 
	"select spDisplayID from kgXref where kgId='%s'", mrnaName);
    }
else
    {
    safef(query, sizeof(query), 
	"select protein from %s where transcript='%s'", 
	genomeSetting("canonicalTable"), mrnaName);
    }
if (!sqlQuickQuery(conn, query, buf, sizeof(buf)))
    return NULL;
return cloneString(buf);
}

void displayData(struct sqlConnection *conn, struct column *colList, 
	struct genePos *gp)
/* Display data in neighborhood of gene. */
{
struct genePos *geneList = NULL;
struct order *ordList = orderGetAll(conn);
struct order *ord = curOrder(ordList);
if (gp != NULL && gp->protein == NULL)
    gp->protein = lookupProtein(conn, gp->name);
curGeneId = gp;
if (cartVarExists(cart, getTextVarName))
    {
    if (gp) geneList = getOrderedList(ord, colList, conn, BIGNUM);
    doGetText(conn, colList, geneList);
    }
else if (cartVarExists(cart, getSeqVarName))
    {
    if (gp) geneList = getOrderedList(ord, colList, conn, BIGNUM);
    doGetSeq(conn, colList, geneList, cartString(cart, getSeqHowVarName));
    }
else if (cartVarExists(cart, getGenomicSeqVarName))
    {
    if (gp) geneList = getOrderedList(ord, colList, conn, BIGNUM);
    doGetGenomicSeq(conn, colList, geneList);
    }
else
    {
    if (gp) geneList = getOrderedList(ord, colList, conn, displayCount);
    doMainDisplay(conn, ord, ordList, colList, geneList);
    }
}

static struct genePos *curGenePos()
/* Return current gene pos from cart variables. */
{
struct genePos *gp;
AllocVar(gp);
gp->name = cloneString(cartString(cart, idVarName));
/* Update cart's searchVarName to idVarName so that subsequent clicks will 
 * have the right value in orgForm's searchVarName. */
cartSetString(cart, searchVarName, gp->name);
if (cartVarExists(cart, idPosVarName))
    {
    hgParseChromRange(database, cartString(cart, idPosVarName),
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

static char *colHtmlFileName(struct column *col)
/* Return html file associated with column.  You can
 * freeMem this when done. */
{
char name[PATH_LEN];
char *org = hgDirForOrg(genome);
safef(name, sizeof(name), "%s/%s/%s/%s.html", rootDir, org, database, col->name);
if (!fileExists(name))
    {
    safef(name, sizeof(name), "%s/%s/%s.html", rootDir, org, col->name);
    if (!fileExists(name))
	safef(name, sizeof(name), "%s/%s.html", rootDir, col->name);
    }
freez(&org);
return cloneString(name);
}

void doColInfo(struct sqlConnection *conn, struct column *colList,
	char *colName)
/* Put up info page on column. */
{
struct column *col = findNamedColumn(colName);
char *htmlFileName;
if (col == NULL)
    errAbort("Can't find column '%s'", colName);
hPrintf("<H2>Column %s - %s</H2>\n", col->shortLabel, col->longLabel);
htmlFileName = colHtmlFileName(col);
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

static char *defaultHgNearDb(char *genome)
/* Return default database for hgNear for given
 * genome (or NULL for default genome.) 
 * You can freeMem the returned value when done. */
{
char *dbName = NULL;
struct dbDb *db = NULL, *dbList = NULL;

if (genome != NULL)
    {
    dbName = hDefaultDbForGenome(genome);
    if (dbName != NULL && hgNearOk(dbName))
	 return dbName;
    freez(&dbName);
    // Find first db with given genome and hgNearOk in ordered list:
    dbList = hGetIndexedDatabases();
    for (db = dbList; db != NULL; db = db->next)
	{
	if (sameString(genome, db->genome) && db->hgNearOk)
	    {
	    dbName = cloneString(db->name);
	    dbDbFree(&dbList);
	    return dbName;
	    }
	}
    }
freez(&dbName);
dbName = hDefaultDb();
if (dbName != NULL && hgNearOk(dbName))
     return dbName;
freez(&dbName);
// Find first db with hgNearOk in ordered list:
if (dbList == NULL)
    dbList = hGetIndexedDatabases();
for (db = dbList; db != NULL; db = db->next)
    {
    if (db->hgNearOk)
	{
	dbName = cloneString(db->name);
	break;
	}
    }
dbDbFree(&dbList);
return dbName;
}

static void makeSureDbHasHgNear()
/* Check that current database supports hgNear. 
 * If not try to find one that does. */
{
if (hgNearOk(database))
    return;
else
    {
    database = defaultHgNearDb(genome);
    if (database == NULL)
	errAbort("No databases are supporting hgNear.");
    genome = hGenome(database);
    cartSetString(cart, dbVarName, database);
    cartSetString(cart, orgVarName, genome);
    }
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

getDbAndGenome(cart, &database, &genome, oldVars);
makeSureDbHasHgNear();
getGenomeSettings();
conn = hAllocConn(database);

/* if kgProtMap2 table exists, this means we are doing KG III */
if (hTableExists(database, "kgProtMap2")) kgVersion = KG_III;

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
else if ((var = cartFindFirstLike(cart, "near.do.up.*")) != NULL)
    {
    doConfigure(conn, colList, var);
    }
else if ((var = cartFindFirstLike(cart, "near.do.down.*")) != NULL)
    {
    doConfigure(conn, colList, var);
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
else if (cartVarExists(cart, customPageDoName))
    doCustomPage(conn, colList);
else if (cartVarExists(cart, customSubmitDoName))
    doCustomSubmit(conn, colList);
else if (cartVarExists(cart, customClearDoName))
    doCustomClear(conn, colList);
else if (cartVarExists(cart, customPasteDoName))
    doCustomPaste(conn, colList);
else if (cartVarExists(cart, customUploadDoName))
    doCustomUpload(conn, colList); 
else if (cartVarExists(cart, customFromUrlDoName))
    doCustomFromUrl(conn, colList);
else if (cartVarExists(cart, orderInfoDoName))
    doOrderInfo(conn);
else if (cartVarExists(cart, affineAliVarName))
    doAffineAlignment(conn);
else if (cartNonemptyString(cart, searchVarName))
    doSearch(conn, colList);
else if (gotAdvFilter())
    displayData(conn, colList, knownPosFirst(conn));
else
    doExamples(conn, colList);
hFreeConn(&conn);
cartRemovePrefix(cart, "near.do.");
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgNear - gene sorter - a cgi script\n"
  "usage:\n"
  "   hgNear\n"
  );
}

int main(int argc, char *argv[])
/* Process command line. */
{
// pushCarefulMemHandler(100000000);
cgiSpoof(&argc, argv);
htmlSetStyle(htmlStyleUndecoratedLink);
htmlSetBgColor(HG_CL_OUTSIDE);
oldVars = hashNew(10);
cartHtmlShell("Gene Sorter v"CGI_VERSION, doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
