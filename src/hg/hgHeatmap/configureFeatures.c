/* configure - Do configuration page. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "obscure.h"
#include "jksql.h"
#include "htmshell.h"
#include "hdb.h"
#include "cart.h"
#include "web.h"
#include "hgExp.h"
#include "hgColors.h"
#include "hgHeatmap.h"
#include "hPrint.h"
#include "ispyFeatures.h"
#include "userSettings.h"

static char const rcsid[] = "$Id: configureFeatures.c,v 1.1 2007/09/27 22:55:52 jsanborn Exp $";

void makeTitle(char *title, char *helpName)
     /* Make title bar. */
{
  hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#"HG_COL_HOTLINKS"\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"2\"><TR>\n");
  hPrintf("<TD ALIGN=LEFT><A HREF=\"../index.html\">%s</A></TD>", "Home");
  hPrintf("<TD ALIGN=CENTER><FONT COLOR=\"#FFFFFF\" SIZE=4>%s</FONT></TD>", title);
  hPrintf("<TD ALIGN=Right><A HREF=\"../goldenPath/help/%s\">%s</A></TD>", 
	  helpName, "Help");
  hPrintf("</TR></TABLE>");
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

char *configVarName(struct column *col, char *varName)
/* Return variable name for configuration. */
{
static char name[64];
safef(name, sizeof(name), "%s%s.%s", colConfigPrefix, col->name, varName);
return name;
}

char *configVarVal(struct column *col, char *varName)
/* Return value for configuration variable.  Return NULL if it
 * doesn't exist or if it is "" */
{
char *name = configVarName(col, varName);
return cartNonemptyString(cart, name);
}


static void configTable(struct column *colList, struct sqlConnection *conn)
/* Write out configuration table */
{
struct column *col;
char varName[128];
boolean isVis;

hPrintf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=1 BGCOLOR=\"#"HG_COL_INSIDE"\">\n");

/* Write out first row - labels. */
hPrintf("<TR BGCOLOR=\"#"HG_COL_HEADER"\">");
hPrintf("<TH ALIGN=left>Name</TH>");
hPrintf("<TH ALIGN=left>On</TH>");
hPrintf("<TH ALIGN=left>Position</TH>");
hPrintf("<TH ALIGN=left>Description</TH>");
hPrintf("<TH ALIGN=left>Configuration</TH>");

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
    safef(varName, sizeof(varName), "%s%s.vis", colConfigPrefix, col->name);
    isVis = cartUsualBoolean(cart, varName, col->on);
    cgiMakeCheckBox(varName, isVis);
    hPrintf("</TD>");

    /* Do left/right button */
    hPrintf("<TD ALIGN=CENTER>");
    safef(varName, sizeof(varName), "heat.do.up.%s", col->name);
    if (col != colList)
	{
	hPrintf("<INPUT NAME=\"%s\" TYPE=\"IMAGE\" VALUE=\"up\" ", varName);
	hPrintf("SRC=\"../images/up.gif\">");
	}
    safef(varName, sizeof(varName), "heat.do.down.%s", col->name);
    if (col->next != NULL)
	{
	hPrintf("<INPUT NAME=\"%s\" TYPE=\"IMAGE\" VALUE=\"down\" ", varName);
	hPrintf("SRC=\"../images/down.gif\">");
	}
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

if (chopString(dupe, ".", words, ArraySize(words)) < 4)
    {
    warn("Strange bumpHow value %s in bumpColList", bumpHow);
    cartRemove(cart, bumpHow);
    noWarnAbort();
    }
upDown = words[2];
colName = words[3];

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
//slSort(pColList, columnCmpPriority);
savePriorities(*pColList);
}

struct userSettings *colUserSettings()
/* Return userSettings object for columns. */
{
struct userSettings *us = userSettingsNew(cart, 
	"Current Column Configuration",
	savedCurrentConfName, colSaveSettingsPrefix);
userSettingsCapturePrefix(us, colConfigPrefix);
userSettingsCaptureVar(us, colOrderVar);
return us;
}


static void configControlPanel()
/* Put up configuration control panel. */
{
  //struct userSettings *us = colUserSettings();
controlPanelStart();
hPrintf("<TABLE BORDER=0 CELLSPACING=1 CELLPADDING=1>\n");
hPrintf("<TR><TD ALIGN=LEFT>");
cgiMakeButton("submit", "submit");
hPrintf("</TD><TD> ");
hPrintf("</TD><TD> ");
hPrintf("</TD><TD>");
hPrintf("Columns:");
hPrintf("</TD><TD> ");
//cgiMakeButton(hideAllConfName, "hide all");
hPrintf("</TD><TD>");
//cgiMakeButton(showAllConfName, "show all");
hPrintf("</TD><TD>");
//cgiMakeButton(defaultConfName, "default");
hPrintf("</TD><TD>");
hPrintf("Settings:");
hPrintf("</TD><TD>");
//cgiMakeButton(saveCurrentConfName, "save");
hPrintf("</TD><TD>");
//cgiMakeOptionalButton(useSavedConfName, "load", !userSettingsAnySaved(us));
hPrintf("</TD></TR></TABLE>");

hPrintf("<TABLE BORDER=0 CELLSPACING=1 CELLPADDING=1>\n");
hPrintf("<TR><TD ALIGN=LEFT>");
hPrintf("Expression ratio colors: ");
//hgExpColorDropDown(cart, expRatioColorVarName);
hPrintf("</TD><TD>");
hPrintf("Show all splicing variants: ");
//cgiMakeCheckBox(showAllSpliceVarName, 
//	cartUsualBoolean(cart, showAllSpliceVarName, FALSE));
hPrintf("</TD><TD>");
//cgiMakeButton(customPageDoName, "custom columns");
hPrintf("</TD></TR></TABLE>");
controlPanelEnd();
}

void doConfigure(struct sqlConnection *conn, struct column *colList, char *bumpVar)
/* Generate configuration page. */
{
if (bumpVar)
    bumpColList(bumpVar, &colList);
hPrintf("<FORM ACTION=\"../cgi-bin/hgHeatmap\" METHOD=POST>\n");
makeTitle("Configure Features", "hgHeatmapHelp.html#Configure");
configControlPanel();
cartSaveSession(cart);

configTable(colList, conn);
hPrintf("</FORM>");
}

void doConfigUseSaved(struct sqlConnection *conn, struct column *colList)
/* Respond to Use Saved Settings button in configuration page. */
{
struct userSettings *us = colUserSettings();
userSettingsLoadForm(us);
}


void doSaveCurrentColumns(struct sqlConnection *conn, struct column *colList)
/* Save the current columns, and then go on. */
{
struct userSettings *us = colUserSettings();
if (userSettingsProcessForm(us))
    {
//    refinePriorities(columnHash);
    slSort(&colList, columnCmpPriority);
    doConfigure(conn, colList, NULL);
    }
}
