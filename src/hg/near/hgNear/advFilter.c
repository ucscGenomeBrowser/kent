/* advFilter - stuff to put up advanced filter controls and to build
 * gene lists based on advanced filters. */

#include "common.h"
#include "hash.h"
#include "portable.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hgNear.h"

static char const rcsid[] = "$Id: advFilter.c,v 1.2 2003/09/06 18:55:09 kent Exp $";

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
	    list = col->advFilter(col, conn, list);
	}
    }
return list;
}

static boolean anyRealInCart(struct cart *cart, char *wild)
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

boolean gotAdvFilter()
/* Return TRUE if advanced filter variables are set. */
{
char wild[64];
safef(wild, sizeof(wild), "%s*", advFilterPrefix);
return anyRealInCart(cart, wild);
}

boolean advFilterColAnySet(struct column *col)
/* Return TRUE if any of the advanced filter variables
 * for this col are set. */
{
char wild[128];
safef(wild, sizeof(wild), "%s%s*", advFilterPrefix, col->name);
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
return cartNonemptyString(cart, name);
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
char *val = cartOptionalString(cart, var);
cgiMakeTextVar(var, val, 8);
}

void advFilterKeyUploadButton(struct column *col)
/* Make a button for uploading keywords. */
{
colButton(col, keyWordUploadPrefix, "Upload List");
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

void advFilterKeyPasteButton(struct column *col)
/* Make a button for uploading keywords. */
{
colButton(col, keyWordPastePrefix, "Paste List");
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
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=POST>\n");
cartSaveSession(cart);
hPrintf("Paste in a list of items to match. ");
cgiMakeButton("submit", "Submit");
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

static void bigButtons()
/* Put up the big clear/submit buttons. */
{
hPrintf("<TABLE><TR><TD>");
hPrintf("Filter form: ");
cgiMakeButton(advFilterClearVarName, "Clear All");
hPrintf(" ");
cgiMakeButton(advFilterListVarName, "List Names");
hPrintf(" ");
cgiMakeButton(advFilterBrowseVarName, "Browse Results");
hPrintf("</TD></TR></TABLE>\n");
}

void doAdvFilter(struct sqlConnection *conn, struct column *colList)
/* Put up advanced filter page. */
{
struct column *col;
boolean passPresent[2];
int onOff = 0;
boolean anyForSecondPass = FALSE;

makeTitle("Gene Family Filter", "hgNearAdvFilter.html");
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=POST>\n");
cartSaveSession(cart);

/* Put up little table with clear all/submit */
bigButtons();
hPrintf("Hint: you can combine filters by copying the results of 'List Matching Names' "
        "and then doing a 'Paste List' in the Name controls.");

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
	hPrintf("<H2>Filter Controls for %s Tracks:</H2>", 
		(onOff ? "Displayed" : "Hidden"));
	hPrintf("<TABLE BORDER=2 CELLSPACING=1 CELLPADDING=1>\n");
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

void doAdvFilterClear(struct sqlConnection *conn, struct column *colList)
/* Clear variables in advanced filter page. */
{
cartRemovePrefix(cart, advFilterPrefix);
cartRemovePrefix(cart, advFilterPrefixI);
doAdvFilter(conn, colList);
}

void doAdvFilterBrowse(struct sqlConnection *conn, struct column *colList)
/* List gene names matching advanced filter. */
{
if (gotAdvFilter())
    {
    // groupOn = "advanced filter";
    // cartSetString(cart, groupVarName, groupOn);
    }
doSearch(conn, colList);
}

void doAdvFilterList(struct sqlConnection *conn, struct column *colList)
/* List gene names matching advanced filter. */
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

#ifdef OLD
struct genePos *getSearchNeighbors(struct column *colList, 
	struct sqlConnection *conn, struct hash *goodHash, int maxCount)
/* Get neighbors by search. */
{
struct genePos *list, *rejectList = NULL;

list = advFilterResults(colList, conn);
list = firstBitsOfList(list, maxCount, &rejectList);
return list;
}
#endif /* OLD */

