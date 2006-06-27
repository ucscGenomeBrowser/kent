#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "hCommon.h"
#include "cart.h"
#include "cheapcgi.h"
#include "web.h"
#include "hgGenome.h"

static char const rcsid[] = "$Id: configure.c,v 1.2 2006/06/27 23:46:27 kent Exp $";

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


void configurePage()
/* Put up configuration page. */
{
cartWebStart(cart, "Configure Genome Association View");
hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\" METHOD=GET>\n");
cartSaveSession(cart);
hPrintf("<TABLE>\n");
hPrintf("<TR>\n");
hPrintf("<TD>\n");
hPrintf("image width: ");
cgiMakeIntVar("pix", cartUsualInt(cart, "pix", hgDefaultPixWidth), 4);
hPrintf("</TD>\n");
hPrintf("<TD>\n");
hPrintf("graph height: ");
cgiMakeIntVar(hggGraphHeight, graphHeight(), 3);
hPrintf("</TD>\n");
hPrintf("<TD>\n");
hPrintf(" graphs per line: ");
makeNumMenu(hggGraphsPerLine, minGraphsPerLine, maxGraphsPerLine, 
	graphsPerLine());
hPrintf("</TD>\n");
hPrintf("<TD>\n");
hPrintf(" lines of graphs: ");
makeNumMenu(hggLinesOfGraphs, minLinesOfGraphs, maxLinesOfGraphs, 
	linesOfGraphs());
hPrintf("</TD>\n");
hPrintf("<TD>\n");
cgiMakeButton("submit", "Submit");
hPrintf("</TD>\n");
hPrintf("</TR>\n");
hPrintf("</TABLE>\n");
hPrintf("</FORM>\n");
cartWebEnd();
}
