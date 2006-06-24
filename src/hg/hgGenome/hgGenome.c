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
#include "hgGenome.h"

static char const rcsid[] = "$Id: hgGenome.c,v 1.10 2006/06/24 00:56:24 kent Exp $";

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

void genomeGif(struct sqlConnection *conn, struct genoLay *gl)
/* Create genome GIF file and HTML that includes it. */
{
struct vGfx *vg;
struct tempName gifTn;
Color shadesOfGray[10];
int maxShade = ArraySize(shadesOfGray)-1;

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
vgClose(&vg);
}

void genoLayDump(struct genoLay *gl)
/* Print out info on genoLay */
{
struct genoLayChrom *chrom;
struct slRef *left, *right, *ref;
int total;
uglyf("gl: lineCount %d, leftLabelWidth %d, rightLabelWidth %d, basesPerPixel %f<BR>\n",
	gl->lineCount, gl->leftLabelWidth, gl->rightLabelWidth, gl->basesPerPixel);
for (left = gl->leftList, right = gl->rightList; left != NULL || right != NULL;)
    {
    total=0;
    if (left != NULL)
	{
	chrom = left->val;
	uglyf("%s@%d,%d[%d] %d ----  ", chrom->fullName, chrom->x, chrom->y, chrom->width, chrom->size);
	total += chrom->size;
        left = left->next;
	}
    if (right != NULL)
	{
	chrom = right->val;
	uglyf("%d  %s@%d,%d[%d]", chrom->size, chrom->fullName, chrom->x, chrom->y, chrom->width);
	total += chrom->size;
        right = right->next;
	}
    uglyf(" : %d<BR>", total);
    }
total=0;
for (ref = gl->bottomList; ref != NULL; ref = ref->next)
    {
    chrom = ref->val;
    total += chrom->size;
    uglyf("%s@%d,%d[%d] %d ...  ", chrom->fullName, chrom->x, chrom->y, chrom->width, chrom->size);
    }
uglyf(" : %d<BR>", total);
}

void webMain(struct sqlConnection *conn)
/* Set up fancy web page with hotlinks bar and
 * sections. */
{
int fontHeight, lineHeight;
struct genoLayChrom *chromList;
struct genoLay *gl;

/* Figure out basic dimensions of image. */
trackLayoutInit(&tl, cart);
fontHeight = mgFontLineHeight(tl.font);
lineHeight = fontHeight*4;

/* Get list of chromosomes and lay them out. */
chromList = genoLayDbChroms(conn, FALSE);
gl = genoLayNew(chromList, tl.font, tl.picWidth, lineHeight, 
	3*tl.nWidth, 4*tl.nWidth);

/* Draw picture. */
genomeGif(conn, gl);
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
return 0;
}
