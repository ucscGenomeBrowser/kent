/* configure - Do configuration page. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "htmshell.h"
#include "hdb.h"
#include "cart.h"
#include "web.h"
#include "hgNear.h"

static char const rcsid[] = "$Id: configure.c,v 1.11 2003/06/25 15:20:43 kent Exp $";

static char *onOffString(boolean on)
/* Return "on" or "off". */
{
return on ? "on" : "off";
}

static void configTable(struct column *colList, struct sqlConnection *conn)
/* Write out configuration table */
{
struct column *col;
char varName[128];
char *onOffVal;
static char *onOffMenu[] = {"off", "on"};

hPrintf("<TABLE BORDER=1>\n");

/* Write out first row - labels. */
hPrintf("<TR BGCOLOR=\"#EFEFFF\">");
hPrintf("<TH>Name</TH>");
hPrintf("<TH>State</TH>");
hPrintf("<TH>Position</TH>");
hPrintf("<TH>Description</TH>");
hPrintf("<TH>Configuration</TH>");

/* Print out configuration controls. */
hPrintf("</TR>");

/* Write out other rows. */
for (col = colList; col != NULL; col = col->next)
    {
    hPrintf("<TR>");

    /* Do small label. */
    hPrintf("<TD>%s</TD>", col->shortLabel);

    /* Do on/off dropdown. */
    hPrintf("<TD>");
    safef(varName, sizeof(varName), "near.col.%s", col->name);
    onOffVal = cartUsualString(cart, varName, onOffString(col->on));
    cgiMakeDropList(varName, onOffMenu, ArraySize(onOffMenu), onOffVal);
    hPrintf("</TD>");

    /* Do left/right button */
    hPrintf("<TD ALIGN=CENTER>");
    safef(varName, sizeof(varName), "near.up.%s", col->name);
    if (col != colList)
	cgiMakeButton(varName, " up ");
    safef(varName, sizeof(varName), "near.down.%s", col->name);
    if (col->next != NULL)
	cgiMakeButton(varName, "down");
    hPrintf("</TD>");

    /* Do long label. */
    hPrintf("<TD>%s</TD>", col->longLabel);

    /* Do configuration controls. */
    if (col->configControls != NULL)
         col->configControls(col);
    else
         hPrintf("<TD>n/a</TD>");

    hPrintf("</TR>\n");
    }
hPrintf("</TABLE>\n");
}

static void savePriorities(struct column *colList)
/* Save priority list. */
{
struct dyString *dy = dyStringNew(4*1024);
struct column *col;

for (col = colList; col != NULL; col = col->next)
    dyStringPrintf(dy, "%s %f ", col->name, col->priority);
cartSetString(cart, colOrderVar, dy->string);
dyStringFree(&dy);
}

static void bumpColList(char *bumpHow, struct column **pColList)
/* Bump a column one way or another */
{
char *dupe = cloneString(bumpHow);
char *words[4], *upDown, *colName;

if (chopString(dupe, ".", words, ArraySize(words)) != 3)
    {
    cartRemove(cart, bumpHow);
    errAbort("Strange bumpHow value %s in bumpColList", bumpHow);
    }
upDown = words[1];
colName = words[2];

if (sameString(upDown, "up"))
    {
    struct column *col, *prev = NULL;
    for (col = *pColList; col != NULL; col = col->next)
        {
	if (sameString(col->name, colName))
	    {
	    if (prev != NULL)
	        {
		float swap = prev->priority;
		prev->priority = col->priority;
		col->priority = swap;
		}
	    break;
	    }
	prev = col;
	}
    }
else
    {
    struct column *col, *next = NULL;
    for (col = *pColList; col != NULL; col = col->next)
        {
	if (sameString(col->name, colName))
	    {
	    if ((next = col->next) != NULL)
	        {
		float swap = next->priority;
		next->priority = col->priority;
		col->priority = swap;
		}
	    break;
	    }
	}
    }
freez(&dupe);
slSort(pColList, columnCmpPriority);
savePriorities(*pColList);
}

void doConfigure(struct sqlConnection *conn, struct column *colList, char *bumpVar)
/* Generate configuration page. */
{
if (bumpVar)
    bumpColList(bumpVar, &colList);
makeTitle("Configure Gene Family Browser", "hgNearConfigure.html");
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=POST>\n");
cartSaveSession(cart);
hPrintf("<TABLE WIDTH=\"100%%\" BORDER=0 CELLSPACING=1 CELLPADDING=1>\n");
hPrintf("<TR><TD ALIGN=LEFT>");
cgiMakeButton(hideAllConfName, "Hide All");
hPrintf(" ");
cgiMakeButton(defaultConfName, "Default Configuration");
hPrintf(" ");
cgiMakeButton("submit", "Submit");
hPrintf("</TD></TR></TABLE>");
configTable(colList, conn);
hPrintf("</FORM>");
}

void doDefaultConfigure(struct sqlConnection *conn, struct column *colList)
/* Do configuration starting with defaults. */
{
struct column *col;
for (col=colList; col != NULL; col = col->next)
    col->on = col->defaultOn;
cartRemoveLike(cart, "near.col.*");
cartRemove(cart, colOrderVar);
doConfigure(conn, colList, NULL);
}

void doConfigHideAll(struct sqlConnection *conn, struct column *colList)
/* Respond to hide all button in configuration page. */
{
char varName[64];
struct column *col;
for (col = colList; col != NULL; col = col->next)
    {
    safef(varName, sizeof(varName), "near.col.%s", col->name);
    cartSetString(cart, varName, "off");
    doConfigure(conn, colList, NULL);
    }
}

