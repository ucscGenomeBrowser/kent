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
hPrintf("<TD>Name</TD>");
hPrintf("<TD>State</TD>");
hPrintf("<TD>Position</TD>");
hPrintf("</TR>");

/* Write out other rows. */
for (col = colList; col != NULL; col = col->next)
    {
    hPrintf("<TR>");

    /* Do small label. */
    hPrintf("<TD>%s</TD>", col->label);

    /* Do on/off dropdown. */
    hPrintf("<TD>");
    safef(varName, sizeof(varName), "near.col.%s", col->name);
    onOffVal = cartUsualString(cart, varName, "on");
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

    hPrintf("</TR>\n");
    }
hPrintf("</TABLE>\n");
}

static void bumpColList(char *bumpHow)
/* Bump a column one way or another */
{
uglyf("%s", bumpHow);
}

void doConfigure(char *bumpVar)
/* Generate configuration page. */
{
struct sqlConnection *conn = hAllocConn();
struct column *colList = getColumns(conn);
if (bumpVar)
    bumpColList(bumpVar);
makeTitle("Configure Gene Family Browser", "hgNearConfigure.html");
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=POST>\n");
hPrintf("<TABLE WIDTH=\"100%%\" BORDER=0 CELLSPACING=1 CELLPADDING=1>\n");
hPrintf("<TR><TD ALIGN=LEFT>");
cgiMakeButton(defaultConfName, "Default Configuration");
hPrintf(" ");
printf("<INPUT TYPE=RESET NAME=\"%s\" VALUE=\"%s\">", "near.reset", "Reset");
hPrintf(" ");
cgiMakeButton("submit", "Submit");
hPrintf("</TD>");
configTable(colList, conn);
hFreeConn(&conn);
}

void doDefaultConfigure()
/* Do configuration starting with defaults. */
{
cartRemoveLike(cart, "near.col.*");
doConfigure(NULL);
}


