#include "common.h"
#include "hCommon.h"
#include "cart.h"
#include "cheapcgi.h"
#include "web.h"
#include "hPrint.h"
#include "hgHeatmap.h"

static char const rcsid[] = "$Id: configure.c,v 1.2 2007/07/19 01:40:23 jzhu Exp $";

void makeNumMenu(char *varName, int minVal, int maxVal, int defaultVal)
/* Make a drop down menu with a limited number of numerical choices. */
{
int choiceCount = maxVal - minVal + 1;
int i;

printf("<SELECT NAME=\"%s\">\n", varName);
for (i=0; i<choiceCount; ++i)
    {
    int choice = i + minVal;
    char *selString = ((choice == defaultVal) ? " SELECTED" : "");
    hPrintf("<OPTION%s>%d</OPTION>\n", selString, choice);
    }
printf("</SELECT>\n");
}

char *chromLayouts[] = {
    layTwoPerLine,
    layOnePerLine,
    layAllOneLine,
    };


void configurePage()
/* Put up configuration page. */
{
cartWebStart(cart, "Configure Genome Heatmaps");
hPrintf("<FORM ACTION=\"../cgi-bin/hgHeatmap\" METHOD=GET>\n");
cartSaveSession(cart);

hPrintf("<TABLE>\n");
hPrintf("<TR><TD>\n");
hPrintf("image width:");
hPrintf("</TD><TD>\n");
cgiMakeIntVar(hghImageWidth, cartUsualInt(cart, hghImageWidth, hgHeatmapDefaultPixWidth), 4);
hPrintf("</TD></TR>\n");

hPrintf("<TR><TD>\n");
hPrintf("experiment height: ");
hPrintf("</TD><TD>\n");
cgiMakeIntVar(hghExperimentHeight, experimentHeight(), 1);
hPrintf("</TD></TR>\n");

hPrintf("<TR><TD>\n");
hPrintf("chromosome layout: ");
hPrintf("</TD><TD>\n");
cgiMakeDropList(hghChromLayout, chromLayouts, ArraySize(chromLayouts), 
        chromLayout());
hPrintf("</TD></TR>\n");
hPrintf("</TABLE>\n");

cgiMakeButton("submit", "submit");
hPrintf("</FORM>\n");

hTableEnd();
cartWebEnd();
}
