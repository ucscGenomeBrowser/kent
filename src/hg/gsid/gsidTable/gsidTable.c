/* gsidTable - GSID Table View */

#include "common.h"
#include "dystring.h"
#include "hash.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "memalloc.h"
#include "jksql.h"
#include "htmshell.h"
#include "cart.h"
#include "hPrint.h"
#include "hdb.h"
#include "hui.h"
#include "web.h"
#include "ra.h"
#include "hgColors.h"
#include "trashDir.h"

#include "gsidTable.h"
#include "versionInfo.h"

static char const rcsid[] = "$Id: gsidTable.c,v 1.37 2008/04/27 01:52:55 fanhsu Exp $";

char *excludeVars[] = { "submit", "Submit", "submit_filter", NULL }; 
/* The excludeVars are not saved to the cart. (We also exclude
 * any variables that start "near.do.") */

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
char *genome;		/* Name of genome - mouse, human, etc. */
char *orderOn;		/* Current sorting column name. */
int displayCount;	/* Number of items to display. */
char *displayCountString; /* Ascii version of display count, including 'all'. */
struct hash *oldCart;	/* Old cart hash. */
//struct hash *genomeSettings;  /* Genome-specific settings from settings.ra. */
struct hash *columnHash;  /* Hash of active columns keyed by name. */
int passedFilterCount;  /* number of subjects passing filter */

struct sqlConnection *conn;
struct column *colList, *col;


void controlPanelStart()
/* Put up start of tables around a control panel. */ {
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



static void mainControlPanel()
/* Make control panel. */
{
controlPanelStart();

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
    int i=0;
    static char *menu[] = {"25", "50", "100", "200", "500", "1000", "all"};
    hPrintf(" display ");
    hPrintf("<SELECT NAME=\"%s\"", countVarName);
    hPrintf(" onchange=\"document.mainForm.submit();\">\n");
    for (i = 0; i < ArraySize(menu); ++i)
        {
        hPrintf("<OPTION VALUE=\"%s\"", menu[i]);
        if (sameString(menu[i], displayCountString))
            hPrintf(" SELECTED");
        hPrintf(">%s\n", menu[i]);
        }
    hPrintf("</SELECT>\n");
    }


/* Make getDna, getText buttons */
    {
    hPrintf(" output ");
    cgiMakeOptionalButton(getSeqPageVarName, "sequence", FALSE);
    hPrintf(" ");
    cgiMakeOptionalButton(getTextVarName, "text", FALSE);
    }
controlPanelEnd();
}


void makeTitle(char *title, char *helpName)
/* Make title bar. */
{
hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#"HG_COL_HOTLINKS"\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"2\"><TR>\n");
hPrintf("<TD ALIGN=LEFT><A HREF=\"/index.html\">%s</A></TD>", wrapWhiteFont("Home"));
hPrintf("<TD ALIGN=CENTER><FONT COLOR=\"#FFFFFF\" SIZE=4>%s</FONT></TD>", title);
hPrintf("<TD ALIGN=Right>", wrapWhiteFont(""));
//hPrintf("<TD ALIGN=Right><A HREF=\"../goldenPath/help/%s\">%s</A></TD>",
//        helpName, wrapWhiteFont("Help"));
hPrintf("</TR></TABLE>");
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
        struct subjInfo *siList)
/* Put up great big table. */
{
struct column *col;
struct subjInfo *si;

if (siList == NULL)
    {
    if (gotAdvFilter())
        {
        warn("No subjects passed filter.");
        }
    return;
    }
hPrintf("<CENTER>");
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
for (si = siList; si != NULL; si = si->next)
    {
    hPrintf("<TR>");
    for (col = colList; col != NULL;
                col = col->next)
        {
        if (col->on)
            {
            if (col->cellPrint == NULL)
                hPrintf("<TD></TD>");
            else
                col->cellPrint(col,si,conn);
            }
        }
    hPrintf("</TR><!-- Row -->\n");
    if (ferror(stdout))
        errAbort("Write error to stdout");
    }
hPrintf("<!-- End Rows -->");

hPrintf("</TABLE>\n");
hPrintf("<BR>Displayed %d out of %d subjects passing filter.",
    slCount(siList), passedFilterCount);
hPrintf("</CENTER>");
}

void doGetText(struct sqlConnection *conn, struct column *colList,
        struct subjInfo *subjList)
/* Put up great big table. */
{
struct subjInfo *si;
struct column *col;
boolean first = TRUE;

if (subjList == NULL)
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
        hPrintf("%s", col->shortLabel);
	}
    }
hPrintf("\n");
for (si = subjList; si != NULL; si = si->next)
    {
    first = TRUE;
    for (col = colList; col != NULL; col = col->next)
        {
        if (col->on)
            {
	    boolean special;
            char *val = col->cellVal(col, si, conn);
            if (first)
                first = FALSE;
            else
                hPrintf("\t");
            if (val == NULL)
                hPrintf("n/a", val);
            else
		{
		special = FALSE;
		if (sameWord(col->name, "cd4Count"))
    		    {
    		    if (sameWord(val, "-1") || sameWord(val, "0"))
			{
			hPrintf("N/A");
			special = TRUE;
			}
    		    }
		if (sameWord(col->name, "hivQuan"))
    		    {
    		    if (sameWord(val, "-1"))
			{
			hPrintf("N/A");
			special = TRUE;
			}
    		    }
		if (sameWord(col->name, "DAEI"))
    		    {
    		    if (sameWord(val, "-1"))
			{
			hPrintf("N/A");
			special = TRUE;
			}
    		    }
		if (sameWord(col->name, "esdi"))
    		    {
    		    if (sameWord(val, "-1"))
			{
			hPrintf("N/A");
			special = TRUE;
			}
    		    }
		if (sameWord(col->name, "hivQuan"))
    		    {
    		    if (sameWord(val, "1000000"))
			{
			hPrintf("&gt; 1000000");
			special = TRUE;
			}
    		    }
		if (sameWord(col->name, "hivQuan"))
    		    {
    		    if (sameWord(val, "200"))
			{
			hPrintf("&lt; 400");
			special = TRUE;
			}
    		    }

		if (!special)
    		    {
    		    hPrintf("%s", val);
    		    }
		}
	    freez(&val);
            }
        }
    hPrintf("\n");
    }
hPrintf("</PRE></TT>");
}

void hotLinks()
/* Put up the hot links bar. */
{
hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#000000\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>\n");
hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#2636D1\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"2\"><TR>\n");

/* Home */
hPrintf("<TD ALIGN=CENTER><A HREF=\"/index.html\" class=\"topbar\"><FONT COLOR=\"#FFFFFF\">Home</FONT></A></TD>");
//, orgEnc);

/* Blat */
hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgBlat?command=start\" class=\"topbar\"><FONT COLOR=\"#FFFFFF\">Blat</FONT></A></TD>");

/* Subject View */
hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/gsidSubj\" class=\"topbar\">%s</A></TD>", "<FONT COLOR=\"#FFFFFF\">Subject View</FONT>");

/* Sequence View */
hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgGateway?db=%s\" class=\"topbar\"><FONT COLOR=\"#FFFFFF\">Sequence View Gateway</FONT></A></TD>", database);

/* Help */

if (cartVarExists(cart, advFilterVarName))
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"/goldenPath/help/gsidTutorial.html#SelectSubject\" TARGET=_blank class=\"topbar\"><FONT COLOR=\"#FFFFFF\">Help</FONT></A></TD>");
    }
else
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"/goldenPath/help/gsidTutorial.html#TableView\" TARGET=_blank class=\"topbar\"><FONT COLOR=\"#FFFFFF\">Help</FONT></A></TD>");
    }

hPuts("</TR></TABLE>");


hPuts("</TD></TR></TABLE>\n");
}

void doMainDisplay(struct sqlConnection *conn,
        struct column *colList, struct subjInfo *subjList)
/* Put up the main gene sorter display - a control panel followed by
 * a big table. */
{
char buf[128];
safef(buf, sizeof(buf), "GSID %s Table View", genome);
hPrintf("<FORM ACTION=\"../cgi-bin/gsidTable\" NAME=\"mainForm\" METHOD=GET>\n");
hotLinks();
cartSaveSession(cart);
printf("<font size=5><B>Table View</B></font>");
mainControlPanel();
if (subjList != NULL)
    bigTable(conn, colList,subjList);

printf("<br>* Estimated Study Day of Infection (ESDI), ");
printf("click <a href=\"http://www.gsid.org/downloads/methods_and_conventions.pdf\" target=_blank> here </a>");
printf(" for further explanation.\n");
printf("<br>* Days After Estimated Infection (DAEI), ");
printf("click <a href=\"http://www.gsid.org/downloads/methods_and_conventions.pdf\" target=_blank> here </a>");
printf(" for further explanation.\n");

hPrintf("</FORM>\n");
}


char *mustFindInRaHash(char *fileName, struct hash *raHash, char *name)
/* Look up in ra hash or die trying. */
{
char *val = hashFindVal(raHash, name);
if (val == NULL)
    errAbort("Missing required %s field in %s", name, fileName);
return val;
}


/* ---- Some helper routines for column methods. ---- */


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



char *columnSetting(struct column *col, char *name, char *defaultVal)
/* Return value of named setting in column, or default if it doesn't exist. */
{
char *result = hashFindVal(col->settings, name);
if (result == NULL)
    result = defaultVal;
return result;
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

struct column *findNamedColumn(char *name)
/* Return column of given name from list or NULL if none. */
{
if (columnHash == NULL)
    internalErr();
return hashFindVal(columnHash, name);
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




void columnVarsFromSettings(struct column *col, char *fileName)
/* Grab a bunch of variables from col->settings and
 * move them into col proper. */
{
struct hash *settings = col->settings;
col->name = mustFindInRaHash(fileName, settings, "name");
spaceToUnderbar(col->name);
col->shortLabel = mustFindInRaHash(fileName, settings, "shortLabel");
col->longLabel = mustFindInRaHash(fileName, settings, "longLabel");
col->priority = atof(mustFindInRaHash(fileName, settings, "priority"));
col->on = col->defaultOn =
        sameString(mustFindInRaHash(fileName, settings, "visibility"), "on");
col->filterOn = FALSE; 
col->type = mustFindInRaHash(fileName, settings, "type");
col->query = hashFindVal(settings, "query");
col->filterDropDown = sameOk(hashFindVal(settings, "filterDropDown"), "on");
col->colNo = -1;  
}


static boolean alwaysExists(struct column *col, struct sqlConnection *conn)
/* We always exist. */
{
return TRUE;
}

static char *noVal(struct column *col, struct subjInfo *si, struct sqlConnection *conn)
/* Return not-available value. */
{
return cloneString("n/a");
}

static int oneColumn(struct column *col)
/* Return that we have single column. */
{
return 1;
}


int columnSettingInt(struct column *col, char *name, int defaultVal)
/* Return value of named integer setting or default if it doesn't exist. */
{
char *result = hashFindVal(col->settings, name);
if (result == NULL)
    return defaultVal;
return atoi(result);
}

static void hPrintSpaces(int count)
/* Print count number of spaces. */
{
while (--count >= 0)
    hPrintf(" ");
}

void colSortLink(struct column *col)
/* print link that will activate sorting on the column */
{

char *plusMinus = "%2B";  /* "+" cgi encoded */

if (sameString(orderOn+1,col->name)&&orderOn[0]=='+')
    plusMinus = "-";
hPrintf("<A href=\"gsidTable?org=%s&db=%s&%s&%s=%s%s\" >", 
    genome, database, cartSidUrlString(cart), orderVarName, plusMinus, col->name);
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
    colSortLink(col);
    hPrintf("%s</A>", col->shortLabel);
    }
else
    {
    int labelLen = strlen(col->shortLabel);
    int diff = colWidth - labelLen;
    if (diff < 0) diff = 0;
    
    colSortLink(col);
    hPrintf("%s</A>", col->shortLabel);
    
    hPrintSpaces(diff);
    }
hPrintf("</PRE></B></TH>");
}

void cellSimplePrint(struct column *col, struct subjInfo *si,
        struct sqlConnection *conn)
/* This just prints one field from table. */
{
char *s = col->cellVal(col, si, conn);
boolean isSubjID = sameString(col->name,"subjId");
hPrintf("<TD>");
if (s == NULL)
    {
    hPrintf("n/a");
    }
else
    {
    if (isSubjID)
	hPrintf("<A HREF=\"gsidSubj?%s&hgs_subj=%s&submit=Go%21\">",cartSidUrlString(cart),s);
    if (sameString(s,""))
	{
	freeMem(s);
	s = cloneString("&nbsp;");
	}

    if (! (sameString(col->name,"dnaSeqs") || sameString(col->name,"aaSeqs")) )
    	hPrintNonBreak(s);
    else
        /* special processing for DNA and protein sequences */
    	hPrintf("%s", s);

    if (isSubjID)
	hPrintf("</A>");
    freeMem(s);
    }
hPrintf("</TD>\n");
}



int sortCmpString(const void *va, const void *vb)
/* Compare to sort rows based on string value. */
{
const struct subjInfo *a = *((struct subjInfo **)va);
const struct subjInfo *b = *((struct subjInfo **)vb);
return strcmp(a->sortString, b->sortString);
}

int sortCmpInteger(const void *va, const void *vb)
/* Compare to sort rows based on integer value. */
{
const struct subjInfo *a = *((struct subjInfo **)va);
const struct subjInfo *b = *((struct subjInfo **)vb);
return a->sortInteger - b->sortInteger;
}

int sortCmpDouble(const void *va, const void *vb)
/* Compare to sort rows based on double value. */
{
const struct subjInfo *a = *((struct subjInfo **)va);
const struct subjInfo *b = *((struct subjInfo **)vb);
return a->sortDouble - b->sortDouble;
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

struct subjInfo *intAdvFilter(struct column *col, 
	struct sqlConnection *conn, struct subjInfo *list)
/* Do advanced filter on string in main table. */
{
char *minString = advFilterVal(col, "min");
char *maxString = advFilterVal(col, "max");
if (minString || maxString)
    {
    int min = minString ? sqlSigned(minString) : 0;
    int max = maxString ? sqlSigned(maxString) : 0;
    struct subjInfo *newList = NULL, *next, *si;
    for (si = list; si != NULL; si = next)
        {
	char *cell = col->cellVal(col, si, conn);
	int val = sqlSigned(cell);
	freez(&cell);
	next = si->next;
	if (!((minString && (val < min)) || (maxString && (val > max))))
	    {
	    slAddHead(&newList, si);
	    }
	}
    slReverse(&newList);
    list = newList;
    }
return list;
}


struct subjInfo *doubleAdvFilter(struct column *col, 
	struct sqlConnection *conn, struct subjInfo *list)
/* Do advanced filter on string in main table. */
{
char *minString = advFilterVal(col, "min");
char *maxString = advFilterVal(col, "max");
if (minString || maxString)
    {
    double min = minString ? sqlDouble(minString) : 0.0;
    double max = maxString ? sqlDouble(maxString) : 0.0;
    struct subjInfo *newList = NULL, *next, *si;
    for (si = list; si != NULL; si = next)
        {
	char *cell = col->cellVal(col, si, conn);
	boolean invalid = sameString(cell,".");
	double val = invalid ? 0.0 : sqlDouble(cell);
	freez(&cell);
	next = si->next;
	if (!invalid && !((minString && (val < min)) || (maxString && (val > max))))
	    {
	    slAddHead(&newList, si);
	    }
	}
    slReverse(&newList);
    list = newList;
    }
return list;
}




struct subjInfo *stringAdvFilter(struct column *col, 
	struct sqlConnection *conn, struct subjInfo *list)
/* Do advanced filter on string in main table. */
{
char *wild = advFilterVal(col, "wild");
struct hash *keyHash = keyFileHash(col);
if (keyHash != NULL)
    {
    struct subjInfo *newList = NULL, *next, *si;
    for (si = list; si != NULL; si = next)
        {
	char *cell = col->cellVal(col, si, conn);
	next = si->next;
	if (hashLookupUpperCase(keyHash, cell))
	    {
	    slAddHead(&newList, si);
	    }
	freez(&cell);
	}
    slReverse(&newList);
    list = newList;
    }
if (wild != NULL)
    {
    boolean orLogic = advFilterOrLogic(col, "logic", TRUE);
    struct subjInfo *newList = NULL, *next, *si;
    struct slName *wildList = stringToSlNames(wild);
    for (si = list; si != NULL; si = next)
        {
	char *cell = col->cellVal(col, si, conn);
	next = si->next;
	if (wildMatchList(cell, wildList, orLogic))
	    {
	    slAddHead(&newList, si);
	    }
	freez(&cell);
	}
    slReverse(&newList);
    list = newList;
    }
hashFree(&keyHash);
return list;
}

/* forward reference */
void showListOfFilterValues(struct column *col, struct sqlConnection *conn);
/* Print out list of values availabe for filter. */


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
                hPrintf("(There is currently 1 item in the list.)", count);
            else
                hPrintf("(There are currently %d items in the list.)", count);
            }
        else
            {
            cartRemove(cart, advFilterName(col, "keyFile"));
            }
       }
    }
if (col->filterDropDown)
    showListOfFilterValues(col, conn);

}



void columnDefaultMethods(struct column *col)
/* Set up default methods. */
{
col->exists = alwaysExists;
col->cellVal = noVal;
col->sortCmp = sortCmpString;
col->cellPrint = cellSimplePrint;
col->labelPrint = labelSimplePrint;
col->tableColumns = oneColumn;
//col->filterControls = col->filterDropDown ? dropDownAdvFilterControls : lookupAdvFilterControls;
col->filterControls = lookupAdvFilterControls;
col->advFilter = stringAdvFilter;
}

char *queryCellVal(struct column *col, struct subjInfo *si,
        struct sqlConnection *conn)
/* return query lookup on subj id */
{
char query[256];
char *answer;
safef(query, sizeof(query), col->query, si->fields[0]);
answer = sqlQuickString(conn, query);
if (answer == NULL) 
    {
    //return(cloneString("N/A"));
    return(cloneString("-1"));
    }
else 
    {
    return answer;
    }
}

char *stringCellVal(struct column *col, struct subjInfo *si,
        struct sqlConnection *conn)
/* Return clone of geneId */
{
if (col->query)
    return queryCellVal(col,si,conn);
else
    return cloneString(si->fields[col->colNo]);
}

void setupColumnString(struct column *col, char *parameters)
/* Set up a column that displays the geneId (accession) */
{
col->cellVal = stringCellVal;
}

void integerCellPrint(struct column *col, struct subjInfo *si,
        struct sqlConnection *conn)
/* Print value including favorite hyperlink in debug column. */
{
boolean special;
special = FALSE;
char *s = col->cellVal(col, si, conn);
hPrintf("<TD align=right>");
if (sameWord(col->name, "cd4Count"))
    {
    if (sameWord(s, "-1") || sameWord(s, "0"))
	{
	printf("N/A");
	special = TRUE;
	}
    }
if (sameWord(col->name, "hivQuan"))
    {
    if (sameWord(s, "-1"))
	{
	printf("N/A");
	special = TRUE;
	}
    }
if (sameWord(col->name, "DAEI"))
    {
    if (sameWord(s, "-1"))
	{
	printf("N/A");
	special = TRUE;
	}
    }
if (sameWord(col->name, "esdi"))
    {
    if (sameWord(s, "-1"))
	{
	printf("N/A");
	special = TRUE;
	}
    }
if (sameWord(col->name, "hivQuan"))
    {
    if (sameWord(s, "1000000"))
	{
	printf("&gt; 1000000");
	special = TRUE;
	}
    }
if (sameWord(col->name, "hivQuan"))
    {
    if (sameWord(s, "200"))
	{
	printf("&lt; 400");
	special = TRUE;
	}
    }
if (!special)
    {
    hPrintf("%s", s);
    }
hPrintf("</TD>");
freeMem(s);
}

void doubleCellPrint(struct column *col, struct subjInfo *si,
        struct sqlConnection *conn)
/* print double value */
{
char *s = col->cellVal(col, si, conn);
char buf[256];
if (sameString(s,"."))  // known bad data value
    safef(buf,sizeof(buf),"%s", s);
else
    safef(buf,sizeof(buf),"%.1f",sqlDouble(s));
freeMem(s);
hPrintf("<TD align=right>");
hPrintf("%s", buf);
hPrintf("</TD>");
}


/* TODO:
    assuming we want to keep this approach,
    for cleanup need to rename struct col member filterDropDown
    to something like showAvailableValues.
    Then remove dropDownAdvFilterControls()
    and rename showListOfFilterValues() to showListOfAvailableValues()
*/

void showListOfFilterValues(struct column *col, struct sqlConnection *conn)
/* Print out list of values availabe for filter. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct slName *list=NULL, *el;

safef(query, sizeof(query),
    "select distinct %s from gsidSubjInfo", col->name);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *val = row[0];
    if (col->remap)
    	val = hashFindVal(col->remap,val);
    slNameAddHead(&list, val);
    }
sqlFreeResult(&sr);

slNameSort(&list);

hPrintf("<BR>\n");
hPrintf("<B>Available Values:</B><BR>\n");
hPrintf("<TABLE>\n");

for (el = list; el; el = el->next)
    {
    hPrintf("<TR><TD>%s</TD></TR>\n", el->name);
    }
    
hPrintf("</TABLE>\n");

slFreeList(&list);

}


void dropDownAdvFilterControls(struct column *col, struct sqlConnection *conn)
/* Print out controls for dropdown list filter. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct slName *list=NULL, *el;

safef(query, sizeof(query),
    "select distinct %s from gsidSubjInfo", col->name);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *val = row[0];
    if (col->remap)
    	val = hashFindVal(col->remap,val);
    slNameAddHead(&list, val);
    }
sqlFreeResult(&sr);

slNameSort(&list);

hPrintf("choose: ");
hPrintf("<SELECT NAME=\"%s\"", "countVarName");
hPrintf(">\n");
hPrintf("<OPTION VALUE=\"\">");

for (el = list; el; el = el->next)
    {
    hPrintf("<OPTION VALUE=\"%s\"", el->name);
    if (sameString(el->name, "displayCountString"))
	hPrintf(" SELECTED");
    hPrintf(">%s\n", el->name);
    }
    
hPrintf("</SELECT>\n");

slFreeList(&list);

}

void minMaxAdvFilterControls(struct column *col, struct sqlConnection *conn)
/* Print out controls for min/max advanced filter. */
{
hPrintf("minimum: ");
advFilterRemakeTextVar(col, "min", 8);
hPrintf(" maximum: ");
advFilterRemakeTextVar(col, "max", 8);
}



void setupColumnInteger(struct column *col, char *parameters)
/* Set up a column that displays an integer */
{
col->cellVal = stringCellVal;
col->cellPrint = integerCellPrint;
col->sortCmp = sortCmpInteger;
col->filterControls = minMaxAdvFilterControls;
col->advFilter = intAdvFilter;
}

void setupColumnDouble(struct column *col, char *parameters)
/* Set up a column that displays an double */
{
col->cellVal = stringCellVal;
col->cellPrint = doubleCellPrint;
col->sortCmp = sortCmpDouble;
col->filterControls = minMaxAdvFilterControls;
col->advFilter = doubleAdvFilter;
}


char *remapCellVal(struct column *col, struct subjInfo *si,
        struct sqlConnection *conn)
/* Return clone of value */
{
char *s = stringCellVal(col,si,conn);
char *result = cloneString(hashFindVal(col->remap,s));
freeMem(s);
return result;
}

void setupColumnRemap(struct column *col, char *parameters)
/* Set up a column that remaps one string to another via .ra settings-made hash */
{
col->remap=newHash(5);
col->cellVal = remapCellVal;
char wordBuf[256];
char valueBuf[256];
char *word = NULL, *wEnd = wordBuf+sizeof(wordBuf);
char *value = NULL, *vEnd = valueBuf+sizeof(valueBuf);
char c = ' ';

while (*parameters != 0)
    {
    word = wordBuf;
    value = valueBuf;
    c = (*parameters++);
    if (c != '"')
	errAbort("remap syntax error in %s",col->name);
   
    while(TRUE)
	{
	c = *parameters++;
	if (c == '"')
	    break;
	*word++= c;
	if (word >= wEnd)
	    errAbort("remap syntax error in %s",col->name);
	}
    *word = 0;	    
    
    c = (*parameters++);
    if (c != '=')
	errAbort("remap syntax error in %s",col->name);
    c = (*parameters++);
    if (c != '"')
	errAbort("remap syntax error in %s",col->name);
    while(TRUE)
	{
	c = *parameters++;
	if (c == '"')
	    break;
	*value++= c;
	if (value >= vEnd)
	    errAbort("remap syntax error in %s",col->name);
	}
    *value = 0;	    
    word = wordBuf;
    value = valueBuf;
    hashAdd(col->remap, word, cloneString(value));

    if (*parameters == ' ')
	++parameters;
    }
}

void setupColumnType(struct column *col)
/* Set up methods and column-specific variables based on  * track type. */
{
char *dupe = cloneString(col->type);
char *s = dupe;
char *type = nextWord(&s); 
columnDefaultMethods(col);
if (type == NULL)
    warn("Missing type value for column %s", col->name);
if (sameString(type, "integer"))
    setupColumnInteger(col, s);
else if (sameString(type, "double"))
    setupColumnDouble(col, s);
else if (sameString(type, "string"))
    setupColumnString(col, s);
else if (sameString(type, "remap"))
    setupColumnRemap(col, s);
else
    errAbort("Unrecognized type %s for %s", col->type, col->name);
freez(&dupe);
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




static char *rootDir = "gsidTableData";

struct hash *readRa(char *rootName)
/* Read in ra in root, root/org, and root/org/database. */
{
return hgReadRa(genome, database, rootDir, rootName, NULL);
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


void refineFilterOn(struct column *colList)
/* Consult cart to see if filtering is on/off. */
{
char *val;
struct column *col;

for (col = colList; col != NULL; col = col->next)
    {
    val = advFilterVal(col,"max");
    if (val != NULL)
        col->filterOn = TRUE; //sameString(val, "1");
    val = advFilterVal(col,"wild");
    if (val != NULL)
        col->filterOn = TRUE; //sameString(val, "1");
    }
}



struct column *getColumns(struct sqlConnection *conn)
/* Return list of columns for big table. */
{
char *raName = "columnDb.ra";
struct column *col, *colList = NULL;
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
	slAddHead(&colList, col);
        }
    }


/* Put columns in hash */
columnHash = hashColumns(colList);

/* Tweak ordering and visibilities as per user settings. */
//refinePriorities(columnHash);
refineVisibility(colList);
refineFilterOn(colList);
slSort(&colList, columnCmpPriority);
return colList;
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


void saveSubjList(struct subjInfo *subjList)
/* save the filtered list of subject gsids to a file for other applications to use */
{
char *outName = cartOptionalString(cart, gsidSubjList);
char *outName2= cartOptionalString(cart, gsidSeqList);
struct tempName tn;
struct tempName tn2;
struct sqlResult *sr;
char **row;
char query[255];
char *chp;
int cnt;

if (!outName) 
    {
    trashDirFile(&tn, "ct", "gsidSubj", ".list");
    outName = tn.forCgi;
    }

if (!outName2) 
    {
    trashDirFile(&tn2, "ct", "gsidSeq", ".list");
    outName2 = tn2.forCgi;
    }
FILE *outF = mustOpen(outName,"w");
FILE *outF2= mustOpen(outName2,"w");
cnt = 0;
while (subjList)
    {
    fprintf(outF, "%s\n", subjList->fields[0]);
    
    safef(query, sizeof(query), 
	  "select dnaSeqId from gsIdXref where subjId='%s'",
	  subjList->fields[0]);

    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
    	{
	/* Remove "ss." from the front of the DNA sequence ID, 
	   so that they could be used both for DNA and protein MSA maf display */
	chp = strstr(row[0], "ss.");
	if (chp != NULL)
	    {
	    fprintf(outF2, "%s\t%s\n", chp+3L, subjList->fields[0]);
    	    }
	else
	    {
	    fprintf(outF2, "%s\t%s\n", row[0], subjList->fields[0]);
    	    }
	cnt++;
	}
    sqlFreeResult(&sr);

    subjList=subjList->next;
    }
carefulClose(&outF);
carefulClose(&outF2);

cartSetString(cart, gsidSubjList, outName);
cartSetString(cart, gsidSeqList, outName2);
}

struct column *curOrder(struct column *ordList)
/* Get ordering currently selected by user, or default
 * (first in list that is on) if none selected. */
{
char *selName;
struct column *ord, *ordDefault = NULL;
if (ordList == NULL)
    errAbort("No orderings available");
selName = orderOn+1;
for (ord = ordList; ord != NULL; ord = ord->next)
    {
    if (ord->on && sameString(ord->name, selName))
        return ord;
    if (!ordDefault && ord->on)
	ordDefault = ord;
    }
return ordDefault;
}

struct subjInfo *getOrderedList(struct column *ord,
        struct column *colList, struct sqlConnection *conn,
        int maxCount)
/* Return sorted list of subjects. */
{
struct subjInfo *subjList = advFilterResults(colList, conn);
struct subjInfo *si;
passedFilterCount = slCount(subjList);

for (si=subjList;si;si=si->next)
    {
    char *s = ord->cellVal(ord, si, conn);
    if (sameString(ord->type,"integer"))
	{
	si->sortInteger = sqlSigned(s);
	freeMem(s);
	}
    else if (sameString(ord->type,"double"))
	{
	if (sameString(s,"."))
	    si->sortDouble = 0;
	else
	    si->sortDouble = sqlDouble(s);
	freeMem(s);
	}
    else
	{
	si->sortString = s;
	}
    }

slSort(&subjList, ord->sortCmp);

if (orderOn[0]=='-')
    slReverse(&subjList);

/* Trim list to max number. */
si = slElementFromIx(subjList, maxCount-1);
if (si != NULL)
    si->next = NULL;

return subjList;
}




void displayData(struct sqlConnection *conn, struct column *colList)
/* Display data in neighborhood of gene. */
{
struct subjInfo *subjList = NULL;
struct column *ordList = colList;
struct column *ord = curOrder(ordList);

if (ord == NULL)  /* no columns are visible, go to back to configure page */
    {
    doConfigure(conn, colList); 
    return;
    };  


if (cartVarExists(cart, getTextVarName))
    {
    subjList = getOrderedList(ord, colList, conn, BIGNUM);
    doGetText(conn, colList, subjList);
    }
else if (cartVarExists(cart, getSeqVarName))
    {
    subjList = getOrderedList(ord, colList, conn, BIGNUM);
    doGetSeq(conn, subjList, cartString(cart, getSeqHowVarName));
    }
else if (cartVarExists(cart, redirectName))
    {
    subjList = getOrderedList(ord, colList, conn, BIGNUM);
    if (subjList)
	{
    	cartRemove(cart, redirectName);
	}
    else  /* if everything has been filtered out, we'll have to go back */
	{
	hPrintf("No subject(s) found with the filtering conditions specified.<br>");
	hPrintf("Click <a href=\"gsidTable?gsidTable.do.advFilter=filter+%28now+on%29\">here</a> "
	    "to return to Select Subjects.<br>");
	}
    }
else
    {
    subjList = getOrderedList(ord, colList, conn, displayCount);
    doMainDisplay(conn, colList, subjList);
    }
}


void doMiddle(struct cart *theCart)
/* Write the middle parts of the HTML page. 
 * This routine sets up some globals and then
 * dispatches to the appropriate page-maker. */
{
cart = theCart;

if (cartVarExists(cart, confVarName))
    doConfigure(conn, colList);
else if (cartVarExists(cart, defaultConfName))
    doDefaultConfigure(conn, colList);
else if (cartVarExists(cart, hideAllConfName))
    doConfigHideAll(conn, colList);
else if (cartVarExists(cart, showAllConfName))
    doConfigShowAll(conn, colList);

else if (cartVarExists(cart, getSeqPageVarName))
    doGetSeqPage(conn, colList);

else if (cartVarExists(cart, advFilterVarName))
    doAdvFilter(conn, colList);
else if (cartVarExists(cart, advFilterClearVarName))
    doAdvFilterClear(conn, colList);
   
else if ((col = advFilterKeyPastePressed(colList)) != NULL)
    doAdvFilterKeyPaste(conn, colList, col);
else if ((col = advFilterKeyPastedPressed(colList)) != NULL)
    doAdvFilterKeyPasted(conn, colList, col);
else if ((col = advFilterKeyUploadPressed(colList)) != NULL)
    doAdvFilterKeyUpload(conn, colList, col);
else if ((col = advFilterKeyClearPressed(colList)) != NULL)
    doAdvFilterKeyClear(conn, colList, col);

else 
    displayData(conn, colList);

cartRemovePrefix(cart, "gsidTable.do.");

}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gsidTable - subjects table view - a cgi script\n"
  "usage:\n"
  "   gsidTable\n"
  );
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(100000000);
cgiSpoof(&argc, argv);
htmlSetStyle(htmlStyleUndecoratedLink);
htmlSetBgColor(HG_CL_OUTSIDE);
oldCart = hashNew(10);

cart = cartAndCookie(hUserCookie(), excludeVars, oldCart);

getDbAndGenome(cart, &database, &genome, oldCart);
hSetDb(database);
conn = hAllocConn();

/* Get sortOn.  Revert to default by subject Id. */
orderOn = cartUsualString(cart, orderVarName, "+subjId");

displayCountString = cartUsualString(cart, countVarName, "50");
if (sameString(displayCountString, "all")) 
    displayCount = BIGNUM;
else
    displayCount = atoi(displayCountString);
colList = getColumns(conn);

if (cgiVarExists("submit_filter"))  
    {  
    struct dyString *head = dyStringNew(1024);
    boolean redir = cgiVarExists(redirectName);
    struct subjInfo *subjList = NULL;
    struct column *ordList = colList;
    struct column *ord = curOrder(ordList);
    subjList = getOrderedList(ord, colList, conn, BIGNUM);
    saveSubjList(subjList);
    if ((!subjList || redir))
	{
	if (subjList && redir)
	    {
	    dyStringPrintf(head,	
		"<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"0;URL=/cgi-bin/%s\">"
		"<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">"
		"<META HTTP-EQUIV=\"Expires\" CONTENT=\"-1\">"
		, cgiString(redirectName));
    	    cartRemove(cart, redirectName);
	    }
	htmStartWithHead(stdout, head->string, "GSID Table View");
	if (!subjList) /* if everything has been filtered out, we'll have to go back */
	    {
	    hPrintf("No subject(s) found with the filtering conditions specified.<br>");
	    hPrintf("Click <a href=\"gsidTable?gsidTable.do.advFilter=filter+%%28now+on%%29\">here</a> "
		"to return to Select Subjects.<br>");
	    }
	cartCheckout(&cart);
    	htmlEnd();
	hFreeConn(&conn);
	return 0;
	}
    }

htmStart(stdout, "GSID Table View");
cartWarnCatcher(doMiddle, cart, htmlVaWarn);
cartCheckout(&cart);
htmlEnd();

hFreeConn(&conn);

return 0;
}
