/* hgTracks - Human Genome browser main cgi script. */
#include "common.h"
#include "hCommon.h"
#include "linefile.h"
#include "portable.h"
#include "memalloc.h"
#include "obscure.h"
#include "dystring.h"
#include "hash.h"
#include "jksql.h"
#include "memgfx.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hgFind.h"
#include "spaceSaver.h"
#include "wormdna.h"
#include "aliType.h"
#include "psl.h"
#include "agpFrag.h"
#include "agpGap.h"
#include "ctgPos.h"
#include "clonePos.h"
#include "genePred.h"
#include "glDbRep.h"
#include "rmskOut.h"
#include "bed.h"
#include "isochores.h"
#include "simpleRepeat.h"
#include "cpgIsland.h"
#include "cytoBand.h"
#include "gcPercent.h"
#include "genomicDups.h"
#include "mapSts.h"
#include "est3.h"
#include "exoFish.h"
#include "roughAli.h"
#include "snp.h"
#include "rnaGene.h"
#include "stsMarker.h"
#include "mouseSyn.h"
#include "knownMore.h"
#include "exprBed.h"

#define CHUCK_CODE 1
#define ROGIC_CODE 1
#define FUREY_CODE 1
#define MAX_CONTROL_COLUMNS 5
#define CONTROL_TABLE_WIDTH 600
#ifdef CHUCK_CODE
/* begin Chuck code */
#define EXPR_DATA_SHADES 16

/* Declare our color gradients and the the number of colors in them */
Color shadesOfGreen[EXPR_DATA_SHADES];
Color shadesOfRed[EXPR_DATA_SHADES];
Color shadesOfBlue[EXPR_DATA_SHADES];
boolean exprBedColorsMade = FALSE; /* Have the shades of Green, Red, and Blue been allocated? */
int maxRGBShade = EXPR_DATA_SHADES - 1;
char *otherFrame = NULL;
char *thisFrame = NULL;
/* end Chuck code */
#endif /*CHUCK_CODE*/

int maxItemsInFullTrack = 300;

/* These variables persist from one incarnation of this program to the
 * next - living mostly in hidden variables. */
char *chromName;		/* Name of chromosome sequence . */
char *database;			/* Name of database we're using. */
char *position; 		/* Name of position. */
int winStart;			/* Start of window in sequence. */
int winEnd;			/* End of window in sequence. */
boolean seqReverse;		/* Look at reverse strand. */
char *userSeqString = NULL;	/* User sequence .fa/.psl file. */
char *eUserSeqString = NULL;    /* CGI encoded user seq. string. */

boolean withLeftLabels = TRUE;		/* Display left labels? */
boolean withCenterLabels = TRUE;	/* Display center labels? */
boolean withGuidelines = TRUE;		/* Display guidelines? */
boolean withRuler = TRUE;		/* Display ruler? */
boolean hideControls = FALSE;		/* Hide all controls? */

boolean calledSelf;			/* True if program called by page it generated. */

struct trackLayout
/* This structure controls the basic dimensions of display. */
    {
    MgFont *font;		/* What font to use. */
    int leftLabelWidth;		/* Width of left labels. */
    int trackWidth;		/* Width of tracks. */
    int picWidth;		/* Width of entire picture. */
    } tl;

void initTl()
/* Initialize layout around small font and a picture about 600 pixels
 * wide. */
{
MgFont *font;
char *s;

font = tl.font = mgSmallFont();
tl.leftLabelWidth = 100;
tl.picWidth = 610;
s = cgiOptionalString("pix");
if (s != NULL && isdigit(s[0]))
    {
    tl.picWidth = atoi(s);
    if (tl.picWidth > 5000)
        tl.picWidth = 5000;
    if (tl.picWidth < 320)
        tl.picWidth = 320;
    }
tl.trackWidth = tl.picWidth - tl.leftLabelWidth;
}

enum trackVisibility 
/* How to look at a track. */
    {
    tvHide=0, 		/* Hide it. */
    tvDense=1,        /* Squish it together. */
    tvFull=2        /* Expand it out. */
    };  

char *tvStrings[] = 
/* User interface strings for above. */
    {
    "hide",
    "dense",
    "full",
    };


char *offOn[] =
/* Off/on control. */
    {
    "off",
    "on",
    };

/* Other global variables. */
int seqBaseCount;	/* Number of bases in sequence. */
int winBaseCount;	/* Number of bases in window. */

int maxShade = 9;	/* Highest shade in a color gradient. */
Color shadesOfGray[10+1];	/* 10 shades of gray from white to black
                                 * Red is put at end to alert overflow. */
Color shadesOfBrown[10+1];	/* 10 shades of brown from tan to tar. */
static struct rgbColor brownColor = {100, 50, 0};
static struct rgbColor tanColor = {255, 240, 200};


Color shadesOfSea[10+1];       /* Ten sea shades. */
static struct rgbColor darkSeaColor = {0, 60, 120};
static struct rgbColor lightSeaColor = {200, 220, 255};

struct trackGroup
/* Structure that displays a group of tracks. */
    {
    struct trackGroup *next;   /* Next on list. */
    char *mapName;             /* Name on image map and for ui buttons. */
    enum trackVisibility visibility; /* How much of this to see if possible. */
    enum trackVisibility limitedVis; /* How much of this actually see. */

    char *longLabel;           /* Long label to put in center. */
    char *shortLabel;          /* Short label to put on side. */

    bool mapsSelf;          /* True if system doesn't need to do map box. */
    bool drawName;          /* True if BED wants name drawn in box. */

    Color *colorShades;	       /* Color scale (if any) to use. */
    struct rgbColor color;     /* Main color. */
    Color ixColor;             /* Index of main color. */
    struct rgbColor altColor;  /* Secondary color. */
    Color ixAltColor;

    void (*loadItems)(struct trackGroup *tg);
    /* loadItems loads up items for the chromosome range indicated.  It also usually sets the
     * following variables.  */ 
    void *items;               /* Some type of slList of items. */

    char *(*itemName)(struct trackGroup *tg, void *item);
    /* Return name of one of a member of items above to display on left side. */

    char *(*mapItemName)(struct trackGroup *tg, void *item);
    /* Return name to associate on map. */

    int (*totalHeight)(struct trackGroup *tg, enum trackVisibility vis);
        /* Return total height. Called before and after drawItems. 
         * Must set the following variables. */
    int height;                /* Total height - must be set by above call. */
    int lineHeight;            /* Height per track including border. */
    int heightPer;             /* Height per track minus border. */

    int (*itemHeight)(struct trackGroup *tg, void *item);
    /* Return height of one item. */

    void (*drawItems)(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis);
    /* Draw item list, one per track. */

    int (*itemStart)(struct trackGroup *tg, void *item);
    /* Return start of item in base pairs. */

    int (*itemEnd)(struct trackGroup *tg, void *item);
    /* Return start of item in base pairs. */

    void (*freeItems)(struct trackGroup *tg);
    /* Free item list. */

    Color (*itemColor)(struct trackGroup *tg, void *item, struct memGfx *mg);
    /* Get color of item (optional). */

    void *customPt;            /* Misc pointer variable unique to group. */
    int customInt;             /* Misc int variable unique to group. */
    int subType;               /* Variable to say what subtype this is for similar groups
                                * to share code. */
    };
struct trackGroup *tGroupList = NULL;  /* List of all tracks. */

static boolean tgLoadNothing(){return TRUE;}
static void tgDrawNothing(){}
static void tgFreeNothing(){}
static char *tgNoName(){return"";}

static int tgFixedItemHeight(struct trackGroup *tg, void *item)
/* Return item height for fixed height track. */
{
return tg->lineHeight;
}

static int tgFixedTotalHeight(struct trackGroup *tg, enum trackVisibility vis)
/* Most fixed height track groups will use this to figure out the height they use. */
{
tg->lineHeight = mgFontLineHeight(tl.font)+1;
tg->heightPer = tg->lineHeight - 1;
switch (vis)
    {
    case tvFull:
	tg->height = slCount(tg->items) * tg->lineHeight;
	break;
    case tvDense:
	tg->height = tg->lineHeight;
	break;
    }
return tg->height;
}



int tgWeirdItemStart(struct trackGroup *tg, void *item)
/* Space filler function for tracks without regular items. */
{
return -1;
}

int tgWeirdItemEnd(struct trackGroup *tg, void *item)
/* Space filler function for tracks without regular items. */
{
return -1;
}

void makeHiddenVar(char *name, char *val)
/* Make hidden variable. */
{
cgiMakeHiddenVar(name, val);	/* Variable set to let us know we called ourselves. */
putchar('\n');
}

void makeHiddenBoolean(char *name, boolean val)
{
if (val)
    makeHiddenVar(name, "1");
}

void saveHiddenVars()
/* Save all the variables we want to continue between states. */
{
char buf[16];
putchar('\n');
makeHiddenVar("old", chromName);    /* Variable set when calling ourselves. */
makeHiddenBoolean("seqReverse", seqReverse);
makeHiddenBoolean("hideControls", hideControls);
makeHiddenVar("db", database);
if(otherFrame != NULL)
    cgiMakeHiddenVar("of",otherFrame);
if(thisFrame != NULL)
    cgiMakeHiddenVar("tf",thisFrame);
cgiContinueHiddenVar("ss");
}

static struct dyString *uiStateUrlPart(struct trackGroup *toggleGroup)
/* Return a string that contains all the UI state in CGI var
 * format.  If toggleGroup is non-null the visibility of that
 * group will be toggled in the string. */
{
struct dyString *dy = newDyString(512);
struct trackGroup *tg;

dyStringPrintf(dy, "&db=%s&pix=%d", database, tl.picWidth);
if (eUserSeqString != NULL)
    dyStringPrintf(dy, "&ss=%s", eUserSeqString);
if (withLeftLabels)
    dyStringPrintf(dy, "&leftLabels=on");
if (withCenterLabels)
    dyStringPrintf(dy, "&centerLabels=on");
if (withGuidelines)
    dyStringPrintf(dy, "&guidelines=on");
if (withRuler)
    dyStringPrintf(dy, "&ruler=on");
else
    dyStringPrintf(dy, "&ruler=off");
for (tg = tGroupList; tg != NULL; tg = tg->next)
    {
    int vis = tg->visibility;
    if (tg == toggleGroup)
	{
	if (vis == tvDense)
	    vis = tvFull;
	else if (vis == tvFull)
	    vis = tvDense;
	}
    dyStringPrintf(dy, "&%s=%s", tg->mapName, tvStrings[vis]);
    }
/* Chuck code to sync with frames */
if(otherFrame)
    dyStringPrintf(dy, "&of=%s",otherFrame);
if(thisFrame)
    dyStringPrintf(dy, "&th=%s",thisFrame);
return dy;
}

char *hgTracksDefUrl()
/* Return URL of browser plus options that keep the UI state
 * intact. */
{
static struct dyString *dy = NULL;
if (dy == NULL)
    {
    struct dyString *ui = uiStateUrlPart(NULL);
    dy = newDyString(0);
    dyStringPrintf(dy, "%s%s", hgTracksName(), ui->string);
    freeDyString(&ui);
    }
return dy->string;
}



void mapBoxReinvoke(int x, int y, int width, int height, 
	struct trackGroup *toggleGroup, char *chrom,
	int start, int end, char *message)
/* Print out image map rectangle that would invoke this program again.
 * If toggleGroup is non-NULL then toggle that track between full and dense.
 * If chrom is non-null then jump to chrom:start-end. */
{
struct dyString *ui = uiStateUrlPart(toggleGroup);

printf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
if (chrom == NULL)
    {
    chrom = chromName;
    start = winStart;
    end = winEnd;
    }
printf("HREF=\"%s?seqName=%s&old=%s&winStart=%d&winEnd=%d", 
	hgTracksName(), chrom, chrom, start, end);
printf("%s\"", ui->string);
freeDyString(&ui);

if (toggleGroup)
    printf(" ALT= \"Change between dense and full view of %s track\">\n", 
           toggleGroup->shortLabel);
else
    printf(" ALT= \"jump to %s\">\n", message);
}


void mapBoxToggleVis(int x, int y, int width, int height, struct trackGroup *curGroup)
/* Print out image map rectangle that would invoke this program again.
 * program with the current track expanded. */
{
mapBoxReinvoke(x, y, width, height, curGroup, NULL, 0, 0, NULL);
}

void mapBoxJumpTo(int x, int y, int width, int height, char *newChrom, int newStart, int newEnd, char *message)
/* Print out image map rectangle that would invoke this program again
 * at a different window. */
{
mapBoxReinvoke(x, y, width, height, NULL, newChrom, newStart, newEnd, message);
}



void mapBoxHc(int start, int end, int x, int y, int width, int height, 
	char *group, char *item, char *statusLine)
/* Print out image map rectangle that would invoke the htc (human track click)
 * program. */
{
char *encodedItem = cgiEncode(item);
printf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
printf("HREF=\"%s?o=%d&t=%d&g=%s&i=%s&c=%s&l=%d&r=%d&db=%s&pix=%d\" ", 
    hgcName(), start, end, group, encodedItem, chromName, winStart, winEnd, 
    database, tl.picWidth);
printf("ALT= \"%s\">\n", statusLine); 
freeMem(encodedItem);
}

boolean privateVersion()
/* Returns TRUE if this is the 'private' version. */
{
static boolean gotIt = FALSE;
static boolean private = FALSE;
if (!gotIt)
    {
    char *t = getenv("HTTP_HOST");
    if (t != NULL && startsWith("genome-test", t))
        private = TRUE;
    gotIt = TRUE;
    }
return private;
}

boolean chromTableExists(char *tabSuffix)
/* Return true if chromosome specific version of table exists. */
{
char table[256];
sprintf(table, "%s%s", chromName, tabSuffix);
return hTableExists(table);
}

void drawScaledBox(struct memGfx *mg, int chromStart, int chromEnd, double scale, 
	int xOff, int y, int height, Color color)
/* Draw a box scaled from chromosome to window coordinates. */
{
int x1 = round((double)(chromStart-winStart)*scale) + xOff;
int x2 = round((double)(chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
if (w < 1)
    w = 1;
mgDrawBox(mg, x1, y, w, height, color);
}


struct simpleFeature
/* Minimal feature - just stores position in browser coordinates. */
    {
    struct simpleFeature *next;
    int start, end;			/* Start/end in browser coordinates. */
    int grayIx;                         /* Level of gray usually. */
    };

enum {lfSubXeno = 1};

struct linkedFeatures
    {
    struct linkedFeatures *next;
    int start, end;			/* Start/end in browser coordinates. */
    int tallStart, tallEnd;		/* Start/end of fat display. */
    int grayIx;				/* Average of components. */
    char name[32];			/* Accession of query seq. */
    int orientation;                    /* Orientation. */
    struct simpleFeature *components;   /* List of component simple features. */
    void *extra;			/* Extra info that varies with type. */
    };


char *linkedFeaturesName(struct trackGroup *tg, void *item)
/* Return name of item. */
{
struct linkedFeatures *lf = item;
return lf->name;
}

void freeLinkedFeatures(struct linkedFeatures **pList)
/* Free up a linked features list. */
{
struct linkedFeatures *lf;
for (lf = *pList; lf != NULL; lf = lf->next)
    slFreeList(&lf->components);
slFreeList(pList);
}

void freeLinkedFeaturesItems(struct trackGroup *tg)
/* Free up linkedFeaturesTrack items. */
{
freeLinkedFeatures((struct linkedFeatures**)(&tg->items));
}

enum {blackShadeIx=9,whiteShadeIx=0};

Color whiteIndex()
/* Return index of white. */
{
return shadesOfGray[0];
}

Color blackIndex()
/* Return index of black. */
{
return shadesOfGray[maxShade];
}

Color grayIndex()
/* Return index of gray. */
{
return shadesOfGray[(maxShade+1)/2];
}

Color lightGrayIndex()
/* Return index of light gray. */
{
return shadesOfGray[3];
}

void makeGrayShades(struct memGfx *mg)
/* Make eight shades of gray in display. */
{
int i;
for (i=0; i<=maxShade; ++i)
    {
    struct rgbColor rgb;
    int level = 255 - (255*i/maxShade);
    if (level < 0) level = 0;
    rgb.r = rgb.g = rgb.b = level;
    shadesOfGray[i] = mgFindColor(mg, rgb.r, rgb.g, rgb.b);
    }
shadesOfGray[maxShade+1] = MG_RED;
}

void mgMakeColorGradient(struct memGfx *mg, 
    struct rgbColor *start, struct rgbColor *end,
    int steps, Color *colorIxs)
/* Make a color gradient that goes smoothly from start
 * to end colors in given number of steps.  Put indices
 * in color table in colorIxs */
{
double scale = 0, invScale;
double invStep;
int i;
int r,g,b;

steps -= 1;	/* Easier to do the calculation in an inclusive way. */
invStep = 1.0/steps;
for (i=0; i<=steps; ++i)
    {
    invScale = 1.0 - scale;
    r = invScale * start->r + scale * end->r;
    g = invScale * start->g + scale * end->g;
    b = invScale * start->b + scale * end->b;
    colorIxs[i] = mgFindColor(mg, r, g, b);
    scale += invStep;
    }
}

void makeBrownShades(struct memGfx *mg)
/* Make some shades of brown in display. */
{
int i;
mgMakeColorGradient(mg, &tanColor, &brownColor, maxShade+1, shadesOfBrown);
}

void makeSeaShades(struct memGfx *mg)
/* Make some shades of brown in display. */
{
int i;
mgMakeColorGradient(mg, &lightSeaColor, &darkSeaColor, maxShade+1, shadesOfSea);
}


int percentGrayIx(int percent)
/* Return gray shade corresponding to a number from 50 - 100 */
{
int opPercent = 100-percent;
int adjPercent = 100-opPercent-(opPercent>>1);
int level = (adjPercent*maxShade + 50)/100;
if (level < 0) level = 0;
if (level > maxShade) level = maxShade;
return level;
}

int grayInRange(int val, int minVal, int maxVal)
/* Return gray shade corresponding to a number from minVal - maxVal */
{
int range = maxVal - minVal;
int level;
level = ((val-minVal)*maxShade + (range>>1))/range;
if (level <= 0) level = 1;
if (level > maxShade) level = maxShade;
return level;
}


int pslGrayIx(struct psl *psl, boolean isXeno)
/* Figure out gray level for an RNA block. */
{
double misFactor;
double hitFactor;
int res;

if (isXeno)
    {
    misFactor = (psl->misMatch + psl->qNumInsert + psl->tNumInsert)*2.5;
    }
else
    {
    misFactor = (psl->misMatch + psl->qNumInsert)*5;
    }
misFactor /= (psl->match + psl->misMatch + psl->repMatch);
hitFactor = 1.0 - misFactor;
res = round(hitFactor * maxShade);
if (res < 0) res = 0;
if (res >= maxShade) res = maxShade-1;
return res;
}


static int cmpLfWhiteToBlack(const void *va, const void *vb)
/* Help sort from white to black. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
return a->grayIx - b->grayIx;
}

static int cmpLfBlackToWhite(const void *va, const void *vb)
/* Help sort from white to black. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
return b->grayIx - a->grayIx;
}

void sortByGray(struct trackGroup *tg, enum trackVisibility vis)
/* Sort linked features by grayIx. */
{
if (vis == tvDense)
    slSort(&tg->items, cmpLfWhiteToBlack);
else
    slSort(&tg->items, cmpLfBlackToWhite);
}

static void linkedFeaturesDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw linked features items. */
{
int baseWidth = seqEnd - seqStart;
struct linkedFeatures *lf;
struct simpleFeature *sf;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2;
int midLineOff = heightPer/2;
int shortOff = 2, shortHeight = heightPer-4;
int tallStart, tallEnd, s, e, e2, s2;
int itemOff, itemHeight;
boolean isFull = (vis == tvFull);
Color *shades = tg->colorShades;
Color bColor = tg->ixAltColor;
double scale = width/(double)baseWidth;
boolean isXeno = tg->subType == lfSubXeno;
boolean hideLine = (vis == tvDense && tg->subType == lfSubXeno);

if (vis == tvDense)
    sortByGray(tg, vis);
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    int midY = y + midLineOff;
    int compCount = 0;
    int w;
    if (tg->itemColor && shades == NULL)
	color = tg->itemColor(tg, lf, mg);
    tallStart = lf->tallStart;
    tallEnd = lf->tallEnd;
    if (lf->components != NULL && !hideLine)
	{
	x1 = round((double)((int)lf->start-winStart)*scale) + xOff;
	x2 = round((double)((int)lf->end-winStart)*scale) + xOff;
	w = x2-x1;
	if (isFull)
	    {
	    if (shades) bColor =  shades[(lf->grayIx>>1)];
	    mgBarbedHorizontalLine(mg, x1, midY, x2-x1, 2, 5, 
		    lf->orientation, bColor);
	    }
	if (shades) color =  shades[lf->grayIx+isXeno];
	mgDrawBox(mg, x1, midY, w, 1, color);
	}
    for (sf = lf->components; sf != NULL; sf = sf->next)
	{
	if (shades) color =  shades[lf->grayIx+isXeno];
	s = sf->start;
	e = sf->end;
	if (s < tallStart)
	    {
	    e2 = e;
	    if (e2 > tallStart) e2 = tallStart;
	    drawScaledBox(mg, s, e2, scale, xOff, y+shortOff, shortHeight, color);
	    s = e2;
	    }
	if (e > tallEnd)
	    {
	    s2 = s;
	    if (s2 < tallEnd) s2 = tallEnd;
	    drawScaledBox(mg, s2, e, scale, xOff, y+shortOff, shortHeight, color);
	    e = s2;
	    }
	if (e > s)
	    {
	    drawScaledBox(mg, s, e, scale, xOff, y, heightPer, color);
	    ++compCount;
	    }
	}
    if (isFull)
	y += lineHeight;
    }
}

void incRange(UBYTE *start, int size)
/* Add one to range of bytes, taking care to not overflow. */
{
int i;
UBYTE b;
for (i=0; i<size; ++i)
    {
    b = start[i];
    if (b < 254)
	start[i] = b+2;
    }
}

void resampleBytes(UBYTE *s, int sct, UBYTE *d, int dct)
/* Shrink or stretch an line of bytes. */
{
#define WHOLESCALE 256
if (sct > dct)	/* going to do some averaging */
	{
	int i;
	int j, jend, lj;
	long lasts, ldiv;
	long acc, div;
	long t1,t2;

	ldiv = WHOLESCALE;
	lasts = s[0];
	lj = 0;
	for (i=0; i<dct; i++)
		{
		acc = lasts*ldiv;
		div = ldiv;
		t1 = (i+1)*(long)sct;
		jend = t1/dct;
		for (j = lj+1; j<jend; j++)
			{
			acc += s[j]*WHOLESCALE;
			div += WHOLESCALE;
			}
		t2 = t1 - jend*(long)dct;
		lj = jend;
		lasts = s[lj];
		if (t2 == 0)
			{
			ldiv = WHOLESCALE;
			}
		else
			{
			ldiv = WHOLESCALE*t2/dct;
			div += ldiv;
			acc += lasts*ldiv;
			ldiv = WHOLESCALE-ldiv;
			}
		*d++ = acc/div;
		}
	}
else if (dct == sct)	/* they's the same */
	{
	while (--dct >= 0)
		*d++ = *s++;
	}
else if (sct == 1)
	{
	while (--dct >= 0)
		*d++ = *s;
	}
else/* going to do some interpolation */
	{
	int i;
	long t1;
	long p1;
	long err;
	int dct2;

	dct -= 1;
	sct -= 1;
	dct2 = dct/2;
	t1 = 0;
	for (i=0; i<=dct; i++)
		{
		p1 = t1/dct;
		err =  t1 - p1*dct;
		if (err == 0)
			*d++ = s[p1];
		else
			*d++ = (s[p1]*(dct-err)+s[p1+1]*err+dct2)/dct;
		t1 += sct;
		}
	}
}

void grayThreshold(UBYTE *pt, int count)
/* Convert from 0-4 representation to gray scale rep. */
{
UBYTE b;
int i;

for (i=0; i<count; ++i)
    {
    b = pt[i];
    if (b == 0)
	pt[i] = shadesOfGray[0];
    else if (b == 1)
	pt[i] = shadesOfGray[2];
    else if (b == 2)
	pt[i] = shadesOfGray[4];
    else if (b == 3)
	pt[i] = shadesOfGray[6];
    else if (b >= 4)
	pt[i] = shadesOfGray[9];
    }
}


static void linkedFeaturesDrawAverage(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense clone items. */
{
int baseWidth = seqEnd - seqStart;
UBYTE *useCounts;
int i;
int lineHeight = mgFontLineHeight(font);
struct simpleFeature *sf;
struct linkedFeatures *lf;
int x1, x2, w;

AllocArray(useCounts, width);
memset(useCounts, 0, width * sizeof(useCounts[0]));
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    for (sf = lf->components; sf != NULL; sf = sf->next)
	{
	x1 = roundingScale(sf->start-winStart, width, baseWidth);
	if (x1 < 0)
	   x1 = 0;
	x2 = roundingScale(sf->end-winStart, width, baseWidth);
	if (x2 >= width)
	   x2 = width-1;
	w = x2-x1;
	if (w >= 0)
	    {
	    if (w == 0)
	        w = 1;
	    incRange(useCounts+x1, w); 
	    }
	}
    }
grayThreshold(useCounts, width);
for (i=0; i<lineHeight; ++i)
    mgPutSegZeroClear(mg, xOff, yOff+i, width, useCounts);
freeMem(useCounts);
}

static void linkedFeaturesAverageDense(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense linked features items. */
{
if (vis == tvFull)
    {
    linkedFeaturesDraw(tg, seqStart, seqEnd, mg, xOff, yOff, width, font, color, vis);
    }
else if (vis == tvDense)
    {
    linkedFeaturesDrawAverage(tg, seqStart, seqEnd, mg, xOff, yOff, width, font, color, vis);
    }
}

int lfCalcGrayIx(struct linkedFeatures *lf)
/* Calculate gray level from components. */
{
struct simpleFeature *sf;
int count = 0;
int total = 0;

for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    ++count;
    total += sf->grayIx;
    }
if (count == 0)
    return whiteShadeIx;
return (total+(count>>1))/count;
}

void finishLf(struct linkedFeatures *lf)
/* Calculate beginning and end of lf from components, etc. */
{
struct simpleFeature *sf;

slReverse(&lf->components);
if ((sf = lf->components) != NULL)
    {
    int start = sf->start;
    int end = sf->end;

    for (sf = sf->next; sf != NULL; sf = sf->next)
	{
	if (sf->start < start)
	    start = sf->start;
	if (sf->end > end)
	    end = sf->end;
	}
    lf->start = lf->tallStart = start;
    lf->end = lf->tallEnd = end;
    }
lf->grayIx = lfCalcGrayIx(lf);
}

int linkedFeaturesItemStart(struct trackGroup *tg, void *item)
/* Return start chromosome coordinate of item. */
{
struct linkedFeatures *lf = item;
return lf->start;
}

int linkedFeaturesItemEnd(struct trackGroup *tg, void *item)
/* Return end chromosome coordinate of item. */
{
struct linkedFeatures *lf = item;
return lf->end;
}

struct trackGroup *linkedFeaturesTg()
/* Return generic track group for linked features. */
{
struct trackGroup *tg = NULL;

AllocVar(tg);
tg->freeItems = freeLinkedFeaturesItems;
tg->drawItems = linkedFeaturesDraw;
tg->colorShades = shadesOfGray;
tg->itemName = linkedFeaturesName;
tg->mapItemName = linkedFeaturesName;
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = linkedFeaturesItemStart;
tg->itemEnd = linkedFeaturesItemEnd;
return tg;
}

int orientFromChar(char c)
/* Return 1 or -1 in place of + or - */
{
if (c == '-')
    return -1;
else
    return 1;
}

char charFromOrient(int orient)
/* Return + or - in place of 1 or -1 */
{
if (orient < 0)
    return '-';
else
    return '+';
}

void setTgDarkLightColors(struct trackGroup *tg, int r, int g, int b)
/* Set track group color to r,g,b.  Set altColor to a lighter version
 * of the same. */
{
tg->colorShades = NULL;
tg->color.r = r;
tg->color.g = g;
tg->color.b = b;
tg->altColor.r = (r+255)/2;
tg->altColor.g = (g+255)/2;
tg->altColor.b = (b+255)/2;
}


struct linkedFeatures *lfFromPslx(struct psl *psl, int sizeMul, boolean isXeno)
/* Create a linked feature item from pslx.  Pass in sizeMul=1 for DNA, 
 * sizeMul=3 for protein. */
{
unsigned *starts = psl->tStarts;
unsigned *sizes = psl->blockSizes;
int i, blockCount = psl->blockCount;
int grayIx = pslGrayIx(psl, isXeno);
struct simpleFeature *sfList = NULL, *sf;
struct linkedFeatures *lf;
boolean rcTarget = (psl->strand[1] == '-');

AllocVar(lf);
lf->grayIx = grayIx;
strncpy(lf->name, psl->qName, sizeof(lf->name));
lf->orientation = orientFromChar(psl->strand[0]);
if (rcTarget)
    lf->orientation = -lf->orientation;
for (i=0; i<blockCount; ++i)
    {
    AllocVar(sf);
    sf->start = sf->end = starts[i];
    sf->end += sizes[i]*sizeMul;
    if (rcTarget)
        {
	int s, e;
	s = psl->tSize - sf->end;
	e = psl->tSize - sf->start;
	sf->start = s;
	sf->end = e;
	}
    sf->grayIx = grayIx;
    slAddHead(&sfList, sf);
    }
slReverse(&sfList);
lf->components = sfList;
finishLf(lf);
lf->start = psl->tStart;	/* Correct for rounding errors... */
lf->end = psl->tEnd;
return lf;
}

struct linkedFeatures *lfFromPsl(struct psl *psl, boolean isXeno)
/* Create a linked feature item from psl. */
{
return lfFromPslx(psl, 1, isXeno);
}



struct linkedFeatures *lfFromPslsInRange(char *table, int start, int end, char *chromName, boolean isXeno)
/* Return linked features from range of table. */
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct linkedFeatures *lfList = NULL, *lf;

if (chromName == NULL)
    {
    sprintf(query, "select * from %s where tStart<%u and tEnd>%u",
	table, winEnd, winStart);
    }
else
    {
    sprintf(query, "select * from %s where tName = '%s' and tStart<%u and tEnd>%u",
	table, chromName, winEnd, winStart);
    }
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct psl *psl = pslLoad(row);
    lf = lfFromPsl(psl, isXeno);
    slAddHead(&lfList, lf);
    pslFree(&psl);
    }
slReverse(&lfList);
sqlFreeResult(&sr);
hFreeConn(&conn);
return lfList;
}



void loadMrnaAli(struct trackGroup *tg)
/* Load up rnas from table into trackGroup items. */
{
char table[64];
sprintf(table, "%s_mrna", chromName);
tg->items = lfFromPslsInRange(table, winStart, winEnd, NULL, FALSE);
}

#ifdef FUREY_CODE
void loadBacEnds(struct trackGroup *tg)
/* Load up bac ends from table into trackGroup items. */
{
tg->items = lfFromPslsInRange("bacEnds", winStart, winEnd, NULL, FALSE);
}
#endif /* FUREY_CODE */

#ifdef ROGIC_CODE
struct linkedFeatures *lfFromPslsInRangeByChrom(char *table, char *chrom, int start, int end)
/* Return linked features from range of table. */
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct linkedFeatures *lfList = NULL, *lf;

sprintf(query, "select * from %s where tName = '%s' and tStart<%u and tEnd>%u",
    table, chrom, winEnd, winStart);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct psl *psl = pslLoad(row);
    lf = lfFromPsl(psl, FALSE);
    slAddHead(&lfList, lf);
    pslFree(&psl);
    }
slReverse(&lfList);
sqlFreeResult(&sr);
hFreeConn(&conn);
return lfList;
}

void loadMgcMrnaAli(struct trackGroup *tg)
/* Load up rnas from table into trackGroup items. */
{
tg->items = lfFromPslsInRangeByChrom("mgc_mrna", chromName, winStart, winEnd);
}

struct trackGroup *fullMgcMrnaTg()
/* Make track group of full length mRNAs. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "mgc_mrna";
tg->visibility = tvHide;
tg->longLabel = "Full Length MGC mRNAs";
tg->shortLabel = "Full MGC mRNAs";
tg->loadItems = loadMgcMrnaAli;
return tg;
}
#endif /* ROGIC_CODE */

struct trackGroup *fullMrnaTg()
/* Make track group of full length mRNAs. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgMrna";
tg->visibility = tvFull;
tg->longLabel = "Full Length mRNAs";
tg->shortLabel = "Full mRNAs";
tg->loadItems = loadMrnaAli;
return tg;
}

#ifdef FUREY_CODE
struct trackGroup *bacEndsTg()
/* Make track group of BAC end pairs. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "BACends";
tg->visibility = tvHide;
tg->longLabel = "BAC end pairs";
tg->shortLabel = "BAC ends";
tg->loadItems = loadBacEnds;
return tg;
}
#endif /* FUREY_CODE */

char *usrPslMapName(struct trackGroup *tg, void *item)
/* Return name of item. */
{
struct linkedFeatures *lf = item;
return lf->extra;
}

void loadUserPsl(struct trackGroup *tg)
/* Load up rnas from table into trackGroup items. */
{
char *ss = cgiString("ss");
char buf[1024];
char buf2[3*512];
char *ssWords[4];
int wordCount;
char *faFileName, *pslFileName;
struct lineFile *f;
struct psl *psl;
struct linkedFeatures *lfList = NULL, *lf;
enum gfType qt, tt;
int sizeMul = 1;

strcpy(buf, ss);
wordCount = chopLine(buf, ssWords);
if (wordCount < 2)
    errAbort("Badly formated ss variable");
pslFileName = ssWords[0];
faFileName = ssWords[1];


pslxFileOpen(pslFileName, &qt, &tt, &f);
if (qt == gftProt)
    {
    setTgDarkLightColors(tg, 0, 80, 150);
    tg->colorShades = NULL;
    sizeMul = 3;
    }
tg->itemName = linkedFeaturesName;
while ((psl = pslNext(f)) != NULL)
    {
    if (sameString(psl->tName, chromName) && psl->tStart < winEnd && psl->tEnd > winStart)
	{
	lf = lfFromPslx(psl, sizeMul, TRUE);
	sprintf(buf2, "%s %s", ss, psl->qName);
	lf->extra = cloneString(buf2);
	slAddHead(&lfList, lf);
	}
    pslFree(&psl);
    }
slReverse(&lfList);
lineFileClose(&f);
tg->items = lfList;
}


struct trackGroup *userPslTg()
/* Make track group of user pasted sequence. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgUserPsl";
tg->visibility = tvFull;
tg->longLabel = "Your Sequence from BLAT Search";
tg->shortLabel = "BLAT Sequence";
tg->loadItems = loadUserPsl;
tg->mapItemName = usrPslMapName;
return tg;
}

void loadEstAli(struct trackGroup *tg)
/* Load up rnas from table into trackGroup items. */
{
char table[64];
sprintf(table, "%s_est", chromName);
tg->items = lfFromPslsInRange(table, winStart, winEnd, NULL, FALSE);
}

struct trackGroup *estTg()
/* Make track group of full length mRNAs. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgEst";
tg->visibility = tvHide;
tg->longLabel = "Human ESTs";
tg->shortLabel = "Human ESTs";
tg->loadItems = loadEstAli;
tg->drawItems = linkedFeaturesAverageDense;
return tg;
}

void loadIntronEstAli(struct trackGroup *tg)
/* Load up rnas from table into trackGroup items. */
{
char table[64];
sprintf(table, "%s_intronEst", chromName);
tg->items = lfFromPslsInRange(table, winStart, winEnd, NULL, FALSE);
}

struct trackGroup *intronEstTg()
/* Make track group of full length mRNAs. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgIntronEst";
tg->visibility = tvDense;
tg->longLabel = "Human ESTs That Have Been Spliced";
tg->shortLabel = "Spliced ESTs";
tg->loadItems = loadIntronEstAli;
tg->drawItems = linkedFeaturesAverageDense;
return tg;
}

void loadMusTest1(struct trackGroup *tg)
/* Load up mouse alignments (psl format) from table. */
{
tg->items = lfFromPslsInRange("musTest1", winStart, winEnd, chromName, TRUE);
}

struct trackGroup *musTest1Tg()
/* Make track group of full length mRNAs. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgMusTest1";
tg->visibility = tvHide;
tg->longLabel = "Mouse Translated Blat Alignments Score > 40";
tg->shortLabel = "Mouse Test 40";
tg->loadItems = loadMusTest1;
return tg;
}

void loadMusTest2(struct trackGroup *tg)
/* Load up mouse alignments (psl format) from table. */
{
tg->items = lfFromPslsInRange("musTest2", winStart, winEnd, chromName, TRUE);
}

struct trackGroup *musTest2Tg()
/* Make track group of full length mRNAs. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgMusTest2";
tg->visibility = tvHide;
tg->longLabel = "Mouse Translated Blat Alignments Score > 6";
tg->shortLabel = "Mouse Test 6";
tg->loadItems = loadMusTest2;
return tg;
}

void loadBlatMouse(struct trackGroup *tg)
/* Load up mouse alignments (psl format) from table. */
{
char table[64];
sprintf(table, "%s_blatMouse", chromName);
tg->items = lfFromPslsInRange(table, winStart, winEnd, NULL, TRUE);
}

struct trackGroup *blatMouseTg()
/* Make track group of mouse alignments. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgBlatMouse";
tg->visibility = tvDense;
tg->longLabel = "Mouse Translated Blat Alignments";
tg->shortLabel = "Mouse Blat";
tg->loadItems = loadBlatMouse;
tg->subType = lfSubXeno;
tg->colorShades = shadesOfBrown;
tg->color.r = brownColor.r;
tg->color.g = brownColor.g;
tg->color.b = brownColor.b;
return tg;
}

void loadMus7of8(struct trackGroup *tg)
/* Load up mouse alignments (psl format) from table. */
{
tg->items = lfFromPslsInRange("mus7of8", winStart, winEnd, NULL, TRUE);
}

struct trackGroup *mus7of8Tg()
/* Make track group of mouse alignments. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgMus7of8";
tg->visibility = tvDense;
tg->longLabel = "Clipped Mouse Translated Blat Alignments, 7 of 8 Seed";
tg->shortLabel = "Mouse 7 of 8";
tg->loadItems = loadMus7of8;
tg->subType = lfSubXeno;
tg->colorShades = shadesOfBrown;
tg->color.r = brownColor.r;
tg->color.g = brownColor.g;
tg->color.b = brownColor.b;
return tg;
}

void loadMusPairOf4(struct trackGroup *tg)
/* Load up mouse alignments (psl format) from table. */
{
tg->items = lfFromPslsInRange("musPairOf4", winStart, winEnd, NULL, TRUE);
}

struct trackGroup *musPairOf4Tg()
/* Make track group of mouse alignments. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgMusPairOf4";
tg->visibility = tvDense;
tg->longLabel = "Clipped Mouse Translated Blat Alignments, Pair of 4 Seed";
tg->shortLabel = "Mouse Pair of 4";
tg->loadItems = loadMusPairOf4;
tg->subType = lfSubXeno;
tg->colorShades = shadesOfBrown;
tg->color.r = brownColor.r;
tg->color.g = brownColor.g;
tg->color.b = brownColor.b;
return tg;
}



struct linkedFeatures *lfFromGenePredInRange(char *table, 
	char *chrom, int start, int end)
/* Return linked features from range of a gene prediction table. */
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct linkedFeatures *lfList = NULL, *lf;
int grayIx = maxShade;

sprintf(query, "select * from %s where chrom='%s' and txStart<%u and txEnd>%u",
    table, chrom, winEnd, winStart);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct simpleFeature *sfList = NULL, *sf;
    struct genePred *gp = genePredLoad(row);
    unsigned *starts = gp->exonStarts;
    unsigned *ends = gp->exonEnds;
    int i, blockCount = gp->exonCount;

    AllocVar(lf);
    lf->grayIx = grayIx;
    strncpy(lf->name, gp->name, sizeof(lf->name));
    lf->orientation = orientFromChar(gp->strand[0]);
    for (i=0; i<blockCount; ++i)
	{
	AllocVar(sf);
	sf->start = starts[i];
	sf->end = ends[i];
	sf->grayIx = grayIx;
	slAddHead(&sfList, sf);
	}
    slReverse(&sfList);
    lf->components = sfList;
    finishLf(lf);
    lf->tallStart = gp->cdsStart;
    lf->tallEnd = gp->cdsEnd;
    slAddHead(&lfList, lf);
    genePredFree(&gp);
    }
slReverse(&lfList);
sqlFreeResult(&sr);
hFreeConn(&conn);
return lfList;
}

void abbr(char *s, char *fluff)
/* Cut out fluff from s. */
{
int len;
s = strstr(s, fluff);
if (s != NULL)
   {
   len = strlen(fluff);
   strcpy(s, s+len);
   }
}

char *genieName(struct trackGroup *tg, void *item)
/* Return abbreviated genie name. */
{
struct linkedFeatures *lf = item;
char *full = lf->name;
static char abbrev[32];

strncpy(abbrev, full, sizeof(abbrev));
abbr(abbrev, "00000");
abbr(abbrev, "0000");
abbr(abbrev, "000");
abbr(abbrev, "ctg");
abbr(abbrev, "Affy.");
return abbrev;
}

char *genieMapName(struct trackGroup *tg, void *item)
/* Return un-abbreviated genie name. */
{
struct linkedFeatures *lf = item;
return lf->name;
}


void loadGenieAlt(struct trackGroup *tg)
/* Load up Genie alt genes. */
{
tg->items = lfFromGenePredInRange("genieAlt", chromName, winStart, winEnd);
}

struct trackGroup *genieAltTg()
/* Make track group of full length mRNAs. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "genieAlt";
tg->visibility = tvDense;
tg->longLabel = "Affymetrix Gene Predictions";
tg->shortLabel = "Affy Genes";
tg->loadItems = loadGenieAlt;
setTgDarkLightColors(tg, 125, 0, 150);
tg->itemName = genieName;
tg->mapItemName = genieMapName;
return tg;
}

void lookupKnownNames(struct linkedFeatures *lfList)
/* This converts the Genie ID to the HUGO name where possible. */
{
struct linkedFeatures *lf;
char query[256];
struct sqlConnection *conn = hAllocConn();
char *newName;

if (hTableExists("knownMore"))
    {
    struct knownMore *km;
    struct sqlResult *sr;
    char **row;

    for (lf = lfList; lf != NULL; lf = lf->next)
	{
	sprintf(query, "select * from knownMore where transId = '%s'", lf->name);
	sr = sqlGetResult(conn, query);
	if ((row = sqlNextRow(sr)) != NULL)
	    {
	    km = knownMoreLoad(row);
	    strncpy(lf->name, km->name, sizeof(lf->name));
	    if (km->omimId)
	        lf->extra = km;
	    else
	        knownMoreFree(&km);
	    }
	sqlFreeResult(&sr);
	}
    }
else if (hTableExists("knownInfo"))
    {
    for (lf = lfList; lf != NULL; lf = lf->next)
	{
	sprintf(query, "select name from knownInfo where transId = '%s'", lf->name);
	sqlQuickQuery(conn, query, lf->name, sizeof(lf->name));
	}
    }
hFreeConn(&conn);
}

void loadGenieKnown(struct trackGroup *tg)
/* Load up Genie known genes. */
{
tg->items = lfFromGenePredInRange("genieKnown", chromName, winStart, winEnd);
if (tg->visibility == tvFull && 
	slCount(tg->items) <= maxItemsInFullTrack)
    {
    lookupKnownNames(tg->items);
    }
}

Color genieKnownColor(struct trackGroup *tg, void *item, struct memGfx *mg)
/* Return color to draw known gene in. */
{
struct linkedFeatures *lf = item;

if (startsWith("AK.", lf->name))
    {
    static int colIx = 0;
    if (!colIx)
	colIx = mgFindColor(mg, 0, 120, 200);
    return colIx;
    }
#ifdef SOMETIMES
else if (lf->extra)
    {
    static int colIx = 0;
    if (!colIx)
	colIx = mgFindColor(mg, 200, 0, 0);
    return colIx;
    }
#endif /* SOMETIMES */
else
    {
    return tg->ixColor;
    }
}

struct trackGroup *genieKnownTg()
/* Make track group of known genes. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "genieKnown";
tg->visibility = tvFull;
tg->longLabel = "Known Genes (from full length mRNAs)";
tg->shortLabel = "Known Genes";
tg->loadItems = loadGenieKnown;
setTgDarkLightColors(tg, 20, 20, 170);
tg->itemName = genieName;
tg->mapItemName = genieMapName;
tg->itemColor = genieKnownColor;
return tg;
}

char *refGeneName(struct trackGroup *tg, void *item)
/* Return abbreviated genie name. */
{
struct linkedFeatures *lf = item;
if (lf->extra != NULL) return lf->extra;
else return lf->name;
}

char *refGeneMapName(struct trackGroup *tg, void *item)
/* Return un-abbreviated genie name. */
{
struct linkedFeatures *lf = item;
return lf->name;
}

void lookupRefNames(struct linkedFeatures *lfList)
/* This converts the refSeq accession to a gene name where possible. */
{
struct linkedFeatures *lf;
char query[256];
struct sqlConnection *conn = hAllocConn();
char *newName;

if (hTableExists("refLink"))
    {
    struct knownMore *km;
    struct sqlResult *sr;
    char **row;

    for (lf = lfList; lf != NULL; lf = lf->next)
	{
	sprintf(query, "select name from refLink where mrnaAcc = '%s'", lf->name);
	sr = sqlGetResult(conn, query);
	if ((row = sqlNextRow(sr)) != NULL)
	    {
	    lf->extra = cloneString(row[0]);
	    }
	sqlFreeResult(&sr);
	}
    }
hFreeConn(&conn);
}


void loadRefGene(struct trackGroup *tg)
/* Load up RefSeq known genes. */
{
tg->items = lfFromGenePredInRange("refGene", chromName, winStart, winEnd);
if (tg->visibility == tvFull && 
	slCount(tg->items) <= maxItemsInFullTrack)
    {
    lookupRefNames(tg->items);
    }
}


struct trackGroup *refGeneTg()
/* Make track group of known genes from refSeq. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgRefGene";
tg->visibility = tvFull;
tg->longLabel = "Known Genes (from RefSeq)";
tg->shortLabel = "Known Genes";
tg->loadItems = loadRefGene;
setTgDarkLightColors(tg, 20, 20, 170);
tg->itemName = refGeneName;
tg->mapItemName = refGeneMapName;
return tg;
}


void loadEnsGene(struct trackGroup *tg)
/* Load up Ensembl genes. */
{
tg->items = lfFromGenePredInRange("ensGene", chromName, winStart, winEnd);
}

char *ensGeneName(struct trackGroup *tg, void *item)
/* Return abbreviated ensemble gene name. */
{
struct linkedFeatures *lf = item;
char *full = lf->name;
static char abbrev[32];

strncpy(abbrev, full, sizeof(abbrev));
abbr(abbrev, "SEPT20T.");
return abbrev;
}

char *ensGeneMapName(struct trackGroup *tg, void *item)
/* Return full ensemble gene name. */
{
struct linkedFeatures *lf = item;
return lf->name;
}

struct trackGroup *ensemblGeneTg()
/* Make track group of Ensembl predictions. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgEnsGene";
tg->visibility = tvDense;
tg->longLabel = "Ensembl Gene Predictions";
tg->shortLabel = "Ensembl Genes";
tg->loadItems = loadEnsGene;
setTgDarkLightColors(tg, 150, 0, 0);
tg->itemName = ensGeneName;
tg->mapItemName = ensGeneMapName;
return tg;
}

void loadSoftberryGene(struct trackGroup *tg)
/* Load up Softberry genes. */
{
tg->items = lfFromGenePredInRange("softberryGene", chromName, winStart, winEnd);
}

struct trackGroup *softberryGeneTg()
/* Make track group of Softberry predictions. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgSoftberryGene";
tg->visibility = tvDense;
tg->longLabel = "Fgenesh++ Gene Predictions";
tg->shortLabel = "Fgenesh++ Genes";
tg->loadItems = loadSoftberryGene;
setTgDarkLightColors(tg, 0, 100, 0);
return tg;
}

void loadSanger22Gene(struct trackGroup *tg)
/* Load up Softberry genes. */
{
tg->items = lfFromGenePredInRange("sanger22", chromName, winStart, winEnd);
}

char *sanger22Name(struct trackGroup *tg, void *item)
/* Return Sanger22 name. */
{
struct linkedFeatures *lf = item;
char *full = lf->name;
static char abbrev[64];

strncpy(abbrev, full, sizeof(abbrev));
abbr(abbrev, "Em:");
abbr(abbrev, ".C22");
//abbr(abbrev, ".mRNA");
return abbrev;
}


struct trackGroup *sanger22Tg()
/* Make track group of Sanger's chromosome 22 gene annotations. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgSanger22";
tg->visibility = tvFull;
tg->longLabel = "Sanger Centre Chromosome 22 Annotations";
tg->shortLabel = "Sanger 22";
tg->loadItems = loadSanger22Gene;
tg->itemName = sanger22Name;
setTgDarkLightColors(tg, 0, 100, 180);
return tg;
}


void goldLoad(struct trackGroup *tg)
/* Load up golden path from database table to trackGroup items. */
{
char table[64];
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct agpFrag *fragList = NULL, *frag;
struct agpGap *gapList = NULL, *gap;

/* Get the frags and load into tg->items. */
sprintf(table, "%s_gold", chromName);
sprintf(query, "select * from %s where chromStart<%u and chromEnd>%u",
    table, winEnd, winStart);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    frag = agpFragLoad(row);
    slAddHead(&fragList, frag);
    }
slReverse(&fragList);
sqlFreeResult(&sr);
tg->items = fragList;

/* Get the gaps into tg->customPt. */
sprintf(table, "%s_gap", chromName);
sprintf(query, "select * from %s where chromStart<%u and chromEnd>%u",
    table, winEnd, winStart);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gap = agpGapLoad(row);
    slAddHead(&gapList, gap);
    }
slReverse(&gapList);
sqlFreeResult(&sr);
tg->customPt = gapList;
hFreeConn(&conn);
}

void goldFree(struct trackGroup *tg)
/* Free up goldTrackGroup items. */
{
agpFragFreeList((struct agpFrag**)&tg->items);
agpGapFreeList((struct agpGap**)&tg->customPt);
}

char *goldName(struct trackGroup *tg, void *item)
/* Return name of gold track item. */
{
struct agpFrag *frag = item;
return frag->frag;
}

static void goldDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw golden path items. */
{
int baseWidth = seqEnd - seqStart;
struct agpFrag *frag;
struct agpGap *gap;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2,w;
int midLineOff = heightPer/2;
boolean isFull = (vis == tvFull);
Color brown = color;
Color gold = tg->ixAltColor;
Color col;
int ix = 0;
double scale = (double)width/(double)baseWidth;

/* Draw gaps if any. */
if (!isFull)
    {
    int midY = y + midLineOff;
    for (gap = tg->customPt; gap != NULL; gap = gap->next)
	{
	if (!sameWord(gap->bridge, "no"))
	    {
	    drawScaledBox(mg, gap->chromStart, gap->chromEnd, scale, xOff, midY, 1, brown);
	    }
	}
    }

for (frag = tg->items; frag != NULL; frag = frag->next)
    {
    x1 = round((double)((int)frag->chromStart-winStart)*scale) + xOff;
    x2 = round((double)((int)frag->chromEnd-winStart)*scale) + xOff;
    w = x2-x1;
    color =  ((ix&1) ? gold : brown);
    if (w < 1)
	w = 1;
    mgDrawBox(mg, x1, y, w, heightPer, color);
    if (isFull)
	y += lineHeight;
    else if (baseWidth < 10000000)
	{
	char status[256];
	sprintf(status, "%s:%d-%d %s %s:%d-%d", 
	    frag->frag, frag->fragStart, frag->fragEnd,
	    frag->strand,
	    frag->chrom, frag->chromStart, frag->chromEnd);

	mapBoxHc(frag->chromStart, frag->chromEnd, x1,y,w,heightPer, tg->mapName, 
	    frag->frag, status);
	}
    ++ix;
    }
}

int goldItemStart(struct trackGroup *tg, void *item)
/* Return start chromosome coordinate of item. */
{
struct agpFrag *lf = item;
return lf->chromStart;
}

int goldItemEnd(struct trackGroup *tg, void *item)
/* Return end chromosome coordinate of item. */
{
struct agpFrag *lf = item;
return lf->chromEnd;
}

struct trackGroup *goldTrackGroup()
/* Make track group for golden path */
{
struct trackGroup *tg;

AllocVar(tg);
tg->mapName = "hgGold";
tg->visibility = tvHide;
tg->longLabel = "Assembly from Fragments";
tg->shortLabel = "Assembly";
tg->loadItems = goldLoad;
tg->freeItems = goldFree;
tg->drawItems = goldDraw;
tg->color.r = 150;
tg->color.g = 100;
tg->color.b = 30;
tg->altColor.r = 230;
tg->altColor.g = 170;
tg->altColor.b = 40;
tg->itemName = goldName;
tg->mapItemName = goldName;
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = goldItemStart;
tg->itemEnd = goldItemEnd;
return tg;
}

/* Repeat items.  Since there are so many of these, to avoid 
 * memory problems we don't query the database and store the results
 * during repeatLoad, but rather query the database during the
 * actual drawing. */

struct repeatItem
/* A repeat track item. */
    {
    struct repeatItem *next;
    char *class;
    char *className;
    int yOffset;
    };

static struct repeatItem *otherRepeatItem = NULL;
static char *repeatClassNames[] =  {
    "SINE", "LINE", "LTR", "DNA", "Simple", "Low Complexity", "Satellite", "tRNA", "Other",
};
static char *repeatClasses[] = {
    "SINE", "LINE", "LTR", "DNA", "Simple_repeat", "Low_complexity", "Satellite", "tRNA", "Other",
};

struct repeatItem *makeRepeatItems()
/* Make the stereotypical repeat masker tracks. */
{
struct repeatItem *ri, *riList = NULL;
int i;
int numClasses = ArraySize(repeatClasses);
for (i=0; i<numClasses; ++i)
    {
    AllocVar(ri);
    ri->class = repeatClasses[i];
    ri->className = repeatClassNames[i];
    slAddHead(&riList, ri);
    }
otherRepeatItem = riList;
slReverse(&riList);
return riList;
}

void repeatLoad(struct trackGroup *tg)
/* Load up repeat tracks.  (Will query database during drawing for a change.) */
{
tg->items = makeRepeatItems();
}

void repeatFree(struct trackGroup *tg)
/* Free up goldTrackGroup items. */
{
slFreeList(&tg->items);
}

char *repeatName(struct trackGroup *tg, void *item)
/* Return name of repeat item track. */
{
struct repeatItem *ri = item;
return ri->className;
}

static void repeatDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
{
int baseWidth = seqEnd - seqStart;
struct repeatItem *ri;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2,w;
boolean isFull = (vis == tvFull);
Color col;
int ix = 0;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
char query[256];

if (isFull)
    {
    /* Do gray scale representation spread out among tracks. */
    struct hash *hash = newHash(6);
    struct rmskOut ro;
    int percId;
    int grayLevel;
    char statusLine[128];

    for (ri = tg->items; ri != NULL; ri = ri->next)
        {
	ri->yOffset = y;
	y += lineHeight;
	hashAdd(hash, ri->class, ri);
	}
    sprintf(query, "select * from %s_rmsk where genoStart<%u and genoEnd>%u",
	chromName, winEnd, winStart);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	rmskOutStaticLoad(row, &ro);
	ri = hashFindVal(hash, ro.repClass);
	if (ri == NULL)
	   ri = otherRepeatItem;
	percId = 1000 - ro.milliDiv - ro.milliDel - ro.milliIns;
	grayLevel = grayInRange(percId, 500, 1000);
	col = shadesOfGray[grayLevel];
	x1 = roundingScale(ro.genoStart-winStart, width, baseWidth)+xOff;
	x2 = roundingScale(ro.genoEnd-winStart, width, baseWidth)+xOff;
	w = x2-x1;
	if (w <= 0)
	    w = 1;
	mgDrawBox(mg, x1, ri->yOffset, w, heightPer, col);
	if (baseWidth <= 100000)
	    {
	    if (ri == otherRepeatItem)
		{
		sprintf(statusLine, "Repeat %s, family %s, class %s",
		    ro.repName, ro.repFamily, ro.repClass);
		}
	    else
		{
		sprintf(statusLine, "Repeat %s, family %s",
		    ro.repName, ro.repFamily);
		}
	    mapBoxHc(ro.genoStart, ro.genoEnd, x1, ri->yOffset, w, heightPer, tg->mapName,
	    	ro.repName, statusLine);
#ifdef OLDCRUFT
	    printf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", 
	    	x1, ri->yOffset, x1+w, ri->yOffset+heightPer);
	    printf("HREF=\"../cgi-bin/hgc?g=%s&i=%s&c=%s&l=%d&r=%d&db=%s&o=%d\" ", 
		tg->mapName, ro.repName, chromName, winStart, winEnd, database, 
		ro.genoStart);
	    printf("ALT= \"%s\" TITLE=\"%s\">\n", statusLine, statusLine); 
#endif /* OLDCRUFT */
	    }
	}
    freeHash(&hash);
    }
else
    {
    /* Do black and white on single track. */
    sprintf(query, "select genoStart,genoEnd from %s_rmsk where genoStart<%u and genoEnd>%u",
	chromName, winEnd, winStart);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	int start = sqlUnsigned(row[0]);
	int end = sqlUnsigned(row[1]);
	x1 = roundingScale(start-winStart, width, baseWidth)+xOff;
	x2 = roundingScale(end-winStart, width, baseWidth)+xOff;
	w = x2-x1;
	if (w <= 0)
	    w = 1;
	mgDrawBox(mg, x1, yOff, w, heightPer, MG_BLACK);
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

struct trackGroup *repeatTg()
/* Make track group for repeats. */
{
struct trackGroup *tg;

AllocVar(tg);
tg->mapName = "hgRepeat";
tg->visibility = tvDense;
tg->longLabel = "Repeating Elements by RepeatMasker";
tg->shortLabel = "RepeatMasker";
tg->loadItems = repeatLoad;
tg->freeItems = repeatFree;
tg->drawItems = repeatDraw;
tg->colorShades = shadesOfGray;
tg->itemName = repeatName;
tg->mapItemName = repeatName;
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = tgWeirdItemStart;
tg->itemEnd = tgWeirdItemEnd;
return tg;
}

typedef struct slList *(*ItemLoader)(char **row);

void bedLoadQuery(struct trackGroup *tg, char *query, ItemLoader loader)
/* Load up bed items from a query. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct slList *itemList = NULL, *item;

/* Get the frags and load into tg->items. */
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    item = loader(row);
    slAddHead(&itemList, item);
    }
slReverse(&itemList);
sqlFreeResult(&sr);
tg->items = itemList;
hFreeConn(&conn);
}

void bedLoadChrom(struct trackGroup *tg, char *table, ItemLoader loader)
/* Generic tg->item loader from chromosome table. */
{
char query[256];
sprintf(query, "select * from %s_%s where chromStart<%u and chromEnd>%u",
    chromName, table, winEnd, winStart);
bedLoadQuery(tg, query, loader);
}

void bedLoadItem(struct trackGroup *tg, char *table, ItemLoader loader)
/* Generic tg->item loader. */
{
char query[256];
sprintf(query, "select * from %s where chrom = '%s' and chromStart<%u and chromEnd>%u",
    table, chromName, winEnd, winStart);
bedLoadQuery(tg, query, loader);
}

static void bedDrawSimple(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw simple Bed items. */
{
int baseWidth = seqEnd - seqStart;
struct bed *item;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2,w;
boolean isFull = (vis == tvFull);
double scale = width/(double)baseWidth;
int invPpt;
Color *shades = tg->colorShades;

for (item = tg->items; item != NULL; item = item->next)
    {
    x1 = round((double)((int)item->chromStart-winStart)*scale) + xOff;
    x2 = round((double)((int)item->chromEnd-winStart)*scale) + xOff;
    w = x2-x1;
    if (tg->itemColor != NULL)
        color = tg->itemColor(tg, item, mg);
    else
	{
	if (shades)
	    color = shades[grayInRange(item->score, 0, 1000)];
	}
    if (w < 1)
	w = 1;
    if (color)
	{
	mgDrawBox(mg, x1, y, w, heightPer, color);
	if (tg->drawName)
	    {
	    /* Clip here so that text will tend to be more visible... */
	    char *s = tg->itemName(tg, item);
	    if (x1 < xOff)
		x1 = xOff;
	    if (x2 > xOff + width)
		x2 = xOff + width;
	    w = x2-x1;
	    if (w > mgFontStringWidth(font, s))
		mgTextCentered(mg, x1, y, w, heightPer, MG_WHITE, font, s);
	    }
	}
    if (isFull)
	y += lineHeight;
    }
}

char *bedName(struct trackGroup *tg, void *item)
/* Return name of bed track item. */
{
struct bed *bed = item;
return bed->name;
}


int bedItemStart(struct trackGroup *tg, void *item)
/* Return start position of item. */
{
struct bed *bed = item;
return bed->chromStart;
}

int bedItemEnd(struct trackGroup *tg, void *item)
/* Return end position of item. */
{
struct bed *bed = item;
return bed->chromEnd;
}


struct trackGroup *bedTg()
/* Get track group loaded with generic bed values. */
{
struct trackGroup *tg;
AllocVar(tg);
tg->visibility = tvDense;
tg->drawItems = bedDrawSimple;
tg->itemName = bedName;
tg->mapItemName = bedName;
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = bedItemStart;
tg->itemEnd = bedItemEnd;
return tg;
}


void isochoreLoad(struct trackGroup *tg)
/* Load up isochores from database table to trackGroup items. */
{
bedLoadItem(tg, "isochores", (ItemLoader)isochoresLoad);
}

void isochoreFree(struct trackGroup *tg)
/* Free up isochore items. */
{
isochoresFreeList((struct isochores**)&tg->items);
}

char *isochoreName(struct trackGroup *tg, void *item)
/* Return name of gold track item. */
{
struct isochores *iso = item;
static char buf[64];
sprintf(buf, "%3.1f%% GC", 0.1*iso->gcPpt);
return buf;
}

static void isochoreDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw isochore items. */
{
int baseWidth = seqEnd - seqStart;
struct isochores *item;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2,w;
boolean isFull = (vis == tvFull);
double scale = width/(double)baseWidth;
int invPpt;

for (item = tg->items; item != NULL; item = item->next)
    {
    x1 = round((double)((int)item->chromStart-winStart)*scale) + xOff;
    x2 = round((double)((int)item->chromEnd-winStart)*scale) + xOff;
    w = x2-x1;
    color = shadesOfGray[grayInRange(item->gcPpt, 340, 617)];
    if (w < 1)
	w = 1;
    mgDrawBox(mg, x1, y, w, heightPer, color);
    mapBoxHc(item->chromStart, item->chromEnd, x1, y, w, heightPer, tg->mapName,
	item->name, item->name);
#ifdef OLDCRUFT
    printf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x1, y, x1+w, y+heightPer);
    printf("HREF=\"../cgi-bin/hgc?g=%s&i=%s&c=%s&l=%d&r=%d&db=%s&o=%d\" ", 
	tg->mapName, item->name, chromName, winStart, winEnd, database,
	item->chromStart);
    printf("ALT= \"%s\" TITLE=\"%s\">\n", item->name, item->name); 
#endif /* OLDCRUFT */
    if (isFull)
	y += lineHeight;
    }
}



struct trackGroup *isochoresTg()
/* Make track group for isochores. */
{
struct trackGroup *tg = bedTg();

tg->mapName = "hgIsochore";
tg->visibility = tvHide;
tg->longLabel = "GC-rich (dark) and AT-rich (light) isochores";
tg->shortLabel = "isochores";
tg->loadItems = isochoreLoad;
tg->freeItems = isochoreFree;
tg->drawItems = isochoreDraw;
tg->colorShades = shadesOfGray;
tg->itemName = isochoreName;
return tg;
}

char *simpleRepeatName(struct trackGroup *tg, void *item)
/* Return name of simpleRepeats track item. */
{
struct simpleRepeat *rep = item;
static char buf[17];
int len;

if (rep->sequence == NULL)
    return rep->name;
len = strlen(rep->sequence);
if (len <= 16)
    return rep->sequence;
else
    {
    memcpy(buf, rep->sequence, 13);
    memcpy(buf+13, "...", 3);
    buf[16] = 0;
    return buf;
    }
}

void loadSimpleRepeats(struct trackGroup *tg)
/* Load up simpleRepeats from database table to trackGroup items. */
{
bedLoadItem(tg, "simpleRepeat", (ItemLoader)simpleRepeatLoad);
}

void freeSimpleRepeats(struct trackGroup *tg)
/* Free up isochore items. */
{
simpleRepeatFreeList((struct simpleRepeat**)&tg->items);
}

struct trackGroup *simpleRepeatTg()
/* Make track group for simple repeats. */
{
struct trackGroup *tg = bedTg();

tg->mapName = "hgSimpleRepeat";
tg->visibility = tvHide;
tg->longLabel = "Simple Tandem Repeats";
tg->shortLabel = "Simple Repeats";
tg->loadItems = loadSimpleRepeats;
tg->freeItems = freeSimpleRepeats;
tg->itemName = simpleRepeatName;
return tg;
}

void loadCpgIsland(struct trackGroup *tg)
/* Load up simpleRepeats from database table to trackGroup items. */
{
bedLoadItem(tg, "cpgIsland", (ItemLoader)cpgIslandLoad);
}

void freeCpgIsland(struct trackGroup *tg)
/* Free up isochore items. */
{
cpgIslandFreeList((struct cpgIsland**)&tg->items);
}

Color cpgIslandColor(struct trackGroup *tg, void *item, struct memGfx *mg)
/* Return color of cpgIsland track item. */
{
struct cpgIsland *el = item;

return (el->length < 400 ? tg->ixAltColor : tg->ixColor);
}

struct trackGroup *cpgIslandTg()
/* Make track group for simple repeats. */
{
struct trackGroup *tg = bedTg();

tg->mapName = "hgCpgIsland";
tg->visibility = tvHide;
tg->longLabel = "CpG Islands. (Islands < 400 Bases Are Light Green)";
tg->shortLabel = "CpG Islands";
tg->loadItems = loadCpgIsland;
tg->freeItems = freeCpgIsland;
tg->color.r = 0;
tg->color.g = 100;
tg->color.b = 0;
tg->altColor.r = 128;
tg->altColor.g = 228;
tg->altColor.b = 128;
tg->itemColor = cpgIslandColor;
return tg;
}


void loadCpgIsland2(struct trackGroup *tg)
/* Load up simpleRepeats from database table to trackGroup items. */
{
bedLoadItem(tg, "cpgIsland2", (ItemLoader)cpgIslandLoad);
}

struct trackGroup *cpgIsland2Tg()
/* Make track group for simple repeats. */
{
struct trackGroup *tg = bedTg();

tg->mapName = "hgCpgIsland2";
tg->visibility = tvHide;
tg->longLabel = "LaDeana Hillier's CpG Islands (dark if >= 400 bases long)";
tg->shortLabel = "CpG Hillier";
tg->loadItems = loadCpgIsland2;
tg->freeItems = freeCpgIsland;
tg->color.r = 0;
tg->color.g = 100;
tg->color.b = 0;
tg->altColor.r = 128;
tg->altColor.g = 228;
tg->altColor.b = 128;
tg->itemColor = cpgIslandColor;
return tg;
}

char *cytoBandName(struct trackGroup *tg, void *item)
/* Return name of cytoBand track item. */
{
struct cytoBand *band = item;
static char buf[32];
int len;

sprintf(buf, "%s%s", skipChr(band->chrom), band->name);
return buf;
}

char *abbreviatedBandName(struct trackGroup *tg, struct cytoBand *band, MgFont *font, int width)
/* Return a string abbreviated enough to fit into space. */
{
int textWidth;
static char string[32];

/* If have enough space, return original chromosome/band. */
sprintf(string, "%s%s", skipChr(band->chrom), band->name);
textWidth = mgFontStringWidth(font, string);
if (textWidth <= width)
    return string;

/* Try leaving off chromosome  */
sprintf(string, "%s", band->name);
textWidth = mgFontStringWidth(font, string);
if (textWidth <= width)
    return string;

/* Try leaving off initial letter */
sprintf(string, "%s", band->name+1);
textWidth = mgFontStringWidth(font, string);
if (textWidth <= width)
    return string;

return NULL;
}

void cytoBandColor(struct cytoBand *band, Color cenColor, Color *retBoxColor, Color *retTextColor)
/* Figure out color of band. */
{
char *stain = band->gieStain;
if (startsWith("gneg", stain))
    {
    *retBoxColor = shadesOfGray[1];
    *retTextColor = MG_BLACK;
    }
else if (startsWith("gpos", stain))
    {
    int percentage = 100;
    stain += 4;	
    if (isdigit(stain[0]))
        percentage = atoi(stain);
    *retBoxColor = shadesOfGray[grayInRange(percentage, -30, 100)];
    *retTextColor = MG_WHITE;
    }
else if (startsWith("gvar", stain))
    {
    *retBoxColor = shadesOfGray[2];
    *retTextColor = MG_WHITE;
    }
else 
    {
    *retBoxColor = cenColor;
    *retTextColor = MG_WHITE;
    }
}

static void cytoBandDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw cytoBand items. */
{
int baseWidth = seqEnd - seqStart;
struct cytoBand *band;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2,w;
int midLineOff = heightPer/2;
boolean isFull = (vis == tvFull);
Color col, textCol;
int ix = 0;
char *s;
double scale = width/(double)baseWidth;
for (band = tg->items; band != NULL; band = band->next)
    {
    x1 = round((double)((int)band->chromStart-winStart)*scale) + xOff;
    x2 = round((double)((int)band->chromEnd-winStart)*scale) + xOff;
    /* Clip here so that text will tend to be more visible... */
    if (x1 < xOff)
	x1 = xOff;
    if (x2 > xOff + width)
	x2 = xOff + width;
    w = x2-x1;
    if (w < 1)
	w = 1;
    cytoBandColor(band, tg->ixAltColor, &col, &textCol);
    mgDrawBox(mg, x1, y, w, heightPer, col);
    s = abbreviatedBandName(tg, band, tl.font, w);
    if (s != NULL)
	mgTextCentered(mg, x1, y, w, heightPer, textCol, tl.font, s);
    mapBoxHc(band->chromStart, band->chromEnd, x1,y,w,heightPer, tg->mapName, 
	band->name, band->name);
    if (isFull)
	y += lineHeight;
    ++ix;
    }
}


void loadCytoBands(struct trackGroup *tg)
/* Load up simpleRepeats from database table to trackGroup items. */
{
bedLoadItem(tg, "cytoBand", (ItemLoader)cytoBandLoad);
}

void freeCytoBands(struct trackGroup *tg)
/* Free up isochore items. */
{
cytoBandFreeList((struct cytoBand**)&tg->items);
}

struct trackGroup *cytoBandTg()
/* Make track group for simple repeats. */
{
struct trackGroup *tg = bedTg();

tg->mapName = "hgCytoBands";
tg->visibility = tvDense;
tg->longLabel = "Chromosome Bands Localized by FISH Mapping Clones";
tg->shortLabel = "Chromosome Band";
tg->loadItems = loadCytoBands;
tg->freeItems = freeCytoBands;
tg->drawItems = cytoBandDraw;
tg->itemName = cytoBandName;
tg->altColor.r = 150;
tg->altColor.g = 50;
tg->altColor.b = 50;
return tg;
}

void loadGcPercent(struct trackGroup *tg)
/* Load up simpleRepeats from database table to trackGroup items. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct gcPercent *itemList = NULL, *item;
char query[256];

sprintf(query, "select * from %s where chrom = '%s' and chromStart<%u and chromEnd>%u",
    "gcPercent", chromName, winEnd, winStart);

/* Get the frags and load into tg->items. */
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    item = gcPercentLoad(row);
    if (item->gcPpt != 0)
	{
	slAddHead(&itemList, item);
	}
    else
        gcPercentFree(&item);
    }
slReverse(&itemList);
sqlFreeResult(&sr);
tg->items = itemList;
hFreeConn(&conn);
}

void freeGcPercent(struct trackGroup *tg)
/* Free up isochore items. */
{
gcPercentFreeList((struct gcPercent**)&tg->items);
}

char *gcPercentName(struct trackGroup *tg, void *item)
/* Return name of gcPercent track item. */
{
struct gcPercent *gc = item;
static char buf[32];

sprintf(buf, "%3.1f%% GC", 0.1*gc->gcPpt);
return buf;
}

static int gcPercentMin = 320;
static int gcPercentMax = 600;

Color gcPercentColor(struct trackGroup *tg, void *item, struct memGfx *mg)
/* Return name of gcPercent track item. */
{
struct gcPercent *gc = item;
int ppt = gc->gcPpt;
int grayLevel;

grayLevel = grayInRange(ppt, gcPercentMin, gcPercentMax);
return shadesOfGray[grayLevel];
}

static void gcPercentDenseDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw gcPercent items. */
{
int baseWidth = seqEnd - seqStart;
UBYTE *useCounts;
UBYTE *aveCounts;
int i;
int lineHeight = mgFontLineHeight(font);
struct gcPercent *gc;
int s, e, w;
int log2 = digitsBaseTwo(baseWidth);
int shiftFactor = log2 - 17;
int sampleWidth;

if (shiftFactor < 0)
    shiftFactor = 0;
sampleWidth = (baseWidth>>shiftFactor);
AllocArray(useCounts, sampleWidth);
AllocArray(aveCounts, width);
memset(useCounts, 0, sampleWidth * sizeof(useCounts[0]));
for (gc = tg->items; gc != NULL; gc = gc->next)
    {
    s = ((gc->chromStart - seqStart)>>shiftFactor);
    e = ((gc->chromStart - seqStart)>>shiftFactor);
    if (s < 0) s = 0;
    if (e > sampleWidth) e = sampleWidth;
    w = e - s;
    if (w > 0)
	memset(useCounts+s, grayInRange(gc->gcPpt, gcPercentMin, gcPercentMax), w);
    }
resampleBytes(useCounts, sampleWidth, aveCounts, width);
grayThreshold(aveCounts, width);
for (i=0; i<lineHeight; ++i)
    mgPutSegZeroClear(mg, xOff, yOff+i, width, aveCounts);
freeMem(useCounts);
freeMem(aveCounts);
}

static void gcPercentDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw gcPercent items. */
{
if (vis == tvDense)
   gcPercentDenseDraw(tg, seqStart, seqEnd, mg, xOff, yOff, width, font, color, vis);
else
   bedDrawSimple(tg, seqStart, seqEnd, mg, xOff, yOff, width, font, color, vis);
}


struct trackGroup *gcPercentTg()
/* Make track group for simple repeats. */
{
struct trackGroup *tg = bedTg();

tg->mapName = "hgGcPercent";
tg->visibility = tvHide;
tg->longLabel = "Percentage GC in 20,000 Base Windows";
tg->shortLabel = "GC Percent";
tg->loadItems = loadGcPercent;
tg->freeItems = freeGcPercent;
tg->itemName = gcPercentName;
tg->colorShades = shadesOfGray;
tg->itemColor = gcPercentColor;
return tg;
}

struct genomicDups *filterOldDupes(struct genomicDups *oldList)
/* Get rid of all but recent/artifact dupes. */
{
struct genomicDups *newList = NULL, *dup, *next;
for (dup = oldList; dup != NULL; dup = next)
    {
    next = dup->next;
    if (dup->score > 980)
        {
	slAddHead(&newList, dup);
	}
    else
        {
	genomicDupsFree(&dup);
	}
    }
slReverse(&newList);
return newList;
}

void loadGenomicDups(struct trackGroup *tg)
/* Load up simpleRepeats from database table to trackGroup items. */
{
bedLoadItem(tg, "genomicDups", (ItemLoader)genomicDupsLoad);
if (!privateVersion())
    tg->items = filterOldDupes(tg->items);
}

void freeGenomicDups(struct trackGroup *tg)
/* Free up isochore items. */
{
genomicDupsFreeList((struct genomicDups**)&tg->items);
}

Color genomicDupsColor(struct trackGroup *tg, void *item, struct memGfx *mg)
/* Return name of gcPercent track item. */
{
struct genomicDups *dup = item;
int ppt = dup->score;
int grayLevel;

if (ppt > 990)
    return tg->ixColor;
else if (ppt > 980)
    return tg->ixAltColor;
grayLevel = grayInRange(ppt, 900, 1000);
return shadesOfGray[grayLevel];
}

char *genomicDupsName(struct trackGroup *tg, void *item)
/* Return full genie name. */
{
struct genomicDups *gd = item;
char *full = gd->name;
static char abbrev[64];
int len;
char *e;

strcpy(abbrev, skipChr(full));
abbr(abbrev, "om");
return abbrev;
}



struct trackGroup *genomicDupsTg()
/* Make track group for simple repeats. */
{
struct trackGroup *tg = bedTg();

tg->mapName = "hgGenomicDups";
tg->visibility = tvDense;
tg->longLabel = "Duplications of >1000 bases of non-RepeatMasked Sequence";
tg->shortLabel = "duplications";
tg->loadItems = loadGenomicDups;
tg->freeItems = freeGenomicDups;
tg->itemName = genomicDupsName;
tg->color.r = 230;
tg->color.g = 120;
tg->color.b = 0;
tg->altColor.r = 220;
tg->altColor.g = 180;
tg->altColor.b = 0;
tg->itemColor = genomicDupsColor;
return tg;
}

void loadGenethon(struct trackGroup *tg)
/* Load up simpleRepeats from database table to trackGroup items. */
{
bedLoadItem(tg, "mapGenethon", (ItemLoader)mapStsLoad);
}

void freeGenethon(struct trackGroup *tg)
/* Free up isochore items. */
{
mapStsFreeList((struct mapSts**)&tg->items);
}

struct trackGroup *genethonTg()
/* Make track group for simple repeats. */
{
struct trackGroup *tg = bedTg();

tg->mapName = "hgGenethon";
tg->visibility = tvDense;
tg->longLabel = "Various STS Markers";
tg->shortLabel = "STS Markers";
tg->loadItems = loadGenethon;
tg->freeItems = freeGenethon;
return tg;
}

void loadEst3(struct trackGroup *tg)
/* Load up simpleRepeats from database table to trackGroup items. */
{
bedLoadItem(tg, "est3", (ItemLoader)est3Load);
}

void freeEst3Items(struct trackGroup *tg)
/* Free up isochore items. */
{
est3FreeList((struct est3**)&tg->items);
}

char *est3Name(struct trackGroup *tg, void *item)
/* Return what to display on left column of open track. */
{
struct est3 *e3 = item;
static char name[64];

sprintf(name, "%d %s 3' ESTs", e3->estCount, e3->strand);
return name;
}

char *est3MapName(struct trackGroup *tg, void *item)
/* Return what to write in client-side image map. */
{
return "est3";
}


struct trackGroup *est3Tg()
/* Make track group for simple repeats. */
{
struct trackGroup *tg = bedTg();

tg->mapName = "hgEst3";
tg->visibility = tvDense;
tg->longLabel = "EST 3' Ends, Filtered and Clustered";
tg->shortLabel = "EST 3' Ends";
tg->loadItems = loadEst3;
tg->freeItems = freeEst3Items;
tg->itemName = est3Name;
tg->mapItemName = est3MapName;
return tg;
}

void loadExoFish(struct trackGroup *tg)
/* Load up exoFish from database table to trackGroup items. */
{
bedLoadItem(tg, "exoFish", (ItemLoader)exoFishLoad);
}

void freeExoFish(struct trackGroup *tg)
/* Free up isochore items. */
{
exoFishFreeList((struct exoFish**)&tg->items);
}

char *exoFishName(struct trackGroup *tg, void *item)
/* Return what to display on left column of open track. */
{
struct exoFish *exo = item;
static char name[64];

sprintf(name, "ecore score %d", exo->score);
return name;
}

Color exoFishColor(struct trackGroup *tg, void *item, struct memGfx *mg)
/* Return color of exofish track item. */
{
struct exoFish *el = item;
int ppt = el->score;
int grayLevel;

grayLevel = grayInRange(ppt, -500, 1000);
return shadesOfSea[grayLevel];
}


struct trackGroup *exoFishTg()
/* Make track group for exoFish. */
{
struct trackGroup *tg = bedTg();

tg->mapName = "hgExoFish";
tg->visibility = tvDense;
tg->longLabel = "Exofish Tetraodon/Human Evolutionarily Conserved Regions (ecores)";
tg->shortLabel = "Exofish ecores";
tg->loadItems = loadExoFish;
tg->freeItems = freeExoFish;
tg->itemName = exoFishName;
tg->colorShades = shadesOfGray;
tg->itemColor = exoFishColor;
tg->color.r = darkSeaColor.r;
tg->color.g = darkSeaColor.g;
tg->color.b = darkSeaColor.b;
return tg;
}

void loadExoMouse(struct trackGroup *tg)
/* Load up exoMouse from database table to trackGroup items. */
{
bedLoadItem(tg, "exoMouse", (ItemLoader)roughAliLoad);
if (tg->visibility == tvDense && slCount(tg->items) < 1000)
    {
    slSort(&tg->items, bedCmpScore);
    }
}

void freeExoMouse(struct trackGroup *tg)
/* Free up isochore items. */
{
roughAliFreeList((struct roughAli**)&tg->items);
}

char *exoMouseName(struct trackGroup *tg, void *item)
/* Return what to display on left column of open track. */
{
struct roughAli *exo = item;
static char name[17];

strncpy(name, exo->name, sizeof(name)-1);
return name;
}


Color exoMouseColor(struct trackGroup *tg, void *item, struct memGfx *mg)
/* Return color of exoMouse track item. */
{
struct roughAli *el = item;
int ppt = el->score;
int grayLevel;

grayLevel = grayInRange(ppt, -100, 1000);
return shadesOfBrown[grayLevel];
}


struct trackGroup *exoMouseTg()
/* Make track group for exoMouse. */
{
struct trackGroup *tg = bedTg();

tg->mapName = "hgExoMouse";
if (sameString(database, "hg6") && sameString(chromName, "chr22") && privateVersion())
    tg->visibility = tvDense;
else
    tg->visibility = tvHide;
tg->longLabel = "Mouse/Human Evolutionarily Conserved Regions (by Exonerate)";
tg->shortLabel = "Exonerate Mouse";
tg->loadItems = loadExoMouse;
tg->freeItems = freeExoMouse;
tg->itemName = exoMouseName;
tg->colorShades = shadesOfBrown;
tg->itemColor = exoMouseColor;
tg->color.r = brownColor.r;
tg->color.g = brownColor.g;
tg->color.b = brownColor.b;
return tg;
}


void loadSnp(struct trackGroup *tg)
/* Load up simpleRepeats from database table to trackGroup items. */
{
bedLoadItem(tg, tg->customPt, (ItemLoader)snpLoad);
}

void freeSnp(struct trackGroup *tg)
/* Free up isochore items. */
{
snpFreeList((struct snp**)&tg->items);
}

struct trackGroup *snpTg(char *table)
/* Make track group for snps. */
{
struct trackGroup *tg = bedTg();

tg->mapName = table;
tg->visibility = tvDense;
tg->longLabel = "SNP";
tg->shortLabel = "Single Nucleotide Polymorphisms (SNP)";
tg->loadItems = loadSnp;
tg->freeItems = freeSnp;
tg->customPt = table;
return tg;
}

struct trackGroup *snpNihTg()
/* Make track group for NIH. */
{
struct trackGroup *tg = snpTg("snpNih");
tg->shortLabel = "Overlap SNPs";
tg->longLabel = "Single Nucleotide Polymorphisms (SNPs) from Clone Overlaps";
return tg;
}

struct trackGroup *snpTscTg()
/* Make track group for TSC. */
{
struct trackGroup *tg = snpTg("snpTsc");
tg->shortLabel = "Random SNPs";
tg->longLabel = "Single Nucleotide Polymorphisms (SNPs) from Random Reads";
return tg;
}

void loadRnaGene(struct trackGroup *tg)
/* Load up rnaGene from database table to trackGroup items. */
{
bedLoadItem(tg, "rnaGene", (ItemLoader)rnaGeneLoad);
}

void freeRnaGene(struct trackGroup *tg)
/* Free up rnaGene items. */
{
rnaGeneFreeList((struct rnaGene**)&tg->items);
}

Color rnaGeneColor(struct trackGroup *tg, void *item, struct memGfx *mg)
/* Return color of rnaGene track item. */
{
struct rnaGene *el = item;

return (el->isPsuedo ? tg->ixAltColor : tg->ixColor);
}

char *rnaGeneName(struct trackGroup *tg, void *item)
/* Return RNA gene name. */
{
struct rnaGene *el = item;
char *full = el->name;
static char abbrev[64];
int len;
char *e;

strcpy(abbrev, skipChr(full));
subChar(abbrev, '_', ' ');
abbr(abbrev, " pseudogene");
if ((e = strstr(abbrev, "-related")) != NULL)
    strcpy(e, "-like");
return abbrev;
}

struct trackGroup *rnaGeneTg()
/* Make track group for simple repeats. */
{
struct trackGroup *tg = bedTg();

tg->mapName = "hgRnaGene";
tg->visibility = tvFull;
tg->longLabel = "Non-coding RNA Genes (dark) and Pseudogenes (light)";
tg->shortLabel = "RNA Genes";
tg->loadItems = loadRnaGene;
tg->freeItems = freeRnaGene;
tg->itemName = rnaGeneName;
tg->color.r = 170;
tg->color.g = 80;
tg->color.b = 0;
tg->altColor.r = 230;
tg->altColor.g = 180;
tg->altColor.b = 130;
tg->itemColor = rnaGeneColor;
return tg;
}

void loadStsMarker(struct trackGroup *tg)
/* Load up stsMarkers from database table to trackGroup items. */
{
bedLoadItem(tg, "stsMarker", (ItemLoader)stsMarkerLoad);
}

void freeStsMarker(struct trackGroup *tg)
/* Free up stsMarker items. */
{
stsMarkerFreeList((struct stsMarker**)&tg->items);
}

Color stsMarkerColor(struct trackGroup *tg, void *item, struct memGfx *mg)
/* Return color of stsMarker track item. */
{
struct stsMarker *el = item;
int ppt = el->score;

if (el->genethonChrom[0] != '0' || el->marshfieldChrom[0] != '0')
    {
    if (ppt >= 900)
       return MG_BLUE;
    else
       return tg->ixAltColor;
    }
else if (el->fishChrom[0] != '0')
    {
    return MG_GREEN;
    }
else
    {
    if (ppt >= 900)
       return MG_BLACK;
    else
       return MG_GRAY;
    }
}


struct trackGroup *stsMarkerTg()
/* Make track group for sts markers. */
{
struct trackGroup *tg = bedTg();

tg->mapName = "hgStsMarker";
tg->visibility = tvDense;
tg->longLabel = "STS Markers on Genetic (blue), FISH (green) and Radiation Hybrid (black) Maps";
tg->shortLabel = "STS Markers";
tg->loadItems = loadStsMarker;
tg->freeItems = freeStsMarker;
tg->itemColor = stsMarkerColor;
tg->color.r = 0;
tg->color.g = 0;
tg->color.b = 0;
tg->altColor.r = 128;
tg->altColor.g = 128;
tg->altColor.b = 255;
return tg;
}

void loadMouseSyn(struct trackGroup *tg)
/* Load up mouseSyn from database table to trackGroup items. */
{
bedLoadItem(tg, "mouseSyn", (ItemLoader)mouseSynLoad);
}

void freeMouseSyn(struct trackGroup *tg)
/* Free up mouseSyn items. */
{
mouseSynFreeList((struct mouseSyn**)&tg->items);
}

Color mouseSynItemColor(struct trackGroup *tg, void *item, struct memGfx *mg)
/* Return color of mouseSyn track item. */
{
struct mouseSyn *ms = item;
return (ms->segment&1) ? tg->ixColor : tg->ixAltColor;
}

struct trackGroup *mouseSynTg()
/* Make track group for mouseSyn. */
{
struct trackGroup *tg = bedTg();

tg->mapName = "hgMouseSyn";
tg->visibility = tvHide;
tg->longLabel = "Corresponding Chromosome in Mouse";
tg->shortLabel = "Mouse Synteny";
tg->loadItems = loadMouseSyn;
tg->freeItems = freeMouseSyn;
tg->color.r = 120;
tg->color.g = 70;
tg->color.b = 30;
tg->itemColor = mouseSynItemColor;
tg->drawName = TRUE;
return tg;
}

#ifdef EXAMPLE
void loadXyz(struct trackGroup *tg)
/* Load up xyz from database table to trackGroup items. */
{
bedLoadItem(tg, "xyz", (ItemLoader)xyzLoad);
}

void freeXyz(struct trackGroup *tg)
/* Free up xyz items. */
{
xyzFreeList((struct xyz**)&tg->items);
}

struct trackGroup *xyzTg()
/* Make track group for xyz. */
{
struct trackGroup *tg = bedTg();

tg->mapName = "hgXyz";
tg->visibility = tvDense;
tg->longLabel = "xyz";
tg->shortLabel = "xyz";
tg->loadItems = loadXyz;
tg->freeItems = freeXyz;
return tg;
}
#endif /* EXAMPLE */

struct wabaChromHit
/* Records where waba alignment hits chromosome. */
    {
    struct wabaChromHit *next;	/* Next in list. */
    char *query;	  	/* Query name. */
    int chromStart, chromEnd;   /* Chromosome position. */
    char strand;                /* + or - for strand. */
    int milliScore;             /* Parts per thousand */
    char *squeezedSym;          /* HMM Symbols */
    };

struct wabaChromHit *wchLoad(char *row[])
/* Create a wabaChromHit from database row. 
 * Since squeezedSym autoSql can't generate this,
 * alas. */
{
int size;
char *sym;
struct wabaChromHit *wch;

AllocVar(wch);
wch->query = cloneString(row[0]);
wch->chromStart = sqlUnsigned(row[1]);
wch->chromEnd = sqlUnsigned(row[2]);
wch->strand = row[3][0];
wch->milliScore = sqlUnsigned(row[4]);
size = wch->chromEnd - wch->chromStart;
wch->squeezedSym = sym = needLargeMem(size+1);
memcpy(sym, row[5], size);
sym[size] = 0;
return wch;
}

void wchFree(struct wabaChromHit **pWch)
/* Free a singlc wabaChromHit. */
{
struct wabaChromHit *wch = *pWch;
if (wch != NULL)
    {
    freeMem(wch->squeezedSym);
    freeMem(wch->query);
    freez(pWch);
    }
}

void wchFreeList(struct wabaChromHit **pList)
/* Free list of wabaChromHits. */
{
struct wabaChromHit *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    wchFree(&el);
    }
*pList = NULL;
}

void wabaLoad(struct trackGroup *tg)
/* Load up waba items intersecting window. */
{
char table[64];
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct agpFrag *fragList = NULL, *frag;
struct agpGap *gapList = NULL, *gap;
struct wabaChromHit *wch, *wchList = NULL;

/* Get the frags and load into tg->items. */
sprintf(table, "%s%s", chromName, (char *)tg->customPt);
sprintf(query, "select * from %s where chromStart<%u and chromEnd>%u",
    table, winEnd, winStart);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    wch = wchLoad(row);
    slAddHead(&wchList, wch);
    }
slReverse(&wchList);
tg->items = wchList;
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void wabaFree(struct trackGroup *tg)
/* Free up wabaTrackGroup items. */
{
wchFreeList((struct wabaChromHit**)&tg->items);
}


void makeSymColors(struct trackGroup *tg, enum trackVisibility vis,
	Color symColor[128])
/* Fill in array with color for each symbol value. */
{
memset(symColor, MG_WHITE, 128);
symColor['1'] = symColor['2'] = symColor['3'] = symColor['H'] 
   = tg->ixColor;
symColor['L'] = tg->ixAltColor;
}

int countSameColor(char *sym, int symCount, Color symColor[])
/* Count how many symbols are the same color as the current one. */
{
Color color = symColor[sym[0]];
int ix;
for (ix = 1; ix < symCount; ++ix)
    {
    if (symColor[sym[ix]] != color)
        break;
    }
return ix;
}

#ifdef OLD
void wabaItemMap(struct trackGroup *tg, void *item, int x, int y, int width, int height)
/* Print image map line on one waba item. */
{
struct wabaChromHit *wch = item;
char *id = tg->mapName;
char *shortName = cgiEncode(tg->mapItemName(tg, item));
char *statusLine = cgiEncode(tg->itemName(tg, item));

printf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
printf("HREF=\"../cgi-bin/hgc?g=%s&i=%s&c=%s&l=%d&r=%d&db=%s&o=%d\" ", 
    id, shortName, chromName, winStart, winEnd, database, wch->chromStart);
printf("ALT= \"%s\" TITLE=\"%s\">\n", statusLine, statusLine); 
freeMem(shortName);
freeMem(statusLine);
}
#endif /* OLD */


static void wabaDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw waba alignment items. */
{
Color symColor[128];
int baseWidth = seqEnd - seqStart;
struct wabaChromHit *wch;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2,w;
boolean isFull = (vis == tvFull);
int ix = 0;


makeSymColors(tg, vis, symColor);
for (wch = tg->items; wch != NULL; wch = wch->next)
    {
    int chromStart = wch->chromStart;
    int symCount = wch->chromEnd - chromStart;
    int typeStart, typeWidth, typeEnd;
    char *sym = wch->squeezedSym;
    for (typeStart = 0; typeStart < symCount; typeStart = typeEnd)
	{
	typeWidth = countSameColor(sym+typeStart, symCount-typeStart, symColor);
	typeEnd = typeStart + typeWidth;
	color = symColor[sym[typeStart]];
	if (color != MG_WHITE)
	    {
	    x1 = roundingScale(typeStart+chromStart-winStart, width, baseWidth)+xOff;
	    x2 = roundingScale(typeEnd+chromStart-winStart, width, baseWidth)+xOff;
	    w = x2-x1;
	    if (w < 1)
		w = 1;
	    mgDrawBox(mg, x1, y, w, heightPer, color);
	    }
	}
    if (isFull)
	y += lineHeight;
    }
}

char *wabaName(struct trackGroup *tg, void *item)
/* Return name of waba track item. */
{
struct wabaChromHit *wch = item;
return wch->query;
}

int wabaItemStart(struct trackGroup *tg, void *item)
/* Return starting position of waba item. */
{
struct wabaChromHit *wch = item;
return wch->chromStart;
}

int wabaItemEnd(struct trackGroup *tg, void *item)
/* Return ending position of waba item. */
{
struct wabaChromHit *wch = item;
return wch->chromEnd;
}

struct trackGroup *wabaTrackGroup()
/* Return track with fields shared by waba-based 
 * alignment tracks filled in. */
{
struct trackGroup *tg;
AllocVar(tg);
tg->loadItems = wabaLoad;
tg->freeItems = wabaFree;
tg->drawItems = wabaDraw;
tg->itemName = wabaName;
tg->mapItemName = wabaName;
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = wabaItemStart;
tg->itemEnd = wabaItemEnd;
return tg;
}

struct trackGroup *tetTg()
/* Make track group for Tetraodon alignments. */
{
struct trackGroup *tg;

tg = wabaTrackGroup();
tg->mapName = "hgTet";
tg->visibility = tvDense;
tg->longLabel = "Tetraodon nigroviridis Homologies";
tg->shortLabel = "Tetraodon";
tg->color.r = 50;
tg->color.g = 100;
tg->color.b = 200;
tg->altColor.r = 85;
tg->altColor.g = 170;
tg->altColor.b = 225;
tg->customPt = "_tet_waba";
return tg;
}

void contigLoad(struct trackGroup *tg)
/* Load up contigs from database table to trackGroup items. */
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct ctgPos *ctgList = NULL, *ctg;

/* Get the contigs and load into tg->items. */
sprintf(query, "select * from ctgPos where chrom = '%s' and chromStart<%u and chromEnd>%u",
    chromName, winEnd, winStart);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ctg = ctgPosLoad(row);
    slAddHead(&ctgList, ctg);
    }
slReverse(&ctgList);
sqlFreeResult(&sr);
hFreeConn(&conn);
tg->items = ctgList;
}

char *abbreviateContig(char *string, MgFont *font, int width)
/* Return a string abbreviated enough to fit into space. */
{
int textWidth;

/* If have enough space, return original unabbreviated string. */
textWidth = mgFontStringWidth(font, string);
if (textWidth <= width)
    return string;

/* Try skipping over 'ctg' */
string += 3;
textWidth = mgFontStringWidth(font, string);
if (textWidth <= width)
    return string;
return NULL;
}

static void contigDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw contig items. */
{
int baseWidth = seqEnd - seqStart;
struct ctgPos *ctg;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2,w;
int midLineOff = heightPer/2;
boolean isFull = (vis == tvFull);
Color col;
int ix = 0;
char *s;
double scale = width/(double)baseWidth;
for (ctg = tg->items; ctg != NULL; ctg = ctg->next)
    {
    x1 = round((double)((int)ctg->chromStart-winStart)*scale) + xOff;
    x2 = round((double)((int)ctg->chromEnd-winStart)*scale) + xOff;
    /* Clip here so that text will tend to be more visible... */
    if (x1 < xOff)
	x1 = xOff;
    if (x2 > xOff + width)
	x2 = xOff + width;
    w = x2-x1;
    if (w < 1)
	w = 1;
    mgDrawBox(mg, x1, y, w, heightPer, color);
    s = abbreviateContig(ctg->contig, tl.font, w);
    if (s != NULL)
	mgTextCentered(mg, x1, y, w, heightPer, MG_WHITE, tl.font, s);
    if (isFull)
	y += lineHeight;
    else 
	{
	mapBoxHc(ctg->chromStart, ctg->chromEnd, x1,y,w,heightPer, tg->mapName, 
	    tg->mapItemName(tg, ctg), 
	    tg->itemName(tg, ctg));
	}
    ++ix;
    }
}


void contigFree(struct trackGroup *tg)
/* Free up contigTrackGroup items. */
{
ctgPosFreeList((struct ctgPos**)&tg->items);
}


char *contigName(struct trackGroup *tg, void *item)
/* Return name of contig track item. */
{
struct ctgPos *ctg = item;
return ctg->contig;
}

int contigItemStart(struct trackGroup *tg, void *item)
/* Return start of contig track item. */
{
struct ctgPos *ctg = item;
return ctg->chromStart;
}

int contigItemEnd(struct trackGroup *tg, void *item)
/* Return end of contig track item. */
{
struct ctgPos *ctg = item;
return ctg->chromEnd;
}

struct trackGroup *contigTg()
/* Make track group for contig */
{
struct trackGroup *tg;

AllocVar(tg);
tg->mapName = "hgContig";
tg->visibility = tvHide;
tg->longLabel = "Fingerprint Map Contigs";
tg->shortLabel = "FPC Contigs";
tg->loadItems = contigLoad;
tg->freeItems = contigFree;
tg->drawItems = contigDraw;
tg->color.r = 150;
tg->color.g = 0;
tg->color.b = 0;
tg->itemName = contigName;
tg->mapItemName = contigName;
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = contigItemStart;
tg->itemEnd = contigItemEnd;
return tg;
}

struct cloneFrag
/* A fragment of a clone. */
    {
    struct cloneFrag *next;	/* Next in list. */
    char *name;			/* Name of fragment.  Not allocated here. */
    };

struct cloneFragPos
/* An alignment involving a clone frag. */
    {
    struct cloneFragPos *next;	/* Next in list. */
    struct cloneFrag *frag;     /* Fragment info. */
    struct psl *psl;            /* Alignment info. Memory owned here. Possibly NULL. */
    struct gl *gl;              /* Golden path position info. */
    int start, end;             /* Start end in chromosome. */
    };

struct cloneInfo
/* Info about a clone and where it aligns. */
    {
    struct cloneInfo *next; 	/* Next in list */
    char *name;                 /* Name of clone. (Not allocated here) */
    short phase;			/* Htg phase - 1 2 or 3. */
    char stage;                 /* Stage - (P)redraft, (D)raft, (F)inished. */
    struct cloneFrag *fragList; /* List of fragments. */
    int fragCount;              /* Count of fragments. */
    struct cloneFragPos *cfaList;   /* List of alignments. */
    struct spaceSaver *ss;      /* How this is layed out. */
    int cloneStart, cloneEnd;       /* Min/Max position of alignments. */
    };

int cmpCloneInfo(const void *va, const void *vb)
/* Compare two cloneInfos by cloneStart. */
{
const struct cloneInfo *a = *((struct cloneInfo **)va);
const struct cloneInfo *b = *((struct cloneInfo **)vb);
return a->cloneStart - b->cloneStart;
}


void cloneInfoFree(struct cloneInfo **pCi)
/* free up a clone info. */
{
struct cloneInfo *ci;
if ((ci = *pCi) != NULL)
    {
    struct cloneFragPos *cfa;
    for (cfa = ci->cfaList; cfa != NULL; cfa = cfa->next)
	{
	pslFree(&cfa->psl);
	glFree(&cfa->gl);
	}
    slFreeList(&ci->cfaList);
    slFreeList(&ci->fragList);
    freez(pCi);
    }
}

void cloneInfoFreeList(struct cloneInfo **pList)
/* Free up a list of cloneInfo's. */
{
struct cloneInfo *el,*next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    cloneInfoFree(&el);
    }
*pList = NULL;
}


struct cloneFrag *findCloneFrag(struct cloneInfo *ci, char *fragName)
/* Search for named frag and return it, or NULL if not found. */
{
struct cloneFrag *frag;
for (frag = ci->fragList; frag != NULL; frag = frag->next)
    {
    if (sameString(frag->name, fragName))
	return frag;
    }
return NULL;
}

void layoutCloneAli(struct cloneInfo *ci)
/* Figure out space saver layout for alignments in clone. */
{
struct spaceSaver *ss;
struct cloneFragPos *cfa;
int start = 0x3fffffff;
int end = 0;

ss = ci->ss = spaceSaverNew(winStart, winEnd);
for (cfa = ci->cfaList; cfa != NULL; cfa = cfa->next)
    {
    spaceSaverAdd(ss, cfa->start, cfa->end, cfa);
    }
spaceSaverFinish(ss);
}

char *cloneName(struct trackGroup *tg, void *item)
/* Return name of gold track item. */
{
struct cloneInfo *ci = item;
return ci->name;
}

int cloneFragMaxWin = 1500000;

int oneOrRowCount(struct cloneInfo *ci)
/* Return row count, but at least one. */
{
int rowCount = ci->ss->rowCount;
if (rowCount < 1) rowCount = 1;
return rowCount;
}

static int cloneItemHeight(struct trackGroup *tg, void *item)
/* Return item height for fixed height track. */
{
struct cloneInfo *ci = item;
int height1 = mgFontLineHeight(tl.font)+1;
if (winBaseCount <= cloneFragMaxWin)
    return height1*oneOrRowCount(ci)+2;
else
    return height1;
}

static int cloneItemStart(struct trackGroup *tg, void *item)
/* Return start of item on clone track. */
{
struct cloneInfo *ci = item;
return ci->cloneStart;
}

static int cloneItemEnd(struct trackGroup *tg, void *item)
/* Return end of item on clone track. */
{
struct cloneInfo *ci = item;
return ci->cloneEnd;
}



static int cloneTotalHeight(struct trackGroup *tg, enum trackVisibility vis)
/* Height of a clone track. */
{
switch (vis)
    {
    case tvFull:
	{
	int total = 0;
	struct cloneInfo *ci;
	for (ci = tg->items; ci != NULL; ci = ci->next)
	    {
	    total += tg->itemHeight(tg, ci);
	    }
	tg->height = total+2;
	break;
	}
    case tvDense:
	tg->lineHeight = mgFontLineHeight(tl.font)+1;
	tg->heightPer = tg->lineHeight - 1;
	tg->height = tg->lineHeight;
	break;
    }
return tg->height;
}

static void drawOneClone(struct cloneInfo *ci, int seqStart, int seqEnd,
    struct memGfx *mg, int xOff, int yOff, int width,
    MgFont *font, int lineHeight, Color color, boolean stagger, 
    boolean hiliteDupes)
/* Draw a single clone item - using space saver layout on fragments. */
{
struct cloneFragPos *cfa;
struct psl *psl;
int y;
int heightPer = lineHeight-1;
struct spaceSaver *ss = ci->ss;
int baseWidth = seqEnd - seqStart;
struct spaceNode *sn;
int x1, x2, w;
char *s;
int textWidth;
char fullPos[256];
struct hash *dupeHash = NULL;
Color col;
struct hashEl *hel;

if (hiliteDupes)
    {
    struct hash *uniqHash = newHash(7);
    dupeHash = newHash(6);
    for (sn = ss->nodeList; sn != NULL; sn = sn->next)
	{
	cfa = sn->val;
	if (cfa->end - cfa->start > 1000)
	    {
	    s = strchr(cfa->frag->name, '_');
	    if (s != NULL)
		{
		s += 1;
		if (hashLookup(uniqHash, s) == NULL)
		    {
		    hashAdd(uniqHash, s, NULL);
		    }
		else	/* Got a dupe! */
		    {
		    if ((hel = hashLookup(dupeHash, s)) == NULL)
			hashAdd(dupeHash, s, sn);
		    }
		}
	    }
	}
    freeHash(&uniqHash);
    }

for (sn = ss->nodeList; sn != NULL; sn = sn->next)
    {
    if (stagger)
	y = yOff + sn->row*lineHeight;
    else
	y = yOff;
    cfa = sn->val;
    x1 = roundingScale(cfa->start-winStart, width, baseWidth)+xOff;
    x2 = roundingScale(cfa->end-winStart, width, baseWidth)+xOff;

    /* Clip here so that text will tend to be more visible... */
    if (x1 < xOff)
	x1 = xOff;
    if (x2 > xOff + width)
	x2 = xOff + width;
    w = x2-x1;
    if (w < 1)
	w = 1;
    s = strchr(cfa->frag->name, '_');
    if (s == NULL)
	s = "";
    else
	s += 1;
    col = color;
    if (hiliteDupes)
	{
	if ((hel = hashLookup(dupeHash, s)) != NULL)
	    {
	    if (hel->val == sn)
		col = MG_RED;
	    else
		col = MG_BLUE;
	    }
	}
    mgDrawBox(mg, x1, y, w, heightPer, col);
    textWidth = mgFontStringWidth(font, s);
    if (textWidth <= w)
	mgTextCentered(mg, x1, y, w, heightPer, MG_WHITE, font, s);
    if (baseWidth <= 2000000)
	{
	psl = cfa->psl;
	if (psl != NULL)
	    {
	    sprintf(fullPos, "%s %d to %d of %d, strand %s, hits %d to %d", 
	    	psl->qName, psl->qStart, 
		psl->qEnd, psl->qSize, psl->strand,
		psl->tStart, psl->tEnd);
	    mapBoxHc(cfa->start, cfa->end, x1,y,w,heightPer, "hgClone", cfa->frag->name, fullPos);
	    }
	else
	    mapBoxHc(cfa->start, cfa->end, x1,y,w,heightPer, "hgClone", cfa->frag->name, cfa->frag->name);
	}
    }
freeHash(&dupeHash);
}

/* These tables are for combining sequence scores. 
 *    0 = no coverage
 *    1 = predraft
 *    2 = draft
 *    3 = deep draft
 *    4 = finished  */
static UBYTE predraftInc[5] = {1, 1, 2, 3, 4};  
static UBYTE draftInc[5] = {2, 2, 3, 3, 4};
static UBYTE finishedInc[5] = {4, 4, 4, 4, 4};

void incStage(UBYTE *b, int width, char stage)
/* Increment b from 0 to width-1 according to stage. */
{
UBYTE *inc = NULL;
int i;

if (stage == 'P')
    inc = predraftInc;
else if (stage == 'D')
    inc = draftInc;
else if (stage == 'F')
    inc = finishedInc;
else
    errAbort("Unknown stage %c", stage);
for (i=0; i<width; ++i)
   b[i] = inc[b[i]];
}

static void cloneDenseDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense clone items. */
{
int baseWidth = seqEnd - seqStart;
UBYTE *useCounts;
UBYTE *aveCounts;
int i;
int lineHeight = mgFontLineHeight(font);
struct cloneInfo *ci;
struct cloneFragPos *cfa;   /* List of alignments. */
int s, e, w;
int log2 = digitsBaseTwo(baseWidth);
int shiftFactor = log2 - 17;
int sampleWidth;

if (shiftFactor < 0)
    shiftFactor = 0;
sampleWidth = (baseWidth>>shiftFactor);
AllocArray(useCounts, sampleWidth);
AllocArray(aveCounts, width);
memset(useCounts, 0, sampleWidth * sizeof(useCounts[0]));
for (ci = tg->items; ci != NULL; ci = ci->next)
    {
    char stage = ci->stage;
    for (cfa = ci->cfaList; cfa != NULL; cfa = cfa->next)
	{
	s = ((cfa->start - seqStart)>>shiftFactor);
	e = ((cfa->end - seqStart)>>shiftFactor);
	if (s < 0) s = 0;
	if (e > sampleWidth) e = sampleWidth;
	w = e - s;
	if (w > 0)
	    incStage(useCounts+s, w, stage);
	}
    }
resampleBytes(useCounts, sampleWidth, aveCounts, width);
grayThreshold(aveCounts, width);
for (i=0; i<lineHeight; ++i)
    mgPutSegZeroClear(mg, xOff, yOff+i, width, aveCounts);
freeMem(useCounts);
freeMem(aveCounts);
}

static void cloneFullDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw full  clone items. */
{
int y = yOff;
int lineHeight = mgFontLineHeight(font)+1;
struct cloneInfo *ci;
Color light = tg->ixAltColor;
int oneHeight;
int x1, x2, w;
int baseWidth = seqEnd - seqStart;
int tooBig = (winBaseCount > cloneFragMaxWin);


for (ci = tg->items; ci != NULL; ci = ci->next)
    {
    if (!tooBig)
	oneHeight = oneOrRowCount(ci)*lineHeight+2;
    else
	oneHeight = lineHeight;
    x1 = roundingScale(ci->cloneStart-winStart, width, baseWidth)+xOff;
    x2 = roundingScale(ci->cloneEnd-winStart, width, baseWidth)+xOff;
    w = x2-x1;
    mgDrawBox(mg, x1, y, w, oneHeight-1, light);
    if (!tooBig)
	drawOneClone(ci, seqStart, seqEnd, mg, xOff, y+1, width, font, lineHeight, 
		color, TRUE, tg->subType);
    else
	drawOneClone(ci, seqStart, seqEnd, mg, xOff, y, width, font, oneHeight-1, 
		color, FALSE, tg->subType);
    y += oneHeight;
    }
}

static void cloneDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw clone items. */
{
if (vis == tvFull)
    cloneFullDraw(tg, seqStart, seqEnd, mg, xOff, yOff, width, font, color, vis);
else
    cloneDenseDraw(tg, seqStart, seqEnd, mg, xOff, yOff, width, font, color, vis);
}


struct hash *realiCloneHash;
struct cloneInfo *realiCloneList;

void loadRealiClonesInWindow()
/* Load up realiCloneHash and realiCloneList with the clones in the window. */
{
if (realiCloneList == NULL)
    {
    char query[256];
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr = NULL;
    char **row;
    struct cloneInfo *ci;
    struct psl *psl;
    char *fragName;
    struct cloneFrag *cf;
    char cloneName[128];
    struct hashEl *hel;
    struct cloneFragPos *cfa;
    char *s;
    struct clonePos cp;

    /* Load in clone extents from database. */
    realiCloneHash = newHash(12);
    sprintf(query, 
    	"select * from cloneAliPos where chrom='%s'and chromStart<%u and chromEnd>%u",
	chromName, winEnd, winStart);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	clonePosStaticLoad(row, &cp);
	AllocVar(ci);
	hel = hashAdd(realiCloneHash, cp.name, ci);
	ci->name = hel->name;
	ci->cloneStart = cp.chromStart;
	ci->cloneEnd = cp.chromEnd;
	ci->phase = cp.phase;
	slAddHead(&realiCloneList, ci);
	}
    sqlFreeResult(&sr);

    /* Load in alignments from database and sort them by clone. */
    sprintf(query, "select * from %s_frags where tStart<%u and tEnd>%u",
	chromName, winEnd, winStart);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	psl = pslLoad(row);
	fragName = psl->qName;
	strcpy(cloneName, fragName);
	s = strchr(cloneName, '_');
	if (s != NULL)
	    *s = 0;
	if ((hel = hashLookup(realiCloneHash, cloneName)) == NULL)
	    {
	    warn("%s not in range in cloneAliPos");
	    continue;
	    }
	ci = hel->val;
	if ((cf = findCloneFrag(ci, fragName)) == NULL)
	    {
	    AllocVar(cf);
	    cf->name = fragName;
	    slAddHead(&ci->fragList, cf);
	    }
	AllocVar(cfa);
	cfa->frag = cf;
	cfa->psl = psl;
	cfa->start = psl->tStart;
	cfa->end = psl->tEnd;
	slAddHead(&ci->cfaList, cfa);
	}
    slSort(&realiCloneList, cmpCloneInfo);
    sqlFreeResult(&sr);
    hFreeConn(&conn);

    /* Do preliminary layout processing for each clone. */
    for (ci = realiCloneList; ci != NULL; ci = ci->next)
	{
	slReverse(&ci->cfaList);
	layoutCloneAli(ci);
	}
    }
}


struct hash *glCloneHash;
struct cloneInfo *glCloneList;

void glLoadInWindow()
/* Load up glCloneHash and glCloneList with the clones in the window. */
{
if (glCloneList == NULL)
    {
    char query[256];
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr = NULL;
    char **row;
    struct cloneInfo *ci;
    struct gl *gl;
    char *fragName;
    struct cloneFrag *cf;
    char cloneName[128];
    struct hashEl *hel;
    struct cloneFragPos *cfa;
    struct clonePos cp;
    char *s;

    /* Load in clone extents from database. */
    glCloneHash = newHash(12);
    sprintf(query, 
    	"select * from clonePos where chrom='%s'and chromStart<%u and chromEnd>%u",
	chromName, winEnd, winStart);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	clonePosStaticLoad(row, &cp);
	AllocVar(ci);
	hel = hashAdd(glCloneHash, cp.name, ci);
	ci->name = hel->name;
	ci->cloneStart = cp.chromStart;
	ci->cloneEnd = cp.chromEnd;
	ci->phase = cp.phase;
	ci->stage = cp.stage[0];
	slAddHead(&glCloneList, ci);
	}
    sqlFreeResult(&sr);

    /* Load in alignments from database and sort them by clone. */
    sprintf(query, "select * from %s_gl where start<%u and end>%u",
	chromName, winEnd, winStart);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	gl = glLoad(row);
	fragName = gl->frag;
	strcpy(cloneName, fragName);
	s = strchr(cloneName, '_');
	if (s != NULL)
	    *s = 0;
	if ((hel = hashLookup(glCloneHash, cloneName)) == NULL)
	    {
	    if (!sameString(database, "hg4") && !sameString(database, "hg5")
	    	&& !sameString(database, "hg6") && !sameString(database, "hg7"))
		/* Honestly, Asif and Jim will fix this someday! */
		warn("%s not in range in clonePos", cloneName);
	    continue;
	    }
	ci = hel->val;
	if ((cf = findCloneFrag(ci, fragName)) == NULL)
	    {
	    AllocVar(cf);
	    cf->name = fragName;
	    slAddHead(&ci->fragList, cf);
	    }
	AllocVar(cfa);
	cfa->frag = cf;
	cfa->gl = gl;
	cfa->start = gl->start;
	cfa->end = gl->end;
	slAddHead(&ci->cfaList, cfa);
	}
    slSort(&glCloneList, cmpCloneInfo);
    sqlFreeResult(&sr);
    hFreeConn(&conn);

    /* Do preliminary layout processing for each clone. */
    for (ci = glCloneList; ci != NULL; ci = ci->next)
	{
	slReverse(&ci->cfaList);
	layoutCloneAli(ci);
	}
    }
}



void coverageLoad(struct trackGroup *tg)
/* Load up clone alignments from database tables and organize. */
{
glLoadInWindow();
tg->items = glCloneList;
}

void coverageFree(struct trackGroup *tg)
/* Free up clone track group items. */
{
cloneInfoFreeList(&glCloneList);
freeHash(&glCloneHash);
}

struct trackGroup *coverageTrackGroup()
/* Make track group for golden path positions of all frags. */
{
struct trackGroup *tg;

AllocVar(tg);
tg->mapName = "hgCover";
tg->visibility = tvDense;
tg->longLabel = "Clone Coverage/Fragment Position";
tg->shortLabel = "Coverage";
tg->loadItems = coverageLoad;
tg->freeItems = coverageFree;
tg->drawItems = cloneDraw;
tg->altColor.r = 180;
tg->altColor.g = 180;
tg->altColor.b = 180;
tg->itemName = cloneName;
tg->mapItemName = cloneName;
tg->totalHeight = cloneTotalHeight;
tg->itemHeight = cloneItemHeight;
tg->itemStart = cloneItemStart;
tg->itemEnd = cloneItemEnd;
return tg;
}


void gapLoad(struct trackGroup *tg)
/* Load up clone alignments from database tables and organize. */
{
bedLoadChrom(tg, "gap", (ItemLoader)agpGapLoad);
}

void gapFree(struct trackGroup *tg)
/* Free up isochore items. */
{
agpGapFreeList((struct agpGap**)&tg->items);
}

char *gapName(struct trackGroup *tg, void *item)
/* Return name of gold track item. */
{
static char buf[24];
struct agpGap *gap = item;
sprintf(buf, "%s %s", gap->type, gap->bridge);
return buf;
}

int gapItemStart(struct trackGroup *tg, void *item)
/* Return start of gold track item. */
{
struct agpGap *gap = item;
return gap->chromStart;
}

int gapItemEnd(struct trackGroup *tg, void *item)
/* Return end of gold track item. */
{
struct agpGap *gap = item;
return gap->chromEnd;
}

static void gapDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw gap items. */
{
int baseWidth = seqEnd - seqStart;
struct agpGap *item;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2,w;
boolean isFull = (vis == tvFull);
double scale = width/(double)baseWidth;
int halfSize = heightPer/2;

for (item = tg->items; item != NULL; item = item->next)
    {
    x1 = round((double)((int)item->chromStart-winStart)*scale) + xOff;
    x2 = round((double)((int)item->chromEnd-winStart)*scale) + xOff;
    w = x2-x1;
    if (w < 1)
	w = 1;
    if (sameString(item->bridge, "no"))
	mgDrawBox(mg, x1, y, w, heightPer, color);
    else  /* Leave white line in middle of bridged gaps. */
        {
	mgDrawBox(mg, x1, y, w, halfSize, color);
	mgDrawBox(mg, x1, y+heightPer-halfSize, w, halfSize, color);
	}
    if (isFull)
	{
	char name[32];
	sprintf(name, "%s", item->type);
	mapBoxHc(item->chromStart, item->chromEnd, x1, y, w, heightPer, tg->mapName,
	    name, name);
#ifdef OLDCRUFT
	printf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x1, y, x1+w, y+heightPer);
	printf("HREF=\"../cgi-bin/hgc?g=%s&i=%s&c=%s&l=%d&r=%d&db=%s&o=%d\" ", 
	    tg->mapName, name, chromName, winStart, winEnd, database, 
	    item->chromStart);
	printf("ALT= \"%s bridged? %s\" TITLE=\"%s bridged? %s\">\n", item->type, item->bridge,
		item->type, item->bridge); 
#endif /* OLDCRUFT */
	y += lineHeight;
	}
    }
}


struct trackGroup *gapTg()
/* Make track group for golden path positions of all frags. */
{
struct trackGroup *tg;
AllocVar(tg);
tg->mapName = "hgGap";
tg->visibility = tvDense;
tg->longLabel = "Gap Locations";
tg->shortLabel = "Gap";
tg->loadItems = gapLoad;
tg->freeItems = gapFree;
tg->drawItems = gapDraw;
tg->itemName = gapName;
tg->mapItemName = gapName;
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = gapItemStart;
tg->itemEnd = gapItemEnd;
return tg;
}

#ifdef CHUCK_CODE
/* begin Chuck code */


int calcExprBedFullSize(struct exprBed  *exp) 
/* Because exprBedItemHeight and exprBedTotalHeight are also
used to center the labels on the left hand side
it was necessary to write this function which is essentially
a duplication of exprBedTotalHeight */
{
if(exp != NULL) 
    return 2*exp->numExp;
else
    return 0;
}

char *abbrevExprBedName(char *name)
{
static char abbrev[32];
char *ret;
strncpy(abbrev, name, sizeof(abbrev));
abbr(abbrev, "LINK_Em:");
ret = strstr(abbrev, "_");
ret++;
return ret;
}

char *exprBedName(struct trackGroup *tg, void *item) 
/* Return Name minus LINK_Em:_CloneName_. */
{
struct exprBed *exp = item;
char *full = exp->name;
return abbrevExprBedName(full);
}

char* exprBedItemName(struct trackGroup *tg, void *item)
/* Checks to make sure there is enough room for the next item 
   and returns the name if there is */
{
int maxHeight = tg->totalHeight(tg, tg->limitedVis);
int numItems = slCount(tg->items);
if(tg->items != item) 
    return cloneString("");
else 
    return cloneString(tg->shortLabel);
}

static int exprBedItemHeight(struct trackGroup *tg, void *item)
/* Return item height for fixed height track. */
{
int minHeight;
struct exprBed *exp = item;
if(tg->limitedVis == tvFull) 
    minHeight = calcExprBedFullSize(exp);
else 
    minHeight = mgFontLineHeight(tl.font);

if(tg->items == item) 
    return minHeight;
else 
    return 0;
}

static int exprBedTotalHeight(struct trackGroup *tg, enum trackVisibility vis)
/* exprBed track groups will use this to figure out the height they use. */
{
struct exprBed *exp = tg->items;
tg->lineHeight = tg->itemHeight(tg, tg->items);
tg->heightPer = tg->lineHeight - 1;
switch (vis)
    {
    case tvFull:
	tg->lineHeight = tg->heightPer = calcExprBedFullSize(exp);
	tg->height = tg->lineHeight +1;
	break;
    case tvDense:
	tg->lineHeight = mgFontLineHeight(tl.font);
	tg->heightPer = tg->lineHeight;
	tg->height = tg->lineHeight+1;;
	break;
    } 
return tg->height;
}

void makeRedGreenShades(struct memGfx *mg) 
/* Allocate the  shades of Red, Green and Blue */
{
int i;
int maxShade = ArraySize(shadesOfGreen) -1;
for(i=0; i<=maxShade; i++) 
    {
    struct rgbColor rgbGreen, rgbRed, rgbBlue;
    int level = (255*i/(maxShade));
    if(level<0) level = 0;
    shadesOfRed[i] = mgFindColor(mg, level, 0, 0);
    shadesOfGreen[i] = mgFindColor(mg, 0, level, 0);
    shadesOfBlue[i] = mgFindColor(mg, 0, 0, level);
    }
exprBedColorsMade = TRUE;
}


Color getExprDataColor(float val, float maxDeviation, boolean RG_COLOR_SCHEME ) 
/** Returns the appropriate Color from the shadesOfGreen and shadesOfRed arrays
 * @param float val - acutual data to be represented
 * @param float maxDeviation - maximum (and implicitly minimum) values represented
 * @param boolean RG_COLOR_SCHEME - are we red/green(TRUE) or red/blue(FALSE) ?
 */
{
float absVal = fabs(val);
int colorIndex = 0;

/* cap the value to be less than or equal to maxDeviation */
if(absVal > maxDeviation)
    absVal = maxDeviation;

/* project the value into the number of colors we have.  
 *   * i.e. if val = 1.0 and max is 2.0 and number of shades is 16 then index would be
 * 1 * 15 /2.0 = 7.5 = 7
 */
if(maxDeviation == 0) 
    errAbort("ERROR: hgTracksExample::getExprDataColor() maxDeviation can't be zero\n"); 

colorIndex = (int)(absVal * maxRGBShade/maxDeviation);

/* Return the correct color depending on color scheme and shades */
if(RG_COLOR_SCHEME) 
    {
    if(val > 0) 
	return shadesOfRed[colorIndex];
    else 
	return shadesOfGreen[colorIndex];
    }
else 
    {
    if(val > 0) 
	return shadesOfRed[colorIndex];
    else 
	return shadesOfBlue[colorIndex];
    }
}

static void mgExprBedBox(struct memGfx *mg, int xOff, int yOff, int width, int height, struct exprBed *exp)
{
int y1, y2;
int strips = exp->numExp;
int i;
int color = 1;

y2 = 0;
for (i=0; i<strips; ++i)
    {
    Color color = getExprDataColor(exp->scores[i],0.7,TRUE);
    y1 = y2;
    y2 = (i+1)*height/strips;
    mgDrawBox(mg, xOff, yOff+y1, width, y2-y1, color);
    }
}

void mapBoxHcWTarget(int start, int end, int x, int y, int width, int height, 
	char *group, char *item, char *statusLine, boolean target, char *otherFrame)
/* Print out image map rectangle that would invoke the htc (human track click)
 * program. */
{
char *encodedItem = cgiEncode(item);
printf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
printf("HREF=\"%s?o=%d&t=%d&g=%s&i=%s&c=%s&l=%d&r=%d&db=%s&pix=%d\" ", 
    hgcName(), start, end, group, encodedItem, chromName, winStart, winEnd, 
    database, tl.picWidth);
if(target) 
    {
    printf(" target=\"%s\" ", otherFrame);
    } 
printf("ALT= \"%s\" TITLE=\"%s\">\n", statusLine, statusLine); 
freeMem(encodedItem);
}


static void exprBedDraw(struct trackGroup *tg, int seqStart, int seqEnd,
			struct memGfx *mg, int xOff, int yOff, int width,
			MgFont *font, Color color, enum trackVisibility vis) 
/**
 * Draw the box for a ExprBed.h
 */
{
int baseWidth = seqEnd - seqStart;
struct exprBed *item;
int y = yOff;
struct exprBed *exp;

int lineHeight = tg->lineHeight;
int x1,x2,w;
boolean isFull = (vis == tvFull);
double scale = width/(double)baseWidth;
int fontHeight = mgFontLineHeight(font);
exp = tg->items;

if(!exprBedColorsMade)
    makeRedGreenShades(mg);

for (item = tg->items; item != NULL; item = item->next)
    {
    x1 = round((double)((int)item->chromStart-winStart)*scale) + xOff;
    x2 = round((double)((int)item->chromEnd-winStart)*scale) + xOff;
    w = x2-x1;
    if (tg->itemColor != NULL)
	color = tg->itemColor(tg, item, mg);
    if (w < 1)
	w = 1;
    mgExprBedBox(mg, x1, y, w, lineHeight, item);
    if(otherFrame != NULL)
	mapBoxHcWTarget(item->chromStart, item->chromEnd, x1, y, w, lineHeight, 
			tg->mapName, item->name, abbrevExprBedName(item->name), TRUE, otherFrame); 
    else
	mapBoxHcWTarget(item->chromStart, item->chromEnd, x1, y, w, lineHeight, 
			tg->mapName, item->name, abbrevExprBedName(item->name), FALSE, NULL); 
    }
}

void loadRosettaPeBed(struct trackGroup *tg)
/* Load up exprBed from database table rosettaPe to trackgroup items. */
{
bedLoadItem(tg, "rosettaPe", (ItemLoader)exprBedLoad);
}

void loadRosettaTeBed(struct trackGroup *tg)
/* Load up exprBed from database table rosettaTe to trackgroup items. */
{
bedLoadItem(tg, "rosettaTe", (ItemLoader)exprBedLoad);
}


void freeExprBed(struct trackGroup *tg)
/* Free up exprBed items. */
{
exprBedFreeList((struct exprBed**)&tg->items);
}


struct trackGroup *rosettaPeTg()
/* Make track group for Rosetta Track Predicted Exons. */
{
struct trackGroup *tg = bedTg();

tg->mapsSelf = TRUE;
tg->drawItems = exprBedDraw;
tg->mapName = "rosettaPe";
tg->visibility = tvDense;
tg->itemHeight = exprBedItemHeight;
tg->itemName = exprBedItemName;
tg->totalHeight = exprBedTotalHeight;
tg->longLabel = "Rosetta Predicted Exon Data";
tg->shortLabel = "Rosetta P.E.";
tg->loadItems = loadRosettaPeBed;
tg->freeItems = freeExprBed;
return tg;
}
struct trackGroup *rosettaTeTg()
/* Make track group for Rosetta Track Predicted Exons. */
{
struct trackGroup *tg = bedTg();

tg->mapsSelf = TRUE;
tg->drawItems = exprBedDraw;
tg->mapName = "rosettaTe";
tg->visibility = tvDense;
tg->itemHeight = exprBedItemHeight;
tg->itemName = exprBedItemName;
tg->totalHeight = exprBedTotalHeight;
tg->longLabel = "Rosetta Confirmed Exon Data";
tg->shortLabel = "Rosetta T.E.";
tg->loadItems = loadRosettaTeBed;
tg->freeItems = freeExprBed;
return tg;
}
#endif /*CHUCK_CODE*/


/* The next two functions are being phased out as the Rosetta data
set is split into two tracks instead of one large one. */
#ifdef ALL_ROSETTA
void loadExprBed(struct trackGroup *tg)
/* Load up exprBed from database table to trackGroup items. */
{
bedLoadItem(tg, "exprBed", (ItemLoader)exprBedLoad);
}

struct trackGroup *exprBedTg()
/* Make track group for exprBed. */
{
struct trackGroup *tg = bedTg();

tg->mapsSelf = TRUE;
tg->drawItems = exprBedDraw;
tg->mapName = "hgExprBed";
tg->visibility = tvHide;
tg->itemHeight = exprBedItemHeight;
tg->itemName = exprBedItemName;
tg->totalHeight = exprBedTotalHeight;
tg->longLabel = "Rosetta Data";
tg->shortLabel = "Rosetta Data";
tg->loadItems = loadExprBed;
tg->freeItems = freeExprBed;
return tg;
}
#endif /*ALL_ROSETTA*/


#ifdef SOMEDAY
/* These next three vars are used to communicate info from the
 * loadAltGraph routine to the agTransOut routine */
struct linkedFeatures *agTempList;  /* Transcripts get added to this list. */
int agTempOrientation;
int agClusterIx;
int agTranscriptIx;

void agTransOut(struct ggAliInfo *da, int cStart, int cEnd)
/* Called to convert gene graph transcript to something more permanent. */
{
struct linkedFeatures *lf;
struct simpleFeature *sfList = NULL, *sf;
struct ggVertex *vertices = da->vertices;
int vertexCount = da->vertexCount;
int i;
static int tot = 0;
int minStart = 0x3fffffff;
int maxEnd = -minStart;
int start,end;

AllocVar(lf);
lf->grayIx = maxShade;
sprintf(lf->name, "cluster %d.%d", agClusterIx+1, agTranscriptIx++);
lf->orientation = agTempOrientation;
for (i=0; i<vertexCount; i+=2)
    {
    AllocVar(sf);
    start = sf->start = vertices[i].position;
    if (start < minStart)
	minStart = start;
    end = sf->end = vertices[i+1].position;
    if (end > maxEnd)
	maxEnd = end;
    sf->grayIx = maxShade;
    slAddHead(&sfList, sf);
    }
lf->start = minStart;
lf->end = maxEnd;
slReverse(&sfList);
lf->components = sfList;
slAddHead(&agTempList, lf);
}

void loadAltGraph(struct trackGroup *tg)
/* Get alt splicing graphs and turn them into linked features. */
{
struct hgBac *bac = hgGetBac(chromName);
char query[256];
struct sqlConnection *conn = hgAllocConn();
struct sqlResult *sr = NULL;
char **row;
boolean firstTime = TRUE;
boolean anyIntersection = FALSE;
HGID lastParent = 0;

sprintf(query, 
    "select * from altGraph where startBac=%u and endPos>%d and startPos<%d",
    bac->id, winStart, winEnd);

sr = sqlGetResult(conn, query);
agTempList = NULL;
agClusterIx = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct geneGraph *gg = ggFromRow(row);
    agTranscriptIx = 0;
    agTempOrientation = gg->orientation;
    traverseGeneGraph(gg, gg->startPos, gg->endPos, agTransOut);
    freeGeneGraph(&gg);
    ++agClusterIx;
    }
slReverse(&agTempList);
tg->items = agTempList;
agTempList = NULL;
sqlFreeResult(&sr);
hgFreeConn(&conn);
}

struct trackGroup *altGraphTg()
/* Make track group of altGraph. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgAltGraph";
tg->visibility = tvFull;
tg->longLabel = "mRNA Clusters";
tg->shortLabel = "mRNA Clusters";
tg->loadItems = loadAltGraph;
return tg;
}
#endif /* SOMEDAY */

enum trackVisibility limitVisibility(struct trackGroup *tg)
/* Return default visibility limited by number of items. */
{
enum trackVisibility vis = tg->visibility;
if (vis != tvFull)
    return vis;
if (slCount(tg->items) <= maxItemsInFullTrack)
    return tvFull;
else
    return tvDense;
}

void makeActiveImage(struct trackGroup *groupList)
/* Make image and image map. */
{
struct trackGroup *group;
MgFont *font = tl.font;
struct memGfx *mg;
struct tempName gifTn;
char *mapName = "map";

int fontHeight = mgFontLineHeight(font);
int insideHeight = fontHeight-1;
int border = 1;
int xOff = border;
int pixWidth, pixHeight;
int insideWidth;
int y;

/* Coordinates of open/close/hide buttons. */
#ifdef SOON
int openCloseHideWidth = 150;
#endif
int openCloseHideWidth = 0;
int openWidth = 50, openOffset = 0;
int closeWidth = 50, closeOffset = 50;
int hideWidth = 50, hideOffset = 100;

int typeCount = slCount(groupList);
int leftLabelWidth = 0;

int rulerHeight = fontHeight;
int yAfterRuler = border;
int relNumOff;
int i;

/* Figure out dimensions and allocate drawing space. */
if (withLeftLabels)
    {
    leftLabelWidth = tl.leftLabelWidth;
    xOff += leftLabelWidth + border;
    }    
pixWidth = tl.picWidth;
insideWidth = pixWidth-border-xOff;
pixHeight = border;
if (withRuler)
    {
    yAfterRuler += rulerHeight;
    pixHeight += rulerHeight;
    }
for (group = groupList; group != NULL; group = group->next)
    {
    if (group->visibility != tvHide)
	{
	pixHeight += group->totalHeight(group, group->limitedVis);
	if (withCenterLabels)
	    pixHeight += fontHeight;
	}
    }
mg = mgNew(pixWidth, pixHeight);
mgClearPixels(mg);
makeGrayShades(mg);
makeBrownShades(mg);
makeSeaShades(mg);

/* Start up client side map. */
printf("<MAP Name=%s>\n", mapName);
/* Find colors to draw in. */
for (group = groupList; group != NULL; group = group->next)
    {
    if (group->limitedVis != tvHide)
	{
	group->ixColor = mgFindColor(mg, 
		group->color.r, group->color.g, group->color.b);
	group->ixAltColor = mgFindColor(mg, 
		group->altColor.r, group->altColor.g, group->altColor.b);
	}
    }

/* Draw left labels. */
if (withLeftLabels)
    {
    int inWid = xOff-border*3;
    mgDrawBox(mg, xOff-border*2, 0, border, pixHeight, mgFindColor(mg, 0, 0, 200));
    mgSetClip(mg, border, border, inWid, pixHeight-2*border);
    y = border;
    if (withRuler)
	{
	mgTextRight(mg, border, y, inWid-1, rulerHeight, 
	    MG_BLACK, font, "Base Position");
	y += rulerHeight;
	}
    for (group = groupList; group != NULL; group = group->next)
        {
	struct slList *item;
	switch (group->limitedVis)
	    {
	    case tvHide:
		break;	/* Do nothing; */
	    case tvFull:
		if (withCenterLabels)
		    y += fontHeight;
		for (item = group->items; item != NULL; item = item->next)
		    {
		    char *name = group->itemName(group, item);
		    int itemHeight = group->itemHeight(group, item);
		    mgTextRight(mg, border, y, inWid-1, itemHeight, group->ixColor, font, name);
		    y += itemHeight;
		    }
		break;
	    case tvDense:
		if (withCenterLabels)
		    y += fontHeight;
		mgTextRight(mg, border, y, inWid-1, group->lineHeight, group->ixColor, font, group->shortLabel);
		y += group->lineHeight;
		break;
	    }
        }
    }

/* Draw guidelines. */
if (withGuidelines)
    {
    int clWidth = insideWidth-openCloseHideWidth;
    int ochXoff = xOff + clWidth;
    int height = pixHeight - 2*border;
    int x;
    Color color = mgFindColor(mg, 220, 220, 255);
    int lineHeight = mgFontLineHeight(tl.font)+1;

    mgSetClip(mg, xOff, border, insideWidth, height);
    y = border;

    for (x = xOff+9; x<pixWidth; x += 10)
	mgDrawBox(mg, x, y, 1, height, color);
#ifdef SOMETIMES
    for (y= yAfterRuler + lineHeight/2 - 1; y<pixHeight; y += lineHeight)
        mgDrawBox(mg, xOff, y, insideWidth, 1, color);
#endif /* SOMETIMES */
    }

/* Show ruler at top. */
if (withRuler)
    {
    y = 0;
    mgSetClip(mg, xOff, y, insideWidth, rulerHeight);
    relNumOff = winStart;
    if (seqReverse)
	relNumOff = -relNumOff;
    mgDrawRulerBumpText(mg, xOff, y, rulerHeight, insideWidth, MG_BLACK, font,
	relNumOff, winBaseCount, 0, 1);

    /* Make hit boxes that will zoom program around ruler. */
	{
	int boxes = 30;
	int winWidth = winEnd - winStart;
	int newWinWidth = winWidth/4;
	int i, ws, we = 0, ps, pe = 0;
	int mid, ns, ne;
	double wScale = (double)winWidth/boxes;
	double pScale = (double)insideWidth/boxes;
	for (i=1; i<=boxes; ++i)
	    {
	    ps = pe;
	    ws = we;
	    pe = round(pScale*i);
	    we = round(wScale*i);
	    mid = (ws + we)/2 + winStart;
	    ns = mid-newWinWidth/2;
	    ne = ns + newWinWidth;
	    mapBoxJumpTo(ps+xOff,y,pe-ps,rulerHeight,
		chromName, ns, ne, "3x zoom");
	    }
	}
    }


/* Draw center labels. */
if (withCenterLabels)
    {
    int clWidth = insideWidth-openCloseHideWidth;
    int ochXoff = xOff + clWidth;
    mgSetClip(mg, xOff, border, insideWidth, pixHeight - 2*border);
    y = yAfterRuler;
    for (group = groupList; group != NULL; group = group->next)
        {
	if (group->limitedVis != tvHide)
	    {
	    Color color = group->ixColor;
	    mgTextCentered(mg, xOff, y+1, clWidth, insideHeight, color, font, group->longLabel);
	    mapBoxToggleVis(0,y+1,pixWidth,insideHeight,group);
#ifdef SOON
	    mgTextCentered(mg, ochXoff, y+1, openCloseHideWidth, insideHeight, 
		color, font, "[open] [close] [hide]");
#endif
	    y += fontHeight;
	    y += group->height;
	    }
        }
    }

/* Draw tracks. */
y = yAfterRuler;
for (group = groupList; group != NULL; group = group->next)
    {
    if (group->limitedVis != tvHide)
	{
	if (withCenterLabels)
	    y += fontHeight;
	mgSetClip(mg, xOff, y, insideWidth, group->height);
	group->drawItems(group, winStart, winEnd,
	    mg, xOff, y, insideWidth, 
	    font, group->ixColor, group->limitedVis);
	y += group->height;
	}
    }

/* Make map background. */
    {
    y = yAfterRuler;
    for (group = groupList; group != NULL; group = group->next)
        {
	struct slList *item;
	switch (group->limitedVis)
	    {
	    case tvHide:
		break;	/* Do nothing; */
	    case tvFull:
		if (withCenterLabels)
		    y += fontHeight;
		for (item = group->items; item != NULL; item = item->next)
		    {
		    int height = group->itemHeight(group, item);
		    if (!group->mapsSelf)
			{
			mapBoxHc(group->itemStart(group, item), group->itemEnd(group, item),
			    0,y,pixWidth,height, group->mapName, 
			    group->mapItemName(group, item), 
			    group->itemName(group, item));
			}
		    y += height;
		    }
		break;
	    case tvDense:
		if (withCenterLabels)
		    y += fontHeight;
		mapBoxToggleVis(0,y,pixWidth,group->lineHeight,group);
		y += group->lineHeight;
		break;
	    }
        }
    }

/* Finish map. */
printf("</MAP>\n");

/* Save out picture and tell html file about it. */
makeTempName(&gifTn, "hgt", ".gif");
mgSaveGif(mg, gifTn.forCgi);
printf(
    "<P><IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s><BR>\n",
    gifTn.forHtml, pixWidth, pixHeight, mapName);

mgFree(&mg);
}


void makeCheckBox(char *name, boolean isChecked)
/* Create a checkbox with the given name in the given state. */
{
printf("<INPUT TYPE=CHECKBOX NAME=\"%s\" VALUE=on%s>", name,
    (isChecked ? " CHECKED" : "") );
}

void printEnsemblAnchor()
/* Print anchor to Ensembl display on same window. */
{
int bigStart, bigEnd, smallStart, smallEnd;
int ucscSize;

ucscSize = winEnd - winStart;
bigStart = smallStart = winStart;
bigEnd = smallEnd = winEnd;
if (ucscSize < 1000000)
    {
    bigStart -= 500000;
    if (bigStart < 0) bigStart = 0;
    bigEnd += 500000;
    if (bigEnd > seqBaseCount) bigEnd = seqBaseCount;
    printf("<A HREF=\"http://www.ensembl.org/perl/contigview"
	   "?chr=%s&vc_start=%d&vc_end=%d&wvc_start=%d&wvc_end=%d\" TARGET=_blank>",
	    chromName, bigStart, bigEnd, smallStart, smallEnd);
    }
else
    {
    printf("<A HREF=\"http://www.ensembl.org/perl/contigview"
	   "?chr=%s&vc_start=%d&vc_end=%d\" TARGET=_blank>",
	    chromName, bigStart, bigEnd);
    }
}


void doForm()
/* Make the tracks display form with the zoom/scroll
 * buttons and the active image. */
{
struct trackGroup *group;
int controlColNum=0;
/* See if want to include sequence search results. */
userSeqString = cgiOptionalString("ss");
if (userSeqString != NULL)
    eUserSeqString = cgiEncode(userSeqString);

hideControls = cgiBoolean("hideControls");
if (calledSelf)
    {
    char *s;
    /* Set center and left labels from UI. */
    withLeftLabels = cgiBoolean("leftLabels");
    withCenterLabels = cgiBoolean("centerLabels");
    withGuidelines = cgiBoolean("guidelines");
    if ((s = cgiOptionalString("ruler")) != NULL)
	withRuler = !sameWord(s, "off");
    }

/* Make list of all track groups. */
if (hTableExists("cytoBand")) slSafeAddHead(&tGroupList, cytoBandTg());
if (hTableExists("mapGenethon")) slSafeAddHead(&tGroupList, genethonTg());
if (hTableExists("stsMarker")) slSafeAddHead(&tGroupList, stsMarkerTg());
if (hTableExists("mouseSyn")) slSafeAddHead(&tGroupList, mouseSynTg());
if (hTableExists("isochores")) slSafeAddHead(&tGroupList, isochoresTg());
if (hTableExists("gcPercent")) slSafeAddHead(&tGroupList, gcPercentTg());
if (hTableExists("ctgPos")) slSafeAddHead(&tGroupList, contigTg());
slSafeAddHead(&tGroupList, goldTrackGroup());
slSafeAddHead(&tGroupList, gapTg());
if (hTableExists("genomicDups")) slSafeAddHead(&tGroupList, genomicDupsTg());
slSafeAddHead(&tGroupList, coverageTrackGroup());
if (userSeqString != NULL) slSafeAddHead(&tGroupList, userPslTg());
if (hTableExists("genieKnown")) slSafeAddHead(&tGroupList, genieKnownTg());
if (hTableExists("refGene")) slSafeAddHead(&tGroupList, refGeneTg());
if (sameString(chromName, "chr22") && hTableExists("sanger22")) slSafeAddHead(&tGroupList, sanger22Tg());
if (hTableExists("genieAlt")) slSafeAddHead(&tGroupList, genieAltTg());
if (hTableExists("ensGene")) slSafeAddHead(&tGroupList, ensemblGeneTg());
if (hTableExists("softberryGene")) slSafeAddHead(&tGroupList, softberryGeneTg());
if (chromTableExists("_mrna")) slSafeAddHead(&tGroupList, fullMrnaTg());
if (chromTableExists("_intronEst")) slSafeAddHead(&tGroupList, intronEstTg());
if (chromTableExists("_est")) slSafeAddHead(&tGroupList, estTg());
if (hTableExists("est3")) slSafeAddHead(&tGroupList, est3Tg());
if (hTableExists("cpgIsland")) slSafeAddHead(&tGroupList, cpgIslandTg());
if (privateVersion())
    {
    if (hTableExists("cpgIsland2")) slSafeAddHead(&tGroupList, cpgIsland2Tg());
    if (hTableExists("mus7of8")) slSafeAddHead(&tGroupList, mus7of8Tg());
    if (hTableExists("musPairOf4")) slSafeAddHead(&tGroupList, musPairOf4Tg());
    if (hTableExists("musTest1")) slSafeAddHead(&tGroupList, musTest1Tg());
    if (hTableExists("musTest2")) slSafeAddHead(&tGroupList, musTest2Tg());
    }
if (chromTableExists("_blatMouse")) slSafeAddHead(&tGroupList, blatMouseTg());
if (hTableExists("exoMouse")) slSafeAddHead(&tGroupList, exoMouseTg());
if (hTableExists("exoFish")) slSafeAddHead(&tGroupList, exoFishTg());
if (chromTableExists("_tet_waba")) slSafeAddHead(&tGroupList, tetTg());
if (hTableExists("rnaGene")) slSafeAddHead(&tGroupList, rnaGeneTg());
if (hTableExists("snpNih")) slSafeAddHead(&tGroupList, snpNihTg());
if (hTableExists("snpTsc")) slSafeAddHead(&tGroupList, snpTscTg());
if (chromTableExists("_rmsk")) slSafeAddHead(&tGroupList, repeatTg());
if (hTableExists("simpleRepeat")) slSafeAddHead(&tGroupList, simpleRepeatTg());
if (hTableExists("mgc_mrna")) slSafeAddHead(&tGroupList, fullMgcMrnaTg());
if (hTableExists("bacEnds")) slSafeAddHead(&tGroupList, bacEndsTg());
#ifdef CHUCK_CODE
if (sameString(chromName, "chr22") && hTableExists("rosettaTe")) slSafeAddHead(&tGroupList,rosettaTeTg());   
if (sameString(chromName, "chr22") && hTableExists("rosettaPe")) slSafeAddHead(&tGroupList,rosettaPeTg()); 
#endif /*CHUCK_CODE*/
/* This next track is being phased out as the Rosetta data is being split into
   two data sets, keep this one around for a little while for debugging */
#ifdef ALL_ROSETTA
if (hTableExists("exprBed")) slSafeAddHead(&tGroupList, exprBedTg());
#endif /* ALL_ROSETTA */

slReverse(&tGroupList);

/* Get visibility values if any from ui. */
for (group = tGroupList; group != NULL; group = group->next)
    {
    char *s = cgiOptionalString(group->mapName);
    if (s != NULL)
	{
	int vis = stringArrayIx(s, tvStrings, ArraySize(tvStrings));
	if (vis < 0)
	    errAbort("Unknown value %s for %s", s, group->mapName);
	group->visibility = vis;
	}
    }

/* Tell groups to load their items. */
for (group = tGroupList; group != NULL; group = group->next)
    {
    if (group->visibility != tvHide)
	{
	group->loadItems(group); 
	group->limitedVis = limitVisibility(group);
	}
    }

/* Tell browser where to go when they click on image. */
printf("<FORM ACTION=\"%s\">\n\n", hgTracksName());

/* Center everything from now on. */
printf("<CENTER>\n");

if (!hideControls)
    {
    /* Show title . */
    printf("<H2>Chromosome %s, Bases %d-%d, Size %d</H2>", skipChr(chromName), 
	    winStart+1, winEnd, winEnd - winStart);

    /* Put up scroll and zoom controls. */
    fputs("move ", stdout);
    cgiMakeButton("left3", "<<<");
    cgiMakeButton("left2", " <<");
    cgiMakeButton("left1", " < ");
    cgiMakeButton("right1", " > ");
    cgiMakeButton("right2", ">> ");
    cgiMakeButton("right3", ">>>");
    fputs(" zoom in ", stdout);
    cgiMakeButton("in1", "1.5x");
    cgiMakeButton("in2", " 3x ");
    cgiMakeButton("in3", "10x");
    fputs(" zoom out ", stdout);
    cgiMakeButton("out1", "1.5x");
    cgiMakeButton("out2", " 3x ");
    cgiMakeButton("out3", "10x");
    fputs("<BR>\n", stdout);

    /* Make line that says position. */
	{
	fputs("position ", stdout);
	cgiMakeTextVar("position", position, 30);
	fputs(" pixel width ", stdout);
	cgiMakeIntVar("pix", tl.picWidth, 4);
	fputs(" ", stdout);
	cgiMakeButton("submit", "jump");
	printf(" <A HREF=\"../goldenPath/help/hgTracksHelp.html\" TARGET=_blank>User's Guide</A>");
	fputs("<BR>\n", stdout);
	}
    }

/* Make clickable image and map. */
makeActiveImage(tGroupList);

if (!hideControls)
    {
    fputs("</CENTER>", stdout);
    fputs("Click on an item in a track to view more information on that item. "
	  "Click center label to toggle between full and dense display of "
	  "that track.  Tracks with more than 300 items are always displayed "
	  "densely.  Click on base position to zoom in by 3x around where you "
	  "clicked.<BR>",
	  stdout);
    fputs("<TABLE BORDER=\"1\" WIDTH=\"100%\"><TR><TD><P ALIGN=CENTER>", stdout);
    printf("<A HREF=\"%s?o=%d&g=getDna&i=mixed&c=%s&l=%d&r=%d&db=%s\">"
	  "View DNA</A></TD>",  hgcName(),
	  winStart, chromName, winStart, winEnd, database);
    if (sameString(database, "hg6"))
	{
	fputs("<TD><P ALIGN=CENTER>", stdout);
	printEnsemblAnchor();
	fputs("Visit Ensembl</A></TD>", stdout);
	}
    if (sameString(database, "hg5") || sameString(database, "hg6") || sameString(database, "hg7"))
	fprintf(stdout, "<TD><P ALIGN=CENTER><A HREF=\"../cgi-bin/hgBlat?db=%s\">BLAT Search</A></TD>", database);
    fprintf(stdout, "<TD><P ALIGN=CENTER><A HREF=\"/index.html\">Genome Home</A></TD>");
    fputs("</TR>", stdout);
    fputs("</TABLE><CENTER>\n", stdout);

    /* Display bottom control panel. */
    fputs("Chromosome ", stdout);
    cgiMakeDropList("seqName", hgChromNames, 24, chromName);
    fputs(" bases ",stdout);
    cgiMakeIntVar("winStart", winStart, 12);
    fputs(" - ", stdout);
    cgiMakeIntVar("winEnd", winEnd, 12);
    printf(" Guidelines ");
    makeCheckBox("guidelines", withGuidelines);
    printf(" <B>Labels:</B> ");
    printf("left ");
    makeCheckBox("leftLabels", withLeftLabels);
    printf("center ");
    makeCheckBox("centerLabels", withCenterLabels);
    printf(" ");
    cgiMakeButton("submit", "refresh");
    printf("<BR>\n");


    /* Display viewing options for each group. */
    /* Chuck: This is going to be wrapped in a table so that
     * the controls don't wrap around randomly
     */
    printf("<table border=0 cellspacing=1 cellpadding=1 width=%d>\n", CONTROL_TABLE_WIDTH);
    printf("<tr><th colspan=%d>\n", MAX_CONTROL_COLUMNS);
    printf("<BR><B>Track Controls:</B><BR> ");
    printf("</th></tr>\n");
    printf("<tr><td align=left>\n");
    printf(" Base Position <BR>");
    cgiMakeDropList("ruler", offOn, 2, offOn[withRuler]);
    printf("</td>");
    controlColNum=1;
    for (group = tGroupList; group != NULL; group = group->next)
	{
	if(controlColNum >= MAX_CONTROL_COLUMNS) 
	    {
	    printf("</tr><tr><td align=left>\n");
	    controlColNum =1;
	    }
	else 
	    {
	    printf("<td align=left>\n");
	    controlColNum++;
	    }
	printf(" %s<BR> ", group->shortLabel);
	cgiMakeDropList(group->mapName, tvStrings, ArraySize(tvStrings), tvStrings[group->visibility]);
	printf("</td>\n");
	}
    /* now finish out the table */
    for( ; controlColNum < MAX_CONTROL_COLUMNS; controlColNum++)
	printf("<td>&nbsp</td>\n");
    printf("</tr>\n</table>\n");
    }
saveHiddenVars();

/* Clean up. */
for (group = tGroupList; group != NULL; group = group->next)
    {
    if (group->visibility != tvHide)
	group->freeItems(group);
    }
printf("</FORM>");
}


void zoomAroundCenter(double amount)
/* Set ends so as to zoom around center by scaling amount. */
{
int center = (winStart + winEnd)/2;
int newCount = (int)(winBaseCount*amount + 0.5);
if (newCount < 30) newCount = 30;
if (newCount > seqBaseCount)
    newCount = seqBaseCount;
winStart = center - newCount/2;
winEnd = winStart + newCount;
if (winStart <= 0 && winEnd >= seqBaseCount)
    {
    winStart = 0;
    winEnd = seqBaseCount;
    }
else if (winStart <= 0)
    {
    winStart = 0;
    winEnd = newCount;
    }
else if (winEnd > seqBaseCount)
    {
    winEnd = seqBaseCount;
    winStart = winEnd - newCount;
    }
winBaseCount = winEnd - winStart;
}

void relativeScroll(double amount)
/* Scroll percentage of visible window. */
{
int offset;
int newStart, newEnd;

if (seqReverse)
    amount = -amount;
offset = (int)(amount * winBaseCount + 0.5);
/* Make sure don't scroll of ends. */
newStart = winStart + offset;
newEnd = winEnd + offset;
if (newStart < 0)
    offset = -winStart;
else if (newEnd > seqBaseCount)
    offset = seqBaseCount - winEnd;

/* Move window. */
winStart += offset;
winEnd += offset;
}

#ifdef OLD
boolean findGenomePos(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Find position in genome of spec.  Don't alter
 * return variables if some sort of error. */
{
boolean isUnique = TRUE;
spec = trimSpaces(spec);
if (hgIsChromRange(spec))
    {
    hgParseChromRange(spec, retChromName, retWinStart, retWinEnd);
    return TRUE;
    }
else if (isContigName(spec))
    {
    findContigPos(spec, retChromName, retWinStart, retWinEnd);
    return TRUE;
    }
else if (findCytoBand(spec, retChromName, retWinStart, retWinEnd))
    {
    return TRUE;
    }
else if (findClonePos(spec, retChromName, retWinStart, retWinEnd))
    {
    return TRUE;
    }
else if (findMrnaPos(spec, retChromName, retWinStart, retWinEnd, &isUnique))
    return isUnique;
else if (findStsPos(spec, retChromName, retWinStart, retWinEnd))
    return TRUE;
else if (findGenethonPos(spec, retChromName, retWinStart, retWinEnd))
    return TRUE;
else if (findMrnaKeys(spec))
    return FALSE;
else
    {
    errAbort("Sorry, couldn't locate %s in genome database\n", spec);
    return TRUE;
    }
}
#endif /* OLD */

boolean findGenomePos(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd)
{
struct hgPositions *hgp;
struct hgPos *pos;
struct dyString *ui;

hgp = hgPositionsFind(spec, "");
if (hgp == NULL || hgp->posCount == 0)
    {
    hgPositionsFree(&hgp);
    errAbort("Sorry, couldn't locate %s in genome database\n", spec);
    return TRUE;
    }
if ((pos = hgp->singlePos) != NULL)
    {
    *retChromName = pos->chrom;
    *retWinStart = pos->chromStart;
    *retWinEnd = pos->chromEnd;
    hgPositionsFree(&hgp);
    return TRUE;
    }
else
    {
    hgPositionsHtml(hgp, stdout);
    hgPositionsFree(&hgp);
    return FALSE;
    }
freeDyString(&ui);
}


void doMiddle()
/* Print the body of an html file.  This routine handles zooming and
 * scrolling. */
{
char *oldSeq;
char *submitVal;
boolean testing = FALSE;

// pushCarefulMemHandler(32*1024*1024);
/* Initialize layout and database. */
database = cgiOptionalString("db");
if (database == NULL)
    database = "hg6";
hSetDb(database);
initTl();

/* Read in input from CGI. */
oldSeq = cgiOptionalString("old");
if (oldSeq != NULL)
   calledSelf = TRUE; 
position = cgiOptionalString("position");
if ((submitVal = cgiOptionalString("submit")) != NULL)
    if (sameString(submitVal, "refresh"))
        position = NULL;
if (position == NULL)
    position = "";
if (position != NULL && position[0] != 0)
    {
    if (!findGenomePos(position, &chromName, &winStart, &winEnd))
        return;
    }
if (chromName == NULL)
    {
    chromName = cgiString("seqName");
    winStart = cgiInt("winStart");
    winEnd = cgiInt("winEnd");
    }

/* Clip chromosomal position to fit. */
seqBaseCount = hChromSize(chromName);
seqReverse = cgiBoolean("seqReverse");
if (winEnd < winStart)
    {
    int temp = winEnd;
    winEnd = winStart;
    winStart = temp;
    }
else if (winStart == winEnd)
    {
    winStart -= 1000;
    winEnd += 1000;
    }
if (winStart < 0)
    winStart = 0;
if (winEnd > seqBaseCount)
    winEnd = seqBaseCount;

winBaseCount = winEnd - winStart;
if (winBaseCount == 0)
    errAbort("Window out of range on %s", chromName);

/* Do zoom/scroll if they hit it. */
if (cgiVarExists("left3"))
    relativeScroll(-0.95);
else if (cgiVarExists("left2"))
    relativeScroll(-0.475);
else if (cgiVarExists("left1"))
    relativeScroll(-0.1);
else if (cgiVarExists("right1"))
    relativeScroll(0.1);
else if (cgiVarExists("right2"))
    relativeScroll(0.475);
else if (cgiVarExists("right3"))
    relativeScroll(0.95);
else if (cgiVarExists("in3"))
    zoomAroundCenter(1.0/10.0);
else if (cgiVarExists("in2"))
    zoomAroundCenter(1.0/3.0);
else if (cgiVarExists("in1"))
    zoomAroundCenter(1.0/1.5);
else if (cgiVarExists("out1"))
    zoomAroundCenter(1.5);
else if (cgiVarExists("out2"))
    zoomAroundCenter(3.0);
else if (cgiVarExists("out3"))
    zoomAroundCenter(10.0);

/* Format position string. */
    {
    char buf[256];
    sprintf(buf, "%s:%d-%d", chromName, winStart+1, winEnd);
    position = cloneString(buf);
    }
/* Chuck code for synching with different frames */
otherFrame = cgiOptionalString("of");
thisFrame = cgiOptionalString("tf");

doForm();
}

void doDown()
{
printf("<H2>The Browser is Being Updated</H2>\n");
printf("The browser is currently unavailable.  We are in the process of\n");
printf("updating the database and the display software with a number of\n");
printf("new tracks, including some gene predictions.  Please try again tomorrow.\n");
}

int main(int argc, char *argv[])
{
htmlSetBackground("../images/floret.jpg");
htmShell("Working Draft Genome Browser v5", doMiddle, NULL);
//htmShell("Browser Being Updated", doDown, NULL);
return 0;
}

