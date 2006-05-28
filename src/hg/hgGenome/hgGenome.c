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

static char const rcsid[] = "$Id: hgGenome.c,v 1.4 2006/05/28 16:50:04 kent Exp $";

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
  "usage:\n"
  "   hgGenome XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct chromosome
/* Information on a chromosome. */
     {
     struct chromosome *next;
     char *fullName;	/* Name of full deal. */
     char *shortName;	/* Name without chr prefix. */
     int size;		/* Size in bases. */
     struct cytoBand *bands;	/* May be NULL */
     int x,y;		/* Upper left corner pixel coordinates*/
     int width,height; 	/* Pixel width and height */
     };

struct chromLayout
/* This has information on how to lay out chromosomes. */
    {
    MgFont *font;		/* Font used for labels */
    int picWidth;			/* Total picture width */
    int picHeight;			/* Total picture height */
    int margin;				/* BLank area around sides */
    int spaceWidth;			/* Width of a space. */
    struct chromosome *leftList;	/* Left chromosomes. */
    struct chromosome *rightList;	/* Right chromosomes. */
    struct chromosome *bottomList;	/* Sex chromosomes are on bottom. */
    int lineCount;			/* Number of chromosome lines. */
    int leftLabelWidth, rightLabelWidth;/* Pixels for left/right labels */
    int lineHeight;			/* Height for one line */
    int totalHeight;	/* Total width/height in pixels */
    double basesPerPixel;	/* Bases per pixel */
    };


int chromosomeCmpAscii(const void *va, const void *vb)
/* Compare two slNames. */
{
const struct chromosome *a = *((struct chromosome **)va);
const struct chromosome *b = *((struct chromosome **)vb);
return strcmp(a->shortName, b->shortName);
}

int chromosomeCmpNum(const void *va, const void *vb)
/* Compare two slNames. */
{
const struct chromosome *a = *((struct chromosome **)va);
const struct chromosome *b = *((struct chromosome **)vb);
char *aName = a->shortName, *bName = b->shortName;
if (isdigit(aName[0]))
    {
    if (isdigit(bName[0]))
        return atoi(aName) - atoi(bName);
    else
        return -1;
    }
else if (isdigit(bName[0]))
    return 1;
else
    return strcmp(aName, bName);
}

struct chromosome *getChromosomes(struct sqlConnection *conn)
/* Get chrom info list. */
{
struct sqlResult *sr;
char **row;
struct chromosome *chrom, *chromList = NULL;
int count = sqlQuickNum(conn, "select count(*) from chromInfo");
if (count > 500)
    errAbort("Sorry, hgGenome only works on assemblies mapped to chromosomes.");
sr = sqlGetResult(conn, "select chrom,size from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    if (startsWith("chr", name) && 
    	!strchr(name, '_') && !sameString("chrM", name) 
	&& !sameString("chrUn", name))
        {
	AllocVar(chrom);
	chrom->fullName = cloneString(name);
	chrom->shortName = chrom->fullName+3;
	chrom->size = sqlUnsigned(row[1]);
	slAddHead(&chromList, chrom);
	}
    }
if (chromList == NULL)
    errAbort("No chromosomes starting with chr in chromInfo for %s.", database);
slReverse(&chromList);
if (isdigit(chromList->shortName[0]))
    {
    uglyf("Sort by number<BR>\n");
    slSort(&chromList, chromosomeCmpNum);
    }
else
    {
    uglyf("Sort by name<BR>\n");
    slSort(&chromList, chromosomeCmpAscii);
    }
return chromList;
}

void separateSexChroms(struct chromosome *in,
	struct chromosome **retAutoList, struct chromosome **retSexList)
/* Separate input chromosome list into sex and non-sex chromosomes. */
{
struct chromosome *autoList = NULL, *sexList = NULL, *chrom, *next;
for (chrom = in; chrom != NULL; chrom = next)
    {
    char *name = chrom->shortName;
    next = chrom->next;
    if (sameWord(name, "X") || sameWord(name, "Y") || sameWord(name, "Z")
    	|| sameWord(name, "W"))
	{
	slAddHead(&sexList, chrom);
	}
    else
        {
	slAddHead(&autoList, chrom);
	}
    }
slReverse(&sexList);
slReverse(&autoList);
*retAutoList = autoList;
*retSexList = sexList;
}

struct chromLayout *chromLayoutCreate(struct chromosome *chromList,
	MgFont *font, int picWidth, int lineHeight)
/* Figure out layout.  For human and most mammals this will be
 * two columns with sex chromosomes on bottom.  This is complicated
 * by the platypus having a bunch of sex chromosomes. */
{
int margin = 2;
struct chromosome *chrom, *left, *right;
struct chromLayout *cl;
int autoCount, halfCount, bases, chromInLine;
int leftLabelWidth=0, rightLabelWidth=0, labelWidth;
int spaceWidth = mgFontCharWidth(font, ' ');
int autosomeOtherPixels=0, sexOtherPixels=0;
int autosomeBasesInLine=0;	/* Maximum bases in a line for autosome. */
int sexBasesInLine=0;		/* Bases in line for sex chromsome. */
double sexBasesPerPixel, autosomeBasesPerPixel, basesPerPixel;
int pos = margin;
int y = 0;

AllocVar(cl);
cl->font = font;
cl->picWidth = picWidth;
cl->margin = margin;
cl->spaceWidth = spaceWidth;

/* Put sex chromosomes on bottom, and rest on left. */
separateSexChroms(chromList, &chromList, &cl->bottomList);
autoCount = slCount(chromList);
cl->leftList = chromList;

/* If there are a lot of chromosomes, then move later
 * (and smaller) chromosomes to a new right column */
if (autoCount > 12)
    {
    halfCount = (autoCount+1)/2;
    chrom = slElementFromIx(chromList, halfCount-1);
    cl->rightList = chrom->next;
    chrom->next = NULL;
    slReverse(&cl->rightList);
    }

/* Figure out space needed for autosomes. */
left = cl->leftList;
right = cl->rightList;
while (left || right)
    {
    bases = 0;
    chromInLine = 0;
    if (left)
        {
	labelWidth = mgFontStringWidth(font, left->shortName) + spaceWidth;
	if (leftLabelWidth < labelWidth)
	    leftLabelWidth = labelWidth;
	bases = left->size;
	left = left->next;
	}
    if (right)
        {
	labelWidth = mgFontStringWidth(font, right->shortName) + spaceWidth;
	if (rightLabelWidth < labelWidth)
	    rightLabelWidth = labelWidth;
	bases += right->size;
	right = right->next;
	}
    if (autosomeBasesInLine < bases)
        autosomeBasesInLine = bases;
    cl->lineCount += 1;
    }

/* Figure out space needed for sex chromosomes. */
if (cl->bottomList)
    {
    cl->lineCount += 1;
    bases = 0;
    sexOtherPixels = spaceWidth + 2*margin;
    for (chrom = cl->bottomList; chrom != NULL; chrom = chrom->next)
	{
	sexBasesInLine += chrom->size;
	labelWidth = mgFontStringWidth(font, chrom->shortName) + spaceWidth;
	if (chrom == cl->bottomList )
	    {
	    if (leftLabelWidth < labelWidth)
		leftLabelWidth  = labelWidth;
	    sexOtherPixels = leftLabelWidth;
	    }
	else if (chrom->next == NULL)
	    {
	    if (rightLabelWidth < labelWidth)
		rightLabelWidth  = labelWidth;
	    sexOtherPixels += rightLabelWidth + spaceWidth;
	    }
	else
	    {
	    sexOtherPixels += labelWidth + spaceWidth;
	    }
	}
    }

/* Figure out the number of bases needed per pixel. */
autosomeOtherPixels = 2*margin + spaceWidth + leftLabelWidth + rightLabelWidth;
basesPerPixel = autosomeBasesPerPixel 
	= autosomeBasesInLine/(picWidth-autosomeOtherPixels);
if (cl->bottomList)
    {
    sexBasesPerPixel = sexBasesInLine/(picWidth-sexOtherPixels);
    if (sexBasesPerPixel > basesPerPixel)
        basesPerPixel = sexBasesPerPixel;
    }
cl->leftLabelWidth = leftLabelWidth;
cl->rightLabelWidth = rightLabelWidth;
cl->basesPerPixel = basesPerPixel;
uglyf("autosomeOtherPixels %d, sexOtherPixels %d, picWidth %d<BR>\n", autosomeOtherPixels, sexOtherPixels, picWidth);

/* Set pixel positions for left autosomes */
for (chrom = cl->leftList; chrom != NULL; chrom = chrom->next)
    {
    chrom->x = leftLabelWidth + margin;
    chrom->y = y;
    chrom->width = round(chrom->size/basesPerPixel);
    chrom->height = lineHeight;
    y += lineHeight;
    }

/* Set pixel positions for right autosomes */
y = 0;
for (chrom = cl->rightList; chrom != NULL; chrom = chrom->next)
    {
    chrom->width = round(chrom->size/basesPerPixel);
    chrom->height = lineHeight;
    chrom->x = picWidth - margin - rightLabelWidth - chrom->width;
    chrom->y = y;
    y += lineHeight;
    }
cl->picHeight = 2*margin + lineHeight * cl->lineCount;
y = cl->picHeight - margin - lineHeight;

/* Set pixel positions for sex chromosomes */
for (chrom = cl->bottomList; chrom != NULL; chrom = chrom->next)
    {
    chrom->y = y;
    chrom->width = round(chrom->size/basesPerPixel);
    chrom->height = lineHeight;
    if (chrom == cl->bottomList)
	chrom->x = leftLabelWidth + margin;
    else if (chrom->next == NULL)
        chrom->x = picWidth - margin - rightLabelWidth - chrom->width;
    else
	chrom->x = 2*spaceWidth+mgFontStringWidth(font,chrom->shortName) + pos;
    pos = chrom->x + chrom->width;
    }
return cl;
}

void drawChrom(struct vGfx *vg, struct chromosome *chrom, int chromBoxHeight,
	int color)
{
vgBox(vg, chrom->x, chrom->y + chrom->height - chromBoxHeight, 
    chrom->width, chromBoxHeight, color);
}

void leftChromAndLabel(struct vGfx *vg, struct chromLayout *cl,
	struct chromosome *chrom, int fontHeight, int chromBoxHeight,
	int color)
/* Draw a chromosome with label on left. */
{
vgTextRight(vg, cl->margin, chrom->y + chrom->height - fontHeight, 
    chrom->x - cl->margin - cl->spaceWidth, fontHeight, color,
    cl->font, chrom->shortName);
drawChrom(vg, chrom, chromBoxHeight, color);
}

void rightChromAndLabel(struct vGfx *vg, struct chromLayout *cl,
	struct chromosome *chrom, int fontHeight, int chromBoxHeight,
	int color)
/* Draw a chromosome with label on left. */
{
vgText(vg, chrom->x + chrom->width + cl->spaceWidth,
	chrom->y + chrom->height - fontHeight, 
	color, cl->font, chrom->shortName);
drawChrom(vg, chrom, chromBoxHeight, color);
}

void midChromAndLabel(struct vGfx *vg, struct chromLayout *cl,
	struct chromosome *chrom, int fontHeight, int chromBoxHeight,
	int color)
/* Draw a chromosome with label on left. */
{
MgFont *font = cl->font;
int textWidth = mgFontStringWidth(font, chrom->shortName);
vgTextRight(vg, chrom->x - textWidth - cl->spaceWidth, 
    chrom->y + chrom->height - fontHeight, 
    textWidth, fontHeight, color,
    font, chrom->shortName);
drawChrom(vg, chrom, chromBoxHeight, color);
}

void genomeGif(struct sqlConnection *conn, struct chromLayout *cl)
/* Create genome GIF file and HTML that includes it. */
{
struct vGfx *vg;
struct tempName gifTn;
struct chromosome *chrom;
int chromBoxHeight = 8;
MgFont *font = cl->font;
int fontHeight = mgFontPixelHeight(font);
int color = MG_BLACK;

makeTempName(&gifTn, "hgtIdeo", ".gif");
vg = vgOpenGif(cl->picWidth, cl->picHeight, gifTn.forCgi);
printf("<IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d>",
	    gifTn.forHtml, cl->picWidth, cl->picHeight);

for (chrom = cl->leftList; chrom != NULL; chrom = chrom->next)
    leftChromAndLabel(vg, cl, chrom, fontHeight, chromBoxHeight, color);
for (chrom = cl->rightList; chrom != NULL; chrom = chrom->next)
    rightChromAndLabel(vg, cl, chrom, fontHeight, chromBoxHeight, color);
for (chrom = cl->bottomList; chrom != NULL; chrom = chrom->next)
    {
    if (chrom == cl->bottomList)
	leftChromAndLabel(vg, cl, chrom, fontHeight, chromBoxHeight, color);
    else if (chrom->next == NULL)
	rightChromAndLabel(vg, cl, chrom, fontHeight, chromBoxHeight, color);
    else
        midChromAndLabel(vg, cl, chrom, fontHeight, chromBoxHeight, color);
    }

#ifdef SOMEDAY
if(doIdeo)
    {
    char startBand[16];
    char endBand[16];
    char title[32];
    startBand[0] = endBand[0] = '\0';
    fillInStartEndBands(ideoTrack, startBand, endBand, sizeof(startBand)); 
    /* Draw the ideogram. */
    makeTempName(&gifTn, "hgtIdeo", ".gif");
    /* Start up client side map. */
    hPrintf("<MAP Name=%s>\n", mapName);
    ideoHeight = gfxBorder + ideoTrack->height;
    vg = vgOpenGif(ideoWidth, ideoHeight, gifTn.forCgi);
    makeGrayShades(vg);
    makeBrownShades(vg);
    makeSeaShades(vg);
    ideoTrack->ixColor = vgFindRgb(vg, &ideoTrack->color);
    ideoTrack->ixAltColor = vgFindRgb(vg, &ideoTrack->altColor);
    vgSetClip(vg, 0, gfxBorder, ideoWidth, ideoTrack->height);
    if(sameString(startBand, endBand)) 
	safef(title, sizeof(title), "%s (%s)", chromName, startBand);
    else
	safef(title, sizeof(title), "%s (%s-%s)", chromName, startBand, endBand);
    textWidth = mgFontStringWidth(font, title);
    vgTextCentered(vg, 2, gfxBorder, textWidth, ideoTrack->height, MG_BLACK, font, title);
    ideoTrack->drawItems(ideoTrack, winStart, winEnd, vg, textWidth+4, gfxBorder, ideoWidth-textWidth-4,
			 font, ideoTrack->ixColor, ideoTrack->limitedVis);
    vgUnclip(vg);
    /* Save out picture and tell html file about it. */
    vgClose(&vg);
    /* Finish map. */
    hPrintf("</MAP>\n");
    }
hPrintf("<TABLE BORDER=0 CELLPADDING=0>");
if(doIdeo)
    {
    hPrintf("<TR><TD HEIGHT=5></TD></TR>");
    hPrintf("<TR><TD><IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s >",
	    gifTn.forHtml, ideoWidth, ideoHeight, mapName);
    hPrintf("</TD></TR>");
    hPrintf("<TR><TD HEIGHT=5></TD></TR></TABLE>");
    }
else
    hPrintf("<TR><TD HEIGHT=10></TD></TR></TABLE>");
if(ideoTrack != NULL)
    {
    ideoTrack->limitedVisSet = TRUE;
    ideoTrack->limitedVis = tvHide; /* Don't draw in main gif. */
    }
#endif /* SOMEDAY */
vgClose(&vg);
}

void webMain(struct sqlConnection *conn)
/* Set up fancy web page with hotlinks bar and
 * sections. */
{
struct chromosome *chromList, *chrom, *left, *right;
struct chromLayout *cl;
int total;
int fontHeight, lineHeight;
trackLayoutInit(&tl, cart);
fontHeight = mgFontLineHeight(tl.font);
lineHeight = fontHeight*3;
chromList = getChromosomes(conn);
cl = chromLayoutCreate(chromList, tl.font, tl.picWidth, lineHeight);
uglyf("cl: lineCount %d, leftLabelWidth %d, rightLabelWidth %d, basesPerPixel %f<BR>\n",
	cl->lineCount, cl->leftLabelWidth, cl->rightLabelWidth, cl->basesPerPixel);
for (left = cl->leftList, right = cl->rightList; left != NULL || right != NULL;)
    {
    total=0;
    if (left != NULL)
	{
	uglyf("%s@%d[%d] %d ----  ", left->fullName, left->x, left->width, left->size);
	total += left->size;
        left = left->next;
	}
    if (right != NULL)
	{
	uglyf("%d  %s@%d[%d]", right->size, right->fullName, right->x, right->width);
	total += right->size;
        right = right->next;
	}
    uglyf(" : %d<BR>", total);
    }
total=0;
for (chrom = cl->bottomList; chrom != NULL; chrom = chrom->next)
    {
    total += chrom->size;
    uglyf("%s@%d[%d] %d ...  ", chrom->fullName, chrom->x, chrom->width, chrom->size);
    }
uglyf(" : %d<BR>", total);
genomeGif(conn, cl);
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
