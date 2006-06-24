/* hgGenome - Full genome (as opposed to chromosome) view of data. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"
#include "hui.h"
#include "dbDb.h"
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

static char const rcsid[] = "$Id: hgGenome.c,v 1.13 2006/06/24 16:34:24 kent Exp $";

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
struct hash *oldCart;	/* Old cart hash. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
char *genome;		/* Name of genome - mouse, human, etc. */
struct trackLayout tl;  /* Dimensions of things, fonts, etc. */


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

void drawChromGraph(struct vGfx *vg, struct sqlConnection *conn, 
	struct genoLay *gl, char *chromGraph, int yOff, int height, Color color)
/* Draw chromosome graph on all chromosomes in layout at given
 * y offset and height. */
{
char *fileName = chromGraphBinaryFileName(chromGraph, conn);
struct chromGraphBin *cgb = chromGraphBinOpen(fileName);
double gMin, gMax, gScale;
double pixelsPerBase = 1.0/gl->basesPerPixel;

chromGraphDataRange(chromGraph, conn, &gMin, &gMax);
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

void genomeGif(struct sqlConnection *conn, struct genoLay *gl,
	char *chromGraph)
/* Create genome GIF file and HTML that includes it. */
{
struct vGfx *vg;
struct tempName gifTn;
Color shadesOfGray[10];
int maxShade = ArraySize(shadesOfGray)-1;
int spacing = 1;

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
if (sqlTableExists(conn, chromGraph))
    drawChromGraph(vg, conn, gl, chromGraph, 
	    gl->betweenChromOffsetY + 2*spacing, 
	    gl->betweenChromHeight - 3*spacing, MG_BLUE);
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
struct slName *el;
struct slName *userList = userListAll();
struct slName *dbList=chromGraphListAll(conn);
int totalCount = 1 + slCount(userList) + slCount(dbList);
char **menu, **values;
int i = 0;

AllocArray(menu, totalCount);
AllocArray(values, totalCount);
menu[0] = "";
values[0] = "none";

for (el = userList; el != NULL; el = el->next)
    {
    ++i;
    menu[i] = el->name;
    values[i] = el->name;
    }
for (el = dbList; el != NULL; el = el->next)
    {
    ++i;
    menu[i] = el->name;
    values[i] = el->name;
    }
cgiMakeDropListFull(varName, menu, values, totalCount, values[0], NULL);
}

void webMain(struct sqlConnection *conn)
/* Set up fancy web page with hotlinks bar and
 * sections. */
{
int fontHeight, lineHeight;
struct genoLayChrom *chromList;
struct genoLay *gl;

/* Start form and save session var. */
hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\" METHOD=GET>\n");
cartSaveSession(cart);

hPrintf("graph ");
graphDropdown(conn, hggGraph1);
hPrintf(" vs ");
graphDropdown(conn, hggGraph2);
hPrintf(" ");
cgiMakeButton("submit", "Go!");
hPrintf(" ");
cgiMakeButton(hggConfigure, "Configure");
hPrintf("<BR>");
hPrintf(" significance threshold:");
cgiMakeDoubleVar(hggThreshold, 3.5, 3);
hPrintf(" ");
cgiMakeButton(hggBrowse, "Browse Regions");
hPrintf(" ");
cgiMakeButton(hggSort, "Sort Genes");
hPrintf("<BR>");

/* Figure out basic dimensions of image. */
trackLayoutInit(&tl, cart);
fontHeight = mgFontLineHeight(tl.font);
lineHeight = fontHeight*3;

/* Get list of chromosomes and lay them out. */
chromList = genoLayDbChroms(conn, FALSE);
gl = genoLayNew(chromList, tl.font, tl.picWidth, lineHeight, 
	3*tl.nWidth, 4*tl.nWidth);
/* Draw picture. */
genomeGif(conn, gl, "fakeChromGraph");
hPrintf("</FORM>\n");
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

    {
    /* Default case - start fancy web page. */
    cartWebStart(cart, "%s Genome View", genome);
    webMain(conn);
    cartWebEnd();
    }
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
