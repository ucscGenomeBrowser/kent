/* configure - Do configuration page. */

#include "common.h"
#include "jksql.h"
#include "cart.h"
#include "hPrint.h"
#include "hgColors.h"

#include "gsidTable.h"


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

hPrintf("<center>");
hPrintf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=1 BGCOLOR=\"#"HG_COL_INSIDE"\">\n");

/* Write out first row - labels. */
hPrintf("<TR BGCOLOR=\"#"HG_COL_HEADER"\">");
hPrintf("<TH ALIGN=left>Name</TH>");
hPrintf("<TH ALIGN=left>On</TH>");
//hPrintf("<TH ALIGN=left>Position</TH>");
hPrintf("<TH ALIGN=left>Description</TH>");
//hPrintf("<TH ALIGN=left>Configuration</TH>");

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

    ///* Do left/right button */
    //hPrintf("<TD ALIGN=CENTER>");
    //safef(varName, sizeof(varName), "gsidTable.do.up.%s", col->name);
    //if (col != colList)
    //    {
    //    hPrintf("<INPUT NAME=\"%s\" TYPE=\"IMAGE\" VALUE=\"up\" ", varName);
    //    hPrintf("SRC=\"../images/up.gif\">");
    //    }
    //safef(varName, sizeof(varName), "gsidTable.do.down.%s", col->name);
    //if (col->next != NULL)
    //    {
    //    hPrintf("<INPUT NAME=\"%s\" TYPE=\"IMAGE\" VALUE=\"down\" ", varName);
    //    hPrintf("SRC=\"../images/down.gif\">");
    //    }
    //hPrintf("</TD>");

    /* Do long label. */
    hPrintf("<TD>%s</TD>", col->longLabel);

    ///* Do configuration controls. */
    //if (col->configControls != NULL)
    //     col->configControls(col);
    //else
    //     hPrintf("<TD>n/a</TD>");

    hPrintf("</TR>\n");
    }
hPrintf("</TABLE>\n");
hPrintf("</center>");
}

// NOTE: this only needed if we start letting user change column order
//
//static void savePriorities(struct column *colList)
///* Save priority list. */
//{
//struct dyString *dy = dyStringNew(4*1024);
//struct column *col;
//
//for (col = colList; col != NULL; col = col->next)
//    dyStringPrintf(dy, "%s %f ", col->name, col->priority);
//cartSetString(cart, colOrderVar, dy->string);
//dyStringFree(&dy);
//}

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
cgiMakeButton(hideAllConfName, "hide all");
hPrintf("</TD><TD>");
cgiMakeButton(showAllConfName, "show all");
hPrintf("</TD><TD>");
cgiMakeButton(defaultConfName, "default");
hPrintf("</TD><TD>");
//hPrintf("Settings:");
//hPrintf("</TD><TD>");
//cgiMakeButton(saveCurrentConfName, "save");
//hPrintf("</TD><TD>");
//cgiMakeOptionalButton(useSavedConfName, "load", !userSettingsAnySaved(us));
hPrintf("</TD></TR></TABLE>");

//hPrintf("<TABLE BORDER=0 CELLSPACING=1 CELLPADDING=1>\n");
//hPrintf("<TR><TD ALIGN=LEFT>");
//hPrintf("Expression ratio colors: ");
//hgExpColorDropDown(cart, expRatioColorVarName);
//hPrintf("</TD><TD>");
//hPrintf("Show all splicing variants: ");
//cgiMakeCheckBox(showAllSpliceVarName,
//        cartUsualBoolean(cart, showAllSpliceVarName, FALSE));
//hPrintf("</TD><TD>");
//cgiMakeButton(customPageDoName, "custom columns");
//hPrintf("</TD></TR></TABLE>");
controlPanelEnd();
}

void doConfigure(struct sqlConnection *conn, struct column *colList) //, char *bumpVar)
/* Generate configuration page. */
{
//if (bumpVar)
//    bumpColList(bumpVar, &colList);
hPrintf("<FORM ACTION=\"../cgi-bin/gsidTable\" METHOD=POST>\n");
//makeTitle("Configure Table View", "gsidTableHelp.html#Configure");
hotLinks();
configControlPanel();
cartSaveSession(cart);

configTable(colList, conn);
hPrintf("</FORM>");
}

// only needed if we allow the order to be changed
//
//static void restoreDefaultOrder(struct column **pColList)
///* Restore order of columns to default using priority settings. */
//{
//struct column *col;
//for (col = *pColList; col != NULL; col = col->next)
//    {
//    char *priority = columnSetting(col, "priority", NULL);
//    if (priority != NULL)
//        col->priority = atof(priority);
//    }
//slSort(pColList, columnCmpPriority);
//}

void doDefaultConfigure(struct sqlConnection *conn, struct column *colList)
/* Do configuration starting with defaults. */
{
struct column *col;
for (col=colList; col != NULL; col = col->next)
    col->on = col->defaultOn;
cartRemovePrefix(cart, colConfigPrefix);
cartRemove(cart, colOrderVar);
//restoreDefaultOrder(&colList);
doConfigure(conn, colList); //, NULL);
}

static void  configAllVis(struct sqlConnection *conn, struct column *colList,
        boolean how)
/* Respond to hide all button in configuration page. */
{
char varName[64];
struct column *col;
for (col = colList; col != NULL; col = col->next)
    {
    safef(varName, sizeof(varName), "%s%s.vis", colConfigPrefix, col->name);
    cartSetBoolean(cart, varName, how);
    }
doConfigure(conn, colList); //, NULL);
}

void doConfigHideAll(struct sqlConnection *conn, struct column *colList)
/* Respond to hide all button in configuration page. */
{
configAllVis(conn, colList, 0);
}

void doConfigShowAll(struct sqlConnection *conn, struct column *colList)
/* Respond to hide all button in configuration page. */
{
configAllVis(conn, colList, 1);
}




