/* hgGenome is a CGI script that produces a web page containing
 * a graphic with all chromosomes in genome, and a graph or two
 * on top of them. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
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
#include "chromGraph.h"
#include "hgGenome.h"

static char const rcsid[] = "$Id: hgGenome.c,v 1.32 2006/07/01 20:10:32 kent Exp $";

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
struct hash *oldCart;	/* Old cart hash. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
char *genome;		/* Name of genome - mouse, human, etc. */
struct trackLayout tl;  /* Dimensions of things, fonts, etc. */
struct genoGraph *ggList; /* List of active genome graphs */
struct hash *ggHash;	  /* Hash of active genome graphs */
boolean withLabels;	/* Draw labels? */


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGenome - Full genome (as opposed to chromosome) view of data\n"
  "This is a cgi script, but it takes the following parameters:\n"
  "   db=<genome database>\n"
  "   hggt_table=on where table is name of chromGraph table\n"
  );
}

/* ---- Get list of graphs. ---- */

struct genoGraph *getUserGraphs()
/* Get list of all user graphs */
{
struct genoGraph *list = NULL, *gg;
char *fileName = cartOptionalString(cart, hggUploadRa);
if (fileName != NULL && fileExists(fileName))
    {
    struct hash *ra;
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    while ((ra = raNextRecord(lf)) != NULL)
        {
	char *name = hashFindVal(ra, "name");
	char *binaryFile = hashFindVal(ra, "binaryFile");
	if (name != NULL && binaryFile != NULL && fileExists(binaryFile))
	    {
	    char nameBuf[256];
	    safef(nameBuf, sizeof(nameBuf), "%s%s", hggUserTag, name);
	    AllocVar(gg);
	    gg->name = cloneString(nameBuf);
	    gg->shortLabel = name;
	    gg->longLabel = hashFindVal(ra, "description");
	    if (gg->longLabel == NULL) gg->longLabel = name;
	    gg->binFileName = binaryFile;
	    gg->settings = chromGraphSettingsGet(name, NULL, NULL, NULL);
	    chromGraphSettingsFillFromHash(gg->settings, ra, name);
	    slAddHead(&list, gg);
	    }
	}
    }
slReverse(&list);
return list;
}

struct genoGraph *getDbGraphs(struct sqlConnection *conn)
/* Get graphs defined in database. */
{
struct genoGraph *list = NULL, *gg;
char *trackDbTable = hTrackDbName();
struct sqlConnection *conn2 = hAllocConn();
struct sqlResult *sr;
char **row;

/* Get initial information from metaChromGraph table */
if (sqlTableExists(conn, "metaChromGraph"))
    {
    sr = sqlGetResult(conn, "select name,binaryFile from metaChromGraph");
    while ((row = sqlNextRow(sr)) != NULL)
        {
	char *table = row[0], *binaryFile = row[1];

	AllocVar(gg);
	gg->name = gg->shortLabel = gg->longLabel = cloneString(table);
	gg->binFileName = cloneString(binaryFile);
	slAddHead(&list, gg);
	}
    sqlFreeResult(&sr);
    }

/* Where possible fill in additional info from trackDb.  Also
 * add db: prefix to name. */
for (gg = list; gg != NULL; gg = gg->next)
    {
    char query[512];
    char nameBuf[256];
    safef(query, sizeof(query), 
    	"select * from %s where tableName='%s'", trackDbTable, gg->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
	struct trackDb *tdb = trackDbLoad(row);
	struct chromGraphSettings *cgs = chromGraphSettingsGet(gg->name,
		conn2, tdb, cart);
	gg->shortLabel = tdb->shortLabel;
	gg->longLabel = tdb->longLabel;
	gg->settings = cgs;
	}
    else
        gg->settings = chromGraphSettingsGet(gg->name, NULL, NULL, NULL);
    sqlFreeResult(&sr);

    /* Add db: prefix to separate user and db name space. */
    safef(nameBuf, sizeof(nameBuf), "%s%s", hggDbTag, gg->name);
    gg->name = cloneString(nameBuf);
    }
hFreeConn(&conn2);
slReverse(&list);
return list;
}

void getGenoGraphs(struct sqlConnection *conn)
/* Set up ggList and ggHash with all available genome graphs */
{
struct genoGraph *userList = getUserGraphs();
struct genoGraph *dbList = getDbGraphs(conn);
struct genoGraph *gg;
ggList = slCat(userList, dbList);
ggHash = hashNew(0);
for (gg = ggList; gg != NULL; gg = gg->next)
    {
    hashAdd(ggHash, gg->name, gg);
    }
}

void fakeEmptyLabels(struct chromGraphSettings *cgs)
/* If no labels, fake them at 1/3 and 2/3 of the way through. */
{
if (cgs->linesAtCount == 0)
    {
    cgs->linesAtCount = 2;
    AllocArray(cgs->linesAt, 2);
    double range = cgs->maxVal - cgs->minVal;
    cgs->linesAt[0] = cgs->minVal + range/3.0;
    cgs->linesAt[1] = cgs->minVal + 2.0*range/3.0;
    if (range >= 3)
        {
	cgs->linesAt[0] = round(cgs->linesAt[0]);
	cgs->linesAt[1] = round(cgs->linesAt[1]);
	}
    }
}

void ggRefineUsed(struct genoGraph *gg)
/* Refine genoGraphs that are used. */
{
if (!gg->didRefine)
    {
    struct chromGraphSettings *cgs = gg->settings;
    gg->cgb = chromGraphBinOpen(gg->binFileName);
    if (cgs->minVal >= cgs->minVal)
        {
	cgs->minVal = gg->cgb->minVal;
	cgs->maxVal = gg->cgb->maxVal;
	}
    fakeEmptyLabels(gg->settings);
    gg->didRefine = TRUE;
    }
}

int ggLabelWidth(struct genoGraph *gg, MgFont *font)
/* Return width used by graph labels. */
{
struct chromGraphSettings *cgs = gg->settings;
int i;
int minLabelWidth = 0, labelWidth;
char buf[24];
fakeEmptyLabels(cgs);
for (i=0; i<cgs->linesAtCount; ++i)
    {
    safef(buf, sizeof(buf), "%g ", cgs->linesAt[i]);
    labelWidth = mgFontStringWidth(font, buf);
    if (labelWidth > minLabelWidth)
        minLabelWidth = labelWidth;
    }
return minLabelWidth;
}

/* ---- Some html helper routines. ---- */

void hvPrintf(char *format, va_list args)
/* Print out some html. */
{
vprintf(format, args);
if (ferror(stdout))
    noWarnAbort();
}

void hPrintf(char *format, ...)
/* Print out some html.  Check for write error so we can
 * terminate if http connection breaks. */
{
va_list(args);
va_start(args, format);
hvPrintf(format, args);
va_end(args);
}

/* Routines to fetch cart variables. */

int graphHeight()
/* Return height of graph. */
{
return cartUsualIntClipped(cart, hggGraphHeight, 27, 5, 200);
}

int graphsPerLine()
/* Return number of graphs to draw per line. */
{
return cartUsualIntClipped(cart, hggGraphsPerLine,
	defaultGraphsPerLine, minGraphsPerLine, maxGraphsPerLine);
}


int linesOfGraphs()
/* Return number of lines of graphs */
{
return cartUsualIntClipped(cart, hggLinesOfGraphs,
	defaultLinesOfGraphs, minLinesOfGraphs, maxLinesOfGraphs);
}

char *graphVarName(int row, int col)
/* Get graph data source variable for given row and column.  Returns
 * static buffer. */
{
static char buf[32];
safef(buf, sizeof(buf), "%s_%d_%d", hggGraphPrefix, row+1, col+1);
return buf;
}

char *graphColorVarName(int row, int col)
/* Get graph color variable for givein row and column. Returns
 * static buffer. */
{
static char buf[32];
safef(buf, sizeof(buf), "%s_%d_%d", hggGraphColorPrefix, row+1, col+1);
return buf;
}

char *graphSourceAt(int row, int col)
/* Return graph source at given row/column, NULL if none. */
{
char *varName = graphVarName(row, col);
char *source = NULL;
if (cartVarExists(cart, varName))
    {
    source = skipLeadingSpaces(cartString(cart, varName));
    if (source[0] == 0)
        source = NULL;
    }
return source;
}

struct genoGraph *ggAt(int row, int col)
/* Return genoGraph at given position or NULL */
{
char *source = graphSourceAt(row, col);
if (source != NULL)
     return hashFindVal(ggHash, source);
return NULL;
}

struct genoGraph *ggFirstVisible()
/* Return first visible graph, or NULL if none. */
{
int graphRows = linesOfGraphs();
int graphCols = graphsPerLine();
struct genoGraph *gg = NULL;
int i,j;
for (i=0; i<graphRows; ++i)
   for (j=0; j<graphCols; ++j)
       if ((gg = ggAt(i,j)) != NULL)
           break;
return gg;
}

struct slRef *ggAllVisible()
/* Return list of references to all visible graphs */
{
struct slRef *list = NULL;
int graphRows = linesOfGraphs();
int graphCols = graphsPerLine();
struct genoGraph *gg = NULL;
int i,j;
for (i=0; i<graphRows; ++i)
   for (j=0; j<graphCols; ++j)
       if ((gg = ggAt(i,j)) != NULL)
           refAddUnique(&list, gg);
slReverse(&list);
return list;
}

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
	boolean leftLabel, boolean rightLabel)
/* Draw chromosome graph on all chromosomes in layout at given
 * y offset and height. */
{
struct genoGraph *gg = hashFindVal(ggHash, chromGraph);
if (gg != NULL)
    {
    struct chromGraphBin *cgb = gg->cgb;
    struct chromGraphSettings *cgs = gg->settings;
    int maxGapToFill = cgs->maxGapToFill;
    double pixelsPerBase = 1.0/gl->basesPerPixel;
    double gMin = cgs->minVal, gMax = cgs->maxVal, gScale;

    gScale = height/(gMax-gMin+1);
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
			    vgDot(vg, x, y, color);
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

// hPrintf("<TABLE><TR><TD BGCOLOR=#888888>\n");
hPrintf("<INPUT TYPE=IMAGE SRC=\"%s\" BORDER=1 WIDTH=%d HEIGHT=%d NAME=\"%s\">",
	    gifTn.forHtml, gl->picWidth, gl->picHeight, hggClick);
// hPrintf("</TD></TR></TABLE>\n");

/* Get our grayscale. */
hMakeGrayShades(vg, shadesOfGray, maxShade);

/* Draw the labels and then the chromosomes. */
genoLayDrawChromLabels(gl, vg, MG_BLACK);
genoLayDrawBandedChroms(gl, vg, database, conn, 
	shadesOfGray, maxShade, MG_BLACK);

/* Draw chromosome graphs. */
for (i=0; i<graphRows; ++i)
    {
    for (j=graphCols-1; j>=0; --j)
	{
	char *graph = graphSourceAt(i,j);
	if (graph != NULL && graph[0] != 0)
	    {
	    Color color = colorFromAscii(vg, graphColorAt(i,j));
	    drawChromGraph(vg, conn, gl, graph, 
		    gl->betweenChromOffsetY + yOffset, 
		    innerHeight,  color, j==0, j==1);
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

struct slName *userListAll()
/* List all graphs that user has uploaded */
{
return NULL;
}

void graphDropdown(struct sqlConnection *conn, char *varName, char *curVal)
/* Make a drop-down with available chrom graphs */
{
struct genoGraph *gg;
int totalCount = 1 + slCount(ggList);
char **menu, **values;
int i = 0;

AllocArray(menu, totalCount);
AllocArray(values, totalCount);
menu[0] = "";
values[0] = "";

for (gg = ggList; gg != NULL; gg = gg->next)
    {
    ++i;
    menu[i] = gg->shortLabel;
    values[i] = gg->name;
    }
cgiMakeDropListWithVals(varName, menu, values, totalCount, curVal);
freez(&menu);
freez(&values);
}

void colorDropdown(int row, int col)
/* Put up color drop down menu. */
{
char *varName = graphColorVarName(row, col);
char *curVal = graphColorAt(row, col);
cgiMakeDropList(varName, allColors, ArraySize(allColors), curVal);
}

struct genoLay *ggLayout(struct sqlConnection *conn, 
	int graphRows, int graphCols)
/* Figure out how to lay out image. */
{
int i,j;
struct genoLayChrom *chromList;
int oneRowHeight;
int minLeftLabelWidth = 0, minRightLabelWidth = 0;

/* Figure out basic dimensions of image. */
trackLayoutInit(&tl, cart);

/* Refine all graphs actually used, and calculate label
 * widths if need be. */
for (i=0; i<graphRows; ++i)
    {
    for (j=0; j<graphCols; ++j)
	{
	char *source = graphSourceAt(i,j);
	if (source != NULL)
	    {
	    struct genoGraph *gg = hashFindVal(ggHash, source);
	    if (gg != NULL)
		{
		ggRefineUsed(gg);
		if (withLabels)
		    {
		    int labelWidth;
		    labelWidth = ggLabelWidth(gg, tl.font);
		    if (j == 0 && labelWidth > minLeftLabelWidth)
			minLeftLabelWidth = labelWidth;
		    if (j == 1 && labelWidth > minRightLabelWidth)
			minRightLabelWidth = labelWidth;
		    }
		}
	    }
	}
    }

/* Get list of chromosomes and lay them out. */
chromList = genoLayDbChroms(conn, FALSE);
oneRowHeight = graphHeight()+betweenRowPad;
return genoLayNew(chromList, tl.font, tl.picWidth, graphRows*oneRowHeight,
	minLeftLabelWidth, minRightLabelWidth);
}

void mainPage(struct sqlConnection *conn)
/* Do main page of application:  hotlinks bar, controls, graphic. */
{
struct genoLay *gl;
int graphRows = linesOfGraphs();
int graphCols = graphsPerLine();
int i, j;
int realCount = 0;

cartWebStart(cart, "%s Genome Graphs", genome);

/* Start form and save session var. */
hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\" METHOD=GET>\n");
cartSaveSession(cart);

/* Draw graph controls. */
hPrintf("<TABLE>");
for (i=0; i<graphRows; ++i)
    {
    hPrintf("<TR>");
    if (graphRows == 1)
	{
	hPrintf("<TD>");
	    hPrintf("graph ");
	hPrintf("</TD>");
	}
    for (j=0; j<graphCols; ++j)
	{
	char *varName = graphVarName(i,j);
	char *curVal = cartUsualString(cart, varName, "");
	if (curVal[0] != 0)
	    ++realCount;
	hPrintf("<TD>");
	graphDropdown(conn, varName, curVal);
	hPrintf(" in ");
	colorDropdown(i, j);
	if (j != graphCols-1) hPrintf(",");
	hPrintf("</TD>");
	}
    if (i == 0)
	{
	hPrintf("<TD>");
	cgiMakeButton("submit", "Go!");
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


/* Get genome layout. */
gl = ggLayout(conn, graphRows, graphCols);

/* Draw picture. */
genomeGif(conn, gl, graphRows, graphCols, graphHeight()+betweenRowPad);
hPrintf("</FORM>\n");
cartWebEnd();
}

void cartMain(struct cart *theCart)
/* We got the persistent/CGI variable cart.  Now
 * set up the globals and make a web page. */
{
struct sqlConnection *conn = NULL;
cart = theCart;
getDbAndGenome(cart, &database, &genome);
hSetDb(database);
withLabels = cartUsualBoolean(cart, hggLabels, TRUE);
conn = hAllocConn();
getGenoGraphs(conn);

if (cartVarExists(cart, hggConfigure))
    {
    configurePage();
    }
else if (cartVarExists(cart, hggUpload))
    {
    uploadPage();
    }
else if (cartVarExists(cart, hggSubmitUpload))
    {
    submitUpload(conn);
    }
else if (cartVarExists(cart, hggCorrelate))
    {
    correlatePage(conn);
    }
else if (cartVarExists(cart, hggBrowse))
    {
    browseRegions(conn);
    }
else if (cartVarExists(cart, hggSort))
    {
    sortGenes(conn);
    }
else if (cartVarExists(cart, hggClickX))
    {
    clickOnImage(conn);
    }
else
    {
    /* Default case - start fancy web page. */
    mainPage(conn);
    }
cartRemovePrefix(cart, hggDo);
}

char *excludeVars[] = {"Submit", "submit", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
// htmlSetStyle(htmlStyleUndecoratedLink);
if (argc != 1)
    usage();
oldCart = hashNew(12);
cartEmptyShell(cartMain, hUserCookie(), excludeVars, oldCart);
return 0;
}
