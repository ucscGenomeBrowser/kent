/* advFilter - stuff to put up advanced filter controls and to build
 * gene lists based on advanced filters. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "portable.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hgColors.h"
#include "hgNear.h"


struct genePos *advFilterResults(struct column *colList, 
	struct sqlConnection *conn)
/* Get list of genes that pass all advanced filter filters.  
 * If no filters are on this returns all genes. */
{
struct genePos *list = knownPosAll(conn);
struct column *col;

if (gotAdvFilter())
/* Then go through and filter it down by column. */
    {
    for (col = colList; col != NULL; col = col->next)
	{
	if (col->advFilter)
	    {
	    list = col->advFilter(col, conn, list);
	    }
	}
    }
return list;
}

boolean gotAdvFilter()
/* Return TRUE if advanced filter variables are set. */
{
char wild[64];
safef(wild, sizeof(wild), "%s*", advFilterPrefix);
return anyRealInCart(cart, wild);
}

char *advFilterName(struct column *col, char *varName)
/* Return variable name for advanced filter. */
{
static char name[64];
safef(name, sizeof(name), "%s%s.%s", advFilterPrefix, col->name, varName);
return name;
}

char *advFilterVal(struct column *col, char *varName)
/* Return value for advanced filter variable.  Return NULL if it
 * doesn't exist or if it is "" */
{
char *name = advFilterName(col, varName);
char *s;
s = cartNonemptyString(cart, name);
if (s != NULL)
    fixSafariSpaceInQuotes(s);
return s;
}

char *advFilterNameI(struct column *col, char *varName)
/* Return name for advanced filter that doesn't force filter. */
{
static char name[64];
safef(name, sizeof(name), "%s%s.%s", advFilterPrefixI, col->name, varName);
return name;
}

void advFilterRemakeTextVar(struct column *col, char *varName, int size)
/* Make a text field of given name and size filling it in with
 * the existing value if any. */
{
char *var = advFilterName(col, varName);
cartMakeTextVar(cart, var, NULL, size);
}

void advFilterKeyUploadButton(struct column *col)
/* Make a button for uploading keywords. */
{
colButton(col, keyWordUploadPrefix, "upload list");
}

struct column *advFilterKeyUploadPressed(struct column *colList)
/* Return column where an key upload button was pressed, or
 * NULL if none. */
{
return colButtonPressed(colList, keyWordUploadPrefix);
}

void doAdvFilterKeyUpload(struct sqlConnection *conn, struct column *colList, 
    struct column *col)
/* Handle upload keyword list button press in advanced filter form. */
{
char *varName = NULL;
char buf[1024];
cartRemovePrefix(cart, keyWordUploadPrefix);
safef(buf, sizeof(buf), "Upload List : %s - %s", col->shortLabel, col->longLabel);
makeTitle(buf, NULL);
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=POST ENCTYPE=\"multipart/form-data\">\n");
cartSaveSession(cart);
hPrintf("Enter the name of a file from your computer that contains a list");
hPrintf("of items separated by a space, tab or line.<BR>");

varName = colVarName(col, keyWordPastedPrefix);
hPrintf("<INPUT TYPE=FILE NAME=\"%s\"> ", varName);
cgiMakeButton("submit", "submit");
hPrintf("</FORM>");
}

void advFilterKeyPasteButton(struct column *col)
/* Make a button for uploading keywords. */
{
colButton(col, keyWordPastePrefix, "paste list");
}

struct column *advFilterKeyPastePressed(struct column *colList)
/* Return column where an key upload button was pressed, or
 * NULL if none. */
{
return colButtonPressed(colList, keyWordPastePrefix);
}

void doAdvFilterKeyPaste(struct sqlConnection *conn, struct column *colList, 
    struct column *col)
/* Handle upload keyword list button press in advanced filter form. */
{
char *varName = NULL;
char buf[1024];
cartRemovePrefix(cart, keyWordPastePrefix);
safef(buf, sizeof(buf), "Paste List : %s - %s", col->shortLabel, col->longLabel);
makeTitle(buf, NULL);
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=POST>\n");
cartSaveSession(cart);
hPrintf("Paste in a list of items to match. ");
cgiMakeButton("submit", "submit");
hPrintf("<BR>\n");
varName = colVarName(col, keyWordPastedPrefix);
cgiMakeTextArea(varName, "", 10, 60);
hPrintf("</FORM>");
}

struct column *advFilterKeyPastedPressed(struct column *colList)
/* Return column where an key upload button was pressed, or
 * NULL if none. */
{
return colButtonPressed(colList, keyWordPastedPrefix);
}

void doAdvFilterKeyPasted(struct sqlConnection *conn, struct column *colList, 
    struct column *col)
/* Handle submission in key-paste in form. */
{
char *pasteVarName = colVarName(col, keyWordPastedPrefix);
char *pasteVal = trimSpaces(cartString(cart, pasteVarName));
char *keyVarName = advFilterName(col, "keyFile");

if (pasteVal == NULL || pasteVal[0] == 0)
    {
    /* If string is empty then clear cart variable. */
    cartRemove(cart, keyVarName);
    }
else
    {
    /* Else write variable to temp file and save temp
     * file name. */
    struct tempName tn;
    FILE *f;
    makeTempName(&tn, "near", ".key");
    f = mustOpen(tn.forCgi, "w");
    mustWrite(f, pasteVal, strlen(pasteVal));
    carefulClose(&f);
    cartSetString(cart, keyVarName, tn.forCgi);
    }
cartRemovePrefix(cart, keyWordPastedPrefix);
doAdvFilter(conn, colList);
}

void advFilterKeyClearButton(struct column *col)
/* Make a button for uploading keywords. */
{
colButton(col, keyWordClearPrefix, "Clear List");
}

struct column *advFilterKeyClearPressed(struct column *colList)
/* Return column where an key upload button was pressed, or
 * NULL if none. */
{
return colButtonPressed(colList, keyWordClearPrefix);
}

void doAdvFilterKeyClear(struct sqlConnection *conn, struct column *colList, 
    struct column *col)
/* Handle upload keyword list button press in advanced filter form. */
{
cartRemovePrefix(cart, keyWordClearPrefix);
cartRemove(cart, advFilterName(col, "keyFile"));
doAdvFilter(conn, colList);
}


static char *anyAllMenu[] = {"all", "any"};

void advFilterAnyAllMenu(struct column *col, char *varName, 
	boolean defaultAny)
/* Make a drop-down menu with value all/any. */
{
char *var = advFilterNameI(col, varName);
char *val = cartUsualString(cart, var, anyAllMenu[defaultAny]);
cgiMakeDropList(var, anyAllMenu, ArraySize(anyAllMenu), val);
}

boolean advFilterOrLogic(struct column *col, char *varName, 
	boolean defaultOr)
/* Return TRUE if user has selected 'all' from any/all menu
 * of given name. */
{
char *var = advFilterNameI(col, varName);
char *val = cartUsualString(cart, var, anyAllMenu[defaultOr]);
return sameWord(val, "any");
}

struct userSettings *filUserSettings()
/* Return userSettings object for columns. */
{
struct userSettings *us = userSettingsNew(cart, 
	"Current Filter Settings", 
	filSavedCurrentVarName, filSaveSettingsPrefix);
userSettingsCapturePrefix(us, advFilterPrefix);
userSettingsCapturePrefix(us, advFilterPrefixI);
return us;
}

static void bigButtons()
/* Put up the big clear/submit buttons. */
{
hPrintf("<TABLE><TR><TD>");
cgiMakeButton("submit", "submit");
hPrintf("</TD><TD>");
cgiMakeButton(advFilterClearVarName, "clear filter");
hPrintf("</TD><TD>");
cgiMakeButton(filSaveCurrentVarName, "save filter");
hPrintf("</TD><TD>");
cgiMakeOptionalButton(filUseSavedVarName, "load filter", 
	!userSettingsAnySaved(filUserSettings()));
hPrintf("</TD></TR></TABLE>");
}

void doAdvFilter(struct sqlConnection *conn, struct column *colList)
/* Put up advanced filter page. */
{
struct column *col;
boolean passPresent[2];
int onOff = 0;

makeTitle("Gene Sorter Filter", "hgNearHelp.html#Filter");
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=%s>\n",
	cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);

controlPanelStart();
hPrintf("On this page you can restrict which genes appear in the main table<BR>");
hPrintf("based on the values in any column. Click the <em>submit</em> button to return<BR>");
hPrintf("to the main Gene Sorter page with the current filter settings applied.");
bigButtons();
hPrintf("Quickly obtain a list of gene "
 "names that pass the filter: ");
cgiMakeButton(advFilterListVarName, "list names");
controlPanelEnd();

/* See if have any to do in either first (displayed columns)
 * or second (hidden columns) pass. */
passPresent[0] = passPresent[1] = FALSE;
for (onOff = 1; onOff >= 0; --onOff)
    {
    for (col = colList; col != NULL; col = col->next)
        if (col->filterControls && col->on == onOff)
	    passPresent[onOff] = TRUE;
    }

/* Print out two tables of search controls - one for displayed
 * columns and one for hidden ones. */
for (onOff = 1; onOff >= 0; --onOff)
    {
    if (passPresent[onOff])
	{
	hPrintf("<H2>Filter Controls for %s Columns:</H2>", 
		(onOff ? "Displayed" : "Hidden"));
	hPrintf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=2 BGCOLOR=\"#"HG_COL_INSIDE"\">\n");
	for (col = colList; col != NULL; col = col->next)
	    {
	    if (col->filterControls && col->on == onOff)
		{
		hPrintf("<TR><TD>");
		hPrintf("<TABLE>\n");
		hPrintf("<TR><TD><B>%s - %s</B></TD></TR>\n", 
			col->shortLabel, col->longLabel);
		hPrintf("<TR><TD>");
		col->filterControls(col, conn);
		hPrintf("</TD></TR>\n");
		hPrintf("</TABLE>");
		hPrintf("<BR>");
		hPrintf("</TD></TR>\n");
		}
	    }
	hPrintf("</TABLE>\n");
	hPrintf("<BR>");
	cgiMakeButton("submit", "submit");
	}
    }
hPrintf("</FORM>\n");
}

void doAdvFilterClear(struct sqlConnection *conn, struct column *colList)
/* Clear variables in advanced filter page. */
{
cartRemovePrefix(cart, advFilterPrefix);
cartRemovePrefix(cart, advFilterPrefixI);
doAdvFilter(conn, colList);
}

void doAdvFilterListCol(struct sqlConnection *conn, struct column *colList,
	char *colName)
/* List a column for genes matching advanced filter. */
{
struct genePos *gp, *list = NULL, *newList = NULL, *gpNext = NULL;
struct column *col = findNamedColumn(colName);
makeTitle("Current Filters", NULL);

if (col == NULL)
    {
    warn("No name column");
    internalErr();
    }
hPrintf("<TT><PRE>");
if (gotAdvFilter())
    {
    list = advFilterResults(colList, conn);
    }
else
    {
    hPrintf("#No filters activated. List contains all genes.\n");
    list = knownPosAll(conn);
    }

/* Now lookup names and sort. */
for (gp = list; gp != NULL; gp = gpNext)
    {
    char *oldName = gp->name;
    gp->name = col->cellVal(col, gp, conn);
    gpNext = gp->next;
    if (gp->name == NULL)
	{
	warn("Unable to find cellVal for %s -- tables out of sync?",
		 oldName);
	}
    else
	{
	slAddHead(&newList,gp);
	}
    
    }
list = newList;
slSort(&list, genePosCmpName);

/* Display. */
for (gp = list; gp != NULL; gp = gp->next)
    {
    hPrintf("%s\n", gp->name);
    }
hPrintf("</PRE></TT>");
}

void doAdvFilterList(struct sqlConnection *conn, struct column *colList)
/* List gene names matching advanced filter. */
{
doAdvFilterListCol(conn, colList, "name");
}

void doNameCurrentFilters()
/* Put up page to save current filter settings. */
{
userSettingsSaveForm(filUserSettings());
}

void doSaveCurrentFilters(struct sqlConnection *conn, struct column *colList)
/* Handle save current filters form result. */
{
if (userSettingsProcessForm(filUserSettings()))
    doAdvFilter(conn, colList);
}

void doUseSavedFilters(struct sqlConnection *conn, struct column *colList)
/* Use indicated filter settings. */
{
userSettingsLoadForm(filUserSettings());
}

