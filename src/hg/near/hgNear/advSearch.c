/* advSearch - stuff to put up advanced search controls and to build
 * gene lists based on advanced searches. */

#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hgNear.h"

static char const rcsid[] = "$Id: advSearch.c,v 1.9 2003/07/08 06:18:04 kent Exp $";

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
safef(wild, sizeof(wild), "%s.*", advSearchPrefix);
return anyRealInCart(cart, wild);
}

boolean advSearchColAnySet(struct column *col)
/* Return TRUE if any of the advanced search variables
 * for this col are set. */
{
char wild[128];
safef(wild, sizeof(wild), "%s.%s.*", advSearchPrefix, col->name);
return anyRealInCart(cart, wild);
}

char *advSearchName(struct column *col, char *varName)
/* Return variable name for advanced search. */
{
static char name[64];
safef(name, sizeof(name), "%s.%s.%s", advSearchPrefix, col->name, varName);
return name;
}

char *advSearchVal(struct column *col, char *varName)
/* Return value for advanced search variable.  Return NULL if it
 * doesn't exist or if it is "" */
{
char *name = advSearchName(col, varName);
char *val = trimSpaces(cartOptionalString(cart, name));
if (val != NULL && val[0] == 0)
    val = NULL;
return val;
}

void advSearchRemakeTextVar(struct column *col, char *varName, int size)
/* Make a text field of given name and size filling it in with
 * the existing value if any. */
{
char *var = advSearchName(col, varName);
char *val = cartOptionalString(cart, var);
cgiMakeTextVar(var, val, 8);
}

static void bigButtons()
/* Put up the big clear/submit buttons. */
{
hPrintf("<TABLE><TR><TD>");
hPrintf("Advanced search form: ");
cgiMakeButton(advSearchClearVarName, "clear all");
hPrintf(" ");
cgiMakeButton(advSearchSubmitVarName, "submit");
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
	if (onOff == FALSE)
	    hPrintf("<H2>Search Controls for Hidden Tracks</H2>");
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
char like[64];
safef(like, sizeof(like), "%s.*", advSearchPrefix);
cartRemoveLike(cart, like);
doAdvancedSearch(conn, colList);
}

void doAdvancedSearchSubmit(struct sqlConnection *conn, struct column *colList)
/* Handle submission in advanced search page. */
{
if (gotAdvSearch())
    {
    groupOn = "advanced search";
    cartSetString(cart, groupVarName, groupOn);
    }
doSearch(conn, colList);
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

struct genePos *getSearchNeighbors(struct column *colList, struct sqlConnection *conn)
/* Get neighbors by search. */
{
/* First get full list. */
struct genePos *list = knownPosAll(conn);
struct genePos *rejectList = NULL;
struct column *col;

/* Then go through and filter it down by column. */
for (col = colList; col != NULL; col = col->next)
    {
    if (col->advancedSearch)
        list = col->advancedSearch(col, conn, list);
    }

list = firstBitsOfList(list, displayCount, &rejectList);
return list;
}

