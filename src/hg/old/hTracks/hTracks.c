/* This is the cgi-script behind the human chromosome 18 browser. */
#include "common.h"
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
#include "psl.h"
#include "agpFrag.h"
#include "agpGap.h"
#include "ctgPos.h"
#include "clonePos.h"
#include "genePred.h"
#include "glDbRep.h"
#include "spaceSaver.h"
#include "wormdna.h"

/* These variables persist from one incarnation of this program to the
 * next - living mostly in hidden variables. */
char *chromName;		/* Name of chromosome sequence . */
char *position; 		/* Name of position. */
int winStart;			/* Start of window in sequence. */
int winEnd;			/* End of window in sequence. */
boolean seqReverse;		/* Look at reverse strand. */

boolean withLeftLabels = TRUE;		/* Display left labels? */
boolean withCenterLabels = TRUE;	/* Display center labels? */

boolean calledSelf;			/* True if program called by page it generated. */

char *chromNames[] =
    {
    "chr18",
    };
int chromCount = 1;

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
font = tl.font = mgSmallFont();
tl.leftLabelWidth = 100;
tl.picWidth = 610;
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

/* Other global variables. */
int seqBaseCount;	/* Number of bases in sequence. */
int winBaseCount;	/* Number of bases in window. */

struct trackGroup
/* Structure that displays a group of tracks. */
    {
    struct trackGroup *next;   /* Next on list. */
    char *mapName;             /* Name on image map and for ui buttons. */
    enum trackVisibility visibility; /* How much of this to see if possible. */
    enum trackVisibility limitedVis; /* How much of this actually see. */

    char *longLabel;           /* Long label to put in center. */
    char *shortLabel;          /* Short label to put on side. */

    boolean ignoresColor;      /* True if doesn't use colors below. */
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

    void (*freeItems)(struct trackGroup *tg);
    /* Free item list. */

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
}

void mapBoxToggleVis(int x, int y, int width, int height, struct trackGroup *curGroup)
/* Print out image map rectangle that would invoke this program again.
 * program with the current track expanded. */
{
struct trackGroup *tg;

printf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
printf("HREF=\"../cgi-bin/hTracks?seqName=%s&old=%s&winStart=%d&winEnd=%d", 
	chromName, chromName, winStart, winEnd);
if (withLeftLabels)
    printf("&leftLabels=on");
if (withCenterLabels)
    printf("&centerLabels=on");
for (tg = tGroupList; tg != NULL; tg = tg->next)
    {
    int vis = tg->visibility;
    if (tg == curGroup)
	{
	if (vis == tvDense)
	    vis = tvFull;
	else if (vis == tvFull)
	    vis = tvDense;
	}
    printf("&%s=%s", tg->mapName, tvStrings[vis]);
    }
printf("\" ALT= \"Change between dense and full view of %s track\">\n", curGroup->shortLabel);
}


void mapBoxHc(int x, int y, int width, int height, char *group, char *item, char *statusLine)
/* Print out image map rectangle that would invoke the htc (human track click)
 * program. */
{
char *encodedItem = cgiEncode(item);
printf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
printf("HREF=\"../cgi-bin/hc?g=%s&i=%s&c=%s&l=%d&r=%d\" ", 
    group, encodedItem, chromName, winStart, winEnd);
printf("ALT= \"%s\">\n", statusLine); 
freeMem(encodedItem);
}


struct simpleFeature
/* Minimal feature - just stores position in browser coordinates. */
    {
    struct simpleFeature *next;
    int start, end;			/* Start/end in browser coordinates. */
    int grayIx;                         /* Level of gray usually. */
    };

struct linkedFeatures
    {
    struct linkedFeatures *next;
    int start, end;			/* Start/end in browser coordinates. */
    int grayIx;				/* Average of components. */
    char name[32];			/* Accession of query seq. */
    int orientation;                    /* Orientation. */
    struct simpleFeature *components;   /* List of component simple features. */
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

Color shadesOfGray[10];		/* 10 shades of gray from white to black. */
int maxShade = 9;

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
int maxShade = ArraySize(shadesOfGray)-1;
for (i=0; i<=maxShade; ++i)
    {
    struct rgbColor rgb;
    int level = 255 - (255*i/maxShade);
    if (level < 0) level = 0;
    rgb.r = rgb.g = rgb.b = level;
    shadesOfGray[i] = mgFindColor(mg, rgb.r, rgb.g, rgb.b);
    }
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

int mrnaGrayIx(int start, int end, int score)
/* Figure out gray level for an RNA block. */
{
int size = end-start;
int halfSize = (size>>1);
int res;
res =  (maxShade * score + halfSize)/size;
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
/* Draw contig items. */
{
int baseWidth = seqEnd - seqStart;
struct linkedFeatures *lf;
struct simpleFeature *sf;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2;
int midLineOff = heightPer/2;
boolean isFull = (vis == tvFull);
boolean grayScale = tg->ignoresColor;
Color bColor = tg->ixAltColor;

if (vis == tvDense)
    sortByGray(tg, vis);
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    int midY = y + midLineOff;
    int compCount = 0;
    int w;
    if (lf->components != NULL)
	{
	x1 = roundingScale(lf->start-winStart, width, baseWidth)+xOff;
	x2 = roundingScale(lf->end-winStart, width, baseWidth)+xOff;
	w = x2-x1;
	if (isFull)
	    {
	    if (grayScale) bColor =  shadesOfGray[(lf->grayIx>>1)];
	    mgBarbedHorizontalLine(mg, x1, midY, x2-x1, 2, 5, 
		    lf->orientation, bColor);
	    }
	if (grayScale) color =  shadesOfGray[lf->grayIx];
	mgDrawBox(mg, x1, midY, w, 1, color);
	}
    for (sf = lf->components; sf != NULL; sf = sf->next)
	{
	x1 = roundingScale(sf->start-winStart, width, baseWidth)+xOff;
	x2 = roundingScale(sf->end-winStart, width, baseWidth)+xOff;
	w = x2-x1;
	if (w < 1)
	    w = 1;
	if (grayScale) color =  shadesOfGray[lf->grayIx];
	mgDrawBox(mg, x1, y, w, heightPer, color);
	++compCount;
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
/* Convert from 0-255 representation to gray scale rep. */
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
	pt[i] = shadesOfGray[3];
    else if (b == 3)
	pt[i] = shadesOfGray[4];
    else if (b == 4)
	pt[i] = shadesOfGray[5];
    else if (b == 5)
	pt[i] = shadesOfGray[6];
    else if (b == 6)
	pt[i] = shadesOfGray[7];
    else if (b < 9)
	pt[i] = shadesOfGray[8];
    else
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
UBYTE *aveCounts;
int i;
int lineHeight = mgFontLineHeight(font);
struct simpleFeature *sf;
struct linkedFeatures *lf;
int s, e, w;
int log2 = digitsBaseTwo(baseWidth);
int shiftFactor = log2 - 18;
int sampleWidth;

if (shiftFactor < 0)
    shiftFactor = 0;
sampleWidth = (baseWidth>>shiftFactor);
AllocArray(useCounts, sampleWidth);
AllocArray(aveCounts, width);
memset(useCounts, 0, sampleWidth * sizeof(useCounts[0]));
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    for (sf = lf->components; sf != NULL; sf = sf->next)
	{
	s = ((sf->start - seqStart)>>shiftFactor);
	e = ((sf->end - seqStart)>>shiftFactor);
	if (s < 0) s = 0;
	if (e > sampleWidth) e = sampleWidth;
	w = e - s;
	if (w > 0)
	    incRange(useCounts+s, w);
	}
    }
resampleBytes(useCounts, sampleWidth, aveCounts, width);
grayThreshold(aveCounts, width);
for (i=0; i<lineHeight; ++i)
    mgPutSeg(mg, xOff, yOff+i, width, aveCounts);
freeMem(useCounts);
freeMem(aveCounts);
}

static void linkedFeaturesAverageDense(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw contig items. */
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
    lf->start = start;
    lf->end = end;
    }
lf->grayIx = lfCalcGrayIx(lf);
}

struct trackGroup *linkedFeaturesTg()
/* Return generic track group for linked features. */
{
struct trackGroup *tg = NULL;

AllocVar(tg);
tg->freeItems = freeLinkedFeaturesItems;
tg->drawItems = linkedFeaturesDraw;
tg->ignoresColor = TRUE;
tg->itemName = linkedFeaturesName;
tg->mapItemName = linkedFeaturesName;
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
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

struct linkedFeatures *lfFromPslsInRange(char *table, int start, int end)
/* Return linked features from range of table. */
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct linkedFeatures *lfList = NULL, *lf;

sprintf(query, "select * from %s where tStart<%u and tEnd>%u",
    table, winEnd, winStart);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct simpleFeature *sfList = NULL, *sf;
    struct psl *psl = pslLoad(row);
    unsigned *starts = psl->tStarts;
    unsigned *sizes = psl->blockSizes;
    int i, blockCount = psl->blockCount;
    int grayIx = mrnaGrayIx(psl->qStart, psl->qEnd, psl->match - psl->misMatch + psl->repMatch - psl->qNumInsert);

    AllocVar(lf);
    lf->grayIx = grayIx;
    strncpy(lf->name, psl->qName, sizeof(lf->name));
    lf->orientation = orientFromChar(psl->strand[0]);
    for (i=0; i<blockCount; ++i)
	{
	AllocVar(sf);
	sf->start = sf->end = starts[i];
	sf->end += sizes[i];
	sf->grayIx = grayIx;
	slAddHead(&sfList, sf);
	}
    slReverse(&sfList);
    lf->components = sfList;
    finishLf(lf);
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
tg->items = lfFromPslsInRange(table, winStart, winEnd);
}

struct trackGroup *fullMrnaTg()
/* Make track group of full length mRNAs. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgMrna";
tg->visibility = tvFull;
tg->longLabel = "Full Length mRNAs (Best in Genome)";
tg->shortLabel = "full mRNAs";
tg->loadItems = loadMrnaAli;
return tg;
}

void loadMrnaAli2(struct trackGroup *tg)
/* Load up rnas from table into trackGroup items. */
{
char table[64];
sprintf(table, "%s_mrna_old", chromName);
tg->items = lfFromPslsInRange(table, winStart, winEnd);
}

struct trackGroup *fullMrnaTg2()
/* Make track group of full length mRNAs. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgMrna2";
tg->visibility = tvFull;
tg->longLabel = "Full Length mRNAs (Near best in Chromosome)";
tg->shortLabel = "full mRNA 2";
tg->loadItems = loadMrnaAli2;
return tg;
}

void loadEstAli(struct trackGroup *tg)
/* Load up rnas from table into trackGroup items. */
{
char table[64];
sprintf(table, "%s_est", chromName);
tg->items = lfFromPslsInRange(table, winStart, winEnd);
}

struct trackGroup *estTg()
/* Make track group of full length mRNAs. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgEst";
tg->visibility = tvDense;
tg->longLabel = "Human ESTs";
tg->shortLabel = "Human ESTs";
tg->loadItems = loadEstAli;
tg->drawItems = linkedFeaturesAverageDense;
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
    slAddHead(&lfList, lf);
    genePredFree(&gp);
    }
slReverse(&lfList);
sqlFreeResult(&sr);
hFreeConn(&conn);
return lfList;
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
tg->visibility = tvFull;
tg->longLabel = "Genie Predictions Using mRNA/ESTs";
tg->shortLabel = "Genie+EST";
tg->loadItems = loadGenieAlt;
return tg;
}

void loadGenieStat(struct trackGroup *tg)
/* Load up Genie Stat genes. */
{
tg->items = lfFromGenePredInRange("genieStat", chromName, winStart, winEnd);
}

struct trackGroup *genieStatTg()
/* Make track group of full length mRNAs. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "genieStat";
tg->visibility = tvFull;
tg->longLabel = "Genie Predictions";
tg->shortLabel = "Genie";
tg->loadItems = loadGenieStat;
return tg;
}


void fragsLoad(struct trackGroup *tg)
/* Load up frags from table into trackGroup items. */
{
char table[64];
sprintf(table, "%s_frags", chromName);
tg->items = lfFromPslsInRange(table, winStart, winEnd);
}

struct trackGroup *fragsTrackGroup()
/* Make track group of full length mRNAs. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgFrags";
tg->visibility = tvFull;
tg->longLabel = "Assembly Fragments";
tg->shortLabel = "fragments";
tg->loadItems = fragsLoad;
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

/* Draw gaps if any. */
if (!isFull)
    {
    int midY = y + midLineOff;
    for (gap = tg->customPt; gap != NULL; gap = gap->next)
	{
	if (!sameWord(gap->bridge, "no"))
	    {
	    x1 = roundingScale(gap->chromStart-1-winStart, width, baseWidth)+xOff;
	    x2 = roundingScale(gap->chromEnd-winStart, width, baseWidth)+xOff;
	    w = x2-x1;
	    mgDrawBox(mg, x1, midY, w, 1, brown);
	    }
	}
    }

for (frag = tg->items; frag != NULL; frag = frag->next)
    {
    x1 = roundingScale(frag->chromStart-1-winStart, width, baseWidth)+xOff;
    x2 = roundingScale(frag->chromEnd-winStart, width, baseWidth)+xOff;
    w = x2-x1;
    if (isFull)
	color = gold;
    else
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

	mapBoxHc(x1,y,w,heightPer, tg->mapName, 
	    frag->frag, status);
	}
    ++ix;
    }
}

struct trackGroup *goldTrackGroup()
/* Make track group for golden path */
{
struct trackGroup *tg;

AllocVar(tg);
tg->mapName = "hgGold";
tg->visibility = tvDense;
tg->longLabel = "Assembly from Fragments (ooGreedy)";
tg->shortLabel = "assembly";
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

for (ctg = tg->items; ctg != NULL; ctg = ctg->next)
    {
    x1 = roundingScale(ctg->chromStart-1-winStart, width, baseWidth)+xOff;
    x2 = roundingScale(ctg->chromEnd-winStart, width, baseWidth)+xOff;
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
	mapBoxHc(x1,y,w,heightPer, tg->mapName, 
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

struct trackGroup *contigTrackGroup()
/* Make track group for contig */
{
struct trackGroup *tg;

AllocVar(tg);
tg->mapName = "hgContig";
tg->visibility = tvDense;
tg->longLabel = "Fingerprint Map Contigs";
tg->shortLabel = "contigs";
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
    int phase;			/* Htg phase - 1 2 or 3. */
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

static int cloneTotalHeight(struct trackGroup *tg, enum trackVisibility vis)
/* Most variable height track groups will use this to figure out the height they use. */
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
	    mapBoxHc(x1,y,w,heightPer, "hgClone", cfa->frag->name, fullPos);
	    }
	else
	    mapBoxHc(x1,y,w,heightPer, "hgClone", cfa->frag->name, cfa->frag->name);
	}
    }
freeHash(&dupeHash);
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
int shiftFactor = log2 - 18;
int sampleWidth;

if (shiftFactor < 0)
    shiftFactor = 0;
sampleWidth = (baseWidth>>shiftFactor);
AllocArray(useCounts, sampleWidth);
AllocArray(aveCounts, width);
memset(useCounts, 0, sampleWidth * sizeof(useCounts[0]));
for (ci = tg->items; ci != NULL; ci = ci->next)
    {
    for (cfa = ci->cfaList; cfa != NULL; cfa = cfa->next)
	{
	s = ((cfa->start - seqStart)>>shiftFactor);
	e = ((cfa->end - seqStart)>>shiftFactor);
	if (s < 0) s = 0;
	if (e > sampleWidth) e = sampleWidth;
	w = e - s;
	if (w > 0)
	    incRange(useCounts+s, w);
	}
    }
resampleBytes(useCounts, sampleWidth, aveCounts, width);
grayThreshold(aveCounts, width);
for (i=0; i<lineHeight; ++i)
    mgPutSeg(mg, xOff, yOff+i, width, aveCounts);
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


void cloneRealignmentLoad(struct trackGroup *tg)
/* Load up clone alignments from database tables and organize. */
{
loadRealiClonesInWindow();
tg->items = realiCloneList;
}

void cloneRealigmentFree(struct trackGroup *tg)
/* Free up clone track group items. */
{
cloneInfoFreeList(&realiCloneList);
freeHash(&realiCloneHash);
}


struct trackGroup *cloneRealignmentTrackGroup()
/* Make track group for clone alignments back to golden path */
{
struct trackGroup *tg;

AllocVar(tg);
tg->mapName = "hgReali";
tg->visibility = tvFull;
tg->longLabel = "Clone Fragment Realignments";
tg->shortLabel = "realignments";
tg->loadItems = cloneRealignmentLoad;
tg->freeItems = cloneRealigmentFree;
tg->drawItems = cloneDraw;
tg->altColor.r = 180;
tg->altColor.g = 180;
tg->altColor.b = 180;
tg->itemName = cloneName;
tg->mapItemName = cloneName;
tg->totalHeight = cloneTotalHeight;
tg->itemHeight = cloneItemHeight;
tg->subType =  TRUE;
return tg;
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
tg->shortLabel = "coverage";
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
return tg;
}


void gapLoad(struct trackGroup *tg)
/* Load up clone alignments from database tables and organize. */
{
struct linkedFeatures *lfList = NULL, *lf;
struct simpleFeature *sf;
char table[64];
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct agpGap agpGap;

sprintf(table, "%s_gap", chromName);
sprintf(query, "select * from %s where chromStart<%u and chromEnd>%u",
    table, winEnd, winStart);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char nbuf[128];
    agpGapStaticLoad(row, &agpGap);
    AllocVar(lf);
    AllocVar(sf);
    lf->start = sf->start = agpGap.chromStart;
    lf->end = sf->end = agpGap.chromEnd;
    lf->grayIx = sf->grayIx = maxShade;
    sprintf(lf->name, "%s_%s", agpGap.type, agpGap.bridge);
    lf->components = sf;
    slAddHead(&lfList, lf);
    }
slReverse(&lfList);
sqlFreeResult(&sr);
hFreeConn(&conn);
tg->items = lfList;
}


struct trackGroup *gapGroup()
/* Make track group for golden path positions of all frags. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgGap";
tg->visibility = tvDense;
tg->longLabel = "Gap Locations";
tg->shortLabel = "gap";
tg->loadItems = gapLoad;
return tg;
}

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

#ifdef SOMEDAY
void genieLoad(struct trackGroup *tg)
/* Get genie predictions and turn them into linked features. */
{
struct hgBac *bac = hgGetBac(chromName);
char query[256];
struct sqlConnection *conn = hgAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct linkedFeatures *lfList = NULL, *lf;

sprintf(query, 
    "select * from hgTranscript where startBac=%u and endPos>%d and startPos<%d",
    bac->id, winStart, winEnd);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct simpleFeature *sfList = NULL, *sf;
    struct hgTranscript *tn = hgTranscriptLoad(row);
    unsigned *starts = tn->exonStartPos;
    unsigned *ends = tn->exonEndPos;
    int i, exonCount = tn->exonCount;
    AllocVar(lf);
    lf->grayIx = maxShade;
    strncpy(lf->name, tn->name, sizeof(lf->name));
    lf->orientation = tn->orientation;
    for (i=0; i<exonCount; ++i)
	{
	AllocVar(sf);
	sf->start = starts[i];
	sf->end = ends[i];
	sf->grayIx = maxShade;
	slAddHead(&sfList, sf);
	}
    slReverse(&sfList);
    lf->components = sfList;
    finishLf(lf);
    slAddHead(&lfList, lf);
    hgTranscriptFree(&tn);
    }
slReverse(&lfList);
tg->items = lfList;
sqlFreeResult(&sr);
hgFreeConn(&conn);
}


struct trackGroup *genieTg()
/* Make track group of altGraph. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgTn";
tg->visibility = tvFull;
tg->longLabel = "Genie Gene Predictions";
tg->shortLabel = "Genie";
tg->loadItems = genieLoad;
tg->color.b = 130;
tg->altColor.r = tg->altColor.g = 128;
tg->altColor.b = 194;
tg->ignoresColor = FALSE;
return tg;
}
#endif /* SOMEDAY */

enum trackVisibility limitVisibility(struct trackGroup *tg)
/* Return default visibility limited by number of items. */
{
enum trackVisibility vis = tg->visibility;
if (vis != tvFull)
    return vis;
if (slCount(tg->items) <= 200)
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
pixHeight = border + rulerHeight;
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
		    mgTextCentered(mg, border, y, inWid, itemHeight, group->ixColor, font, name);
		    y += itemHeight;
		    }
		break;
	    case tvDense:
		if (withCenterLabels)
		    y += fontHeight;
		mgTextCentered(mg, border, y, inWid, group->lineHeight, group->ixColor, font, group->shortLabel);
		y += group->lineHeight;
		break;
	    }
        }
    }

/* Draw center labels. */
if (withCenterLabels)
    {
    int clWidth = insideWidth-openCloseHideWidth;
    int ochXoff = xOff + clWidth;
    mgSetClip(mg, xOff, border, insideWidth, pixHeight - 2*border);
    y = border;
    for (group = groupList; group != NULL; group = group->next)
        {
	if (group->limitedVis != tvHide)
	    {
	    Color color = group->ixColor;
	    mgTextCentered(mg, xOff, y+1, clWidth, insideHeight, color, font, group->longLabel);
	    mapBoxToggleVis(0,y+1,pixWidth,insideHeight,group);
#ifdef SOON
	    uglyf("ochXoff = %d, ochWidth = %d, pixWidth = %d", ochXoff, openCloseHideWidth, pixWidth);
	    mgTextCentered(mg, ochXoff, y+1, openCloseHideWidth, insideHeight, 
		color, font, "[open] [close] [hide]");
#endif
	    y += fontHeight;
	    y += group->height;
	    }
        }
    }

/* Draw tracks. */
y = 1;
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

/* Show ruler at bottom. */
y = pixHeight - rulerHeight;
mgSetClip(mg, xOff, y, insideWidth, rulerHeight);
relNumOff = winStart;
if (seqReverse)
    relNumOff = -relNumOff;
mgDrawRuler(mg, xOff, y, rulerHeight, insideWidth, MG_BLACK, font,
    relNumOff, winBaseCount);
mapBoxHc(xOff,y,insideWidth,rulerHeight, "getDna", "mixed", "Fetch DNA");

/* Make map background. */
    {
    y = border;
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
		    mapBoxHc(0,y,pixWidth,height, group->mapName, 
			group->mapItemName(group, item), 
			group->itemName(group, item));
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


void makeNumText(char *name, int num, int digits)
/* Make a text control filled with a number. */
{
printf("<INPUT TYPE=TEXT NAME=\"%s\" SIZE=%d VALUE=%d>", name, digits, num);
}

void makeText(char *name, char *initialVal, int chars)
/* Make a text control filled with initial value. */
{
printf("<INPUT TYPE=TEXT NAME=\"%s\" SIZE=%d VALUE=%s>", name, chars, initialVal);
}

void makeSubmitButton(char *name, char *value)
/* Make a submit button. */
{
printf("<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\">", name, value);
}

void makeDropList(char *name, char *menu[], int menuSize, char *checked)
/* Make a drop-down list. */
{
int i;
char *selString;
if (checked == NULL) checked = menu[0];
printf("<SELECT ALIGN=CENTER NAME=\"%s\">", name);
for (i=0; i<menuSize; ++i)
    {
    if (!differentWord(menu[i], checked))
        selString = " SELECTED";
    else
        selString = "";
    printf("<OPTION%s>%s</OPTION>", selString, menu[i]);
    }
printf("</SELECT>");
}

void makeCheckBox(char *name, boolean isChecked)
/* Create a checkbox with the given name in the given state. */
{
printf("<INPUT TYPE=CHECKBOX NAME=\"%s\" VALUE=on%s>", name,
    (isChecked ? " CHECKED" : "") );
}

void doForm()
/* Make the tracks display form with the zoom/scroll
 * buttons and the active image. */
{
struct trackGroup *group;

if (calledSelf)
    {
    /* Set center and left labels from UI. */
    withLeftLabels = cgiBoolean("leftLabels");
    withCenterLabels = cgiBoolean("centerLabels");
    }

/* Make list of all track groups. */
slSafeAddHead(&tGroupList, contigTrackGroup());
slSafeAddHead(&tGroupList, goldTrackGroup());
slSafeAddHead(&tGroupList, coverageTrackGroup());
slSafeAddHead(&tGroupList, gapGroup());
slSafeAddHead(&tGroupList, cloneRealignmentTrackGroup());
slSafeAddHead(&tGroupList, genieAltTg());
slSafeAddHead(&tGroupList, genieStatTg());
slSafeAddHead(&tGroupList, fullMrnaTg());
slSafeAddHead(&tGroupList, fullMrnaTg2());
slSafeAddHead(&tGroupList, estTg());
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
printf("<FORM ACTION=\"%shTracks\">\n\n", cgiDir());

/* Center everything from now on. */
printf("<CENTER>\n");

/* Show title . */
printf("<H2>%s</H2>", chromName);

/* Put up scroll and zoom controls. */
fputs("move ", stdout);
makeSubmitButton("left3", "<<<");
makeSubmitButton("left2", " <<");
makeSubmitButton("left1", " < ");
makeSubmitButton("right1", " > ");
makeSubmitButton("right2", ">> ");
makeSubmitButton("right3", ">>>");
fputs(" zoom in ", stdout);
makeSubmitButton("in1", "1.5x");
makeSubmitButton("in2", " 3x ");
makeSubmitButton("in3", "10x");
fputs(" zoom out ", stdout);
makeSubmitButton("out1", "1.5x");
makeSubmitButton("out2", " 3x ");
makeSubmitButton("out3", "10x");
fputs("<BR>\n", stdout);

/* Make line that says position. */
    {
    fputs("position ", stdout);
    makeText("position", position, 30);
    makeSubmitButton("submit", "jump");
    fputs("<BR>\n", stdout);
    }

/* Make clickable image and map. */
makeActiveImage(tGroupList);
fputs("Click on image to view more information on a gene or alignment. Click on ruler for DNA<BR>", stdout);

/* Display bottom control panel. */
htmlHorizontalLine();
fputs("Chromosome ", stdout);
makeDropList("seqName", chromNames, chromCount, chromName);
fputs(" bases ",stdout);
makeNumText("winStart", winStart, 12);
fputs(" - ", stdout);
makeNumText("winEnd", winEnd, 12);
printf("<B>Labels:</B> ");
printf("left ");
makeCheckBox("leftLabels", withLeftLabels);
printf("center ");
makeCheckBox("centerLabels", withCenterLabels);
printf(" ");
makeSubmitButton("submit", "refresh");
printf("<BR>\n");


/* Display viewing options for each group. */
printf("<B>Track Controls:</B> ");
for (group = tGroupList; group != NULL; group = group->next)
    {
    printf(" %s ", group->shortLabel);
    makeDropList(group->mapName, tvStrings, ArraySize(tvStrings), tvStrings[group->visibility]);
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


char *isHumanChrom(char *chrom)
/* Returns "cannonical" name of chromosome or NULL
 * if not a chromosome. */
{
int i;
for (i=0; i<chromCount; ++i)
    if (sameWord(chromNames[i], chrom))
	return chromNames[i];
return NULL;
}

boolean parseHumanChromRange(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Parse something of form chrom:start-end into pieces. */
{
char *chrom, *start, *end;
char buf[256];
strncpy(buf, spec, sizeof(buf));
chrom = buf;
start = strchr(chrom, ':');
if (start == NULL)
    return FALSE;
else 
    *start++ = 0;
end = strchr(start, '-');
if (end == NULL)
    return FALSE;
else
    *end++ = 0;
chrom = trimSpaces(chrom);
start = trimSpaces(start);
end = trimSpaces(end);
if (!isdigit(start[0]))
    return FALSE;
if (!isdigit(end[0]))
    return FALSE;
if ((chrom = isHumanChrom(chrom)) == NULL)
    return FALSE;
if (retChromName != NULL)
    *retChromName = chrom;
if (retWinStart != NULL)
    *retWinStart = atoi(start);
if (retWinEnd != NULL)
    *retWinEnd = atoi(end);
return TRUE;
}

boolean isHumanChromRange(char *spec)
/* Returns TRUE if spec is chrom:N-M for some human
 * chromosome chrom and some N and M. */
{
return parseHumanChromRange(spec, NULL, NULL, NULL);
}

void findContigPos(char *contig, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Find position in genome of contig.  Don't alter
 * return variables if some sort of error. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(256);
char **row;
struct ctgPos *ctgPos;
dyStringPrintf(query, "select * from ctgPos where contig = '%s'", contig);
sr = sqlMustGetResult(conn, query->string);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Couldn't find contig %s", contig);
ctgPos = ctgPosLoad(row);
*retChromName = isHumanChrom(ctgPos->chrom);
*retWinStart = ctgPos->chromStart;
*retWinEnd = ctgPos->chromEnd;
ctgPosFree(&ctgPos);
freeDyString(&query);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

boolean isContigName(char *contig)
/* REturn TRUE if a FPC contig name. */
{
return startsWith("ctg", contig);
}

boolean isAccForm(char *s)
/* Returns TRUE if s is of format to be a genbank accession. */
{
int len = strlen(s);
if (len < 6 || len > 10)
    return FALSE;
if (!isalpha(s[0]))
    return FALSE;
if (!isdigit(s[len-1]))
    return FALSE;
return TRUE;
}

boolean findClonePos(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Return clone position. */
{
if (!isAccForm(spec))
    return FALSE;
else
    {
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr = NULL;
    struct dyString *query = newDyString(256);
    char **row;
    boolean ok = FALSE;
    struct clonePos *clonePos;
    dyStringPrintf(query, "select * from clonePos where name like '%s%%'", spec);
    sr = sqlMustGetResult(conn, query->string);
    row = sqlNextRow(sr);
    if (row != NULL)
	{
	clonePos = clonePosLoad(row);
	*retChromName = isHumanChrom(clonePos->chrom);
	*retWinStart = clonePos->chromStart;
	*retWinEnd = clonePos->chromEnd;
	clonePosFree(&clonePos);
	ok = TRUE;
	}
    freeDyString(&query);
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    return ok;
    }
}


void findGenomePos(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Find position in genome of spec.  Don't alter
 * return variables if some sort of error. */
{
spec = trimSpaces(spec);
if (isHumanChromRange(spec))
    {
    parseHumanChromRange(spec, retChromName, retWinStart, retWinEnd);
    }
else if (isContigName(spec))
    {
    findContigPos(spec, retChromName, retWinStart, retWinEnd);
    }
else if (findClonePos(spec, retChromName, retWinStart, retWinEnd))
    {
    }
else
    {
    errAbort("Sorry, couldn't locate %s in genome database\n", spec);
    }
}

void doMiddle()
/* Print the body of an html file.  This routine handles zooming and
 * scrolling. */
{
char *oldSeq;
boolean testing = FALSE;

pushCarefulMemHandler(32*1024*1024);
/* Initialize layout. */
initTl();

/* Read in input from CGI. */
oldSeq = cgiOptionalString("old");
if (oldSeq != NULL)
   calledSelf = TRUE; 
chromName = cgiString("seqName");
winStart = cgiInt("winStart");
winEnd = cgiInt("winEnd");
position = cgiOptionalString("position");
if (position == NULL)
    position = "";
if (position != NULL && position[0] != 0)
    {
    findGenomePos(position, &chromName, &winStart, &winEnd);
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
    sprintf(buf, "%s:%d-%d", chromName, winStart, winEnd);
    position = cloneString(buf);
    }
doForm();
}

int main(int argc, char *argv[])
{
htmShell("GAP Browser", doMiddle, NULL);
}
