#include "common.h"
#include "portable.h"
#include "dystring.h"
#include "jksql.h"
#include "ens.h"
#include "hgap.h"
#include "memgfx.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "geneGraph.h"
#include "hgGene.h"
#include "maDbRep.h"

/* These variables persist from one incarnation of this program to the
 * next - living mostly in hidden variables. */
char *seqName;			/* Name of sequence . */
int winStart;			/* Start of window in sequence. */
int winEnd;			/* End of window in sequence. */
boolean seqReverse;		/* Look at reverse strand. */

boolean withLeftLabels = TRUE;		/* Display left labels? */
boolean withCenterLabels = TRUE;	/* Display center labels? */

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
    enum trackVisibility visibility; /* How much of this to see. */

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

    int (*totalHeight)(struct trackGroup *tg);
        /* Return total height. Called before and after drawItems. 
         * Must set the following variables. */
    int height;                /* Total height - must be set by above call. */
    int lineHeight;            /* Height per track including border. */
    int heightPer;             /* Height per track minus border. */

    void (*drawItems)(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color);
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

static int tgUsualHeight(struct trackGroup *tg)
/* Most track groups will use this to figure out the height they use. */
{
tg->lineHeight = mgFontLineHeight(tl.font)+1;
tg->heightPer = tg->lineHeight - 1;
switch (tg->visibility)
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
makeHiddenVar("old", seqName);        /* Variable set when calling ourselves. */
sprintf(buf, "%d", winStart);
makeHiddenVar("winStart", buf);
sprintf(buf, "%d", winEnd);
makeHiddenVar("winEnd", buf);
makeHiddenBoolean("seqReverse", seqReverse);
}


void mapBoxFull(int x, int y, int width, int height, struct trackGroup *curGroup)
/* Print out image map rectangle that would invoke this program again.
 * program with the current trach expanded. */
{
struct trackGroup *tg;

printf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
printf("HREF=\"../cgi-bin/hgTracks?seqName=%s&old=%s&winStart=%d&winEnd=%d", 
	seqName, seqName, winStart, winEnd);
if (withLeftLabels)
    printf("&leftLabels=on");
if (withCenterLabels)
    printf("&centerLabels=on");
for (tg = tGroupList; tg != NULL; tg = tg->next)
    {
    int vis = tvFull;
    if (tg != curGroup)
	vis = tg->visibility;
    printf("&%s=%s", tg->mapName, tvStrings[vis]);
    }
printf("\" ALT= \"Change to full view of %s track\">\n", curGroup->shortLabel);
}

void mapBoxHtc(int x, int y, int width, int height, char *group, char *item, char *statusLine)
/* Print out image map rectangle that would invoke the htc (human track click)
 * program. */
{
char *encodedItem = cgiEncode(item);
printf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
printf("HREF=\"../cgi-bin/htc?g=%s&i=%s&c=%s&l=%d&r=%d\" ", 
    group, encodedItem, seqName, winStart, winEnd);
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


static void linkedFeaturesDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color)
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
boolean isFull = (tg->visibility == tvFull);
boolean grayScale = tg->ignoresColor;
Color bColor = tg->ixAltColor;


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

struct hgNest *findBelow(struct hgNest *nest, HGID id)
/* Return element in nest matching ID. */
{
struct hgNest *child;
struct hgNest *grand;

if (nest->id == id)
    return nest;
for (child = nest->children; child != NULL; child = child->next)
    {
    if (child->children != NULL)
	{
	if ((grand = findBelow(child->children, id)) != NULL)
	    return grand;
	}
    else if (child->id == id)
	return child;
    }
return NULL;
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

struct ensTranscriptRef
/* A list of references to transcripts. */
    {
    struct ensTranscriptRef *next;	/* Next in list. */
    struct ensTranscript *ref;		/* Ref to transcript (not allocated here) */
    };

void ensGeneLoad(struct trackGroup *tg)
/* Load up genes and make a list of transcripts they contain. */
{
struct ensGene *geneList, *gene;
struct ensTranscript *trans;
struct ensTranscriptRef *trList = NULL, *tr;

tg->customPt = geneList = ensGenesInBacRange(seqName, winStart, winEnd);
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    for (trans = gene->transcriptList; trans != NULL; trans = trans->next)
	{
	AllocVar(tr);
	tr->ref = trans;
	slAddHead(&trList, tr);
	}
    }
slReverse(&trList);
tg->items = trList;
}

void ensGeneFree(struct trackGroup *tg)
/* Free up genes. */
{
ensFreeGeneList((struct ensGene **)&tg->customPt);
slFreeList(&tg->items);
}

char *ensGeneName(struct trackGroup *tg, void *item)
/* Return name of item. */
{
struct ensTranscriptRef *tr = item;
return tr->ref->id;
}

char *ensGeneMapName(struct trackGroup *tg, void *item)
/* Return map name of item. */
{
struct ensTranscriptRef *tr = item;
return tr->ref->id;
}


static void ensGeneDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color)
/* Draw gene splicing diagram items. */
{
int baseWidth = seqEnd - seqStart;
struct ensTranscriptRef *trList = tg->items;
struct ensTranscriptRef *tr;
struct ensTranscript *trans;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int midLineOff = heightPer/2;
struct dlNode *node;
boolean isFull = (tg->visibility == tvFull);

for (tr = trList; tr != NULL; tr = tr->next)
    {
    struct ensTranscript *trans = tr->ref;
    struct ensExon *firstExon = trans->exonList->head->val;
    int orientation = firstExon->orientation;
    int exonIx = 1;
    int midY = y + midLineOff;
    int x1,x2;

    ensTranscriptBounds(trans, &x1, &x2);
    x1 = roundingScale(x1-winStart, width, baseWidth) + xOff;
    x2 = roundingScale(x2-winStart, width, baseWidth) + xOff;
    if (isFull)
	mgBarbedHorizontalLine(mg, x1, midY, x2-x1, 2, 5, orientation, tg->ixAltColor);
    mgDrawBox(mg, x1, midY, x2-x1, 1, color);

    for (node = trans->exonList->head; node->next != NULL; node = node->next)
	{
	struct ensExon *exon = node->val;
	int w;
	int twid;
	char tbuf[16];
	int eStart, eEnd;
	struct contigTree *contig = exon->contig;
	eStart = ensBrowserCoordinates(contig, exon->seqStart);
	eEnd = ensBrowserCoordinates(contig, exon->seqEnd);

	x1 = roundingScale(eStart-winStart,width,baseWidth) + xOff;
	x2 = roundingScale(eEnd-winStart, width, baseWidth) + xOff;
	w = x2-x1;
	if (w < 1)
	    w = 1;
        mgDrawBox(mg, x1, y, w, heightPer, color);

      	/* Draw exon number in middle if exon is big enough. */
	sprintf(tbuf, "%d", exonIx);
	twid = mgFontStringWidth(font, tbuf);
	if (w >= twid)
	    {
	    mgTextCentered(mg, x1, y, w, heightPer,
	       MG_WHITE, font, tbuf); 
	    }
	++exonIx;
	}
    if (isFull)
	y += lineHeight;
    }
}


struct trackGroup *ensGeneTg()
/* Return the track group for EnsEMBL gene predictions. */
{
static struct trackGroup *tg = NULL;
static struct trackGroup trackGroup;
if (tg == NULL)
    {
    tg = &trackGroup;
    tg->mapName = "ensTx";
    tg->visibility = tvFull;
    tg->longLabel = "EnsEMBL Gene Predictions";
    tg->shortLabel = "EnsEMBL Genes";
    tg->color.b = 130;
    tg->altColor.r = tg->altColor.g = 128;
    tg->altColor.b = 194;
    tg->loadItems = ensGeneLoad;
    tg->freeItems = ensGeneFree;
    tg->itemName = ensGeneName;
    tg->mapItemName = ensGeneMapName;
    tg->totalHeight = tgUsualHeight;
    tg->drawItems = ensGeneDraw;
    }
return tg;
}

struct bacTreeRef
/* A reference to a bac's contig tree. */
    {
    struct bacTreeRef *next;		/* Next in list. */
    struct contigTree *bacTree;	/* Ref to bac's contig (not allocated here) */
    };

void ensContigLoad(struct trackGroup *tg)
/* Load up info about contigs. */
{
struct bacTreeRef *refList = NULL;
struct bacTreeRef *ref;
struct contigTree *bacTree;

bacTree = ensBacContigs(seqName);
AllocVar(ref);
ref->bacTree = bacTree;
slAddHead(&refList, ref);
tg->items = refList;
}

void ensContigFree(struct trackGroup *tg)
/* Free contig info. */
{
slFreeList(&tg->items);
}

char *ensContigName(struct trackGroup *tg, void *item)
/* Return name of item. */
{
struct bacTreeRef *ref = item;
return ref->bacTree->id;
}

char *ensContigMapName(struct trackGroup *tg, void *item)
/* Return map name of item (actually name of parent). */
{
struct bacTreeRef *ref = item;
return ref->bacTree->id;
}

static void ensContigDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct memGfx *mg, int xOff, int yOff, int width, 
        MgFont *font, Color color)
/* Draw contig items. */
{
int baseWidth = seqEnd - seqStart;
struct bacTreeRef *ref, *refList = tg->items;
struct contigTree *bacTree, *contig;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int midLineOff = heightPer/2;
int midY;
int x1,x2;
boolean isFull = (tg->visibility == tvFull);


for (ref = refList; ref != NULL; ref = ref->next)
    {
    /* Draw line across entire Bac. */
    bacTree = ref->bacTree;
    x1 = bacTree->browserOffset;
    x2 = x1 + bacTree->browserLength;
    x1 = roundingScale(x1-winStart, width, baseWidth) + xOff;
    x2 = roundingScale(x2-winStart, width, baseWidth) + xOff;
    midY = y + midLineOff;
    mgDrawBox(mg, x1, midY, x2-x1, 1, color);

    /* Draw boxes for each contig. */
    for (contig = ref->bacTree->children; contig != NULL; contig = contig->next)
	{
	int w;
	char *id = contig->id;
	x1 = contig->browserOffset;
	x2 = x1 + contig->browserLength;
	if (rangeIntersection(x1,x2,winStart,winEnd) > 0)
	    {
	    x1 = roundingScale(x1-winStart, width, baseWidth) + xOff;
	    x2 = roundingScale(x2-winStart, width, baseWidth) + xOff;
	    w = x2-x1;
	    if (w < 1)
		w = 1;
	    mgDrawBox(mg, x1, y, w, heightPer, color);
	    mapBoxHtc(x1, y, w, heightPer, "ensContig", id, id);

	    /* Draw full name in middle if big enough. */
	    if (mgFontStringWidth(font, id) <= w)
		{
		mgTextCentered(mg, x1, y, w, heightPer,
		   MG_WHITE, font, id); 
		}
	    /* Otherwise try and draw just number. */
	    else
		{
		char tbuf[16];
		char cloneName[32];
		int contigIx;

		ensParseContig(id, cloneName, &contigIx);
		sprintf(tbuf, "%d", contigIx);
		if (mgFontStringWidth(font, tbuf) <= w)
		    {
		    mgTextCentered(mg, x1, y, w, heightPer,
		       MG_WHITE, font, tbuf); 
		    }
		}
	    }
	}
    if (isFull)
	y += lineHeight;
    }
}

struct trackGroup *ensContigTg()
/* Return the track group for EnsEMBL gene predictions. */
{
static struct trackGroup *tg = NULL;
static struct trackGroup trackGroup;
if (tg == NULL)
    {
    tg = &trackGroup;
    tg->visibility = tvFull;
    tg->mapName = "ensClone";
    tg->longLabel = "EnsEMBL Clones and Contig Order";
    tg->shortLabel = "EnsEMBL Clones";
    tg->color.b = 140;
    tg->loadItems = ensContigLoad;
    tg->freeItems = tgFreeNothing;
    tg->itemName = ensContigName;
    tg->mapItemName = ensContigMapName;
    tg->totalHeight = tgUsualHeight;
    tg->drawItems = ensContigDraw;
    }
return tg;
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

static int cmpEnsFeat(const void *va, const void *vb)
/* Compare corder's from two contigs. */
{
const struct ensFeature *a = *((struct ensFeature **)va);
const struct ensFeature *b = *((struct ensFeature **)vb);
int diff;
diff = strcmp(a->qName, b->qName);
if (diff == 0)
    diff = a->qStart - b->qStart;
return diff;
}

enum ensAnalysisTypes
/* Will have to redo this if they reorder their table.... */
{
  eatProt = 1,
  eatFullMrna = 2,
  eatEst = 3,
  eatRepeats = 4,
  eatGenscan = 5,
  eatPfam = 6,
};

struct linkedFeatures *clusterEnsFeatures(struct ensFeature **pEfList)
/* Cluster ensemble feature list. */
{
struct ensFeature *ef;
char *lastQname = "";
struct linkedFeatures *lf = NULL, *lfList = NULL;
struct simpleFeature *sf;
struct contigTree *ct;

if (*pEfList == NULL)
    return NULL;
slSort(pEfList, cmpEnsFeat);
for (ef = *pEfList; ef != NULL; ef = ef->next)
    {
    double grayLevel = 1.0;
    int qSize = ef->qEnd - ef->qStart;
    int score = ef->score;
    if (!sameString(lastQname, ef->qName))
	{
	lastQname = ef->qName;
	if (lf != NULL)
	    finishLf(lf);
	AllocVar(lf);
	slAddHead(&lfList, lf);
	strncpy(lf->name, ef->qName, sizeof(lf->name));
	lf->orientation = ef->orientation;
	}
    AllocVar(sf);
    ct = ef->tContig;
    sf->start = ensBrowserCoordinates(ct, ef->tStart);
    sf->end = ensBrowserCoordinates(ct, ef->tEnd);
    switch (ef->type)
	{
	case eatPfam:
	    grayLevel = ef->score * 0.3 / qSize + 0.2;
	    break;
	case eatProt:
	    grayLevel = ef->score * 0.036 / qSize + 0.2;
	    break;
	case eatFullMrna:
	    grayLevel = ef->score * 0.052 / qSize + 0.2;
	    break;
	case eatEst:
	    grayLevel = ef->score * 0.089 / qSize + 0.2;
	    break;
	default:
	    grayLevel = 1.0;
	    break;
	}
    sf->grayIx = round(maxShade*grayLevel);
    if (sf->grayIx < 0)
	sf->grayIx = 0;
    if (sf->grayIx > maxShade)
	sf->grayIx = maxShade;
    slAddHead(&lf->components, sf);
    }
finishLf(lf);
slReverse(&lfList);
return lfList;
}

int pfamGrayIx(int start, int end, int score)
/* Figure out gray level for an pFam block. */
{
int size = end-start;
int halfSize = (size>>1);
int res;
res =  (maxShade * (score+(size>>2)) + halfSize)/size;
if (res > maxShade)
    res = maxShade;
return res;
}

int blastBitGrayIx(int start, int end, int score)
/* Figure out gray level for a blast bit score. */
{
int size = end-start;
int res;
res =  (maxShade * score + size)/(size<<1);
if (res > maxShade)
    res = maxShade;
return res;
}


static struct ensAnalysis **ensAnalysisTable = NULL;
static int ensAnalysisTableSize;
static struct ensFeature **ensTypeLists = NULL;

void ensFeatPreload()
/* Load stuff all the ensemble features tracks need. */
{
if (ensTypeLists == NULL)
    {
    struct ensFeature *fullList, *feat, *next;
    int type;

    /* Get analysis table and allocate a slot for each type. */
    ensGetAnalysisTable(&ensAnalysisTable, &ensAnalysisTableSize);
    ensTypeLists = needMem(sizeof(ensTypeLists[0]) * ensAnalysisTableSize);

    /* Get features list and divide it into sub-lists based on
     * type analysis type. */
    fullList = ensFeaturesInBacRange(seqName, winStart, winEnd);
    for (feat = fullList; feat != NULL; feat = next)
	{
	next = feat->next;
	type = feat->type;
	/* Check range and report error, but don't crash. */
	if (type < 0 || type >= ensAnalysisTableSize)
	    {
	    static boolean reported = FALSE;
	    if (!reported)
		{
		warn("Type %d out of range, skipping\n", type);
		reported = TRUE;
		}
	    ensFreeFeature(&feat);
	    }
	else
	    {
	    struct ensFeature **typeList = &ensTypeLists[type];
	    slAddHead(typeList, feat);
	    }
	}
    }
}

void ensFeatFreeRemaining()
/* Free features that aren't individually freed. */
{
if (ensTypeLists != NULL)
    {
    int i;
    for (i=0; i<ensAnalysisTableSize; ++i)
	{
	ensFreeFeatureList(&ensTypeLists[i]);
	}
    freez(&ensTypeLists);
    }
}

void sortByGray(struct trackGroup *tg)
/* Sort linked features by grayIx. */
{
if (tg->visibility == tvDense)
    slSort(&tg->items, cmpLfWhiteToBlack);
else
    slSort(&tg->items, cmpLfBlackToWhite);
}

void ensFeatLoad(struct trackGroup *tg)
/* Load up info about features. */
{
int type = tg->customInt;
ensFeatPreload();
tg->items = clusterEnsFeatures(&ensTypeLists[type]);
sortByGray(tg);
}

void ensFeatFree(struct trackGroup *tg)
/* Free up info about features. */
{
ensFeatFreeRemaining();
freeLinkedFeaturesItems(tg);
}

struct trackGroup *ensFeatTg(int type)
/* Return track group filled in for generic EnsEMBL features. */
{
struct trackGroup *tg = NULL;
AllocVar(tg);
tg->loadItems = ensFeatLoad;
tg->freeItems = ensFeatFree;
tg->drawItems = linkedFeaturesDraw;
tg->ignoresColor = TRUE;
tg->itemName = linkedFeaturesName;
tg->mapItemName = linkedFeaturesName;
tg->totalHeight = tgUsualHeight;
tg->customInt = type;
return tg;
}

struct trackGroup *ensProtTg()
/* Return track group for Swiss Protein hits. */
{
struct trackGroup *tg = ensFeatTg(eatProt);
tg->mapName = "ensProt";
tg->visibility = tvHide;
tg->longLabel = "EnsEMBL Protein Homologies (BLASTP on SwissProt and trEMBL)";
tg->shortLabel = "EnsEMBL Protein";
return tg;
}

struct trackGroup *ensPfamTg()
/* Return track group for protein family hits. */
{
struct trackGroup *tg = ensFeatTg(eatPfam);
tg->mapName = "ensPfam";
tg->visibility = tvHide;
tg->longLabel = "EnsEMBL Protein Family Homologies (hmmpfam on PfamFrag)";
tg->shortLabel = "EnsEMBL Pfam";
return tg;
}

struct trackGroup *ensMrnaTg()
/* Return track group for Vertebrate mRNA hits. */
{
struct trackGroup *tg = ensFeatTg(eatFullMrna);
tg->mapName = "ensMrna";
tg->visibility = tvHide;
tg->longLabel = "EnsEMBL mRNA Homologies (TBLASTN on Vertebrate mRNA)";
tg->shortLabel = "EnsEMBL mRNA";
return tg;
}

struct trackGroup *ensEstTg()
/* Return track group for EST hits. */
{
struct trackGroup *tg = ensFeatTg(eatEst);
tg->mapName = "ensEst";
tg->visibility = tvHide;
tg->longLabel = "EnsEMBL EST Homologies (TBLASTN on dbEST)";
tg->shortLabel = "EnsEMBL EST";
return tg;
}

struct trackGroup *ensGenscanTg()
/* Return track group for EST hits. */
{
struct trackGroup *tg = ensFeatTg(eatGenscan);
tg->mapName = "ensGenscan";
tg->visibility = tvHide;
tg->longLabel = "EnsEMBL Predicted Exons (Genscan)";
tg->shortLabel = "EnsEMBL Genscan";
return tg;
}

struct hgNest *mustFindBelow(struct hgNest *nest, HGID id)
/* Return matching element in nest or squawk and die. */
{
struct hgNest *match;
if ((match = findBelow(nest, id)) == NULL)
    errAbort("couldn't findBelow %lu", id);
return match;
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
tg->totalHeight = tgUsualHeight;
return tg;
}

void loadMrnaAli(struct trackGroup *tg)
/* Load up rnas from table into trackGroup items. */
{
char *table = tg->customPt;
char query[256];
struct sqlConnection *conn = hgAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct hgBac *bac = hgGetBac(seqName);
struct linkedFeatures *lfList = NULL, *lf;

sprintf(query, "select * from %s where tStartBac=%u and tEndPos>%d and tStartPos<%d",
    table, bac->id, winStart, winEnd);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct simpleFeature *sfList = NULL, *sf;
    struct mrnaAli *ma = mrnaAliLoad(row);
    if (ma->hasIntrons || ma->score >= 200)
	{
	unsigned *starts = ma->tBlockStarts;
	unsigned *sizes = ma->blockSizes;
	int i, blockCount = ma->blockCount;
	int grayIx = mrnaGrayIx(ma->qStart, ma->qEnd, ma->score);

	AllocVar(lf);
	lf->grayIx = grayIx;
	strncpy(lf->name, ma->qAcc, sizeof(lf->name));
	lf->orientation = ma->orientation;
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
	mrnaAliFree(&ma);
	}
    }
slReverse(&lfList);
tg->items = lfList;
sqlFreeResult(&sr);
hgFreeConn(&conn);
sortByGray(tg);
}

struct trackGroup *allEstTg()
/* Make track group of all EST (and mRNA) hits */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgEstNoI";
tg->visibility = tvHide;
tg->longLabel = "EST Hits Lacking Introns (PatSpace on Human ESTs)";
tg->shortLabel = "ESTs no Introns";
tg->customPt = "noIntronEstAli";
tg->loadItems = loadMrnaAli;
return tg;
}

struct trackGroup *intronEstTg()
/* Make track group of EST hits with introns. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgEstI";
tg->visibility = tvDense;
tg->longLabel = "ESTs with Introns (PatSpace on Human ESTs)";
tg->shortLabel = "ESTs w/ Introns";
tg->customPt = "estAli";
tg->loadItems = loadMrnaAli;
return tg;
}

struct trackGroup *fullMrnaTg()
/* Make track group of full length mRNAs. */
{
struct trackGroup *tg = linkedFeaturesTg();
tg->mapName = "hgMrna";
tg->visibility = tvDense;
tg->longLabel = "Full Length mRNAs (PatSpace on Human mRNAs)";
tg->shortLabel = "full mRNAs";
tg->customPt = "mrnaAli";
tg->loadItems = loadMrnaAli;
return tg;
}

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
struct hgBac *bac = hgGetBac(seqName);
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

void genieLoad(struct trackGroup *tg)
/* Get genie predictions and turn them into linked features. */
{
struct hgBac *bac = hgGetBac(seqName);
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


struct linkedFeatures *loadBlastx(char *blockTable)
/* Load blastx hits and connect them. */
{
struct hgBac *bac = hgGetBac(seqName);
struct hgNest *bacNest = bac->nest;
struct hgNest *conNest;
struct dyString *query = newDyString(1024);
struct sqlConnection *conn = hgAllocConn();
struct sqlResult *sr = NULL;
char **row;
boolean firstTime = TRUE;
boolean anyIntersection = FALSE;
struct linkedFeatures *lfList = NULL, *lf = NULL;
struct simpleFeature *sf;
HGID lastParentId = 0;

dyStringPrintf(query,
    "select parent,target,tstart,tend,query_acc,pscore from %s "
    "where %s.target in (", blockTable, blockTable);
dyStringPrintf(query, "%lu,", bacNest->id);
for (conNest = bacNest->children; conNest != NULL; conNest = conNest->next)
    {
    if (rangeIntersection(winStart,winEnd,conNest->offset,conNest->offset+conNest->size) > 0)
	{
	anyIntersection = TRUE;
	if (firstTime)
	    firstTime = FALSE;
	else
	    dyStringAppend(query, ",");
	dyStringPrintf(query, "%lu", conNest->id);
	}
    }
if (!anyIntersection)
    return NULL;
dyStringAppend(query, ")");

sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    HGID parentId = sqlUnsigned(row[0]);
    HGID targetId = sqlUnsigned(row[1]);
    int tStart = sqlUnsigned(row[2]);
    int tEnd = sqlUnsigned(row[3]);

    if (lastParentId != parentId)
	{
	if (lastParentId != 0)
	    finishLf(lf);
	lastParentId = parentId;
	AllocVar(lf);
	slAddHead(&lfList, lf);
	strncpy(lf->name, row[4], sizeof(lf->name));
	}
    AllocVar(sf);
    conNest = mustFindBelow(bacNest, targetId);
    tStart = hgOffset(conNest, tStart, bacNest);
    tEnd = hgOffset(conNest, tEnd, bacNest);
    sf->start = tStart;
    sf->end = tEnd;
    sf->grayIx = percentGrayIx(sqlUnsigned(row[5]));
    slAddHead(&lf->components, sf);
    }
if (lastParentId != 0)
    finishLf(lf);

sqlFreeResult(&sr);
hgFreeConn(&conn);
slReverse(&lfList);
return lfList;
}

void hgapBlastxLoad(struct trackGroup *tg)
/* Load up items about features. */
{
tg->items = loadBlastx(tg->customPt);
sortByGray(tg);
}


struct trackGroup *hgapBlastxTg()
/* Return track group for HGAP blastx. */
{
static struct trackGroup *tg = NULL;
static struct trackGroup trackGroup;

if (tg == NULL)
    {
    tg = &trackGroup;
    tg->mapName = "hgBlastx";
    tg->visibility = tvDense;
    tg->longLabel = "Translated Hits to Non-Redundant Protein Database (BLASTX on NR)";
    tg->shortLabel = "BLASTX Hits";
    tg->loadItems = hgapBlastxLoad;
    tg->freeItems = freeLinkedFeaturesItems;
    tg->drawItems = linkedFeaturesDraw;
    tg->ignoresColor = TRUE;
    tg->itemName = linkedFeaturesName;
    tg->mapItemName = linkedFeaturesName;
    tg->totalHeight = tgUsualHeight;
    tg->customPt = "blastx_block";
    }
return tg;
}

struct trackGroup *hgapBlastx14Tg()
/* Return track group for HGAP blastx. */
{
static struct trackGroup *tg = NULL;
static struct trackGroup trackGroup;

if (tg == NULL)
    {
    tg = &trackGroup;
    tg->mapName = "hgBlastx14";
    tg->visibility = tvHide;
    tg->longLabel = "Translated Hits to Non-Redundant Protein Database (BLASTX on NR)";
    tg->shortLabel = "BLASTX-14 Hits";
    tg->loadItems = hgapBlastxLoad;
    tg->freeItems = freeLinkedFeaturesItems;
    tg->drawItems = linkedFeaturesDraw;
    tg->ignoresColor = TRUE;
    tg->itemName = linkedFeaturesName;
    tg->mapItemName = linkedFeaturesName;
    tg->totalHeight = tgUsualHeight;
    tg->customPt = "blastx14_block";
    }
return tg;
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
	pixHeight += group->totalHeight(group);
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
    if (group->visibility != tvHide)
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
	switch (group->visibility)
	    {
	    case tvHide:
		break;	/* Do nothing; */
	    case tvFull:
		if (withCenterLabels)
		    y += fontHeight;
		for (item = group->items; item != NULL; item = item->next)
		    {
		    char *name = group->itemName(group, item);
		    mgTextCentered(mg, border, y, inWid, group->lineHeight, group->ixColor, font, name);
		    y += group->lineHeight;
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
    mgSetClip(mg, xOff, border, insideWidth, pixHeight - 2*border);
    y = border;
    for (group = groupList; group != NULL; group = group->next)
        {
	if (group->visibility != tvHide)
	    {
	    mgTextCentered(mg, xOff, y+1, insideWidth, insideHeight, group->ixColor, font, group->longLabel);
	    y += fontHeight;
	    y += group->height;
	    }
        }
    }

/* Draw tracks. */
y = 1;
for (group = groupList; group != NULL; group = group->next)
    {
    if (group->visibility != tvHide)
	{
	if (withCenterLabels)
	    y += fontHeight;
	mgSetClip(mg, xOff, y, insideWidth, group->height);
	group->drawItems(group, winStart, winEnd,
	    mg, xOff, y, insideWidth, 
	    font, group->ixColor);
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
mapBoxHtc(xOff,y,insideWidth,rulerHeight, "getDna", "mixed", "Fetch DNA");

/* Make map background. */
    {
    y = border;
    for (group = groupList; group != NULL; group = group->next)
        {
	struct slList *item;
	switch (group->visibility)
	    {
	    case tvHide:
		break;	/* Do nothing; */
	    case tvFull:
		if (withCenterLabels)
		    y += fontHeight;
		for (item = group->items; item != NULL; item = item->next)
		    {
		    int height = group->lineHeight;
		    mapBoxHtc(0,y,pixWidth,height, group->mapName, 
			group->mapItemName(group, item), 
			group->itemName(group, item));
		    y += height;
		    }
		break;
	    case tvDense:
		if (withCenterLabels)
		    y += fontHeight;
		mapBoxFull(0,y,pixWidth,group->lineHeight,group);
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

char *cloneChoices[] = {
"AC000001",
"AC000005",
"AC000016",
"AC000033",
"AC002399",
"AC004256",
"AC007479",
"AC007666",
"AC007702",
"AC012191",
"AL050317",
"AB014083",
"AB023052",
"AC001644",
"AC007056",
"AC009587",
};

void writeAllClones()
{
char *outDir = "/d/biodata/human/testSets/ens16/fa.merged";
char path[512];
int i;

for (i=0; i<ArraySize(cloneChoices); ++i)
    {
    char *clone = cloneChoices[i];
    int dnaSize = ensBacBrowserLength(clone);
    struct dnaSeq *seq = ensDnaInBacRange(clone, 0, dnaSize, dnaLower);
    sprintf(path, "%s/%s.fa", outDir, clone);
    faWrite(path, clone, seq->dna, seq->size);
    freeDnaSeq(&seq);
    }
errAbort("Wrote all clones!");
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
slSafeAddHead(&tGroupList, ensContigTg());
slSafeAddHead(&tGroupList, ensProtTg());
slSafeAddHead(&tGroupList, ensPfamTg());
slSafeAddHead(&tGroupList, ensEstTg());
slSafeAddHead(&tGroupList, ensMrnaTg());
slSafeAddHead(&tGroupList, ensGenscanTg());
slSafeAddHead(&tGroupList, ensGeneTg());
slSafeAddHead(&tGroupList, genieTg());
slSafeAddHead(&tGroupList, altGraphTg());
slSafeAddHead(&tGroupList, fullMrnaTg());
slSafeAddHead(&tGroupList, intronEstTg());
slSafeAddHead(&tGroupList, allEstTg());
slSafeAddHead(&tGroupList, hgapBlastxTg());
slSafeAddHead(&tGroupList, hgapBlastx14Tg());
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
	if (group->visibility == tvFull && slCount(group->items) > 500)
	    {
	    errAbort("Sorry, there are too many items (%d) in %s to display in full mode. "
	             "Please go back and select dense or hide in the Track Control section for %s. ",
		     slCount(group->items), group->shortLabel, group->shortLabel);
	    }
	}

    }

/* Tell browser where to go when they click on image. */
printf("<FORM ACTION=\"%shgTracks\">\n\n", cgiDir());

/* Center everything from now on. */
printf("<CENTER>\n");

/* Show title . */
printf("<H2>%s</H2>", seqName);

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

/* Make clickable image and map. */
makeActiveImage(tGroupList);
fputs("Click on image to view more information on a gene or alignment. Click on ruler for DNA<BR>", stdout);

/* Display bottom control panel. */
htmlHorizontalLine();
printf("<B>Current Clone: </B>");
makeDropList("seqName", cloneChoices, ArraySize(cloneChoices), seqName);
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

void doMiddle()
/* Print the body of an html file.  This routine handles zooming and
 * scrolling. */
{
char *oldSeq;

pushCarefulMemHandler(32*1024*1024);
/* Initialize layout. */
initTl();

/* Read in input from CGI. */
oldSeq = cgiOptionalString("old");
if (oldSeq != NULL)
   calledSelf = TRUE; 
seqName = cgiString("seqName");
seqBaseCount = ensBacBrowserLength(seqName);
if (!calledSelf || !sameWord(seqName, oldSeq))
    {
    winStart = 0;
    winEnd = seqBaseCount;
    }
else
    {
    winStart = cgiInt("winStart");
    winEnd = cgiInt("winEnd");
    }
seqReverse = cgiBoolean("seqReverse");
winBaseCount = winEnd - winStart;

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
doForm();
}

int main(int argc, char *argv[])
{
htmShell("Tracks Display", doMiddle, NULL);
}
