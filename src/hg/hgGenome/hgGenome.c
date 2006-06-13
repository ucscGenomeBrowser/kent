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

static char const rcsid[] = "$Id: hgGenome.c,v 1.7 2006/06/13 18:09:17 kent Exp $";

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

struct genoLayChrom
/* Information on a chromosome. */
     {
     struct genoLayChrom *next;
     char *fullName;	/* Name of full deal. Set before genoLayNew. */
     char *shortName;	/* Name w/out chr prefix. Set before genoLayNew. */
     int size;		/* Size in bases. Set before genoLayNew. */
     int x,y;		/* Upper left corner pixel coordinates*/
     int width,height; 	/* Pixel width and height */
     };

struct genoLay
/* This has information on how to lay out chromosomes. */
    {
    MgFont *font;		/* Font used for labels */
    struct genoLayChrom *chromList;	/* List of all chromosomes */
    int picWidth;			/* Total picture width */
    int picHeight;			/* Total picture height */
    int margin;				/* Blank area around sides */
    int spaceWidth;			/* Width of a space. */
    struct slRef *leftList;	/* Left chromosomes. */
    struct slRef *rightList;	/* Right chromosomes. */
    struct slRef *bottomList;	/* Sex chromosomes are on bottom. */
    int lineCount;			/* Number of chromosome lines. */
    int leftLabelWidth, rightLabelWidth;/* Pixels for left/right labels */
    int lineHeight;			/* Height for one line */
    int totalHeight;	/* Total width/height in pixels */
    double basesPerPixel;	/* Bases per pixel */
    };


char *skipDigits(char *s)
/* Return first char of s that's not a digit */
{
while (isdigit(*s))
   ++s;
return s;
}

int genoLayChromCmpNum(const void *va, const void *vb)
/* Compare two slNames. */
{
const struct genoLayChrom *a = *((struct genoLayChrom **)va);
const struct genoLayChrom *b = *((struct genoLayChrom **)vb);
char *aName = a->shortName, *bName = b->shortName;
if (isdigit(aName[0]))
    {
    if (isdigit(bName[0]))
	{
	int diff = atoi(aName) - atoi(bName);
	if (diff == 0)
	    diff = strcmp(skipDigits(aName), skipDigits(bName));
	return diff;
	}
    else
        return -1;
    }
else if (isdigit(bName[0]))
    return 1;
else
    return strcmp(aName, bName);
}

struct genoLayChrom *genoLayDbChroms(struct sqlConnection *conn, 
	boolean withRandom)
/* Get chrom info list. */
{
struct sqlResult *sr;
char **row;
struct genoLayChrom *chrom, *chromList = NULL;
int count = sqlQuickNum(conn, "select count(*) from chromInfo");
if (count > 500)
    errAbort("Sorry, hgGenome only works on assemblies mapped to chromosomes.");
sr = sqlGetResult(conn, "select chrom,size from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    if (withRandom || (startsWith("chr", name) && 
    	!strchr(name, '_') && !sameString("chrM", name) 
	&& !sameString("chrUn", name)))
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
slSort(&chromList, genoLayChromCmpNum);
return chromList;
}

void separateSexChroms(struct slRef *in,
	struct slRef **retAutoList, struct slRef **retSexList)
/* Separate input chromosome list into sex and non-sex chromosomes. */
{
struct slRef *autoList = NULL, *sexList = NULL, *ref, *next;

for (ref = in; ref != NULL; ref = next)
    {
    struct genoLayChrom *chrom = ref->val;
    char *name = chrom->shortName;
    next = ref->next;
    if (sameWord(name, "X") || sameWord(name, "Y") || sameWord(name, "Z")
    	|| sameWord(name, "W"))
	{
	slAddHead(&sexList, ref);
	}
    else
        {
	slAddHead(&autoList, ref);
	}
    }
slReverse(&sexList);
slReverse(&autoList);
*retAutoList = autoList;
*retSexList = sexList;
}

struct genoLay *genoLayNew(struct genoLayChrom *chromList,
	MgFont *font, int picWidth, int lineHeight,
	int minLeftLabelWidth, int minRightLabelWidth)
/* Figure out layout.  For human and most mammals this will be
 * two columns with sex chromosomes on bottom.  This is complicated
 * by the platypus having a bunch of sex chromosomes. */
{
int margin = 2;
struct slRef *refList = NULL, *ref, *left, *right;
struct genoLayChrom *xyz;
struct genoLay *cl;
int autoCount, halfCount, bases, chromInLine;
int leftLabelWidth=0, rightLabelWidth=0, labelWidth;
int spaceWidth = mgFontCharWidth(font, ' ');
int extraLabelPadding = 0;
int autosomeOtherPixels=0, sexOtherPixels=0;
int autosomeBasesInLine=0;	/* Maximum bases in a line for autosome. */
int sexBasesInLine=0;		/* Bases in line for sex chromsome. */
double sexBasesPerPixel, autosomeBasesPerPixel, basesPerPixel;
int pos = margin;
int y = 0;

refList = refListFromSlList(chromList);
AllocVar(cl);
cl->chromList = chromList;
cl->font = font;
cl->picWidth = picWidth;
cl->margin = margin;
cl->spaceWidth = spaceWidth;

/* Put sex chromosomes on bottom, and rest on left. */
separateSexChroms(refList, &refList, &cl->bottomList);
autoCount = slCount(refList);
cl->leftList = refList;

/* If there are a lot of chromosomes, then move later
 * (and smaller) chromosomes to a new right column */
if (autoCount > 12)
    {
    halfCount = (autoCount+1)/2;
    ref = slElementFromIx(refList, halfCount-1);
    cl->rightList = ref->next;
    ref->next = NULL;
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
	xyz = left->val;
	labelWidth = mgFontStringWidth(font, xyz->shortName) + spaceWidth;
	if (leftLabelWidth < labelWidth)
	    leftLabelWidth = labelWidth;
	bases = xyz->size;
	left = left->next;
	}
    if (right)
        {
	xyz = right->val;
	labelWidth = mgFontStringWidth(font, xyz->shortName) + spaceWidth;
	if (rightLabelWidth < labelWidth)
	    rightLabelWidth = labelWidth;
	bases += xyz->size;
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
    for (ref = cl->bottomList; ref != NULL; ref = ref->next)
	{
	xyz = ref->val;
	sexBasesInLine += xyz->size;
	labelWidth = mgFontStringWidth(font, xyz->shortName) + spaceWidth;
	if (ref == cl->bottomList )
	    {
	    if (leftLabelWidth < labelWidth)
		leftLabelWidth  = labelWidth;
	    sexOtherPixels = leftLabelWidth;
	    }
	else if (ref->next == NULL)
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

/* Do some adjustments if side labels are bigger than needed for
 * chromosome names. */
if (leftLabelWidth < minLeftLabelWidth)
    {
    extraLabelPadding += (minLeftLabelWidth - leftLabelWidth);
    leftLabelWidth = minLeftLabelWidth;
    }
if (rightLabelWidth < minRightLabelWidth)
    {
    extraLabelPadding += (minRightLabelWidth - rightLabelWidth);
    rightLabelWidth = minRightLabelWidth;
    }
sexOtherPixels += extraLabelPadding;

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

/* Save positions and sizes of some things in layout structure. */
cl->leftLabelWidth = leftLabelWidth;
cl->rightLabelWidth = rightLabelWidth;
cl->basesPerPixel = basesPerPixel;
uglyf("autosomeOtherPixels %d, sexOtherPixels %d, picWidth %d<BR>\n", autosomeOtherPixels, sexOtherPixels, picWidth);

/* Set pixel positions for left autosomes */
for (ref = cl->leftList; ref != NULL; ref = ref->next)
    {
    xyz = ref->val;
    xyz->x = leftLabelWidth + margin;
    xyz->y = y;
    xyz->width = round(xyz->size/basesPerPixel);
    xyz->height = lineHeight;
    y += lineHeight;
    }

/* Set pixel positions for right autosomes */
y = 0;
for (ref = cl->rightList; ref != NULL; ref = ref->next)
    {
    xyz = ref->val;
    xyz->width = round(xyz->size/basesPerPixel);
    xyz->height = lineHeight;
    xyz->x = picWidth - margin - rightLabelWidth - xyz->width;
    xyz->y = y;
    y += lineHeight;
    }
cl->picHeight = 2*margin + lineHeight * cl->lineCount;
y = cl->picHeight - margin - lineHeight;

/* Set pixel positions for sex chromosomes */
for (ref = cl->bottomList; ref != NULL; ref = ref->next)
    {
    xyz = ref->val;
    xyz->y = y;
    xyz->width = round(xyz->size/basesPerPixel);
    xyz->height = lineHeight;
    if (ref == cl->bottomList)
	xyz->x = leftLabelWidth + margin;
    else if (ref->next == NULL)
        xyz->x = picWidth - margin - rightLabelWidth - xyz->width;
    else
	xyz->x = 2*spaceWidth+mgFontStringWidth(font,xyz->shortName) + pos;
    pos = xyz->x + xyz->width;
    }
return cl;
}

void leftLabel(struct vGfx *vg, struct genoLay *cl,
	struct genoLayChrom *chrom, int fontHeight, int chromBoxHeight,
	int color)
/* Draw a chromosome with label on left. */
{
vgTextRight(vg, cl->margin, chrom->y + chrom->height - fontHeight, 
    chrom->x - cl->margin - cl->spaceWidth, fontHeight, color,
    cl->font, chrom->shortName);
}

void rightLabel(struct vGfx *vg, struct genoLay *cl,
	struct genoLayChrom *chrom, int fontHeight, int chromBoxHeight,
	int color)
/* Draw a chromosome with label on left. */
{
vgText(vg, chrom->x + chrom->width + cl->spaceWidth,
	chrom->y + chrom->height - fontHeight, 
	color, cl->font, chrom->shortName);
}

void midLabel(struct vGfx *vg, struct genoLay *cl,
	struct genoLayChrom *chrom, int fontHeight, int chromBoxHeight,
	int color)
/* Draw a chromosome with label on left. */
{
MgFont *font = cl->font;
int textWidth = mgFontStringWidth(font, chrom->shortName);
vgTextRight(vg, chrom->x - textWidth - cl->spaceWidth, 
    chrom->y + chrom->height - fontHeight, 
    textWidth, fontHeight, color,
    font, chrom->shortName);
}

void drawChrom(struct vGfx *vg, struct genoLayChrom *chrom, int chromBoxHeight,
	int color)
/* Draw chromosomes. */
{
vgBox(vg, chrom->x, chrom->y + chrom->height - chromBoxHeight, 
    chrom->width, chromBoxHeight, color);
}


void genomeGif(struct sqlConnection *conn, struct genoLay *cl)
/* Create genome GIF file and HTML that includes it. */
{
struct vGfx *vg;
struct tempName gifTn;
struct slRef *ref;
struct genoLayChrom *chrom;
int chromBoxHeight = 8;
MgFont *font = cl->font;
int fontHeight = mgFontPixelHeight(font);
int color = MG_BLACK;

makeTempName(&gifTn, "hgtIdeo", ".gif");
vg = vgOpenGif(cl->picWidth, cl->picHeight, gifTn.forCgi);
printf("<IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d>",
	    gifTn.forHtml, cl->picWidth, cl->picHeight);

/* Draw chromosome labels. */
for (ref = cl->leftList; ref != NULL; ref = ref->next)
    leftLabel(vg, cl, ref->val, fontHeight, chromBoxHeight, color);
for (ref = cl->rightList; ref != NULL; ref = ref->next)
    rightLabel(vg, cl, ref->val, fontHeight, chromBoxHeight, color);
for (ref = cl->bottomList; ref != NULL; ref = ref->next)
    {
    chrom = ref->val;
    if (ref == cl->bottomList)
	leftLabel(vg, cl, chrom, fontHeight, chromBoxHeight, color);
    else if (ref->next == NULL)
	rightLabel(vg, cl, chrom, fontHeight, chromBoxHeight, color);
    else
        midLabel(vg, cl, chrom, fontHeight, chromBoxHeight, color);
    }

/* Draw chromosomes proper. */
for (chrom = cl->chromList; chrom != NULL; chrom = chrom->next)
    drawChrom(vg, chrom, chromBoxHeight, color);

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
struct genoLayChrom *chromList, *chrom;
struct slRef *left, *right, *ref;
struct genoLay *cl;
int total;
int fontHeight, lineHeight;
trackLayoutInit(&tl, cart);
fontHeight = mgFontLineHeight(tl.font);
lineHeight = fontHeight*3;
chromList = genoLayDbChroms(conn, FALSE);
cl = genoLayNew(chromList, tl.font, tl.picWidth, lineHeight, 
	3*tl.nWidth, 4*tl.nWidth);
uglyf("cl: lineCount %d, leftLabelWidth %d, rightLabelWidth %d, basesPerPixel %f<BR>\n",
	cl->lineCount, cl->leftLabelWidth, cl->rightLabelWidth, cl->basesPerPixel);
for (left = cl->leftList, right = cl->rightList; left != NULL || right != NULL;)
    {
    total=0;
    if (left != NULL)
	{
	chrom = left->val;
	uglyf("%s@%d[%d] %d ----  ", chrom->fullName, chrom->x, chrom->width, chrom->size);
	total += chrom->size;
        left = left->next;
	}
    if (right != NULL)
	{
	chrom = right->val;
	uglyf("%d  %s@%d[%d]", chrom->size, chrom->fullName, chrom->x, chrom->width);
	total += chrom->size;
        right = right->next;
	}
    uglyf(" : %d<BR>", total);
    }
total=0;
for (ref = cl->bottomList; ref != NULL; ref = ref->next)
    {
    chrom = ref->val;
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
