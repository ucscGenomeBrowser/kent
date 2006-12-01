#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "hCommon.h"
#include "cart.h"
#include "cheapcgi.h"
#include "web.h"
#include "hPrint.h"
#include "hgGenome.h"

static char const rcsid[] = "$Id: configure.c,v 1.6 2006/12/01 23:57:02 kent Exp $";

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
cgiMakeIntVar(hggImageWidth, cartUsualInt(cart, hggImageWidth, hgDefaultPixWidth), 4);
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
hPrintf("<TABLE><TR><TD>\n");
hPrintf("numerical labels: ");
cartMakeCheckBox(cart, hggLabels, TRUE);
hPrintf(" <I>Label axis on left for first graph and on right for last graph</I>");
hPrintf("</TD></TR></TABLE>\n");
hPrintf("<TABLE><TR><TD>\n");
hPrintf("region pad: ");
cgiMakeIntVar(hggRegionPad, regionPad(), 6);
hPrintf(" <I>Number of bases to add to either side of regions over threshold</I>");
hPrintf("</TD></TR></TABLE>\n");
int regionPad();

hPrintf("</FORM>\n");
cartWebEnd();
}
