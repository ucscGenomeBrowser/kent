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

static char const rcsid[] = "$Id: hgGenome.c,v 1.19 2006/06/28 22:29:28 kent Exp $";

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
struct hash *oldCart;	/* Old cart hash. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
char *genome;		/* Name of genome - mouse, human, etc. */
struct trackLayout tl;  /* Dimensions of things, fonts, etc. */
struct genoGraph *ggList; /* List of active genome graphs */
struct hash *ggHash;	  /* Hash of active genome graphs */


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
	    safef(nameBuf, sizeof(nameBuf), "user: %s", name);
	    AllocVar(gg);
	    gg->name = cloneString(nameBuf);
	    gg->shortLabel = name;
	    gg->longLabel = hashFindVal(ra, "description");
	    if (gg->longLabel == NULL) gg->longLabel = name;
	    gg->binFileName = binaryFile;
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
    	"select shortLabel,longLabel,settings from %s where tableName='%s'",
	trackDbTable, gg->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
	gg->shortLabel = cloneString(row[0]);
	gg->longLabel = cloneString(row[1]);
	gg->settings = raFromString(row[2]);
	}
    sqlFreeResult(&sr);

    /* Add db: prefix to separate user and db name space. */
    safef(nameBuf, sizeof(nameBuf), "db: %s", gg->name);
    gg->name = cloneString(nameBuf);
    }

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
    hashAdd(ggHash, gg->name, gg);
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
return cartUsualIntClipped(cart, hggGraphHeight, 25, 5, 200);
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
    return vgFindColorIx(vg, 200, 100, 0);
else if (sameWord("green", asciiColor))
    return vgFindColorIx(vg, 0, 180, 0);
else if (sameWord("gray", asciiColor))
    return MG_GRAY;
else
    return MG_BLACK;
}

/* Page drawing stuff. */

void drawChromGraph(struct vGfx *vg, struct sqlConnection *conn, 
	struct genoLay *gl, char *chromGraph, int yOff, int height, Color color)
/* Draw chromosome graph on all chromosomes in layout at given
 * y offset and height. */
{
struct genoGraph *gg = hashFindVal(ggHash, chromGraph);
if (gg != NULL)
    {
    char *fileName = gg->binFileName;
    struct chromGraphBin *cgb = chromGraphBinOpen(fileName);
    double pixelsPerBase = 1.0/gl->basesPerPixel;
    double gMin = cgb->minVal, gMax = cgb->maxVal, gScale;

    gScale = height/(gMax-gMin+1);
    while (chromGraphBinNextChrom(cgb))
	{
	struct genoLayChrom *chrom = hashFindVal(gl->chromHash, cgb->chrom);
	while (chromGraphBinNextVal(cgb))
	    {
	    if (chrom)
		{
		int y = (height - ((cgb->val - gMin)*gScale));
		int x = (pixelsPerBase*cgb->chromStart);
		vgDot(vg, x+chrom->x, y+chrom->y+yOff, color);
		}
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
printf("<IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d>",
	    gifTn.forHtml, gl->picWidth, gl->picHeight);

/* Get our grayscale. */
hMakeGrayShades(vg, shadesOfGray, maxShade);

/* Draw the labels and then the chromosomes. */
genoLayDrawChromLabels(gl, vg, MG_BLACK);
genoLayDrawBandedChroms(gl, vg, database, conn, 
	shadesOfGray, maxShade, MG_BLACK);

/* Draw chromosome graphs. */
for (i=0; i<graphRows; ++i)
    {
    for (j=0; j<graphCols; ++j)
	{
	char *graph = graphSourceAt(i,j);
	if (graph[0] != 0)
	    {
	    Color color = colorFromAscii(vg, graphColorAt(i,j));
	    drawChromGraph(vg, conn, gl, graph, 
		    gl->betweenChromOffsetY + yOffset, 
		    innerHeight,  color);
	    }
	}
    yOffset += oneRowHeight;
    }
vgClose(&vg);
}

struct slName *userListAll()
/* List all graphs that user has uploaded */
{
return NULL;
}

void graphDropdown(struct sqlConnection *conn, char *varName)
/* Make a drop-down with available chrom graphs */
{
struct genoGraph *gg;
int totalCount = 1 + slCount(ggList);
char *curVal = cartUsualString(cart, varName, "");
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

void mainPage(struct sqlConnection *conn)
/* Do main page of application:  hotlinks bar, controls, graphic. */
{
struct genoLayChrom *chromList;
struct genoLay *gl;
int graphRows = linesOfGraphs();
int graphCols = graphsPerLine();
int i, j;
int oneRowHeight;

cartWebStart(cart, "%s Genome Graphs", genome);
getGenoGraphs(conn);

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
    for (j=graphCols-1; j>=0; --j)
	{
	hPrintf("<TD>");
	graphDropdown(conn, graphVarName(i,j));
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
cgiMakeButton(hggUpload, "(Upload)");
hPrintf(" ");
cgiMakeButton(hggConfigure, "Configure");
hPrintf(" ");
cgiMakeButton(hggCorrelate, "(Correlate)");
hPrintf(" significance threshold:");
cgiMakeDoubleVar(hggThreshold, 3.5, 3);
hPrintf(" ");
cgiMakeButton(hggBrowse, "(Browse Regions)");
hPrintf(" ");
cgiMakeButton(hggSort, "(Sort Genes)");
hPrintf("<BR>");

/* Figure out basic dimensions of image. */
trackLayoutInit(&tl, cart);

/* Get list of chromosomes and lay them out. */
chromList = genoLayDbChroms(conn, FALSE);
oneRowHeight = graphHeight()+3;
gl = genoLayNew(chromList, tl.font, tl.picWidth, graphRows*oneRowHeight,
	3*tl.nWidth, 4*tl.nWidth);

/* Draw picture. */
genomeGif(conn, gl, graphRows, graphCols, oneRowHeight);
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
conn = hAllocConn();

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
    cartWebStart(cart, "Theoretically correlating, please go back");
    cartWebEnd();
    }
else if (cartVarExists(cart, hggBrowse))
    {
    cartWebStart(cart, "Theoretically browsing, please go back");
    cartWebEnd();
    }
else if (cartVarExists(cart, hggSort))
    {
    cartWebStart(cart, "Theoretically sorting, please go back");
    cartWebEnd();
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
htmlSetStyle(htmlStyleUndecoratedLink);
if (argc != 1)
    usage();
oldCart = hashNew(12);
cartEmptyShell(cartMain, hUserCookie(), excludeVars, oldCart);
return 0;
}
