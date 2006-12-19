/* mainPage - draws the main hgGenome page, including some controls
 * on the top and the graphic. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"
#include "hui.h"
#include "dbDb.h"
#include "ra.h"
#include "hdb.h"
#include "web.h"
#include "portable.h"
#include "hgColors.h"
#include "trackLayout.h"
#include "chromInfo.h"
#include "vGfx.h"
#include "genoLay.h"
#include "cytoBand.h"
#include "hCytoBand.h"
#include "errCatch.h"
#include "chromGraph.h"
#include "customTrack.h"
#include "hPrint.h"
#include "jsHelper.h"
#include "hgGenome.h"

static char const rcsid[] = "$Id: mainPage.c,v 1.5 2006/12/19 20:19:15 kent Exp $";


static char *allColors[] = {
    "black", "blue", "purple", "red", "orange", "yellow", "green", "gray",
};

static char *defaultColors[maxLinesOfGraphs][maxGraphsPerLine] = {
    {"black", "blue", "red", "yellow", },
    {"gray", "purple", "green", "orange",},
    {"black", "blue", "red", "yellow", },
    {"gray", "purple", "green", "orange",},
    {"black", "blue", "red", "yellow", },
    {"gray", "purple", "green", "orange",},
};

char *graphColorAt(int row, int col)
/* Return graph color at given row/column, NULL if nonw. */
{
char *varName = graphColorVarName(row, col);
char *color = cartUsualString(cart, varName, defaultColors[row][col]);
if (color == NULL) color = defaultColors[0][0];
return color;
}

Color colorFromAscii(struct vGfx *vg, char *asciiColor)
/* Get color index for a named color. */
{
if (sameWord("red", asciiColor))
    return MG_RED;
else if (sameWord("blue", asciiColor))
    return MG_BLUE;
else if (sameWord("yellow", asciiColor))
    return vgFindColorIx(vg, 190, 190, 0);
else if (sameWord("purple", asciiColor))
    return vgFindColorIx(vg, 150, 0, 200);
else if (sameWord("orange", asciiColor))
    return vgFindColorIx(vg, 230, 120, 0);
else if (sameWord("green", asciiColor))
    return vgFindColorIx(vg, 0, 180, 0);
else if (sameWord("gray", asciiColor))
    return MG_GRAY;
else
    return MG_BLACK;
}

/* Page drawing stuff. */

void drawChromGraph(struct vGfx *vg, struct sqlConnection *conn, 
	struct genoLay *gl, char *chromGraph, int yOff, int height, Color color,
	boolean leftLabel, boolean rightLabel, boolean firstInRow)
/* Draw chromosome graph on all chromosomes in layout at given
 * y offset and height. */
{
boolean yellowMissing = getYellowMissing();
struct genoGraph *gg = hashFindVal(ggHash, chromGraph);
if (gg != NULL)
    {
    /* Get binary data source and scaling info etc. */
    struct chromGraphBin *cgb = gg->cgb;
    struct chromGraphSettings *cgs = gg->settings;
    int maxGapToFill = cgs->maxGapToFill;
    static struct rgbColor missingDataColor = { 200, 200, 100};
    Color missingColor = vgFindRgb(vg, &missingDataColor);
    double pixelsPerBase = 1.0/gl->basesPerPixel;
    double gMin = cgs->minVal, gMax = cgs->maxVal, gScale;
    gScale = height/(gMax-gMin);
    // uglyf("chromGraph %s, gMin %f, gMax %f, Height %d, gScale %f\n<BR>\n", chromGraph, gMin, gMax, height, gScale);

    /* Draw significance threshold as a light blue line */
    if (leftLabel)
        {
	static struct rgbColor guidelineColor = { 220, 220, 255};
	Color lightBlue = vgFindRgb(vg, &guidelineColor);
	struct slRef *ref;
	struct genoLayChrom *chrom;
	int rightX = gl->picWidth - gl->rightLabelWidth - gl->margin;
	int leftX = gl->leftLabelWidth + gl->margin;
	int width = rightX - leftX;
	double threshold = cartUsualDouble(cart, hggThreshold, 
		defaultThreshold);
	int y = height - ((threshold - gMin)*gScale) + yOff;
	for (ref = gl->leftList; ref != NULL; ref = ref->next)
	    {
	    chrom = ref->val;
	    vgBox(vg, leftX, y + chrom->y, width, 1, lightBlue);
	    }
	ref = gl->bottomList;
	if (ref != NULL)
	    {
	    chrom = ref->val;
	    vgBox(vg, leftX, y + chrom->y, width, 1, lightBlue);
	    }
	}

    /* Draw graphs on each chromosome */
    chromGraphBinRewind(cgb);
    while (chromGraphBinNextChrom(cgb))
	{
	struct genoLayChrom *chrom = hashFindVal(gl->chromHash, cgb->chrom);
	if (chrom)
	    {
	    int chromX = chrom->x, chromY = chrom->y;
	    int minY = chromY + yOff;
	    int maxY = chromY + yOff + height - 1;

	    if (chromGraphBinNextVal(cgb))
	        {
		/* Handle first point as special case here, so don't
		 * have to test for first point in inner loop. */
		int x,y,start,lastStart,lastX,lastY;
		start = lastStart = cgb->chromStart;
		x = lastX = pixelsPerBase*start + chromX;
		y = lastY = (height - ((cgb->val - gMin)*gScale)) + chromY+yOff;
		if (y < minY) y = minY;
		else if (y > maxY) y = maxY;
		vgDot(vg, x, y, color);

		/* Draw rest of points, connecting with line to previous point
		 * if not too far off. */
		while (chromGraphBinNextVal(cgb))
		    {
		    start = cgb->chromStart;
		    x = pixelsPerBase*start + chromX;
		    y = (height - ((cgb->val - gMin)*gScale)) + chromY+yOff;
		    if (y < minY) y = minY;
		    else if (y > maxY) y = maxY;
		    if (x != lastX || y != lastY)
		        {
			if (start - lastStart <= maxGapToFill)
			    vgLine(vg, lastX, lastY, x, y, color);
			else
			    {
			    if (yellowMissing && firstInRow)
			        {
				int width = x - lastX - 1;
				if (width > 0)
				    vgBox(vg, lastX+1, minY, width, height, missingColor);
				}
			    vgDot(vg, x, y, color);
			    }
			}
		    lastX = x;
		    lastY = y;
		    lastStart = start;
		    }
		}
	    }
	else
	    {
	    /* Just read and discard data. */
	    while (chromGraphBinNextVal(cgb))
	        ;
	    }
	}


    /* Draw labels. */
    if (withLabels && (leftLabel || rightLabel))
        {
	int lineY = yOff;
	int i,j;
	int spaceWidth = tl.nWidth;
	int tickWidth = spaceWidth*2/3;
	int fontPixelHeight = mgFontPixelHeight(gl->font);
	for (i=0; i<gl->lineCount; ++i)
	    {
	    for (j=0; j< cgs->linesAtCount; ++j)
	        {
		double lineAt = cgs->linesAt[j];
		int y = (height - ((lineAt - gMin)*gScale)) + lineY;
		int textTop = y - fontPixelHeight/2+1;
		int textBottom = textTop + fontPixelHeight;
		char label[24];
		safef(label, sizeof(label), "%g", lineAt);
		if (leftLabel)
		    {
		    vgBox(vg, gl->margin + gl->leftLabelWidth - tickWidth-1, y,
		    	tickWidth, 1, color);
		    if (textTop >= lineY && textBottom < lineY + height)
			{
			vgTextRight(vg, gl->margin, textTop, 
			    gl->leftLabelWidth-spaceWidth, fontPixelHeight,
			    color, gl->font, label);
			}
		    }
		if (rightLabel)
		    {
		    vgBox(vg, 
		    	gl->picWidth - gl->margin - gl->rightLabelWidth+1, 
		    	y, tickWidth, 1, color);
		    if (textTop >= lineY && textBottom < lineY + height)
			{
			vgText(vg,
			    gl->picWidth - gl->margin - gl->rightLabelWidth + spaceWidth,
			    textTop, color, gl->font, label);
			}
		    }
		}
	    lineY += gl->lineHeight;
	    }
	}
    }
}

void genomeGif(struct sqlConnection *conn, struct genoLay *gl,
	int graphRows, int graphCols, int oneRowHeight)
/* Create genome GIF file and HTML that includes it. */
{
struct vGfx *vg;
struct tempName gifTn;
Color shadesOfGray[10];
int maxShade = ArraySize(shadesOfGray)-1;
int spacing = 1;
int yOffset = 2*spacing;
int innerHeight = oneRowHeight - 3*spacing;
int i,j;

/* Create gif file and make reference to it in html. */
makeTempName(&gifTn, "hgtIdeo", ".gif");
vg = vgOpenGif(gl->picWidth, gl->picHeight, gifTn.forCgi);

hPrintf("<INPUT TYPE=IMAGE SRC=\"%s\" BORDER=1 WIDTH=%d HEIGHT=%d NAME=\"%s\">",
	    gifTn.forHtml, gl->picWidth, gl->picHeight, hggClick);

/* Get our grayscale. */
hMakeGrayShades(vg, shadesOfGray, maxShade);

/* Draw the labels and then the chromosomes. */
genoLayDrawChromLabels(gl, vg, MG_BLACK);
genoLayDrawBandedChroms(gl, vg, database, conn, 
	shadesOfGray, maxShade, MG_BLACK);

/* Draw chromosome graphs. */
for (i=0; i<graphRows; ++i)
    {
    boolean firstInRow = TRUE;
    for (j=graphCols-1; j>=0; --j)
	{
	char *graph = graphSourceAt(i,j);
	if (graph != NULL && graph[0] != 0)
	    {
	    Color color = colorFromAscii(vg, graphColorAt(i,j));
	    drawChromGraph(vg, conn, gl, graph, 
		    gl->betweenChromOffsetY + yOffset, 
		    innerHeight,  color, j==0, j==1, firstInRow);
	    firstInRow = FALSE;
	    }
	}
    yOffset += oneRowHeight;
    }

vgBox(vg, 0, 0, gl->picWidth, 1, MG_GRAY);
vgBox(vg, 0, gl->picHeight-1, gl->picWidth, 1, MG_GRAY);
vgBox(vg, 0, 0, 1, gl->picHeight, MG_GRAY);
vgBox(vg, gl->picWidth-1, 0, 1, gl->picHeight, MG_GRAY);
vgClose(&vg);
}

void graphDropdown(struct sqlConnection *conn, char *varName, char *curVal, char *js)
/* Make a drop-down with available chrom graphs */
{
int totalCount = 1 + slCount(ggList);
char **menu, **values;
int i = 0;
struct slRef *ref;

AllocArray(menu, totalCount);
AllocArray(values, totalCount);
menu[0] = "-- nothing --";
values[0] = "";

for (ref = ggList; ref != NULL; ref = ref->next)
    {
    struct genoGraph *gg = ref->val;
    ++i;
    menu[i] = gg->shortLabel;
    values[i] = gg->name;
    }
cgiMakeDropListFull(varName, menu, values, totalCount, curVal, js);
freez(&menu);
freez(&values);
}

void colorDropdown(int row, int col, char *js)
/* Put up color drop down menu. */
{
char *varName = graphColorVarName(row, col);
char *curVal = graphColorAt(row, col);
cgiMakeDropListFull(varName, allColors, allColors, ArraySize(allColors), curVal, js);
}

static void addThresholdGraphCarries(struct dyString *dy, int graphRows, int graphCols)
/* Add javascript that carries over threshold and graph vars
 * to new form. */
{
jsTextCarryOver(dy, hggThreshold);
int i,j;
for (i=0; i<graphRows; ++i)
    for (j=0; j<graphCols; ++j)
        {
	jsDropDownCarryOver(dy, graphVarName(i,j));
	jsDropDownCarryOver(dy, graphColorVarName(i,j));
	}
}

static struct dyString *onChangeStart(int graphRows, int graphCols)
/* Return common prefix to onChange javascript string */
{
struct dyString *dy = jsOnChangeStart();
addThresholdGraphCarries(dy, graphRows, graphCols);
return dy;
}

static char *onChangeClade(int graphRows, int graphCols)
/* Return javascript executed when they change clade. */
{
struct dyString *dy = onChangeStart(graphRows, graphCols);
jsDropDownCarryOver(dy, "clade");
dyStringAppend(dy, " document.hiddenForm.org.value=0;");
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
return jsOnChangeEnd(&dy);
}

static char *onChangeOrg(int graphRows, int graphCols)
/* Return javascript executed when they change organism. */
{
struct dyString *dy = onChangeStart(graphRows, graphCols);
jsDropDownCarryOver(dy, "clade");
jsDropDownCarryOver(dy, "org");
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
return jsOnChangeEnd(&dy);
}

static void saveOnChangeOtherFunction(int graphRows, int graphCols)
/* Write out Javascript function to save vars in hidden
 * form and submit. */
{
struct dyString *dy = dyStringNew(0);
addThresholdGraphCarries(dy, graphRows, graphCols);
jsDropDownCarryOver(dy, "clade");
jsDropDownCarryOver(dy, "org");
jsDropDownCarryOver(dy, "db");
char *js = jsOnChangeEnd(&dy);
chopSuffixAt(js, '"');
hPrintf("<SCRIPT>\n");
hPrintf("function changeOther()\n");
hPrintf("{\n");
hPrintf("%s\n", js);
hPrintf("}\n");
hPrintf("</SCRIPT>\n");
}

static char *onChangeOther()
/* Return javascript executed when they change database. */
{
return "onChange=\"changeOther();\"";
}

void mainPage(struct sqlConnection *conn)
/* Do main page of application:  hotlinks bar, controls, graphic. */
{
struct genoLay *gl;
int graphRows = linesOfGraphs();
int graphCols = graphsPerLine();
int i, j;
int realCount = 0;
char *scriptName = "/cgi-bin/hgGenome";

cartWebStart(cart, "%s Genome Graphs", genome);

/* Start form and save session var. */
hPrintf("<FORM ACTION=\"..%s\" NAME=\"mainForm\" METHOD=GET>\n", scriptName);
cartSaveSession(cart);

/* Write some javascript functions */
jsWriteFunctions();
saveOnChangeOtherFunction(graphRows, graphCols);

/* Print clade, genome and assembly line. */
boolean gotClade = hGotClade();
char *jsOther = onChangeOther(graphRows, graphCols);
    {
    hPrintf("<TABLE>");
    if (gotClade)
	{
	hPrintf("<TR><TD><B>clade:</B>\n");
	printCladeListHtml(hGenome(database), onChangeClade(graphRows, graphCols));
	htmlNbSpaces(3);
	hPrintf("<B>genome:</B>\n");
	printGenomeListForCladeHtml(database, onChangeOrg(graphRows, graphCols));
	}
    else
	{
	hPrintf("<TR><TD><B>genome:</B>\n");
	printGenomeListHtml(database, onChangeOrg(graphRows, graphCols));
	}
    htmlNbSpaces(3);
    hPrintf("<B>assembly:</B>\n");
    printAssemblyListHtml(database, jsOther);
    hPrintf("</TD></TR>\n");
    hPrintf("</TABLE>");
    }

/* Draw graph controls. */
hPrintf("<TABLE>");
for (i=0; i<graphRows; ++i)
    {
    hPrintf("<TR>");
    hPrintf("<TD><B>graph</B></TD>");
    for (j=0; j<graphCols; ++j)
	{
	char *varName = graphVarName(i,j);
	char *curVal = cartUsualString(cart, varName, "");
	if (curVal[0] != 0)
	    ++realCount;
	hPrintf("<TD>");
	graphDropdown(conn, varName, curVal, jsOther);
	hPrintf(" in ");
	colorDropdown(i, j, jsOther);
	if (j != graphCols-1) hPrintf(",");
	hPrintf("</TD>");
	}
    hPrintf("</TR>");
    }
hPrintf("</TABLE>");
cgiMakeButton(hggUpload, "Upload");
hPrintf(" ");
cgiMakeButton(hggConfigure, "Configure");
hPrintf(" ");
cgiMakeOptionalButton(hggCorrelate, "Correlate", realCount < 2);
hPrintf(" significance threshold:");
cartMakeDoubleVar(cart, hggThreshold, defaultThreshold,  3);
hPrintf(" ");
cgiMakeOptionalButton(hggBrowse, "Browse Regions", realCount == 0);
hPrintf(" ");
cgiMakeOptionalButton(hggSort, "Sort Genes", 
	realCount == 0 || !hgNearOk(database));
hPrintf("<BR>");


if (ggList != NULL)
    {
    /* Get genome layout.  This can fail so it is wrapped in an error
     * catcher. */
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
	{
	gl = ggLayout(conn, graphRows, graphCols);

	/* Draw picture. Enclose in table to add a couple of pixels between
	 * it and controls on IE. */
	hPrintf("<TABLE CELLPADDING=2><TR><TD>\n");
	genomeGif(conn, gl, graphRows, graphCols, graphHeight()+betweenRowPad);
	hPrintf("</TD></TR></TABLE>\n");

	/* Write a little click-on-help */
	hPrintf("<i>Click on a chromosome to open Genome Browser at that position.</i>");

	}
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	 warn(errCatch->message->string);
    errCatchFree(&errCatch); 
    }
else
    {
    hPrintf("<BR>No graph data is available for this assembly.  You can still upload your own "
            "data though.");
    }
hPrintf("</FORM>\n");

/* Hidden form - fo the benefit of javascript. */
    {
    /* Copy over both the regular, non-changing variables, and
    * also all the source/color pairs that depend on the 
    * configuration. */
    static char *regularVars[] = {
      "clade", "org", "db", hggThreshold,
      };
    int regularCount = ArraySize(regularVars);
    int varCount = regularCount + 2 * graphRows * graphCols;
    int varIx = regularCount;
    char **allVars;
    AllocArray(allVars, varCount);
    CopyArray(regularVars, allVars, regularCount);
    for (i=0; i<graphRows; ++i)
        {
	for (j=0; j<graphCols; ++j)
	    {
	    allVars[varIx++] = cloneString(graphVarName(i,j));
	    allVars[varIx++] = cloneString(graphColorVarName(i,j));
	    }
	}
    jsCreateHiddenForm(cart, scriptName, allVars, varCount);
    }

cartWebEnd();
}

