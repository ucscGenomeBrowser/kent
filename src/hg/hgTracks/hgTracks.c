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
#include "vGfx.h"
#include "browserGfx.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"
#include "hdb.h"
#include "hui.h"
#include "hgFind.h"
#include "spaceSaver.h" 
#include "wormdna.h"
#include "aliType.h"
#include "psl.h"
#include "agpFrag.h"
#include "agpGap.h"
#include "cgh.h"
#include "ctgPos.h"
#include "clonePos.h"
#include "bactigPos.h"
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
#include "fishClones.h"
#include "stsMarker.h"
#include "stsMap.h"
#include "recombRate.h"
#include "chr18deletions.h"
#include "mouseOrtho.h"
#include "humanParalog.h"
#include "synteny100000.h"
#include "mouseSyn.h"
#include "mouseSynWhd.h"
#include "ensPhusionBlast.h"
#include "syntenyBerk.h"
#include "syntenySanger.h"
#include "netAlign.h"
#include "netGap.h"
#include "chainBlock.h"
#include "chain.h"
#include "chainGap.h"
#include "knownMore.h"
#include "estPair.h"
#include "customTrack.h"
#include "trackDb.h"
#include "pslWScore.h"
#include "lfs.h"
#include "mcnBreakpoints.h"
#include "expRecord.h"
#include "altGraph.h"
#include "geneGraph.h"
#include "sample.h"
#include "genMapDb.h"
#include "altGraphX.h"
#include "genomicSuperDups.h"
#include "celeraDupPositive.h"
#include "celeraCoverage.h"
#include "web.h"

#define MAX_CONTROL_COLUMNS 5
#define NAME_LEN 256
#define EXPR_DATA_SHADES 16
#define CHROMOSOME_SHADES 4
#define LOW 1
#define MEDIUM 2
#define BRIGHT 3
#define MAXPIXELS 14000
boolean hgDebug = FALSE;      /* Activate debugging code. Set to true by hgDebug=on in command line*/
int imagePixelHeight = 0;
int colorBin[MAXPIXELS][256]; /* count of colors for each pixel for each color */
/* Declare our color gradients and the the number of colors in them */
Color shadesOfGreen[EXPR_DATA_SHADES];
Color shadesOfRed[EXPR_DATA_SHADES];
Color shadesOfBlue[EXPR_DATA_SHADES];
boolean exprBedColorsMade = FALSE; /* Have the shades of Green, Red, and Blue been allocated? */
int maxRGBShade = EXPR_DATA_SHADES - 1;

/* Declare colors for chromosome coloring 8 colors * 3 shades = 24 colors allocated */
boolean chromosomeColorsMade = FALSE; /* Have the 3 shades of 8 chromosome colors been allocated? */

Color chromColor[CHROMOSOME_SHADES * 8];

int z;
int maxCount;
int bestColor;
int maxChromShade = CHROMOSOME_SHADES - 1;
int maxItemsInFullTrack = 300;  /* Maximum number of items displayed in full */
int guidelineSpacing = 10;	/* Pixels between guidelines. */

struct cart *cart;	/* The cart where we keep persistent variables. */

/* These variables persist from one incarnation of this program to the
 * next - living mostly in the cart. */
char *chromName;		/* Name of chromosome sequence . */
char *database;			/* Name of database we're using. */
char *organism;			/* Name of organism we're working on. */
char *position = NULL; 		/* Name of position. */
int winStart;			/* Start of window in sequence. */
int winEnd;			/* End of window in sequence. */
char *userSeqString = NULL;	/* User sequence .fa/.psl file. */
char *ctFileName = NULL;	/* Custom track file. */

/* These variables are set by getPositionFromCustomTracks() at the very 
 * beginning of tracksDisplay(), and then used by loadCustomTracks(). */
struct customTrack *ctList = NULL;  /* Custom tracks. */
struct slName *browserLines = NULL; /* Custom track "browser" lines. */

boolean withLeftLabels = TRUE;		/* Display left labels? */
boolean withCenterLabels = TRUE;	/* Display center labels? */
boolean withGuidelines = TRUE;		/* Display guidelines? */
boolean withPopUps = TRUE;		/* Display PopUps? */
boolean withRuler = TRUE;		/* Display ruler? */
boolean hideControls = FALSE;		/* Hide all controls? */

/* Structure returned from findGenomePos. 
We use this to to expand any tracks to full
 that were found to contain the searched-upon
position string */
struct hgPositions *hgp = NULL;

struct hash *zooSpeciesHash = NULL;

struct trackLayout
/* This structure controls the basic dimensions of display. */
    {
    MgFont *font;		/* What font to use. */
    int leftLabelWidth;		/* Width of left labels. */
    int trackWidth;		/* Width of tracks. */
    int picWidth;		/* Width of entire picture. */
    } tl;

boolean suppressHtml = FALSE;	
	/* If doing PostScript output we'll suppress most
         * of HTML output. */

void hPrintf(char *format, ...)
/* Printf that can be suppressed if not making
 * html. */
{
va_list(args);
va_start(args, format);
if (!suppressHtml)
    {
    vprintf(format, args);
    }
va_end(args);
}

void hPuts(char *string)
/* Puts that can be suppressed if not making
 * html. */
{
if (!suppressHtml)
    puts(string);
}

void hPutc(char c)
/* putc than can be suppressed if not makeing html. */
{
if (!suppressHtml)
    fputc('\n', stdout);
}

void hWrites(char *string)
/* Write string with no '\n' if not suppressed. */
{
if (!suppressHtml)
    fputs(string, stdout);
}

void hButton(char *name, char *label)
/* Write out button if not suppressed. */
{
if (!suppressHtml)
    cgiMakeButton(name, label);
}

void hTextVar(char *varName, char *initialVal, int charSize)
/* Write out text entry field if not suppressed. */
{
if (!suppressHtml)
    cgiMakeTextVar(varName, initialVal, charSize);
}

void hIntVar(char *varName, int initialVal, int maxDigits)
/* Write out numerical entry field if not supressed. */
{
if (!suppressHtml)
    cgiMakeIntVar(varName, initialVal, maxDigits);
}

void hCheckBox(char *varName, boolean checked)
/* Make check box if not suppressed. */
{
if (!suppressHtml)
    cgiMakeCheckBox(varName, checked);
}

void hDropList(char *name, char *menu[], int menuSize, char *checked)
/* Make a drop-down list with names if not suppressed. */
{
if (!suppressHtml)
    cgiMakeDropList(name, menu, menuSize, checked);
}

void printHtmlComment(char *comment)
/* Function to print output as a comment so it is not seen in the HTML
 * output but only in the HTML source
 * param comment _ The comment to be printed */
{
hPrintf("\n<!-- DEBUG: %s -->\n", comment);
//fflush(stdout); /* USED ONLY FOR DEBUGGING BECAUSE THIS IS SLOW - MATT */
}

void setPicWidth(char *s)
/* Set pixel width from ascii string. */
{
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

void initTl()
/* Initialize layout around small font and a picture about 600 pixels
 * wide. */
{
MgFont *font;
char *s;

font = tl.font = mgSmallFont();
tl.leftLabelWidth = 120;
tl.picWidth = 620;
setPicWidth(cartOptionalString(cart, "pix"));
}


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
static struct rgbColor guidelineColor = { 220, 220, 255};

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
    boolean limitedVisSet;	     /* Is limited visibility set? */

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
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis);
    /* Draw item list, one per track. */

    int (*itemStart)(struct trackGroup *tg, void *item);
    /* Return start of item in base pairs. */

    int (*itemEnd)(struct trackGroup *tg, void *item);
    /* Return start of item in base pairs. */

    void (*freeItems)(struct trackGroup *tg);
    /* Free item list. */

    Color (*itemColor)(struct trackGroup *tg, void *item, struct vGfx *vg);
    /* Get color of item (optional). */


    void (*mapItem)(struct trackGroup *tg, void *item, char *itemName, int start, int end, 
		    int x, int y, int width, int height); 
    /* Write out image mapping for a given item */

    boolean hasUi;		/* True if has an extended UI page. */
    void *extraUiData;		/* Pointer for track specific filter etc. data. */
    void (*trackFilter)(struct trackGroup *tg);	
    /* Stuff to handle user interface parts. */

    void *customPt;            /* Misc pointer variable unique to group. */
    int customInt;             /* Misc int variable unique to group. */
    int subType;               /* Variable to say what subtype this is for similar groups
                                * to share code. */
    unsigned short private;	/* True(1) if private, false(0) otherwise. */
    int bedSize;		/* Number of fields if a bed file. */
    float priority;	/* Priority to load tracks in, i.e. order to load tracks in. */
};

void loadSampleIntoLinkedFeature(struct trackGroup *tg);
void loadSampleZoo(struct trackGroup *tg);

struct trackGroup *tGroupList = NULL;  /* List of all tracks. */

struct trackGroup *sortGroupList = NULL; /* Used temporarily for sample sorting. */
static boolean tgLoadNothing(){return TRUE;}
static void tgDrawNothing(){}
static void tgFreeNothing(){}
static char *tgNoName(){return"";}

int tgCmpPriority(const void *va, const void *vb)
/* Compare to sort based on priority. */
{
const struct trackGroup *a = *((struct trackGroup **)va);
const struct trackGroup *b = *((struct trackGroup **)vb);
float dif = a->priority - b->priority;
if (dif < 0)
   return -1;
else if (dif == 0.0)
   return 0;
else
   return 1;
}

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


int updateY( char *name, char *nextName, int lineHeight )
/* only increment height when name root (minus the period if
 * there is one) is different from previous one.
  *This assumes that the entries are sorted by name as they would
  *be if loaded by hgLoadSample*/
{
int ret;
char *tempstr = NULL;
char *tempstr2 = NULL;

//return(0);

tempstr = cloneString( name );
if( strstr( name, "." ) != NULL )
   strtok( tempstr, "." );

tempstr2 = cloneString( nextName );
if( strstr( nextName, "." ) != NULL )
   strtok( tempstr2, "." );

if( !sameString( tempstr, tempstr2 )) 
   ret = lineHeight; 
else 
   ret = 0;

freeMem(tempstr);
freeMem(tempstr2);

return(ret);
}

static int tgUserDefinedTotalHeight(struct trackGroup *tg, enum trackVisibility vis)
/* Wiggle track groups will use this to figure out the height they use
as defined in the cart */
{

struct slList *item;
int start, lines;

int heightFromCart;
char o1[128];

snprintf( o1, 128, "%s.heightPer", tg->mapName);

if( vis == tvDense )
    heightFromCart = 10;
else
    heightFromCart = atoi(cartUsualString(cart, o1, "50"));

tg->lineHeight = max(mgFontLineHeight(tl.font)+1, heightFromCart);
tg->heightPer = tg->lineHeight - 1;
switch (vis)
    {
    case tvFull:

    lines = 1;
    for (item = tg->items; item != NULL; item = item->next)
	{
        if( item->next != NULL )
            if( updateY( tg->itemName(tg, item), tg->itemName(tg, item->next), 1 ))
                lines++;
    }
	tg->height = lines * tg->lineHeight;
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

enum trackVisibility limitVisibility(struct trackGroup *tg, void *items)
/* Return default visibility limited by number of items. */
{
if (!tg->limitedVisSet)
    {
    enum trackVisibility vis = tg->visibility;
    tg->limitedVisSet = TRUE;
    if (vis == tvFull)
	{
	if (slCount(items) > maxItemsInFullTrack)
	    vis = tvDense;
	}
    tg->limitedVis = vis;
    }
return tg->limitedVis;
}

static struct dyString *uiStateUrlPart(struct trackGroup *toggleGroup)
/* Return a string that contains all the UI state in CGI var
 * format.  If toggleGroup is non-null the visibility of that
 * group will be toggled in the string. */
{
struct dyString *dy = newDyString(512);
struct trackGroup *tg;

dyStringPrintf(dy, "%s=%u", cartSessionVarName(), cartSessionId(cart));
for (tg = tGroupList; tg != NULL; tg = tg->next)
    {
    int vis = tg->visibility;
    if (tg == toggleGroup)
	{
	if (vis == tvDense)
	    vis = tvFull;
	else if (vis == tvFull)
	    vis = tvDense;
	dyStringPrintf(dy, "&%s=%s", tg->mapName, hStringFromTv(vis));
	}
    }
return dy;
}


void mapBoxTrackUi(int x, int y, int width, int height, struct trackGroup *tg)
/* Print out image map rectangle that invokes hgTrackUi. */
{
char *track = tg->mapName;
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
hPrintf("HREF=\"%s?%s=%u&c=%s&g=%s\"", hgTrackUiName(), 
	    cartSessionVarName(), cartSessionId(cart),
	    chromName, tg->mapName);
if( withPopUps ) 
    hPrintf(" onMouseOver=\"javascript:popup('%s controls');\" onMouseOut=\"javascript:popupoff();\"", tg->shortLabel);
hPrintf(">\n");
}

void mapBoxReinvoke(int x, int y, int width, int height, 
	struct trackGroup *toggleGroup, char *chrom,
	int start, int end, char *message)
/* Print out image map rectangle that would invoke this program again.
 * If toggleGroup is non-NULL then toggle that track between full and dense.
 * If chrom is non-null then jump to chrom:start-end. */
{
struct dyString *ui = uiStateUrlPart(toggleGroup);

hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
if (chrom == NULL)
    {
    chrom = chromName;
    start = winStart;
    end = winEnd;
    }
hPrintf("HREF=\"%s?position=%s:%d-%d",
	hgTracksName(), chrom, start+1, end);
hPrintf("&%s\"", ui->string);
freeDyString(&ui);

/*if (toggleGroup && withPopUps)
        hPrintf(" onMouseOver=\"javascript:popup('Change between dense and full view of %s track');\" onMouseOut=\"javascript:popupoff();\"", toggleGroup->shortLabel);
else
    if( withPopUps ) 
        hPrintf(" onMouseOver=\"javascript:popup('%s');\" onMouseOut=\"javascript:popupoff();\"",message);*/
hPrintf(">\n");
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


char *hgcNameAndSettings()
/* Return path to hgc with variables to store UI settings. */
{
static struct dyString *dy = NULL;
if (dy == NULL)
    {
    dy = newDyString(128);
    dyStringPrintf(dy, "%s?%s", hgcName(), cartSidUrlString(cart));
    }
return dy->string;
}

void mapBoxHc(int start, int end, int x, int y, int width, int height, 
	char *group, char *item, char *statusLine)
/* Print out image map rectangle that would invoke the htc (human track click)
 * program. */
{
char *encodedItem = cgiEncode(item);
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
hPrintf("HREF=\"%s&o=%d&t=%d&g=%s&i=%s&c=%s&l=%d&r=%d&db=%s&pix=%d\" ", 
    hgcNameAndSettings(), start, end, group, encodedItem, chromName, winStart, winEnd, 
    database, tl.picWidth);
/*if (start !=-1)*/
if( withPopUps ) 
    hPrintf(" onMouseOver=\"javascript:popup('%s');\" onMouseOut=\"javascript:popupoff();\"",statusLine);
hPrintf(">\n");
freeMem(encodedItem);
}

boolean chromTableExists(char *tabSuffix)
/* Return true if chromosome specific version of table exists. */
{
char table[256];
sprintf(table, "%s%s", chromName, tabSuffix);
return hTableExists(table);
}

void drawScaledBox(struct vGfx *vg, int chromStart, int chromEnd, 
	double scale, int xOff, int y, int height, Color color)
/* Draw a box scaled from chromosome to window coordinates. */
{
int x1 = round((double)(chromStart-winStart)*scale) + xOff;
int x2 = round((double)(chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
if (w < 1)
    w = 1;
vgBox(vg, x1, y, w, height, color);
}

void drawScaledBoxSample(struct vGfx *vg, int chromStart, int chromEnd, double scale, 
	int xOff, int y, int height, Color color, int score)
/* Draw a box scaled from chromosome to window coordinates. */
{
int i;
int x1, x2, w;

x1 = round((double)(chromStart-winStart)*scale) + xOff;
x2 = round((double)(chromEnd-winStart)*scale) + xOff;
if (x2 >= MAXPIXELS)
    x2 = MAXPIXELS - 1;
w = x2-x1;

if (w < 1)
    w = 1;
vgBox(vg, x1, y, w, height, color);
if ((x1 >= 0) && (x1 < MAXPIXELS) && (chromEnd >= winStart) && (chromStart <= winEnd))
    {
    for (i = x1 ; i < x1+w; i++)
        {
        assert(i<MAXPIXELS);
        z = colorBin[i][color] ;  /*pick color of highest scoreing alignment  for this pixel */
        colorBin[i][color] = (z > score)? z : score;
        }
    }
}


void filterItems(struct trackGroup *tg, 
    boolean (*filter)(struct trackGroup *tg, void *item),
    char *filterType)
/* Filter out items from trackGroup->itemList. */
{
struct slList *newList = NULL, *oldList = NULL, *el, *next;
boolean exclude = FALSE;
boolean color = FALSE;
boolean isFull;

if (sameWord(filterType, "none"))
    return;

isFull = (limitVisibility(tg, tg->items) == tvFull);
if (sameWord(filterType, "include"))
    exclude = FALSE;
else if (sameWord(filterType, "exclude"))
    exclude = TRUE;
else
    color = TRUE;

for (el = tg->items; el != NULL; el = next)
    {
    next = el->next;
    if (filter(tg, el) ^ exclude)
        {
	slAddHead(&newList, el);
	}
    else
        {
	slAddHead(&oldList, el);
	}
    }
slReverse(&newList);
if (color)
   {
   slReverse(&oldList);
   /* Draw stuff that passes filter first in full mode, last in dense. */
   if (isFull)
       newList = slCat(newList, oldList);
   else
       newList = slCat(oldList, newList);
   }
tg->items = newList;
}

int getFilterColor(char *type, int colorIx)
/* Get color corresponding to filter. */
{
/* int colorIx = MG_BLACK;*/
if (sameString(type, "red"))
    colorIx = MG_RED;
else if (sameString(type, "green"))
    colorIx = MG_GREEN;
else if (sameString(type, "blue"))
    colorIx = MG_BLUE;
return colorIx;
}

struct simpleFeature
/* Minimal feature - just stores position in browser coordinates. */
    {
    struct simpleFeature *next;
    int start, end;			/* Start/end in browser coordinates. */
    int grayIx;                         /* Level of gray usually. */
    };

enum {lfSubXeno = 1};
enum {lfSubSample = 2};
enum {lfWithBarbs = 3}; /* turn on barbs to show direction based on strand field */

struct linkedFeatures
{
    struct linkedFeatures *next;
    int start, end;			/* Start/end in browser coordinates. */
    int tallStart, tallEnd;		/* Start/end of fat display. */
    int grayIx;				/* Average of components. */
    int filterColor;			/* Filter color (-1 for none) */
    float score;                        /* score for this feature */
    char name[NAME_LEN];		/* Accession of query seq. */
    int orientation;                    /* Orientation. */
    struct simpleFeature *components;   /* List of component simple features. */
    void *extra;			/* Extra info that varies with type. */
    char popUp[NAME_LEN];		/* text for popup */
    };

struct linkedFeaturesPair
    {
      struct linkedFeaturesPair *next;
      char *cloneName;                /*clone name for est pair */
      struct linkedFeatures *lf5prime;   /*linked features for 5 prime est */
      struct linkedFeatures *lf3prime;   /*linked features for 5 prime est */
    };

int linkedFeaturesCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
return a->start - b->start;
}


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


struct linkedFeaturesSeries
/* series of linked features that are comprised of multiple linked features */
{
    struct linkedFeaturesSeries *next; 
    char *name;                      /* name for series of linked features */
    int start, end;                     /* Start/end in browser coordinates. */
    int orientation;                    /* Orientation. */
    int grayIx;				/* Gray index (average of features) */
    boolean noLine;                     /* if true don't draw line connecting features */
    struct linkedFeatures *features;    /* linked features for a series */
};

char *linkedFeaturesSeriesName(struct trackGroup *tg, void *item)
/* Return name of item */
{
struct linkedFeaturesSeries *lfs = item;
return lfs->name;
}

void freeLinkedFeaturesSeries(struct linkedFeaturesSeries **pList)
/* Free up a linked features series list. */
{
struct linkedFeaturesSeries *lfs;

for (lfs = *pList; lfs != NULL; lfs = lfs->next)
    freeLinkedFeatures(&lfs->features);
slFreeList(pList);
}

void freeLinkedFeaturesSeriesItems(struct trackGroup *tg)
/* Free up linkedFeaturesSeriesTrack items. */
{
freeLinkedFeaturesSeries((struct linkedFeaturesSeries**)(&tg->items));
}

void linkedFeaturesToLinkedFeaturesSeries(struct trackGroup *tg)
/* Convert a linked features struct to a linked features series struct */
{
struct linkedFeaturesSeries *lfsList = NULL, *lfs;
struct linkedFeatures *lf;

for (lf = tg->items; lf != NULL; lf = lf->next) 
    { 
    AllocVar(lfs);
    lfs->features = lf;
    lfs->grayIx = lf->grayIx;
    lfs->start = lf->start;
    lfs->end = lf->end;
    slAddHead(&lfsList, lfs);
    }
slReverse(&lfsList);
for (lfs = lfsList; lfs != NULL; lfs = lfs->next) 
    lfs->features->next = NULL;
tg->items = lfsList;
}

void linkedFeaturesSeriesToLinkedFeatures(struct trackGroup *tg)
/* Convert a linked features series struct to a linked features struct */
{
struct linkedFeaturesSeries *lfs;
struct linkedFeatures *lfList = NULL, *lf;

for (lfs = tg->items; lfs != NULL; lfs = lfs->next) 
    {
    slAddHead(&lfList, lfs->features);
    lfs->features = NULL;
    }
slReverse(&lfList);
freeLinkedFeaturesSeriesItems(tg);
tg->items = lfList;
}


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

int vgFindRgb(struct vGfx *vg, struct rgbColor *rgb)
/* Find color index corresponding to rgb color. */
{
return vgFindColorIx(vg, rgb->r, rgb->g, rgb->b);
}

void makeGrayShades(struct vGfx *vg)
/* Make eight shades of gray in display. */
{
int i;
for (i=0; i<=maxShade; ++i)
    {
    struct rgbColor rgb;
    int level = 255 - (255*i/maxShade);
    if (level < 0) level = 0;
    rgb.r = rgb.g = rgb.b = level;
    shadesOfGray[i] = vgFindRgb(vg, &rgb);
    }
shadesOfGray[maxShade+1] = MG_RED;
}

void vgMakeColorGradient(struct vGfx *vg, 
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
    colorIxs[i] = vgFindColorIx(vg, r, g, b);
    scale += invStep;
    }
}

void makeBrownShades(struct vGfx *vg)
/* Make some shades of brown in display. */
{
vgMakeColorGradient(vg, &tanColor, &brownColor, maxShade+1, shadesOfBrown);
}

void makeSeaShades(struct vGfx *vg)
/* Make some shades of blue in display. */
{
vgMakeColorGradient(vg, &lightSeaColor, &darkSeaColor, maxShade+1, shadesOfSea);
}

void makeRedGreenShades(struct vGfx *vg) 
/* Allocate the  shades of Red, Green and Blue */
{
static struct rgbColor black = {0, 0, 0};
static struct rgbColor red = {255, 0, 0};
static struct rgbColor green = {0, 255, 0};
static struct rgbColor blue = {0, 0, 255};
vgMakeColorGradient(vg, &black, &blue, EXPR_DATA_SHADES, shadesOfBlue);
vgMakeColorGradient(vg, &black, &red, EXPR_DATA_SHADES, shadesOfRed);
vgMakeColorGradient(vg, &black, &green, EXPR_DATA_SHADES, shadesOfGreen);
exprBedColorsMade = TRUE;
}

void makeChromosomeShades(struct vGfx *vg) 
/* Allocate the  shades of 8 colors in 3 shades to cover 24 chromosomes  */
{
static struct rgbColor blk = {0, 0, 0};
static struct rgbColor white = {255, 255, 255};
static struct rgbColor chrom1 = {255, 0, 0};  /* red */
static struct rgbColor chrom2 = {255, 153, 0}; /* orange */
static struct rgbColor chrom3 = {255, 255, 0}; /* yellow */
static struct rgbColor chrom4 = {0, 255, 0}; /* green */
static struct rgbColor chrom5 = {0, 255, 255}; /* cyan? aka lt blue */
static struct rgbColor chrom6 = {0, 0, 255}; /* blue */
static struct rgbColor chrom7 = {255, 0, 255}; /* magenta aka purple */
static struct rgbColor chrom8 = {102, 51, 0}; /* brown */

chromColor[0] = vgFindColorIx(vg, 0,0, 0); /*black*/
chromColor[1] = vgFindColorIx(vg, 0xff,0xcc, 0xcc);  /* light red */
chromColor[2] = vgFindColorIx(vg, 0xcc,0, 0);      /* med red */
chromColor[3] = vgFindColorIx(vg, 0xff,0, 0);      /* bright red */

chromColor[4] = vgFindColorIx(vg, 0xff,0x66,0);     /* bright orange */
chromColor[5] = vgFindColorIx(vg, 0xff,0x99,0);
chromColor[6] = vgFindColorIx(vg, 0xff,0,0xcc);     /* magenta */

chromColor[7] = vgFindColorIx(vg, 0xff,0xff,0x0);     /* bright yellow  */
chromColor[8] = vgFindColorIx(vg, 0xcc,0xcc,0x99);     /* olive green*/
chromColor[9] = vgFindColorIx(vg, 0x99,0x66,0x0);   /* brown */

chromColor[10] = vgFindColorIx(vg, 0,0xff,0);      /*bt gr*/
chromColor[11] = vgFindColorIx(vg, 0xcc,0xff,0);    /* yellow green */
chromColor[12] = vgFindColorIx(vg, 0x66,0x66,0);  /* dark  green*/

chromColor[13] = vgFindColorIx(vg, 0xcc,0xff,0xff);  /*lt cyan*/
chromColor[14] = vgFindColorIx(vg, 0,0xff,0xff);    /*med cyan*/
chromColor[15] = vgFindColorIx(vg, 0x0,0x0,0x0); /* black */

chromColor[16] = vgFindColorIx(vg, 0x99,0xcc,0xff);  /*light med blue */
chromColor[17] = vgFindColorIx(vg, 0x66,0x99,0xff);  /* med blue */
chromColor[18] = vgFindColorIx(vg, 0,0 ,0xcc);     /* deep blue */

chromColor[19] = vgFindColorIx(vg, 0xcc,0x99,0xff);  /*lt violet*/
chromColor[20] = vgFindColorIx(vg, 0xcc,0x33,0xff);  /* med violet */
chromColor[21] = vgFindColorIx(vg, 0x99,0,0xcc);    /* dark violet */

chromColor[22] = vgFindColorIx(vg, 0xcc,0xcc,0xcc); /* light gray */
chromColor[23] = vgFindColorIx(vg, 0x99,0x99,0x99); /* med gray */
chromColor[24] = vgFindColorIx(vg, 0x66,0x66,0x66);  /* med gray */

chromColor[25] = vgFindColorIx(vg, 0x0,0x0,0x0); /* black */
/*
vgMakeColorGradient(vg, &white, &chrom1, CHROMOSOME_SHADES, shadesOfChrom1);
vgMakeColorGradient(vg, &white, &chrom2, CHROMOSOME_SHADES, shadesOfChrom2);
vgMakeColorGradient(vg, &white, &chrom3, CHROMOSOME_SHADES, shadesOfChrom3);
vgMakeColorGradient(vg, &white, &chrom4, CHROMOSOME_SHADES, shadesOfChrom4);
vgMakeColorGradient(vg, &white, &chrom5, CHROMOSOME_SHADES, shadesOfChrom5);
vgMakeColorGradient(vg, &white, &chrom6, CHROMOSOME_SHADES, shadesOfChrom6);
vgMakeColorGradient(vg, &white, &chrom7, CHROMOSOME_SHADES, shadesOfChrom7);
vgMakeColorGradient(vg, &white, &chrom8, CHROMOSOME_SHADES, shadesOfChrom8);
chromColor[0] = kshadesOfChrom1[LOW];
chromColor[1] = shadesOfChrom1[MEDIUM];
chromColor[2] = shadesOfChrom1[BRIGHT];
chromColor[3] = shadesOfChrom2[LOW];
chromColor[4] = shadesOfChrom2[MEDIUM];
chromColor[5] = shadesOfChrom2[BRIGHT];
chromColor[6] = shadesOfChrom3[LOW];
chromColor[7] = shadesOfChrom3[MEDIUM];
chromColor[8] = shadesOfChrom3[BRIGHT];
chromColor[9] = shadesOfChrom4[LOW];
chromColor[10] = shadesOfChrom4[MEDIUM];
chromColor[11] = shadesOfChrom4[BRIGHT];
chromColor[12] = shadesOfChrom5[LOW];
chromColor[13] = shadesOfChrom5[MEDIUM];
chromColor[14] = shadesOfChrom5[BRIGHT];
chromColor[15] = shadesOfChrom6[LOW];
chromColor[16] = shadesOfChrom6[MEDIUM];
chromColor[17] = shadesOfChrom6[BRIGHT];
chromColor[18] = shadesOfChrom7[LOW];
chromColor[19] = shadesOfChrom7[MEDIUM];
chromColor[20] = shadesOfChrom7[BRIGHT];
chromColor[21] = shadesOfChrom8[LOW];
chromColor[22] = shadesOfChrom8[MEDIUM];
chromColor[23] = shadesOfChrom8[BRIGHT];
*/
chromosomeColorsMade = TRUE;
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


int percentGrayIx(int percent)
/* Return gray shade corresponding to a number from 50 - 100 */
{
return grayInRange(percent, 50, 100);
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
if (res < 1) res = 1;
if (res >= maxShade) res = maxShade-1;
return res;
}


static int cmpLfWhiteToBlack(const void *va, const void *vb)
/* Help sort from white to black. */
{
const struct linkedFeaturesSeries *a = *((struct linkedFeaturesSeries **)va);
const struct linkedFeaturesSeries *b = *((struct linkedFeaturesSeries **)vb);
return a->grayIx - b->grayIx;
}

static int cmpLfBlackToWhite(const void *va, const void *vb)
/* Help sort from white to black. */
{
const struct linkedFeaturesSeries *a = *((struct linkedFeaturesSeries **)va);
const struct linkedFeaturesSeries *b = *((struct linkedFeaturesSeries **)vb);
return b->grayIx - a->grayIx;
}

static int linkedFeaturesCmpStart(const void *va, const void *vb)
/* Help sort linkedFeatures by starting pos. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
return a->start - b->start;
}

void clippedBarbs(struct vGfx *vg, int x, int y, 
	int width, int barbHeight, int barbSpacing, int barbDir, Color color,
	boolean needDrawMiddle)
/* Draw barbed line.  Clip it to fit the window first though since
 * some barbed lines will span almost the whole chromosome, and the
 * clipping at the lower level is not efficient since we added
 * PostScript output support. */
{
int x2 = x + width;
if (x < 0) x = 0;
if (x2 > vg->width) x2 = vg->width;
vgBarbedHorizontalLine(vg, x, y, x2 - x, barbHeight, barbSpacing, barbDir,
	color, needDrawMiddle);
}

void innerLine(struct vGfx *vg, int x, int y, int w, Color color)
/* Draw a horizontal line of given width minus a pixel on either
 * end.  This pixel is needed for PostScript only, but doesn't
 * hurt elsewhere. */
{
if (w > 1)
   vgLine(vg, x+1, y, x+w-1, y, color);
}

static void linkedFeaturesSeriesDraw(struct trackGroup *tg, 
	int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw linked features items. */
{
int baseWidth = seqEnd - seqStart;
struct linkedFeaturesSeries *lfs;
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

memset(colorBin, 0, MAXPIXELS * sizeof(colorBin[0]));

if (tg->itemColor)	/* Item color overrides spectrum processing. */
    {
    shades = NULL;
    }

if (vis == tvDense && shades)
    {
    slSort(&tg->items, cmpLfWhiteToBlack);
    }

for (lfs = tg->items; lfs != NULL; lfs = lfs->next)
    {
    int midY = y + midLineOff;
    int compCount = 0;
    int w;
    /* int prevEnd = -1; */
    int prevEnd = lfs->start;

    for (lf = lfs->features; lf != NULL; lf = lf->next)
        {
        if (lf->filterColor > 0)
            color = lf->filterColor;
        else if (tg->itemColor)
	    {
            color = tg->itemColor(tg, lf, vg);
	    bColor = tg->ixAltColor;
	    }
        else if (shades) 
            color =  shades[lf->grayIx+isXeno];
        tallStart = lf->tallStart;
        tallEnd = lf->tallEnd;

        x1 = round((double)((int)prevEnd-winStart)*scale) + xOff;
        x2 = round((double)((int)lf->start-winStart)*scale) + xOff;
        w = x2-x1;
        if ((isFull) && (prevEnd != -1) && !lfs->noLine) 
              {
              clippedBarbs(vg, x1, midY, w, 2, 5, 
                           lfs->orientation, bColor, TRUE);
              }
        if (prevEnd != -1 && !lfs->noLine) 
	    innerLine(vg, x1, midY, w, color);
        prevEnd = lf->end;
      
        if (lf->components != NULL && !hideLine)
            {
            x1 = round((double)((int)lf->start-winStart)*scale) + xOff;
            x2 = round((double)((int)lf->end-winStart)*scale) + xOff;
            w = x2-x1;
            if(tg->mapsSelf && tg->mapItem)
            tg->mapItem(tg, lfs, lf->name, lf->start, lf->end, x1, y, (x1 == x2 ? 1 : x2-x1), lineHeight);
            if (isFull)
                {
                if (shades) bColor =  shades[(lf->grayIx>>1)];
                clippedBarbs(vg, x1, midY, x2-x1, 2, 5, 
                         lf->orientation, bColor, FALSE);
                }
	    innerLine(vg, x1, midY, w, color);
            }
        for (sf = lf->components; sf != NULL; sf = sf->next)
            {
            s = sf->start; e = sf->end;

            if (s < tallStart)
                {
                e2 = e;
                if (e2 > tallStart) e2 = tallStart;
                drawScaledBoxSample(vg, s, e2, scale, xOff, y+shortOff, shortHeight, color , lf->score);
                s = e2;
                }
            if (e > tallEnd)
                {
                s2 = s;
                if (s2 < tallEnd) s2 = tallEnd;
                drawScaledBoxSample(vg, s2, e, scale, xOff, y+shortOff, shortHeight, color , lf->score);
                e = s2;
                }
            if (e > s)
                {
                drawScaledBoxSample(vg, s, e, scale, xOff, y, heightPer, color, lf->score);
                ++compCount;
                }
            if(hgDebug)
                {
                char buff[16];
                int textWidth;
                int sx1 = roundingScale(sf->start-winStart, width, baseWidth)+xOff;
                int sx2 = roundingScale(sf->end-winStart, width, baseWidth)+xOff;
                int sw = sx2 - sx1;
                snprintf(buff, sizeof(buff), "%.2f", lf->score);
                textWidth = mgFontStringWidth(font, buff);
                if (textWidth <= sw )
                    vgTextCentered(vg, sx1, y, sw, heightPer, MG_WHITE, font, buff);
                }
            }
        }
    if ((isFull) && (prevEnd != -1) && !lfs->noLine && (prevEnd < lfs->end)) 
	{
        x1 = round((double)((int)prevEnd-winStart)*scale) + xOff;
        x2 = round((double)((int)lfs->end-winStart)*scale) + xOff;
        w = x2-x1;
	clippedBarbs(vg, x1, midY, w, 2, 5, 
			       lfs->orientation, bColor, TRUE);
	innerLine(vg, x1, midY, w, color);
	}

    if (isFull) y += lineHeight;
    } 
}



int whichBin( double num, double thisMin, double thisMax, double binCount )
/* Get bin value from num. */
{
return (num - thisMin) * binCount / (thisMax - thisMin);
}

double whichNum( double bin, double thisMin, double thisMax, double binCount )
/* gets range nums. from bin values*/
{
return( (thisMax - thisMin) / binCount * bin + thisMin );
}


int basePositionToXAxis( int base, int seqStart, int seqEnd, int
                width, int xOff  ) 
{
    int baseWidth = seqEnd - seqStart;
    double scale = width/(double)baseWidth;
    double x1 = round((double)((int)base-seqStart)*scale) + xOff; 
    return(x1);
}

int humMusZoomLevel( void )
{
    int zoom1 = 80000, zoom2 = 5000, zoom3 = 300; /* bp per data point */      
    int pixPerBase = (winEnd - winStart)/ tl.picWidth;
    if(pixPerBase >= zoom1)
        return(1);
    else if( pixPerBase >= zoom2 ) 
        return(2);
    else if(pixPerBase >= zoom3)
        return(3);
    else    
        return(0);
}

            

int gfxBorder = 1;

int trackOffsetX()
/* Return x offset where track display proper begins. */
{
int x = gfxBorder;
if (withLeftLabels)
    x += tl.leftLabelWidth + gfxBorder;
return x;
}

static void drawWiggleHorizontalLine( struct vGfx *vg, 
	double where, double min0, double max0, 
	int binCount, int y, double hFactor, int heightPer, 
	Color lineColor )
/* Draws a blue horizontal line on a wiggle track at a specified
 * location based on the range and number of bins*/
{
int bin;
double y1;
bin = -whichBin( where, min0, max0, binCount);
y1 = (int)((double)y+((double)bin)*hFactor+(double)heightPer);
vgBox( vg, 0, y1, vg->width, 1, lineColor );
}

static void wiggleLinkedFeaturesDraw(struct trackGroup *tg, 
    int seqStart, int seqEnd,
    struct vGfx *vg, int xOff, int yOff, int width, 
    MgFont *font, Color color, enum trackVisibility vis)
/* Currently this routine is adapted from Terry's 
 * linkedFeatureSeriesDraw() routine.
 * It is called for 'sample 9' tracks as specified in the trackDb.ra.
 * and it looks at the cart to decide whether to interpolate, fill blocks,
 * and use anti-aliasing.*/
{
int i;
int baseWidth = seqEnd - seqStart;
struct linkedFeatures *lf;
struct simpleFeature *sf;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2;
boolean isFull = (vis == tvFull);
Color bColor = tg->ixAltColor;
double scale = width/(double)baseWidth;
int prevX = -1;
int gapPrevX = -1;
double prevY = -1;
double y1 = -1, y2;
int ybase;
int sampleX, sampleY; /* A sample in sample coordinates. 
                       * Sample X coordinate is chromosome coordinate.
		       * Sample Y coordinate is usually 0-1000 */
int binCount = 999;   /* Maximum sample Y coordinate. */
int bin;	      /* Sample Y coordinates are first converted to
                       * bin coordinates, and then to pixels.  I'm not
		       * totally sure why.  */


int currentX, currentXEnd, currentWidth;

int leftSide, rightSide;

int insideX = trackOffsetX();
int trackTabWidth = 11;
int trackPastTabX = (withLeftLabels ? trackTabWidth : 0);
int trackPastTabWidth = tl.picWidth - trackPastTabX;

int noZoom = 1;
enum wiggleOptEnum wiggleType;
char *interpolate = NULL;
char *aa = NULL; 
boolean antiAlias = FALSE;
int fill; 
char *lineGapStr = NULL;
int lineGapSize;
double min0, max0;

char o1[128]; /* Option 1 - linear interp */
char o2[128]; /* Option 2 - anti alias */
char o3[128]; /* Option 3 - fill */
char o4[128]; /* Option 4 - max gap where interpolation is still done */

double hFactor, hFactor2;
double minRange, maxRange;


Color gridColor = vgFindRgb(vg, &guidelineColor); /* for horizontal lines*/

lf=tg->items;    
if(lf==NULL) return;

//take care of cart options
snprintf( o1, sizeof(o1),"%s.linear.interp", tg->mapName);
snprintf( o2, sizeof(o2), "%s.anti.alias", tg->mapName);
snprintf( o3, sizeof(o3),"%s.fill", tg->mapName);
snprintf( o4, sizeof(o4),"%s.interp.gap", tg->mapName);
interpolate = cartUsualString(cart, o1, "Linear Interpolation");
wiggleType = wiggleStringToEnum(interpolate);
aa = cartUsualString(cart, o2, "on");
antiAlias = sameString(aa, "on");
fill = atoi(cartUsualString(cart, o3, "1"));
lineGapSize = atoi(cartUsualString(cart, o4, "200"));

cartSetString( cart, "win", "F" );

heightPer = tg->heightPer+1;
hFactor = (double)heightPer/1000.0;

//this information should be moved to the trackDb.ra
if( sameString( tg->mapName, "humMus" ) )
    {
    minRange = 300.0;
    maxRange = 1000.0;

    min0 = whichNum( minRange, -7.99515, 6.54171, 1000 );
    max0 = whichNum( maxRange, -7.99515, 6.54171, 1000 );

    /*draw horizontal line across track at 0.0, 2.0, and 5.0*/
    if( !isFull )
	{
	drawWiggleHorizontalLine( vg, 0.0, min0, max0, binCount,
	    y, hFactor, heightPer, gridColor );
	drawWiggleHorizontalLine( vg, 2.0, min0, max0, binCount,
	    y, hFactor, heightPer, gridColor );
	drawWiggleHorizontalLine( vg, 5.0, min0, max0, binCount,
	    y, hFactor, heightPer, gridColor );
	}

    }
else if( sameString( tg->mapName, "humMusL" ) 
	|| sameString( tg->mapName, "musHumL" )  )
    {
    minRange = 0.0;
    maxRange = whichBin( 6.0, 0.0, 8.0 ,binCount );
    min0 = whichNum( minRange, 0.0, 8.0, binCount );
    max0 = whichNum( maxRange, 0.0, 8.0, binCount );

    //errAbort( "whichBin=%g\n", maxRange );

    if( isFull )
	{
	for( i=1; i<=6; i++ )
	    drawWiggleHorizontalLine( vg, (double)i, min0, max0,
		binCount, y, hFactor, heightPer, gridColor );
	}
    }

else if( sameString( tg->mapName, "zoo" ) )
    {
    /*Always interpolate zoo track (since gaps are explicitly defined*/
    lineGapSize = -1;
    minRange = 500.0;
    maxRange = 1000.0;
    }
else if( sameString( tg->mapName, "zooCons" ) )
    {
    minRange = 0.0;
    maxRange = 1000.0;
    }
else if( sameString( tg->mapName, "binomialCons2" ) )
    {
    minRange = 0.0;
    maxRange = 1000.0;
    }
else if( sameString( tg->mapName, "binomialCons3" ) )
    {
    minRange = 0.0;
    maxRange = 200.0;
    }
else if( sameString( tg->mapName, "binomialCons" ) )
    {
    minRange = 0.0;
    maxRange = 500.0;
    }
else
    {
    minRange = 1.0;
    maxRange = 1000.0;
    }

for(lf = tg->items; lf != NULL; lf = lf->next) 
    {
    gapPrevX = -1;
    prevX = -1;
    ybase = (int)((double)y+hFactor+(double)heightPer);

    for (sf = lf->components; sf != NULL; sf = sf->next)
	{
	sampleX = sf->start;
	sampleY = sf->end - sampleX;	// Stange encoding but so it is. 

	/*mapping or sequencing gap*/
	if (sampleY == 0)
	    {
	    bin = -whichBin( (int)((maxRange - minRange)/5.0+minRange), 
	    	minRange, maxRange, binCount );
	    y1 = (int)((double)y+((double)bin)* hFactor+(double)heightPer);
	    if( gapPrevX >= 0 )
		drawScaledBox(vg, sampleX, gapPrevX, scale, 
			xOff, (int)y1, (int)(.10*heightPer), shadesOfGray[2]);
	    gapPrevX = sampleX;
	    prevX = -1; /*connect next point with gray bar too*/
	    continue;
	    }
	if (sampleY > maxRange)
	    sampleY = maxRange;
	if (sampleY < minRange)
	    sampleY = minRange;
	bin = -whichBin( sampleY, minRange, maxRange, binCount );

	x1 = round((double)(sampleX-winStart)*scale) + xOff;
	y1 = (int)((double)y+((double)bin)* hFactor+(double)heightPer);

	if (prevX > 0)
	    {
	    y2 = prevY;
	    x2 = round((double)(prevX-winStart)*scale) + xOff;
	    if( wiggleType == wiggleLinearInterpolation ) 
	    /*connect samples*/
		{
		if( lineGapSize < 0 || sampleX - prevX <= lineGapSize )   
		    /*don't interpolate over large gaps*/
		    {
		    if (fill)
			vgFillUnder(vg, x1,y1, x2,y2, ybase, bColor);
		    else
			vgLine(vg, x1,y1, x2,y2, color);
		    }
		}
	    }

	/* Draw the points themselves*/
	if (wiggleType != wiggleLinearInterpolation)
	    {
	    drawScaledBox(vg, sampleX, sampleX+1, scale, 
	    	xOff, (int)y1-1, 3, color);
	    if( fill )
		drawScaledBox(vg, sampleX, sampleX+1, scale, xOff, (int)y1+2, 
		    ybase-y1-2, bColor);
	    }
	prevX = gapPrevX = sampleX;
	prevY = y1;
	}

    leftSide = max( tg->itemStart(tg,lf), winStart );
    rightSide = min(  tg->itemEnd(tg,lf), winEnd );

    currentX =  round((double)((int)leftSide-winStart)*scale) + xOff;
    currentXEnd =  round((double)((int)rightSide-winStart)*scale) + xOff;
    currentWidth = currentXEnd - currentX;

    if( noZoom && isFull )
	{
	mapBoxHc(lf->start, lf->end, currentX ,y, currentWidth,
	    heightPer, tg->mapName, tg->mapItemName(tg, lf), tg->itemName(tg, lf));

	if( lf->next != NULL )
	    y += updateY( lf->name, lf->next->name, lineHeight );
	else
	    y += lineHeight;
	}
    }
}


static void linkedFeaturesDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw linked features items. */
/* Integrated with linkedFeaturesSeriesDraw */
{
/* Convert to a linked features series object */
linkedFeaturesToLinkedFeaturesSeries(tg);
/* Draw items */
linkedFeaturesSeriesDraw(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
/* Convert Back */
linkedFeaturesSeriesToLinkedFeatures(tg);
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


static void linkedFeaturesSeriesDrawAverage(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense clone items. */
{
int baseWidth = seqEnd - seqStart;
double scale = width/(double)baseWidth;
UBYTE *useCounts;
int i;
int lineHeight = mgFontLineHeight(font);
struct simpleFeature *sf;
struct linkedFeatures *lf;
struct linkedFeaturesSeries *lfs;
int x1, x2, w;

AllocArray(useCounts, width);
memset(useCounts, 0, width * sizeof(useCounts[0]));
for (lfs = tg->items; lfs != NULL; lfs = lfs->next) 
    {
    /* int prevEnd = -1; */
    int prevEnd = lfs->start;
    for (lf = lfs->features; lf != NULL; lf = lf->next)
        {
	if (prevEnd != -1)
	    {
	    x1 = round((double)((int)prevEnd-winStart)*scale) + xOff;
	    x2 = round((double)((int)lf->start-winStart)*scale) + xOff;
            w = x2 - x1;
            if (w > 0)
	        vgBox(vg, x1, yOff + tg->heightPer/2, w, 1, vgFindColorIx(vg,0,0,0));
            }
        prevEnd = lf->end;
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
    }
grayThreshold(useCounts, width);
vgVerticalSmear(vg,xOff,yOff,width,lineHeight,useCounts,TRUE);
freeMem(useCounts);
}

static void linkedFeaturesDrawAverage(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense clone items. */
{
/* Convert to a linked features series object */
linkedFeaturesToLinkedFeaturesSeries(tg);
/* Draw items */
linkedFeaturesSeriesDrawAverage(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
/* Convert Back */
linkedFeaturesSeriesToLinkedFeatures(tg);
}

static void linkedFeaturesAverageDense(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense linked features items. */
{
/* Draw items */
if (vis == tvFull)
    {
    linkedFeaturesDraw(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
    }
else if (vis == tvDense)
    {
    linkedFeaturesDrawAverage(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
    }
}

static void linkedFeaturesSeriesAverageDense(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense linked features series items. */
{
  /*if (vis == tvFull)
    {*/
    linkedFeaturesSeriesDraw(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
    /*    }
else if (vis == tvDense)
    {
    linkedFeaturesSeriesDrawAverage(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
    }*/
}


static void linkedFeaturesDrawPair(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw linked features items for EST pairs. */
{
int baseWidth = seqEnd - seqStart;
struct linkedFeaturesPair *lfPair;
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
 
struct rgbColor color5prime = tg->color;
struct rgbColor color3prime = tg->altColor;
shades = NULL;

for (lfPair = tg->items; lfPair != NULL; lfPair = lfPair->next)
    {   
    int midY = y + midLineOff;
    int compCount = 0;
    int w, i;
    
    for(i=1;i<=2;++i)
      {/*  drawing has to be done twice for each pair */
      if(i==1)
        {
	lf = lfPair->lf5prime;
	color = vgFindRgb(vg, &color5prime);
	bColor = color;
        }
      else
        {
	lf = lfPair->lf3prime;
	lf->orientation = -lf->orientation;
	color = vgFindRgb(vg, &color3prime);
	bColor = color;
        } 
      if(lf != NULL) 
	  {
	  if (tg->itemColor && shades == NULL)
	      color = tg->itemColor(tg, lf, vg);
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
		  clippedBarbs(vg, x1, midY, x2-x1, 2, 5, 
					 lf->orientation, bColor, FALSE);
		  }
	      if (shades) color =  shades[lf->grayIx+isXeno];
	      innerLine(vg, x1, midY, w, color);
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
		  drawScaledBox(vg, s, e2, scale, xOff, y+shortOff, shortHeight, color);
		  s = e2;
		  }
	      if (e > tallEnd)
		  {
		  s2 = s;
		  if (s2 < tallEnd) s2 = tallEnd;
		  drawScaledBox(vg, s2, e, scale, xOff, y+shortOff, shortHeight, color);
		  e = s2;
		  }
	      if (e > s)
		  {
		  drawScaledBox(vg, s, e, scale, xOff, y, heightPer, color);
		  ++compCount;
		  }
	      }
	  }
      }
    if (isFull)
	y += lineHeight;
    }
}

static void linkedFeaturesDrawAveragePair(struct trackGroup *tg, 
	int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense clone items for EST pairs. */
{
int baseWidth = seqEnd - seqStart;
UBYTE *useCounts;
int i, j;
int lineHeight = mgFontLineHeight(font);
struct simpleFeature *sf;
struct linkedFeaturesPair *lfPair;
struct linkedFeatures *lf;
int x1, x2, w, allCount;
 
AllocArray(useCounts, width);
memset(useCounts, 0, width * sizeof(useCounts[0]));
for (lfPair = tg->items; lfPair != NULL; lfPair = lfPair->next)
    {
     
      for(j=1;j<=2;++j)
	{  /* drawing has to be done twice for each pair */
	  if(j==1){
	    lf = lfPair->lf5prime;
	  }
	  else{
	    lf = lfPair->lf3prime;
	  }
	  if(lf != NULL) 
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
	}
    }
grayThreshold(useCounts, width);
vgVerticalSmear(vg,xOff,yOff,width,lineHeight,useCounts,TRUE);
freeMem(useCounts);
}
static void linkedFeaturesAverageDensePair(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense linked features items. */
{
  if (vis == tvFull)
    {
      linkedFeaturesDrawPair(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
    }
  else if (vis == tvDense)
    {
      linkedFeaturesDrawAveragePair(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
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

void linkedFeaturesMethods(struct trackGroup *tg)
/* Fill in track group methods for linked features. */
{
tg->freeItems = freeLinkedFeaturesItems;
tg->drawItems = linkedFeaturesDraw;
tg->itemName = linkedFeaturesName;
tg->mapItemName = linkedFeaturesName;
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = linkedFeaturesItemStart;
tg->itemEnd = linkedFeaturesItemEnd;
}

void sampleLinkedFeaturesMethods(struct trackGroup *tg)
/* Fill in track group methods for 'sample' tracks. */
{
tg->freeItems = freeLinkedFeaturesItems;
tg->drawItems = wiggleLinkedFeaturesDraw;
tg->itemName = linkedFeaturesName;
tg->mapItemName = linkedFeaturesName;
tg->totalHeight = tgUserDefinedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = linkedFeaturesItemStart;
tg->itemEnd = linkedFeaturesItemEnd;
}

int linkedFeaturesSeriesItemStart(struct trackGroup *tg, void *item)
/* Return start chromosome coordinate of item. */
{
struct linkedFeaturesSeries *lfs = item;
return lfs->start;
}

int linkedFeaturesSeriesItemEnd(struct trackGroup *tg, void *item)
/* Return end chromosome coordinate of item. */
{
struct linkedFeaturesSeries *lfs = item;
return lfs->end;
}

void linkedFeaturesSeriesMethods(struct trackGroup *tg)
/* Fill in track group methods for linked features.series */
{
tg->freeItems = freeLinkedFeaturesSeriesItems;
tg->drawItems = linkedFeaturesSeriesAverageDense;
tg->itemName = linkedFeaturesSeriesName;
tg->mapItemName = linkedFeaturesSeriesName;
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = linkedFeaturesSeriesItemStart;
tg->itemEnd = linkedFeaturesSeriesItemEnd;
}

struct trackGroup *linkedFeaturesTg()
/* Return generic track group for linked features. */
{
struct trackGroup *tg = NULL;
AllocVar(tg);
linkedFeaturesMethods(tg);
tg->colorShades = shadesOfGray;
return tg;
}

struct linkedFeatures *lfFromBed(struct bed *bed)
/* Return a linked feature from a (full) bed. */
{
struct linkedFeatures *lf;
struct simpleFeature *sf, *sfList = NULL;
int grayIx = grayInRange(bed->score, 0, 1000);
int *starts = bed->chromStarts, start;
int *sizes = bed->blockSizes;
int blockCount = bed->blockCount, i;

assert(starts != NULL && sizes != NULL && blockCount > 0);
AllocVar(lf);
lf->grayIx = grayIx;
strncpy(lf->name, bed->name, sizeof(lf->name));
lf->orientation = orientFromChar(bed->strand[0]);
for (i=0; i<blockCount; ++i)
    {
    AllocVar(sf);
    start = starts[i] + bed->chromStart;
    sf->start = start;
    sf->end = start + sizes[i];
    sf->grayIx = grayIx;
    slAddHead(&sfList, sf);
    }
slReverse(&sfList);
lf->components = sfList;
finishLf(lf);
lf->tallStart = bed->thickStart;
lf->tallEnd = bed->thickEnd;
return lf;
}


struct linkedFeatures *lfFromSample(struct sample *sample)
/* Return a linked feature from a full sample (wiggle) track. */
{
struct linkedFeatures *lf;
struct simpleFeature *sf, *sfList = NULL;
int grayIx = grayInRange(sample->score, 0, 1000);
int *X = sample->samplePosition, start;
int *Y = sample->sampleHeight;
int sampleCount = sample->sampleCount, i;

assert(X != NULL && Y != NULL && sampleCount > 0);
AllocVar(lf);
lf->grayIx = grayIx;
strncpy(lf->name, sample->name, sizeof(lf->name));
lf->orientation = orientFromChar(sample->strand[0]);

for (i=0; i<sampleCount; ++i)
    {
    AllocVar(sf);
    start = X[i] + sample->chromStart;
    sf->start = start;

    if( Y[i] == -29 )      /*hack for negative values not loading correctly*/
        sf->end = start;
    else if( Y[i] == 0 )
        sf->end = start + 1;
    else
        sf->end = start + Y[i];

    sf->grayIx = grayIx;
    slAddHead(&sfList, sf);
    }
slReverse(&sfList);
lf->components = sfList;
finishLf(lf);
lf->end = sample->chromEnd;
return lf;
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


struct linkedFeatures *lfFromPslx(struct psl *psl, int sizeMul, boolean isXeno, boolean nameGetsPos)
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
lf->score = (psl->match - psl->misMatch - psl->repMatch);
lf->grayIx = grayIx;
if (nameGetsPos)
    {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s:%d-%d %s:%d-%d", psl->qName, psl->qStart, psl->qEnd,
    	psl->tName, psl->tStart, psl->tEnd);
    lf->extra = cloneString(buf);
    snprintf(lf->name, sizeof(lf->name), "%s %dk", psl->qName, psl->qStart/1000);
    snprintf(lf->popUp, sizeof(lf->popUp), "%s:%d-%d score %9.0f", psl->qName, psl->qStart, psl->qEnd, lf->score);
    }
else
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
return lfFromPslx(psl, 1, isXeno, FALSE);
}

void filterMrna(struct trackGroup *tg, struct linkedFeatures **pLfList)
/* Apply filters if any to mRNA linked features. */
{
struct linkedFeatures *lf, *next, *newList = NULL, *oldList = NULL;
struct mrnaUiData *mud = tg->extraUiData;
struct mrnaFilter *fil;
char *type;
int i = 0;
boolean anyFilter = FALSE;
boolean colorIx = 0;
boolean isExclude = FALSE;
boolean andLogic = TRUE;
char query[256];
struct sqlResult *sr;
char **row;
struct sqlConnection *conn = NULL;
boolean isFull;

if (*pLfList == NULL || mud == NULL)
    return;

/* First make a quick pass through to see if we actually have
 * to do the filter. */
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    {
    fil->pattern = cartUsualString(cart, fil->key, "");
    if (fil->pattern[0] != 0)
        anyFilter = TRUE;
    }
if (!anyFilter)
    return;

isFull = (limitVisibility(tg, tg->items) == tvFull);

type = cartUsualString(cart, mud->filterTypeVar, "red");
if (sameString(type, "exclude"))
    isExclude = TRUE;
else if (sameString(type, "include"))
    isExclude = FALSE;
else
    colorIx = getFilterColor(type, MG_BLACK);
type = cartUsualString(cart, mud->logicTypeVar, "and");
andLogic = sameString(type, "and");


/* Make a pass though each filter, and start setting up search for
 * those that have some text. */
conn = hAllocConn();
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    {
    fil->pattern = cartUsualString(cart, fil->key, "");
    if (fil->pattern[0] != 0)
	{
	fil->hash = newHash(10);
	if ((fil->mrnaTableIx = sqlFieldIndex(conn, "mrna", fil->table)) < 0)
	    internalErr();
	}
    }

/* Scan tables id/name tables to build up hash of matching id's. */
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    {
    struct hash *hash = fil->hash;
    int wordIx, wordCount;
    char *words[128];

    if (hash != NULL)
	{
	boolean anyWild;
	char *dupPat = cloneString(fil->pattern);
	wordCount = chopLine(dupPat, words);
	for (wordIx=0; wordIx <wordCount; ++wordIx)
	    {
	    char *pattern = cloneString(words[wordIx]);
	    if (lastChar(pattern) != '*')
		{
		int len = strlen(pattern)+1;
		pattern = needMoreMem(pattern, len, len+1);
		pattern[len-1] = '*';
		}
	    anyWild = (strchr(pattern, '*') != NULL || strchr(pattern, '?') != NULL);
	    sprintf(query, "select * from %s", fil->table);
	    touppers(pattern);
	    sr = sqlGetResult(conn, query);
	    while ((row = sqlNextRow(sr)) != NULL)
		{
		boolean gotMatch;
		touppers(row[1]);
		if (anyWild)
		    gotMatch = wildMatch(pattern, row[1]);
		else
		    gotMatch = sameString(pattern, row[1]);
		if (gotMatch)
		    {
		    hashAdd(hash, row[0], NULL);
		    }
		}
	    sqlFreeResult(&sr);
	    freez(&pattern);
	    }
	freez(&dupPat);
	}
    }

/* Scan through linked features coloring and or including/excluding ones that 
 * match filter. */
for (lf = *pLfList; lf != NULL; lf = next)
    {
    boolean passed = andLogic;
    next = lf->next;
    sprintf(query, "select * from mrna where acc = '%s'", lf->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	for (fil = mud->filterList; fil != NULL; fil = fil->next)
	    {
	    if (fil->hash != NULL)
		{
		if (hashLookup(fil->hash, row[fil->mrnaTableIx]) == NULL)
		    {
		    if (andLogic)    
			passed = FALSE;
		    }
		else
		    {
		    if (!andLogic)
		        passed = TRUE;
		    }
		}
	    }
	}
    sqlFreeResult(&sr);
    if (passed ^ isExclude)
	{
	slAddHead(&newList, lf);
	if (colorIx > 0)
	    lf->filterColor = colorIx;
	}
    else
        {
	slAddHead(&oldList, lf);
	}
    }

slReverse(&newList);
slReverse(&oldList);
if (colorIx > 0)
   {
   /* Draw stuff that passes filter first in full mode, last in dense. */
   if (isFull)
       {
       newList = slCat(newList, oldList);
       }
   else
       {
       newList = slCat(oldList, newList);
       }
   }
*pLfList = newList;

/* Free up hashes, etc. */
for (fil = mud->filterList; fil != NULL; fil = fil->next)
    {
    hashFree(&fil->hash);
    }
hFreeConn(&conn);
}

void connectedLfFromPslsInRange(struct sqlConnection *conn,
    struct trackGroup *tg, int start, int end, char *chromName,
    boolean isXeno, boolean nameGetsPos)
/* Return linked features from range of table after have
 * already connected to database.. */
{
char *track = tg->mapName;
struct sqlResult *sr = NULL;
char **row;
int rowOffset;
char *optionChrStr;
struct linkedFeatures *lfList = NULL, *lf;
char optionChr[128]; /* Option -  chromosome filter */
char extraWhere[128] ;

snprintf( optionChr, sizeof(optionChr), "%s.chromFilter", tg->mapName);
optionChrStr = cartUsualString(cart, optionChr, "All");
if (startsWith("chr",optionChrStr)) 
	{
	snprintf(extraWhere, sizeof(extraWhere), "qName = \"%s\"",optionChrStr);
	sr = hRangeQuery(conn, track, chromName, start, end, extraWhere, &rowOffset);
	}
else
	{
	snprintf(extraWhere, sizeof(extraWhere), " ");
	sr = hRangeQuery(conn, track, chromName, start, end, NULL, &rowOffset);
	}

if (sqlCountColumns(sr) < 21+rowOffset)
    errAbort("trackDb has incorrect table type for table \"%s\"",
	     tg->mapName);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct psl *psl = pslLoad(row+rowOffset);
    lf = lfFromPslx(psl, 1, isXeno, nameGetsPos);
    slAddHead(&lfList, lf);
    pslFree(&psl);
    }
slReverse(&lfList);
if (limitVisibility(tg, lfList) == tvFull)
    slSort(&lfList, linkedFeaturesCmpStart);
if (tg->extraUiData)
    filterMrna(tg, &lfList);
sqlFreeResult(&sr);
tg->items = lfList;
}

void lfFromPslsInRange(struct trackGroup *tg, int start, int end, 
	char *chromName, boolean isXeno, boolean nameGetsPos)
/* Return linked features from range of table. */
{
struct sqlConnection *conn = hAllocConn();
connectedLfFromPslsInRange(conn, tg, start, end, chromName, 
	isXeno, nameGetsPos);
hFreeConn(&conn);
}

void lfFromPslsInRangeAndFilter(struct trackGroup *tg, int start, int end, 
	char *chromName, boolean isXeno, boolean nameGetsPos)
/* Return linked features from range of table. */
{
struct sqlConnection *conn = hAllocConn();
connectedLfFromPslsInRange(conn, tg, start, end, chromName, 
	isXeno, nameGetsPos);
hFreeConn(&conn);
}


struct linkedFeaturesSeries *lfsFromBed(struct lfs *lfsbed)
/* Create linked feature series object from database bed record */ 
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row, rest[32];
int rowOffset, i;
struct linkedFeaturesSeries *lfs; 
struct linkedFeatures *lfList = NULL, *lf; 

AllocVar(lfs);
lfs->name = cloneString(lfsbed->name);
lfs->start = lfsbed->chromStart;
lfs->end = lfsbed->chromEnd;
lfs->orientation = orientFromChar(lfsbed->strand[0]);

/* Get linked features */
for (i = 0; i < lfsbed->lfCount; i++)  
{
  AllocVar(lf);
  sprintf(rest, "qName = '%s'", lfsbed->lfNames[i]);
  sr = hRangeQuery(conn, lfsbed->pslTable, lfsbed->chrom, lfsbed->lfStarts[i], lfsbed->lfStarts[i] + lfsbed->lfSizes[i], rest, &rowOffset);
  if ((row = sqlNextRow(sr)) != NULL)
  {
    struct psl *psl = pslLoad(row);
    lf = lfFromPsl(psl, FALSE);
    slAddHead(&lfList, lf);
    pslFree(&psl);
  }
  sqlFreeResult(&sr);
}
slReverse(&lfList);
sqlFreeResult(&sr);
hFreeConn(&conn);
lfs->features = lfList;
return lfs;
} 

struct linkedFeaturesSeries *lfsFromBedsInRange(char *table, int start, int end, char *chromName)
/* Return linked features from range of table. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int rowOffset;
struct linkedFeaturesSeries *lfsList = NULL, *lfs; 

sr = hOrderedRangeQuery(conn, table, chromName, start, end, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct lfs *lfsbed = lfsLoad(row+rowOffset);
    lfs = lfsFromBed(lfsbed);
    slAddHead(&lfsList, lfs);
    lfsFree(&lfsbed);
    }
slReverse(&lfsList);
sqlFreeResult(&sr);
hFreeConn(&conn);
return lfsList;
}

void loadBacEndPairs(struct trackGroup *tg)
/* Load up bac end pairs from table into trackGroup items. */
{
tg->items = lfsFromBedsInRange("bacEndPairs", winStart, winEnd, chromName);
}

void bacEndPairsMethods(struct trackGroup *tg)
/* Fill in track group methods for linked features.series */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadBacEndPairs;
}


void loadBacEndPairsBad(struct trackGroup *tg)
/* Load up fosmid end pairs from table into trackGroup items. */
{
tg->items = lfsFromBedsInRange("bacEndPairsBad", winStart, winEnd, chromName);
}


void bacEndPairsBadMethods(struct trackGroup *tg)
/* Fill in track group methods for linked features.series */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadBacEndPairsBad;
}

void loadBacEndPairsLong(struct trackGroup *tg)
/* Load up BAC end pairs from table into trackGroup items. */
{
tg->items = lfsFromBedsInRange("bacEndPairsLong", winStart, winEnd, chromName);
}


void bacEndPairsLongMethods(struct trackGroup *tg)
/* Fill in track group methods for linked features.series */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadBacEndPairsLong;
}

void loadFosEndPairs(struct trackGroup *tg)
/* Load up fosmid end pairs from table into trackGroup items. */
{
tg->items = lfsFromBedsInRange("fosEndPairs", winStart, winEnd, chromName);
}

void fosEndPairsMethods(struct trackGroup *tg)
/* Fill in track group methods for linked features.series */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadFosEndPairs;
}

void loadFosEndPairsBad(struct trackGroup *tg)
/* Load up fosmid end pairs from table into trackGroup items. */
{
tg->items = lfsFromBedsInRange("fosEndPairsBad", winStart, winEnd, chromName);
}


void fosEndPairsBadMethods(struct trackGroup *tg)
/* Fill in track group methods for linked features.series */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadFosEndPairsBad;
}

void loadFosEndPairsLong(struct trackGroup *tg)
/* Load up fosmid end pairs from table into trackGroup items. */
{
tg->items = lfsFromBedsInRange("fosEndPairsLong", winStart, winEnd, chromName);
}


void fosEndPairsLongMethods(struct trackGroup *tg)
/* Fill in track group methods for linked features.series */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadFosEndPairsLong;
}

struct linkedFeaturesPair *lfFromPslsInRangeForEstPair(char *table, char *chrom, int start, int end, boolean isXeno)
/* Return a linked list of structures that have a pair of linked features for 5' and 3' ESTs from range of table. */
{
  char query[256], query1[256], query2[256];
  struct sqlConnection *conn = hAllocConn();
  struct sqlConnection *conn1 = hAllocConn();
  struct sqlResult *sr = NULL, *sr1 = NULL;
  char **row, **row1, **row2;
  struct linkedFeaturesPair *lfListPair = NULL, *lf=NULL;
  struct estPair *ep = NULL;
  char mytable[64];
  boolean hasBin;
  int rowOffset;

  hFindSplitTable(chrom, "all_est", mytable, &hasBin);
  sprintf(query, "select * from %s where chrom='%s' and chromStart<%u and chromEnd>%u",
	  table, chrom, winEnd, winStart);

  sr = sqlGetResult(conn, query); 
  while ((row = sqlNextRow(sr)) != NULL)
    {
      AllocVar(lf);
      ep = estPairLoad(row);
      lf->cloneName = ep->mrnaClone;
          sprintf(query1, "select * from all_est where qName='%s' and tStart=%u and tEnd=%u",
	      ep->acc5, ep->start5, ep->end5);
      sr1 = sqlGetResult(conn1, query1);
      if((row1 = sqlNextRow(sr1)) != NULL)
	{
	  struct psl *psl;
	  if (hasBin) {
	    psl = pslLoad(row1 + 1);
	  } else {
	    psl = pslLoad(row1);
	  }
	  lf->lf5prime = lfFromPsl(psl, isXeno);
	  pslFree(&psl);
	}
	sqlFreeResult(&sr1);
     
        sprintf(query2, "select * from all_est where qName = '%s' and tStart=%u and tEnd=%u", 
  	      ep->acc3, ep->start3, ep->end3); 
        sr1 = sqlGetResult(conn1, query2); 
        if((row2 = sqlNextRow(sr1)) != NULL) 
  	{ 
	  struct psl *psl;
	  if (hasBin) {
	    psl = pslLoad(row2 + 1);
	  } else {
	    psl = pslLoad(row2);
	  }
  	  lf->lf3prime = lfFromPsl(psl, isXeno); 
  	  pslFree(&psl); 
	} 
      sqlFreeResult(&sr1);
      slSafeAddHead(&lfListPair, lf);
    }
  hFreeConn(&conn1);
  slReverse(&lfListPair);
  sqlFreeResult(&sr);
  hFreeConn(&conn);
  return lfListPair;
}

void loadEstPairAli(struct trackGroup *tg)
/* Load up est pairs from table into trackGroup items. */
{
  tg->items = lfFromPslsInRangeForEstPair("estPair", chromName, winStart, winEnd, FALSE);
}

void freeLinkedFeaturesPair(struct linkedFeaturesPair **pList)
/* Free up a linked features pair list. */
{
  struct linkedFeaturesPair *lf;
  for (lf = *pList; lf != NULL; lf = lf->next){
    slFreeList(&lf->lf5prime->components);
    slFreeList(&lf->lf3prime->components);
  }
  slFreeList(pList);
}

void estFreePair(struct trackGroup *tg)
/* Free up linkedFeaturesPairTrack items. */
{
//  freeLinkedFeaturesPair((struct linkedFeaturesPair**)(&tg->items)); 
}

char *getEstPairName(struct trackGroup *tg, void *item)
{
  struct linkedFeaturesPair *lf = item;
  return lf->cloneName;
}

void estPairMethods(struct trackGroup *tg)
/* Make track group of est pairs. */
{
linkedFeaturesMethods(tg);
tg->freeItems = estFreePair;
tg->itemName = getEstPairName;
tg->mapItemName = getEstPairName;
tg->loadItems = loadEstPairAli;
tg->drawItems = linkedFeaturesAverageDensePair;
}


char *mapNameFromLfExtra(struct trackGroup *tg, void *item)
/* Return name of item. */
{
struct linkedFeatures *lf = item;
return lf->extra;
}

void parseSs(char *ss, char **retPsl, char **retFa)
/* Parse out ss variable into components. */
{
static char buf[1024];
char *words[2];
int wordCount;

strcpy(buf, ss);
wordCount = chopLine(buf, words);
if (wordCount < 2)
    errAbort("Badly formated ss variable");
*retPsl = words[0];
*retFa = words[1];
}

boolean ssFilesExist(char *ss)
/* Return TRUE if both files in ss exist. */
{
char *faFileName, *pslFileName;
parseSs(ss, &pslFileName, &faFileName);
return fileExists(pslFileName) && fileExists(faFileName);
}

void loadUserPsl(struct trackGroup *tg)
/* Load up rnas from table into trackGroup items. */
{
char *ss = userSeqString;
char buf2[3*512];
char *faFileName, *pslFileName;
struct lineFile *f;
struct psl *psl;
struct linkedFeatures *lfList = NULL, *lf;
enum gfType qt, tt;
int sizeMul = 1;

parseSs(ss, &pslFileName, &faFileName);
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
	lf = lfFromPslx(psl, sizeMul, TRUE, FALSE);
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
tg->mapItemName = mapNameFromLfExtra;
tg->priority = 11;
return tg;
}



struct linkedFeatures *connectedLfFromGenePredInRange(
        struct sqlConnection *conn, char *table, 
	char *chrom, int start, int end)
/* Return linked features from range of a gene prediction table after 
 * we have already connected to database. */
{
struct sqlResult *sr = NULL;
char **row;
struct linkedFeatures *lfList = NULL, *lf;
int grayIx = maxShade;
int rowOffset;

sr = hRangeQuery(conn, table, chrom, start, end, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct simpleFeature *sfList = NULL, *sf;
    struct genePred *gp = genePredLoad(row + rowOffset);
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
return lfList;
}

struct linkedFeatures *lfFromGenePredInRange(char *table, 
	char *chrom, int start, int end)
/* Return linked features from range of a gene prediction table. */
{
struct linkedFeatures *lfList = NULL;
struct sqlConnection *conn = hAllocConn();
lfList = connectedLfFromGenePredInRange(conn, table, chrom, start, end);
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

void genieAltMethods(struct trackGroup *tg)
/* Make track group of full length mRNAs. */
{
tg->itemName = genieName;
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
if (limitVisibility(tg, tg->items) == tvFull)
    {
    lookupKnownNames(tg->items);
    }
}

Color genieKnownColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color to draw known gene in. */
{
struct linkedFeatures *lf = item;

if (startsWith("AK.", lf->name))
    {
    static int colIx = 0;
    if (!colIx)
	colIx = vgFindColorIx(vg, 0, 120, 200);
    return colIx;
    }
#ifdef SOMETIMES
else if (lf->extra)
    {
    static int colIx = 0;
    if (!colIx)
	colIx = vgFindColorIx(vg, 200, 0, 0);
    return colIx;
    }
#endif /* SOMETIMES */
else
    {
    return tg->ixColor;
    }
}

void genieKnownMethods(struct trackGroup *tg)
/* Make track group of known genes. */
{
tg->loadItems = loadGenieKnown;
tg->itemName = genieName;
tg->itemColor = genieKnownColor;
}

char *refGeneName(struct trackGroup *tg, void *item)
/* Return abbreviated genie name. */
{
    static char cat[128];
struct linkedFeatures *lf = item;
if (lf->extra != NULL) 
    {
    sprintf(cat,"%s",(char *)lf->extra);
    return cat;
    }
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
if (limitVisibility(tg, tg->items) == tvFull)
    {
    lookupRefNames(tg->items);
    slSort(&tg->items, linkedFeaturesCmpStart);
    }
}


Color refGeneColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color to draw refseq gene in. */
{
struct linkedFeatures *lf = item;
int col = tg->ixColor;
struct rgbColor *normal = &(tg->color);
struct rgbColor lighter, lightest;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];

/* If refSeqStatus is available, use it to determine the color.
 * Reviewed -> normal, Provisional -> lighter, Predicted(other) -> lightest 
 * If no refSeqStatus, color it normally. 
 */
if (hTableExists("refSeqStatus"))
    {
    sprintf(query, "select status from refSeqStatus where mrnaAcc = '%s'",
	    lf->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
	if (startsWith("Reviewed", row[0]))
	    {
	    /* Use the usual color */
	    }
	else if (startsWith("Provisional", row[0]))
	    {
	    lighter.r = (6*normal->r + 4*255) / 10;
	    lighter.g = (6*normal->g + 4*255) / 10;
	    lighter.b = (6*normal->b + 4*255) / 10;
	    col = vgFindRgb(vg, &lighter);
	    }
	else
	    {
	    lightest.r = (1*normal->r + 2*255) / 3;
	    lightest.g = (1*normal->g + 2*255) / 3;
	    lightest.b = (1*normal->b + 2*255) / 3;
	    col = vgFindRgb(vg, &lightest);
	    }
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
return(col);
}


void refGeneMethods(struct trackGroup *tg)
/* Make track group of known genes from refSeq. */
{
tg->loadItems = loadRefGene;
tg->itemName = refGeneName;
tg->mapItemName = refGeneMapName;
tg->itemColor = refGeneColor;
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

void ensGeneMethods(struct trackGroup *tg)
/* Make track group of Ensembl predictions. */
{
tg->itemName = ensGeneName;
}

Color intronEstColorAndOrient(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Always returns MG_BLACK, attempts to orient ests using data in estOrientInfo table. */
{
struct linkedFeatures *lf = item;
char query[512];
char buf[64], *s = NULL;
struct sqlConnection *conn = hAllocConn();
int col = MG_BLACK;
int estOrient = 0;

if(hTableExists("estOrientInfo"))
    {
    snprintf(query, sizeof(query), 
	     "select intronOrientation from estOrientInfo where name='%s' and chromStart=%d and chromEnd=%d and chrom='%s'", 
	     lf->name, lf->start, lf->end, chromName);
    estOrient = sqlQuickNum(conn, query);
    }
/* estOrient should still be zero if estOrientInfo table doesn't exist. */
if(estOrient != 0)
    {
    if(estOrient < 0)
	lf->orientation = -1 * lf->orientation;
    }
else /* if can't find in estOrientInfo table */
    {
    sprintf(query, "select direction from mrna where acc='%s'", lf->name);
    if ((s = sqlQuickQuery(conn, query, buf, sizeof(buf))) != NULL)
	{
	if (s[0] == '3')
	    lf->orientation = -1 * lf->orientation;	/* Not the best place for this but... */
	}
    }
hFreeConn(&conn);
return col;
}

Color estColorAndOrient(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Always returns MG_BLACK, also tries to orient them using genbank info. */
{
struct linkedFeatures *lf = item;
char query[512];
char buf[64], *s;
struct sqlConnection *conn = hAllocConn();
int col = MG_BLACK;
int estOrient = 0;

sprintf(query, "select direction from mrna where acc='%s'", lf->name);
if ((s = sqlQuickQuery(conn, query, buf, sizeof(buf))) != NULL)
    {
    if (s[0] == '3')
	{
	lf->orientation = -lf->orientation;	/* Not the best place for this but... */
	}
    }
hFreeConn(&conn);
return col;
}

void intronEstMethods(struct trackGroup *tg)
/* Make track group of EST methods - overrides color handler. */
{
tg->itemColor = intronEstColorAndOrient;
tg->extraUiData = newMrnaUiData(tg->mapName, FALSE);
}

void estMethods(struct trackGroup *tg)
/* Make track group of EST methods - overrides color handler. */
{
tg->itemColor = estColorAndOrient;
tg->extraUiData = newMrnaUiData(tg->mapName, FALSE);
}

void mrnaMethods(struct trackGroup *tg)
/* Make track group of mRNA methods. */
{
tg->extraUiData = newMrnaUiData(tg->mapName, FALSE);
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


void sanger22Methods(struct trackGroup *tg)
/* Make track group of Sanger's chromosome 22 gene annotations. */
{
tg->itemName = sanger22Name;
}


int cmpAgpFrag(const void *va, const void *vb)
/* Compare two agpFrags by chromStart. */
{
const struct agpFrag *a = *((struct agpFrag **)va);
const struct agpFrag *b = *((struct agpFrag **)vb);
return a->chromStart - b->chromStart;
}


void goldLoad(struct trackGroup *tg)
/* Load up golden path from database table to trackGroup items. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct agpFrag *fragList = NULL, *frag;
struct agpGap *gapList = NULL, *gap;
int rowOffset;

/* Get the frags and load into tg->items. */
sr = hRangeQuery(conn, "gold", chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    frag = agpFragLoad(row+rowOffset);
    slAddHead(&fragList, frag);
    }
slSort(&fragList, cmpAgpFrag);
sqlFreeResult(&sr);
tg->items = fragList;

/* Get the gaps into tg->customPt. */
sr = hRangeQuery(conn, "gap", chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gap = agpGapLoad(row+rowOffset);
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
        struct vGfx *vg, int xOff, int yOff, int width, 
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
	    drawScaledBox(vg, gap->chromStart, gap->chromEnd, scale, xOff, midY, 1, brown);
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
    vgBox(vg, x1, y, w, heightPer, color);
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

void goldMethods(struct trackGroup *tg)
/* Make track group for golden path */
{
tg->loadItems = goldLoad;
tg->freeItems = goldFree;
tg->drawItems = goldDraw;
tg->itemName = goldName;
tg->mapItemName = goldName;
}


struct netItem
/* A net track item. */
    {
    struct netItem *next;
    int level;
    char *className;
    int yOffset;
    };

static char *netClassNames[] =  {
    "Level 1", "Level 2", "Level 3", "Level 4", "Level 5", "Level 6", "Level 7", "Level 8" };

struct netItem *makeNetItems()
/* Make the levels for net alignment track. */
{
struct netItem *ni, *niList = NULL;
int i;
int numClasses = ArraySize(netClassNames);
for (i=0; i<numClasses; ++i)
    {
    AllocVar(ni);
    ni->level = i;
    ni->className = netClassNames[i];
    slAddHead(&niList, ni);
    }
slReverse(&niList);
return niList;
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
        struct vGfx *vg, int xOff, int yOff, int width, 
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
int rowOffset;

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
    sr = hRangeQuery(conn, "rmsk", chromName, winStart, winEnd, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	rmskOutStaticLoad(row+rowOffset, &ro);
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
	vgBox(vg, x1, ri->yOffset, w, heightPer, col);
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
	    }
	}
    freeHash(&hash);
    }
else
    {
    char table[64];
    boolean hasBin;
    struct dyString *query = newDyString(1024);
    /* Do black and white on single track.  Fetch less than we need from database. */
    if (hFindSplitTable(chromName, "rmsk", table, &hasBin))
        {
	dyStringPrintf(query, "select genoStart,genoEnd from %s where ", table);
	if (hasBin)
	    hAddBinToQuery(winStart, winEnd, query);
	dyStringPrintf(query, "genoStart<%u and genoEnd>%u", winEnd, winStart);
	sr = sqlGetResult(conn, query->string);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    int start = sqlUnsigned(row[0]);
	    int end = sqlUnsigned(row[1]);
	    x1 = roundingScale(start-winStart, width, baseWidth)+xOff;
	    x2 = roundingScale(end-winStart, width, baseWidth)+xOff;
	    w = x2-x1;
	    if (w <= 0)
		w = 1;
	    vgBox(vg, x1, yOff, w, heightPer, MG_BLACK);
	    }
	}
    dyStringFree(&query);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void repeatMethods(struct trackGroup *tg)
/* Make track group for repeats. */
{
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
}

typedef struct slList *(*ItemLoader)(char **row);

void bedLoadItem(struct trackGroup *tg, char *table, ItemLoader loader)
/* Generic tg->item loader. */
{
  
struct sqlConnection *conn = hAllocConn();
int rowOffset;
struct sqlResult *sr = hRangeQuery(conn, table, chromName, 
	winStart, winEnd, NULL, &rowOffset);
char **row;
struct slList *itemList = NULL, *item;

while ((row = sqlNextRow(sr)) != NULL)
    {
    item = loader(row + rowOffset);
    slAddHead(&itemList, item);
    }

slReverse(&itemList);
slSort(&itemList, bedCmp);
sqlFreeResult(&sr);
tg->items = itemList;
hFreeConn(&conn);
}

static Color contrastingColor(struct vGfx *vg, int backgroundIx)
/* Return black or white whichever would be more visible over
 * background. */
{
struct rgbColor c = vg->colorIxToRgb(vg, backgroundIx);
int valSquared = c.r * c.r + c.g * c.g + c.b * c.b;
int threshold = 3*150*150;
if (valSquared > threshold)
    return MG_BLACK;
else
    return MG_WHITE;
}

static void bedDrawSimple(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
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
int midLineOff = heightPer/2;
int dir;
Color *shades = tg->colorShades;
Color textColor = MG_WHITE;


for (item = tg->items; item != NULL; item = item->next)
    {
    x1 = round((double)((int)item->chromStart-winStart)*scale) + xOff;
    x2 = round((double)((int)item->chromEnd-winStart)*scale) + xOff;
    w = x2-x1;
    if (tg->itemColor != NULL)
        color = tg->itemColor(tg, item, vg);
    else
	{
	if (shades)
	    color = shades[grayInRange(item->score, 0, 1000)];
	}
    if (w < 1)
	w = 1;
    if (color)
	{
	vgBox(vg, x1, y, w, heightPer, color);
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
		{
		textColor = contrastingColor(vg, color);
		vgTextCentered(vg, x1, y, w, heightPer, textColor, font, s);
		}
	    }
	}
    if (tg->subType == lfWithBarbs)
        {
        dir = 0;
        if(sameString(item->strand , "+")) 
            dir = 1;
        if(sameString(item->strand , "-")) 
            dir = -1;
	w = x2-x1;
        if (dir != 0)
	    {
	    int midY = y + midLineOff;
	    textColor = contrastingColor(vg, color);
	    clippedBarbs(vg, x1, midY, w, 2, 5, dir, textColor, TRUE);
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
if (bed->name == NULL)
    return "";
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


void bedMethods(struct trackGroup *tg)
/* Fill in methods for (simple) bed tracks. */
{
tg->drawItems = bedDrawSimple;
tg->itemName = bedName;
tg->mapItemName = bedName;
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = bedItemStart;
tg->itemEnd = bedItemEnd;
}

struct trackGroup *bedTg()
/* Get track group loaded with generic bed values. */
{
struct trackGroup *tg;
AllocVar(tg);
bedMethods(tg);
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
        struct vGfx *vg, int xOff, int yOff, int width, 
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
    vgBox(vg, x1, y, w, heightPer, color);
    mapBoxHc(item->chromStart, item->chromEnd, x1, y, w, heightPer, tg->mapName,
	item->name, item->name);
    if (isFull)
	y += lineHeight;
    }
}

void isochoresMethods(struct trackGroup *tg)
/* Make track group for isochores. */
{
tg->loadItems = isochoreLoad;
tg->freeItems = isochoreFree;
tg->drawItems = isochoreDraw;
tg->colorShades = shadesOfGray;
tg->itemName = isochoreName;
}

/*Ewan's stuff */
/******************************************************************/
					/*Royden fun test code*/
/******************************************************************/

#ifdef OLD2
struct celeraDupPositive *filterOldDupes(struct celeraDupPositive *oldList)
/* Get rid of all but recent/artifact dupes. */
{
struct celeraDupPositive *newList = NULL, *dup, *next;
for (dup = oldList; dup != NULL; dup = next)
    {
    next = dup->next;

    /*if (dup->score > 980) */
    /*    {  */
    	slAddHead(&newList, dup);
    /*	} */
    /*else */
    /*    { */
    /*	celeraDupPositiveFree(&dup); */
    /* } */


    }
slReverse(&newList);
return newList;
}

#endif /* OLD2 */

void loadCeleraDupPositive(struct trackGroup *tg)
/* Load up simpleRepeats from database table to trackGroup items. */
{
bedLoadItem(tg, "celeraDupPositive", (ItemLoader)celeraDupPositiveLoad);
if (tg->visibility == tvDense && slCount(tg->items) <= maxItemsInFullTrack)
    slSort(&tg->items, bedCmpScore);
else
    slSort(&tg->items, bedCmp);
}

void freeCeleraDupPositive(struct trackGroup *tg)
/* Free up isochore items. */
{
celeraDupPositiveFreeList((struct celeraDupPositive**)&tg->items);
}

Color celeraDupPositiveColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return name of gcPercent track item. */
{
struct celeraDupPositive *dup = item;
/*int ppt = dup->score;*/
int grayLevel;

/*if (ppt > 990)*/
    return tg->ixColor;
/*else if (ppt > 980)*/
/*    return tg->ixAltColor;*/
/* grayLevel = grayInRange(ppt, 900, 1000); */
grayLevel=grayInRange(990,900,1000);

return shadesOfGray[grayLevel];
}

char *celeraDupPositiveName(struct trackGroup *tg, void *item)
/* Return full genie name. */
{
struct celeraDupPositive *gd = item;
char *full = gd->name;
static char abbrev[64];

strcpy(abbrev, skipChr(full));
abbr(abbrev, "om");
return abbrev;
}


void celeraDupPositiveMethods(struct trackGroup *tg)
/* Make track group for simple repeats. */
{
tg->loadItems = loadCeleraDupPositive;
tg->freeItems = freeCeleraDupPositive;
tg->itemName = celeraDupPositiveName;
tg->itemColor = celeraDupPositiveColor;
}
/******************************************************************/
		/*end of Royden test Code celeraDupPositive */
/******************************************************************/
/******************************************************************/
			/*Royden fun test code CeleraCoverage*/
/******************************************************************/

#ifdef OLD3



struct celeraCoverage *filterOldCoverage(struct celeraCoverage *oldList)
/* Get rid of all but recent/artifact dupes. */
{
struct celeraCoverage *newList = NULL, *dup, *next;
for (dup = oldList; dup != NULL; dup = next)
    {
    next = dup->next;

    /*if (dup->score > 980) */
    /*    {  */
    	slAddHead(&newList, dup);
    /*	} */
    /*else */
    /*    { */
    /*	celeraDupPositiveFree(&dup); */
    /* } */


    }
slReverse(&newList);
return newList;
}

#endif /* OLD3 */

void loadCeleraCoverage(struct trackGroup *tg)
/* Load up simpleRepeats from database table to trackGroup items. */
{
bedLoadItem(tg, "celeraCoverage", (ItemLoader)celeraCoverageLoad);
if (tg->visibility == tvDense && slCount(tg->items) <= maxItemsInFullTrack)
    slSort(&tg->items, bedCmpScore);
else
    slSort(&tg->items, bedCmp);
}

void freeCeleraCoverage(struct trackGroup *tg)
/* Free up isochore items. */
{
celeraCoverageFreeList((struct celeraCoverage**)&tg->items);
}

Color celeraCoverageColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return name of gcPercent track item. */
{
struct celeraDupPositive *dup = item;
/*int ppt = dup->score;*/
int grayLevel;

/*if (ppt > 990)*/
    return tg->ixColor;
/*else if (ppt > 980)*/
/*    return tg->ixAltColor;*/
/* grayLevel = grayInRange(ppt, 900, 1000); */
grayLevel=grayInRange(990,900,1000);

return shadesOfGray[grayLevel];
}

char *celeraCoverageName(struct trackGroup *tg, void *item)
/* Return full genie name. */
{
struct celeraCoverage *gd = item;
char *full = gd->name;
static char abbrev[64];

strcpy(abbrev, skipChr(full));
abbr(abbrev, "om");
return abbrev;
}


void celeraCoverageMethods(struct trackGroup *tg)
/* Make track group for simple repeats. */
{
tg->loadItems = loadCeleraCoverage;
tg->freeItems = freeCeleraCoverage;
tg->itemName = celeraCoverageName;
tg->itemColor = celeraCoverageColor;
}
/******************************************************************/
		/*end of Royden test Code celeraCoverage */
/******************************************************************/
#ifdef OLD4


struct genomicSuperDups *filterOldCoverage(struct genomicSuperDups *oldList)
/* Get rid of all but recent/artifact dupes. */
{
struct genomicSuperDups *newList = NULL, *dup, *next;
for (dup = oldList; dup != NULL; dup = next)
    {
    next = dup->next;

    /*if (dup->score > 980) */
    /*    {  */
    	slAddHead(&newList, dup);
    /*	} */
    /*else */
    /*    { */
    /*	celeraDupPositiveFree(&dup); */
    /* } */


    }
slReverse(&newList);
return newList;
}

#endif /* OLD4 */

void loadGenomicSuperDups(struct trackGroup *tg)
/* Load up simpleRepeats from database table to trackGroup items. */
{
bedLoadItem(tg, "genomicSuperDups", (ItemLoader)genomicSuperDupsLoad);
if (tg->visibility == tvDense && slCount(tg->items) <= maxItemsInFullTrack)
    slSort(&tg->items, bedCmpScore);
else
    slSort(&tg->items, bedCmp);
}

void freeGenomicSuperDups(struct trackGroup *tg)
/* Free up isochore items. */
{
genomicSuperDupsFreeList((struct genomicSuperDups**)&tg->items);
}

Color genomicSuperDupsColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return name of gcPercent track item. */
{
struct genomicSuperDups *dup = item;
int ppt = dup->score;
int grayLevel;
char *verdict=dup->verdict;


if ((verdict[0]=='B')&&(verdict[1]=='A')&&(verdict[2]=='D'))
    return vgFindColorIx(vg, 255,51,51);


else if (ppt > 990)
    return tg->ixColor;
else if (ppt > 980)
    return tg->ixAltColor;
grayLevel = grayInRange(ppt, 900, 1000);
return shadesOfGray[grayLevel];
}

char *genomicSuperDupsName(struct trackGroup *tg, void *item)
/* Return full genie name. */
{
struct genomicSuperDups *gd = item;
char *full = gd->name;
static char abbrev[64];

strcpy(abbrev, skipChr(full));
abbr(abbrev, "om");
return abbrev;
}


void genomicSuperDupsMethods(struct trackGroup *tg)
/* Make track group for simple repeats. */
{
tg->loadItems = loadGenomicSuperDups;
tg->freeItems = freeGenomicSuperDups;
tg->itemName = genomicSuperDupsName;
tg->itemColor = genomicSuperDupsColor;
}
/******************************************************************/
		/*end of Royden test Code genomicSuperDups */
/******************************************************************/
/*end Ewan's*/

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

void simpleRepeatMethods(struct trackGroup *tg)
/* Make track group for simple repeats. */
{
tg->loadItems = loadSimpleRepeats;
tg->freeItems = freeSimpleRepeats;
tg->itemName = simpleRepeatName;
}

Color cpgIslandColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color of cpgIsland track item. */
{
struct cpgIsland *el = item;
return (el->length < 300 ? tg->ixAltColor : tg->ixColor);
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

void cpgIslandMethods(struct trackGroup *tg)
/* Make track group for simple repeats. */
{
tg->loadItems = loadCpgIsland;
tg->freeItems = freeCpgIsland;
tg->itemColor = cpgIslandColor;
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
    *retBoxColor = shadesOfGray[maxShade];
    *retTextColor = MG_WHITE;
    }
else 
    {
    *retBoxColor = cenColor;
    *retTextColor = MG_WHITE;
    }
}

static void cytoBandDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
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
    vgBox(vg, x1, y, w, heightPer, col);
    s = abbreviatedBandName(tg, band, tl.font, w);
    if (s != NULL)
	vgTextCentered(vg, x1, y, w, heightPer, textCol, tl.font, s);
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

void cytoBandMethods(struct trackGroup *tg)
/* Make track group for simple repeats. */
{
tg->loadItems = loadCytoBands;
tg->freeItems = freeCytoBands;
tg->drawItems = cytoBandDraw;
tg->itemName = cytoBandName;
}

static void swissDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw simple Bed items for protein track of SWISS-PROT, etc. */
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
int midLineOff = heightPer/2;
int dir;
Color *shades = tg->colorShades;

for (item = tg->items; item != NULL; item = item->next)
    {
    x1 = round((double)((int)item->chromStart-winStart)*scale) + xOff;
    x2 = round((double)((int)item->chromEnd-winStart)*scale) + xOff;
    w = x2-x1;
    if (tg->itemColor != NULL)
        color = tg->itemColor(tg, item, vg);
    else
	{
	if (shades)
	    {
	    color = shades[grayInRange(item->score, 0, 1000)];
	    }
	}
    if (w < 1)
	w = 1;
    if (color)
	{
	if (strstr(item->name, "PDB")     != NULL) color = MG_BLACK;
        if (strstr(item->name, "SWISS")  != NULL) 
		{
		color = vgFindColorIx(vg, 150, 0, 0);
		}
        if (strstr(item->name, "TrEMBL")  != NULL) 
		{
		color = vgFindColorIx(vg, 190, 100, 100);
		}
	
	vgBox(vg, x1, y, w, heightPer, color);
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
		vgTextCentered(vg, x1, y, w, heightPer, MG_WHITE, font, s);
	    }
	}
    if (tg->subType == lfWithBarbs)
        {
        dir = 0;
        if(sameString(item->strand , "+")) 
            dir = 1;
        if(sameString(item->strand , "-")) 
            dir = -1;
	w = x2-x1;
        if (dir != 0)
	    {
	    int midY = y + midLineOff;
	    clippedBarbs(vg, x1, midY, w, 2, 5, dir, MG_WHITE, TRUE);
	    }
        }
    if (isFull)
	y += lineHeight;
    }
}

static int swissMin = 320;
static int swissMax = 600;

Color swissColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return name of protein track item. */
{
int l;
char *chp;
struct bed *bed = item;

l = strlen(bed->name);
if (l > 6)
    {
    return(tg->ixColor);
    }
else return(MG_GRAY);
}

char *swissName(struct trackGroup *tg, void *item)
/* Return name of xxx track item. */
{
struct bed *sw = item;
static char buf[32];
char *chp;

sprintf(buf, "%s",  sw->name);

chp = strstr(buf, "(SWISS)");
if (chp != NULL)
	{
	chp = buf+strlen(buf)-strlen("(SWISS)");
	*chp = '\0';
	}
			
chp = strstr(buf, "(PDB)");
if (chp != NULL)
        {
	chp = buf+strlen(buf)-strlen("(PDB)");
	*chp = '\0';
	}

chp = strstr(buf, "(TrEMBL)");
if (chp != NULL)
        {
	chp = buf+strlen(buf)-strlen("(TrEMBL)");
	*chp = '\0';
	}
			
return(buf);
}

void swissMethods(struct trackGroup *tg)
/* Make track group for simple repeats. */
{
tg->drawItems = swissDraw;
tg->itemName  = swissName;
tg->itemColor = swissColor;
tg->drawName  = TRUE;
}


void loadGcPercent(struct trackGroup *tg)
/* Load up simpleRepeats from database table to trackGroup items. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct gcPercent *itemList = NULL, *item;
char query[256];

sprintf(query, "select * from %s where chrom = '%s' and chromStart<%u and chromEnd>%u", tg->mapName,
    chromName, winEnd, winStart);

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

Color gcPercentColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return name of gcPercent track item. */
{
struct gcPercent *gc = item;
int ppt = gc->gcPpt;
int grayLevel;

grayLevel = grayInRange(ppt, gcPercentMin, gcPercentMax);
return shadesOfGray[grayLevel];
}

static void gcPercentDenseDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
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
vgVerticalSmear(vg,xOff,yOff,width,lineHeight,aveCounts,TRUE);
freeMem(useCounts);
freeMem(aveCounts);
}

static void gcPercentDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw gcPercent items. */
{
if (vis == tvDense)
   gcPercentDenseDraw(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
else
   bedDrawSimple(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
}


void gcPercentMethods(struct trackGroup *tg)
/* Make track group for simple repeats. */
{
tg->loadItems = loadGcPercent;
tg->freeItems = freeGcPercent;
tg->itemName = gcPercentName;
tg->colorShades = shadesOfGray;
tg->itemColor = gcPercentColor;
}

char *recombRateMap;
enum recombRateOptEnum recombRateType;

boolean recombRateSetRate(struct trackGroup *tg, void *item)
/* Change the recombRate value to the one chosen */
{
struct recombRate *el = item;
switch (recombRateType)
    {
    case rroeDecodeAvg:
	return TRUE;
        break;
    case rroeDecodeFemale:
	el->decodeAvg = el->decodeFemale;
        return TRUE;
        break;
    case rroeDecodeMale:
	el->decodeAvg = el->decodeMale;
        return TRUE;
        break;
    case rroeMarshfieldAvg:
	el->decodeAvg = el->marshfieldAvg;
	return TRUE;
        break;
    case rroeMarshfieldFemale:
	el->decodeAvg = el->marshfieldFemale;
        return TRUE;
        break;
    case rroeMarshfieldMale:
	el->decodeAvg = el->marshfieldMale;
        return TRUE;
        break;
    case rroeGenethonAvg:
	el->decodeAvg = el->genethonAvg;
	return TRUE;
        break;
    case rroeGenethonFemale:
	el->decodeAvg = el->genethonFemale;
        return TRUE;
        break;
    case rroeGenethonMale:
	el->decodeAvg = el->genethonMale;
        return TRUE;
        break;
    default:
	return FALSE;
        break;
    }
}

void loadRecombRate(struct trackGroup *tg)
/* Load up recombRate from database table to trackGroup items. */
{
recombRateMap = cartUsualString(cart, "recombRate.type", rroeEnumToString(0));
recombRateType = rroeStringToEnum(recombRateMap);
bedLoadItem(tg, "recombRate", (ItemLoader)recombRateLoad);
filterItems(tg, recombRateSetRate, "include");
}

void freeRecombRate(struct trackGroup *tg)
/* Free up recombRate items. */
{
recombRateFreeList((struct recombRate**)&tg->items);
}

char *recombRateName(struct trackGroup *tg, void *item)
/* Return name of recombRate track item. */
{
struct recombRate *rr = item;
static char buf[32];

switch (recombRateType)
    {
    case rroeDecodeAvg: case rroeMarshfieldAvg: case rroeGenethonAvg:
	sprintf(buf, "%3.1f cM/Mb (Avg)", rr->decodeAvg);
        break;
    case rroeDecodeFemale: case rroeMarshfieldFemale: case rroeGenethonFemale:
	sprintf(buf, "%3.1f cM/Mb (F)", rr->decodeAvg);
        break;
    case rroeDecodeMale: case rroeMarshfieldMale: case rroeGenethonMale:
	sprintf(buf, "%3.1f cM/Mb (M)", rr->decodeAvg);
        break;
    default:
	sprintf(buf, "%3.1f cM/Mb (Avg)", rr->decodeAvg);
        break;
    }
return buf;
}

static int recombRateMin = 320;
static int recombRateMax = 600;

Color recombRateColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color for item in recombRate track item. */
{
struct recombRate *rr = item;
float rate = rr->decodeAvg;
int rcr;
int grayLevel;

rcr = (int)(rr->decodeAvg * 200);
grayLevel = grayInRange(rcr, recombRateMin, recombRateMax);
return shadesOfGray[grayLevel];
}

void recombRateMethods(struct trackGroup *tg)
/* Make track group for recombination rates. */
{
tg->loadItems = loadRecombRate;
tg->freeItems = freeRecombRate;
tg->itemName = recombRateName;
tg->colorShades = shadesOfGray;
tg->itemColor = recombRateColor;
}


/* Chromosome 18 deletions track */
void loadChr18deletions(struct trackGroup *tg)
/* Load up chr18deletions from database table to trackGroup items. */
{
bedLoadItem(tg, "chr18deletions", (ItemLoader)chr18deletionsLoad);
}

void freeChr18deletions(struct trackGroup *tg)
/* Free up chr18deletions items. */
{
chr18deletionsFreeList((struct chr18deletions**)&tg->items);
}

static void drawChr18deletions(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw chr18deletions items. */
{
int baseWidth = seqEnd - seqStart;
struct chr18deletions *cds;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int midLineOff = heightPer/2;
int shortOff = 2, shortHeight = heightPer-4;
int tallStart, tallEnd, shortStart, shortEnd;
int itemOff, itemHeight;
boolean isFull = (vis == tvFull);
double scale = width/(double)baseWidth;

memset(colorBin, 0, MAXPIXELS * sizeof(colorBin[0]));

if (vis == tvDense)
    {
    slSort(&tg->items, cmpLfWhiteToBlack);
    }

for (cds = tg->items; cds != NULL; cds = cds->next)
    {
    int midY = y + midLineOff;
    int wTall, wShort, end, start, blocks;

    for (blocks = 0; blocks < cds->ssCount; blocks++) 
	{ 
    	tallStart = cds->largeStarts[blocks];
	tallEnd = cds->largeEnds[blocks];
	shortStart = cds->smallStarts[blocks];
	shortEnd = cds->smallEnds[blocks];
	wTall = tallEnd - tallStart;
	wShort = shortEnd - shortStart;

	if (shortStart < tallStart)
	    {
	    end = tallStart;
	    drawScaledBoxSample(vg, shortStart, end, scale, xOff, y+shortOff, shortHeight, color , cds->score);
	    }
	if (shortEnd > tallEnd)
	    {
	    start = tallEnd;
	    drawScaledBoxSample(vg, start, shortEnd, scale, xOff, y+shortOff, shortHeight, color , cds->score);
	    }
	if (tallEnd > tallStart)
	    {
	    drawScaledBoxSample(vg, tallStart, tallEnd, scale, xOff, y, heightPer, color, cds->score);
	    }
	}
    if (isFull) y += lineHeight;
    }
}

void chr18deletionsMethods(struct trackGroup *tg)
/* Make track group for recombination rates. */
{
tg->loadItems = loadChr18deletions;
tg->freeItems = freeChr18deletions;
tg->drawItems = drawChr18deletions;
}

/* Make track group for simple repeats. */

#ifdef OLD
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
#endif /* OLD */

void loadGenomicDups(struct trackGroup *tg)
/* Load up simpleRepeats from database table to trackGroup items. */
{
bedLoadItem(tg, "genomicDups", (ItemLoader)genomicDupsLoad);
if (limitVisibility(tg, tg->items) == tvFull)
    slSort(&tg->items, bedCmpScore);
else
    slSort(&tg->items, bedCmp);
}

void freeGenomicDups(struct trackGroup *tg)
/* Free up isochore items. */
{
genomicDupsFreeList((struct genomicDups**)&tg->items);
}

Color genomicDupsColor(struct trackGroup *tg, void *item, struct vGfx *vg)
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

strcpy(abbrev, skipChr(full));
abbr(abbrev, "om");
return abbrev;
}


void genomicDupsMethods(struct trackGroup *tg)
/* Make track group for simple repeats. */
{
tg->loadItems = loadGenomicDups;
tg->freeItems = freeGenomicDups;
tg->itemName = genomicDupsName;
tg->itemColor = genomicDupsColor;
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

void genethonMethods(struct trackGroup *tg)
/* Make track group for simple repeats. */
{
tg->loadItems = loadGenethon;
tg->freeItems = freeGenethon;
}

Color exoFishColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color of exofish track item. */
{
struct exoFish *el = item;
int ppt = el->score;
int grayLevel;

grayLevel = grayInRange(ppt, -500, 1000);
return shadesOfSea[grayLevel];
}

void exoFishMethods(struct trackGroup *tg)
/* Make track group for exoFish. */
{
tg->itemColor = exoFishColor;
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


Color exoMouseColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color of exoMouse track item. */
{
struct roughAli *el = item;
int ppt = el->score;
int grayLevel;

grayLevel = grayInRange(ppt, -100, 1000);
return shadesOfBrown[grayLevel];
}


void exoMouseMethods(struct trackGroup *tg)
/* Make track group for exoMouse. */
{
if (sameString(chromName, "chr22") && hIsPrivateHost())
    tg->visibility = tvDense;
else
    tg->visibility = tvHide;
tg->loadItems = loadExoMouse;
tg->freeItems = freeExoMouse;
tg->itemName = exoMouseName;
tg->itemColor = exoMouseColor;
}

char *xenoMrnaName(struct trackGroup *tg, void *item)
/* Return what to display on left column of open track:
 * In this case display 6 letters of organism name followed
 * by mRNA accession. */
{
struct linkedFeatures *lf = item;
char *name = lf->name;
struct sqlConnection *conn = hAllocConn();
char query[256];
char organism[256], *org;
sprintf(query, "select organism.name from mrna,organism where mrna.acc = '%s' and mrna.organism = organism.id", name);
org = sqlQuickQuery(conn, query, organism, sizeof(organism));
hFreeConn(&conn);
if (org == NULL)
    return name;
else
    {
    static char compName[64];
    char *s;
    s = skipToSpaces(org);
    if (s != NULL)
      *s = 0;
    strncpy(compName, org, 7);
    compName[7] = 0;
    strcat(compName, " ");
    strcat(compName, name);
    return compName;
    }
return name;
}

void xenoMrnaMethods(struct trackGroup *tg)
/* Fill in custom parts of xeno mrna alignments. */
{
tg->itemName = xenoMrnaName;
tg->extraUiData = newMrnaUiData(tg->mapName, TRUE);
}

Color getChromColor(char *name, struct vGfx *vg)
{
    int chromNum = 0;
    Color colorNum = 0;
    if(!chromosomeColorsMade)
        makeChromosomeShades(vg);
    if (atoi(name) != 0)
        chromNum =  atoi(name);
    else if (!strcmp(name,"X"))
        chromNum = 23;
    else if (!strcmp(name,"X "))
        chromNum = 23;
    else if (!strcmp(name,"Y"))
        chromNum = 24;
    else if (!strcmp(name,"Y "))
        chromNum = 24;
    if (chromNum > 24) chromNum = 0;
    colorNum = chromColor[chromNum];
    return colorNum;
}

void netLoad(struct trackGroup *tg)
/* Load up net tracks.  (Will query database during drawing for a change.) */
{
tg->items = makeNetItems();
}

void netFree(struct trackGroup *tg)
/* Free up netGroup items. */
{
slFreeList(&tg->items);
}

char *netName(struct trackGroup *tg, void *item)
/* Return name of net level track. */
{
struct netItem *ni = item;
return ni->className;
}

Color netItemColor(struct netAlign *ms, struct vGfx *vg)
/* Return color of track item based on chromsome. */
{
char chromStr[20];     
if (strlen(ms->qName) == 8)
    {
    strncpy(chromStr,(char *)(ms->qName+1),1);
    chromStr[1] = '\0';
    }
else if (strlen(ms->qName) == 9)
    {
    strncpy(chromStr,(char *)(ms->qName+1),2);
    chromStr[2] = '\0';
    }
else
    {
    strncpy(chromStr,ms->qName+3,2);
    chromStr[2] = '\0';
    }
return ((Color)getChromColor(chromStr, vg));
}

static void netDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
{
int baseWidth = seqEnd - seqStart;
struct netItem *ni;
char chromStr[10];
int y = yOff;
int heightPer = tg->heightPer;
int midLineOff = heightPer/2;
int lineHeight = tg->lineHeight;
int x1,x2,w, b1,b2, g1,g2;
boolean isFull = (vis == tvFull);
Color col;
int ix = 0;
struct sqlConnection *conn = hAllocConn();
struct sqlConnection *conn2 = hAllocConn();
struct sqlResult *sr = NULL;
struct sqlResult *srGap = NULL;
char **row;
char **gapRow;
int rowOffset, gapRowOffset, textColor;
char levelWhere[64];
struct dyString *bubble;

if (isFull)
    {
    /* Do gray scale representation spread out among tracks. */
    struct hash *hash = newHash(8);
    struct netAlign na, np;
    int percId;
    int grayLevel, boxStart, boxEnd, level = 1;
    char statusLine[128];
    char levelName[10];
    char where[128];
    int midY, dir = 0;

    for (ni = tg->items; ni != NULL; ni = ni->next)
        {
        ni->yOffset = y;
        y += lineHeight;
        sprintf(levelName,"%d", ni->level);
        hashAdd(hash, levelName, ni);
        }
    for (level = 1 ; level <=8 ; level++)
        {
        sprintf(where,"level = %d",level);
        sr = hOrderedRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, where, &rowOffset);
        row = sqlNextRow(sr) ;
        if (row == NULL) continue;
        netAlignStaticLoad(row+rowOffset, &np);
        netAlignStaticLoad(row+rowOffset, &na);
        sprintf(levelName,"%d", np.level-1);
        ni = hashMustFindVal(hash, levelName);
        midY = ni->yOffset + midLineOff;
        percId = np.score;
        grayLevel = grayInRange(percId, 500, 1000);
        col = shadesOfGray[grayLevel];
        col = netItemColor(&na, vg);
        if (np.tStart >= winStart)
            {
            x1 = roundingScale(np.tStart-winStart, width, baseWidth)+xOff;
            boxStart = np.tStart;
            }
        else
            {
            x1 = roundingScale(0, width, baseWidth)+xOff;
            boxStart = winStart;
            }
        b1 = x1;
        g1 = x1; //in case there are no gaps
        x2 = roundingScale(np.tEnd-winStart, width, baseWidth)+xOff;
        boxEnd = np.tEnd;
        w = x2-x1;
        if (w <= 0)
            w = 1;
        while ((row = sqlNextRow(sr)) != NULL)
            {
            netAlignStaticLoad(row+rowOffset, &na);
            bubble = newDyString(1024);
            dyStringPrintf(bubble, "%s %dk %s ", np.qName, np.qStart/1000,np.strand);
            if (np.tR > 0 || np.qR > 0)
                dyStringPrintf(bubble, " RM Repeats %d/%d",np.tR,np.qR);
            if (np.tOldR > 0 || np.qOldR > 0)
                dyStringPrintf(bubble, " ancient Rep %d/%d",np.tOldR,np.qOldR);
            if (np.tNewR > 0 || np.qNewR > 0)
                dyStringPrintf(bubble, " new Rep %d/%d",np.tNewR,np.qNewR);
            if (np.tTrf > 0 || np.qTrf > 0)
                dyStringPrintf(bubble, " Trf Rep %d/%d",np.tTrf,np.qTrf);
            if (np.tN > 0 || np.qN > 0)
                dyStringPrintf(bubble, " Ns %d/%d",np.tN,np.qN);
            if (np.qFar > 0)
                dyStringPrintf(bubble, " Far %d",np.qFar);
            if (np.qOver > 0)
                dyStringPrintf(bubble, " Over %d",np.qOver);
            if (np.qDup > 0)
                dyStringPrintf(bubble, " Dup %d",np.qDup);
            textColor = contrastingColor(vg, col);
            dir = 0;
            if(sameString(np.strand , "+")) 
                dir = 1;
            if(sameString(np.strand , "-")) 
                dir = -1;
            if (sameString(na.type, "gap"))
                {
                boxEnd = na.tStart;
                if (na.tStart >= winStart)
                    {
                    b2 = roundingScale(na.tStart-winStart, width, baseWidth)+xOff;
                    g1 = b2;
                    }
                else
                    {
                    b2 = -1;
                    g1 = roundingScale(0, width, baseWidth)+xOff;
                    }
                g2 = roundingScale(na.tEnd-winStart, width, baseWidth)+xOff;
                if ((g2 - g1) > 3)
                    /* draw narrow line for gap */
                    {
                    clippedBarbs(vg, g1, midY, g2-g1, 2, 5, 
                                   dir, col, TRUE);
                    innerLine(vg, g1, midY, g2-g1, col);
                    }
                w = b2-b1;
                /* draw block */
                if (w <= 0)
                    w = 1;
                else
                    vgBox(vg, b1, ni->yOffset, w, heightPer, col);
                if (w > 3)
                    clippedBarbs(vg, b1, midY, w, 2, 5, dir, textColor, TRUE);
                if (w > 1)
                    {
                    sprintf(levelName,"%d", na.level);
                    mapBoxHc(boxStart, boxEnd, b1, ni->yOffset, w, heightPer, tg->mapName,
                        levelName, bubble->string);
                    }
                boxStart = na.tEnd;
                b1 = roundingScale(na.tEnd-winStart, width, baseWidth)+xOff;
                /*if (np.tStart > winStart)
                    x1 = roundingScale(np.tStart-winStart, width, baseWidth)+xOff;
                else 
                    x1 = roundingScale(0, width, baseWidth)+xOff;*/
                w = x2-b1;
                if (w <= 0)
                    w = 1;
                }
            else 
                {
                vgBox(vg, x1, ni->yOffset, w, heightPer, col);
                if (w > 3)
                    clippedBarbs(vg, x1, midY, w, 2, 5, dir, textColor, TRUE);
                if (w > 1)
                    {
                    sprintf(levelName,"%d", na.level);
                    mapBoxHc(na.tStart, na.tEnd, x1, ni->yOffset, w, heightPer, tg->mapName,
                        levelName, bubble->string);
                    }
                col = netItemColor(&na, vg);
                x1 = roundingScale(na.tStart-winStart, width, baseWidth)+xOff;
                x2 = roundingScale(na.tEnd-winStart, width, baseWidth)+xOff;
                boxStart = na.tStart;
                boxEnd = na.tEnd;
                w = x2-x1;
                if (w <= 0)
                    w = 1;
                }
            netAlignStaticLoad(row+rowOffset, &np);
            dyStringFree(&bubble);
            }
            vgBox(vg, b1, ni->yOffset, w, heightPer, col);
            textColor = contrastingColor(vg, col);
            if (w > 3)
                clippedBarbs(vg, b1, midY, w, 2, 5, dir, textColor, TRUE);
        }
    freeHash(&hash);
    }
else
    {
    char table[64];
    boolean hasBin;
    struct netAlign na;
    struct dyString *query = newDyString(1024);
    /* Do black and white on single track.  Fetch less than we need from database. */
    if (hFindSplitTable(chromName, tg->mapName, table, &hasBin))
        {
	dyStringPrintf(query, "select tStart,tEnd, qName from %s where ", table);
	if (hasBin)
	    hAddBinToQuery(winStart, winEnd, query);
	dyStringPrintf(query, "tStart<%u and tEnd>%u", winEnd, winStart);
    sr = sqlGetResult(conn, query->string);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    int start = sqlUnsigned(row[0]);
	    int end = sqlUnsigned(row[1]);
	    x1 = roundingScale(start-winStart, width, baseWidth)+xOff;
	    x2 = roundingScale(end-winStart, width, baseWidth)+xOff;
	    w = x2-x1;
	    if (w <= 0)
		w = 1;
        strncpy(chromStr,row[2]+3,2);
        chromStr[2] = '\0';
        col = getChromColor(chromStr, vg);
	    vgBox(vg, x1, yOff, w, heightPer, col);
	    }
	}
    dyStringFree(&query);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
hFreeConn(&conn2);
}

Color pslItemColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color of mouseOrtho track item. */
{
struct linkedFeatures *lf = item;
char *chptr = strstr(lf->name, "chr");

if (chptr == NULL)
    {
    return(tg->ixColor);
    }
else
    {
    char chromStr[3];
    Color c;
    strncpy(chromStr, (char *)(chptr+3), 2);
    chromStr[2] = '\0';
    c = getChromColor(chromStr, vg);
    tg->ixAltColor = c;
    return c;
    }
}

void loadXenoPslWithPos(struct trackGroup *tg)
/* load up all of the psls from correct table into tg->items item list*/
{
lfFromPslsInRange(tg, winStart,winEnd, chromName, TRUE, TRUE);
}

void longXenoPslMethods(struct trackGroup *tg)
/* Fill in custom parts of blatMus - assembled mouse genome blat vs. human. */
{
char option[128]; /* Option -  rainbow chromosome color */
char optionChr[128]; /* Option -  chromosome filter */
char *optionChrStr; 
char *optionStr ;
snprintf( option, sizeof(option), "%s.color", tg->mapName);
optionStr = cartUsualString(cart, option, "on");
tg->mapItemName = mapNameFromLfExtra;
if( sameString( optionStr, "on" )) /*use chromosome coloring*/
    tg->itemColor = pslItemColor;
else
    tg->itemColor = NULL;
tg->loadItems = loadXenoPslWithPos;
}

struct linkedFeatures *lfFromChainx(struct chain *chain, int sizeMul, boolean isXeno, boolean nameGetsPos)
/* Create a linked feature item from chainx.  Pass in sizeMul=1 for DNA, 
 * sizeMul=3 for protein. */
{
/*unsigned *starts = chain->tStarts;
/*unsigned *sizes = chain->blockSizes;
/*int i, blockCount = chain->blockCount;*/
int grayIx = 1;
struct simpleFeature *sfList = NULL, *sf;
struct linkedFeatures *lf;
struct boxIn *bl, *next = NULL;
boolean rcTarget = (chain->qStrand == '-');

AllocVar(lf);
lf->score = chain->score;
lf->grayIx = grayIx;
if (nameGetsPos)
    {
    char buf[256];
    snprintf(buf, sizeof(buf), "%d", chain->id);
#ifdef OLD
    snprintf(buf, sizeof(buf), "%s:%d-%d %s:%d-%d", chain->qName, chain->qStart, chain->qEnd,
    	chain->tName, chain->tStart, chain->tEnd);
#endif /* OLD */
    lf->extra = cloneString(buf);
    snprintf(lf->name, sizeof(lf->name), "id %d %s %dk", chain->id, chain->qName, chain->qStart/1000);
    }
else
    strncpy(lf->name, chain->qName, sizeof(lf->name));
lf->orientation = '+';
if (rcTarget)
    lf->orientation = -lf->orientation;
for (bl = chain->blockList; bl != NULL; bl = bl->next)
    {
    AllocVar(sf);
    sf->start = sf->end = bl->tStart;
    sf->end = bl->tEnd;
    /*
    if (rcTarget)
        {
	int s, e;
	s = chain->tSize - sf->end;
	e = chain->tSize - sf->start;
	sf->start = s;
	sf->end = e;
	}
    */
    sf->grayIx = grayIx;
    slAddHead(&sfList, sf);
    }
slReverse(&sfList);
lf->components = sfList;
finishLf(lf);
lf->start = chain->tStart;	/* Correct for rounding errors... */
lf->end = chain->tEnd;
return lf;
}

void connectedLfFromChainsInRange(struct sqlConnection *conn, struct sqlConnection *conn2,
    struct trackGroup *tg, int start, int end, char *chromName,
    boolean isXeno, boolean nameGetsPos)
/* Return linked features from range of table after have
 * already connected to database.. */
{
char *track = tg->mapName;
char trackGap[128];
struct sqlResult *sr = NULL, *srGap = NULL;
char **row, **rowGap;
int rowOffset;
char *optionChrStr;
struct linkedFeatures *lfList = NULL, *lf;
struct boxIn *bList = NULL;
char optionChr[128]; /* Option -  chromosome filter */
char extraWhere[128] , query[256];

snprintf( trackGap, sizeof(trackGap), "%s_%sGap", chromName, track);
snprintf( optionChr, sizeof(optionChr), "%s.chromFilter", tg->mapName);
optionChrStr = cartUsualString(cart, optionChr, "All");
if (startsWith("chr",optionChrStr)) 
	{
	snprintf(extraWhere, sizeof(extraWhere), "qName = \"%s\"",optionChrStr);
	sr = hRangeQuery(conn, track, chromName, start, end, extraWhere, &rowOffset);
	}
else
	{
	snprintf(extraWhere, sizeof(extraWhere), " ");
	sr = hRangeQuery(conn, track, chromName, start, end, NULL, &rowOffset);
	}

if (sqlCountColumns(sr) < 11+rowOffset)
    errAbort("trackDb has incorrect table type for table \"%s\"",
	     tg->mapName);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct chain *chain = chainLoad(row+rowOffset);

    sprintf(query, "SELECT tName, tStart, tEnd, qStart from %s where chainId = %d and tStart < %d and tEnd > %d and tStart >= %d and tEnd <= %d",
                trackGap, chain->id, chain->tEnd, chain->tStart, winStart, winEnd);
    srGap = sqlGetResult(conn2, query);
    while ((rowGap = sqlNextRow(srGap)) != NULL)
        {
        struct chainGap *cg = chainGapLoad(rowGap);

        struct boxIn *b;
        AllocVar(b);
        b->tStart = cg->tStart;
        b->tEnd = cg->tEnd;
        b->qStart = cg->qStart;
        b->qEnd = cg->qStart + cg->tEnd - cg->tStart;
        slAddHead(&bList, b);
        }
    slReverse(&bList);
    chain->blockList = bList;
    lf = lfFromChainx(chain, 0, isXeno, nameGetsPos);
    slAddHead(&lfList, lf);
    bList = NULL;
    chainFree(&chain);
    }
slReverse(&lfList);
if (limitVisibility(tg, lfList) == tvFull)
    slSort(&lfList, linkedFeaturesCmpStart);
if (tg->extraUiData)
    filterMrna(tg, &lfList);
sqlFreeResult(&sr);
sqlFreeResult(&srGap);
tg->items = lfList;
}

void lfFromChainsInRange(struct trackGroup *tg, int start, int end, 
	char *chromName, boolean isXeno, boolean nameGetsPos)
/* Return linked features from range of table. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlConnection *conn2 = hAllocConn();
connectedLfFromChainsInRange(conn, conn2, tg, start, end, chromName, 
	isXeno, nameGetsPos);
hFreeConn(&conn);
hFreeConn(&conn2);
}

void loadChainWithPos(struct trackGroup *tg)
/* load up all of the chains from correct table into tg->items item list*/
{
lfFromChainsInRange(tg, winStart,winEnd, chromName, TRUE, TRUE);
}

void chainMethods(struct trackGroup *tg)
/* Fill in custom parts of alignment chains */
{
char option[128]; /* Option -  rainbow chromosome color */
char optionChr[128]; /* Option -  chromosome filter */
char *optionChrStr; 
char *optionStr ;
snprintf( option, sizeof(option), "%s.color", tg->mapName);
optionStr = cartUsualString(cart, option, "on");
tg->mapItemName = mapNameFromLfExtra;
if( sameString( optionStr, "on" )) /*use chromosome coloring*/
    tg->itemColor = pslItemColor;
else
    tg->itemColor = NULL;
tg->loadItems = loadChainWithPos;
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

Color rnaGeneColor(struct trackGroup *tg, void *item, struct vGfx *vg)
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

void rnaGeneMethods(struct trackGroup *tg)
/* Make track group for rna genes . */
{
tg->loadItems = loadRnaGene;
tg->freeItems = freeRnaGene;
tg->itemName = rnaGeneName;
tg->itemColor = rnaGeneColor;
}


Color stsColor(struct vGfx *vg, int altColor, 
	char *genethonChrom, char *marshfieldChrom, 
	char *fishChrom, int ppt)
/* Return color given info about marker. */
{
if (genethonChrom[0] != '0' || marshfieldChrom[0] != '0')
    {
    if (ppt >= 900)
       return MG_BLUE;
    else
       return altColor;
    }
else if (fishChrom[0] != '0')
    {
    static int greenIx = -1;
    if (greenIx < 0)
        greenIx = vgFindColorIx(vg, 0, 200, 0);
    return greenIx;
    }
else
    {
    if (ppt >= 900)
       return MG_BLACK;
    else
       return MG_GRAY;
    }
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

Color stsMarkerColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color of stsMarker track item. */
{
struct stsMarker *el = item;
return stsColor(vg, tg->ixAltColor, el->genethonChrom, el->marshfieldChrom,
    el->fishChrom, el->score);
}

void stsMarkerMethods(struct trackGroup *tg)
/* Make track group for sts markers. */
{
tg->loadItems = loadStsMarker;
tg->freeItems = freeStsMarker;
tg->itemColor = stsMarkerColor;
}

char *stsMapFilter;
char *stsMapMap;
enum stsMapOptEnum stsMapType;
int stsMapFilterColor = MG_BLACK;

boolean stsMapFilterItem(struct trackGroup *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct stsMap *el = item;
switch (stsMapType)
    {
    case smoeGenetic:
	return el->genethonChrom[0] != '0' || el->marshfieldChrom[0] != '0'
	    || el->decodeChrom[0] != '0';
        break;
    case smoeGenethon:
	return el->genethonChrom[0] != '0';
        break;
    case smoeMarshfield:
	return el->marshfieldChrom[0] != '0';
        break;
    case smoeDecode:
	return el->decodeChrom[0] != '0';
        break;
    case smoeGm99:
	return el->gm99Gb4Chrom[0] != '0';
        break;
    case smoeWiYac:
	return el->wiYacChrom[0] != '0';
        break;
    case smoeWiRh:
	return el->wiRhChrom[0] != '0';
        break;
    case smoeTng:
	return el->shgcTngChrom[0] != '0';
        break;
    default:
	return FALSE;
        break;
    }
}


void loadStsMap(struct trackGroup *tg)
/* Load up stsMarkers from database table to trackGroup items. */
{
stsMapFilter = cartUsualString(cart, "stsMap.filter", "blue");
stsMapMap = cartUsualString(cart, "stsMap.type", smoeEnumToString(0));
stsMapType = smoeStringToEnum(stsMapMap);
bedLoadItem(tg, "stsMap", (ItemLoader)stsMapLoad);
filterItems(tg, stsMapFilterItem, stsMapFilter);
stsMapFilterColor = getFilterColor(stsMapFilter, MG_BLACK);
}

void loadStsMap28(struct trackGroup *tg)
/* Load up stsMarkers from database table to trackGroup items. */
{
stsMapFilter = cartUsualString(cart, "stsMap.filter", "blue");
stsMapMap = cartUsualString(cart, "stsMap.type", smoeEnumToString(0));
stsMapType = smoeStringToEnum(stsMapMap);
bedLoadItem(tg, "stsMap", (ItemLoader)stsMapLoad28);
filterItems(tg, stsMapFilterItem, stsMapFilter);
stsMapFilterColor = getFilterColor(stsMapFilter, MG_BLACK);
}

void freeStsMap(struct trackGroup *tg)
/* Free up stsMap items. */
{
stsMapFreeList((struct stsMap**)&tg->items);
}

Color stsMapColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color of stsMap track item. */
{
if (stsMapFilterItem(tg, item))
    return stsMapFilterColor;
else
    {
    struct stsMap *el = item;
    if (el->score >= 900)
        return MG_BLACK;
    else
        return MG_GRAY;
    }
}


void stsMapMethods(struct trackGroup *tg)
/* Make track group for sts markers. */
{
struct sqlConnection *conn = hAllocConn();
if (sqlCountRows(conn, "stsMap") == 26) 
    {
    tg->loadItems = loadStsMap;
    }
else
    {
    tg->loadItems = loadStsMap28;
    }
hFreeConn(&conn);
tg->freeItems = freeStsMap;
tg->itemColor = stsMapColor;
}

void loadGenMapDb(struct trackGroup *tg)
/* Load up genMapDb from database table to trackGroup items. */
{
bedLoadItem(tg, "genMapDb", (ItemLoader)genMapDbLoad);
}

void freeGenMapDb(struct trackGroup *tg)
/* Free up genMapDb items. */
{
genMapDbFreeList((struct genMapDb**)&tg->items);
}

void genMapDbMethods(struct trackGroup *tg)
/* Make track group for GenMapDb Clones */
{
tg->loadItems = loadGenMapDb;
tg->freeItems = freeGenMapDb;
}

char *fishClonesFilter;
char *fishClonesMap;
enum fishClonesOptEnum fishClonesType;
int fishClonesFilterColor = MG_GREEN;

boolean fishClonesFilterItem(struct trackGroup *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct fishClones *el = item;
int i;
switch (fishClonesType)
    {
    case fcoeFHCRC:
        for (i = 0; i < el->placeCount; i++)  
	    if (sameString(el->labs[i],"FHCRC")) 
	        return TRUE;
        return FALSE;
        break;
    case fcoeNCI:
        for (i = 0; i < el->placeCount; i++)  
	    if (sameString(el->labs[i],"NCI")) 
	        return TRUE;
        return FALSE;
        break;
    case fcoeSC:
        for (i = 0; i < el->placeCount; i++)  
	    if (sameString(el->labs[i],"SC")) 
	        return TRUE;
        return FALSE;
        break;
    case fcoeRPCI:
        for (i = 0; i < el->placeCount; i++)  
	    if (sameString(el->labs[i],"RPCI")) 
	        return TRUE;
        return FALSE;
        break;
    case fcoeCSMC:
        for (i = 0; i < el->placeCount; i++)  
	    if (sameString(el->labs[i],"CSMC")) 
	        return TRUE;
        return FALSE;
        break;
    case fcoeLANL:
        for (i = 0; i < el->placeCount; i++)  
	    if (sameString(el->labs[i],"LANL")) 
	        return TRUE;
        return FALSE;
        break;
    case fcoeUCSF:
        for (i = 0; i < el->placeCount; i++)  
	    if (sameString(el->labs[i],"UCSF")) 
	        return TRUE;
        return FALSE;
        break;
    default:
	return FALSE;
        break;
    }
}

void loadFishClones(struct trackGroup *tg)
/* Load up fishClones from database table to trackGroup items. */
{
fishClonesFilter = cartUsualString(cart, "fishClones.filter", "green");
fishClonesMap = cartUsualString(cart, "fishClones.type", fcoeEnumToString(0));
fishClonesType = fcoeStringToEnum(fishClonesMap);
bedLoadItem(tg, "fishClones", (ItemLoader)fishClonesLoad);
filterItems(tg, fishClonesFilterItem, fishClonesFilter);
fishClonesFilterColor = getFilterColor(fishClonesFilter, 0);
}


void freeFishClones(struct trackGroup *tg)
/* Free up fishClones items. */
{
fishClonesFreeList((struct fishClones**)&tg->items);
}

Color fishClonesColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color of fishClones track item. */
{
if ((fishClonesFilterItem(tg, item)) && (fishClonesFilterColor))
    return fishClonesFilterColor;
else
    return tg->ixColor;
}

void fishClonesMethods(struct trackGroup *tg)
/* Make track group for FISH clones. */
{
tg->loadItems = loadFishClones;
tg->freeItems = freeFishClones;
tg->itemColor = fishClonesColor;
}


void loadSyntenyBerk(struct trackGroup *tg)
{
bedLoadItem(tg, "syntenyBerk", (ItemLoader)syntenyBerkLoad);
slSort(&tg->items, bedCmp);
}

void freeSyntenyBerk(struct trackGroup *tg)
{
syntenyBerkFreeList((struct syntenyBerk**)&tg->items);
}

void loadSyntenySanger(struct trackGroup *tg)
{
bedLoadItem(tg, "syntenySanger", (ItemLoader)syntenySangerLoad);
slSort(&tg->items, bedCmp);
}

void freeSyntenySanger(struct trackGroup *tg)
{
syntenySangerFreeList((struct syntenySanger**)&tg->items);
}

void loadSynteny100000(struct trackGroup *tg)
{
bedLoadItem(tg, "synteny100000", (ItemLoader)synteny100000Load);
slSort(&tg->items, bedCmp);
}

void freeSynteny100000(struct trackGroup *tg)
{
synteny100000FreeList((struct synteny100000**)&tg->items);
}

void loadMouseOrtho(struct trackGroup *tg)
{
bedLoadItem(tg, "mouseOrtho", (ItemLoader)mouseOrthoLoad);
slSort(&tg->items, bedCmpPlusScore);
}

void freeMouseOrtho(struct trackGroup *tg)
{
mouseOrthoFreeList((struct mouseOrtho**)&tg->items);
}

Color mouseOrthoItemColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color of psl track item based on chromsome. */
{
char chromStr[20];     
struct mouseOrtho *ms = item;
if (strlen(ms->name) == 8)
{
    strncpy(chromStr,(char *)(ms->name+1),1);
    chromStr[1] = '\0';
}
else if (strlen(ms->name) == 9)
{
    strncpy(chromStr,(char *)(ms->name+1),2);
    chromStr[2] = '\0';
}
else
    strncpy(chromStr,ms->name,2);
return ((Color)getChromColor(chromStr, vg));
}

void loadHumanParalog(struct trackGroup *tg)
{
bedLoadItem(tg, "humanParalog", (ItemLoader)humanParalogLoad);
slSort(&tg->items, bedCmpPlusScore);
}

void freeHumanParalog(struct trackGroup *tg)
{
humanParalogFreeList((struct humanParalog**)&tg->items);
}

Color humanParalogItemColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color of psl track item based on chromsome. */
{
char chromStr[20];     
struct humanParalog *ms = item;
if (strlen(ms->name) == 8)
{
    strncpy(chromStr,(char *)(ms->name+1),1);
    chromStr[1] = '\0';
}
else if (strlen(ms->name) == 9)
{
    strncpy(chromStr,(char *)(ms->name+1),2);
    chromStr[2] = '\0';
}
else
    strncpy(chromStr,ms->name,2);
return ((Color)getChromColor(chromStr, vg));
}
Color syntenyItemColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color of psl track item based on chromsome. */
{
char chromStr[20];     
struct bed *ms = item;
if (strlen(ms->name) == 8)
    {
    strncpy(chromStr,(char *)(ms->name+1),1);
    chromStr[1] = '\0';
    }
else if (strlen(ms->name) == 9)
    {
    strncpy(chromStr,(char *)(ms->name+1),2);
    chromStr[2] = '\0';
    }
else
    {
    strncpy(chromStr,ms->name+3,2);
    chromStr[2] = '\0';
    }
return ((Color)getChromColor(chromStr, vg));
}

Color syntenyBerkItemColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color of psl track item based on chromsome. */
{
char chromStr[20];     
char *chptr;
struct syntenyBerk *ms = item;
if (strlen(ms->name) == 8)
    {
    strncpy(chromStr,(char *)(ms->name+1),1);
    chromStr[1] = '\0';
    }
else if (strlen(ms->name) == 9)
    {
    strncpy(chromStr,(char *)(ms->name+1),2);
    chromStr[2] = '\0';
    }
else
    {
    chptr = strstr(ms->name,"chr");
    strncpy(chromStr,(char *)(chptr+3),2);
    chromStr[2] = '\0';
    }
return ((Color)getChromColor(chromStr, vg));
}

Color syntenySangerItemColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color of psl track item based on chromsome. */
{
char chromStr[20];     
char *chptr;
struct syntenySanger *ms = item;
if (strlen(ms->name) == 8)
    {
    strncpy(chromStr,(char *)(ms->name+1),1);
    chromStr[1] = '\0';
    }
else if (strlen(ms->name) == 9)
    {
    strncpy(chromStr,(char *)(ms->name+1),2);
    chromStr[2] = '\0';
    }
else
    {
    chptr = strstr(ms->name,"chr");
    strncpy(chromStr,(char *)(chptr+3),2);
    chromStr[2] = '\0';
    }
return ((Color)getChromColor(chromStr, vg));
}

void loadMouseSyn(struct trackGroup *tg)
/* Load up mouseSyn from database table to trackGroup items. */
{
bedLoadItem(tg, "mouseSyn", (ItemLoader)mouseSynLoad);
}

void synteny100000Methods(struct trackGroup *tg)
{
tg->loadItems = loadSynteny100000;
tg->freeItems = freeSynteny100000;
tg->itemColor = syntenyItemColor;
tg->drawName = FALSE;
tg->subType = lfWithBarbs ;
}

void syntenyBerkMethods(struct trackGroup *tg)
{
tg->loadItems = loadSyntenyBerk;
tg->freeItems = freeSyntenyBerk;
tg->itemColor = syntenyBerkItemColor;
tg->drawName = FALSE;
tg->subType = lfWithBarbs ;
}

void syntenySangerMethods(struct trackGroup *tg)
{
tg->loadItems = loadSyntenySanger;
tg->freeItems = freeSyntenySanger;
tg->itemColor = syntenySangerItemColor;
tg->drawName = FALSE;
tg->subType = lfWithBarbs ;
}

void mouseOrthoMethods(struct trackGroup *tg)
{
char option[128];
char *optionStr ;
tg->loadItems = loadMouseOrtho;
tg->freeItems = freeMouseOrtho;

snprintf( option, sizeof(option), "%s.color", tg->mapName);
optionStr = cartUsualString(cart, option, "on");
if( sameString( optionStr, "on" )) /*use anti-aliasing*/
    tg->itemColor = mouseOrthoItemColor;
else
    tg->itemColor = NULL;
tg->drawName = TRUE;
}

void humanParalogMethods(struct trackGroup *tg)
{
char option[128];
char *optionStr ;
tg->loadItems = loadHumanParalog;
tg->freeItems = freeHumanParalog;

snprintf( option, sizeof(option), "%s.color", tg->mapName);
optionStr = cartUsualString(cart, option, "on");
if( sameString( optionStr, "on" )) /*use anti-aliasing*/
    tg->itemColor = humanParalogItemColor;
else
    tg->itemColor = NULL;
tg->drawName = TRUE;
}

void freeMouseSyn(struct trackGroup *tg)
/* Free up mouseSyn items. */
{
mouseSynFreeList((struct mouseSyn**)&tg->items);
}

Color mouseSynItemColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color of mouseSyn track item. */
{
char chromStr[20];     
struct mouseSyn *ms = item;

strncpy(chromStr, ms->name+strlen("mouse "), 2);
chromStr[2] = '\0';
return ((Color)getChromColor(chromStr, vg));
}

void mouseSynMethods(struct trackGroup *tg)
/* Make track group for mouseSyn. */
{
tg->loadItems = loadMouseSyn;
tg->freeItems = freeMouseSyn;
tg->itemColor = mouseSynItemColor;
tg->drawName = TRUE;
}

void loadMouseSynWhd(struct trackGroup *tg)
/* Load up mouseSynWhd from database table to trackGroup items. */
{
bedLoadItem(tg, "mouseSynWhd", (ItemLoader)mouseSynWhdLoad);
}

void freeMouseSynWhd(struct trackGroup *tg)
/* Free up mouseSynWhd items. */
{
mouseSynWhdFreeList((struct mouseSynWhd**)&tg->items);
}

Color mouseSynWhdItemColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color of mouseSynWhd track item. */
{
char chromStr[20];
struct mouseSynWhd *ms = item;

if (startsWith("chr", ms->name))
    {
    strncpy(chromStr, ms->name+strlen("chr"), 2);
    chromStr[2] = '\0';
    return((Color)getChromColor(chromStr, vg));
    }
else
    {
    return(tg->ixColor);
    }
}

void mouseSynWhdMethods(struct trackGroup *tg)
/* Make track group for mouseSyn. */
{
tg->loadItems = loadMouseSynWhd;
tg->freeItems = freeMouseSynWhd;
tg->itemColor = mouseSynWhdItemColor;
tg->subType = lfWithBarbs;
}

void loadEnsPhusionBlast(struct trackGroup *tg)
/* Load up ensPhusionBlast from database table to trackGroup items. */
{
struct ensPhusionBlast *epb;
char *ptr;
char buf[16];

bedLoadItem(tg, tg->mapName, (ItemLoader)ensPhusionBlastLoad);
// for name, append abbreviated starting position to the xeno chrom:
for (epb=tg->items;  epb != NULL;  epb=epb->next)
    {
    ptr = strchr(epb->name, '.');
    if (ptr == NULL)
	ptr = epb->name;
    else
	ptr++;
    snprintf(buf, sizeof(buf), "%s %dk", ptr, (int)(epb->xenoStart/1000));
    free(epb->name);
    epb->name = cloneString(buf);
    }
}

void freeEnsPhusionBlast(struct trackGroup *tg)
/* Free up ensPhusionBlast items. */
{
ensPhusionBlastFreeList((struct ensPhusionBlast**)&tg->items);
}

Color ensPhusionBlastItemColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Return color of ensPhusionBlast track item. */
{
struct ensPhusionBlast *epb = item;
char *ptr;
char chromStr[20];

if ((ptr = strstr(epb->name, "chr")) != NULL)
    {
    strncpy(chromStr, ptr+strlen("chr"), 2);
    chromStr[2] = '\0';
    return((Color)getChromColor(chromStr, vg));
    }
else
    {
    return(tg->ixColor);
    }
}

void ensPhusionBlastMethods(struct trackGroup *tg)
/* Make track group for mouseSyn. */
{
tg->loadItems = loadEnsPhusionBlast;
tg->freeItems = freeEnsPhusionBlast;
tg->itemColor = ensPhusionBlastItemColor;
tg->subType = lfWithBarbs;
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


static void wabaDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
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
	    vgBox(vg, x1, y, w, heightPer, color);
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

void wabaMethods(struct trackGroup *tg)
/* Return track with fields shared by waba-based 
 * alignment tracks filled in. */
{
tg->loadItems = wabaLoad;
tg->freeItems = wabaFree;
tg->drawItems = wabaDraw;
tg->itemName = wabaName;
tg->mapItemName = wabaName;
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = wabaItemStart;
tg->itemEnd = wabaItemEnd;
}

void tetWabaMethods(struct trackGroup *tg)
/* Make track group for Tetraodon alignments. */
{
wabaMethods(tg);
tg->customPt = "_tet_waba";
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
        struct vGfx *vg, int xOff, int yOff, int width, 
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
    vgBox(vg, x1, y, w, heightPer, color);
    s = abbreviateContig(ctg->contig, tl.font, w);
    if (s != NULL)
	vgTextCentered(vg, x1, y, w, heightPer, MG_WHITE, tl.font, s);
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

void contigMethods(struct trackGroup *tg)
/* Make track group for contig */
{
tg->loadItems = contigLoad;
tg->freeItems = contigFree;
tg->drawItems = contigDraw;
tg->itemName = contigName;
tg->mapItemName = contigName;
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = contigItemStart;
tg->itemEnd = contigItemEnd;
}

void bactigLoad(struct trackGroup *tg)
/* Load up bactigs from database table to trackGroup items. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct bactigPos *bactigList = NULL, *bactig;
int rowOffset;

/* Get the bactigs and load into tg->items. */
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd,
		 NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bactig = bactigPosLoad(row+rowOffset);
    slAddHead(&bactigList, bactig);
    }
slReverse(&bactigList);
sqlFreeResult(&sr);
hFreeConn(&conn);
tg->items = bactigList;
}

void bactigFree(struct trackGroup *tg)
/* Free up bactigTrackGroup items. */
{
bactigPosFreeList((struct bactigPos**)&tg->items);
}

char *abbreviateBactig(char *string, MgFont *font, int width)
/* Return a string abbreviated enough to fit into space. */
{
int textWidth;

/* There's no abbreviating to do for bactig names; just return the name 
 * if it fits, NULL if it doesn't fit. */
textWidth = mgFontStringWidth(font, string);
if (textWidth <= width)
    return string;
return NULL;
}

static void bactigDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw bactig items. */
{
int baseWidth = seqEnd - seqStart;
struct bactigPos *bactig;
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
for (bactig = tg->items; bactig != NULL; bactig = bactig->next)
    {
    x1 = round((double)((int)bactig->chromStart-winStart)*scale) + xOff;
    x2 = round((double)((int)bactig->chromEnd-winStart)*scale) + xOff;
    /* Clip here so that text will tend to be more visible... */
    if (x1 < xOff)
	x1 = xOff;
    if (x2 > xOff + width)
	x2 = xOff + width;
    w = x2-x1;
    if (w < 1)
	w = 1;
    vgBox(vg, x1, y, w, heightPer, color);
    s = abbreviateBactig(bactig->name, tl.font, w);
    if (s != NULL)
	vgTextCentered(vg, x1, y, w, heightPer, MG_WHITE, tl.font, s);
    if (isFull)
	y += lineHeight;
    else 
	{
	mapBoxHc(bactig->chromStart, bactig->chromEnd, x1,y,w,heightPer,
		 tg->mapName, bactig->name, bactig->name);
	}
    ++ix;
    }
}

void bactigMethods(struct trackGroup *tg)
/* Make track group for bactigPos */
{
tg->loadItems = bactigLoad;
tg->freeItems = bactigFree;
tg->drawItems = bactigDraw;
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
    struct vGfx *vg, int xOff, int yOff, int width,
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
    vgBox(vg, x1, y, w, heightPer, col);
    textWidth = mgFontStringWidth(font, s);
    if (textWidth <= w)
	vgTextCentered(vg, x1, y, w, heightPer, MG_WHITE, font, s);
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
    {
    warn("Unknown stage %c (%d)", stage, stage);
    inc = draftInc;
    }
for (i=0; i<width; ++i)
   b[i] = inc[b[i]];
}

static void cloneDenseDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
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
vgVerticalSmear(vg,xOff,yOff,width,lineHeight,aveCounts,TRUE);
freeMem(useCounts);
freeMem(aveCounts);
}

static void cloneFullDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
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
int hilight = MG_CYAN;
boolean gotTiling = hTableExists("tilingPath");
struct sqlConnection *conn = NULL;
int bgColor;
char accOnly[64];


if (gotTiling)
    conn = hAllocConn();
for (ci = tg->items; ci != NULL; ci = ci->next)
    {
    bgColor = light;
    if (gotTiling)
	{
	char query[256], buf[256];
	strcpy(accOnly, ci->name);
	chopSuffix(accOnly);
	sprintf(query, "select accession from tilingPath where accession = '%s'", accOnly);
        if (sqlQuickQuery(conn, query, buf, sizeof(buf)) != NULL)
	    bgColor = hilight;
	}
    if (!tooBig)
	oneHeight = oneOrRowCount(ci)*lineHeight+2;
    else
	oneHeight = lineHeight;
    x1 = roundingScale(ci->cloneStart-winStart, width, baseWidth)+xOff;
    x2 = roundingScale(ci->cloneEnd-winStart, width, baseWidth)+xOff;
    w = x2-x1;
    vgBox(vg, x1, y, w, oneHeight-1, bgColor);
    if (!tooBig)
	drawOneClone(ci, seqStart, seqEnd, vg, xOff, y+1, width, font, lineHeight, 
		color, TRUE, tg->subType);
    else
	drawOneClone(ci, seqStart, seqEnd, vg, xOff, y, width, font, oneHeight-1, 
		color, FALSE, tg->subType);
    y += oneHeight;
    }
hFreeConn(&conn);
}

static void cloneDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw clone items. */
{
if (vis == tvFull)
    cloneFullDraw(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
else
    cloneDenseDraw(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
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
    int rowOffset;

    /* Load in clone extents from database. */
    glCloneHash = newHash(12);
    sr = hRangeQuery(conn, "clonePos", chromName, winStart, winEnd, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	clonePosStaticLoad(row+rowOffset, &cp);
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

    /* Load in gl from database and sort them by clone. */
    sr = hRangeQuery(conn, "gl", chromName, winStart, winEnd, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	gl = glLoad(row+rowOffset);
	fragName = gl->frag;
	strcpy(cloneName, fragName);
	s = strchr(cloneName, '.');
	if (s != NULL)
	    s = strchr(s, '_');
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

void coverageMethods(struct trackGroup *tg)
/* Make track group for golden path positions of all frags. */
{
tg->loadItems = coverageLoad;
tg->freeItems = coverageFree;
tg->drawItems = cloneDraw;
tg->itemName = cloneName;
tg->mapItemName = cloneName;
tg->totalHeight = cloneTotalHeight;
tg->itemHeight = cloneItemHeight;
tg->itemStart = cloneItemStart;
tg->itemEnd = cloneItemEnd;
}


void gapLoad(struct trackGroup *tg)
/* Load up clone alignments from database tables and organize. */
{
bedLoadItem(tg, "gap", (ItemLoader)agpGapLoad);
}

void gapFree(struct trackGroup *tg)
/* Free up gap items. */
{
agpGapFreeList((struct agpGap**)&tg->items);
}

char *gapName(struct trackGroup *tg, void *item)
/* Return name of gap track item. */
{
static char buf[24];
struct agpGap *gap = item;
sprintf(buf, "%s %s", gap->type, gap->bridge);
return buf;
}

static void gapDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
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
	vgBox(vg, x1, y, w, heightPer, color);
    else  /* Leave white line in middle of bridged gaps. */
        {
	vgBox(vg, x1, y, w, halfSize, color);
	vgBox(vg, x1, y+heightPer-halfSize, w, halfSize, color);
	}
    if (isFull)
	{
	char name[32];
	sprintf(name, "%s", item->type);
	mapBoxHc(item->chromStart, item->chromEnd, x1, y, w, heightPer, tg->mapName,
	    name, name);
	y += lineHeight;
	}
    }
}


void gapMethods(struct trackGroup *tg)
/* Make track group for positions of all gaps. */
{
tg->loadItems = gapLoad;
tg->freeItems = gapFree;
tg->drawItems = gapDraw;
tg->itemName = gapName;
tg->mapItemName = gapName;
}

int pslWScoreScale(struct pslWScore *psl, boolean isXeno, float maxScore)
/* takes the score field and scales it to the correct shade using maxShade and maxScore */
{
/* move from float to int by multiplying by 100 */
int score = (int)(100 * psl->score);
int level;
level = grayInRange(score, 0, (int)(100 * maxScore));
if(level==1) level++;
return level;
}

struct linkedFeatures *lfFromPslWScore(struct pslWScore *psl, int sizeMul, boolean isXeno, float maxScore)
/* Create a linked feature item from pslx.  Pass in sizeMul=1 for DNA, 
 * sizeMul=3 for protein. */
{
unsigned *starts = psl->tStarts;
unsigned *sizes = psl->blockSizes;
int i, blockCount = psl->blockCount;
int grayIx = pslWScoreScale(psl, isXeno, maxScore);
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

struct linkedFeatures *lfFromPslsWScoresInRange(char *table, int start, int end, char *chromName, boolean isXeno, float maxScore)
/* Return linked features from range of table with the scores scaled appriately */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
int rowOffset;
struct linkedFeatures *lfList = NULL, *lf;

sr = hRangeQuery(conn,table,chromName,start,end,NULL, &rowOffset);
while((row = sqlNextRow(sr)) != NULL)
    {
    struct pslWScore *pslWS = pslWScoreLoad(row);
    lf = lfFromPslWScore(pslWS, 1, FALSE, maxScore);
    slAddHead(&lfList, lf);
    pslWScoreFree(&pslWS);
    }
slReverse(&lfList);
sqlFreeResult(&sr);
hFreeConn(&conn);
return lfList;
}

void loadUniGeneAli(struct trackGroup *tg)
{
tg->items = lfFromPslsWScoresInRange("uniGene", winStart, winEnd, chromName,FALSE, 1.0);
}

void uniGeneMethods(struct trackGroup *tg)
{
linkedFeaturesMethods(tg);
tg->loadItems = loadUniGeneAli;
tg->colorShades = shadesOfGray;
}

char *perlegenName(struct trackGroup *tg, void *item)
/* return the actual perlegen name, in form xx/yyyy cut off xx/ return yyyy */
{
char * name;
struct linkedFeatures *lf = item;
name = strstr(lf->name, "/");
name ++;
if(name != NULL)
    return name;
else
    return "unknown";
}

Color perlegenColor(struct trackGroup *tg, struct linkedFeatures *lf, struct simpleFeature *sf, struct vGfx *vg)
/* if it is the start or stop blocks make the color the shades
 * otherwise use black */
{
if(lf->components == sf || (sf->next == NULL))
    return tg->colorShades[lf->grayIx+tg->subType];
else
    return blackIndex();
}

int perlegenHeight(struct trackGroup *tg, struct linkedFeatures *lf, struct simpleFeature *sf) 
/* if the item isn't the first or the last make it smaller */
{
if(sf == lf->components || sf->next == NULL)
    return tg->heightPer;
else 
    return (tg->heightPer-4);
}

static void perlegenLinkedFeaturesDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* currently this routine is adapted from Terry's linkedFeatureSeriesDraw() routine.
 * it could be cleaned up some but more importantly it should be integrated back 
 * into the main draw routine */
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
int s, e, e2, s2;
int itemOff, itemHeight;
boolean isFull = (vis == tvFull);
Color *shades = tg->colorShades;
Color bColor = tg->ixAltColor;
double scale = width/(double)baseWidth;
boolean isXeno = tg->subType == lfSubXeno;
boolean hideLine = (vis == tvDense && tg->subType == lfSubXeno);
int midY = y + midLineOff;
int compCount = 0;
int w;
int prevEnd = -1;

lf=tg->items;    
for(lf = tg->items; lf != NULL; lf = lf->next) 
    {
    if (lf->components != NULL && !hideLine)
	{
	x1 = round((double)((int)lf->start-winStart)*scale) + xOff;
	x2 = round((double)((int)lf->end-winStart)*scale) + xOff;
	w = x2-x1;
	color =  shades[lf->grayIx+isXeno];
	/* draw perlgen thick line ... */
	vgBox(vg, x1, y+shortOff+1, w, shortHeight-2, color);
	}
    for (sf = lf->components; sf != NULL; sf = sf->next)
	{
	color = perlegenColor(tg, lf, sf, vg);
	heightPer = perlegenHeight(tg, lf, sf);
	s = sf->start;
	e = sf->end;
	drawScaledBox(vg, s, e, scale, xOff, y+((tg->heightPer - heightPer)/2), heightPer, color);
	/* if we're at the stop or start of a linked feature add a black tick for the snp 
	 * in addtion to the larger tic of shaded color */
	if(heightPer == tg->heightPer)
	    drawScaledBox(vg, s, e, scale, xOff, y+((tg->heightPer - heightPer - 4)/2), (heightPer -4), blackIndex());
	}
    if (isFull)
	y += lineHeight;
    }
}

void altGraphMapItem(struct trackGroup *tg, void *item, char *itemName, int start, int end, 
		    int x, int y, int width, int height)
/* create a link for each altGraph that centers it on browser with 
   known genes, human mrnas, and intron est tracks open */
{
struct altGraph *ag = item;
if(tg->visibility == tvFull)
    {
    hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
    hPrintf("HREF=\"%s?position=%s:%d-%d&mrna=full&intronEst=full&refGene=full&altGraph=full&%s\"",
	   hgTracksName(), ag->tName, ag->tStart, ag->tEnd, cartSidUrlString(cart));
    hPrintf(" ALT=\"Zoom to browser coordinates of altGraph\">\n");
    }
}

static void altGraphDraw(struct trackGroup *tg, int seqStart, int seqEnd,         
			 struct vGfx *vg, int xOff, int yOff, int width, 
			 MgFont *font, Color color, enum trackVisibility vis)
/* Draws the blocks for an alt-spliced gene and the connections */
{
int baseWidth = seqEnd - seqStart;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2;
boolean isFull = (vis == tvFull);
double scale = width/(double)baseWidth;
int i;
double y1, y2;
int midLineOff = heightPer/2;
struct altGraph *ag=NULL, *agList = NULL;
int s =0, e=0;
agList = tg->items;
for(ag = agList; ag != NULL; ag = ag->next)
    {	   
    x1 = round((double)((int)ag->tStart-winStart)*scale) + xOff;
    x2 = round((double)((int)ag->tEnd-winStart)*scale) + xOff;
    if(tg->mapsSelf && tg->mapItem)
	{
	tg->mapItem(tg, ag, "notUsed", ag->tStart, ag->tEnd, xOff, y, width, heightPer);
	}
    if(!isFull && (x2-x1 > 0))
	{
	vgBox(vg, x1, yOff+tg->heightPer/2, x2-x1, 1, MG_BLACK);
	}
    for(i=0; i< ag->edgeCount; i++)
	{
        /* draw exons as boxes */
	if( (ag->vTypes[ag->edgeStarts[i]] == ggHardStart || ag->vTypes[ag->edgeStarts[i]] == ggSoftStart)  
	    && (ag->vTypes[ag->edgeEnds[i]] == ggHardEnd || ag->vTypes[ag->edgeEnds[i]] == ggSoftEnd)) 
	    {
	    s = ag->vPositions[ag->edgeStarts[i]] + ag->tStart;
	    e = ag->vPositions[ag->edgeEnds[i]] + ag->tStart;
	    if(isFull)
		drawScaledBox(vg, s, e, scale, xOff, y+(heightPer/2), heightPer/2, MG_BLACK);
	    else
		drawScaledBox(vg, s, e, scale, xOff, y, heightPer, MG_BLACK);
	    }
	if(isFull)
	    {
	    /* draw introns as arcs */
	    if( (ag->vTypes[ag->edgeStarts[i]] == ggHardEnd || ag->vTypes[ag->edgeStarts[i]] == ggSoftEnd) 
		&& (ag->vTypes[ag->edgeEnds[i]] == ggHardStart || ag->vTypes[ag->edgeEnds[i]] == ggSoftStart))
		{
		int x1, x2;
		int midX;   
		int midY = y + heightPer/2;
		s = ag->vPositions[ag->edgeStarts[i]] + ag->tStart;
		e = ag->vPositions[ag->edgeEnds[i]] + ag->tStart;
		x1 = round((double)((int) s - winStart)*scale) + xOff;
		x2 = round((double)((int) e - winStart)*scale) + xOff;
		midX = (x1+x2)/2;
		vgLine(vg, x1, midY, midX, y, MG_BLACK);
		vgLine(vg, midX, y, x2, midY, MG_BLACK);
		}
	    }
	}
    if(isFull)
	y += lineHeight;
    }
}


void altGraphLoadItems(struct trackGroup *tg)
/* load the altGraph data to a trackGroup */
{
struct sqlConnection *conn = hAllocConn();
int rowOffSet;
char **row;
struct altGraph *ag=NULL, *agList=NULL;
struct sqlResult *sr = hRangeQuery(conn, tg->mapName, chromName,
				   winStart, winEnd, NULL, &rowOffSet);
while((row = sqlNextRow(sr)) != NULL)
    {
    ag = altGraphLoad(row + rowOffSet);
    slAddHead(&agList, ag);
    }
slReverse(&agList);
sqlFreeResult(&sr);
tg->items = agList;
}

void altGraphFreeItems(struct trackGroup *tg)
/* free up tha altGraph items in tg->items */
{
altGraphFreeList((struct altGraph**)(&tg->items));
}

static int altGraphFixedTotalHeight(struct trackGroup *tg, enum trackVisibility vis)
/* set track height to 2 * font size if full , 1 * if dense
*/
{
switch (vis)
    {
    case tvFull:
	tg->lineHeight  = 2 * mgFontLineHeight(tl.font)+1;
	tg->heightPer = tg->lineHeight -1;
	tg->height = slCount(tg->items) * tg->lineHeight;
	break;
    case tvDense:
	tg->lineHeight  = mgFontLineHeight(tl.font)+1;
	tg->heightPer = tg->lineHeight -1;
	tg->height = tg->lineHeight;
	break;
    }
return tg->height;
}

char *altGraphItemName(struct trackGroup *tg, void *item)
/* returns the number of alternative splice paths as a string name */
{
char buff[32];
struct altGraph *ag = item;
int numSplicings = altGraphNumAltSplices(ag);
snprintf(buff, sizeof(buff), "%d", numSplicings );
return (cloneString(buff));
}

void altGraphMethods(struct trackGroup *tg)
/* setup special methods for altGraph track */
{
tg->drawItems = altGraphDraw;
tg->loadItems = altGraphLoadItems;
tg->freeItems = altGraphFreeItems;
tg->totalHeight = altGraphFixedTotalHeight;
tg->itemName = altGraphItemName;
tg->mapsSelf = TRUE;
tg->mapItem = altGraphMapItem;
}

void altGraphXMapItem(struct trackGroup *tg, void *item, char *itemName, int start, int end, 
		    int x, int y, int width, int height)
/* create a link for each altGraphX that centers it on browser with 
   known genes, human mrnas, and intron est tracks open */
{
struct altGraphX *ag = item;
char buff[32];
snprintf(buff, sizeof(buff), "%d", ag->id);
mapBoxHc(start, end, x, y, width, height, tg->mapName, buff, "altGraphX Details");
}

boolean altGraphXEdgeSeen(struct altGraphX *ag, int *seen, int *seenCount, int mrnaIx)
/* is the mrnaIx already in seen? */
{
int i=0;
boolean result = FALSE;
for(i=0; i<*seenCount; i++)
    {
    if(ag->mrnaTissues[seen[i]] == ag->mrnaTissues[mrnaIx] ||
       ag->mrnaLibs[seen[i]] == ag->mrnaLibs[mrnaIx])
	{
	result = TRUE;
	break;
	}
    }
if(!result)
    {
    seen[*seenCount++] = mrnaIx;
    }
return result;
}

int altGraphConfidenceForEdge(struct altGraphX *ag, int eIx)
/* count how many unique libraries or tissues contain a given edge */
{
struct evidence *ev = slElementFromIx(ag->evidence, eIx);
int *seen = NULL;
int seenCount = 0,i;
int conf = 0;
AllocArray(seen, ag->edgeCount);
for(i=0; i<ag->edgeCount; i++)
    seen[i] = -1;
for(i=0; i<ev->evCount; i++)
    if(!altGraphXEdgeSeen(ag, seen, &seenCount, ev->mrnaIds[i]))
	conf++;
freez(&seen);
return conf;
}

boolean altGraphXInEdges(struct ggEdge *edges, int v1, int v2)
/* Return TRUE if a v1-v2 edge is in the list FALSE otherwise. */
{
struct ggEdge *e = NULL;
for(e = edges; e != NULL; e = e->next)
    {
    if(e->vertex1 == v1 && e->vertex2 == v2)
	return TRUE;
    }
return FALSE;
}

Color altGraphXColorForEdge(struct vGfx *vg, struct altGraphX *ag, int eIx)
/* Return the color of an edge given by confidence */
{
int confidence = altGraphConfidenceForEdge(ag, eIx);
Color c = shadesOfGray[maxShade/4];
struct geneGraph *gg = NULL;
struct ggEdge *edges = NULL;

if(ag->edgeTypes[eIx] == ggCassette)
    {
    if(!exprBedColorsMade)
	makeRedGreenShades(vg);
    if(confidence == 1) c = shadesOfRed[(maxRGBShade - 6 > 0) ? maxRGBShade - 6 : 0];
    else if(confidence == 2) c = shadesOfRed[(maxRGBShade - 4 > 0) ? maxRGBShade - 4: 0];
    else if(confidence >= 3) c = shadesOfRed[(maxRGBShade - 4 > 0) ? maxRGBShade - 1: 0];
    }
else
    {
    if(confidence == 1) c = shadesOfGray[maxShade/3];
    else if(confidence == 2) c = shadesOfGray[2*maxShade/3];
    else if(confidence >= 3) c = shadesOfGray[maxShade];
    }
return c;
}

static void altGraphXDraw(struct trackGroup *tg, int seqStart, int seqEnd,         
			 struct vGfx *vg, int xOff, int yOff, int width, 
			 MgFont *font, Color color, enum trackVisibility vis)
/* Draws the blocks for an alt-spliced gene and the connections */
{
int baseWidth = seqEnd - seqStart;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2;
boolean isFull = (vis == tvFull);
double scale = width/(double)baseWidth;
int i;
double y1, y2;
int midLineOff = heightPer/2;
struct altGraphX *ag=NULL, *agList = NULL;
int s =0, e=0;
agList = tg->items;
for(ag = agList; ag != NULL; ag = ag->next)
    {	   
    x1 = round((double)((int)ag->tStart-winStart)*scale) + xOff;
    x2 = round((double)((int)ag->tEnd-winStart)*scale) + xOff;
    if(tg->mapsSelf && tg->mapItem)
	{
	tg->mapItem(tg, ag, "notUsed", ag->tStart, ag->tEnd, xOff, y, width, heightPer);
	}
    if(!isFull && (x2-x1 > 0))
	{
	vgBox(vg, x1, yOff+tg->heightPer/2, x2-x1, 1, MG_BLACK);
	}
    for(i= ag->edgeCount -1; i >= 0; i--)   // for(i=0; i< ag->edgeCount; i++)
	{
	char buff[16];
	int textWidth;
	int sx1 = 0;
	int sx2 = 0;
	int sw = 0;
	s = ag->vPositions[ag->edgeStarts[i]];
	e = ag->vPositions[ag->edgeEnds[i]];
	sx1 = roundingScale(s-winStart, width, baseWidth)+xOff;
	sx2 = roundingScale(e-winStart, width, baseWidth)+xOff;
	sw = sx2 - sx1;
	snprintf(buff, sizeof(buff), "%d-%d", ag->edgeStarts[i], ag->edgeEnds[i]);        /* draw exons as boxes */
	if( (ag->vTypes[ag->edgeStarts[i]] == ggHardStart || ag->vTypes[ag->edgeStarts[i]] == ggSoftStart)  
	    && (ag->vTypes[ag->edgeEnds[i]] == ggHardEnd || ag->vTypes[ag->edgeEnds[i]] == ggSoftEnd)) 
	    {
	    Color color2 = altGraphXColorForEdge(vg, ag, i);
	    if(isFull)
		{
		drawScaledBox(vg, s, e, scale, xOff, y+(heightPer/2), heightPer/2, color2);
		textWidth = mgFontStringWidth(font, buff);
		if (textWidth <= sw + 2 && hgDebug )
		    vgTextCentered(vg, sx2-textWidth-2, y+(heightPer/2), textWidth+2, heightPer/2, MG_WHITE, font, buff);
		}
	    else
		drawScaledBox(vg, s, e, scale, xOff, y, heightPer, color2);

	    }
	if(isFull)
	    {
	    /* draw introns as arcs */

	    if( (ag->vTypes[ag->edgeStarts[i]] == ggHardEnd || ag->vTypes[ag->edgeStarts[i]] == ggSoftEnd) 
		&& (ag->vTypes[ag->edgeEnds[i]] == ggHardStart || ag->vTypes[ag->edgeEnds[i]] == ggSoftStart))
		{
		Color color2 = altGraphXColorForEdge(vg, ag, i);
		int x1, x2;
		int midX;   
		int midY = y + heightPer/2;
		s = ag->vPositions[ag->edgeStarts[i]];
		e = ag->vPositions[ag->edgeEnds[i]];
		x1 = round((double)((int) s - winStart)*scale) + xOff;
		x2 = round((double)((int) e - winStart)*scale) + xOff;
		midX = (x1+x2)/2;
		vgLine(vg, x1, midY, midX, y, color2);
		vgLine(vg, midX, y, x2, midY, color2);
		textWidth = mgFontStringWidth(font, buff);
		if (textWidth <= sw && hgDebug )
		    vgTextCentered(vg, sx1, y+(heightPer/2), sw, heightPer/2, MG_BLACK, font, buff);
		}
	    }
	}
    if(isFull)
	y += lineHeight;
    }
}


void altGraphXLoadItems(struct trackGroup *tg)
/* load the altGraphX data to a trackGroup */
{
struct sqlConnection *conn = hAllocConn();
int rowOffSet;
char **row;
struct altGraphX *ag=NULL, *agList=NULL;
struct sqlResult *sr = hRangeQuery(conn, tg->mapName, chromName,
				   winStart, winEnd, NULL, &rowOffSet);
while((row = sqlNextRow(sr)) != NULL)
    {
    ag = altGraphXLoad(row + rowOffSet);
    slAddHead(&agList, ag);
    }
slReverse(&agList);
sqlFreeResult(&sr);
tg->items = agList;
}

void altGraphXFreeItems(struct trackGroup *tg)
/* free up tha altGraphX items in tg->items */
{
altGraphXFreeList((struct altGraphX**)(&tg->items));
}

static int altGraphXFixedTotalHeight(struct trackGroup *tg, enum trackVisibility vis)
/* set track height to 2 * font size if full , 1 * if dense
*/
{
switch (vis)
    {
    case tvFull:
	tg->lineHeight  = 2 * mgFontLineHeight(tl.font)+1;
	tg->heightPer = tg->lineHeight -1;
	tg->height = slCount(tg->items) * tg->lineHeight;
	break;
    case tvDense:
	tg->lineHeight  = mgFontLineHeight(tl.font)+1;
	tg->heightPer = tg->lineHeight -1;
	tg->height = tg->lineHeight;
	break;
    }
return tg->height;
}

char *altGraphXItemName(struct trackGroup *tg, void *item)
/* returns the number of alternative splice paths as a string name */
{
char buff[32];
struct altGraphX *ag = item;
int count =0, i=0;
for(i=0; i<ag->edgeCount; i++)
    if(ag->edgeTypes[i] == ggCassette)
	count++;
snprintf(buff, sizeof(buff), "%d", count );
return (cloneString(buff));
}

void altGraphXMethods(struct trackGroup *tg)
/* setup special methods for altGraphX track */
{
tg->drawItems = altGraphXDraw;
tg->loadItems = altGraphXLoadItems;
tg->freeItems = altGraphXFreeItems;
tg->totalHeight = altGraphXFixedTotalHeight;
tg->itemName = altGraphXItemName;
tg->mapsSelf = TRUE;
tg->mapItem = altGraphXMapItem;
}

int lfNamePositionCmp(const void *va, const void *vb)
/* Compare based on name, then chromStart, used for
   sorting sample based tracks. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
int dif;
char *tgName = NULL;
if(sortGroupList != NULL)
    tgName = sortGroupList->shortLabel;
if(tgName != NULL)
    {
    if(sameString(a->name, tgName) && differentString(b->name, tgName))
	return -1;
    if(sameString(b->name, tgName) && differentString(a->name, tgName))
	return 1;
    }
dif = strcmp(a->name, b->name);
if (dif == 0)
    dif = a->start - b->start;
return dif;
}

void zooSpeciesHashInit()
/* Function to init list of zoo species */
{
char *name = NULL;
char *val = NULL;

zooSpeciesHash = hashNew(6);

name = cloneString("Human");
val = cloneString("1");
hashAdd(zooSpeciesHash, name, val);
name = cloneString("Chimpanzee");
val = cloneString("2");
hashAdd(zooSpeciesHash, name, val);
name = cloneString("Baboon");
val = cloneString("3");
hashAdd(zooSpeciesHash, name, val);
name = cloneString("Cat");
val = cloneString("4");
hashAdd(zooSpeciesHash, name, val);
name = cloneString("Dog");
val = cloneString("5");
hashAdd(zooSpeciesHash, name, val);
name = cloneString("Cow");
val = cloneString("6");
hashAdd(zooSpeciesHash, name, val);
name = cloneString("Pig");
val = cloneString("7");
hashAdd(zooSpeciesHash, name, val);
name = cloneString("Rat");
val = cloneString("8");
hashAdd(zooSpeciesHash, name, val);
name = cloneString("Mouse");
val = cloneString("9");
hashAdd(zooSpeciesHash, name, val);
name = cloneString("Chicken");
val = cloneString("10");
hashAdd(zooSpeciesHash, name, val);
name = cloneString("Fugu");
val = cloneString("11");
hashAdd(zooSpeciesHash, name, val);
name = cloneString("Tetraodon");
val = cloneString("12");
hashAdd(zooSpeciesHash, name, val);
name = cloneString("Zebrafish");
val = cloneString("13");
hashAdd(zooSpeciesHash, name, val);
}

int lfZooCmp(const void *va, const void *vb)
/* Compare based on name, then chromStart, used for
   sorting sample based tracks. */
{
struct linkedFeatures *a = *((struct linkedFeatures **)va);
struct linkedFeatures *b = *((struct linkedFeatures **)vb);
char *aVal =  hashFindVal(zooSpeciesHash, a->name);
char *bVal =  hashFindVal(zooSpeciesHash, b->name);
int aV = atoi(aVal);
int bV = atoi(bVal);
return aV - bV;
}

void loadHumMusL(struct trackGroup *tg)
/* Load humMusL track with 2 zoom levels and one normal level. 
 * Also used for loading the musHumL track (called Human Cons) 
 * on the mm2 mouse browser. It decides which of 4 tables to
 * load based on how large of a window the user is looking at*/
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct sample *sample;
struct linkedFeatures *lfList = NULL, *lf;
char *hasDense = NULL;
char *where = NULL;
char query[256];
unsigned int chromSize = hChromSize(chromName);
int resolution = 0;
char tableName[256];
int z;
float pixPerBase = 0;

if(tl.picWidth == 0)
    errAbort("hgTracks.c::loadHumMusL() - can't have pixel
	    width of 0");
pixPerBase = (winEnd - winStart)/ tl.picWidth;


/* Determine zoom level. */
z = humMusZoomLevel();
if(z == 1 )
    snprintf(tableName, sizeof(tableName), "%s_%s", "zoom1",
	    tg->mapName);
else if( z == 2)
    snprintf(tableName, sizeof(tableName), "%s_%s", "zoom50",
	    tg->mapName);
else if(z == 3)
    snprintf(tableName, sizeof(tableName), "%s_%s",
	    "zoom2500", tg->mapName);
else
    snprintf(tableName, sizeof(tableName), "%s", tg->mapName);

sr = hRangeQuery(conn, tableName, chromName, winStart, winEnd,
    where, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    sample = sampleLoad(row+rowOffset);
    lf = lfFromSample(sample);
    slAddHead(&lfList, lf);
    sampleFree(&sample);
    }
if(where != NULL)
    freez(&where);
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfList);

/* sort to bring items with common names to the same line
   but only for tracks with a summary table (with name=shortLabel)
   in
   dense mode*/
if( hasDense != NULL )
    {
    sortGroupList = tg; /* used to put track name at
	                 * top of sorted list. */
    slSort(&lfList, lfNamePositionCmp);
    sortGroupList = NULL;
    }
tg->items = lfList;

/* Since we've taken care of loading too many things using the
      zoom tables, take care of limiting visibility here. */
tg->limitedVis = tg->visibility;
tg->limitedVisSet = TRUE;
}

void zooMethods( struct trackGroup *tg )
/* Overide the zoo sample type load function to look for zoomed out tracks. */
{
tg->loadItems = loadSampleZoo;
}

void humMusLMethods( struct trackGroup *tg )
/* Overide the humMusL load function to look for zoomed out tracks. */
{
tg->loadItems = loadHumMusL;
}

void musHumLMethods( struct trackGroup *tg )
/* Overide the musHumL load function to look for zoomed out tracks. */
{
tg->loadItems = loadHumMusL;
}


void loadAffyTranscriptome(struct trackGroup *tg)
/* Convert sample info in window to linked feature. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct sample *sample;
struct linkedFeatures *lfList = NULL, *lf;
char *hasDense = NULL;
char *where = NULL;
char query[256];
unsigned int chromSize = hChromSize(chromName);
int resolution = 0;
char tableName[256];
int zoom1 = 23924, zoom2 = 2991; /* bp per data point */
float pixPerBase = 0;
/* Set it up so we don't have linear interpretation.. */
char *noLinearInterpString = wiggleEnumToString(wiggleNoInterpolation);
cartSetString(cart, "affyTranscriptome.linear.interp", noLinearInterpString);

if(tl.picWidth == 0)
    errAbort("hgTracks.c::loadAffyTranscriptome() - can't have pixel width of 0");
pixPerBase = (winEnd - winStart)/ tl.picWidth;


/* Determine zoom level. */
if(pixPerBase >= zoom1)
    snprintf(tableName, sizeof(tableName), "%s_%s", "zoom1", tg->mapName);
else if(pixPerBase >= zoom2)
    snprintf(tableName, sizeof(tableName), "%s_%s", "zoom2", tg->mapName);
else 
    snprintf(tableName, sizeof(tableName), "%s", tg->mapName);

/*see if we have a summary table*/
if(hTableExists(tableName))
    snprintf(query, sizeof(query), "select name from %s where name = '%s' limit 1",  tableName, tg->shortLabel);
else
    {
    warn("<p>Couldn't find table %s<br><br>", tableName);
    snprintf(query, sizeof(query), "select name from %s where name = '%s' limit 1",  tg->mapName, tg->shortLabel);
    snprintf(tableName, sizeof(tableName), "%s", tg->mapName);
    }

hasDense = sqlQuickQuery(conn, query, query, sizeof(query));

/* If we're in dense mode and have a summary table load it. */
if(tg->visibility == tvDense)
    {
    if(hasDense != NULL)
	{
	snprintf(query, sizeof(query), " name = '%s' ", tg->shortLabel);
	where = cloneString(query);
	}
    }

sr = hRangeQuery(conn, tableName, chromName, winStart, winEnd, where, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    sample = sampleLoad(row+rowOffset);
    lf = lfFromSample(sample);
    slAddHead(&lfList, lf);
    sampleFree(&sample);
    }
if(where != NULL)
    freez(&where);
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfList);

/* sort to bring items with common names to the same line
but only for tracks with a summary table (with name=shortLabel) in
dense mode*/
if( hasDense != NULL )
    {
    sortGroupList = tg; /* used to put track name at top of sorted list. */
    slSort(&lfList, lfNamePositionCmp);
    sortGroupList = NULL;
    }
tg->items = lfList;

/* Since we've taken care of loading too many things using the 
   zoom tables, take care of limiting visibility here. */
tg->limitedVis = tg->visibility;
tg->limitedVisSet = TRUE;
}


void affyTranscriptomeMethods(struct trackGroup *tg)
/* Overide the load function to look for zoomed out tracks. */
{

tg->loadItems = loadAffyTranscriptome;
}

void mapBoxHcTwoItems(int start, int end, int x, int y, int width, int height, 
	char *group, char *item1, char *item2, char *statusLine)
/* Print out image map rectangle that would invoke the htc (human track click)
 * program. */
{
char *encodedItem1 = cgiEncode(item1);
char *encodedItem2 = cgiEncode(item2);
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
hPrintf("HREF=\"%s&o=%d&t=%d&g=%s&i=%s&i2=%s&c=%s&l=%d&r=%d&db=%s&pix=%d\" ", 
       hgcNameAndSettings(), start, end, group, encodedItem1, encodedItem2,chromName, winStart, winEnd, 
       database, tl.picWidth);
hPrintf("ALT=\"%s\">\n", statusLine); 
freeMem(encodedItem1);
freeMem(encodedItem2);
}


void lfsMapItemName(struct trackGroup *tg, void *item, char *itemName, int start, int end, 
		    int x, int y, int width, int height)
{
struct linkedFeaturesSeries *lfs = tg->items;
struct linkedFeaturesSeries *lfsItem = item;
if(tg->visibility == tvFull)
    mapBoxHcTwoItems(start, end, x,y, width, height, tg->mapName, lfsItem->name, itemName, itemName);
}


struct linkedFeaturesSeries *lfsFromMsBedSimple(struct bed *bedList, char * name)
/* create a lfs containing all beds on a single line */
{
struct linkedFeaturesSeries *lfs;
struct linkedFeatures *lf;
struct bed *bed = NULL;

if(bedList == NULL)
    return NULL;
/* create one linkedFeatureSeries with average score for each lf
   in bed->score */
AllocVar(lfs);
if(name != NULL)
    lfs->name = cloneString(name);
else
    lfs->name = cloneString("unknown");
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    lf = lfFromBed(bed);
    /* lf->tallStart = bed->chromStart;
       lf->tallEnd = bed->chromEnd; */
    lf->score = bed->score;
    slAddHead(&lfs->features, lf);  
    }
return lfs;
}


struct slInt {
    /* list of ints, should probably switch to slRef but harder to debug */
    struct slInt *next;
    int val;
};

void expRecordMapTypes(struct hash *expIndexesToNames, struct hash *indexes, int *numIndexes, 
		       struct expRecord *erList,  int index, char *filter, int filterIndex)
/* creates two hashes which contain a mapping from 
   experiment to type and from type to lists of experiments */
{
struct expRecord *er = NULL;
struct slRef *val=NULL;
struct slInt *sr=NULL, *srList = NULL;
struct hash *seen = newHash(2);
char buff[256];
int unique = 0;
for(er = erList; er != NULL; er = er->next)
    {
    if ((filterIndex == -1) || (sameString(filter, er->extras[filterIndex])))
        {
	val = hashFindVal(seen, er->extras[index]);
	if (val == NULL)
	    {
	    /* if this type is new 
	       save the index for this type */
	    AllocVar(val);
	    snprintf(buff, sizeof(buff), "%d", unique);
	    hashAdd(expIndexesToNames, buff, er->extras[index]);
	    val->val = cloneString(buff);
	    hashAdd(seen, er->extras[index], val);

	    /* save the indexes associated with this index */
	    AllocVar(sr);
	    sr->val = er->id;
	    hashAdd(indexes, buff, sr);
	    unique++;
	    }
	else
	    {
	    /* if this type has been seen before 
	       tack the new index on the end of the list */
	    AllocVar(sr);
	    srList = hashMustFindVal(indexes, val->val);
	    sr->val = er->id;
	    slAddTail(&srList,sr);
	    }
	}
    }

hashTraverseVals(seen, freeMem);
hashFree(&seen);
*numIndexes = unique;
}

int lfsSortByName(const void *va, const void *vb)    
/* used for slSorting linkedFeaturesSeries */
{
const struct linkedFeaturesSeries *a = *((struct linkedFeaturesSeries **)va);
const struct linkedFeaturesSeries *b = *((struct linkedFeaturesSeries **)vb);
return(strcmp(a->name, b->name));
}

int nci60LfsSortByName(const void *va, const void *vb)    
/* used for slSorting linkedFeaturesSeries */
{
const struct linkedFeaturesSeries *a = *((struct linkedFeaturesSeries **)va);
const struct linkedFeaturesSeries *b = *((struct linkedFeaturesSeries **)vb);
/* make sure that the duplicate and nsclc end up at the end */
if(sameString(a->name, "DUPLICATE"))
    return 1;
if(sameString(a->name, "NSCLC"))
    return 1;
if(sameString(b->name, "DUPLICATE"))
    return -1;
if(sameString(b->name, "NSCLC"))
    return -1;
return(strcmp(a->name, b->name));
}


struct linkedFeatures *bedFilterMinLength(struct bed *bedList, int minLength ) 
{
struct linkedFeatures *lf = NULL, *lfList=NULL;
struct bed *bed = NULL;

/* traditionally if there is nothing to show
   show nothing .... */
if(bedList == NULL)
    return NULL;

for(bed = bedList; bed != NULL; bed = bed->next)
    {
	/* create the linked features */
    if( bed->chromEnd - bed->chromStart >=  minLength )
        {
        lf = lfFromBed(bed);
        slAddHead(&lfList, lf);
        }
    }

slReverse(&lfList);
return lfList;
}



struct linkedFeaturesSeries *msBedGroupByIndex(struct bed *bedList, char *database, char *table, int expIndex, 
					       char *filter, int filterIndex) 
/* groups bed expScores in multiple scores bed by the expIndex 
   in the expRecord->extras array. Makes use of hashes to remember numerical
   index of experiments, as hard to do in a list. */
{
struct linkedFeaturesSeries *lfsList = NULL, *lfs, **lfsArray;
struct linkedFeatures *lf = NULL;
struct sqlConnection *conn;
struct hash *indexes;
struct hash *expTypes;
struct hash *expIndexesToNames;
int numIndexes = 0, currentIndex, i;
struct expRecord *erList = NULL, *er=NULL;
struct slInt *srList = NULL, *sr=NULL;
char buff[256];
struct bed *bed;

/* traditionally if there is nothing to show
   show nothing .... */
if(bedList == NULL)
    return NULL;

/* otherwise if we're goint to do some filtering
   set up the data structures */
conn = sqlConnect(database);
indexes = newHash(2);
expTypes = newHash(2);
expIndexesToNames = newHash(2);

/* load the experiment information */
snprintf(buff, sizeof(buff), "select * from %s order by id asc", table);
erList = expRecordLoadByQuery(conn, buff);
if(erList == NULL)
    errAbort("hgTracks::msBedGroupByIndex() - can't get any records for %s in table %s\n", buff, table);
sqlDisconnect(&conn);

/* build hash to map experiment ids to types */
for(er = erList; er != NULL; er = er->next)
    {
    snprintf(buff, sizeof(buff), "%d", er->id);
    hashAdd(expTypes, buff, er->extras[expIndex]);
    }
/* get the number of indexes and the experiment values associated
   with each index */
expRecordMapTypes(expIndexesToNames, indexes, &numIndexes, erList, expIndex, filter, filterIndex);
if(numIndexes == 0)
    errAbort("hgTracks::msBedGroupByIndex() - numIndexes can't be 0");
lfsArray = needMem(sizeof(struct linkedFeaturesSeries*) * numIndexes);

/* initialize our different tissue linkedFeatureSeries) */
for(i=0;i<numIndexes;i++)
    {    
    char *name=NULL;
    AllocVar(lfsArray[i]);
    snprintf(buff, sizeof(buff), "%d", i);
    name = hashMustFindVal(expIndexesToNames, buff);	
    lfsArray[i]->name = cloneString(name);
    }
/* for every bed we need to group together the tissue specific
 scores in that bed */
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    /* for each tissue we need to average the scores together */
    for(i=0; i<numIndexes; i++) 
	{
	float aveScores = 0;
	int aveCount =0;

	/* get the indexes of experiments that we want to average 
	 in form of a slRef list */
	snprintf(buff, sizeof(buff), "%d", i);
	srList = hashMustFindVal(indexes, buff);
	currentIndex = srList->val;

	/* create the linked features */
	lf = lfFromBed(bed);

	/* average the scores together to get the ave score for this
	   tissue type */
	for(sr = srList; sr != NULL; sr = sr->next)
	    {
	    currentIndex = sr->val;
	    if( bed->expScores[currentIndex] != -10000) 
		{
		aveScores += bed->expScores[currentIndex];
		aveCount++;
		}
	    }

	/* if there were some good values do the average 
	   otherwise mark as missing */
	if(aveCount != 0)
	    lf->score = aveScores/aveCount;
	else
	    lf->score = -10000;
	
	/* add this linked feature to the correct 
	   linkedFeaturesSeries */
	slAddHead(&lfsArray[i]->features, lf);
	}
    }
/* Summarize all of our linkedFeatureSeries in one linkedFeatureSeries list */
for(i=0; i<numIndexes; i++)
    {
    slAddHead(&lfsList, lfsArray[i]);
    }

hashTraverseVals(indexes, freeMem);
expRecordFreeList(&erList);
freeHash(&indexes);
freeHash(&expTypes);
freeHash(&expIndexesToNames);
slReverse(&lfsList);
return lfsList;
}


void lfsFromAffyBed(struct trackGroup *tg)
/* filters the bedList stored at tg->items
into a linkedFeaturesSeries as determined by
filter type */
{
struct linkedFeaturesSeries *lfsList = NULL, *lfs;
struct linkedFeatures *lf;
struct bed *bed = NULL, *bedList= NULL;
char *affyMap = cartUsualString(cart, "affy.type", affyEnumToString(affyTissue));
enum affyOptEnum affyType = affyStringToEnum(affyMap);
int i=0;
bedList = tg->items;

if(tg->limitedVis == tvDense)
    {
    tg->items = lfsFromMsBedSimple(bedList, "Affymetrix");
    }
else if(affyType == affyTissue)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "affyExps", affyTissue, NULL, -1);
    slSort(&tg->items,lfsSortByName);
    }
else if(affyType == affyId)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "affyExps", affyId, NULL, -1);
    }
else if(affyType == affyChipType)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "affyExps", affyChipType, NULL, -1);
    }
else
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "affyExps", affyTissue, affyMap, 1);
    slSort(&tg->items,lfsSortByName);
    }
bedFreeList(&bedList);
}

void lfsFromNci60Bed(struct trackGroup *tg)
/* filters the bedList stored at tg->items
into a linkedFeaturesSeries as determined by
filter type */
{
struct linkedFeaturesSeries *lfsList = NULL, *lfs;
struct linkedFeatures *lf;
struct bed *bed = NULL, *bedList= NULL;
char *nci60Map = cartUsualString(cart, "nci60.type", nci60EnumToString(0));
enum nci60OptEnum nci60Type = nci60StringToEnum(nci60Map);
int i=0;
bedList = tg->items;

if(tg->limitedVis == tvDense)
    {
    tg->items = lfsFromMsBedSimple(bedList, "NCI 60");
    }
else if(nci60Type == nci60Tissue)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "nci60Exps", 1, NULL, -1);
    slSort(&tg->items,nci60LfsSortByName);
    }
else if(nci60Type == nci60All)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "nci60Exps", 0, NULL, -1);
    }
else
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "nci60Exps", 0, nci60Map, 1);
    slSort(&tg->items,lfsSortByName);
    }
bedFreeList(&bedList);
}

struct bed *rosettaFilterByExonType(struct bed *bedList)
/* remove beds from list depending on user preference for 
   seeing confirmed and/or predicted exons */
{
struct bed *bed=NULL, *tmp=NULL, *tmpList=NULL;
char *exonTypes = cartUsualString(cart, "rosetta.et", rosettaExonEnumToString(0));
enum rosettaExonOptEnum et = rosettaStringToExonEnum(exonTypes);

if(et == rosettaAllEx)
    return bedList;

/* go through and remove appropriate beds */
for(bed = bedList; bed != NULL; )
    {
    if(et == rosettaConfEx)
	{
	tmp = bed->next;
	if(bed->name[strlen(bed->name) -2] == 't')
	    slSafeAddHead(&tmpList, bed);
	else
	    bedFree(&bed);
	bed = tmp;
	}
    else if(et == rosettaPredEx)
	{
	tmp = bed->next;
	if(bed->name[strlen(bed->name) -2] == 'p')
	    slSafeAddHead(&tmpList, bed);
	else
	    bedFree(&bed);
	bed = tmp;
	}
    }
slReverse(&tmpList);
return tmpList;
}

void lfsFromRosettaBed(struct trackGroup *tg)
/* filters the bedList stored at tg->items
into a linkedFeaturesSeries as determined by
filter type */
{
struct linkedFeaturesSeries *lfsList = NULL, *lfs;
struct linkedFeatures *lf;
struct bed *bed = NULL, *bedList= NULL, *tmp=NULL, *tmpList=NULL;
char *rosettaMap = cartUsualString(cart, "rosetta.type", rosettaEnumToString(0));
enum rosettaOptEnum rosettaType = rosettaStringToEnum(rosettaMap);
char *exonTypes = cartUsualString(cart, "rosetta.et", "Confirmed Only");
int i=0, et=-1;
bedList = tg->items;

bedList = rosettaFilterByExonType(bedList);

/* determine how to display the experiments */
if(tg->limitedVis == tvDense)
    {
    tg->items = lfsFromMsBedSimple(bedList, "Rosetta");
    }
else if(rosettaType == rosettaAll)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "rosettaExps", 0, NULL, -1);
    }
else if(rosettaType == rosettaPoolOther)
    {
    lfsList = msBedGroupByIndex(bedList, "hgFixed", "rosettaExps", 1, NULL, -1);
    lfsList->name=cloneString("Common Reference");
    lfsList->next->name=cloneString("Other Exps");
    tg->items = lfsList;
    }
else 
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "rosettaExps", 0, rosettaMap, 1);
    }    
bedFreeList(&bedList);
}



void lfFromAncientRBed(struct trackGroup *tg)
/* filters the bedList stored at tg->items
into a linkedFeaturesSeries as determined by
minimum munber of aligned bases cutoff */
{
struct bed *bed = NULL, *bedList= NULL, *tmp=NULL, *tmpList=NULL;
int ancientRMinLength = atoi(cartUsualString(cart, "ancientR.minLength", "50"));
bedList = tg->items;
tg->items = bedFilterMinLength(bedList, ancientRMinLength);
bedFreeList(&bedList);
}



void lfsFromCghNci60Bed(struct trackGroup *tg)
{
struct linkedFeaturesSeries *lfsList = NULL, *lfs;
struct linkedFeatures *lf;
struct bed *bed = NULL, *bedList= NULL;
char *cghNci60Map = cartUsualString(cart, "cghNci60.type", cghoeEnumToString(0));
enum cghNci60OptEnum cghNci60Type = cghoeStringToEnum(cghNci60Map);
int i=0;
bedList = tg->items;


if(tg->limitedVis == tvDense)
    {
    tg->items = lfsFromMsBedSimple(bedList, "CGH NCI 60");
    }
else if (cghNci60Type == cghoeTissue)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "cghNci60Exps", 1, NULL, -1);
    }
else if (cghNci60Type == cghoeAll)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "cghNci60Exps", 0, NULL, -1);
    }
else
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "cghNci60Exps", 0, cghNci60Map, 1);
    }
bedFreeList(&bedList);
}

struct linkedFeaturesSeries *lfsFromMsBed(struct trackGroup *tg, struct bed *bedList)
/* create a linkedFeatureSeries from a bed list making each
   experiment a different linkedFeaturesSeries */
{
struct linkedFeaturesSeries *lfsList = NULL, *lfs;
struct linkedFeatures *lf;
struct bed *bed = NULL;
int i=0;
if(tg->limitedVis == tvDense)
    {
    lfsList = lfsFromMsBedSimple(bedList, tg->shortLabel);
    }
else 
    {
    /* for each experiment create a linked features series */
    for(i = 0; i < bedList->expCount; i++) 
	{
	char buff[256];
	AllocVar(lfs);
	if(bedList != NULL)
	    {
	    snprintf(buff, sizeof(buff), "%d", bedList->expIds[i]);
	    lfs->name = cloneString(buff);
	    }
	else
	    lfs->name = cloneString(tg->shortLabel);
      	for(bed = bedList; bed != NULL; bed = bed->next)
	    {
	    lf = lfFromBed(bed);
	    lf->tallStart = bed->chromStart; 
	    lf->tallEnd = bed->chromEnd; 
	    lf->score = bed->expScores[i];
	    slAddHead(&lfs->features, lf);
	    }
	slReverse(&lfs->features);
	slAddHead(&lfsList, lfs);
	}
    slReverse(&lfsList);
    }
return lfsList;
}


Color cghNci60Color(struct trackGroup *tg, void *item, struct vGfx *vg ) 
{
struct linkedFeatures *lf = item;
float val = lf->score;
float absVal = fabs(val);
int colorIndex = 0;
float maxDeviation = 1.0;
char *colorScheme = cartUsualString(cart, "cghNci60.color", "gr");
/* colorScheme should be stored somewhere not looked up every time... */

/* Make sure colors available */
if(!exprBedColorsMade)
    makeRedGreenShades(vg);
if(val == -10000)
    return shadesOfGray[5];

if(tg->visibility == tvDense) 
    /* True value stored as integer in score field and was multiplied by 100 */ 
    absVal = absVal/100;

/* Check on mode */
if (tg->visibility == tvFull)
    {
    maxDeviation = 0.7;
    } 
 else 
    {
    maxDeviation = 0.5;
    }

/* cap the value to be less than or equal to maxDeviation */
if(absVal > maxDeviation)
    absVal = maxDeviation;

/* project the value into the number of colors we have.  
 *   * i.e. if val = 1.0 and max is 2.0 and number of shades is 16 then index would be
 * 1 * 15 /2.0 = 7.5 = 7
 */
colorIndex = (int)(absVal * maxRGBShade/maxDeviation);
if(val < 0) 
    if (sameString(colorScheme, "gr")) 
        return shadesOfRed[colorIndex];
    else
        return shadesOfGreen[colorIndex];    
else 
    {
    if (sameString(colorScheme, "gr"))
	return shadesOfGreen[colorIndex];
    else if (sameString(colorScheme, "rg"))
        return shadesOfRed[colorIndex];
    else
	return shadesOfBlue[colorIndex];
    }
}



Color expressionColor(struct trackGroup *tg, void *item, struct vGfx *vg,
		 float denseMax, float fullMax) 
/* Does the score->color conversion for various microarray tracks */
{
struct linkedFeatures *lf = item;
float val = lf->score;
float absVal = fabs(val);
int colorIndex = 0;
float maxDeviation = 1.0;
static char *colorSchemes[] = { "rg", "rb" };
static char *colorScheme = NULL;
static int colorSchemeFlag = -1;

/* set up the color scheme items if not done yet */
if(colorScheme == NULL)
    colorScheme = cartUsualString(cart, "exprssn.color", "rg");
if(colorSchemeFlag == -1)
    colorSchemeFlag = stringArrayIx(colorScheme, colorSchemes, ArraySize(colorSchemes));

/* if val is error value show make it gray */
if(val <= -10000)
    return shadesOfGray[5];

/* we approximate a float by storing it as an int,
   thus to bring us back to right scale divide by 1000.
   i.e. 1.27 was stored as 1270 and needs to be converted to 1.27 */
if(tg->limitedVis == tvDense)
    absVal = absVal/1000;

if(!exprBedColorsMade)
    makeRedGreenShades(vg);

/* cap the value to be less than or equal to maxDeviation */
if (tg->limitedVis == tvFull)
    maxDeviation = fullMax;
else 
    maxDeviation = denseMax;

/* cap the value to be less than or equal to maxDeviation */
if(absVal > maxDeviation)
    absVal = maxDeviation;

/* project the value into the number of colors we have.  
 * i.e. if val = 1.0 and max is 2.0 and number of shades is 16 then index would be
 * 1 * 15 /2.0 = 7.5 = 7
 */
colorIndex = (int)(absVal * maxRGBShade/maxDeviation);
if(val > 0) 
    return shadesOfRed[colorIndex];
else 
    {
    if(colorSchemeFlag == 0)
	return shadesOfGreen[colorIndex];
    else 
	return shadesOfBlue[colorIndex];
    }
}

Color nci60Color(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Does the score->color conversion for various microarray tracks */
{
return expressionColor(tg, item, vg, 1.0, 2.6);
}

Color getColorForAffyExpssn(float val, float max)
/* Return the correct color for a given score */
{
struct rgbColor color; 
int colorIndex = 0;
int offset = 0;   /* really there is no dynamic range below 64 (2^6) */

if(val == -10000)
    return shadesOfGray[5];
val = fabs(val);

/* take the log for visualization */
if(val > 0)
    val = logBase2(val);
else
    val = 0;

/* scale offset down to 0 */
if(val > offset) 
    val = val - offset;
else
    val = 0;

if (max <= 0) 
    errAbort("hgTracks::getColorForAffyExpssn() maxDeviation can't be zero\n"); 
max = logBase2(max);
max = max - offset;
if(max < 0)
    errAbort("hgTracks::getColorForAffyExpssn() - Max val should be greater than 0 but it is: %g", max);
    
if(val > max) 
    val = max;
colorIndex = (int)(val * maxShade/max);
return shadesOfSea[colorIndex];
}

Color affyColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Does the score->color conversion for affymetrix arrays */
{
struct linkedFeatures *lf = item;
float score = lf->score;
if(tg->visibility == tvDense)
    score = score/10;
if(!exprBedColorsMade)
    makeRedGreenShades(vg);
return getColorForAffyExpssn(score, 262144/16); /* 262144 == 2^18 */
}

Color affyRatioColor(struct trackGroup *tg, void *item, struct vGfx *vg)
/* Does the score->color conversion for affymetrix arrays using ratios,
 * if dense do an intensity color in blue based on score value otherwise do
 * red/green display from expScores */
{
struct linkedFeatures *lf = item;
float score = lf->score;
if(!exprBedColorsMade)
    makeRedGreenShades(vg);
if(tg->visibility == tvDense)
    {
    score = score/10;
    return getColorForAffyExpssn(score, 262144/16); /* 262144 == 2^18 */
    }
else
    {
    return expressionColor(tg, item, vg, 1.0, 3.0);
    }
}

void loadMultScoresBed(struct trackGroup *tg)
/* Convert bed info in window to linked feature. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
int itemCount =0;
struct bed *bedList = NULL, *bed;
struct linkedFeatures *lfList = NULL, *lf;
struct linkedFeaturesSeries *lfsList = NULL, *lfs;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, 15);
    slAddHead(&bedList, bed);
    itemCount++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&bedList);

/* a lot of the filters condense many items down to 
   two or three, this can be computationally expensive.
   use the maxItemsInFullTrack as a cap on the number that
   will be computed */
if(!tg->limitedVisSet)
    {
    enum trackVisibility vis = tg->visibility;
    tg->limitedVisSet = TRUE;
    if(vis == tvFull)
	{
	if(itemCount > maxItemsInFullTrack) 
	    vis = tvDense;
	}
    tg->limitedVis = vis;
    }

/* run the filter if it exists, otherwise use default */
if(tg->trackFilter != NULL)
    {
    /* let the filter do the assembly of the linkedFeaturesList */
    tg->items = bedList;
    tg->trackFilter(tg);
    }
else
    {
    /* use default behavior of one row for each experiment */
    tg->items = lfsFromMsBed(tg, bedList);
    bedFreeList(&bedList);
    }
}

char *rosettaName(struct trackGroup *tg, void *item)
/* Return Abbreviated rosetta experiment name */
{
struct linkedFeaturesSeries *lfs = item;
char *full = NULL;
static char abbrev[32];
char *tmp = strstr(lfs->name, "_vs_");
if(tmp != NULL) 
    {
    tmp += 4;
    full = tmp = cloneString(tmp);
    tmp = strstr(tmp, "_(");
    if(tmp != NULL)
	*tmp = '\0';
    strncpy(abbrev, full, sizeof(abbrev));
    freez(&full);
    }
else if(lfs->name != NULL) 
    {
    strncpy(abbrev, lfs->name, sizeof(abbrev));
    }
else 
    {
    strncpy(abbrev, tg->shortLabel, sizeof(abbrev));
    }
return abbrev;
}

void loadMaScoresBed(struct trackGroup *tg)
/* load up bed15 data types into linkedFeaturesSeries and then set the noLines
   flag on each one */
{
struct linkedFeaturesSeries *lfs;
loadMultScoresBed(tg);
for(lfs = tg->items; lfs != NULL; lfs = lfs->next)
    {
    lfs->noLine = TRUE;
    }
}


void rosettaMethods(struct trackGroup *tg)
/* methods for Rosetta track using bed track */
{
linkedFeaturesSeriesMethods(tg);
tg->itemColor = nci60Color;
tg->loadItems = loadMaScoresBed;
tg->trackFilter = lfsFromRosettaBed;
tg->itemName = rosettaName;
tg->mapItem = lfsMapItemName;
tg->mapsSelf = TRUE;
}

void nci60Methods(struct trackGroup *tg)
/* set up special methods for NCI60 track and tracks with multiple
   scores in general */
{
linkedFeaturesSeriesMethods(tg);
tg->itemColor = nci60Color;
tg->loadItems = loadMaScoresBed;
tg->trackFilter = lfsFromNci60Bed ;
tg->mapItem = lfsMapItemName;
tg->mapsSelf = TRUE;
}


void affyMethods(struct trackGroup *tg)
/* set up special methods for NCI60 track and tracks with multiple
   scores in general */
{
linkedFeaturesSeriesMethods(tg);
tg->itemColor = affyColor;
tg->loadItems = loadMaScoresBed;
tg->trackFilter = lfsFromAffyBed;
tg->mapItem = lfsMapItemName;
tg->mapsSelf = TRUE;
}

void affyRatioMethods(struct trackGroup *tg)
/* set up special methods for NCI60 track and tracks with multiple
   scores in general */
{
linkedFeaturesSeriesMethods(tg);
tg->itemColor = affyRatioColor;
tg->loadItems = loadMaScoresBed;
tg->trackFilter = lfsFromAffyBed;
tg->mapItem = lfsMapItemName;
tg->mapsSelf = TRUE;
}

void cghNci60Methods(struct trackGroup *tg)
/* set up special methods for CGH NCI60 track */
{
linkedFeaturesSeriesMethods(tg);
tg->itemColor = cghNci60Color;
tg->loadItems = loadMultScoresBed;
tg->trackFilter = lfsFromCghNci60Bed;
}

void perlegenMethods(struct trackGroup *tg)
/* setup special methods for haplotype track */
{
tg->drawItems = perlegenLinkedFeaturesDraw;
tg->itemName = perlegenName;
tg->colorShades = shadesOfSea;
}

void loadAncientR(struct trackGroup *tg)
/* Load up ancient repeats from database table to trackGroup items
 * filtering out those below a certain length threshold,
   in number of aligned bases. */
{
bedLoadItem(tg, "ancientR", (ItemLoader)bedLoad12);
lfFromAncientRBed(tg);
}


void ancientRMethods(struct trackGroup *tg)
/* setup special methods for ancientR track */
{
tg->loadItems = loadAncientR;
//tg->trackFilter = lfsFromAncientRBed;
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

void mapBoxHcWTarget(int start, int end, int x, int y, int width, int height, 
	char *group, char *item, char *statusLine, boolean target, char *otherFrame)
/* Print out image map rectangle that would invoke the htc (human track click)
 * program. */
{
char *encodedItem = cgiEncode(item);
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
hPrintf("HREF=\"%s&o=%d&t=%d&g=%s&i=%s&c=%s&l=%d&r=%d&db=%s&pix=%d\" ", 
    hgcNameAndSettings(), start, end, group, encodedItem, chromName, winStart, winEnd, 
    database, tl.picWidth);
if(target) 
    {
    hPrintf(" target=\"%s\" ", otherFrame);
    } 
hPrintf("ALT=\"%s\" TITLE=\"%s\">\n", statusLine, statusLine); 
freeMem(encodedItem);
}


/* Use the RepeatMasker style code to generate the
 * the Comparative Genomic Hybridization track */

static struct repeatItem *otherCghItem = NULL;
/*static char *cghClassNames[] = {
  "Breast", "CNS", "Colon", "Leukemia", "Lung", "Melanoma", "Ovary", "Prostate", "Renal",
  };*/
static char *cghClasses[] = {
  "BREAST", "CNS", "COLON", "LEUKEMIA", "LUNG", "MELANOMA", "OVARY", "PROSTATE", "RENAL",
  };
/* static char *cghClasses[] = {
  "BT549(D439b)", "HS578T(D268a)", "MCF7(D820b)", "MCF7ADR(D212a)", "MDA-231(D213b)", "MDA-435(D266a)", "MDA-N(D266b)", "T47D(D212b)", 
  }; */

struct repeatItem *makeCghItems()
/* Make the stereotypical CGH tracks */
{
struct repeatItem *ri, *riList = NULL;
int i;
int numClasses = ArraySize(cghClasses);
for (i=0; i<numClasses; ++i)
    {
    AllocVar(ri);
    ri->class = cghClasses[i];
    ri->className = cghClasses[i];
    slAddHead(&riList, ri);
    }
otherCghItem = riList;
slReverse(&riList);
return riList;
}

void cghLoadTrack(struct trackGroup *tg)
/* Load up CGH tracks.  (Will query database during drawing for a change.) */
{
tg->items = makeCghItems();
}

static void cghDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
{
int baseWidth = seqEnd - seqStart;
struct repeatItem *cghi;
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
int rowOffset;
struct cgh cghRecord;

/* Set up the shades of colors */
if (!exprBedColorsMade)
    makeRedGreenShades(vg);

if (isFull)
    {
    /* Create tissue specific average track */
    struct hash *hash = newHash(6);
    char statusLine[128];

    for (cghi = tg->items; cghi != NULL; cghi = cghi->next)
        {
	cghi->yOffset = y;
	y += lineHeight;
	hashAdd(hash, cghi->class, cghi);
	}
    sr = hRangeQuery(conn, "cgh", chromName, winStart, winEnd, "type = 2", &rowOffset);
    /* sr = hRangeQuery(conn, "cgh", chromName, winStart, winEnd, "type = 3", &rowOffset); */
    while ((row = sqlNextRow(sr)) != NULL)
        {
	cghStaticLoad(row+rowOffset, &cghRecord);
	cghi = hashFindVal(hash, cghRecord.tissue); 
	/* cghi = hashFindVal(hash, cghRecord.name); */
	if (cghi == NULL)
	   cghi = otherCghItem;
	col = getExprDataColor((cghRecord.score * -1), 0.7, TRUE);
	x1 = roundingScale(cghRecord.chromStart-winStart, width, baseWidth)+xOff;
	x2 = roundingScale(cghRecord.chromEnd-winStart, width, baseWidth)+xOff;
	w = x2-x1;
	if (w <= 0)
	    w = 1;
	vgBox(vg, x1, cghi->yOffset, w, heightPer, col);
        }
    freeHash(&hash);
    }
else
    {
    sr = hRangeQuery(conn, "cgh", chromName, winStart, winEnd, "type = 1", &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	cghStaticLoad(row+rowOffset, &cghRecord);
	col = getExprDataColor((cghRecord.score * -1), 0.5, TRUE);
	x1 = roundingScale(cghRecord.chromStart-winStart, width, baseWidth)+xOff;
	x2 = roundingScale(cghRecord.chromEnd-winStart, width, baseWidth)+xOff;
	w = x2-x1;
	if (w <= 0)
	  w = 1;
	vgBox(vg, x1, yOff, w, heightPer, col);
        }
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void cghMethods(struct trackGroup *tg)
/* Make track group for CGH experiments. */
{
tg->loadItems = cghLoadTrack;
tg->freeItems = repeatFree;
tg->drawItems = cghDraw;
tg->colorShades = shadesOfGray;
tg->itemName = repeatName;
tg->mapItemName = repeatName;
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = tgWeirdItemStart;
tg->itemEnd = tgWeirdItemEnd;
}


void loadMcnBreakpoints(struct trackGroup *tg)
/* Load up MCN breakpoints from database table to trackGroup items. */
{
bedLoadItem(tg, "mcnBreakpoints", (ItemLoader)mcnBreakpointsLoad);
}

void freeMcnBreakpoints(struct trackGroup *tg)
/* Free up MCN Breakpoints items. */
{
mcnBreakpointsFreeList((struct mcnBreakpoints**)&tg->items);
}

void mcnBreakpointsMethods(struct trackGroup *tg)
/* Make track group for mcnBreakpoints. */
{
tg->loadItems = loadMcnBreakpoints;
tg->freeItems = freeMcnBreakpoints;
}


static void drawTriangle(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw triangle items.   Relies mostly on bedDrawSimple, but does put
 * a horizontal box connecting items in full mode. */
{
/* In dense mode try and draw golden background for promoter regions. */
if (vis == tvDense)
    {
    if (hTableExists("rnaCluster"))
        {
	int heightPer = tg->heightPer;
	Color gold = vgFindColorIx(vg, 250,190,60);
	int promoSize = 1000;
	int rowOffset;
	int baseWidth = seqEnd - seqStart;
	double scale = width/(double)baseWidth;
	struct sqlConnection *conn = hAllocConn();
	struct sqlResult *sr = hRangeQuery(conn, "rnaCluster", chromName, 
		winStart - promoSize, winEnd + promoSize, NULL, &rowOffset);
	char **row;
	// vgBox(vg, xOff, yOff, width, heightPer, gold);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    int start, end;
	    row += rowOffset;
	    if (row[5][0] == '-')
	        {
		start = atoi(row[2]);
		end = start + promoSize;
		}
	    else
	        {
		end = atoi(row[1]);
		start = end - promoSize;
		}
	    drawScaledBox(vg, start, end, scale, xOff, yOff, heightPer, gold);
	    }

	hFreeConn(&conn);
	}
    }
bedDrawSimple(tg, seqStart, seqEnd, vg, xOff, yOff, width, font, color, vis);
}

void triangleMethods(struct trackGroup *tg)
/* Register custom methods for regulatory triangle track. */
{
tg->drawItems = drawTriangle;
}

void smallBreak()
/* Draw small horizontal break */
{
hPrintf("<FONT SIZE=1><BR></FONT>\n");
}

void drawButtonBox(struct vGfx *vg, int x, int y, int w, int h, int enabled)
/* Draw a min-raised looking button. */
{
int light = shadesOfGray[1], mid = shadesOfGray[2], dark = shadesOfGray[4];
if (enabled) 
    {
    vgBox(vg, x, y, w, 1, light);
    vgBox(vg, x, y+1, 1, h-1, light);
    vgBox(vg, x+1, y+1, w-2, h-2, mid);
    vgBox(vg, x+1, y+h-1, w-1, 1, dark);
    vgBox(vg, x+w-1, y+1, 1, h-1, dark);
    }
else				/* try to make the button look as if
				 * it is already depressed */
    {
    vgBox(vg, x, y, w, 1, dark);
    vgBox(vg, x, y+1, 1, h-1, dark);
    vgBox(vg, x+1, y+1, w-2, h-2, light);
    vgBox(vg, x+1, y+h-1, w-1, 1, light);
    vgBox(vg, x+w-1, y+1, 1, h-1, light);
    }
}


void beforeFirstPeriod( char *str )
{
char *t = rindex( str, '.' );

if( t == NULL )
    return;
else
    str[strlen(str) - strlen(t)] = '\0';
}

void printYAxisLabel( struct vGfx *vg, int y, struct trackGroup *group, char *labelString,
        double min0, double max0 )
/*print a label for a horizontal y-axis line*/
{
    double tmp;
    int fontHeight = mgFontLineHeight(tl.font);
    double ymin = y - (group->heightPer / 2) + fontHeight;
    int itemHeight0 = group->itemHeight(group, group->items);
    int inWid = trackOffsetX()-gfxBorder*3;
    
    tmp = -whichBin( atof(labelString), min0, max0, 999 );
    tmp = (int)((double)ymin+((double)tmp)*(double)group->heightPer/1000.0+(double)group->heightPer)-fontHeight/2.0;
    if( !withCenterLabels ) tmp -= fontHeight;
    vgTextRight(vg, gfxBorder, tmp, inWid-1, itemHeight0, group->ixColor, tl.font, labelString );
}

void makeActiveImage(struct trackGroup *groupList, char *psOutput)
/* Make image and image map. */
{
struct trackGroup *group;
MgFont *font = tl.font;
struct vGfx *vg;
struct tempName gifTn;
char *mapName = "map";
int fontHeight = mgFontLineHeight(font);
int insideHeight = fontHeight-1;
int insideX = trackOffsetX();
int trackTabX = gfxBorder;
int trackTabWidth = 11;
int trackPastTabX = (withLeftLabels ? trackTabWidth : 0);
int trackPastTabWidth = tl.picWidth - trackPastTabX;
int pixWidth, pixHeight;
int insideWidth;
int y;
int typeCount = slCount(groupList);
int leftLabelWidth = 0;
int rulerHeight = fontHeight;
int yAfterRuler = gfxBorder;
int relNumOff;
int i;

int ymin, ymax;
int scaledHeightPer;
double minRange, maxRange;
int binCount = 999;
char minRangeStr[32];
char maxRangeStr[32];

struct slList *prev = NULL;
int start;
int newy;

/* Figure out dimensions and allocate drawing space. */
pixWidth = tl.picWidth;
insideWidth = pixWidth-gfxBorder-insideX;
pixHeight = gfxBorder;
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
imagePixelHeight = pixHeight;
if (psOutput)
    vg = vgOpenPostScript(pixWidth, pixHeight, psOutput);
else
    {
    makeTempName(&gifTn, "hgt", ".gif");
    vg = vgOpenGif(pixWidth, pixHeight, gifTn.forCgi);
    }
makeGrayShades(vg);
makeBrownShades(vg);
makeSeaShades(vg);

/* Start up client side map. */
hPrintf("<MAP Name=%s>\n", mapName);
/* Find colors to draw in. */
for (group = groupList; group != NULL; group = group->next)
    {
    if (group->limitedVis != tvHide)
	{
	group->ixColor = vgFindRgb(vg, &group->color);
	group->ixAltColor = vgFindRgb(vg, &group->altColor);
	}
    }

/* Draw left labels. */
if (withLeftLabels)
    {
    int inWid = insideX-gfxBorder*3;
    int nextY, lastY, trackIx = 0;
    double min0, max0;

    vgBox(vg, insideX-gfxBorder*2, 0, gfxBorder, pixHeight, 
    	vgFindColorIx(vg, 0, 0, 200));
    vgSetClip(vg, gfxBorder, gfxBorder, inWid, pixHeight-2*gfxBorder);
    y = gfxBorder;
    if (withRuler)
	{
	vgTextRight(vg, gfxBorder, y, inWid-1, rulerHeight, 
	    MG_BLACK, font, "Base Position");
	y += rulerHeight;
	}
    for (group = groupList; group != NULL; group = group->next)
        {
	struct slList *item;
	int h;
	double tmp;
	lastY = y;
	if (group->limitedVis != tvHide)
	    {
	    nextY = lastY + group->totalHeight(group, group->limitedVis);
	    if (withCenterLabels)
		nextY += fontHeight;
	    h = nextY - lastY - 1;
 	    drawButtonBox(vg, trackTabX, lastY, trackTabWidth, h, group->hasUi); 
	    if (group->hasUi)
		mapBoxTrackUi(trackTabX, lastY, trackTabWidth, h, group);
	    }

	if( sameString( group->mapName, "humMus" ) )
	    {

	    min0 = whichNum( 300.0, -7.99515, 6.54171, binCount );
	    max0 =  whichNum( 1000.0, -7.99515, 6.54171, binCount );
	    sprintf( minRangeStr, "%0.2g", min0  );
	    sprintf( maxRangeStr, "%0.2g", max0 );


	    if( group->limitedVis == tvDense )
		{
		printYAxisLabel( vg, y, group, "0.0", min0, max0 );
		printYAxisLabel( vg, y, group, "2.0", min0, max0 );
		printYAxisLabel( vg, y, group, "5.0", min0, max0 );
		}

	    }
	else if( sameString( group->mapName, "humMusL" ) ||
		 sameString( group->mapName, "musHumL" ))
	    {

	    minRange = 0.0;
	    maxRange = whichBin( 6.0, 0.0, 8.0 ,binCount ); 
	    min0 = whichNum( minRange, 0.0, 8.0, binCount );
	    max0 = whichNum( maxRange, 0.0, 8.0, binCount );


	    sprintf( minRangeStr, " "  );
	    sprintf( maxRangeStr, " " );

	    if( group->limitedVis == tvFull && group->heightPer >= 74  )
		{
		printYAxisLabel( vg, y+5, group, "1.0", min0, max0 );
		printYAxisLabel( vg, y+5, group, "2.0", min0, max0 );
		printYAxisLabel( vg, y+5, group, "3.0", min0, max0 );
		printYAxisLabel( vg, y+5, group, "4.0", min0, max0 );
		printYAxisLabel( vg, y+5, group, "5.0", min0, max0 );
		printYAxisLabel( vg, y+5, group, "6.0", min0, max0 );
		}

	    }

	else if( sameString( group->mapName, "zoo" ) )
	    {
	    sprintf( minRangeStr, "%d", (int)whichNum( 500.0, 1.0, 100.0, 1000 ));
	    sprintf( maxRangeStr, "%d", (int)whichNum( 1000.0, 1.0, 100.0, 1000 ));
	    }
	else if( sameString( group->mapName, "zooCons" ) )
	    {
	    sprintf( minRangeStr, "%d", (int)whichNum( 0.0, 0.0, 5.0, 1000 ));
	    sprintf( maxRangeStr, "%d", (int)whichNum( 1000.0, 0.0, 5.0, 1000 ));
	    }
	else if( sameString( group->mapName, "binomialCons2" ) )
	    {
	    sprintf( minRangeStr, "%d", (int)0.0 );
	    sprintf( maxRangeStr, "%d", (int)300.0);
	    }
	else if( sameString( group->mapName, "binomialCons3" ) )
	    {
	    sprintf( minRangeStr, "%d", (int)0.0);
	    sprintf( maxRangeStr, "%d", (int)200.0);
	    }
	else if( sameString( group->mapName, "binomialCons" ) )
	    {
	    sprintf( minRangeStr, "%d", (int)0.0);
	    sprintf( maxRangeStr, "%d", (int)500.0);
	    }
	else
	    {
	    sprintf( minRangeStr, "%d", 1); //whichNum( 1.0, 0.0, 100.0, 1000 ));
	    sprintf( maxRangeStr, "%d", 100);// whichNum( 1000.0, 0.0, 100.0, 1000 ));
	    }

	
	switch (group->limitedVis)
	    {
	    case tvHide:
		break;	/* Do nothing; */
	    case tvFull:
		if (withCenterLabels)
		    y += fontHeight;
		start = 1;

		if( group->subType == lfSubSample && group->items == NULL )
		    y += group->height;


		for (item = group->items; item != NULL; item = item->next)
		    {
	            char *rootName;
		    char *name = group->itemName(group, item);
		    int itemHeight = group->itemHeight(group, item);
		    newy = y;
		    
		    
		    /* Do some fancy stuff for sample tracks. 
		     * Draw y-value limits for 'sample' tracks. */
		    if(group->subType == lfSubSample )
			{

			if( prev == NULL )
			    newy += itemHeight;
			else
			    newy += updateY( name, group->itemName(group, prev), itemHeight );
			if( newy == y )
			    continue;

			if( group->heightPer > (3 * fontHeight ) )
			    {
			    ymax = y - (group->heightPer / 2) + (fontHeight / 2);
			    ymin = y + (group->heightPer / 2) - (fontHeight / 2);
			    vgTextRight(vg, gfxBorder, ymin, inWid-1, itemHeight,
					group->ixAltColor, font, minRangeStr );
			    vgTextRight(vg, gfxBorder, ymax, inWid-1, itemHeight,
					group->ixAltColor, font, maxRangeStr );
			    }
			prev = item;

			rootName = cloneString( name );
			beforeFirstPeriod( rootName );
			if( sameString( group->mapName, "humMusL" ))
			    vgTextRight(vg, gfxBorder, y, inWid - 1, itemHeight,
					group->ixColor, font, "Mouse Cons    ");
			else if( sameString( group->mapName, "musHumL" ))
			    vgTextRight(vg, gfxBorder, y, inWid - 1, itemHeight,
					group->ixColor, font, "Human Cons    ");
			else
			    vgTextRight(vg, gfxBorder, y, inWid - 1, itemHeight,
					group->ixColor, font, rootName );

			freeMem( rootName );
			start = 0;
			y = newy;
			}
		    else
			{
			vgTextRight(vg, gfxBorder, y, inWid - 1, 
				itemHeight, group->ixColor, font, name);
			y += itemHeight;
			}
		    }
		break;
	    case tvDense:
		
		if (withCenterLabels)
		    y += fontHeight;
		
		/*draw y-value limits for 'sample' tracks. (always puts 0-100% range)*/
		if( group->subType == lfSubSample && group->heightPer > (3 * fontHeight ) )
		    {
		    ymax = y - (group->heightPer / 2) + (fontHeight / 2);
		    ymin = y + (group->heightPer / 2) - (fontHeight / 2);
		    vgTextRight(vg, gfxBorder, ymin, inWid-1, group->lineHeight, 
				group->ixAltColor, font, minRangeStr );
		    vgTextRight(vg, gfxBorder, ymax, inWid-1, group->lineHeight, 
				group->ixAltColor, font, maxRangeStr );
		    }
		vgTextRight(vg, gfxBorder, y, inWid-1, group->lineHeight, 
			    group->ixColor, font, group->shortLabel);
		y += group->lineHeight;
		break;
	    }
        }
    vgUnclip(vg);
    }

/* Draw guidelines. */
if (withGuidelines)
    {
    int clWidth = insideWidth;
    int ochXoff = insideX + clWidth;
    int height = pixHeight - 2*gfxBorder;
    int x;
    Color color = vgFindRgb(vg, &guidelineColor);
    int lineHeight = mgFontLineHeight(tl.font)+1;

    vgSetClip(vg, insideX, gfxBorder, insideWidth, height);
    y = gfxBorder;

    for (x = insideX+guidelineSpacing-1; x<pixWidth; x += guidelineSpacing)
	vgBox(vg, x, y, 1, height, color);
    vgUnclip(vg);
    }

/* Show ruler at top. */
if (withRuler)
    {
    y = 0;
    vgSetClip(vg, insideX, y, insideWidth, rulerHeight);
    relNumOff = winStart;
    vgDrawRulerBumpText(vg, insideX, y, rulerHeight, insideWidth, MG_BLACK, font,
			relNumOff, winBaseCount, 0, 1);
    vgUnclip(vg);

    /* Make hit boxes that will zoom program around ruler. */
	{
	int boxes = 30;
	int winWidth = winEnd - winStart;
	int newWinWidth = winWidth/3;
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
	    if (ns < 0)
		{
		ns = 0;
		ne -= ns;
		}
	    if (ne > seqBaseCount)
		{
		ns -= (ne - seqBaseCount);
		ne = seqBaseCount;
		}
	    mapBoxJumpTo(ps+insideX,y,pe-ps,rulerHeight,
			 chromName, ns, ne, "3x zoom");
	    }
	}
    }


/* Draw center labels. */
if (withCenterLabels)
    {
    int clWidth = insideWidth;
    int ochXoff = insideX + clWidth;
    vgSetClip(vg, insideX, gfxBorder, insideWidth, pixHeight - 2*gfxBorder);
    y = yAfterRuler;
    for (group = groupList; group != NULL; group = group->next)
        {
	if (group->limitedVis != tvHide)
	    {
	    Color color = group->ixColor;
	    vgTextCentered(vg, insideX, y+1, 
			   clWidth, insideHeight, color, font, group->longLabel);
	    mapBoxToggleVis(trackPastTabX, y+1, 
			    trackPastTabWidth, insideHeight, group);
	    y += fontHeight;
	    y += group->height;
	    }
        }
    vgUnclip(vg);
    }


/* Draw tracks. */
y = yAfterRuler;
for (group = groupList; group != NULL; group = group->next)
    {
    if (group->limitedVis != tvHide)
	{
	if (withCenterLabels)
	    y += fontHeight;
	vgSetClip(vg, insideX, y, insideWidth, group->height);
	group->drawItems(group, winStart, winEnd,
			 vg, insideX, y, insideWidth, 
			 font, group->ixColor, group->limitedVis);
	vgUnclip(vg);
	y += group->height;
	}
    }

/* Make map background. */
{

int currentX, currentXEnd, currentWidth;
int leftSide, rightSide;

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
	    start = 1;
	    if( group->subType == lfSubSample && group->items == NULL )
		y += group->lineHeight;

	    for (item = group->items; item != NULL; item = item->next)
		{
		int height = group->itemHeight(group, item);

            

		/*wiggle tracks don't always increment height (y-value) here*/
		if( group->subType == lfSubSample )
		    {
		    newy = y;
		    if( !start && item->next != NULL  )
			{
			newy += updateY( group->itemName(group, item),
					 group->itemName(group, item->next),
					 height );
			}
		    else if( item->next != NULL || start )
			newy += height;
		    start = 0;
		    y = newy;
		    }
		else
		    {
		    if (!group->mapsSelf)
			{
                        mapBoxHc(group->itemStart(group, item), group->itemEnd(group, item),
				 trackPastTabX,y,trackPastTabWidth,height, group->mapName,
				 group->mapItemName(group, item),
				 group->itemName(group, item));
			}
		    y += height;
		    }
		}
	    break;
	case tvDense:
	    if (withCenterLabels)
		y += fontHeight;
	    mapBoxToggleVis(trackPastTabX,y,trackPastTabWidth,group->lineHeight,group);
	    y += group->lineHeight;
    
	    break;
	}
    }
}
/* Finish map. */
hPrintf("</MAP>\n");

/* Save out picture and tell html file about it. */
vgClose(&vg);
smallBreak();
smallBreak();
hPrintf(
    "<IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s onMouseOut=\"javascript:popupoff();\"><BR>\n",
    gifTn.forHtml, pixWidth, pixHeight, mapName);
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
    hPrintf("<A HREF=\"http://www.ensembl.org/perl/contigview"
	   "?chr=%s&vc_start=%d&vc_end=%d&wvc_start=%d&wvc_end=%d\" TARGET=_blank>",
	    chromName, bigStart, bigEnd, smallStart, smallEnd);
    }
else
    {
    hPrintf("<A HREF=\"http://www.ensembl.org/perl/contigview"
	   "?chr=%s&vc_start=%d&vc_end=%d\" TARGET=_blank>",
	    chromName, bigStart, bigEnd);
    }
}

typedef void (*TrackHandler)(struct trackGroup *tg);

struct hash *handlerHash;

void registerTrackHandler(char *name, TrackHandler handler)
/* Register a track handling function. */
{
if (handlerHash == NULL)
    handlerHash = newHash(6);
if (hashLookup(handlerHash, name))
    warn("handler duplicated for track %s", name);
else
    {
    hashAdd(handlerHash, name, handler);
    }
}

TrackHandler lookupTrackHandler(char *name)
/* Lookup handler for track of give name.  Return NULL if
 * none. */
{
if (handlerHash == NULL)
    return NULL;
return hashFindVal(handlerHash, name);
}

boolean colorsSame(struct rgbColor *a, struct rgbColor *b)
/* Return true if two colors are the same. */
{
return a->r == b->r && a->g == b->g && a->b == b->b;
}

void loadSimpleBed(struct trackGroup *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct bed *(*loader)(char **row);
struct bed *bed, *list = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;

if (tg->bedSize <= 3)
    loader = bedLoad3;
else if (tg->bedSize == 4)
    loader = bedLoad;
else
    loader = bedLoad5;
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = loader(row+rowOffset);
    slAddHead(&list, bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);
tg->items = list;
}

void loadGappedBed(struct trackGroup *tg)
/* Convert bed info in window to linked feature. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoad12(row+rowOffset);
    lf = lfFromBed(bed);
    slAddHead(&lfList, lf);
    bedFree(&bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}

void loadSampleIntoLinkedFeature(struct trackGroup *tg)
/* Convert sample info in window to linked feature. */
{
int maxWiggleTrackHeight = 2500;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct sample *sample;
struct linkedFeatures *lfList = NULL, *lf;
char *hasDense = NULL;
char *where = NULL;
char query[256];

/*see if we have a summary table*/
snprintf(query, sizeof(query), 
	"select name from %s where name = '%s' limit 1", 
	tg->mapName, tg->shortLabel);
//errAbort( "%s", query );
hasDense = sqlQuickQuery(conn, query, query, sizeof(query));

/* If we're in dense mode and have a summary table load it. */
if(tg->visibility == tvDense)
    {
    if(hasDense != NULL)
	{
	snprintf(query, sizeof(query), " name = '%s' ", tg->shortLabel);
	where = cloneString(query);
	}
    }

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, where, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    sample = sampleLoad(row + rowOffset);
    lf = lfFromSample(sample);
    slAddHead(&lfList, lf);
    sampleFree(&sample);
    }
if(where != NULL)
    freez(&where);
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfList);

/* sort to bring items with common names to the same line
but only for tracks with a summary table (with name=shortLabel) in
dense mode*/

if( hasDense != NULL )
    {
    sortGroupList = tg; /* used to put track name at top of sorted list. */
    slSort(&lfList, lfNamePositionCmp);
    sortGroupList = NULL;
    }
tg->items = lfList;

/*turn off full mode if there are too many rows or each row is too
 * large. A total of maxWiggleTrackHeight is allowed for number of
 * rows times the rowHeight*/
if( tg->visibility == tvFull && tgUserDefinedTotalHeight( tg, tvFull ) > maxWiggleTrackHeight  )
    {
    tg->limitedVisSet = TRUE;
    tg->limitedVis = tvDense;
    }
}

void loadSampleZoo(struct trackGroup *tg)
/* Convert sample info in window to linked feature. */
{
int maxWiggleTrackHeight = 2500;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
struct sample *sample;
struct linkedFeatures *lfList = NULL, *lf;
char *hasDense = NULL;
char *where = NULL;
char query[256];

/*see if we have a summary table*/
snprintf(query, sizeof(query), "select name from %s where name = '%s' limit 1", tg->mapName, tg->shortLabel);
//errAbort( "%s", query );
hasDense = sqlQuickQuery(conn, query, query, sizeof(query));

/* If we're in dense mode and have a summary table load it. */
if(tg->visibility == tvDense)
    {
    if(hasDense != NULL)
	{
	snprintf(query, sizeof(query), " name = '%s' ", tg->shortLabel);
	where = cloneString(query);
	}
    }

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, where, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    sample = sampleLoad(row + rowOffset);
    lf = lfFromSample(sample);
    slAddHead(&lfList, lf);
    sampleFree(&sample);
    }
if(where != NULL)
    freez(&where);
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfList);

/* sort to bring items with common names to the same line
but only for tracks with a summary table (with name=shortLabel) in
dense mode*/
if( hasDense != NULL )
    {
    sortGroupList = tg; /* used to put track name at top of sorted list. */
    slSort(&lfList, lfNamePositionCmp);
    sortGroupList = NULL;
    }

// Sort in species phylogenetic order
slSort(&lfList, lfZooCmp);

tg->items = lfList;

/*turn off full mode if there are too many rows or each row is too
 * large. A total of maxWiggleTrackHeight is allowed for number of
 * rows times the rowHeight*/
if( tg->visibility == tvFull && tgUserDefinedTotalHeight( tg, tvFull ) > maxWiggleTrackHeight  )
    {
    tg->limitedVisSet = TRUE;
    tg->limitedVis = tvDense;
    }
}

void loadGenePred(struct trackGroup *tg)
/* Convert bed info in window to linked feature. */
{
tg->items = lfFromGenePredInRange(tg->mapName, chromName, winStart, winEnd);
}

void loadPsl(struct trackGroup *tg)
/* load up all of the psls from correct table into tg->items item list*/
{
lfFromPslsInRange(tg, winStart,winEnd, chromName, FALSE, FALSE);
}

void loadXenoPsl(struct trackGroup *tg)
/* load up all of the psls from correct table into tg->items item list*/
{
lfFromPslsInRange(tg, winStart,winEnd, chromName, TRUE, FALSE);
}

void drawColorMethods(struct trackGroup *tg)
/* Fill in color track items based on chrom  */
{
char option[128]; /* Option -  rainbow chromosome color */
char optionChr[128]; /* Option -  chromosome filter */
char *optionChrStr; 
char *optionStr ;
snprintf( option, sizeof(option), "%s.color", tg->mapName);
optionStr = cartUsualString(cart, option, "off");
tg->mapItemName = mapNameFromLfExtra;
if( sameString( optionStr, "on" )) /*use chromosome coloring*/
    tg->itemColor = pslItemColor;
else
    tg->itemColor = NULL;
linkedFeaturesMethods(tg);
tg->loadItems = loadGenePred;
}

void netMethods(struct trackGroup *tg)
/* Make track group for chain/net alignment. */
{
tg->loadItems = netLoad;
tg->freeItems = netFree;
tg->drawItems = netDraw;
tg->itemName = netName;
tg->mapItemName = netName;
tg->totalHeight = tgFixedTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = tgWeirdItemStart;
tg->itemEnd = tgWeirdItemEnd;
}

void fillInFromType(struct trackGroup *group, struct trackDb *tdb)
/* Fill in various function pointers in group from type field of tdb. */
{

char *typeLine = tdb->type, *words[8], *type;
int wordCount;
if (typeLine == NULL)
    return;
wordCount = chopLine(typeLine, words);
if (wordCount <= 0)
    return;
type = words[0];
if (sameWord(type, "bed"))
    {
    int fieldCount = 3;
    if (wordCount > 1)
        fieldCount = atoi(words[1]);
    group->bedSize = fieldCount;
    if (fieldCount < 12)
	{
	bedMethods(group);
	group->loadItems = loadSimpleBed;
	}
    else 
	{
	linkedFeaturesMethods(group);
	group->loadItems = loadGappedBed;
	}
    }
else if (sameWord(type, "sample"))
    {
    int fieldCount = 9;

    if (wordCount > 1)
        fieldCount = atoi(words[1]);
    group->bedSize = fieldCount;
    if (fieldCount == 9)
	{
	group->subType = lfSubSample;     /*make subType be "sample" (=2)*/
	sampleLinkedFeaturesMethods(group);
	group->loadItems = loadSampleIntoLinkedFeature;
	}
    else
	{
	errAbort("A 'sample' track must have exactly 9 fields.(%d)", fieldCount);
	}
    }
else if (sameWord(type, "genePred"))
    {
    linkedFeaturesMethods(group);
    group->loadItems = loadGenePred;
    group->colorShades = NULL;
    }
else if (sameWord(type, "psl"))
    {
    char *subType = ".";
    if (wordCount >= 2)
       subType = words[1];
    linkedFeaturesMethods(group);
    if (!tdb->useScore)
        group->colorShades = NULL;
    if (sameString(subType, "xeno"))
	{
	group->loadItems = loadXenoPsl;
	group->subType = lfSubXeno;
	}
    else
	group->loadItems = loadPsl;
    if (sameString(subType, "est"))
	group->drawItems = linkedFeaturesAverageDense;
    }
else if (sameWord(type, "chain"))
    {
    char *subType = ".";
    if (wordCount >= 2)
       subType = words[1];
    linkedFeaturesMethods(group);
    if (!tdb->useScore)
        group->colorShades = NULL;
    }
else if (sameWord(type, "netAlign"))
    {
    netMethods(group);
    }
}

struct trackGroup *trackGroupFromTrackDb(struct trackDb *tdb)
/* Create a track group based on the tdb. */
{
struct trackGroup *group;
AllocVar(group);
group->mapName = cloneString(tdb->tableName);
group->visibility = tdb->visibility;
group->shortLabel = tdb->shortLabel;
group->longLabel = tdb->longLabel;
group->color.r = tdb->colorR;
group->color.g = tdb->colorG;
group->color.b = tdb->colorB;
group->altColor.r = tdb->altColorR;
group->altColor.g = tdb->altColorG;
group->altColor.b = tdb->altColorB;
group->lineHeight = mgFontLineHeight(tl.font)+1;
group->heightPer = group->lineHeight - 1;
group->private = tdb->private;
group->priority = tdb->priority;
if (tdb->useScore)
    {
    /* Todo: expand spectrum opportunities. */
    if (colorsSame(&brownColor, &group->color))
        group->colorShades = shadesOfBrown;
    else if (colorsSame(&darkSeaColor, &group->color))
        group->colorShades = shadesOfSea;
    else
	group->colorShades = shadesOfGray;
    }
fillInFromType(group, tdb);
return group;
}

void loadFromTrackDb(struct trackGroup **pTrackList)
/* Load tracks from database, consulting handler list. */
{
struct trackDb *tdb, *tdbList = hTrackDb(chromName);
struct trackGroup *group;
TrackHandler handler;

for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    group = trackGroupFromTrackDb(tdb);
    group->hasUi = TRUE;
    handler = lookupTrackHandler(tdb->tableName);
    if (handler != NULL)
	handler(group);
    if (group->loadItems == NULL)
        warn("No load handler for %s", tdb->tableName);
    else if (group->drawItems == NULL)
        warn("No draw handler for %s", tdb->tableName);
    else
	{
	slAddHead(pTrackList, group);
	}
    }
}

void rikenMrnaLoadItems(struct trackGroup *tg)
/* Load riken mrna's  - have to get them from special secret database. */
{
struct sqlConnection *conn = sqlConnect("mgsc");
connectedLfFromPslsInRange(conn, tg, winStart, winEnd, chromName, FALSE, FALSE);
sqlDisconnect(&conn);
}

void rikenMethods(struct trackGroup *tg)
/* Load up riken mRNA specific methods. */
{
tg->loadItems = rikenMrnaLoadItems;
}

void ensRikenLoadItems(struct trackGroup *tg)
/* Load ensRiken genepreds  - have to get them from special secret database. */
{
struct sqlConnection *conn = sqlConnect("mgsc");
tg->items = connectedLfFromGenePredInRange(conn, tg->mapName, chromName,
					   winStart, winEnd);
sqlDisconnect(&conn);
}

void ensRikenMethods(struct trackGroup *tg)
/* Load up ensRiken specific methods. */
{
tg->loadItems = ensRikenLoadItems;
}

void ensMergeLoadItems(struct trackGroup *tg)
/* Load ensMerge genepreds  - have to get them from special secret database. */
{
struct sqlConnection *conn = sqlConnect("mgsc");
tg->items = connectedLfFromGenePredInRange(conn, tg->mapName, chromName,
					   winStart, winEnd);
sqlDisconnect(&conn);
}

void ensMergeMethods(struct trackGroup *tg)
/* Load up ensMerge specific methods. */
{
tg->loadItems = ensMergeLoadItems;
}

void secretRikenTracks(struct trackGroup **pTrackList)
/* If not on right host remove Riken tracks. */
{
if (!hIsMgscHost())
    {
    struct trackGroup *tg;
    for (tg = *pTrackList; tg != NULL; tg = tg->next)
        {
	  if (sameString(tg->mapName, "rikenMrna") ||
	      sameString(tg->mapName, "ensRiken")  ||
	      sameString(tg->mapName, "ensMergeTierB")  ||
	      sameString(tg->mapName, "ensMergeTierC")  ||
	      sameString(tg->mapName, "ensMergeTierD"))
	     {
	     slRemoveEl(pTrackList, tg);
	     }
	}
    }
}


void ctLoadSimpleBed(struct trackGroup *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct customTrack *ct = tg->customPt;
struct bed *bed, *nextBed, *list = NULL;
for (bed = ct->bedList; bed != NULL; bed = nextBed)
    {
    nextBed = bed->next;
    if (bed->chromStart < winEnd && bed->chromEnd > winStart 
    		&& sameString(chromName, bed->chrom))
	{
	slAddHead(&list, bed);
	}
    }
slSort(&list, bedCmp);
tg->items = list;
}

void ctLoadGappedBed(struct trackGroup *tg)
/* Convert bed info in window to linked feature. */
{
struct customTrack *ct = tg->customPt;
struct bed *bed;
struct linkedFeatures *lfList = NULL, *lf;

for (bed = ct->bedList; bed != NULL; bed = bed->next)
    {
    if (bed->chromStart < winEnd && bed->chromEnd > winStart 
    		&& sameString(chromName, bed->chrom))
	{
	lf = lfFromBed(bed);
	slAddHead(&lfList, lf);
	}
    }
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}

char *ctMapItemName(struct trackGroup *tg, void *item)
/* Return composite item name for custom tracks. */
{
  char *itemName = tg->itemName(tg, item);
  static char buf[256];
  sprintf(buf, "%s %s", ctFileName, itemName);
  return buf;
}

struct trackGroup *newCustomTrack(struct customTrack *ct)
/* Make up a new custom track. */
{
struct trackGroup *tg;
char buf[64];
tg = trackGroupFromTrackDb(ct->tdb);
if (ct->fieldCount < 12)
    {
    tg->loadItems = ctLoadSimpleBed;
    }
else
    {
    tg->loadItems = ctLoadGappedBed;
    }
tg->customPt = ct;
tg->mapItemName = ctMapItemName;
return tg;
}

char *getPositionFromCustomTracks()
/*
  Parses custom track data to get the position variable
return - The first chromosome position variable found in the 
 custom track data.
 */
{
char *pos = NULL;
struct slName *bl = NULL;

ctList = customTracksParseCart(cart, &browserLines, &ctFileName);

for (bl = browserLines; bl != NULL; bl = bl->next)
    {
    char *words[96];
    int wordCount;

    wordCount = chopLine(bl->name, words);
    if (wordCount >= 3)
        {
        char *command = words[1];
        if (sameString(command, "position"))
            {
            /* Return the first position found */
            pos = words[2];
            break;
            }
        }
    }

return pos;
}

void loadCustomTracks(struct trackGroup **pGroupList)
/* Load up custom tracks and append to list. */
{
struct customTrack *ct;
struct trackGroup *tg;
struct slName *bl;

/* The loading is now handled by getPositionFromCustomTracks(). */
/* Process browser commands in custom track. */
for (bl = browserLines; bl != NULL; bl = bl->next)
    {
    char *words[96];
    int wordCount;

    wordCount = chopLine(bl->name, words);
    if (wordCount > 1)
        {
	char *command = words[1];
	if (sameString(command, "hide") 
            || sameString(command, "dense") 
            || sameString(command, "full"))
	    {
	    if (wordCount > 2)
	        {
		int i;
		for (i=2; i<wordCount; ++i)
		    {
		    char *s = words[i];
		    struct trackGroup *tg;
		    boolean toAll = sameWord(s, "all");
		    for (tg = *pGroupList; tg != NULL; tg = tg->next)
		        {
			if (toAll || sameString(s, tg->mapName))
			    cartSetString(cart, tg->mapName, command);
			}
		    }
		}
	    }
	else if (sameString(command, "position"))
	    {
	    char *chrom;
	    int start, end;
            
	    if (wordCount < 3)
	        errAbort("Expecting 3 words in browser position line");
	    if (!hgIsChromRange(words[2])) 
	        errAbort("browser position needs to be in chrN:123-456 format");
	    hgParseChromRange(words[2], &chromName, &winStart, &winEnd);

            /*Fix a start window of -1 that is returned when a custom track position
              begins at 0
            */
            if (winStart < 0) 
                {
                winStart = 0;
                }
	    }
	else if (sameString(command, "pix"))
	    {
	    int width;
	    if (wordCount != 3)
	        errAbort("Expecting 3 words in pix line");
	    setPicWidth(words[2]);
	    }
	}
    }

for (ct = ctList; ct != NULL; ct = ct->next)
    {
    char *vis;
    tg = newCustomTrack(ct);
    vis = cartOptionalString(cart, tg->mapName);
    if (vis != NULL)
	tg->visibility = hTvFromString(vis);
    slAddHead(pGroupList, tg);
    }

}

char *wrapWhiteFont(char *s)
/* Write white font around s */
{
static char buf[256];
sprintf(buf, "<FONT COLOR=\"#FFFFFF\">%s</FONT>", s);
return buf;
}

void hideAllTracks(struct trackGroup *tGroupList)
/* hide all the tracks (and any in trackDb too) */
{
struct sqlConnection *conn = hAllocConn();
char *name;
struct sqlResult *sr;
char **row;
char *trackDb = hTrackDbName();
char query[256];
assert(trackDb);
snprintf(query, sizeof(query), "select tableName from %s", trackDb);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    assert(row[0]);
    cartSetString(cart, row[0], "hide");
    }
freez(&trackDb);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


void hotLinks()
/* Put up the hot links bar. */
{
boolean gotBlat = hIsBlatIndexedDatabase(database);
struct dyString *uiVars = uiStateUrlPart(NULL);

hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#000000\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>\n");
hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#536ED3\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"2\"><TR>\n");
hPrintf("<TD ALIGN=CENTER><A HREF=\"/index.html?org=%s\">%s</A></TD>", organism, wrapWhiteFont("Home"));

if (gotBlat)
    {
    hPrintf("<TD><P ALIGN=CENTER><A HREF=\"../cgi-bin/hgBlat?%s\">%s</A></TD>", uiVars->string, wrapWhiteFont("BLAT"));
    }
hPrintf("<TD ALIGN=CENTER><A HREF=\"%s&o=%d&g=getDna&i=mixed&c=%s&l=%d&r=%d&db=%s&%s\">"
      " %s </A></TD>",  hgcNameAndSettings(),
      winStart, chromName, winStart, winEnd, database, uiVars->string, wrapWhiteFont(" DNA "));
hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgText?db=%s&position=%s:%d-%d&phase=table&%s=%u\">%s</A></TD>",
       database, chromName, winStart+1, winEnd, cartSessionVarName(),
       cartSessionId(cart), wrapWhiteFont("Tables"));

if (gotBlat)
    {
    hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgCoordConv?origDb=%s&position=%s:%d-%d&phase=table&%s\">%s</A></TD>", database, chromName, winStart+1, winEnd, uiVars->string, wrapWhiteFont("Convert"));
    }
if (sameString(database, "hg12"))
    {
    hPuts("<TD ALIGN=CENTER>");
    printEnsemblAnchor();
    hPrintf("%s</A></TD>", wrapWhiteFont("Ensembl"));
    hPrintf("<TD ALIGN=CENTER><A HREF=\"http://www.ncbi.nlm.nih.gov/cgi-bin/Entrez/maps.cgi?CHR=%s&BEG=%d&END=%d\" TARGET=_blank>",
    	skipChr(chromName), winStart+1, winEnd);
    hPrintf("%s</A></TD>", wrapWhiteFont("Map View"));
    }
hPrintf("<TD ALIGN=CENTER><A HREF=\"../cgi-bin/hgTracks?%s=%u&hgt.psOutput=on\">%s</A></TD>\n",cartSessionVarName(),
       cartSessionId(cart), wrapWhiteFont("PDF/PS"));
hPrintf("<TD ALIGN=CENTER><A HREF=\"../goldenPath/help/hgTracksHelp.html\" TARGET=_blank>%s</A></TD>\n", wrapWhiteFont("Guide"));
hPuts("</TR></TABLE>");
hPuts("</TD></TR></TABLE>\n");
}

void doTrackForm(char *psOutput)
/* Make the tracks display form with the zoom/scroll
 * buttons and the active image. */
{
struct trackGroup *group;
char *freezeName = NULL;
int controlColNum=0;
char *s;
boolean hideAll = cgiVarExists("hgt.hideAll");

if (psOutput != NULL)
   {
   suppressHtml = TRUE;
   hideControls = TRUE;
   }

/* Tell browser where to go when they click on image. */
hPrintf("<FORM ACTION=\"%s\" NAME=\"TrackForm\">\n\n", hgTracksName());
cartSaveSession(cart);


/* See if want to include sequence search results. */
userSeqString = cartOptionalString(cart, "ss");
if (userSeqString && !ssFilesExist(userSeqString))
    {
    userSeqString = NULL;
    cartRemove(cart, "ss");
    }
if (!hideControls)
    hideControls = cartUsualBoolean(cart, "hideControls", FALSE);
withLeftLabels = cartUsualBoolean(cart, "leftLabels", TRUE);
withCenterLabels = cartUsualBoolean(cart, "centerLabels", TRUE);
withGuidelines = cartUsualBoolean(cart, "guidelines", TRUE);
withPopUps = cartUsualBoolean(cart, "popUps", TRUE);
s = cartUsualString(cart, "ruler", "on");
withRuler = sameWord(s, "on");

/* Register tracks that include some non-standard methods. */
registerTrackHandler("cytoBand", cytoBandMethods);
registerTrackHandler("bacEndPairs", bacEndPairsMethods);
registerTrackHandler("bacEndPairsBad", bacEndPairsBadMethods);
registerTrackHandler("bacEndPairsLong", bacEndPairsLongMethods);
registerTrackHandler("fosEndPairs", fosEndPairsMethods);
registerTrackHandler("fosEndPairsBad", fosEndPairsBadMethods);
registerTrackHandler("fosEndPairsLong", fosEndPairsLongMethods);
registerTrackHandler("genMapDb", genMapDbMethods);
registerTrackHandler("cgh", cghMethods);
registerTrackHandler("mcnBreakpoints", mcnBreakpointsMethods);
registerTrackHandler("fishClones", fishClonesMethods);
registerTrackHandler("mapGenethon", genethonMethods);
registerTrackHandler("stsMarker", stsMarkerMethods);
registerTrackHandler("stsMap", stsMapMethods);
registerTrackHandler("recombRate", recombRateMethods);
registerTrackHandler("chr18deletions", chr18deletionsMethods);
registerTrackHandler("mouseSyn", mouseSynMethods);
registerTrackHandler("mouseSynWhd", mouseSynWhdMethods);
registerTrackHandler("ensRatMusHom", ensPhusionBlastMethods);
registerTrackHandler("synteny100000", synteny100000Methods);
registerTrackHandler("syntenyBuild30", synteny100000Methods);
registerTrackHandler("syntenyBerk", syntenyBerkMethods);
registerTrackHandler("syntenySanger", syntenySangerMethods);
registerTrackHandler("mouseOrtho", mouseOrthoMethods);
registerTrackHandler("mouseOrthoSeed", mouseOrthoMethods);
//registerTrackHandler("orthoTop4", drawColorMethods);
registerTrackHandler("humanParalog", humanParalogMethods);
registerTrackHandler("isochores", isochoresMethods);
registerTrackHandler("gcPercent", gcPercentMethods);
registerTrackHandler("gcPercentSmall", gcPercentMethods);
registerTrackHandler("swiss", swissMethods);
registerTrackHandler("ctgPos", contigMethods);
registerTrackHandler("bactigPos", bactigMethods);
registerTrackHandler("gold", goldMethods);
registerTrackHandler("gap", gapMethods);
registerTrackHandler("genomicDups", genomicDupsMethods);
registerTrackHandler("clonePos", coverageMethods);
registerTrackHandler("genieKnown", genieKnownMethods);
registerTrackHandler("refGene", refGeneMethods);
registerTrackHandler("sanger22", sanger22Methods);
registerTrackHandler("genieAlt", genieAltMethods);
registerTrackHandler("ensGene", ensGeneMethods);
registerTrackHandler("mrna", mrnaMethods);
registerTrackHandler("intronEst", intronEstMethods);
registerTrackHandler("est", estMethods);
registerTrackHandler("tightMrna", mrnaMethods);
registerTrackHandler("tightEst", mrnaMethods);
registerTrackHandler("estPair", estPairMethods);
registerTrackHandler("cpgIsland", cpgIslandMethods);
registerTrackHandler("exoMouse", exoMouseMethods);
registerTrackHandler("blatHuman", longXenoPslMethods);
registerTrackHandler("blatMus", longXenoPslMethods);
registerTrackHandler("multAlignWebbcat", longXenoPslMethods);
registerTrackHandler("multAlignWebbchicken", longXenoPslMethods);
registerTrackHandler("multAlignWebbbaboon", longXenoPslMethods);
registerTrackHandler("multAlignWebbchimp", longXenoPslMethods);
registerTrackHandler("multAlignWebbcow", longXenoPslMethods);
registerTrackHandler("multAlignWebbdog", longXenoPslMethods);
registerTrackHandler("multAlignWebbfugu", longXenoPslMethods);
registerTrackHandler("multAlignWebbhuman", longXenoPslMethods);
registerTrackHandler("multAlignWebbmouse", longXenoPslMethods);
registerTrackHandler("multAlignWebbpig", longXenoPslMethods);
registerTrackHandler("multAlignWebbrat", longXenoPslMethods);
registerTrackHandler("multAlignWebbzebrafish", longXenoPslMethods);
registerTrackHandler("aarMm2", longXenoPslMethods);
registerTrackHandler("blastzMm2", longXenoPslMethods);
registerTrackHandler("blastzMm2Sc", longXenoPslMethods);
registerTrackHandler("blastzMm2Ref", longXenoPslMethods);
registerTrackHandler("blastzRecipBest", longXenoPslMethods);
registerTrackHandler("blastzStrictChainBaboon", longXenoPslMethods);
registerTrackHandler("blastzStrictChainCat", longXenoPslMethods);
registerTrackHandler("blastzStrictChainChimp", longXenoPslMethods);
registerTrackHandler("blastzStrictChainChicken", longXenoPslMethods);
registerTrackHandler("blastzStrictChainCow", longXenoPslMethods);
registerTrackHandler("blastzStrictChainDog", longXenoPslMethods);
registerTrackHandler("blastzStrictChainFugu", longXenoPslMethods);
registerTrackHandler("blastzStrictChainHuman", longXenoPslMethods);
registerTrackHandler("blastzStrictChainMouse", longXenoPslMethods);
registerTrackHandler("blastzStrictChainPig", longXenoPslMethods);
registerTrackHandler("blastzStrictChainRat", longXenoPslMethods);
registerTrackHandler("blastzStrictChainTetra", longXenoPslMethods);
registerTrackHandler("blastzStrictChainZebrafish", longXenoPslMethods);
registerTrackHandler("blatChimp", longXenoPslMethods);
registerTrackHandler("chimpBac", longXenoPslMethods);
registerTrackHandler("blastzMouse", longXenoPslMethods);
registerTrackHandler("blastzBestMouse", longXenoPslMethods);
registerTrackHandler("blastzTightMouse", longXenoPslMethods);
registerTrackHandler("blastzHg", longXenoPslMethods);
registerTrackHandler("blastzHgRef", longXenoPslMethods);
registerTrackHandler("blastzHgTop", longXenoPslMethods);
registerTrackHandler("blastzMmHg", longXenoPslMethods);
registerTrackHandler("blastzMmHgRef", longXenoPslMethods);
registerTrackHandler("blastzMmHg12", longXenoPslMethods);
registerTrackHandler("blastzMmHg12Best", longXenoPslMethods);
registerTrackHandler("blastzHuman", longXenoPslMethods);
registerTrackHandler("blastzBestHuman", longXenoPslMethods);
registerTrackHandler("blastBestHuman_75", longXenoPslMethods);
registerTrackHandler("blastBestHuman", longXenoPslMethods);
registerTrackHandler("blastzAllHuman", longXenoPslMethods);
registerTrackHandler("blastzTightHuman", longXenoPslMethods);
registerTrackHandler("blastzMouseSyn", longXenoPslMethods);
registerTrackHandler("blastzChain", chainMethods);
registerTrackHandler("xenoBestMrna", xenoMrnaMethods);
registerTrackHandler("xenoMrna", xenoMrnaMethods);
registerTrackHandler("xenoEst", xenoMrnaMethods);
registerTrackHandler("exoFish", exoFishMethods);
registerTrackHandler("tet_waba", tetWabaMethods);
registerTrackHandler("rnaGene", rnaGeneMethods);
registerTrackHandler("rmsk", repeatMethods);
registerTrackHandler("rmskNew", repeatMethods);
registerTrackHandler("simpleRepeat", simpleRepeatMethods);
registerTrackHandler("uniGene",uniGeneMethods);
registerTrackHandler("perlegen",perlegenMethods);
registerTrackHandler("nci60", nci60Methods);
registerTrackHandler("cghNci60", cghNci60Methods);
registerTrackHandler("rosetta", rosettaMethods);
registerTrackHandler("affy", affyMethods);
registerTrackHandler("affyRatio", affyRatioMethods);
registerTrackHandler("ancientR", ancientRMethods );
registerTrackHandler("altGraph", altGraphMethods );
registerTrackHandler("altGraphX", altGraphXMethods );
registerTrackHandler("triangle", triangleMethods );
registerTrackHandler("triangleSelf", triangleMethods );
registerTrackHandler("transfacHit", triangleMethods );
/* MGC related */
registerTrackHandler("mgcNcbiPicks", estMethods);
registerTrackHandler("mgcNcbiSplicedPicks", intronEstMethods);
registerTrackHandler("mgcUcscPicks", intronEstMethods);
registerTrackHandler("humMusL", humMusLMethods);
registerTrackHandler("zoo", zooMethods);
registerTrackHandler("musHumL", musHumLMethods);
registerTrackHandler("affyTranscriptome", affyTranscriptomeMethods);
registerTrackHandler("rikenMrna", rikenMethods);
registerTrackHandler("ensRiken", ensRikenMethods);
registerTrackHandler("ensMergeTierB", ensMergeMethods);
registerTrackHandler("ensMergeTierC", ensMergeMethods);
registerTrackHandler("ensMergeTierD", ensMergeMethods);
registerTrackHandler("genomicSuperDups", genomicSuperDupsMethods);
registerTrackHandler("celeraDupPositive", celeraDupPositiveMethods);
registerTrackHandler("celeraCoverage", celeraCoverageMethods);

/* Load regular tracks, blatted tracks, and custom tracks. 
 * Best to load custom last. */
loadFromTrackDb(&tGroupList);
secretRikenTracks(&tGroupList);
if (userSeqString != NULL) slSafeAddHead(&tGroupList, userPslTg());
loadCustomTracks(&tGroupList);


slSort(&tGroupList, tgCmpPriority);

/* Get visibility values if any from ui. */
for (group = tGroupList; group != NULL; group = group->next)
    {
    char *s = cartOptionalString(cart, group->mapName);
    if (s != NULL)
	group->visibility = hTvFromString(s);
    }

/* If hideAll flag set, make all tracks hidden */
if(hideAll)
    {
    for (group = tGroupList; group != NULL; group = group->next)
	{
	group->visibility = tvHide;
	}
    hideAllTracks(tGroupList);
    }

/* Tell groups to load their items. */
for (group = tGroupList; group != NULL; group = group->next)
    {
    if (group->visibility != tvHide)
	{
	group->loadItems(group); 
	limitVisibility(group, group->items);
	}
    }

/* Center everything from now on. */
hPrintf("<CENTER>\n");

if (!hideControls)
    {
    hotLinks();
    /* Show title . */
    freezeName = hFreezeFromDb(database);
    if(freezeName == NULL)
	freezeName = "Unknown";
    hPrintf("<FONT SIZE=5><B>UCSC Genome Browser on %s Freeze</B></FONT><BR>\n",freezeName); 
    /* This is a clear submit button that browsers will use by default when enter is pressed in position box. */
    hPrintf("<INPUT TYPE=IMAGE BORDER=0 NAME=\"hgt.dummyEnterButton\" src=\"../images/DOT.gif\">");
    /* Put up scroll and zoom controls. */
    hWrites("move ");
    hButton("hgt.left3", "<<<");
    hButton("hgt.left2", " <<");
    hButton("hgt.left1", " < ");
    hButton("hgt.right1", " > ");
    hButton("hgt.right2", ">> ");
    hButton("hgt.right3", ">>>");
    hWrites(" zoom in ");
    hButton("hgt.in1", "1.5x");
    hButton("hgt.in2", " 3x ");
    hButton("hgt.in3", "10x");
    hWrites(" zoom out ");
    hButton("hgt.out1", "1.5x");
    hButton("hgt.out2", " 3x ");
    hButton("hgt.out3", "10x");
    hWrites("<BR>\n");

    /* Make line that says position. */
	{
	char buf[256];
        char *javascript = "onchange=\"document.location = '/cgi-bin/hgTracks?db=' + document.TrackForm.db.options[document.TrackForm.db.selectedIndex].value;\"";

        if (containsStringNoCase(database, "zoo"))
            {
            hPuts("Organism ");
            printAssemblyListHtmlExtra(database, javascript);
            }

	sprintf(buf, "%s:%d-%d", chromName, winStart+1, winEnd);
	position = cloneString(buf);
	hWrites("position ");
	hTextVar("position", position, 30);
	hPrintf("  size %d, ", winEnd-winStart);
	hWrites(" image width ");
	hIntVar("pix", tl.picWidth, 4);
	hWrites(" ");
	hButton("submit", "jump");
	hPutc('\n');
	}
    }

/* Make clickable image and map. */
makeActiveImage(tGroupList, psOutput);

if (!hideControls)
    {
    struct controlGrid *cg = NULL;

    hPrintf("<TABLE BORDER=0 CELLSPACING=1 CELLPADDING=1 WIDTH=%d COLS=%d><TR>\n", 
    	tl.picWidth, 27);
    hPrintf("<TD COLSPAN=6 ALIGN=CENTER NOWRAP>");
    hPrintf("move start<BR>");
    hButton("hgt.dinkLL", " < ");
    hTextVar("dinkL", cartUsualString(cart, "dinkL", "2.0"), 3);
    hButton("hgt.dinkLR", " > ");
    hPrintf("<TD COLSPAN=15>");
    hWrites("Click on a feature for details. "
	  "Click on base position to zoom in around cursor. "
	  "Click on left mini-buttons for track-specific options" );
    hPrintf("<TD COLSPAN=6 ALIGN=CENTER NOWRAP>");
    hPrintf("move end<BR>");
    hButton("hgt.dinkRL", " < ");
    hTextVar("dinkR", cartUsualString(cart, "dinkR", "2.0"), 3);
    hButton("hgt.dinkRR", " > ");
    hPrintf("<TR></TABLE>\n");
    smallBreak();

    /* Display bottom control panel. */
    hButton("hgt.reset", "reset all");
    hPrintf(" ");
    hButton("hgt.hideAll", "hide all");
    hPrintf(" Guidelines ");
    hCheckBox("guidelines", withGuidelines);
    hPrintf(" <B>Labels:</B> ");
    hPrintf("left ");
    hCheckBox("leftLabels", withLeftLabels);
    hPrintf("center ");
    hCheckBox("centerLabels", withCenterLabels);
    hPrintf("popUp ");
    hCheckBox("popUps", withPopUps);
    hPrintf(" ");
    hButton("submit", "refresh");
    hPrintf("<BR>\n");

    /* Display viewing options for each group. */
    /* Chuck: This is going to be wrapped in a table so that
     * the controls don't wrap around randomly
     */
    if( chromosomeColorsMade )
        {
        hPrintf("<B>Chromosome Color Key:</B><BR> ");
        hPrintf("<IMG SRC = \"../images/colorchrom.gif\" BORDER=1 WIDTH=551 HEIGHT=18 ><BR>\n");
        }
    hPrintf("<table border=0 cellspacing=1 cellpadding=1 width=%d>\n", CONTROL_TABLE_WIDTH);
    hPrintf("<tr><th colspan=%d>\n", MAX_CONTROL_COLUMNS);
    smallBreak();
    hPrintf("<B>Track Controls:</B>");
    hPrintf("</th></tr>\n");
    hPrintf("<tr>\n");
    hPrintf("<tr><td colspan='5' align='middle' nowrap>Note: Tracks with more than %d items are always displayed in "
           "dense mode.</td></tr>\n", maxItemsInFullTrack);

    cg = startControlGrid(MAX_CONTROL_COLUMNS, "left");
    controlGridStartCell(cg);
    hPrintf(" Base Position <BR>");
    hDropList("ruler", offOn, 2, offOn[withRuler]);
    controlGridEndCell(cg);
    for (group = tGroupList; group != NULL; group = group->next)
	{
	controlGridStartCell(cg);
	if (group->hasUi)
	    hPrintf("<A HREF=\"%s?%s=%u&c=%s&g=%s\">", hgTrackUiName(),
		cartSessionVarName(), cartSessionId(cart),
		chromName, group->mapName);
	hPrintf(" %s<BR> ", group->shortLabel);
	if (group->hasUi)
	    hPrintf("</A>");
	hTvDropDown(group->mapName, group->visibility);
	controlGridEndCell(cg);
	}
    /* now finish out the table */
    endControlGrid(&cg);
    hPrintf("</CENTER>\n");
    }


/* Clean up. */
for (group = tGroupList; group != NULL; group = group->next)
    {
    if (group->visibility != tvHide)
	if (group->freeItems != NULL)
		group->freeItems(group);
    }
hPrintf("</FORM>");
}


void zoomAroundCenter(double amount)
/* Set ends so as to zoom around center by scaling amount. */
{
int center = (winStart + winEnd)/2;
double newCountDbl = (winBaseCount*amount + 0.5);
int newCount;
if (newCountDbl > seqBaseCount)
    newCount = seqBaseCount;
else if (newCountDbl < 0)
    newCount = 0;
else
    newCount = (int)newCountDbl;
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

void dinkWindow(boolean start, int dinkAmount)
/* Move one end or other of window a little. */
{
if (start)
   {
   winStart += dinkAmount;
   if (winStart < 0) winStart = 0;
   }
else
   {
   winEnd += dinkAmount;
   if (winEnd > seqBaseCount)
       winEnd = seqBaseCount;
   }
}

int dinkSize(char *var)
/* Return size to dink. */
{
char *stringVal = cartOptionalString(cart, var);
double x;
double guideBases = (double)guidelineSpacing * (double)(winEnd - winStart) 
	/ ((double)tl.picWidth - trackOffsetX());

if (stringVal == NULL || !isdigit(stringVal[0]))
    {
    stringVal = "1";
    cartSetString(cart, var, stringVal);
    }
x = atof(stringVal);
return round(x*guideBases);
}

char * convertEpsToPdf(char *epsFile) 
/* Convert EPS to PDF and return filename, or NULL if failure. */
{
char *pdfTmpName = NULL, *pdfName=NULL;
char cmdBuffer[2048];
int sysVal = 0;
struct lineFile *lf = NULL;
char *line;
int lineSize=0;
float width=0, height=0;
pdfTmpName = cloneString(epsFile);

/* Get the dimensions of bounding box. */
lf = lineFileOpen(epsFile, TRUE);
while(lineFileNext(lf, &line, &lineSize)) 
    {
    if(strstr( line, "BoundingBox:")) 
	{
	char *words[5];
	chopLine(line, words);
	width = atof(words[3]);
	height = atof(words[4]);
	break;
	}
    }
lineFileClose(&lf);
	
/* Do conversion. */
chopSuffix(pdfTmpName);
pdfName = addSuffix(pdfTmpName, ".pdf");
safef(cmdBuffer, sizeof(cmdBuffer), "ps2pdf -dDEVICEWIDTHPOINTS=%d -dDEVICEHEIGHTPOINTS=%d %s %s", 
      round(width), round(height), epsFile, pdfName);
sysVal = system(cmdBuffer);
if(sysVal != 0)
    freez(&pdfName);
freez(&pdfTmpName);
return pdfName;
}

void handlePostscript()
/* Deal with Postscript output. */
{
struct tempName psTn;
char *pdfFile = NULL;
makeTempName(&psTn, "hgt", ".eps");
printf("<H1>PostScript/PDF Output</H1>\n");
printf("PostScript images can be printed at high resolution "
       "and edited by many drawing programs such as Adobe "
       "Illustrator.<BR>");
doTrackForm(psTn.forCgi);
printf("<A HREF=\"%s\">Click here to download</A> "
       "the browser graphic in PostScript.  ", psTn.forCgi);
pdfFile = convertEpsToPdf(psTn.forCgi);
if(pdfFile != NULL) 
    {
    printf("<BR><BR>PDF can be viewed with Adobe Acrobat Reader.<BR>\n");
    printf("<A HREF=\"%s\">Click here to download</A> "
	   "the browser graphic in PDF", pdfFile);
    }
else
    printf("<BR><BR>PDF format not available");
freez(&pdfFile);
}

void tracksDisplay()
/* Put up main tracks display. This routine handles zooming and
 * scrolling. */
{
char newPos[256];
position = getPositionFromCustomTracks();
if (NULL == position) 
    {
    /* Read in input from CGI. */
    position = cartString(cart, "position");
    }

if(sameString(position, ""))
    errAbort("Please go back and enter a coordinate range in the \"position\" field.<br>For example: chr22:20100000-20200000.\n");

chromName = NULL;
hgp = findGenomePos(position, &chromName, &winStart, &winEnd, cart);


if (NULL != hgp && NULL != hgp->tableList && NULL != hgp->tableList->name)
    {
    cartSetString(cart, hgp->tableList->name, "full");
    }

/* This means that no single result was found 
I.e., multiple results may have been found and are printed out prior to this code*/
if (NULL == chromName)
    {
    return;
    }

seqBaseCount = hChromSize(chromName);
winBaseCount = winEnd - winStart;

/* Do zoom/scroll if they hit it. */
if (cgiVarExists("hgt.left3"))
    relativeScroll(-0.95);
else if (cgiVarExists("hgt.left2"))
    relativeScroll(-0.475);
else if (cgiVarExists("hgt.left1"))
    relativeScroll(-0.1);
else if (cgiVarExists("hgt.right1"))
    relativeScroll(0.1);
else if (cgiVarExists("hgt.right2"))
    relativeScroll(0.475);
else if (cgiVarExists("hgt.right3"))
    relativeScroll(0.95);
else if (cgiVarExists("hgt.in3"))
    zoomAroundCenter(1.0/10.0);
else if (cgiVarExists("hgt.in2"))
    zoomAroundCenter(1.0/3.0);
else if (cgiVarExists("hgt.in1"))
    zoomAroundCenter(1.0/1.5);
else if (cgiVarExists("hgt.out1"))
    zoomAroundCenter(1.5);
else if (cgiVarExists("hgt.out2"))
    zoomAroundCenter(3.0);
else if (cgiVarExists("hgt.out3"))
    zoomAroundCenter(10.0);
else if (cgiVarExists("hgt.dinkLL"))
    dinkWindow(TRUE, -dinkSize("dinkL"));
else if (cgiVarExists("hgt.dinkLR"))
    dinkWindow(TRUE, dinkSize("dinkL"));
else if (cgiVarExists("hgt.dinkRL"))
    dinkWindow(FALSE, -dinkSize("dinkR"));
else if (cgiVarExists("hgt.dinkRR"))
    dinkWindow(FALSE, dinkSize("dinkR"));

/* Clip chromosomal position to fit. */
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
    {
    winStart = 0;
    }

if (winEnd > seqBaseCount) 
    {
    winEnd = seqBaseCount;
    }

if (winStart > seqBaseCount)
    {
    winStart = seqBaseCount - 1000;
    }

winBaseCount = winEnd - winStart;
if (winBaseCount <= 0)
    errAbort("Window out of range on %s", chromName);

/* Save computed position in cart. */
sprintf(newPos, "%s:%d-%d", chromName, winStart+1, winEnd);
cartSetString(cart, "position", newPos);
if (cgiVarExists("hgt.psOutput"))
    handlePostscript();
else
    doTrackForm(NULL);
}


void doMiddle(struct cart *theCart)
/* Print the body of an html file.   */
{
char *debugTmp = NULL;
/* Initialize layout and database. */
cart = theCart;

getDbAndGenome(cart, &database, &organism);
hSetDb(database);

debugTmp = cartUsualString(cart, "hgDebug", "off");
if(sameString(debugTmp, "on"))
    hgDebug = TRUE;
else
    hgDebug = FALSE;

hDefaultConnect();
initTl();

/* Do main display. */
tracksDisplay();
}

void doDown(struct cart *cart)
{
printf("<H2>The Browser is Being Updated</H2>\n");
printf("The browser is currently unavailable.  We are in the process of\n");
printf("updating the database and the display software with a number of\n");
printf("new tracks, including some gene predictions.  Please try again tomorrow.\n");
}

/* Other than submit and Submit all these vars should start with hgt.
 * to avoid weeding things out of other program's namespaces.
 * Because the browser is a central program, most of it's cart 
 * variables are not hgt. qualified.  It's a good idea if other
 * program's unique variables be qualified with a prefix though. */
char *excludeVars[] = { "submit", "Submit", "hgt.reset",
			"hgt.in1", "hgt.in2", "hgt.in3", 
			"hgt.out1", "hgt.out2", "hgt.out3",
			"hgt.left1", "hgt.left2", "hgt.left3", 
			"hgt.right1", "hgt.right2", "hgt.right3", 
			"hgt.dinkLL", "hgt.dinkLR", "hgt.dinkRL", "hgt.dinkRR",
			"hgt.tui", "hgt.hideAll", "hgt.psOutput", "hideControls",
			NULL };

void resetVars()
/* Reset vars except for position and database. */
{
static char *except[] = {"db", "position", NULL};
char *cookieName = hUserCookie();
int sessionId = cgiUsualInt(cartSessionVarName(), 0);
char *hguidString = findCookieData(cookieName);
int userId = (hguidString == NULL ? 0 : atoi(hguidString));
struct cart *oldCart = cartNew(userId, sessionId, NULL, NULL);
cartRemoveExcept(oldCart, except);
cartCheckout(&oldCart);
cgiVarExcludeExcept(except);
}


int main(int argc, char *argv[])
{
cgiSpoof(&argc, argv);
htmlSetBackground("../images/floret.jpg");
if (cgiVarExists("hgt.reset"))
    resetVars();

zooSpeciesHashInit();

cartHtmlShell("UCSC Genome Browser v16", doMiddle, hUserCookie(), excludeVars, NULL);
return 0;
}
