/* advFilter - stuff to put up advanced filter controls and to build
 * gene lists based on advanced filters. */

#include "common.h"
#include "hash.h"
#include "portable.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"
#include "hdb.h"
#include "hgColors.h"
#include "hPrint.h"
#include "gsidTable.h"


struct subjInfo *advFilterResults(struct column *colList, 
	struct sqlConnection *conn)
/* Get list of genes that pass all advanced filter filters.  
 * If no filters are on this returns all genes. */
{
struct subjInfo *list = readAllSubjInfo(conn, colList);
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

char *advFilterName(struct column *col, char *varName)
/* Return variable name for advanced filter. */
{
static char name[64];
safef(name, sizeof(name), "%s%s.%s", advFilterPrefix, col->name, varName);
return name;
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


char *advFilterNameI(struct column *col, char *varName)
/* Return name for advanced filter that doesn't force filter. */
{
static char name[64];
safef(name, sizeof(name), "%s%s.%s", advFilterPrefixI, col->name, varName);
return name;
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

void advFilterKeyPasteButton(struct column *col)
/* Make a button for uploading keywords. */
{
colButton(col, keyWordPastePrefix, "paste list");
}

void advFilterKeyClearButton(struct column *col)
/* Make a button for uploading keywords. */
{
colButton(col, keyWordClearPrefix, "Clear List");
}

void doAdvFilterKeyClear(struct sqlConnection *conn, struct column *colList,
    struct column *col)
/* Handle upload keyword list button press in advanced filter form. */
{   
cartRemovePrefix(cart, keyWordClearPrefix);
cartRemove(cart, advFilterName(col, "keyFile"));
doAdvFilter(conn, colList);
}

struct column *advFilterKeyClearPressed(struct column *colList)
/* Return column where an key upload button was pressed, or
 * NULL if none. */
{   
return colButtonPressed(colList, keyWordClearPrefix);
}       

void doAdvFilterKeyUpload(struct sqlConnection *conn, struct column *colList,
    struct column *col)
/* Handle upload keyword list button press in advanced filter form. */
{   
char *varName = NULL;
cartRemovePrefix(cart, keyWordUploadPrefix);
hPrintf("<H2>Upload List : %s - %s</H2>\n", col->shortLabel, col->longLabel);
hPrintf("<FORM ACTION=\"../cgi-bin/gsidTable\" METHOD=POST ENCTYPE=\"multipart/form-data\">\n");
cartSaveSession(cart);
hPrintf("Enter the name of a text (ASCII) file from your computer that contains a list");
hPrintf(" of items separated by a space, tab or line.<BR>");

varName = colVarName(col, keyWordPastedPrefix);
hPrintf("<INPUT TYPE=FILE NAME=\"%s\"> ", varName);
cgiMakeButton("submit", "submit");
hPrintf("</FORM>");
}


struct column *advFilterKeyUploadPressed(struct column *colList)
/* Return column where an key upload button was pressed, or
 * NULL if none. */
{
return colButtonPressed(colList, keyWordUploadPrefix);
}

void doAdvFilterClear(struct sqlConnection *conn, struct column *colList)
/* Clear variables in advanced filter page. */
{
cartRemovePrefix(cart, advFilterPrefix);
cartRemovePrefix(cart, advFilterPrefixI);
doAdvFilter(conn, colList);
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
cartRemovePrefix(cart, keyWordPastePrefix);
hPrintf("<H2>Paste List : %s - %s</H2>\n", col->shortLabel, col->longLabel);
hPrintf("<FORM ACTION=\"../cgi-bin/gsidTable\" METHOD=POST>\n");
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
    makeTempName(&tn, "gsidTable", ".key");
    f = mustOpen(tn.forCgi, "w");
    mustWrite(f, pasteVal, strlen(pasteVal));
    carefulClose(&f);
    cartSetString(cart, keyVarName, tn.forCgi);
    }
cartRemovePrefix(cart, keyWordPastedPrefix);
doAdvFilter(conn, colList);
}



static void bigButtons()
/* Put up the big clear/submit buttons. */
{
hPrintf("<TABLE><TR><TD>");
cgiMakeButton("submit_filter", "submit");
hPrintf("</TD><TD>");
cgiMakeButton(advFilterClearVarName, "clear filter");
//hPrintf("</TD><TD>");
//cgiMakeButton(filSaveCurrentVarName, "save filter");
//hPrintf("</TD><TD>");
//cgiMakeOptionalButton(filUseSavedVarName, "load filter",
//        !userSettingsAnySaved(filUserSettings()));
hPrintf("</TD></TR></TABLE>");
}



void doAdvFilter(struct sqlConnection *conn, struct column *colList)
/* Put up advanced filter page. */
{
struct column *col;
boolean passPresent[2];
int onOff = 0;

//makeTitle("Select Subjects", "gsidTableHelp.html#Filter");
hotLinks();
hPrintf("<FORM ACTION=\"../cgi-bin/gsidTable\" METHOD=POST>\n");
cartSaveSession(cart);

if (cartVarExists(cart, redirectName))
    {
    /* preserve in cgiVars */
    cgiMakeHiddenVar(redirectName,cartString(cart,redirectName));
    }
printf("<font size=5><B>Select Subjects</B></font>");
controlPanelStart();
hPrintf("On this page you can restrict which subjects appear in the main table<BR>");
hPrintf("based on the values in any column. Click the <em>submit</em> button to return<BR>");
hPrintf("to the Table View (or Sequence View) page with the current filter<BR>settings applied.");
hPrintf("  If available value is more than one word, value must be<BR>enclosed in quotes (e.g. \"West Coast\") to filter correctly.");
bigButtons();
//hPrintf("Quickly obtain a list of gene "
// "names that pass the filter: ");
//cgiMakeButton(advFilterListVarName, "list names");
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

hPrintf("<CENTER><TABLE><TR><TD>");
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
                if (sameWord(col->name, "dnaSeqs") || 
                    sameWord(col->name, "aaSeqs") ) continue;

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
        cgiMakeButton("submit_filter", "submit");
        }
    }
hPrintf("</TD></TR></TABLE></CENTER>");
hPrintf("</FORM>\n");
}


