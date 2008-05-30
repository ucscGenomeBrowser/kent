#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "hCommon.h"
#include "cart.h"
#include "cheapcgi.h"
#include "chromGraph.h"
#include "web.h"
#include "hPrint.h"
#include "hui.h"
#include "hgGenome.h"

static char const rcsid[] = "$Id: configure.c,v 1.17 2008/05/30 21:06:57 hiram Exp $";

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
cartWebStart(cart, "Configure Genome Graphs");
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
hPrintf("</TR>\n");
hPrintf("</TABLE>\n");
hPrintf("<TABLE><TR><TD>\n");
hPrintf("chromosome layout: ");
cgiMakeDropList(hggChromLayout, chromLayouts, ArraySize(chromLayouts), 
        chromLayout());
hPrintf("</TD></TR></TABLE>\n");
hPrintf("<TABLE><TR><TD>\n");
hPrintf("numerical labels: ");
cartMakeCheckBox(cart, hggLabels, TRUE);
hPrintf(" <I>Label axis on left for first graph and on right for last graph</I>");
hPrintf("</TD></TR></TABLE>\n");
hPrintf("<TABLE><TR><TD>\n");
hPrintf("highlight missing: ");
cartMakeCheckBox(cart, hggYellowMissing, FALSE);
hPrintf(" <I>Highlight background in yellow/gray if there is missing data in first graph</I>");
hPrintf("</TD></TR></TABLE>\n");
hPrintf("<TABLE><TR><TD>\n");
hPrintf("region padding: ");
cgiMakeIntVar(hggRegionPad, regionPad(), 6);
hPrintf(" <I>Number of bases to add to either side of regions over threshold</I>");
hPrintf("</TD></TR></TABLE>\n");
hPrintf("<TABLE><TR><TD>\n");
cgiMakeButton("submit", "submit");
hPrintf("</TD></TR></TABLE>\n");
hPrintf("</TD>\n");
hPrintf("</FORM>\n");

webNewSection("Configure Graphs");
hPrintf("Click on the hyperlink by the graph name to configure it.");
hTableStart();
hPrintf("<TR><TH>name</TH>");
hPrintf("<TH>description</TH></TR>");
struct slRef *ref;
for (ref = ggList; ref != NULL; ref = ref->next)
    {
    struct genoGraph *gg = ref->val;
    /* Only show custom graphs, stand-alone DB graphs, and composite */
    /* graphs.  Don't show subGraphs part of a composite. */
    if (gg->isSubGraph == FALSE)
	{
	char *tmp = cgiEncode(gg->name);
	hPrintf("<TR><TD><A HREF=\"../cgi-bin/hgGenome?%s&%s=on&g=%s\">",
		cartSidUrlString(cart), hggConfigureOne, tmp);
	freeMem(tmp);
	hPrintf("%s", gg->shortLabel);
	hPrintf("</A></TD>", gg->shortLabel);
	hPrintf("<TD>%s</TD></TR>\n", gg->longLabel);
	}
    }
hTableEnd();
cartWebEnd();
}

void configureOnePage()
/* Put up configuration for one graph. */
{
/* Figure out which graph we're configuring. */
char *graphName = cartString(cart, "g");
struct genoGraph *gg = hashFindVal(ggHash, graphName);
if (gg == NULL)
    {
    /* Warn/return rather than abort if have problems, so that 
     * cartRemovePrefix(hggDo) is executed to keep us from error
     * loop forever... */
    warn("Graph %s not found", graphName);
    return;
    }

/* Put up web page with controls */
cartWebStart(cart, "Configure %s", gg->shortLabel);
hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\" METHOD=GET>\n");
cartSaveSession(cart);
cgiMakeHiddenVar(hggConfigure, "on");
struct chromGraphSettings *cgs = gg->settings;
char varName[chromGraphVarNameMaxSize];
chromGraphVarName(gg->name, "minVal", varName);
hPrintf("display min value: ");
cartMakeIntVar(cart, varName, cgs->minVal, 5);
chromGraphVarName(gg->name, "maxVal", varName);
hPrintf(" max value: ");
cartMakeIntVar(cart, varName, cgs->maxVal, 5);
hPrintf("<BR>\n");
hPrintf("draw connecting lines between markers separated by up to ");
chromGraphVarName(gg->name, "maxGapToFill", varName);
cartMakeIntVar(cart, varName, cgs->maxGapToFill, 8);
hPrintf(" bases.");
hPrintf("<BR>\n");
cgiMakeButton("submit", "submit");
hPrintf("</FORM>\n");
cartWebEnd();
}

