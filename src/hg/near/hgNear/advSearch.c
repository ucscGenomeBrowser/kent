/* advSearch - stuff to put up advanced search controls and to build
 * gene lists based on advanced searches. */

#include "common.h"
#include "hash.h"
#include "portable.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hgNear.h"

static char const rcsid[] = "$Id: advSearch.c,v 1.18 2003/08/30 05:57:46 kent Exp $";

static struct genePos *advancedSearchResults(struct column *colList, 
	struct sqlConnection *conn)
/* Get list of genes that pass all advanced search filters. 
 * If goodHash is non-null then the genes must also be in goodHash. */
{
struct genePos *list = knownPosAll(conn);
struct column *col;

/* Then go through and filter it down by column. */
for (col = colList; col != NULL; col = col->next)
    {
    if (col->advancedSearch)
        list = col->advancedSearch(col, conn, list);
    }
return list;
}

static boolean anyRealInCart(struct cart *cart, char *wild)
/* Return TRUE if advanced search variables are set. */
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

boolean gotAdvSearch()
/* Return TRUE if advanced search variables are set. */
{
char wild[64];
safef(wild, sizeof(wild), "%s*", advSearchPrefix);
return anyRealInCart(cart, wild);
}

boolean advSearchColAnySet(struct column *col)
/* Return TRUE if any of the advanced search variables
 * for this col are set. */
{
char wild[128];
safef(wild, sizeof(wild), "%s%s*", advSearchPrefix, col->name);
return anyRealInCart(cart, wild);
}

char *advSearchName(struct column *col, char *varName)
/* Return variable name for advanced search. */
{
static char name[64];
safef(name, sizeof(name), "%s%s.%s", advSearchPrefix, col->name, varName);
return name;
}

char *advSearchVal(struct column *col, char *varName)
/* Return value for advanced search variable.  Return NULL if it
 * doesn't exist or if it is "" */
{
char *name = advSearchName(col, varName);
return cartNonemptyString(cart, name);
}

char *advSearchNameI(struct column *col, char *varName)
/* Return name for advanced search that doesn't force search. */
{
static char name[64];
safef(name, sizeof(name), "%s%s.%s", advSearchPrefixI, col->name, varName);
return name;
}

void advSearchRemakeTextVar(struct column *col, char *varName, int size)
/* Make a text field of given name and size filling it in with
 * the existing value if any. */
{
char *var = advSearchName(col, varName);
char *val = cartOptionalString(cart, var);
cgiMakeTextVar(var, val, 8);
}

void advSearchKeyUploadButton(struct column *col)
/* Make a button for uploading keywords. */
{
colButton(col, keyWordUploadPrefix, "Upload List");
}

struct column *advSearchKeyUploadPressed(struct column *colList)
/* Return column where an key upload button was pressed, or
 * NULL if none. */
{
return colButtonPressed(colList, keyWordUploadPrefix);
}

void doAdvSearchKeyUpload(struct sqlConnection *conn, struct column *colList, 
    struct column *col)
/* Handle upload keyword list button press in advanced search form. */
{
char *varName = NULL;
cartRemovePrefix(cart, keyWordUploadPrefix);
hPrintf("<H2>Upload List : %s - %s</H2>\n", col->shortLabel, col->longLabel);
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=POST ENCTYPE=\"multipart/form-data\">\n");
cartSaveSession(cart);
hPrintf("Please enter the name of a file in your computer containing a space");
hPrintf("tab, or ");
hPrintf("line separated list of the item want to include.<BR>");

varName = colVarName(col, keyWordPastedPrefix);
hPrintf("<INPUT TYPE=FILE NAME=\"%s\"> ", varName);
cgiMakeButton("submit", "Submit");
hPrintf("</FORM>");
}

void advSearchKeyPasteButton(struct column *col)
/* Make a button for uploading keywords. */
{
colButton(col, keyWordPastePrefix, "Paste List");
}

struct column *advSearchKeyPastePressed(struct column *colList)
/* Return column where an key upload button was pressed, or
 * NULL if none. */
{
return colButtonPressed(colList, keyWordPastePrefix);
}

void doAdvSearchKeyPaste(struct sqlConnection *conn, struct column *colList, 
    struct column *col)
/* Handle upload keyword list button press in advanced search form. */
{
char *varName = NULL;
cartRemovePrefix(cart, keyWordPastePrefix);
hPrintf("<H2>Paste List : %s - %s</H2>\n", col->shortLabel, col->longLabel);
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=POST>\n");
cartSaveSession(cart);
hPrintf("Paste in a list of items to match. ");
cgiMakeButton("submit", "Submit");
hPrintf("<BR>\n");
varName = colVarName(col, keyWordPastedPrefix);
cgiMakeTextArea(varName, "", 10, 60);
hPrintf("</FORM>");
}

struct column *advSearchKeyPastedPressed(struct column *colList)
/* Return column where an key upload button was pressed, or
 * NULL if none. */
{
return colButtonPressed(colList, keyWordPastedPrefix);
}

void doAdvSearchKeyPasted(struct sqlConnection *conn, struct column *colList, 
    struct column *col)
/* Handle submission in key-paste in form. */
{
char *pasteVarName = colVarName(col, keyWordPastedPrefix);
char *pasteVal = trimSpaces(cartString(cart, pasteVarName));
char *keyVarName = advSearchName(col, "keyFile");

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
doAdvancedSearch(conn, colList);
}

void advSearchKeyClearButton(struct column *col)
/* Make a button for uploading keywords. */
{
colButton(col, keyWordClearPrefix, "Clear List");
}

struct column *advSearchKeyClearPressed(struct column *colList)
/* Return column where an key upload button was pressed, or
 * NULL if none. */
{
return colButtonPressed(colList, keyWordClearPrefix);
}

void doAdvSearchKeyClear(struct sqlConnection *conn, struct column *colList, 
    struct column *col)
/* Handle upload keyword list button press in advanced search form. */
{
cartRemovePrefix(cart, keyWordClearPrefix);
cartRemove(cart, advSearchName(col, "keyFile"));
doAdvancedSearch(conn, colList);
}


static char *anyAllMenu[] = {"all", "any"};

void advSearchAnyAllMenu(struct column *col, char *varName, 
	boolean defaultAny)
/* Make a drop-down menu with value all/any. */
{
char *var = advSearchNameI(col, varName);
char *val = cartUsualString(cart, var, anyAllMenu[defaultAny]);
cgiMakeDropList(var, anyAllMenu, ArraySize(anyAllMenu), val);
}

boolean advSearchOrLogic(struct column *col, char *varName, 
	boolean defaultOr)
/* Return TRUE if user has selected 'all' from any/all menu
 * of given name. */
{
char *var = advSearchNameI(col, varName);
char *val = cartUsualString(cart, var, anyAllMenu[defaultOr]);
return sameWord(val, "any");
}

static void bigButtons()
/* Put up the big clear/submit buttons. */
{
hPrintf("<TABLE><TR><TD>");
hPrintf("Advanced search form: ");
cgiMakeButton(advSearchClearVarName, "Clear All");
hPrintf(" ");
cgiMakeButton(advSearchListVarName, "List Matching Names");
hPrintf(" ");
cgiMakeButton(advSearchBrowseVarName, "Browse Results");
hPrintf("</TD></TR></TABLE>\n");
}

void doAdvancedSearch(struct sqlConnection *conn, struct column *colList)
/* Put up advanced search page. */
{
struct column *col;
boolean passPresent[2];
int onOff = 0;
boolean anyForSecondPass = FALSE;

makeTitle("Gene Family Advanced Search", "hgNearAdvSearch.html");
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=POST>\n");
cartSaveSession(cart);

/* Put up little table with clear all/submit */
bigButtons();
hPrintf("Hint: you can combine searches by copying the results of 'List Matching Names' "
        "and then doing a 'Paste List' in the Name controls.");

/* See if have any to do in either first (displayed columns)
 * or second (hidden columns) pass. */
passPresent[0] = passPresent[1] = FALSE;
for (onOff = 1; onOff >= 0; --onOff)
    {
    for (col = colList; col != NULL; col = col->next)
        if (col->searchControls && col->on == onOff)
	    passPresent[onOff] = TRUE;
    }

/* Print out two tables of search controls - one for displayed
 * columns and one for hidden ones. */
for (onOff = 1; onOff >= 0; --onOff)
    {
    if (passPresent[onOff])
	{
	hPrintf("<H2>Search Controls for %s Tracks:</H2>", 
		(onOff ? "Displayed" : "Hidden"));
	hPrintf("<TABLE BORDER=2 CELLSPACING=1 CELLPADDING=1>\n");
	for (col = colList; col != NULL; col = col->next)
	    {
	    if (col->searchControls && col->on == onOff)
		{
		hPrintf("<TR><TD>");
		hPrintf("<TABLE>\n");
		hPrintf("<TR><TD><B>%s - %s</B></TD></TR>\n", 
			col->shortLabel, col->longLabel);
		hPrintf("<TR><TD>");
		col->searchControls(col, conn);
		hPrintf("</TD></TR>");
		hPrintf("</TABLE>");
		hPrintf("</TD></TR>");
		}
	    }
	hPrintf("</TABLE>\n");
	}
    }

/* Put submit buttons at bottom of page as well. */
bigButtons();
hPrintf("</FORM>\n");
}

void doAdvancedSearchClear(struct sqlConnection *conn, struct column *colList)
/* Clear variables in advanced search page. */
{
cartRemovePrefix(cart, advSearchPrefix);
cartRemovePrefix(cart, advSearchPrefixI);
doAdvancedSearch(conn, colList);
}

void doAdvancedSearchBrowse(struct sqlConnection *conn, struct column *colList)
/* List gene names matching advanced search. */
{
if (gotAdvSearch())
    {
    groupOn = "advanced search";
    cartSetString(cart, groupVarName, groupOn);
    }
doSearch(conn, colList);
}

void doAdvancedSearchList(struct sqlConnection *conn, struct column *colList)
/* List gene names matching advanced search. */
{
struct genePos *gp, *list = NULL;
struct hash *uniqHash = newHash(16);
struct column *col = findNamedColumn(colList, "name");

if (col == NULL)
    {
    warn("No name column");
    internalErr();
    }
hPrintf("<TT><PRE>");
if (gotAdvSearch())
    {
    struct hash *goodHash = cannonicalHash(conn);
    list = getSearchNeighbors(colList, conn, goodHash, BIGNUM);
    }
else
    {
    hPrintf("#No search limits activated. List contains all genes.\n");
    list = knownPosAll(conn);
    }

/* Now lookup names and sort. */
for (gp = list; gp != NULL; gp = gp->next)
    {
    gp->name = col->cellVal(col, gp, conn);
    }
slSort(&list, genePosCmpName);

/* Display. */
for (gp = list; gp != NULL; gp = gp->next)
    {
    hPrintf("%s\n", gp->name);
    }
    
hPrintf("</PRE></TT>");
}


static struct genePos *firstBitsOfList(struct genePos *inList, int maxCount, 
	struct genePos **pRejects)
/* Return first maxCount of inList.  Put rest in rejects. */
{
struct genePos *outList = NULL, *gp, *next;
int count;
for (gp = inList, count=0; gp != NULL; gp = next, count++)
    {
    next = gp->next;
    if (count < maxCount)
        {
	slAddHead(&outList, gp);
	}
    else
        {
	slAddHead(pRejects,gp);
	}
    }
slReverse(&outList);
return outList;
}

struct genePos *getSearchNeighbors(struct column *colList, 
	struct sqlConnection *conn, struct hash *goodHash, int maxCount)
/* Get neighbors by search. */
{
struct genePos *list, *rejectList = NULL;

list = advancedSearchResults(colList, conn);
list = firstBitsOfList(list, maxCount, &rejectList);
return list;
}

