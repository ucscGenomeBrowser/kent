/* infrastructure -- Code shared by all tracks.  Separating this out from
 * hgTracks.c allows a standalone main to make track images. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

/* NOTE: This code was imported from hgTracks.c 1.1469, May 19 2008,
 * so a lot of revision history has been obscured.  To see code history
 * from before this file was created, run this:
 * cvs ann -r 1.1469 hgTracks.c | less +Gp
 */

#include "common.h"
#include "spaceSaver.h"
#include "portable.h"
#include "bed.h"
#include "basicBed.h"
#include "psl.h"
#include "web.h"
#include "hdb.h"
#include "hgFind.h"
#include "hCommon.h"
#include "hgColors.h"
#include "trackDb.h"
#include "bedCart.h"
#include "wiggle.h"
#include "lfs.h"
#include "grp.h"
#include "chromColors.h"
#include "hgTracks.h"
#include "subText.h"
#include "cds.h"
#include "mafTrack.h"
#include "wigCommon.h"
#include "hui.h"
#include "imageV2.h"
#include "bigBed.h"
#include "htmshell.h"
#include "kxTok.h"
#include "hash.h"
#include "decorator.h"
#include "decoratorUi.h"

#ifndef GBROWSE
#include "encode.h"
#include "expRatioTracks.h"
#include "hapmapTrack.h"
#include "retroGene.h"
#include "switchGear.h"
#include "variation.h"
#include "wiki.h"
#include "wormdna.h"
#include "aliType.h"
#include "agpGap.h"
#include "cgh.h"
#include "bactigPos.h"
#include "genePred.h"
#include "genePredReader.h"
#include "gencodeTracks.h"
#include "isochores.h"
#include "spDb.h"
#include "simpleRepeat.h"
#include "cpgIsland.h"
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
#include "stsMapMouseNew.h"
#include "stsMapRat.h"
#include "snpMap.h"
#include "recombRate.h"
#include "recombRateRat.h"
#include "recombRateMouse.h"
#include "chr18deletions.h"
#include "mouseOrtho.h"
#include "humanParalog.h"
#include "synteny100000.h"
#include "mouseSyn.h"
#include "mouseSynWhd.h"
#include "ensFace.h"
#include "ensPhusionBlast.h"
#include "knownMore.h"
#include "customTrack.h"
#include "customFactory.h"
#include "pslWScore.h"
#include "mcnBreakpoints.h"
#include "altGraph.h"
#include "altGraphX.h"
#include "geneGraph.h"
#include "genMapDb.h"
#include "genomicSuperDups.h"
#include "celeraDupPositive.h"
#include "celeraCoverage.h"
#include "simpleNucDiff.h"
#include "tfbsCons.h"
#include "tfbsConsSites.h"
#include "itemAttr.h"
#include "encode.h"
#include "variation.h"
#include "estOrientInfo.h"
#include "versionInfo.h"
#include "retroGene.h"
#include "switchGear.h"
#include "dless.h"
#include "hgConfig.h"
#include "gv.h"
#include "gvUi.h"
#include "protVar.h"
#include "oreganno.h"
#include "oregannoUi.h"
#include "pgSnp.h"
#include "bedDetail.h"
#include "bed12Source.h"
#include "dbRIP.h"
#include "wikiLink.h"
#include "wikiTrack.h"
#include "dnaMotif.h"
#include "hapmapTrack.h"
#include "omicia.h"
#include "nonCodingUi.h"
#include "transMapTracks.h"
#include "retroTracks.h"
#include "pcrResult.h"
#include "variome.h"
#include "pubsTracks.h"
#endif /* GBROWSE */

#ifdef LOWELAB
#include "loweLabTracks.h"
#include "rnaPLFoldTrack.h"
#endif /* LOWELAB */
#ifdef LOWELAB_WIKI
#include "wiki.h"
#endif /* LOWELAB_WIKI */

#include "trackVersion.h"
#include "genbank.h"
#include "bedTabix.h"
#include "knetUdc.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "bigWarn.h"
#include "quickLift.h"
#include "liftOver.h"
#include "bedMethyl.h"

#define CHROM_COLORS 26

/* Declare our color gradients and the the number of colors in them */
Color shadesOfGreen[EXPR_DATA_SHADES];
Color shadesOfRed[EXPR_DATA_SHADES];
Color shadesOfBlue[EXPR_DATA_SHADES];
Color shadesOfYellow[EXPR_DATA_SHADES];
Color shadesOfGreenOnWhite[EXPR_DATA_SHADES];
Color shadesOfRedOnWhite[EXPR_DATA_SHADES];
Color shadesOfBlueOnWhite[EXPR_DATA_SHADES];
Color shadesOfYellowOnWhite[EXPR_DATA_SHADES];
Color shadesOfRedOnYellow[EXPR_DATA_SHADES];
Color shadesOfBlueOnYellow[EXPR_DATA_SHADES];
Color orangeColor = 0;
Color brickColor = 0;
Color blueColor = 0;
Color darkBlueColor = 0;
Color greenColor = 0;
Color darkGreenColor = 0;
boolean exprBedColorsMade = FALSE; /* Have the shades of Green, Red, and Blue been allocated? */
int maxRGBShade = EXPR_DATA_SHADES - 1;

Color scafColor[SCAF_COLORS+1];
/* declare colors for scaffold coloring, +1 for unused scaffold 0 color */

Color chromColor[CHROM_COLORS+1];
/* Declare colors for chromosome coloring, +1 for unused chrom 0 color */

/* Have the 3 shades of 8 chromosome colors been allocated? */
boolean chromosomeColorsMade = FALSE;
boolean doPliColors = FALSE;
/* have the 10 scaffold colors been allocated */
static boolean scafColorsMade = FALSE;

int maxItemsInFullTrack = 1000;  /* Maximum number of items displayed in full */
int maxItemsToUseOverflowDefault = 10000; /* # of items to allow overflow mode*/

/* These variables persist from one incarnation of this program to the
 * next - living mostly in the cart. */

// multi-window variables global to hgTracks
struct window *windows = NULL;  // list of windows in image
struct window *currentWindow = NULL;
bool trackLoadingInProgress;  // flag to delay ss layout until all windows are ready.
int fullInsideX;      // full-image insideX
int fullInsideWidth;  // full-image insideWidth

struct virtRegion *virtRegionList = NULL;     // list of regions in virtual chrom
struct virtChromRegionPos *virtChrom = NULL;  // virtual chrom array of positions and regions
int virtRegionCount = 0;  // number of regions in virtual chromosome
long virtSeqBaseCount = 0;       // number of bases in virtual chromosome
long virtWinBaseCount = 0;       // number of bases in virtual chrom (end - start)
long virtWinStart = 0;  // start of virtual window in bases
long virtWinEnd = 0;    //   end of virtual window in bases
long defaultVirtWinStart = 0;  // default start of virtual window in bases
long defaultVirtWinEnd = 0;    // default end   of virtual window in bases
//char *virtPosition = NULL;          /* Name of virtual position. TODO REMOVE? */
char *virtChromName = NULL;         /* Name of virtual chrom */
boolean virtMode = FALSE;           /* Are we in virtual chrom mode? */
boolean virtChromChanged = FALSE;    /* Has the virtChrom changed? */
boolean emAltHighlight = FALSE;     /* Highlight alternativing regions in virt view? */
int emPadding = 6;                  /* # bases padding for exon-mostly regions */
int gmPadding = 6;                  /* # bases padding for gene-mostly regions */
char *emGeneTable = NULL;           /* Gene table to use for exon mostly */
struct track *emGeneTrack = NULL;   /* Track for gene table for exon mostly */
struct rgbColor vertWindowSeparatorColor = { 255, 220, 220, 255};  // light red
char *multiRegionsBedUrl = "";     /* URL to Multi-window bed regions file */

// demo2
int demo2NumWindows = 70;
int demo2WindowSize = 200;
int demo2StepSize = 200;

// singleTrans (single transcript)
char *singleTransId = "uc001uub.1";

// singleAltHaplos (single haplotype)
char *singleAltHaploId = "chr6_cox_hap2";

char *virtModeType = "default";  /* virtual chrom mode type */
char *lastVirtModeType = "default";
char *virtModeShortDescr = "";   /* short description of virt mode */
char *virtModeExtraState = "";   /* Other settings that affect the virtMode state such as padding or some parameter */
char *lastVirtModeExtraState = "";
struct cart *lastDbPosCart = NULL;   /* store settings for use in lastDbPos and hgTracks.js setupHistory */

char *organism;                 /* Name of organism we're working on. */
char *database;			/* Name of database we're using. */
char *chromName;		/* Name of chromosome sequence . */
char *displayChromName;		/* Name of chromosome sequence to display . */
int winStart;                   /* Start of window in sequence. */
int winEnd;                     /* End of window in sequence. */
char *position = NULL;          /* Name of position. */

int insideX;			/* Start of area to draw track in in pixels. */
int insideWidth;		/* Width of area to draw tracks in in pixels. */
int leftLabelX;                 /* Start of area to draw left labels on. */
int leftLabelWidth;             /* Width of area to draw left labels on. */
float basesPerPixel = 0;       /* bases covered by a pixel; a measure of zoom */
boolean zoomedToBaseLevel;      /* TRUE if zoomed so we can draw bases. */
boolean zoomedToCodonNumberLevel; /* TRUE if zoomed so we can print codons and exon number text in genePreds*/
boolean zoomedToCodonLevel; /* TRUE if zoomed so we can print codons text in genePreds*/
boolean zoomedToCdsColorLevel; /* TRUE if zoomed so we can color each codon*/

boolean withLeftLabels = TRUE;		/* Display left labels? */
boolean withIndividualLabels = TRUE;    /* print labels on item-by-item basis (false to skip) */
boolean withCenterLabels = TRUE;	/* Display center labels? */
boolean withGuidelines = TRUE;		/* Display guidelines? */
boolean withNextExonArrows = FALSE;	/* Display next exon navigation buttons near center labels? */
boolean withExonNumbers = FALSE;	/* Display exon and intron numbers in mouseOver instead of item name */
boolean revCmplDisp = FALSE;          /* reverse-complement display */

boolean measureTiming = FALSE;	/* DON'T EDIT THIS -- use CGI param "&measureTiming=." . */
struct track *trackList = NULL;    /* List of all tracks. */
struct cart *cart;	/* The cart where we keep persistent variables. */

int seqBaseCount;	/* Number of bases in sequence. */
int winBaseCount;	/* Number of bases in window. */

int maxShade = 9;	/* Highest shade in a color gradient. */
Color shadesOfGray[10+1];	/* 10 shades of gray from white to black
                                 * Red is put at end to alert overflow. */
Color shadesOfBrown[10+1];	/* 10 shades of brown from tan to tar. */
struct rgbColor brownColor = {100, 50, 0, 255};
struct rgbColor tanColor = {255, 240, 200, 255};
struct rgbColor guidelineColor = {220, 220, 255, 255};
struct rgbColor multiRegionAltColor = {235, 235, 255, 255};
struct rgbColor undefinedYellowColor = {240,240,180, 255};

Color shadesOfSea[10+1];       /* Ten sea shades. */
struct rgbColor darkSeaColor = {0, 60, 120, 255};
struct rgbColor lightSeaColor = {200, 220, 255, 255};

struct hash *hgFindMatches; /* The matches found by hgFind that should be highlighted. */

struct trackLayout tl;

void initTl()
/* Initialize layout around small font and a picture about 600 pixels
 * wide. */
{
trackLayoutInit(&tl, cart);

}

static boolean isTooLightForTextOnWhite(struct hvGfx *hvg, Color color)
/* Return TRUE if text in this color would probably be invisible on a white background. */
{
struct rgbColor rgbColor =  hvGfxColorIxToRgb(hvg, color);
int colorTotal = rgbColor.r + 2*rgbColor.g + rgbColor.b;
return colorTotal >= 512;
}

Color darkerColor(struct hvGfx *hvg, Color color)
/* Get darker shade of a color - half way between this color and black */
{
struct rgbColor rgbColor =  hvGfxColorIxToRgb(hvg, color);
rgbColor.r = ((int)rgbColor.r)/2;
rgbColor.g = ((int)rgbColor.g)/2;
rgbColor.b = ((int)rgbColor.b)/2;
return hvGfxFindAlphaColorIx(hvg, rgbColor.r, rgbColor.g, rgbColor.b, rgbColor.a);
}

Color somewhatDarkerColor(struct hvGfx *hvg, Color color)
/* Get a somewhat darker shade of a color - 1/3 of the way towards black. */
{
struct rgbColor rgbColor =  hvGfxColorIxToRgb(hvg, color);
rgbColor.r = (2*(int)rgbColor.r)/3;
rgbColor.g = (2*(int)rgbColor.g)/3;
rgbColor.b = (2*(int)rgbColor.b)/3;
return hvGfxFindAlphaColorIx(hvg, rgbColor.r, rgbColor.g, rgbColor.b, rgbColor.a);
}

Color slightlyDarkerColor(struct hvGfx *hvg, Color color)
/* Get a slightly darker shade of a color - 1/4 of the way towards black. */
{
struct rgbColor rgbColor =  hvGfxColorIxToRgb(hvg, color);
rgbColor.r = (9*(int)rgbColor.r)/10;
rgbColor.g = (9*(int)rgbColor.g)/10;
rgbColor.b = (9*(int)rgbColor.b)/10;
return hvGfxFindAlphaColorIx(hvg, rgbColor.r, rgbColor.g, rgbColor.b, rgbColor.a);
}

Color lighterColor(struct hvGfx *hvg, Color color)
/* Get lighter shade of a color - half way between this color and white */
{
struct rgbColor rgbColor =  hvGfxColorIxToRgb(hvg, color);
rgbColor.r = (rgbColor.r+255)/2;
rgbColor.g = (rgbColor.g+255)/2;
rgbColor.b = (rgbColor.b+255)/2;
return hvGfxFindAlphaColorIx(hvg, rgbColor.r, rgbColor.g, rgbColor.b, rgbColor.a);
}

Color somewhatLighterColor(struct hvGfx *hvg, Color color)
/* Get a somewhat lighter shade of a color - 1/3 of the way towards white. */
{
struct rgbColor rgbColor =  hvGfxColorIxToRgb(hvg, color);
rgbColor.r = (2*rgbColor.r+255)/3;
rgbColor.g = (2*rgbColor.g+255)/3;
rgbColor.b = (2*rgbColor.b+255)/3;
return hvGfxFindAlphaColorIx(hvg, rgbColor.r, rgbColor.g, rgbColor.b, rgbColor.a);
}

boolean trackIsCompositeWithSubtracks(struct track *track)
/* Temporary function until all composite tracks point to their own children */
{
return (tdbIsComposite(track->tdb) && track->subtracks != NULL);
}

Color slightlyLighterColor(struct hvGfx *hvg, Color color)
/* Get slightly lighter shade of a color - closer to gray actually  */
{
struct rgbColor rgbColor =  hvGfxColorIxToRgb(hvg, color);
rgbColor.r = (rgbColor.r+128)/2;
rgbColor.g = (rgbColor.g+128)/2;
rgbColor.b = (rgbColor.b+128)/2;
return hvGfxFindAlphaColorIx(hvg, rgbColor.r, rgbColor.g, rgbColor.b, rgbColor.a);
}

boolean winTooBigDoWiggle(struct cart *cart, struct track *tg)
/* return true if we wiggle because the window size exceeds a certain threshold? */
{
boolean doWiggle = FALSE;
char *setting = trackDbSetting(tg->tdb, "maxWindowCoverage" );
if (setting)
    {
    unsigned size = sqlUnsigned(setting);
    if ((size > 0) && ((winEnd - winStart) > size))
        doWiggle = TRUE;
    }
return doWiggle;
}

boolean checkIfWiggling(struct cart *cart, struct track *tg)
/* Check to see if a track should be drawing as a wiggle. */
{
if (tg->limitWiggle)
    return TRUE;

boolean doWiggle = cartOrTdbBoolean(cart, tg->tdb, "doWiggle" , FALSE);

if (!doWiggle)
    doWiggle = winTooBigDoWiggle(cart, tg);

if (doWiggle && isEmpty(tg->networkErrMsg))
    {
    tg->drawLeftLabels = wigLeftLabels;
    tg->colorShades = shadesOfGray;
    tg->mapsSelf = TRUE;
    }

return doWiggle;
}

struct sameItemNode
/* sameItem node */
    {
    struct sameItemNode *next; /* next node */
    struct window *window;     /* in which window - can use to detect duplicate keys */
    void *item;
    struct spaceSaver *ss;
    bool done;                 /* have we (reversed the list and) processed it yet? */
    };


struct spaceSaver *findSpaceSaver(struct track *tg, enum trackVisibility vis)
/* Find SpaceSaver in list. Return spaceSaver found or NULL. */
{
struct spaceSaver *ss = NULL;
// tg->ss is actually a list of spaceSavers with different viz. with newest on top
// We needed to keep the old ss around to trigger proper viz changes.
// Sometimes vis changes because of limitedVis.
// Sometimes the parent composite track's vis is being used to override subtrack vis.
// Since it is not easy to be certain a vis will not be used again, we cannot free old spaceSavers.
for(ss = tg->ss; ss; ss = ss->next)
    {
    if (ss->vis == vis)
	{
	return ss;
	}
    }
return NULL;
}


int packCountRowsOverflow(struct track *tg, int maxCount,
                          boolean withLabels, boolean allowOverflow, enum trackVisibility vis)
/* Return packed height. */
{

// allowOverflow is currently ONLY used by xenoMrna and est tracks.
//  When true,  the extra rows are kept, printed at the bottom in dense and Last Row: overlow count appears at bottom of leftLabel area.
//  When false, the extra rows are tossed, the count seems to equal overflow limit + 2, and limitVisibility lowers vis and retries.

// do not calculate if still loading all windows
if (trackLoadingInProgress) // we pack after all windows are loaded.
    {
    // do not set ss yet
    return 0;  // height of 0 triggers unsetting limitedVis since our data is not all loaded yet and it will get set later.
    }

// do not re-calculate if not needed
if (tg->ss)
    {
    if (tg->ss->window != currentWindow)
	{
	// altGraphX creates its own specialized spaceSavers for full vis.
	// and the routines (hgTracks/altGraphXTrack.c and hg/lib/altGraphX.c) do not set ss->window or ss->vis.
	if (!sameString(tg->tdb->type,"altGraphX"))
	    errAbort("unexpected current window %lu, expected %lu", (unsigned long) currentWindow, (unsigned long) tg->ss->window);
	}
    struct spaceSaver *ss = findSpaceSaver(tg, vis);
    if (ss)
	return ss->rowCount;
    // Falls thru here if a new visibility is needed, such as full changing to pack after limitVisibility.
    // This will usually be when it is the first window and it is requesting a new vis.
    }

if (currentWindow != windows)  // if not first window
    {
    errAbort("unexpected current window %lu, expected first window %lu", (unsigned long) currentWindow, (unsigned long) windows);
    }

// If we get here, currentWindow should be first window i.e. windows var.

if (hasDecorators(tg))
{
    packDecorators(tg);  // be nice to avoid doing this for every pass through packCountRowsOverflow
            // reason being we only ever pack decorations one way, which only depends on the decoration style
            // (and user selections), but not the vis mode - vis just changes height.
}

struct slList *item;
MgFont *font = tl.font;
int extraWidth = tl.mWidth * 2;
long long start, end;  // TODO usually screen pixels, so plain long should work.
struct window *w;
struct track *tgSave = tg;

// find out which items are the same,
// and belong on the same line with (probably one) label:
struct hash *sameItem = newHash(10);
// TODO estimate this as log base 2 # items in all windows
//struct hash *itemDone = newHash(10);  // TODO I do not need this yet in this version if no dupes.
for(w=windows,tg=tgSave; w; w=w->next,tg=tg->nextWindow)
    {
    // Have to init these here
    // save old spaceSaver until callback on another window later
    struct spaceSaver *ssOld = tg->ss;
    tg->ss = spaceSaverNew(0, fullInsideWidth, maxCount);  // actual params do not matter, just using ss->nodeList.
    tg->ss->next = ssOld;
    boolean useItemNameAsKey = isTypeUseItemNameAsKey(tg);
    boolean useMapItemNameAsKey = isTypeUseMapItemNameAsKey(tg);
    char *chrom = w->chromName;
    for (item = tg->items; item != NULL; item = item->next)
	{
	int baseStart = tg->itemStart(tg, item);
	int baseEnd = tg->itemEnd(tg, item);
	if (baseStart < w->winEnd && baseEnd > w->winStart)
	    { // it intersects the window
	    char *mapItemName = tg->mapItemName(tg, item);
	    char key[1024];
	    // TODO I am making the key based on chrom, so no items will be aligned on the same line
	    // that are in different chromosomes.  If the itemName is unique per chromosome or in other ways
	    // then itemName alone could be the key.
	    // TODO we know that for some tracks there is great duplication, names are not unique,
	    // even this key of position+name will not be unique enough.
	    // But fixing that may require creating a new function that converts the item into a string
	    // and then use that to prove uniqueness, or as near as can be had.
	    //
	    // For now, this should be good enough to illustrate the basic behavior we want to see.
	    if (useItemNameAsKey)
		safef(key, sizeof key, "%s",  tg->itemName(tg, item));
	    else if (useMapItemNameAsKey)
		safef(key, sizeof key, "%s", mapItemName);
            else
		safef(key, sizeof key, "%s:%d-%d %s", chrom, baseStart, baseEnd, mapItemName);
	    struct hashEl *hel = hashLookup(sameItem, key);
	    struct sameItemNode *sin;
	    if (hel)
		{
		sin = (struct sameItemNode *)hel->val;
		if (sin->window == w)
		    {
		    // dupe found for key, all windows should have the same number of dupes
		    //char *itemName = tg->itemName(tg, item);
		    //char *itemDataName = getItemDataName(tg, itemName);
		    //char *mapItemName = tg->mapItemName(tg, item);
		    //warn("duplicate keys in same window.\n key=[%s] itemDataName=%s mapItemName=%s w=%lu sin->window=%lu",  // TODO
			//key, itemDataName, mapItemName, (unsigned long) w, (unsigned long) sin->window);
		    }
		}
	    else
		{
		hashAdd(sameItem, key, NULL);
		hel = hashLookup(sameItem, key);
		}
	    AllocVar(sin);
	    sin->window = w;
	    sin->item = item;
	    sin->ss = tg->ss;
	    slAddHead(&hel->val, sin);
	    }
	}
    }

// do spaceSaver layout
struct spaceSaver *ss;
ss = spaceSaverNew(0, fullInsideWidth, maxCount);
ss->vis = vis;
for(w=windows,tg=tgSave; w; w=w->next,tg=tg->nextWindow)
    {
    // save these values so we can detect vis and window changes
    tg->ss->vis = vis;
    tg->ss->window = w;
    char *chrom = w->chromName;
    // int winOffset = w->insideX - fullInsideX;  // no longer needed
    double scale = (double)w->insideWidth/(w->winEnd - w->winStart);
    boolean useItemNameAsKey = isTypeUseItemNameAsKey(tg);
    boolean useMapItemNameAsKey = isTypeUseMapItemNameAsKey(tg);
    for (item = tg->items; item != NULL; item = item->next)
	{
	// TODO match items from different windows by using item start end and name in hash?
	int baseStart = tg->itemStart(tg, item);
	int baseEnd = tg->itemEnd(tg, item);
	if (baseStart < w->winEnd && baseEnd > w->winStart)
	    { // it intersects the window

	    char *mapItemName = tg->mapItemName(tg, item);
	    char key[1024];
	    // TODO see key caveats above
	    // For now, this should be good enough to illustrate the basic behavior we want to see.
	    if (useItemNameAsKey)
		safef(key, sizeof key, "%s",  tg->itemName(tg, item));
	    else if (useMapItemNameAsKey)
		safef(key, sizeof key, "%s",  mapItemName);
	    else
		safef(key, sizeof key, "%s:%d-%d %s", chrom, baseStart, baseEnd, mapItemName);
	    struct hashEl *hel = hashLookup(sameItem, key);
	    if (!hel)
		{
		if (tg->networkErrMsg) // probably timed-out before thread finished adding more items.
		    break;
		errAbort("unexpected error: lookup of key [%s] in sameItem hash failed.", key);
		}
	    struct sameItemNode *sin = (struct sameItemNode *)hel->val;
	    if (!sin)
		errAbort("sin empty (hel->val)!");
	    if (!sin->done)
		{ // still needs to be done
		slReverse(&hel->val);
		sin = (struct sameItemNode *)hel->val;
		}

	    bool noLabel = FALSE;
	    struct window *firstWin = sin->window;  // first window
	    struct window *lastWin = NULL;
	    bool foundWork = FALSE;

	    struct spaceRange *rangeList=NULL, *range;
	    struct spaceNode *nodeList=NULL, *node;
            int rangeWidth = 0; // width in pixels of all ranges
	    int leftLabelSize = 0;
	    for(; sin; sin=sin->next)
		{

                if (sin->window != firstWin && !foundWork)
		    break;  // If we have not found work within the first window, we never will.

		if (sin->done)  // If the node has already been done by a previous pass then skip
		    continue;

		if (lastWin == sin->window)  // If we already did one in this window, skip to the next window
		    continue;
		
		sin->done = TRUE;
		foundWork = TRUE;
		lastWin = sin->window;

		int winOffset = sin->window->insideX - fullInsideX;
		struct slList *item = sin->item;
		int baseStart = tg->itemStart(tg, item);
		int baseEnd = tg->itemEnd(tg, item);
		struct window *w = sin->window;

		// convert bases to pixels
		if (baseStart <= w->winStart)
		    start = 0;
		else
		    start = round((double)(baseStart - w->winStart)*scale);

int expandPack = 0;  // deactivating item boundary extention for now - working on that next
                if (hasDecorators(tg) && expandPack)
                // extend item start with overlays to put the label in the right place
                    {
                    int overlayStart;
                    char decoratorKey[2048];
                    safef(decoratorKey, sizeof decoratorKey, "%s:%d-%d:%s", chrom, baseStart, baseEnd, mapItemName);
                    if (decoratorGroupGetOverlayExtent(tg, decoratorKey, &overlayStart, NULL))
                        {
                        if (overlayStart < start)
                            start = overlayStart;
                        }
                    }

		if (!tg->drawLabelInBox && !tg->drawName && withLabels && (!noLabel))
                // add item label
		    {
		    leftLabelSize = mgFontStringWidth(font,
					       tg->itemName(tg, item)) + extraWidth;
		    if (start - leftLabelSize + winOffset < 0)
			leftLabelSize = start + winOffset;
		    start -= leftLabelSize;
		    }

                if (hasDecorators(tg) && expandPack)
                // expand item start for packing if other decorations push it past the left label edge, for packing
                    {
                    int decoratedStart;
                    char decoratorKey[2048];
                    safef(decoratorKey, sizeof decoratorKey, "%s:%d-%d:%s", chrom, baseStart, baseEnd, mapItemName);
                    if (decoratorGroupGetExtent(tg, decoratorKey, &decoratedStart, NULL))
                        {
                        if (decoratedStart < start)
                            start = decoratedStart;
                        }
                    }

		if (baseEnd >= w->winEnd)
		    end = w->insideWidth;
		else
		    end = round((baseEnd - w->winStart)*scale);

                if (hasDecorators(tg) && expandPack)
                // extend item ends via overlay here
                    {
                    int overlayEnd;
                    char decoratorKey[2048];
                    safef(decoratorKey, sizeof decoratorKey, "%s:%d-%d:%s", chrom, baseStart, baseEnd, mapItemName);
                    if (decoratorGroupGetOverlayExtent(tg, decoratorKey, NULL, &overlayEnd))
                        {
                        if (overlayEnd > end)
                            end = overlayEnd;
                        }
                    }

		if (tg->itemRightPixels && withLabels)
                // draw anything that's supposed to be off the right edge (like right labels)
		    {
		    end += tg->itemRightPixels(tg, item);
		    if (end > w->insideWidth)
			end = w->insideWidth;
		    }

                if (hasDecorators(tg) && expandPack)
                // and extend item end via general decoration extension here, for packing
                    {
                    int decoratedEnd;
                    char decoratorKey[2048];
                    safef(decoratorKey, sizeof decoratorKey, "%s:%d-%d:%s", chrom, baseStart, baseEnd, mapItemName);
                    if (decoratorGroupGetExtent(tg, decoratorKey, NULL, &decoratedEnd))
                        {
                        if (decoratedEnd > end)
                            end = decoratedEnd;
                        }
                    }


		AllocVar(range);
		range->start = start + winOffset;
		range->end = end + winOffset;
		slAddHead(&rangeList, range);
                rangeWidth += (range->end - range->start);

                range->height = 1;
                if (tg->itemHeightRowsForPack != NULL)
                    range->height = tg->itemHeightRowsForPack(tg, item);

                if (hasDecorators(tg))
                    {
                    char itemString[2048];
                    struct linkedFeatures *lf = (struct linkedFeatures*) item;
                    safef(itemString, sizeof(itemString), "%s:%d-%d:%s", chrom, baseStart,
                            baseEnd, lf->name);
                    range->height += decoratorGroupHeight(tg, itemString);
                    }

		AllocVar(node);
		node->val = item;
		node->parentSs = sin->ss;
		node->noLabel = noLabel;
		slAddHead(&nodeList, node);

		noLabel = TRUE; // turns off labels for all following windows - for now.

		}

	    if (!foundWork)
		continue;


	    slReverse(&rangeList);
	    slReverse(&nodeList);

            // non-proportional fixed-width handling (e.g. GTEX)
            if (tg->nonPropPixelWidth)
                {
                int npWidth = tg->nonPropPixelWidth(tg, item);
		npWidth += leftLabelSize;
                if (npWidth > rangeWidth)
                    { // keep the first range but extend it
                    range = rangeList;
                    range->end = range->start + npWidth;
                    range->next = NULL;  // do not need the rest of the ranges
                    }
                }
            boolean doPadding = !cartOrTdbBoolean(cart, tg->tdb, "bedPackDense", FALSE);
	    if (spaceSaverAddOverflowMultiOptionalPadding(
                                ss, rangeList, nodeList, allowOverflow, doPadding) == NULL)
		break;

	    }
	}
    spaceSaverFinish(tg->ss);
    }
// must assign at end to get final row count
for(tg=tgSave; tg; tg=tg->nextWindow)
    {
    tg->ss->rowCount   = ss->rowCount;
    }
tg = tgSave;
spaceSaverFree(&ss);

return tg->ss->rowCount;
}

int packCountRows(struct track *tg, int maxCount, boolean withLabels, enum trackVisibility vis)
/* Return packed height. */
{
return packCountRowsOverflow(tg, maxCount, withLabels, FALSE, vis);
}

char *getItemDataName(struct track *tg, char *itemName)
/* Translate an itemName to its itemDataName, using tg->itemDataName if is not
 * NULL. The resulting value should *not* be freed, and it should be assumed
 * that it will only remain valid until the next call of this function.*/
{
return (tg->itemDataName != NULL) ? tg->itemDataName(tg, itemName)
    : itemName;
}

/* Some little functional stubs to fill in track group
 * function pointers with if we have nothing to do. */
void tgLoadNothing(struct track *tg){}
void tgFreeNothing(struct track *tg){}
int tgItemNoStart(struct track *tg, void *item) {return -1;}
int tgItemNoEnd(struct track *tg, void *item) {return -1;}

int tgFixedItemHeight(struct track *tg, void *item)
/* Return item height for fixed height track. */
{
return tg->lineHeight;
}

int maximumTrackItems(struct track *tg)
/* Return the maximum number of items allowed in track. */
{
static boolean set = FALSE;
static unsigned maxItemsPossible = 0;

if (!set)
    {
    char *maxItemsPossibleStr = cfgOptionDefault("maxItemsPossible", "100000");
    maxItemsPossible = sqlUnsigned(maxItemsPossibleStr);
    }

unsigned int maxItems = trackDbFloatSettingOrDefault(tg->tdb, "maxItems", maxItemsInFullTrack);

if (maxItems > maxItemsPossible)
    maxItems = maxItemsPossible;

return maxItems;
}

int maximumTrackHeight(struct track *tg)
/* Return the maximum track height allowed in pixels. */
{
int maxItems = maximumTrackItems(tg);
int height = maxItems * tl.fontHeight;
if (height > 31000)
    height = 31000;
return height;
}

static int maxItemsToOverflow(struct track *tg)
/* Return the maximum number of items to allow overflow indication. */
{
int answer = maxItemsToUseOverflowDefault;
char *maxItemsString = trackDbSetting(tg->tdb, "maxItemsToOverflow");
if (maxItemsString != NULL)
    answer = sqlUnsigned(maxItemsString);

return answer;
}

int tgFixedTotalHeightOptionalOverflow(struct track *tg, enum trackVisibility vis,
                                       int lineHeight, int heightPer, boolean allowOverflow)
/* Most fixed height track groups will use this to figure out the height
 * they use. */
{
int height;
if ((height = setupForWiggle(tg, vis)) != 0)
    return height;

int rows;
double maxHeight = maximumTrackHeight(tg);
int itemCount = slCount(tg->items);
int maxItemsToUseOverflow = maxItemsToOverflow(tg);
tg->heightPer = heightPer;
tg->lineHeight = lineHeight;

/* Note that the maxCount variable passed to packCountRowsOverflow()
   is tied to the maximum height allowed for a track and influences
   decisions about when to squish, dense, or overflow a track.

   If doing overflow try to pack all the items into the maxHeight area
   or put all the overflow into the last row. If not doing overflow
   allow the track enough rows to go over the maxHeight (thus if the
   spaceSaver fills up the total height will be more than maxHeight).
*/

switch (vis)
    {
    case tvFull:
	if (isTypeBedLike(tg))
	    {
	    if(allowOverflow && itemCount < maxItemsToUseOverflow)
		rows = packCountRowsOverflow(tg, floor(maxHeight/tg->lineHeight), FALSE, allowOverflow, vis);
	    else
		rows = packCountRowsOverflow(tg, floor(maxHeight/tg->lineHeight)+1, FALSE, FALSE, vis);
	    }
	else
	    {
	    rows = slCount(tg->items);
	    }
	break;
    case tvPack:
	{
	if(allowOverflow && itemCount < maxItemsToUseOverflow)
	    rows = packCountRowsOverflow(tg, floor(maxHeight/tg->lineHeight), TRUE, allowOverflow, vis);
	else
	    rows = packCountRowsOverflow(tg, floor(maxHeight/tg->lineHeight)+1, TRUE, FALSE, vis);
        if (tdbIsCompositeChild(tg->tdb))
            {
            boolean doHideEmpties = compositeChildHideEmptySubtracks(cart, tg->tdb, NULL, NULL);
            if (isCenterLabelsPackOff(tg) && !doHideEmpties)
                if (rows == 0)
                    rows = 1;   // compact pack mode, shows just side label
            }
	break;
	}
    case tvSquish:
        {
	tg->heightPer = heightPer/2;
	if ((tg->heightPer & 1) == 0)
	    tg->heightPer -= 1;
	tg->lineHeight = tg->heightPer + 1;
	if(allowOverflow && itemCount < maxItemsToUseOverflow)
	    rows = packCountRowsOverflow(tg, floor(maxHeight/tg->lineHeight), FALSE, allowOverflow, vis);
	else
	    rows = packCountRowsOverflow(tg, floor(maxHeight/tg->lineHeight)+1, FALSE, FALSE, vis);
	break;
	}
    case tvDense:
    default:
        rows = 1;
	break;
    }
tg->height = rows * tg->lineHeight;
return tg->height;
}


int tgFixedTotalHeightNoOverflow(struct track *tg, enum trackVisibility vis)
/* Most fixed height track groups will use this to figure out the height
 * they use. */
{
return tgFixedTotalHeightOptionalOverflow(tg,vis, tl.fontHeight+1, tl.fontHeight, FALSE);
}

int tgFixedTotalHeightUsingOverflow(struct track *tg, enum trackVisibility vis)
/* Returns how much height this track will use, tries to pack overflow into
  last row to avoid being more than maximumTrackHeight(). */
{
int height = tgFixedTotalHeightOptionalOverflow(tg, vis, tl.fontHeight+1, tl.fontHeight, TRUE);
return height;
}

int orientFromChar(char c)
/* Return 1 or -1 in place of + or - */
{
if (c == '-')
    return -1;
if (c == '+')
    return 1;
return 0;
}

char charFromOrient(int orient)
/* Return + or - in place of 1 or -1 */
{
if (orient < 0)
    return '-';
if (orient > 0)
    return '+';
return '.';
}


struct dyString *uiStateUrlPart(struct track *toggleGroup)
/* Return a string that contains all the UI state in CGI var
 * format.  If toggleGroup is non-null the visibility of that
 * track will be toggled in the string. */
{
struct dyString *dy = dyStringNew(512);

dyStringPrintf(dy, "%s=%s", cartSessionVarName(), cartSessionId(cart));
if (toggleGroup != NULL && tdbIsCompositeChild(toggleGroup->tdb))
    {
    int vis = toggleGroup->visibility;
    struct trackDb *tdbParent = tdbGetComposite(toggleGroup->tdb);
    char *parentName = tdbParent->track;
    // Find parent track (as opposed to trackDb)
    struct track *tgParent =toggleGroup->parent;
    char *encodedTableName = cgiEncode(parentName);
    char *view = NULL;
    boolean setView = subgroupFind(toggleGroup->tdb,"view",&view);
    if (tgParent!=NULL&& tgParent->visibility != tvHide && tvCompare(tgParent->visibility,vis) > 0)
        {
        setView = FALSE; // Must open parent to see opened child
        vis = tgParent->visibility;
        }
    if (vis == tvDense)
        {
        if (!toggleGroup->canPack || view != NULL)
            vis = tvFull;
        else
            vis = tvPack;
        }
    else if (vis != tvHide)
        vis = tvDense;

    if (setView)
        {
        dyStringPrintf(dy, "&%s=%s", toggleGroup->tdb->parent->track, hStringFromTv(vis));
        }
    else
        {
        dyStringPrintf(dy, "&%s=%s", encodedTableName, hStringFromTv(vis));
        }
    subgroupFree(&view);
    freeMem(encodedTableName);
    }
else
    {
    if (toggleGroup != NULL)
        {
        int vis = toggleGroup->visibility;
        char *encodedMapName = cgiEncode(toggleGroup->track);
        if (vis == tvDense)
            {
            if (!toggleGroup->canPack
            || (tdbIsComposite(toggleGroup->tdb) && subgroupingExists(toggleGroup->tdb,"view")))
                vis = tvFull;
            else
                vis = tvPack;
            }
        else if (vis == tvFull || vis == tvPack || vis == tvSquish)
            vis = tvDense;
        dyStringPrintf(dy, "&%s=%s", encodedMapName, hStringFromTv(vis));
        freeMem(encodedMapName);
        }
    }
return dy;
}

boolean isWithCenterLabels(struct track *track)
/* Cases: only TRUE when global withCenterLabels is TRUE
 * If track->tdb has a centerLabelDense setting, go with it.
// * If composite child then no center labels in dense mode. */
{
if ( track->originalTrack != NULL)
    return FALSE;
if (!withCenterLabels)
    {
    return FALSE;
    }
if (track != NULL)
    {
    char *centerLabelsDense = trackDbSetting(track->tdb, "centerLabelsDense");
    if (centerLabelsDense)
        {
        return sameWord(centerLabelsDense, "on");
        }
    }
return withCenterLabels;
}

boolean isCenterLabelConditionallySeen(struct track *track)
// returns FALSE if track and prevTrack have same parent, and are both dense subtracks
{
if (isCenterLabelConditional(track))
    {
    if (track->prevTrack
    &&  track->parent == track->prevTrack->parent
    &&  isCenterLabelConditional(track->prevTrack))
        return FALSE;
    }
return isWithCenterLabels(track);
}

boolean isCenterLabelConditional(struct track *track)
/* Dense subtracks and pack subtracks (when centerLabelsPack off set)
 *      show center labels depending on vis of previous track */
{
if (!tdbIsCompositeChild((track)->tdb))
    return FALSE;
enum trackVisibility vis = limitVisibility(track);
if (vis == tvFull || vis == tvSquish)
    return FALSE;
if (vis == tvDense)
    return TRUE;
// pack mode
return isCenterLabelsPackOff(track);
}

boolean isCenterLabelIncluded(struct track *track)
/* Center labels may be conditionally included */
{
/*   need to make this generic for squishyPack tracks  */
/*
if (sameString(track->track, "knownGeneSquish"))
    return FALSE;
    */
if (!isWithCenterLabels(track))
    return FALSE;
if (theImgBox)
    return TRUE;
if (isCenterLabelConditionallySeen(track))
    return TRUE;
return FALSE;
}

void mapStatusMessage(char *format, ...)
/* Write out stuff that will cause a status message to
 * appear when the mouse is over this box. */
{
va_list(args);
va_start(args, format);
hPrintf(" TITLE=\"");
hvPrintf(format, args);
hPutc('"');
va_end(args);
}

void mapBoxReinvoke(struct hvGfx *hvg, int x, int y, int width, int height,
                    struct track *track, boolean toggle, char *chrom,
                    long start, long end, char *message, char *extra)
/* Print out image map rectangle that would invoke this program again.
 * If track is non-NULL then put that track's id in the map item.
 * if toggle is true, then toggle track between full and dense.
 * If chrom is non-null then jump to chrom:start-end.
 * Add extra string to the URL if it's not NULL */
{
struct dyString *ui = uiStateUrlPart(toggle ? track : NULL);
struct dyString *id = dyStringNew(0);
if(track)
    dyStringPrintf(id, " id='%s'", track->track);
x = hvGfxAdjXW(hvg, x, width);

if (extra != NULL)
    {
    dyStringAppend(ui, "&");
    dyStringAppend(ui, extra);
    }
if (chrom == NULL)
    {
    chrom = virtChromName;
    start = virtWinStart;
    end = virtWinEnd;
    }
char pos[512];
safef(pos,sizeof(pos),"%s:%ld-%ld", chrom, start+1, end);
if (virtualSingleChrom())
    {
    char *newPos = disguisePositionVirtSingleChrom(pos); // DISGUISE POS
    safef(pos,sizeof(pos),"%s", newPos);
    freeMem(newPos);
    }

if (theImgBox && curImgTrack)
    {
    char link[512];
    safef(link,sizeof(link),"%s?position=%s&%s",       // NOTE: position may need removing
          hgTracksName(), pos, ui->string);    //       due to portal
    if (!revCmplDisp && x < fullInsideX)  // Do not toggle on side label!
        {
        width -= (fullInsideX+1 - x);
        if (width <= 1)
            {
            dyStringFree(&ui);
            return;
            }
        x = fullInsideX+1;
        }
    else if (revCmplDisp && (x+width) >= fullInsideWidth)
        {
        width -= (x+width) - fullInsideWidth + 1;
        if (width <= 1)
            {
            dyStringFree(&ui);
            return;
            }
        }
    //#ifdef IMAGEv2_SHORT_MAPITEMS
    //  if (x < insideX && x+width > insideX)
    //      warn("mapBoxReinvoke(%s) map item spanning slices. LX:%d TY:%d RX:%d BY:%d  link:[%s]",
    //           hStringFromTv(toggleGroup->visibility),x, y, x+width, y+height, link);
    //#endif//def IMAGEv2_SHORT_MAPITEMS
    imgTrackAddMapItem(curImgTrack,link,(char *)(message != NULL?message:NULL),x, y, x+width,
                       y+height, track ? track->track : NULL);
    }
else
    {
    hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
    hPrintf("HREF=\"%s?position=%s",
            hgTracksName(), pos);
    hPrintf("&%s\"", ui->string);
    if (message != NULL)
        mapStatusMessage("%s", message);
    hPrintf("%s>\n", dyStringContents(id));
    }
dyStringFree(&ui);
dyStringFree(&id);
}

void mapBoxToggleVis(struct hvGfx *hvg, int x, int y, int width, int height,
                     struct track *curGroup)
/* Print out image map rectangle that would invoke this program again.
 * program with the current track expanded. */
{
char buf[4096];
if (tdbIsCompositeChild(curGroup->tdb))
    safef(buf, sizeof(buf),"Click to alter the display density of %s and similar subtracks",
          curGroup->shortLabel);
else if (tdbIsComposite(curGroup->tdb))
    safef(buf, sizeof(buf),"Click to alter the maximum display mode density for all %s subtracks",
          curGroup->shortLabel);
else
    safef(buf, sizeof(buf),"Click to alter the display density of %s", curGroup->shortLabel);

mapBoxReinvoke(hvg, x, y, width, height, curGroup, TRUE, NULL, 0, 0, buf, NULL);
}

void mapBoxJumpTo(struct hvGfx *hvg, int x, int y, int width, int height, struct track *track,
	char *newChrom, long newStart, long newEnd, char *message)
/* Print out image map rectangle that would invoke this program again
 * at a different window. */
{
mapBoxReinvoke(hvg, x, y, width, height, track, FALSE, newChrom, newStart, newEnd,
	       message, NULL);

}

char *hgcNameAndSettings()
/* Return path to hgc with variables to store UI settings. */
{
static struct dyString *dy = NULL;

if (dy == NULL)
    {
    dy = dyStringNew(128);
    dyStringPrintf(dy, "%s?%s", hgcName(), cartSidUrlString(cart));
    }
return dy->string;
}

void mapBoxHgcOrHgGene(struct hvGfx *hvg, int start, int end, int x, int y, int width, int height,
                       char *track, char *item, char *statusLine, char *directUrl, boolean withHgsid,
                       char *extra)
/* Print out image map rectangle that would invoke the hgc (human genome click)
 * program. */
{
struct dyString *id = dyStringNew(0);
if (x < 0) x = 0;
x = hvGfxAdjXW(hvg, x, width);
int xEnd = x+width;
int yEnd = y+height;
dyStringPrintf(id, " id='%s'", track);

if (x < xEnd)
    {
    char *encodedItem = cgiEncode(item);
    char *encodedTrack = cgiEncode(track);
    if (theImgBox && curImgTrack)
        {
        char link[2000];
        if (directUrl)
            {
            safef(link,sizeof(link),directUrl, item, chromName, start, end, encodedTrack, database);
            if (withHgsid)
                safef(link+strlen(link),sizeof(link)-strlen(link),"&%s", cartSidUrlString(cart));
            }
        else
            {
	    // NOTE: chopped out winStart/winEnd
	    // NOTE: Galt added winStart/winEnd back in for multi-region
            safef(link,sizeof(link),"%s&db=%s&c=%s&l=%d&r=%d&o=%d&t=%d&g=%s&i=%s",
                hgcNameAndSettings(), database, cgiEncode(chromName), winStart, winEnd, start, end, encodedTrack, encodedItem);
            }
        if (extra != NULL)
            safef(link+strlen(link),sizeof(link)-strlen(link),"&%s", extra);
        // Add map item to current map (TODO: pass in map)
        #ifdef IMAGEv2_SHORT_MAPITEMS
        if (!revCmplDisp && x < insideX && xEnd > insideX)
            x = insideX;
        else if (revCmplDisp && x < insideWidth && xEnd > insideWidth)
            xEnd = insideWidth - 1;
        #endif//def IMAGEv2_SHORT_MAPITEMS
        imgTrackAddMapItem(curImgTrack,link,(char *)(statusLine!=NULL?statusLine:NULL),
                           x, y, xEnd, yEnd, track);
        }
    else
        {
        hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, xEnd, yEnd);
        if (directUrl)
            {
            hPrintf("HREF=\"");
            hPrintf(directUrl, item, chromName, start, end, encodedTrack, database);
            if (withHgsid)
                hPrintf("&%s", cartSidUrlString(cart));
            }
        else
            {
            hPrintf("HREF=\"%s&o=%d&t=%d&g=%s&i=%s&c=%s&l=%d&r=%d&db=%s&pix=%d",
                hgcNameAndSettings(), start, end, encodedTrack, encodedItem,
                    chromName, winStart, winEnd,
                    database, tl.picWidth);
            }
        if (extra != NULL)
            hPrintf("&%s", extra);
        hPrintf("\" ");
        if (statusLine != NULL)
            mapStatusMessage("%s", statusLine);
        hPrintf("%s>\n", dyStringContents(id));
        }
    freeMem(encodedItem);
    freeMem(encodedTrack);
    }
dyStringFree(&id);
}

void mapBoxHc(struct hvGfx *hvg, int start, int end, int x, int y, int width, int height,
              char *track, char *item, char *statusLine)
/* Print out image map rectangle that would invoke the hgc (human genome click)
 * program. */
{
mapBoxHgcOrHgGene(hvg, start, end, x, y, width, height, track, item, statusLine, NULL, FALSE, NULL);
}

double scaleForPixels(double pixelWidth)
/* Return what you need to multiply bases by to
 * get to scale of pixel coordinates. */
{
return pixelWidth / (winEnd - winStart);
}

int spreadStringCharWidth(int width, int count)
{
    return width/count;
}

void spreadAlignString(struct hvGfx *hvg, int x, int y, int width, int height,
                       Color color, MgFont *font, char *text,
		       char *match, int count, bool dots, bool isCodon)
/* Draw evenly spaced letters in string.  For multiple alignments,
 * supply a non-NULL match string, and then matching letters will be colored
 * with the main color, mismatched letters will have alt color (or
 * matching letters with a dot, and mismatched bases with main color if this
 * option is selected).
 * Draw a vertical bar in orange where sequence lacks gaps that
 * are in reference sequence (possible insertion) -- this is indicated
 * by an escaped insert count in the sequence.  The escape char is backslash.
 * The count param is the number of bases to print, not length of
 * the input line (text) */
{
char cBuf[2] = "";
int i,j,textPos=0;
int x1, x2;
char *motifString = cartOptionalString(cart,BASE_MOTIFS);
boolean complementsToo = cartUsualBoolean(cart, MOTIF_COMPLEMENT, FALSE);
char **motifs = NULL;
boolean *inMotif = NULL;
int motifCount = 0;
Color noMatchColor = lighterColor(hvg, color);
Color clr;
int textLength = strlen(text);
bool selfLine = (match == text);
cBuf[1] = '\0';

/* For some reason, the text is left shifted by one pixel in forward mode and
 * right shift in reverse-complement mode.  Not sure why this is, but its more
 * obvious in reverse-complement mode, with characters truncated at the larger
 * base display zoom level.  I appologize for doing this rather than figure
 * out the whole font size stuff.  markd
 */
x += (revCmplDisp ? 1 : -1);

/* If we have motifs, look for them in the string. */
if(motifString != NULL && strlen(motifString) != 0 && !isCodon)
    {
    touppers(motifString);
    eraseWhiteSpace(motifString);
    motifString = cloneString(motifString);
    motifCount = chopString(motifString, ",", NULL, 0);
    if (complementsToo)
	AllocArray(motifs, motifCount*2);	/* twice as many */
    else
	AllocArray(motifs, motifCount);
    chopString(motifString, ",", motifs, motifCount);
    if (complementsToo)
	{
	for(i = 0; i < motifCount; i++)
	    {
	    int comp = i + motifCount;
	    motifs[comp] = cloneString(motifs[i]);
	    reverseComplement(motifs[comp],strlen(motifs[comp]));
	    }
	motifCount *= 2;	/* now we have this many	*/
	}

    AllocArray(inMotif, textLength);
    for(i = 0; i < motifCount; i++)
	{
	char *mark = text;
	while((mark = stringIn(motifs[i], mark)) != NULL)
	    {
	    int end = mark-text + strlen(motifs[i]);
	    for(j = mark-text; j < end && j < textLength ; j++)
		{
		inMotif[j] = TRUE;
		}
	    mark++;
	    }
	}
    freez(&motifString);
    freez(&motifs);
    }

for (i=0; i<count; i++, text++, textPos++)
    {
    x1 = i * width/count;
    x2 = (i+1) * width/count - 1;
    if (match != NULL && *text == '|')
        {
        /* insert count follows -- replace with a colored vertical bar */
        text++;
	textPos++;
        i--;
        hvGfxBox(hvg, x+x1, y, 1, height, getOrangeColor());
        continue;
        }
    cBuf[0] = *text;
    clr = color;
    if (dots)
        {
        /* display bases identical to reference as dots */
        /* suppress for first line (self line) */
        if (!selfLine && match != NULL && match[i])
            if ((*text != ' ') && (toupper(*text) == toupper(match[i])))
                cBuf[0] = '.';

#ifdef FADE_IN_DOT_MODE
        /* Color gaps in lighter color, even when non-matching bases
           are displayed as dots instead of faded out */
        /* We may want to use this, or add a config setting for it */
        if (*text == '=' || *text == '-' || *text == '.' || *text == 'N')
            clr = noMatchColor;
#endif///def FADE_IN_DOT_MODE
        }
    else
        {
        /* display bases identical to reference in main color, mismatches
         * in alt color */
        if (match != NULL && match[i])
            if ((*text != ' ') && (toupper(*text) != toupper(match[i])))
                clr = noMatchColor;
        }
    if(inMotif != NULL && textPos < textLength && inMotif[textPos])
	{
	hvGfxBox(hvg, x1+x, y, x2-x1, height, clr);
	hvGfxTextCentered(hvg, x1+x, y, x2-x1, height, MG_WHITE, font, cBuf);
	}
    else
        {
        /* restore char for unaligned sequence to lower case */
        if (tolower(cBuf[0]) == tolower(UNALIGNED_SEQ))
            cBuf[0] = UNALIGNED_SEQ;

        /* display bases */
        hvGfxTextCentered(hvg, x1+x, y, x2-x1, height, clr, font, cBuf);
        }
    }
freez(&inMotif);
}

void spreadAlignStringProt(struct hvGfx *hvg, int x, int y, int width, int height,
                           Color color, MgFont *font, char *text, char *match, int count,
                           bool dots, bool isCodon, int seqStart, int offset)
/* Draw evenly spaced letters in string for protein sequence.
 * For multiple alignments,
 * supply a non-NULL match string, and then matching letters will be colored
 * with the main color, mismatched letters will have alt color (or
 * matching letters with a dot, and mismatched bases with main color if this
 * option is selected).
 * Draw a vertical bar in orange where sequence lacks gaps that
 * are in reference sequence (possible insertion) -- this is indicated
 * by an escaped insert count in the sequence.  The escape char is backslash.
 * The count param is the number of bases to print, not length of
 * the input line (text) */
{
    errAbort("Function spreadAlignStringProt() called.  It is not supported on non-GSID server.");
#ifdef NOTNOW
char cBuf[2] = "";
int i,j,textPos=0;
int x1, x2, xx1, xx2;
char *motifString = cartOptionalString(cart,BASE_MOTIFS);
boolean complementsToo = cartUsualBoolean(cart, MOTIF_COMPLEMENT, FALSE);
char **motifs = NULL;
boolean *inMotif = NULL;
int motifCount = 0;
Color noMatchColor = lighterColor(hvg, color);
Color clr;
int textLength = strlen(text);
bool selfLine = (match == text);

/* set alternating colors */
Color color1, color2;
color1 = hvGfxFindColorIx(hvg, 12, 12, 120);
color1 = lighterColor(hvg, color1);
color1 = lighterColor(hvg, color1);
color2 = lighterColor(hvg, color1);

if (!hIsGsidServer())
    {
    errAbort("Function spreadAlignStringProt() called.  It is not supported on non-GSID server.");
    }

cBuf[1] = '\0';

/* If we have motifs, look for them in the string. */
if(motifString != NULL && strlen(motifString) != 0 && !isCodon)
    {
    touppers(motifString);
    eraseWhiteSpace(motifString);
    motifString = cloneString(motifString);
    motifCount = chopString(motifString, ",", NULL, 0);
    if (complementsToo)
	AllocArray(motifs, motifCount*2);	/* twice as many */
    else
	AllocArray(motifs, motifCount);
    chopString(motifString, ",", motifs, motifCount);
    if (complementsToo)
	{
	for(i = 0; i < motifCount; i++)
	    {
	    int comp = i + motifCount;
	    motifs[comp] = cloneString(motifs[i]);
	    reverseComplement(motifs[comp],strlen(motifs[comp]));
	    }
	motifCount *= 2;	/* now we have this many	*/
	}

    AllocArray(inMotif, textLength);
    for(i = 0; i < motifCount; i++)
	{
	char *mark = text;
	while((mark = stringIn(motifs[i], mark)) != NULL)
	    {
	    int end = mark-text + strlen(motifs[i]);
	    for(j = mark-text; j < end && j < textLength ; j++)
		{
		inMotif[j] = TRUE;
		}
	    mark++;
	    }
	}
    freez(&motifString);
    freez(&motifs);
    }
for (i=0; i<count; i++, text++, textPos++)
    {
    x1 = i * width/count;
    x2 = (i+1) * width/count - 1;

    xx1 = (i-1) * width/count;
    xx2 = (i+2) * width/count - 1;

    if (match != NULL && *text == '|')
        {
        /* insert count follows -- replace with a colored vertical bar */
        text++;
	textPos++;
        i--;
        hvGfxBox(hvg, x+x1, y, 1, height, getOrangeColor());
        continue;
        }
    if (*text != ' ')
	cBuf[0] = lookupCodon(text-1);
    else
	cBuf[0] = ' ';
    if (cBuf[0] == 'X') cBuf[0] = '-';
    clr = color;
    if (dots)
        {
        /* display bases identical to reference as dots */
        /* suppress for first line (self line) */
        if (!selfLine && match != NULL && match[i])
            if ((*text != ' ') && (lookupCodon(text-1) ==  lookupCodon(match+textPos-1)))
                cBuf[0] = '.';
        }
    else
        {
        /* display bases identical to reference in main color, mismatches
         * in alt color */
        if (match != NULL && match[i])
            if ((*text != ' ') && (toupper(*text) != toupper(match[i])))
                clr = noMatchColor;
        }

    /* This function is for protein display, so force inMotif to NULL to avoid motif highlighting */
    inMotif = NULL;

    if(inMotif != NULL && textPos < textLength && inMotif[textPos])
	{
	hvGfxBox(hvg, x1+x, y, x2-x1, height, clr);
	hvGfxTextCentered(hvg, x1+x, y, x2-x1, height, MG_WHITE, font, cBuf);
	}
    else
        {
        /* restore char for unaligned sequence to lower case */
        if (tolower(cBuf[0]) == tolower(UNALIGNED_SEQ))
            cBuf[0] = UNALIGNED_SEQ;
        /* display bases */
        if (cBuf[0] != ' ')
	    {
	    /* display AA at the center of a codon */
	    if (((seqStart + textPos) % 3) == offset)
                {
		/* display alternate background color */
                if (((seqStart + textPos)/3 %2) == 0)
                    {
                    hvGfxBox(hvg, xx1+x, y, xx2-xx1, height, color1);
                    }
                else
                    {
                    hvGfxBox(hvg, xx1+x, y, xx2-xx1, height, color2);
                    }

		/* display AA */
                hvGfxTextCentered(hvg, x1+x, y, x2-x1, height, clr, font, cBuf);
                }
	    }
	}
    }
freez(&inMotif);
#endif
}

void spreadBasesString(struct hvGfx *hvg, int x, int y, int width, int height, Color color,
                       MgFont *font, char *s, int count, bool isCodon)
/* Draw evenly spaced letters in string. */
{
spreadAlignString(hvg, x, y, width, height, color, font, s,
                        NULL, count, FALSE, isCodon);
}

boolean scaledBoxToPixelCoords(int chromStart, int chromEnd, double scale, int xOff, int *pX1, int *pX2)
/* Convert chrom coordinates to pixels. Clip to window to prevent integer overflow.
 * For special case of a SNP insert location with item width==0, set pixel width=1 and include
 * insertions at window boundaries.
 * Returns FALSE if it does not intersect the window, or if it would have a negative width. */
{
// Treat 0-length insertions a little differently: include insertions at boundary of window
// and make them 1 pixel wide.
boolean isIns = (chromStart == chromEnd);
if (chromEnd < chromStart) // Invalid coordinates
    return FALSE;  // Ignore.
if (chromStart > winEnd || winStart > chromEnd ||
    (!isIns && chromStart == winEnd) ||
    (!isIns && chromEnd == winStart))  // doesn't overlap window?
    return FALSE; // nothing to do
if (chromStart < winStart) // clip to part overlapping window
    chromStart = winStart;
if (chromEnd > winEnd)     // which prevents x1,x2 from overflowing when zooming-in makes scale large.
    chromEnd = winEnd;
*pX1 = round((double)(chromStart-winStart)*scale) + xOff;
*pX2 = isIns ? (*pX1 + 1) : round((double)(chromEnd-winStart)*scale) + xOff;
return TRUE;
}


void drawScaledBox(struct hvGfx *hvg, int chromStart, int chromEnd,
                   double scale, int xOff, int y, int height, Color color)
/* Draw a box scaled from chromosome to window coordinates.
 * Get scale first with scaleForPixels. */
{
int x1, x2;
if (scaledBoxToPixelCoords(chromStart, chromEnd, scale, xOff, &x1, &x2))
    {
    int w = x2-x1;
    if (w == 0) // when zoomed out, avoid shinking to nothing
	w = 1;
    hvGfxBox(hvg, x1, y, w, height, color);
    }
}

void drawScaledBoxLabel(struct hvGfx *hvg,
     int chromStart, int chromEnd, double scale,
     int xOff, int y, int height, Color color, MgFont *font,  char *label)
/* Draw a box scaled from chromosome to window coordinates and draw a label onto it.
 * Lots of code copied from drawScaledBox */
{
int x1, x2, w;
if (!scaledBoxToPixelCoords(chromStart, chromEnd, scale, xOff, &x1, &x2))
    return;
w = x2-x1;
if (w == 0) // when zoomed out, avoid shinking to nothing
    w = 1;
hvGfxBox(hvg, x1, y, w, height, color);

char *shortLabel = cloneString(label);
/* calculate how many characters we can squeeze into the box */
int charsInBox = w / mgFontCharWidth(font, 'm');
if (charsInBox < strlen(label))
    if (charsInBox > 4)
        strcpy(shortLabel+charsInBox-3, "...");
if (charsInBox < strlen(shortLabel))
    return;
Color labelColor = hvGfxContrastingColor(hvg, color);
hvGfxTextCentered(hvg, x1, y, w, height, labelColor, font, shortLabel);
}

struct xyPair {
    double x, y;
};

struct glyphShape {
    int nPoints;
    struct xyPair* points;
};

/* Glyph definitions
 *
 * An obtuse representation, but this is a list of the glyphs we know how to draw as polygons along with
 * coordinates for the sequence of points for each glyph on the unit square.  Those will then be scaled
 * by whatever the current track height is for actual drawing.
 * There's one glyph that's special-cased - GLYPH_CIRCLE is drawn with a separate routine.
 *
 * Each glyph is expected to be defined on the unit square with 0,0 at the top left and 1,1 at the bottom right.
 * Note that triangle and inverse triangle subvert this a little by extending just outside those bounds.
 */
static struct glyphShape glyphShapes[] = {
    [GLYPH_CIRCLE] = (struct glyphShape) {0, NULL},

    [GLYPH_TRIANGLE] = (struct glyphShape) {3, (struct xyPair[]) {{0.5,0},{1.07735,1},{-0.07735,1}}},
    [GLYPH_INV_TRIANGLE] = (struct glyphShape) {3, (struct xyPair[]) {{0.5,1},{1.07735,0},{-0.07735,0}}},
    [GLYPH_SQUARE] = (struct glyphShape) {4, (struct xyPair[]) {{0,0},{1,0},{1,1},{0,1}}},
    [GLYPH_DIAMOND] = (struct glyphShape) {4, (struct xyPair[]) {{0.5,0},{1,0.5},{0.5,1},{0,0.5}}},
    [GLYPH_PENTAGRAM] = (struct glyphShape) {5, (struct xyPair[]) {{0.5,0.0},{0.824920,1.0},{-0.025731,0.381966},
            {1.02573,0.381966},{0.175080,1}}},
    [GLYPH_OCTAGON] = (struct glyphShape) {8, (struct xyPair[]) {{0.292893,1},{0.707107,1},{1,0.707107},
            {1,0.292893},{0.707107,0},{0.292893,0},{0,0.292893},{0,0.707107}}},
    [GLYPH_STAR] = (struct glyphShape) {10, (struct xyPair[]) {{0.500000,0.000000},{0.624108,0.381966},{1.025731,0.381966},
            {0.700811,0.618034},{0.824920,1.000000},{0.500000,0.763932},{0.175080,1.000000},{0.299189,0.618034},
            {-0.025731,0.381966},{0.375892,0.381966}}}
};


void drawScaledGlyph(struct hvGfx *hvg, int chromStart, int chromEnd, double scale, int xOff, int y,
                      int heightPer, glyphType glyph, boolean filled, Color outlineColor, Color fillColor)
/* Draw a glyph as a circle/polygon.  If filled, draw as with fillColor,
 * which may have transparency.
 */
{
int glyphHeight = heightPer-1;
int startX, endX;
double middleX, middleY = y+heightPer/2.0;
// A glyph might be defined on a wide range - find the center and draw specifically there
// so we don't have a glyph shifting if only part of that window is in view.
int centeredStart, centeredEnd;
centeredStart = (chromStart + chromEnd)/2;
centeredEnd = (chromStart + chromEnd+1)/2;
int ptCount, i, x0, y0;
if (!scaledBoxToPixelCoords(centeredStart, centeredEnd, scale, xOff, &startX, &endX))
    return;  // apparently we don't intersect the window
middleX = (startX+endX)/2.0;
switch (glyph)
    {
    case GLYPH_CIRCLE:
        hvGfxCircle(hvg, middleX, middleY, heightPer/2, fillColor, TRUE);
        hvGfxCircle(hvg, middleX, middleY, heightPer/2, outlineColor, FALSE);
        break;
    default:
        ptCount = glyphShapes[glyph].nPoints;
        struct gfxPoly *poly = gfxPolyNew();
        for (i=0; i<ptCount; i++)
            {
            x0 = middleX + (glyphShapes[glyph].points[i].x-0.5)*glyphHeight;
            y0 = middleY + (glyphShapes[glyph].points[i].y-0.5)*glyphHeight;
            gfxPolyAddPoint(poly, x0, y0);
            }
        hvGfxDrawPoly(hvg,poly,fillColor,TRUE);
        hvGfxDrawPoly(hvg,poly,outlineColor,FALSE);
        gfxPolyFree(&poly);
        break;
    }
}

#define GLYPH_STRING_CIRCLE "Circle"
#define GLYPH_STRING_TRIANGLE "Triangle"
#define GLYPH_STRING_INV_TRIANGLE "InvTriangle"
#define GLYPH_STRING_SQUARE "Square"
#define GLYPH_STRING_DIAMOND "Diamond"
#define GLYPH_STRING_OCTAGON "Octagon"
#define GLYPH_STRING_STAR "Star"
#define GLYPH_STRING_PENTAGRAM "Pentagram"

glyphType parseGlyphType(char *glyphStr)
/* Return the enum glyph type for a string specifying a glyph.
 * Defaults to GLYPH_CIRCLE if the string is unrecognized. */
{
if (sameWordOk(glyphStr, GLYPH_STRING_TRIANGLE))
    return GLYPH_TRIANGLE;
if (sameWordOk(glyphStr, GLYPH_STRING_INV_TRIANGLE))
    return GLYPH_INV_TRIANGLE;
if (sameWordOk(glyphStr, GLYPH_STRING_SQUARE))
    return GLYPH_SQUARE;
if (sameWordOk(glyphStr, GLYPH_STRING_DIAMOND))
    return GLYPH_DIAMOND;
if (sameWordOk(glyphStr, GLYPH_STRING_OCTAGON))
    return GLYPH_OCTAGON;
if (sameWordOk(glyphStr, GLYPH_STRING_STAR))
    return GLYPH_STAR;
if (sameWordOk(glyphStr, GLYPH_STRING_PENTAGRAM))
    return GLYPH_PENTAGRAM;

return GLYPH_CIRCLE;
}

void filterItemsOnNames(struct track *tg)
/* Only keep items with a name in the .nameFilter cart var. 
 * Not using filterItems(), because filterItems has no state at all. */
{
char varName[SMALLBUF];
safef(varName, sizeof(varName), "%s.nameFilter", tg->tdb->track);
char *nameFilterStr = cartNonemptyString(cart, varName);

if (nameFilterStr==NULL)
    return;

struct slName *names = slNameListFromString(nameFilterStr, ',');
struct hash *nameHash = hashFromSlNameList(names);

struct slList *newList = NULL, *el, *next;
for (el = tg->items; el != NULL; el = next)
    {
    next = el->next;
    struct linkedFeatures *lf = (struct linkedFeatures*)el;
    char *name = lf->name;
    if (name && hashLookup(nameHash, name))
	slAddHead(&newList, el);
}
tg->items = newList;

char *suf = "";
int nameCount = slCount(names);
if (nameCount > 1)
    suf = "s";

char buf[SMALLBUF];
safef(buf, sizeof(buf), " (manually filtered to show only %d accession%s)", nameCount, suf);
char *oldLabel = tg->longLabel;
tg->longLabel = catTwoStrings(tg->longLabel, buf);
freez(&oldLabel);

slFreeList(&names);
hashFree(&nameHash);
}

void filterItems(struct track *tg, boolean (*filter)(struct track *tg, void *item),
                char *filterType)
/* Filter out items from track->itemList. */
{
struct slList *newList = NULL, *oldList = NULL, *el, *next;
boolean exclude = FALSE;
boolean color = FALSE;
enum trackVisibility vis = tvHide;

if (sameWord(filterType, "none"))
    return;

if (sameWord(filterType, "include"))
    exclude = FALSE;
else if (sameWord(filterType, "exclude"))
    exclude = TRUE;
else
    {
    color = TRUE;
    /* Important: call limitVisibility on *complete* item list, before it gets
     * divided into new and old!  Otherwise tg->height is set incorrectly. */
    vis = limitVisibility(tg);
    }

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
   if (vis == tvDense)
       newList = slCat(oldList, newList);
   else
       newList = slCat(newList, oldList);
   }
tg->items = newList;

filterItemsOnNames(tg);
}

int getFilterColor(char *type, int colorIx)
/* Get color corresponding to type - MG_RED for "red" etc. */
{
if (sameString(type, "red"))
    colorIx = MG_RED;
else if (sameString(type, "green"))
    colorIx = MG_GREEN;
else if (sameString(type, "blue"))
    colorIx = MG_BLUE;
return colorIx;
}

struct track *trackNew()
/* Allocate track . */
{
struct track *tg;
AllocVar(tg);
tg->color = colorIxToRgb(MG_BLACK);
tg->altColor = colorIxToRgb(MG_BLACK);
return tg;
}

int simpleFeatureCmp(const void *va, const void *vb)
/* Compare to sort based on start. */
{
const struct simpleFeature *a = *((struct simpleFeature **)va);
const struct simpleFeature *b = *((struct simpleFeature **)vb);
return a->start - b->start;
}

int linkedFeaturesCmp(const void *va, const void *vb)
/* Compare to sort based on start. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
return a->start - b->start;
}

char *linkedFeaturesName(struct track *tg, void *item)
/* Return name of item. */
{
struct linkedFeatures *lf = item;
return lf->name;
}

char *linkedFeaturesNameField1(struct track *tg, void *item)
/* return part before first space in item name */
{
struct linkedFeatures *lf = item;
return cloneFirstWord(lf->name);
}

char *linkedFeaturesNameNotField1(struct track *tg, void *item)
/* return part after first space in item name */
{
struct linkedFeatures *lf = item;
return cloneNotFirstWord(lf->name);
}

void linkedFeaturesFreeList(struct linkedFeatures **pList)
/* Free up a linked features list. */
{
struct linkedFeatures *lf;
for (lf = *pList; lf != NULL; lf = lf->next)
    {
    slFreeList(&lf->components);
    slFreeList(&lf->codons);
    }
slFreeList(pList);
}

void linkedFeaturesFreeItems(struct track *tg)
/* Free up linkedFeaturesTrack items. */
{
linkedFeaturesFreeList((struct linkedFeatures**)(&tg->items));
}

#ifndef GBROWSE
boolean gvFilterType(struct gv *el)
/* Check to see if this element should be excluded. */
{
int cnt = 0;
for (cnt = 0; cnt < gvTypeSize; cnt++)
    {
    if (cartVarExists(cart, gvTypeString[cnt]) &&
        cartString(cart, gvTypeString[cnt]) != NULL &&
        differentString(cartString(cart, gvTypeString[cnt]), "0") &&
        sameString(gvTypeDbValue[cnt], el->baseChangeType))
        {
        return FALSE;
        }
    }
return TRUE;
}

boolean gvFilterLoc(struct gv *el)
/* Check to see if this element should be excluded. */
{
int cnt = 0;
for (cnt = 0; cnt < gvLocationSize; cnt++)
    {
    if (cartVarExists(cart, gvLocationString[cnt]) &&
        cartString(cart, gvLocationString[cnt]) != NULL &&
        differentString(cartString(cart, gvLocationString[cnt]), "0") &&
        sameString(gvLocationDbValue[cnt], el->location))
        {
        return FALSE;
        }
    }
return TRUE;
}

boolean gvFilterAccuracy(struct gv *el)
/* Check to see if this element should be excluded. */
{
int cnt = 0;
for (cnt = 0; cnt < gvAccuracySize; cnt++)
    {
    if (cartVarExists(cart, gvAccuracyString[cnt]) &&
        cartString(cart, gvAccuracyString[cnt]) != NULL &&
        differentString(cartString(cart,gvAccuracyString[cnt]), "0") &&
        gvAccuracyDbValue[cnt] == el->coordinateAccuracy)
        {
        /* string 0/1 unselected/selected, unsigned 0/1 estimated/known */
        return FALSE;
        }
    }
return TRUE;
}

boolean gvFilterSrc(struct gv *el, struct gvSrc *srcList)
/* Check to see if this element should be excluded. */
{
int cnt = 0;
struct gvSrc *src = NULL;
char *srcTxt = NULL;
struct hashEl *filterList = NULL;

for (src = srcList; src != NULL; src = src->next)
    {
    if (sameString(el->srcId, src->srcId))
        {
        srcTxt = cloneString(src->src);
        break;
        }
    }
filterList = cartFindPrefix(cart, "gvPos.filter.src.");
if (srcTxt == NULL)
    errAbort("Bad value for srcId");
else if (filterList == NULL)
    {
    /* if no src filters, set or unset, use defaults */
    cartSetInt(cart, gvSrcString[0], 1);
    }
hashElFreeList(&filterList);

for (cnt = 0; cnt < gvSrcSize; cnt++)
    {
    if (cartVarExists(cart, gvSrcString[cnt]) &&
        cartString(cart, gvSrcString[cnt]) != NULL &&
        differentString(cartString(cart,gvSrcString[cnt]), "0") &&
        sameString(gvSrcDbValue[cnt], srcTxt))
        {
        return FALSE;
        }
    }
return TRUE;
}

boolean gvFilterDA(struct gv *el)
/* Check to see if this element should be excluded (disease association attribute) */
{
int cnt = 0;
struct gvAttr *attr = NULL;
char query[256];
char *id = NULL;
struct sqlConnection *conn = hAllocConn(database);

if (el->id != NULL)
    id = el->id;
else
    id = el->name;

sqlSafef(query, sizeof(query), "select * from hgFixed.gvAttr where id = '%s' and attrType = 'disease'", id);
attr = gvAttrLoadByQuery(conn, query);
hFreeConn(&conn);
if (attr == NULL)
    {
    AllocVar(attr);
    attr->attrVal = cloneString("NULL");
    attr->id = NULL; /* so free will work */
    attr->attrType = NULL;
    }
for (cnt = 0; cnt < gvFilterDASize; cnt++)
    {
    if (cartVarExists(cart, gvFilterDAString[cnt]) &&
        cartString(cart, gvFilterDAString[cnt]) != NULL &&
        differentString(cartString(cart, gvFilterDAString[cnt]), "0") &&
        sameString(gvFilterDADbValue[cnt], attr->attrVal))
        {
        gvAttrFree(&attr);
        return FALSE;
        }
    }
gvAttrFree(&attr);
return TRUE;
}

void lookupGvName(struct track *tg)
/* give option on which name to display */
{
struct gvPos *el;
struct sqlConnection *conn = hAllocConn(database);
boolean useHgvs = FALSE;
boolean useId = FALSE;
boolean useCommon = FALSE;
boolean labelStarted = FALSE;

struct hashEl *gvLabels = cartFindPrefix(cart, "gvPos.label");
struct hashEl *label;
if (gvLabels == NULL)
    {
    useHgvs = TRUE; /* default to gene name */
    /* set cart to match what is being displayed */
    cartSetBoolean(cart, "gvPos.label.hgvs", TRUE);
    }
for (label = gvLabels; label != NULL; label = label->next)
    {
    if (endsWith(label->name, "hgvs") && differentString(label->val, "0"))
        useHgvs = TRUE;
    else if (endsWith(label->name, "common") && differentString(label->val, "0"))
        useCommon = TRUE;
    else if (endsWith(label->name, "dbid") && differentString(label->val, "0"))
        useId = TRUE;
    }
if (!useHgvs && !useCommon && !useId)
    {
    /* assume noone really wants no names, squish will still remove names */
    useHgvs = TRUE;
    cartSetBoolean(cart, "gvPos.label.hgvs", TRUE);
    }

for (el = tg->items; el != NULL; el = el->next)
    {
    struct dyString *name = dyStringNew(SMALLDYBUF);
    labelStarted = FALSE; /* reset for each item */
    if (useHgvs)
        {
        dyStringAppend(name, el->label);
        labelStarted = TRUE;
        }
    if (useCommon)
        {
        char query[256];
        char *commonName = NULL;
        sqlSafef(query, sizeof(query), "select attrVal from hgFixed.gvAttr where id = '%s' and attrType = 'commonName'", el->name);
        commonName = sqlQuickString(conn, query);
        if (labelStarted) dyStringAppendC(name, '/');
        else labelStarted = TRUE;
        if (commonName != NULL)
            dyStringAppend(name, commonName);
        else
            dyStringAppend(name, " ");
        }
    if (useId)
        {
        if (labelStarted) dyStringAppendC(name, '/');
        else labelStarted = TRUE;
        dyStringAppend(name, el->name);
        }
    el->id = el->name; /* reassign ID */
    el->name = dyStringCannibalize(&name);
    }
hFreeConn(&conn);
}

void loadGV(struct track *tg)
/* Load human mutation with filter */
{
struct gvPos *list = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlConnection *conn2 = hAllocConn(database);
struct sqlResult *sr;
struct gvSrc *srcList = NULL;
char **row;
int rowOffset;
enum trackVisibility vis = tg->visibility;

if (!cartVarExists(cart, "gvDisclaimer"))
    {
    /* display disclaimer and add flag to cart, program exits from here */
    gvDisclaimer();
    }
else if (sameString("Disagree", cartString(cart, "gvDisclaimer")))
    {
    /* hide track, remove from cart so will get option again later */
    tg->visibility = tvHide;
    cartRemove(cart, "gvDisclaimer");
    cartSetString(cart, "gvPos", "hide");
    return;
    }
/* load as linked list once, outside of loop */
char query[1024];
sqlSafef(query, sizeof query, "select * from hgFixed.gvSrc");
srcList = gvSrcLoadByQuery(conn, query);
/* load part need from gv table, outside of loop (load in hash?) */
sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct gv *details = NULL;
    char query[256];
    struct gvPos *el = gvPosLoad(row);
    sqlSafef(query, sizeof(query), "select * from hgFixed.gv where id = '%s'", el->name);
    details = gvLoadByQuery(conn2, query);
    /* searched items are always visible */
    if (hgFindMatches != NULL && hashIntValDefault(hgFindMatches, el->name, 0) == 1)
        slAddHead(&list, el);
    else if (!gvFilterType(details))
        gvPosFree(&el);
    else if (!gvFilterLoc(details))
        gvPosFree(&el);
    else if (!gvFilterSrc(details, srcList))
        gvPosFree(&el);
    else if (!gvFilterAccuracy(details))
        gvPosFree(&el);
    else if (!gvFilterDA(details))
        gvPosFree(&el);
    else
        slAddHead(&list, el);
    gvFreeList(&details);
    }
sqlFreeResult(&sr);
slReverse(&list);
gvSrcFreeList(&srcList);
tg->items = list;
/* change names here so not affected if change filters later
   and no extra if when viewing dense                        */
if (vis != tvDense)
    {
    lookupGvName(tg);
    }
hFreeConn(&conn);
hFreeConn(&conn2);
}

struct bed *loadGvAsBed (struct track *tg, char *chr, int start, int end)
/* load gv* with filters, for a range, as a bed list (for next item button) */
{
char *chromNameCopy = cloneString(chromName);
int winStartCopy = winStart;
int winEndCopy = winEnd;
struct gvPos *el = NULL;
struct bed *bed, *list = NULL;
/* set globals and use loadGv */
chromName = chr;
winStart = start;
winEnd = end;
loadGV(tg);
/* now walk through tg->items creating the bed list */
for (el = tg->items; el != NULL; el = el->next)
    {
    AllocVar(bed);
    bed->chromStart = el->chromStart;
    bed->chromEnd = el->chromEnd;
    bed->chrom = cloneString(el->chrom);
    bed->name = cloneString(el->name);
    slAddHead(&list, bed);
    }
/* reset globals */
chromName = chromNameCopy;
winStart = winStartCopy;
winEnd = winEndCopy;
return list;
}




boolean oregannoFilterType (struct oreganno *el, struct hash *attrTable)
/* filter of the type of region from the oregannoAttr table */
{
int cnt = 0;
struct oregannoAttr *attr = hashFindVal(attrTable, el->id);
boolean tmpAttr = FALSE;

if (attr == NULL)
    {
    AllocVar(attr);
    attr->attrVal = cloneString("NULL");
    attr->id = NULL; /* so free will work */
    attr->attribute = NULL;
    tmpAttr = TRUE;
    }

for (cnt = 0; cnt < oregannoTypeSize; cnt++)
    {
    if ((!cartVarExists(cart, oregannoTypeString[cnt])
    ||  (cartString(cart, oregannoTypeString[cnt]) != NULL
        && differentString(cartString(cart, oregannoTypeString[cnt]), "0")))
        && (cmpWordsWithEmbeddedNumbers(oregannoTypeDbValue[cnt], attr->attrVal))==0)
        {
        if (tmpAttr == TRUE)
            oregannoAttrFree(&attr);
        return TRUE; /* include this type */
        }
    }
if (tmpAttr == TRUE)
    oregannoAttrFree(&attr);
return FALSE;
}

void loadOreganno (struct track *tg)
/* loads the oreganno track */
{
struct oreganno *list = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
tg->attrTable = oregannoLoadAttrHash(conn);

sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd,
                 NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct oreganno *el = oregannoLoad(row);
    if (!oregannoFilterType(el, tg->attrTable))
        oregannoFree(&el);
    else
        slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);
tg->items = list;
hFreeConn(&conn);
}

struct bed* loadLongTabixAsBed (struct track *tg, char *chr, int start, int end)
/* load bigBed for a range, as a bed list (for next item button). Just grab one item */
{
struct hash *settings = tg->tdb->settingsHash;
char *bigDataUrl = hashFindVal(settings, "bigDataUrl");
struct bedTabixFile *btf = bedTabixFileOpen(bigDataUrl, NULL, 0, 0);
struct bed *bed  = bedTabixReadFirstBed(btf, chromName, start, end, bedLoad5);
bedTabixFileClose(&btf);

return bed;
}


struct bed* loadBigBedAsBed (struct track *tg, char *chr, int start, int end)
/* load bigBed for a range, as a bed list (for next item button). Just grab one item */
{
struct bbiFile *bbiFile = fetchBbiForTrack(tg);
struct lm *lm = lmInit(0);
struct bigBedInterval *intervals = bigBedIntervalQuery(bbiFile, chr,
	start, end, 1, lm);


struct bed *retBed = NULL;
if (intervals != NULL)
    {
    AllocVar(retBed);
    retBed->chrom = cloneString(chr);
    retBed->chromStart = intervals->start;
    retBed->chromEnd = intervals->end;
    }

lmCleanup(&lm);

return retBed;
}

struct bed* loadOregannoAsBed (struct track *tg, char *chr, int start, int end)
/* load oreganno with filters, for a range, as a bed list (for next item button) */
{
char *chromNameCopy = cloneString(chromName);
int winStartCopy = winStart;
int winEndCopy = winEnd;
struct oreganno *el = NULL;
struct bed *bed, *list = NULL;
/* set globals and use loadOreganno */
chromName = chr;
winStart = start;
winEnd = end;
loadOreganno(tg);
/* now walk through tg->items creating the bed list */
for (el = tg->items; el != NULL; el = el->next)
    {
    AllocVar(bed);
    bed->chromStart = el->chromStart;
    bed->chromEnd = el->chromEnd;
    bed->chrom = cloneString(el->chrom);
    bed->name = cloneString(el->id);
    slAddHead(&list, bed);
    }
/* reset globals */
chromName = chromNameCopy;
winStart = winStartCopy;
winEnd = winEndCopy;
return list;
}
#endif /* GBROWSE */

int exonSlRefCmp(const void *va, const void *vb)
/* Sort the exons put on an slRef. */
{
const struct slRef *a = *((struct slRef **)va);
const struct slRef *b = *((struct slRef **)vb);
struct simpleFeature *sfA = a->val;
struct simpleFeature *sfB = b->val;
return sfA->start - sfB->start;
}

int exonSlRefReverseCmp(const void *va, const void *vb)
/* Reverse of the exonSlRefCmp sort. */
{
return -1 * exonSlRefCmp(va, vb);
}


void linkedFeaturesNextPrevExonFind(struct track *tg, boolean next, struct convertRange *crList)
//char *newChrom, int newWinStart, int newWinEnd, long *retVirtWinStart, long *retVirtWinEnd
/* Find next-exon function for linkedFeatures.  Finds corresponding position on virtual chrom for new winStart/winEnd of exon non-virt position,
 * and returns it. This function was cloned from linkedFeaturesLabelNextPrevItem and modified. */
{

struct convertRange *cr;

// Special case speed optimization. (It still works without this).
if (!virtMode)
    {
    for (cr=crList; cr; cr=cr->next)
	{
	if (cr->skipIt)
	    {
	    cr->vStart = -1;
	    cr->vEnd   = -1;
	    }
	else
	    {
	    cr->vStart = cr->start;
	    cr->vEnd   = cr->end;
	    }
	}
    return;
    }

// TODO we could exit this routine faster for the case where it would not be found
//  if we know that the virt chrom is made out of ascending non-overlapping chrom regions.

// set found for skipIts
for (cr=crList; cr; cr=cr->next)
    {
    if (cr->skipIt)
	{
	cr->found = TRUE;
	}
    }

long start = virtWinStart;
long end = virtWinEnd;
long size = virtWinBaseCount;
long sizeWanted = size;
long totalFetched = 0;
boolean firstTime = TRUE;
// result found
//long vStart = 0;
//long vEnd = 0;

/* If there's stuff on the screen, skip past it. */
/* If not, skip to the edge of the window. */
if (next)
    {
    start = end;
    end = start + size;
    if (end > virtSeqBaseCount)
	end = virtSeqBaseCount;
    }
else
    {
    end = start;
    start = end - size;
    if (start < 0)
	start = 0;
    }
size = end - start;

/* Now it's time to do the search. */
if (end > start)
    // GALT TODO BIGNUM == 0x3fffffff which is about 1 billion?
    while (sizeWanted > 0 && sizeWanted < BIGNUM)
	{

	// include the original window so we can detect overlap with it.
	long xStart = start, xEnd = end;
	if (firstTime)
	    {
	    firstTime = FALSE;
	    if (next)
		{
		xStart = virtWinStart;
		}
	    else
		{
		xEnd = virtWinEnd;
		}
	    }

	// Expanding search window to xStart, xEnd, size, sizeWanted

	struct window *windows = makeWindowListFromVirtChrom(xStart, xEnd);

	totalFetched += (xEnd - xStart);

	struct window *w;
	for(w=windows; w; w=w->next)
	    {
	    for (cr=crList; cr; cr=cr->next)
		{

		if (cr->skipIt)
		    continue;

		// exon and region overlap?
		if (sameString(w->chromName, cr->chrom) && (cr->end > w->winStart) && (w->winEnd > cr->start)) // overlap
		    { // clip exon to region

		    //region overlaps?
		    int s = max(w->winStart, cr->start);
		    int e = min(w->winEnd, cr->end);
		    long vs = w->virtStart + (s - w->winStart);
		    long ve = w->virtEnd - (w->winEnd - e);

		    // exon and region overlap
		    if (cr->found)
			{ // if existing, extend its range
			cr->vStart = min(vs, cr->vStart);
			cr->vEnd = max(ve, cr->vEnd);
			}
		    else
			{ // first find
			cr->found = TRUE;
			cr->vStart = vs;
			cr->vEnd = ve;
			}

		    }
		}
	    }

	slFreeList(&windows);

	boolean vrFound = TRUE;
	for (cr=crList; cr; cr=cr->next)
	    {
	    if (!cr->found)
		vrFound = FALSE;
	    }
	if (vrFound)
	    break;

	// give up if we read this amount without finding anything
	if (totalFetched > (virtWinBaseCount+1000000)) //  max gene size supported 1MB supports most genes.
	    {
	    break;
	    }
	

try_again:
	sizeWanted *= 2;
	if (next)
	    {
	    start = end;
	    end += sizeWanted;
	    if (end > virtSeqBaseCount)
		end = virtSeqBaseCount;
	    }
	else
	    {
	    end = start;
	    start -= sizeWanted;
	    if (start < 0)
		start = 0;
	    }
	size = end - start;
	if (end == 0)
	    {
	    break;
	    }
	if (start == virtSeqBaseCount)
	    {
	    break;
	    }

	}

/* Finally, we got something. */
int saveSizeWanted = sizeWanted;
sizeWanted = virtWinEnd - virtWinStart;  // resetting sizeWanted back to the original windows span size

for (cr=crList; cr; cr=cr->next)
    {
    if (cr->found && !cr->skipIt)
	{
	if (next)
	    {
	    // if dangling off the end, need to expand search to find full span
	    if ((cr->vEnd == end) && (end < virtSeqBaseCount))
		{
		sizeWanted = saveSizeWanted;
		goto try_again;
		}
	    }
	else
	    {
	    // if dangling off the start, need to expand search to find full span
	    if ((cr->vStart == start) && (start > 0))
		{
		sizeWanted = saveSizeWanted;
		goto try_again;
		}
	    }
	if (cr->vStart < 0)
	    cr->vStart = 0;
	if (cr->vEnd > virtSeqBaseCount)
	    cr->vEnd = virtSeqBaseCount;
	}
    else
	{
	// if none found, do we just return -1
	cr->vStart = -1;
	cr->vEnd = -1;
	}
    }
}


void linkedFeaturesMoveWinStart(long exonStart, long bufferToEdge, long newWinSize, long *pNewWinStart, long *pNewWinEnd)
/* A function used by linkedFeaturesNextPrevItem to make that function */
/* easy to read. Move the window so that the start of the exon in question */
/* is near the start of the window. */
{
*pNewWinStart = exonStart - bufferToEdge;
if (*pNewWinStart < 0)
    *pNewWinStart = 0;
*pNewWinEnd = *pNewWinStart + newWinSize;
}

void linkedFeaturesMoveWinEnd(long exonEnd, long bufferToEdge, long newWinSize, long *pNewWinStart, long *pNewWinEnd)
/* A function used by linkedFeaturesNextPrevItem to make that function */
/* easy to read. Move the window so that the end of the exon in question */
/* is near the end of the browser window. */
{
*pNewWinEnd = exonEnd + bufferToEdge;
if (*pNewWinEnd > virtSeqBaseCount)
    *pNewWinEnd = virtSeqBaseCount;
*pNewWinStart = *pNewWinEnd - newWinSize;
}

#define EXONTEXTLEN 4096

static void makeExonFrameText(int exonIntronNumber, int numExons, int startPhase, int endPhase, char *buf) 
/* Write mouseover text that describes the exon's phase into buf[EXONTEXTLEN].

   Note that start/end-phases are in the direction of transcription:
   if transcript is on + strand, the start phase is the exonFrame value, and the end phase is the next exonFrame (3' on DNA) value
   if transcript is on - strand, the start phase is the previous (=3' on DNA) exonFrame and the end phase is the exonFrame */
{

if (startPhase==-1) // UTRs don't have a frame at all
    {
    safef(buf, EXONTEXTLEN, "<b>No Codon:</b> Untranslated region<br>");
    }
else
    {
    char *exonNote = "";
    boolean isNotLastExon = (exonIntronNumber<numExons);

    static const char *phasePrefix  = 
        "<b><a target=_blank href='../goldenPath/help/codonPhase.html'> <i class='fa fa-question-circle-o'></i></a></b>";

    if (isNotLastExon)
        {
        if (startPhase==endPhase)
            exonNote = ": in-frame exon";
        else
            exonNote = ": out-of-frame exon";
        safef(buf, EXONTEXTLEN, "<b>Codon phase %s :</b> start %d, end %d%s<br>", phasePrefix, startPhase, endPhase, exonNote);
        } 
    else
        {
        if (startPhase==0)
            exonNote = ": in-frame exon";
        else
            exonNote = ": out-of-frame exon";
        safef(buf, EXONTEXTLEN, "<b>Codon phase %s :</b> start %d%s<br>", phasePrefix, startPhase, exonNote);
        }
    }
}

boolean linkedFeaturesNextPrevItem(struct track *tg, struct hvGfx *hvg, void *item, int x, int y, int w, int h, boolean next)
/* Draw a mapBox over the arrow-button on an *item already in the window*. */
/* Clicking this will do one of several things: */
{
boolean result = FALSE;
struct linkedFeatures *lf = item;
struct simpleFeature *exons = lf->components;
struct simpleFeature *exon = exons;
char *nextExonText;
char *prevExonText;
long newWinSize = virtWinEnd - virtWinStart;
long bufferToEdge = 0.05 * newWinSize;
long newWinStart, newWinEnd;
int numExons = 0;
int exonIx = 0;
struct slRef *exonList = NULL, *ref;
boolean isExon = FALSE;
if (startsWith("chain", tg->tdb->type) || startsWith("lrg", tg->tdb->track))
    {
    nextExonText = trackDbSettingClosestToHomeOrDefault(tg->tdb, "nextExonText", "Next Block");
    prevExonText = trackDbSettingClosestToHomeOrDefault(tg->tdb, "prevExonText", "Prev Block");
    }
else
    {
    nextExonText = trackDbSettingClosestToHomeOrDefault(tg->tdb, "nextExonText", "Next Exon");
    prevExonText = trackDbSettingClosestToHomeOrDefault(tg->tdb, "prevExonText", "Prev Exon");
    }
if (sameString(nextExonText,"Next Exon"))
    isExon = TRUE;
while (exon != NULL)
/* Make a stupid list of exons separate from what's given. */
/* It seems like lf->components isn't necessarily sorted. */
    {
    refAdd(&exonList, exon);
    exon = exon->next;
    }
/* Now sort it. */
if (next)
    slSort(&exonList, exonSlRefCmp);
else
    slSort(&exonList, exonSlRefReverseCmp);
numExons = slCount(exonList);

// translate tall and exons
// lift params into struct for parallel processing.
struct convertRange *cr=NULL, *crList=NULL;
AllocVar(cr);
if (( next && lf->tallEnd   > winStart)
 || (!next && lf->tallStart < winEnd  ))
    {
    cr->chrom = chromName;
    cr->start = lf->tallStart;
    cr->end = lf->tallEnd;
    }
else
    {
    cr->skipIt = TRUE;
    }
slAddHead(&crList, cr);
for (ref = exonList; ref != NULL; ref = ref->next, exonIx++)
    {
    exon = ref->val;
    AllocVar(cr);
    if (( next && exon->end   > winStart)
     || (!next && exon->start < winEnd  ))
	{
	cr->chrom = chromName;
	cr->start = exon->start;
	cr->end = exon->end;
	}
    else
	{
	cr->skipIt = TRUE;
	}
    slAddHead(&crList, cr);
    }
slReverse(&crList);

// convert entire list of ranges to virt coords in parallel.
// translate gene parts in parallel
linkedFeaturesNextPrevExonFind(tg, next, crList);

if ((numExons + 1) != slCount(crList))
    errAbort("Unexpected error in linkedFeaturesNextPrevItem: numExons=%d + 1 != slCount(crList)=%d", numExons, slCount(crList));

// translate tall
cr = crList;
long xTallStart = cr->vStart, xTallEnd = cr->vEnd;

for (ref = exonList, cr=crList->next, exonIx = 0; ref != NULL; ref = ref->next, cr=cr->next, exonIx++)
    {
    char mouseOverText[256];
    boolean bigExon = FALSE;
    boolean revStrand = (lf->orientation == -1);
    exon = ref->val;

    // translate exon
    long xExonStart = cr->vStart, xExonEnd = cr->vEnd;

    if ((xExonEnd != -1) && ((xExonEnd - xExonStart) > (newWinSize - (2 * bufferToEdge))))
	bigExon = TRUE;
    if (next && (xExonEnd != -1) && (xExonEnd > virtWinEnd))
	/* right overhang (but left side of screen in reverse-strand-display) */
	{
	if (xExonStart < virtWinEnd)
	    {
	    /* not an intron hanging over edge. */
	    if ((xTallStart != -1) && (xTallStart > virtWinEnd) && (xTallStart < xExonEnd) && (xTallStart > xExonStart))
		{
		linkedFeaturesMoveWinEnd(xTallStart, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
		if (isExon)
		    {
		    nextExonText = "Coding Start of Exon";
		    prevExonText = "Coding End of Exon";
		    }
		}
	    else if ((xTallEnd != -1) && (xTallEnd > virtWinEnd) && (xTallEnd < xExonEnd) && (xTallEnd > xExonStart))
		{
		linkedFeaturesMoveWinEnd(xTallEnd, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
		if (isExon)
		    {
		    nextExonText = "Coding End of Exon";
		    prevExonText = "Coding Start of Exon";
		    }
		}
	    else
		{
		linkedFeaturesMoveWinEnd(xExonEnd, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
		if (isExon)
		    {
		    nextExonText = "End of Exon";
		    prevExonText = "Start of Exon";
		    }
		}
	    }
	else if (bigExon)
	    linkedFeaturesMoveWinStart(xExonStart, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
	else
	    linkedFeaturesMoveWinEnd(xExonEnd, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
	if (!revStrand)
	    safef(mouseOverText, sizeof(mouseOverText), "%s (%d/%d)", nextExonText, exonIx+1, numExons);
	else
	    safef(mouseOverText, sizeof(mouseOverText), "%s (%d/%d)", prevExonText, numExons-exonIx, numExons);
	mapBoxJumpTo(hvg, x, y, w, h, tg, virtChromName, newWinStart, newWinEnd, mouseOverText);
	result = TRUE;
	break;
	}
    else if (!next && (xExonStart != -1) && (xExonStart < virtWinStart))
	/* left overhang */
	{
	if (xExonEnd > virtWinStart)
	    {
	    /* not an intron hanging over the edge. */
	    if ((xTallEnd != -1) && (xTallEnd < virtWinStart) && (xTallEnd > xExonStart) && (xTallEnd < xExonEnd))
		{
		linkedFeaturesMoveWinStart(xTallEnd, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
		if (isExon)
		    {
		    nextExonText = "Coding Start of Exon";
		    prevExonText = "Coding End of Exon";
		    }
		}
	    else if ((xTallStart != -1) && (xTallStart < virtWinStart) && (xTallStart > xExonStart) && (xTallStart < xExonEnd))
		{
		linkedFeaturesMoveWinStart(xTallStart, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
		if (isExon)
		    {
		    nextExonText = "Coding End of Exon";
		    prevExonText = "Coding Start of Exon";
		    }
		}
	    else
		{
		linkedFeaturesMoveWinStart(xExonStart, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
		if (isExon)
		    {
		    nextExonText = "End of Exon";
		    prevExonText = "Start of Exon";
		    }
		}
	    }
	else if (bigExon)
	    linkedFeaturesMoveWinEnd(xExonEnd, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
	else
	    linkedFeaturesMoveWinStart(xExonStart, bufferToEdge, newWinSize, &newWinStart, &newWinEnd);
	if (!revStrand)
	    safef(mouseOverText, sizeof(mouseOverText), "%s (%d/%d)", prevExonText, numExons-exonIx, numExons);
	else
	    safef(mouseOverText, sizeof(mouseOverText), "%s (%d/%d)", nextExonText, exonIx+1, numExons);
	mapBoxJumpTo(hvg, x, y, w, h, tg, virtChromName, newWinStart, newWinEnd, mouseOverText);
	result = TRUE;
	break;
	}
    }
slFreeList(&exonList);
slFreeList(&crList);
return result;
}

void linkedFeaturesItemExonMaps(struct track *tg, struct hvGfx *hvg, void *item, double scale,
    int y, int heightPer, int sItem, int eItem,
    boolean lButton, boolean rButton, int buttonW)
/* Draw mapBoxes over exons and introns labeled with exon/intron numbers */
{
struct linkedFeatures *lf = item;
struct simpleFeature *exons = lf->components;
struct simpleFeature *exon = exons;
char *exonText, *intronText;
int numExons = 0;
int exonIx = 1;
struct slRef *exonList = NULL, *ref;
// TODO this exonText (and intronText) setting is just a made-up placeholder.
// could add a real setting name. Maybe someday extend to exon names (LRG?) instead of just exon numbers
if (startsWith("chain", tg->tdb->type) || startsWith("lrg", tg->tdb->track))
    {
    exonText   = trackDbSettingClosestToHomeOrDefault(tg->tdb, "exonText"  , "Block");
    intronText = trackDbSettingClosestToHomeOrDefault(tg->tdb, "intronText", "Gap"  ); // what really goes here for chain type?
    }
else
    {
    exonText   = trackDbSettingClosestToHomeOrDefault(tg->tdb, "exonText"  , "Exon"  );
    intronText = trackDbSettingClosestToHomeOrDefault(tg->tdb, "intronText", "Intron");
    }
while (exon != NULL)
/* Make a stupid list of exons separate from what's given. */
/* It seems like lf->components isn't necessarily sorted. */
    {
    refAdd(&exonList, exon);
    exon = exon->next;
    }
/* Now sort it. */
slSort(&exonList, exonSlRefCmp);

numExons = slCount(exonList);
struct genePred *gp = NULL;
if (startsWith("bigGenePred", tg->tdb->type) || startsWith("genePred", tg->tdb->type))
    gp = lf->original;
boolean revStrand = (lf->orientation == -1);
int eLast = -1;
int s = -1;
int e = -1;
char mouseOverText[4096];
boolean isExon = TRUE;
int picStart = insideX;
int picEnd = picStart + insideWidth;
if (lButton)
    picStart += buttonW;
if (rButton)
    picEnd -= buttonW;

for (ref = exonList; TRUE; )
    {
    exon = ref->val;
    if (isExon)
	{
	s = exon->start;
	e = exon->end;
	}
    else
	{
	s = eLast;
	e = exon->start;
	}
    // skip exons and introns that are completely outside the window
    if (s <= winEnd && e >= winStart)
	{
	int sClp = (s < winStart) ? winStart : s;
	int eClp = (e > winEnd)   ? winEnd   : e;

	int sx = round((sClp - winStart)*scale) + insideX;
	int ex = round((eClp - winStart)*scale) + insideX;

        // skip regions entirely outside available picture
        // (accounts for space taken by exon arrows buttons)
	if (sx <= picEnd && ex >= picStart)
	    {
	    // clip it to avail pic
	    sx = (sx < picStart) ? picStart : sx;
    	    ex = (ex > picEnd)   ? picEnd   : ex;

	    int w = ex - sx; // w could be negative, but we'll skip drawing if it is

	    int exonIntronNumber;
	    char *exonIntronText;
	    int numExonIntrons = numExons;
	    if (isExon)
		{
		exonIntronText = exonText;
		}
	    else
		{
		exonIntronText = intronText;
		--numExonIntrons;  // introns are one fewer than exons
		}

            char strandChar;
	    if (!revStrand) {
		exonIntronNumber = exonIx;
                strandChar = '+';
            }
	    else {
		exonIntronNumber = numExonIntrons-exonIx+1;
                strandChar = '-';
            }

            // we still need to show the existing mouseover text
            char* existingText = lf->mouseOver;
            if (isEmpty(existingText))
                existingText = lf->name;

            // construct a string that tells the user about the codon frame situation of this exon
            // char *frameText = "";
            // for coding exons, determine the start and end phase of the exon and an English text describing both:
            // if transcript is on + strand, the start phase is the exonFrame value, and the end phase is the next exonFrame (3' on DNA) value
            // if transcript is on - strand, the start phase is the previous (=3' on DNA) exonFrame and the end phase is the exonFrame
            int startPhase = -1;
            int endPhase = -1;
            char phaseText[EXONTEXTLEN];
            phaseText[0] = 0;
            if ((gp != NULL) && gp->exonFrames && isExon)
                {
                startPhase = gp->exonFrames[exonIx-1];
                if (!revStrand) 
                    endPhase = gp->exonFrames[exonIx];
                else 
                    if (exonIx>1)
                        endPhase = gp->exonFrames[exonIx-2];
                // construct a string that tells the user about the codon frame situation of this exon
                makeExonFrameText(exonIntronNumber, numExons, startPhase, endPhase, phaseText);
                }

	    if (w > 0) // draw exon or intron if width is greater than 0
		{
                // draw mapBoxes for the codons if we are zoomed in far enough
                if (isExon && lf->codons && zoomedToCdsColorLevel)
                    {
                    struct simpleFeature *codon;
                    struct dyString *codonDy = dyStringNew(0);
                    int codonS, codonE;
                    for (codon = lf->codons; codon != NULL; codon = codon->next)
                        {
                        codonS = codon->start; codonE = codon->end;
                        if (codonS > e || codonE < s)
                            continue; // only write out mouseovers for codons in the current exon
                        if (codonS <= winEnd && codonE >= winStart)
                            {
                            int codonSClp = (codonS < winStart) ? winStart : codonS;
                            int codonEClp = (codonE > winEnd)   ? winEnd   : codonE;

                            int codonsx = round((codonSClp - winStart)*scale) + insideX;
                            int codonex = round((codonEClp - winStart)*scale) + insideX;

                            // skip regions entirely outside available picture
                            // (accounts for space taken by exon arrows buttons)
                            if (codonsx <= picEnd && codonex >= picStart)
                                {
                                // clip it to avail pic
                                codonsx = (codonsx < picStart) ? picStart : codonsx;
                                codonex = (codonex > picEnd)   ? picEnd   : codonex;

                                int w = codonex - codonsx;
                                if (w > 0)
                                    {
                                    // temporarily remove the mouseOver from the lf, since linkedFeatureMapItem will always 
                                    // prefer a lf->mouseOver over the itemName
                                    char *oldMouseOver = lf->mouseOver;
                                    lf->mouseOver = NULL;
                                    dyStringClear(codonDy);
                                    // if you change this text, make sure you also change hgTracks.js:mouseOverToLabel
                                    if (!isEmpty(existingText))
                                        dyStringPrintf(codonDy, "<b>Transcript: </b> %s<br>", existingText);
                                    int codonHgvsIx = (codon->codonIndex - 1) * 3;
                                    if (codonHgvsIx >= 0)
                                        dyStringPrintf(codonDy, "<b>Codons: </b> c.%d-%d<br>", codonHgvsIx + 1, codonHgvsIx + 3);
                                    // if you change the text below, also change hgTracks:mouseOverToExon
                                    dyStringPrintf(codonDy, "<b>Strand: </b> %c<br><b>Exon: </b>%s %d of %d<br>%s",
                                                strandChar, exonIntronText, exonIntronNumber, numExonIntrons, phaseText);
                                    tg->mapItem(tg, hvg, item, codonDy->string, tg->mapItemName(tg, item),
                                            sItem, eItem, codonsx, y, w, heightPer);
                                    // and restore the mouseOver
                                    lf->mouseOver = oldMouseOver;
                                    }
                                }
                            }
                        }
                    }
                else // either an intron, or else an exon zoomed out too far for codons (or no codons)
                    {
                    // if you change this text, make sure you also change hgTracks.js:mouseOverToLabel
                    // if you change the text below, also change hgTracks:mouseOverToExon
                    char *posNote = "";
                    char *exonOrIntron = "Intron";
                    if (isExon) 
                        {
                        posNote = "<b>Codons:</b> Zoom in to show cDNA position<br>";
                        exonOrIntron = "Exon";
                        }


                    safef(mouseOverText, sizeof(mouseOverText), "<b>Transcript:</b> %s<br>%s"
                            "<b>Strand:</b> %c<br><b>%s:</b> %s %d of %d<br>%s",
                        existingText, posNote, strandChar, exonOrIntron, exonIntronText, 
                        exonIntronNumber, numExonIntrons, phaseText);

                    // temporarily remove the mouseOver from the lf, since linkedFeatureMapItem will always 
                    // prefer a lf->mouseOver over the itemName
                    char *oldMouseOver = lf->mouseOver;
                    lf->mouseOver = NULL;
                    tg->mapItem(tg, hvg, item, mouseOverText, tg->mapItemName(tg, item),
                        sItem, eItem, sx, y, w, heightPer);
                    // and restore the old mouseOver
                    lf->mouseOver = oldMouseOver;

                    picStart = ex;  // prevent pileups. is this right? add 1? does it work?
                                    // JC: Why do we care about pileups?  First mapbox drawn wins.
                    }
                }
	    }
	}

    if (isExon)
	{
    	eLast = e;
	ref = ref->next;
	if (!ref)
	    break;
	}
    else
	{
	exonIx++;
	}
    isExon = !isExon;

    if (s > winEnd) // since the rest will also be outside the window
	break;

    }

slFreeList(&exonList);
}

static struct window *makeMergedWindowList(struct window *windows)
/* Make a copy of the windows list, merging nearby regions on the same chrom
and which are within some limit (1MB?) of each other. */
{
int mergeLimit = 1024*1024;
struct window *w = windows;
struct window *resultList = NULL;
struct window *mergedW = NULL;
if (!windows)
    errAbort("Unexpected error: windows list NULL in makeMergedWindowList()");
boolean doNew = TRUE;
while(TRUE)
    {
    if (doNew)
	{
	doNew = FALSE;
	AllocVar(mergedW);
	mergedW->chromName = w->chromName;
	mergedW->winStart = w->winStart;
	mergedW->winEnd = w->winEnd;
	w = w->next;
	}
    boolean nearby = FALSE;
    if (w
	&& sameString(w->chromName, mergedW->chromName)
       	&& (w->winStart >= mergedW->winEnd)
   	&& ((w->winStart - mergedW->winEnd) <= mergeLimit)
       ) // nearby and ascending
	nearby = TRUE;
    if (!w || !nearby)
	{
	slAddHead(&resultList, mergedW);
	}
    if (!w)
	break;
    if (nearby)
	{
    	mergedW->winEnd = w->winEnd;
	w = w->next;
	}
    else // done with old, starting new
	{
	doNew = TRUE;
	}
    }

return resultList;
}

struct virtRange
    {
    struct virtRange *next;
    long vStart;
    long vEnd;
    char *item;
    };

int vrCmp(const void *va, const void *vb)
/* Compare to sort based on chromStart. */
{
const struct virtRange *a = *((struct virtRange **)va);
const struct virtRange *b = *((struct virtRange **)vb);
long dif = a->vStart - b->vStart;
if (dif > 0) return 1;
if (dif < 0) return -1;
return 0;
}

int vrCmpEnd(const void *va, const void *vb)
/* Compare to sort based on chromEnd. */
{
const struct virtRange *a = *((struct virtRange **)va);
const struct virtRange *b = *((struct virtRange **)vb);
long dif = a->vEnd - b->vEnd;
if (dif > 0) return 1;
if (dif < 0) return -1;
return 0;
}


void linkedFeaturesLabelNextPrevItem(struct track *tg, boolean next)
/* Default next-gene function for linkedFeatures.  Changes winStart/winEnd
 * and updates position in cart. */
{

long start = virtWinStart;
long end = virtWinEnd;
long size = virtWinBaseCount;
long sizeWanted = size;
long bufferToEdge;
struct virtRange *vrList = NULL;

/* If there's stuff on the screen, skip past it. */
/* If not, skip to the edge of the window. */
if (next)
    {
    start = end;
    end = start + size;
    if (end > virtSeqBaseCount)
	end = virtSeqBaseCount;
    }
else
    {
    end = start;
    start = end - size;
    if (start < 0)
	start = 0;
    }
size = end - start;

struct hash *hash = newHash(8);
boolean firstTime = TRUE;

/* Now it's time to do the search. */
if (end > start)
    // GALT TODO BIGNUM == 0x3fffffff which is about 1 billion?
    for ( ; sizeWanted > 0 && sizeWanted < BIGNUM; )
	{

	// Expanding search window to start, end, size, sizeWanted

	// include the original window so we can detect overlap with it.
	long xStart = start, xEnd = end;
	if (firstTime)
	    {
	    firstTime = FALSE;
	    if (next)
		{
		xStart = virtWinStart;
		}
	    else
		{
		xEnd = virtWinEnd;
		}
	    }

	struct window *windows = makeWindowListFromVirtChrom(xStart, xEnd);

	// To optimize when using tiny exon-like regions, merge nearby regions.
	struct window *mergedWindows = makeMergedWindowList(windows);

	struct window *mw;
	for(mw=mergedWindows; mw; mw=mw->next)
	    {

	    struct bed *items = NULL;

#ifndef GBROWSE
	    if (sameWord(tg->table, WIKI_TRACK_TABLE))
		items = wikiTrackGetBedRange(tg->table, mw->chromName, mw->winStart, mw->winEnd);
	    else if (startsWith("longTabix", tg->tdb->type))
		items = loadLongTabixAsBed(tg, mw->chromName, mw->winStart, mw->winEnd);
	    else if (sameWord(tg->table, "gvPos"))
		items = loadGvAsBed(tg, mw->chromName, mw->winStart, mw->winEnd);
	    else if (sameWord(tg->table, "oreganno"))
		items = loadOregannoAsBed(tg, mw->chromName, mw->winStart, mw->winEnd);
	    else if (tg->isBigBed)
		items = loadBigBedAsBed(tg, mw->chromName, mw->winStart, mw->winEnd);
	    else
#endif /* GBROWSE */
		{
		if (isCustomTrack(tg->table))
		    {
		    struct customTrack *ct = tg->customPt;
		    items = hGetCtBedRange(CUSTOM_TRASH, database, ct->dbTableName, mw->chromName, mw->winStart, mw->winEnd, NULL);
		    }
		else
		    items = hGetBedRange(database, tg->table, mw->chromName, mw->winStart, mw->winEnd, NULL);
		}

	    struct bed *item;
	    while ((item = slPopHead(&items)) != NULL)
		{
		item->next = NULL;


		struct window *w;
		for(w=windows; w; w=w->next)
		    {

		    // item and region overlap?
		    if ((item->chromEnd > w->winStart) && (w->winEnd > item->chromStart)) // overlap
			{ // clip item to region
			int s = max(w->winStart, item->chromStart);
			int e = min(w->winEnd, item->chromEnd);
			long vs = w->virtStart + (s - w->winStart);
			long ve = w->virtEnd - (w->winEnd - e);

			// NOTE TODO should this key string code should be copied from the pack routine?
			//  When I tried the pack code, mapItemName method was crashing, not sure why.
			char key[1024];
			safef(key, sizeof key, "%s:%d-%d %s", item->chrom, item->chromStart, item->chromEnd, item->name);

			struct hashEl *hel = hashLookup(hash, key);
			struct virtRange *vr;
			if (hel)
			    { // if existing, extend its range
			    vr = hel->val;
			    vr->vStart = min(vs, vr->vStart);
			    vr->vEnd = max(ve, vr->vEnd);
			    }
			else
			    { // create new vr
			    AllocVar(vr);
			    vr->vStart = vs;
			    vr->vEnd = ve;
			    vr->item = cloneString(item->name);
			    vr->next = NULL;
			    hashAdd(hash, key, vr);
			    }
			
			}
		    }
		bedFree(&item);
		}


	    }

	slFreeList(&mergedWindows);
	slFreeList(&windows);

	vrList = NULL;
	// create a list of vrs from hash
	struct hashCookie cookie = hashFirst(hash);
	struct hashEl *hel;
	while ((hel = hashNext(&cookie)) != NULL)
	    {
	    slAddHead(&vrList, hel->val);
	    //hel->val = NULL; // keep for reuse
	    }
	slReverse(&vrList);  // probably not needed.

	/* If we got something, or weren't able to search as big as we wanted to */
	/* (in case we're at the end of the chrom).  */
	if (vrList != NULL || (size < sizeWanted))
	    {
	    /* Remove the ones that were on the original screen. */
	    struct virtRange *vr;
	    struct virtRange *goodList = NULL;
	    while ((vr = slPopHead(&vrList)) != NULL)
		{
		vr->next = NULL;
		if ((vr->vEnd > virtWinStart) && (virtWinEnd > vr->vStart))
		    {
		    }
		else
		    slAddHead(&goodList, vr);
		}
	    if (goodList)
		{
		slReverse(&goodList);
		vrList = goodList;
		break;
		}
	    }

try_again:
	sizeWanted *= 2;
	if (next)
	    {
	    start = end;
	    end += sizeWanted;
	    if (end > virtSeqBaseCount)
		end = virtSeqBaseCount;
	    }
	else
	    {
	    end = start;
	    start -= sizeWanted;
	    if (start < 0)
		start = 0;
	    }
	size = end - start;
	if (end == 0)
	    {
	    vrList = NULL;
	    break;
	    }
	if (start == virtSeqBaseCount)
	    {
	    vrList = NULL;
	    break;
	    }

	}


/* Finally, we got something. */
int saveSizeWanted = sizeWanted;
sizeWanted = virtWinEnd - virtWinStart;  // resetting sizeWanted back to the original windows span size
bufferToEdge = (long)(0.05 * (float)sizeWanted);
if (vrList)
    {
    struct virtRange *vr = NULL;
    if (next)
	{
	slSort(&vrList, vrCmp);
	vr = vrList;  // first one is nearest
	// if dangling off the end, need to expand search to find full span
        if ((vr->vEnd == end) && (end < virtSeqBaseCount))
	    {
	    sizeWanted = saveSizeWanted;
	    goto try_again;
	    }
	if (vr->vEnd + bufferToEdge - sizeWanted < virtWinEnd)
	    {
	    // case 1, item end plus 5%% buffer within one screen width ahead
	    virtWinEnd = vr->vEnd + bufferToEdge;
	    virtWinStart = virtWinEnd - sizeWanted;
	    }
	else if (vr->vStart + bufferToEdge - sizeWanted < virtWinEnd)
	    {
	    // case 2, (puzzling) item start plus 5%% buffer within one screen width ahead
	    virtWinEnd = vr->vStart + bufferToEdge;
	    virtWinStart = virtWinEnd - sizeWanted;
	    }
	else
	    {
	    // case 3, default. shift window to item start minus 5%% buffer
	    virtWinStart = vr->vStart - bufferToEdge;
	    virtWinEnd = virtWinStart + sizeWanted;
	    }
	}
    else
	{
	slSort(&vrList, vrCmpEnd);
	slReverse(&vrList);
	vr = vrList;  // first one is nearest
	// if dangling off the start, need to expand search to find full span
        if ((vr->vStart == start) && (start > 0))
	    {
	    sizeWanted = saveSizeWanted;
	    goto try_again;
	    }
	if (vr->vStart - bufferToEdge + sizeWanted > virtWinStart)
	    {
	    // case 1, item start minus 5%% buffer within one screen width behind
	    virtWinStart = vr->vStart - bufferToEdge;
	    virtWinEnd = virtWinStart + sizeWanted;
	    }
        else if (vr->vEnd - bufferToEdge + sizeWanted > virtWinStart)
	    {
	    // case 2, (puzzling) item end minus 5%% buffer within one screen width behind
	    virtWinStart = vr->vEnd - bufferToEdge;
	    virtWinEnd = virtWinStart + sizeWanted;
	    }
        else
	    {
	    // case 3, default. shift window to item end minus 5%% buffer
	    virtWinEnd = vr->vEnd + bufferToEdge;
	    virtWinStart = virtWinEnd - sizeWanted;
	    }
	}
    if (virtWinEnd > virtSeqBaseCount)
	virtWinEnd = virtSeqBaseCount;
    if (virtWinStart < 0)
	virtWinStart = 0;

    virtWinBaseCount = virtWinEnd - virtWinStart;

    }

// Because nextItem now gets called earlier (in tracksDisplay),
// before trackList gets duplicated in trackForm,
// we do not want this track to have open handles and resources
// be duplicated later for each window. Close resources as needed.
tg->items = NULL;
#ifndef GBROWSE
if (tg->isBigBed)
    bbiFileClose(&tg->bbiFile);
#endif /* GBROWSE */

// free the hash and its values?
struct hashCookie cookie = hashFirst(hash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct virtRange *vr = hel->val;
    freeMem(vr->item);
    freeMem(vr);
    }

freeHash(&hash);

}

enum {blackShadeIx=9,whiteShadeIx=0};

char *getScoreFilterClause(struct cart *cart,struct trackDb *tdb,char *scoreColumn)
// Returns "score >= ..." extra where clause if one is needed
{
if (scoreColumn == NULL)
    scoreColumn = "score";

struct dyString *extraWhere = dyStringNew(128);
boolean and = FALSE;
// gets trackDb 'filterBy' clause, which may filter by 'score', 'name', etc
extraWhere = dyAddFilterByClause(cart,tdb,extraWhere,NULL,&and);
extraWhere = dyAddAllScoreFilters(cart,tdb,extraWhere,&and); // All *Filter style filters
if (and == FALSE      // Cannot have both 'filterBy' score and 'scoreFilter'
||  strstrNoCase(extraWhere->string,"score in ") == NULL)
    extraWhere = dyAddFilterAsInt(cart,tdb,extraWhere,SCORE_FILTER,"0:1000",scoreColumn,&and);
if (sameString(extraWhere->string, ""))
    return NULL;
return dyStringCannibalize(&extraWhere);
}


void loadLinkedFeaturesWithLoaders(struct track *tg, struct slList *(*itemLoader)(char **row),
                                   struct linkedFeatures *(*lfFromWhatever)(struct slList *item),
                                   char *scoreColumn, char *moreWhere,
                                   boolean (*itemFilter)(struct slList *item))
/* Make a linkedFeatures loader by providing three functions: (1) a regular */
/* item loader found in all autoSql modules, (2) a custom myStruct->linkedFeatures */
/* translating function, and (3) a function to free the the thing loaded in (1). */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int rowOffset;
struct linkedFeatures *lfList = NULL;
char extraWhere[256] ;
/* Use tg->tdb->track because subtracks inherit composite track's tdb
 * by default, and the variable is named after the composite track. */
if ((scoreColumn != NULL) && (cartVarExistsAnyLevel(cart, tg->tdb, FALSE, SCORE_FILTER)))
    {
    char *scoreFilterClause = getScoreFilterClause(cart, tg->tdb,scoreColumn);
    if (scoreFilterClause != NULL)
        {
        if (moreWhere)
            sqlSafef(extraWhere, sizeof(extraWhere), "%-s and %-s", scoreFilterClause, moreWhere);
        else
            sqlSafef(extraWhere, sizeof(extraWhere), "%-s", scoreFilterClause);
        freeMem(scoreFilterClause);
        sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, extraWhere, &rowOffset);
        }
    }
else
    sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, moreWhere, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct slList *item = itemLoader(row + rowOffset);
    if ((itemFilter == NULL) || (itemFilter(item) == TRUE))
	{
	struct linkedFeatures *lf = lfFromWhatever(item);
	slAddHead(&lfList, lf);
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}

char *linkedFeaturesSeriesName(struct track *tg, void *item)
/* Return name of item */
{
struct linkedFeaturesSeries *lfs = item;
return lfs->name;
}

int linkedFeaturesSeriesCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct linkedFeaturesSeries *a = *((struct linkedFeaturesSeries **)va);
const struct linkedFeaturesSeries *b = *((struct linkedFeaturesSeries **)vb);
return a->start - b->start;
}

void freeLinkedFeaturesSeries(struct linkedFeaturesSeries **pList)
/* Free up a linked features series list. */
{
struct linkedFeaturesSeries *lfs;

for (lfs = *pList; lfs != NULL; lfs = lfs->next)
    linkedFeaturesFreeList(&lfs->features);
slFreeList(pList);
}

void freeLinkedFeaturesSeriesItems(struct track *tg)
/* Free up linkedFeaturesSeriesTrack items. */
{
freeLinkedFeaturesSeries((struct linkedFeaturesSeries**)(&tg->items));
}

void linkedFeaturesToLinkedFeaturesSeries(struct track *tg)
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

void linkedFeaturesSeriesToLinkedFeatures(struct track *tg)
/* Convert a linked features series struct to a linked features struct */
{
struct linkedFeaturesSeries *lfs;
struct linkedFeatures *lfList = NULL;

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

Color veryLightGrayIndex()
/* Return index of very light gray. */
{
return shadesOfGray[2];
}


void makeGrayShades(struct hvGfx *hvg)
/* Make eight shades of gray in display. */
{
hMakeGrayShades(hvg, shadesOfGray, maxShade);
shadesOfGray[maxShade+1] = MG_RED;
}

void makeBrownShades(struct hvGfx *hvg)
/* Make some shades of brown in display. */
{
hvGfxMakeColorGradient(hvg, &tanColor, &brownColor, maxShade+1, shadesOfBrown);
}

void makeSeaShades(struct hvGfx *hvg)
/* Make some shades of blue in display. */
{
hvGfxMakeColorGradient(hvg, &lightSeaColor, &darkSeaColor, maxShade+1, shadesOfSea);
}

void initColors(struct hvGfx *hvg)
/* Initialize the shades of gray etc. */
{
makeGrayShades(hvg);
makeBrownShades(hvg);
makeSeaShades(hvg);
orangeColor = hvGfxFindColorIx(hvg, 230, 130, 0);
brickColor = hvGfxFindColorIx(hvg, 230, 50, 110);
blueColor = hvGfxFindColorIx(hvg, 0,114,198);
darkBlueColor = hvGfxFindColorIx(hvg, 0,70,140);
greenColor = hvGfxFindColorIx(hvg, 28,206,40);
darkGreenColor = hvGfxFindColorIx(hvg, 28,140,40);
}

void makeRedGreenShades(struct hvGfx *hvg)
/* Allocate the shades of Red, Green, Blue and Yellow for expression tracks */
{
static struct rgbColor black = {0, 0, 0, 255};
static struct rgbColor red = {255, 0, 0, 255};
static struct rgbColor green = {0, 255, 0, 255};
static struct rgbColor blue = {0, 0, 255, 255};
static struct rgbColor yellow = {255, 255, 0, 255};
static struct rgbColor white  = {255, 255, 255, 255};
hvGfxMakeColorGradient(hvg, &black, &blue, EXPR_DATA_SHADES, shadesOfBlue);
hvGfxMakeColorGradient(hvg, &black, &red, EXPR_DATA_SHADES, shadesOfRed);
hvGfxMakeColorGradient(hvg, &black, &green, EXPR_DATA_SHADES, shadesOfGreen);
hvGfxMakeColorGradient(hvg, &black, &yellow, EXPR_DATA_SHADES, shadesOfYellow);
hvGfxMakeColorGradient(hvg, &white, &blue,   EXPR_DATA_SHADES, shadesOfBlueOnWhite);
hvGfxMakeColorGradient(hvg, &white, &red,    EXPR_DATA_SHADES, shadesOfRedOnWhite);
hvGfxMakeColorGradient(hvg, &white, &green,  EXPR_DATA_SHADES, shadesOfGreenOnWhite);
hvGfxMakeColorGradient(hvg, &white, &yellow, EXPR_DATA_SHADES, shadesOfYellowOnWhite);
hvGfxMakeColorGradient(hvg, &yellow, &blue,   EXPR_DATA_SHADES, shadesOfBlueOnYellow);
hvGfxMakeColorGradient(hvg, &yellow, &red,    EXPR_DATA_SHADES, shadesOfRedOnYellow);
exprBedColorsMade = TRUE;
}

Color getOrangeColor()
{
return orangeColor;
}

Color getBrickColor()
{
return brickColor;
}

Color getBlueColor()
{
return blueColor;
}

Color getGreenColor()
{
return greenColor;
}

/* at windows >250Kbp, darken chrom break colors on MAF display */
#define CHROM_BREAK_DARK_COLOR_ZOOM 200000

Color getChromBreakGreenColor()
{
return (winEnd-winStart > CHROM_BREAK_DARK_COLOR_ZOOM ?
            darkGreenColor : greenColor);
}

Color getChromBreakBlueColor()
{
return (winEnd-winStart > CHROM_BREAK_DARK_COLOR_ZOOM ?
            darkBlueColor : blueColor);
}

/*	See inc/chromColors.h for color defines	*/
static void makeScaffoldShades(struct hvGfx *hvg)
/* Allocate 10 colors for arbitrary scaffold/contig coloring */
{
    /*	color zero is for error conditions only	*/
scafColor[0] = hvGfxFindColorIx(hvg, 0, 0, 0);
scafColor[1] = hvGfxFindColorIx(hvg, SCAF1_R, SCAF1_G, SCAF1_B);
scafColor[2] = hvGfxFindColorIx(hvg, SCAF2_R, SCAF2_G, SCAF2_B);
scafColor[3] = hvGfxFindColorIx(hvg, SCAF3_R, SCAF3_G, SCAF3_B);
scafColor[4] = hvGfxFindColorIx(hvg, SCAF4_R, SCAF4_G, SCAF4_B);
scafColor[5] = hvGfxFindColorIx(hvg, SCAF5_R, SCAF5_G, SCAF5_B);
scafColor[6] = hvGfxFindColorIx(hvg, SCAF6_R, SCAF6_G, SCAF6_B);
scafColor[7] = hvGfxFindColorIx(hvg, SCAF7_R, SCAF7_G, SCAF7_B);
scafColor[8] = hvGfxFindColorIx(hvg, SCAF8_R, SCAF8_G, SCAF8_B);
scafColor[9] = hvGfxFindColorIx(hvg, SCAF9_R, SCAF9_G, SCAF9_B);
scafColor[10] = hvGfxFindColorIx(hvg, SCAF10_R, SCAF10_G, SCAF10_B);
scafColorsMade = TRUE;
}

/*	See inc/chromColors.h for color defines	*/
void makeChromosomeShades(struct hvGfx *hvg)
/* Allocate the  shades of 8 colors in 3 shades to cover 24 chromosomes  */
{
    /*	color zero is for error conditions only	*/
chromColor[0] = hvGfxFindColorIx(hvg, 0, 0, 0);
chromColor[1] = hvGfxFindColorIx(hvg, CHROM_1_R, CHROM_1_G, CHROM_1_B);
chromColor[2] = hvGfxFindColorIx(hvg, CHROM_2_R, CHROM_2_G, CHROM_2_B);
chromColor[3] = hvGfxFindColorIx(hvg, CHROM_3_R, CHROM_3_G, CHROM_3_B);
chromColor[4] = hvGfxFindColorIx(hvg, CHROM_4_R, CHROM_4_G, CHROM_4_B);
chromColor[5] = hvGfxFindColorIx(hvg, CHROM_5_R, CHROM_5_G, CHROM_5_B);
chromColor[6] = hvGfxFindColorIx(hvg, CHROM_6_R, CHROM_6_G, CHROM_6_B);
chromColor[7] = hvGfxFindColorIx(hvg, CHROM_7_R, CHROM_7_G, CHROM_7_B);
chromColor[8] = hvGfxFindColorIx(hvg, CHROM_8_R, CHROM_8_G, CHROM_8_B);
chromColor[9] = hvGfxFindColorIx(hvg, CHROM_9_R, CHROM_9_G, CHROM_9_B);
chromColor[10] = hvGfxFindColorIx(hvg, CHROM_10_R, CHROM_10_G, CHROM_10_B);
chromColor[11] = hvGfxFindColorIx(hvg, CHROM_11_R, CHROM_11_G, CHROM_11_B);
chromColor[12] = hvGfxFindColorIx(hvg, CHROM_12_R, CHROM_12_G, CHROM_12_B);
chromColor[13] = hvGfxFindColorIx(hvg, CHROM_13_R, CHROM_13_G, CHROM_13_B);
chromColor[14] = hvGfxFindColorIx(hvg, CHROM_14_R, CHROM_14_G, CHROM_14_B);
chromColor[15] = hvGfxFindColorIx(hvg, CHROM_15_R, CHROM_15_G, CHROM_15_B);
chromColor[16] = hvGfxFindColorIx(hvg, CHROM_16_R, CHROM_16_G, CHROM_16_B);
chromColor[17] = hvGfxFindColorIx(hvg, CHROM_17_R, CHROM_17_G, CHROM_17_B);
chromColor[18] = hvGfxFindColorIx(hvg, CHROM_18_R, CHROM_18_G, CHROM_18_B);
chromColor[19] = hvGfxFindColorIx(hvg, CHROM_19_R, CHROM_19_G, CHROM_19_B);
chromColor[20] = hvGfxFindColorIx(hvg, CHROM_20_R, CHROM_20_G, CHROM_20_B);
chromColor[21] = hvGfxFindColorIx(hvg, CHROM_21_R, CHROM_21_G, CHROM_21_B);
chromColor[22] = hvGfxFindColorIx(hvg, CHROM_22_R, CHROM_22_G, CHROM_22_B);
chromColor[23] = hvGfxFindColorIx(hvg, CHROM_X_R, CHROM_X_G, CHROM_X_B);
chromColor[24] = hvGfxFindColorIx(hvg, CHROM_Y_R, CHROM_Y_G, CHROM_Y_B);
chromColor[25] = hvGfxFindColorIx(hvg, CHROM_M_R, CHROM_M_G, CHROM_M_B);
chromColor[26] = hvGfxFindColorIx(hvg, CHROM_Un_R, CHROM_Un_G, CHROM_Un_B);

chromosomeColorsMade = TRUE;
}

void rFindSubtrackColors(struct hvGfx *hvg, struct track *trackList)
/* Find colors of subtracks and their children */
{
struct track *subtrack;
for (subtrack = trackList; subtrack != NULL; subtrack = subtrack->next)
    {
    if (!isSubtrackVisible(subtrack))
        continue;
    subtrack->ixColor = hvGfxFindRgb(hvg, &subtrack->color);
    subtrack->ixAltColor = hvGfxFindRgb(hvg, &subtrack->altColor);
    rFindSubtrackColors(hvg, subtrack->subtracks);
    }
}

void findTrackColors(struct hvGfx *hvg, struct track *trackList)
/* Find colors to draw in. */
{
struct track *track;
for (track = trackList; track != NULL; track = track->next)
    {
    if (track->limitedVis != tvHide)
	{
	track->ixColor = hvGfxFindRgb(hvg, &track->color);
	track->ixAltColor = hvGfxFindRgb(hvg, &track->altColor);
	rFindSubtrackColors(hvg, track->subtracks);
        }
    }
}

int grayInRange(int val, int minVal, int maxVal)
/* Return gray shade corresponding to a number from minVal - maxVal */
{
return hGrayInRange(val, minVal, maxVal, maxShade);
}


int percentGrayIx(int percent)
/* Return gray shade corresponding to a number from 50 - 100 */
{
return grayInRange(percent, 50, 100);
}



static int cmpLfsWhiteToBlack(const void *va, const void *vb)
/* Help sort from white to black. */
{
const struct linkedFeaturesSeries *a = *((struct linkedFeaturesSeries **)va);
const struct linkedFeaturesSeries *b = *((struct linkedFeaturesSeries **)vb);
return a->grayIx - b->grayIx;
}

static int cmpLfWhiteToBlack(const void *va, const void *vb)
/* Help sort from white to black. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
int diff = a->filterColor - b->filterColor;
if (diff == 0)
    diff = a->grayIx - b->grayIx;
return diff;
}

int linkedFeaturesCmpName(const void *va, const void *vb)
/* Help sort linkedFeatures by query name. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
return strcmp(a->name, b->name);
}

int linkedFeaturesCmpStart(const void *va, const void *vb)
/* Help sort linkedFeatures by starting pos. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
return a->start - b->start;
}

void clippedBarbs(struct hvGfx *hvg, int x, int y, int width,
                  int barbHeight, int barbSpacing, int barbDir, Color color, boolean needDrawMiddle)
/* Draw barbed line.  Clip it to fit the window first though since
 * some barbed lines will span almost the whole chromosome, and the
 * clipping at the lower level is not efficient since we added
 * PostScript output support. */
{
int x2 = x + width;

if (barbDir == 0)
    return;

x = max(x, hvg->clipMinX);
x2 = min(x2, hvg->clipMaxX);
width = x2 - x;
if (width > 0)
    hvGfxBarbedHorizontalLine(hvg, x, y, width, barbHeight, barbSpacing, barbDir,
	    color, needDrawMiddle);
}

void innerLine(struct hvGfx *hvg, int x, int y, int w, Color color)
/* Draw a horizontal line of given width minus a pixel on either
 * end.  This pixel is needed for PostScript only, but doesn't
 * hurt elsewhere. */
{
if (w > 1)
   {
   /* Do some clipping here for the benefit of
    * PostScript.   Illustrator has
    * problems if you don't do this when you save
    * as web for some reason.  Can't hurt though in
    * a perfect world it would not be necessary, and
    * it's not necessary for ghostView. */
   int x1 = x+1;
   int x2 = x + w - 1;
   if (x1 < 0) x1 = 0;
   if (x2 > hvg->width) x2 = hvg->width;
   if (x2-x1 > 0)
       hvGfxLine(hvg, x1, y, x2, y, color);
   }
}

static void lfColors(struct track *tg, struct linkedFeatures *lf,
        struct hvGfx *hvg, Color *retColor, Color *retBarbColor)
/* Figure out color to draw linked feature in. */
{
if (!((lf->isBigGenePred) ||(lf->filterColor == 0)|| (lf->filterColor == -1)))
    {
    if (lf->useItemRgb)
	{
        *retColor = *retBarbColor = bedColorToGfxColor(lf->filterColor);
	}
    else
        // When does this case happen?  Why isn't this color also being parsed like above?
	*retColor = *retBarbColor = lf->filterColor;
    }
else if (tg->itemColor)
    {
    *retColor = tg->itemColor(tg, lf, hvg);
    *retBarbColor = tg->ixAltColor;
    }
else if (tg->colorShades)
    {
    boolean isXeno = (tg->subType == lfSubXeno)
                                || (tg->subType == lfSubChain)
                                || startsWith("mrnaBla", tg->table);
    *retColor =  tg->colorShades[lf->grayIx+isXeno];
    *retBarbColor =  tg->colorShades[lf->grayIx];
    }
else
    {
    *retColor = tg->ixColor;
    *retBarbColor = tg->ixAltColor;
    }
}

Color bigGenePredColor(struct track *tg, void *item,  struct hvGfx *hvg)
/* Determine the color of the name for the bigGenePred linked feature. */
{
struct linkedFeatures *lf = item;
return bedColorToGfxColor(lf->filterColor);
}

Color blackItemNameColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Force item name (label) color to black */
{
return hvGfxFindColorIx(hvg, 0, 0, 0);
}

Color linkedFeaturesNameColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Determine the color of the name for the linked feature. */
{
Color col, barbCol;
lfColors(tg, item, hvg, &col, &barbCol);
// Draw the item name fully opaque even even the item itself is drawn with alpha
struct rgbColor rgb = hvGfxColorIxToRgb(hvg, col);
rgb.a = 255;
return hvGfxFindRgb(hvg, &rgb);
}

struct simpleFeature *simpleFeatureCloneList(struct simpleFeature *list)
/* Just copies the simpleFeature list. This is good for making a copy */
/* when the codon list is made. */
{
struct simpleFeature *ret = NULL;
struct simpleFeature *cur;
for (cur = list; cur != NULL; cur = cur->next)
    {
    struct simpleFeature *newSf = NULL;
    AllocVar(newSf);
    newSf->start = cur->start;
    newSf->end = cur->end;
    newSf->qStart = cur->qStart;
    newSf->qEnd = cur->qEnd;
    newSf->grayIx = cur->grayIx;
    slAddHead(&ret, newSf);
    }
slReverse(&ret);
return ret;
}

void lfDrawSpecialGaps(struct linkedFeatures *lf,
		       int intronGap, boolean chainLines, int gapFactor,
		       struct track *tg, struct hvGfx *hvg, int xOff, int y,
		       double scale, Color color, Color bColor,
		       enum trackVisibility vis)
/* If applicable, draw something special for the gap following this block.
 * If intronGap has been specified, draw exon arrows only if the target gap
 * length is at least intronGap.
 * If chainLines, draw a double-line gap if both target and query have a gap
 * (mismatching sequence). */
{
struct simpleFeature *sf = NULL;
int heightPer = tg->heightPer;
int midY = y + (heightPer>>1);
if (! ((intronGap > 0) || chainLines))
    return;
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    if (sf->next != NULL)
	{
	int s, e, qGap, tGap;
	int x1, x2, w;
	if (sf->start >= sf->next->end)
	    {
	    s = sf->next->end;
	    e = sf->start;
	    }
	else
	    {
	    s = sf->end;
	    e = sf->next->start;
	    }
	if (rangeIntersection(winStart, winEnd, s, e) <= 0)
	    continue;
	tGap = e - s;
        if (s < winStart)
            s = winStart;
        if (e > winEnd)
            e = winEnd;
	if (sf->qStart >= sf->next->qEnd)
	    qGap = sf->qStart - sf->next->qEnd;
	else
	    qGap = sf->next->qStart - sf->qEnd;

	x1 = round((double)((int)s-winStart)*scale) + xOff;
	x2 = round((double)((int)e-winStart)*scale) + xOff;
	w = x2 - x1;
	if (chainLines && tGap > 0)
	    {
	    /* Compensate for innerLine's lopping off of a pixel at each end: */
	    x1 -= 1;
	    w += 2;
            /* If the gap in the target is more than gapFactor times the gap
             * in the query we draw only one line, otherwise two. */
	    if (qGap == 0 || (gapFactor > 0 && tGap > gapFactor * qGap))
		innerLine(hvg, x1, midY, w, color);
	    else
		{
		int midY1 = midY - (heightPer>>2);
		int midY2 = midY + (heightPer>>2);
		if (chainLines && (vis == tvSquish))
		    {
		    midY1 = y;
		    midY2 = y + heightPer - 1;
		    }
		innerLine(hvg, x1, midY1, w, color);
		innerLine(hvg, x1, midY2, w, color);
		}
	    }
	if ((vis == tvFull || vis == tvPack) && (intronGap && (qGap == 0) && (tGap >= intronGap)))
	    {
            clippedBarbs(hvg, x1, midY, w, tl.barbHeight, tl.barbSpacing,
			 lf->orientation, bColor, FALSE);
	    }
        }
    }
}

/* Rule of thumb for displaying chain gaps: consider a valid double-sided
 * gap if target side is at most 5 times greater than query side. */
#define CHAIN_GAP_FACTOR 5

void linkedFeaturesDrawAt(struct track *tg, void *item,
                          struct hvGfx *hvg, int xOff, int y, double scale,
                          MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single simple bed item at position. */
{
struct linkedFeatures *lf = item;
struct simpleFeature *sf, *components;
int heightPer = tg->heightPer;
int x1,x2;
int shortOff = heightPer/4;
int shortHeight = heightPer - 2*shortOff;
int tallStart, tallEnd, s, e, e2, s2;
Color bColor;
int intronGap = 0;
boolean chainLines = ((vis != tvDense)&&(tg->subType == lfSubChain));
int gapFactor = CHAIN_GAP_FACTOR;
boolean hideLine = ((tg->subType == lfSubChain) ||
	        ((vis == tvDense) && (tg->subType == lfSubXeno)));
boolean hideArrows = hideLine;
int midY = y + (heightPer>>1);
int w;
char *exonArrowsDense = trackDbSetting(tg->tdb, "exonArrowsDense");
boolean exonArrowsEvenWhenDense = (exonArrowsDense != NULL &&
				   !sameWord(exonArrowsDense, "off"));
boolean exonArrows = (tg->exonArrows &&
		      (vis != tvDense || exonArrowsEvenWhenDense));
boolean exonArrowsAlways = tg->exonArrowsAlways;
struct psl *psl = NULL;
struct dnaSeq *qSeq = NULL;
int qOffset = 0;
enum baseColorDrawOpt drawOpt = baseColorDrawOff;
Color saveColor = color;
boolean indelShowDoubleInsert, indelShowQueryInsert, indelShowPolyA;
if (((vis == tvFull) || (vis == tvPack)) && (tg->subType == lfNoIntronLines))
    {
    hideLine = TRUE;
    hideArrows = FALSE;
    }
indelEnabled(cart, tg->tdb, basesPerPixel, &indelShowDoubleInsert, &indelShowQueryInsert,
	     &indelShowPolyA);
if (indelShowDoubleInsert && !hideLine)
    {
    /* If enabled and we weren't already suppressing the default line,
     * show chain-like lines (single/double gap lines) but without the
     * chain track's gapFactor: */
    chainLines = TRUE;
    hideLine = TRUE;
    gapFactor = 0;
    }

/*if we are zoomed in far enough, look to see if we are coloring
  by codon, and setup if so.*/
if (vis != tvDense)
    {
    drawOpt = baseColorDrawSetup(hvg, tg, lf, &qSeq, &qOffset, &psl);
    if (drawOpt > baseColorDrawOff)
	exonArrows = FALSE;
    }
if ((tg->tdb != NULL) && (vis != tvDense))
    intronGap = atoi(trackDbSettingOrDefault(tg->tdb, "intronGap", "0"));

lfColors(tg, lf, hvg, &color, &bColor);
if (vis == tvDense && trackDbSetting(tg->tdb, EXP_COLOR_DENSE))
    color = saveColor;

color = colorFromCart(tg, color);

struct genePred *gp = NULL;
if (startsWith("genePred", tg->tdb->type) || startsWith("bigGenePred", tg->tdb->type))
    gp = (struct genePred *)(lf->original);

boolean baseColorNeedsCodons = (drawOpt == baseColorDrawItemCodons ||
				drawOpt == baseColorDrawDiffCodons ||
				drawOpt == baseColorDrawGenomicCodons);
if (psl && baseColorNeedsCodons)
    {
    boolean isXeno = ((tg->subType == lfSubXeno) || (tg->subType == lfSubChain) ||
		      startsWith("mrnaBla", tg->table));
    int sizeMul = pslIsProtein(psl) ? 3 : 1;
    lf->codons = baseColorCodonsFromPsl(lf, psl, sizeMul, isXeno, maxShade, drawOpt, tg);
    }
else if (drawOpt > baseColorDrawOff)
    {
    if (gp && gp->cdsStart != gp->cdsEnd)
        lf->codons = baseColorCodonsFromGenePred(lf, gp, (drawOpt != baseColorDrawDiffCodons), cartUsualBooleanClosestToHome(cart, tg->tdb, FALSE, CODON_NUMBERING_SUFFIX, TRUE));
    }
if (psl && drawOpt == baseColorDrawCds && !zoomedToCdsColorLevel)
    baseColorSetCdsBounds(lf, psl, tg);

tallStart = lf->tallStart;
tallEnd = lf->tallEnd;
if ((tallStart == 0 && tallEnd == 0) && lf->start != 0 && !sameWord(tg->table, "jaxQTL3"))
    {
    // sometimes a bed <8 will get passed off as a bed 8, tsk tsk
    tallStart = lf->start;
    tallEnd   = lf->end;
    }
int ourStart = lf->start;
if (ourStart < winStart)
    ourStart = winStart;
int ourEnd = lf->end;
if (ourEnd > winEnd)
    ourEnd = winEnd;
x1 = round((double)((int)ourStart-winStart)*scale) + xOff;
x2 = round((double)((int)ourEnd-winStart)*scale) + xOff;
w = x2-x1;
if (lf->start==lf->end && w==0) // like a SNP insertion point of size=0
    {
    w = 1;
    hvGfxBox(hvg, x1, y, w, heightPer, color);
    return;
    }

// are we highlighting this feature with background highlighting
if (lf->highlightColor && (lf->highlightMode == highlightBackground))
    {
    // draw the background
    hvGfxBox(hvg, x1, y, w, heightPer, lf->highlightColor);

    // draw the item slightly smaller
    y++;
    heightPer -=2;
    }

if (!hideLine)
    {
    innerLine(hvg, x1, midY, w, color);
    }
if (!hideArrows)
    {
    if ((intronGap == 0) && (vis == tvFull || vis == tvPack))
	{
	if (lf->highlightColor && (lf->highlightMode == highlightOutline))
	    clippedBarbs(hvg, x1, midY, w, tl.barbHeight, tl.barbSpacing,
                         lf->orientation, lf->highlightColor, FALSE);
        else
            clippedBarbs(hvg, x1, midY, w, tl.barbHeight, tl.barbSpacing,
                         lf->orientation, bColor, FALSE);
        }
    }

components = (lf->codons && zoomedToCdsColorLevel) ? lf->codons : lf->components;


for (sf = components; sf != NULL; sf = sf->next)
    {
    s = sf->start; e = sf->end;

    /* Draw UTR portion(s) of exon, if any: */
    if (s < tallStart)
	{
	e2 = e;
	if (e2 > tallStart) e2 = tallStart;
	if (lf->highlightColor && (lf->highlightMode == highlightOutline))
	    {
	    drawScaledBox(hvg, s, e2, scale, xOff, y+shortOff , shortHeight , lf->highlightColor);
	    drawScaledBox(hvg, s, e2, scale, xOff + 1, y+shortOff + 1, shortHeight - 2, color);
	    }
	else
	    {
	    drawScaledBox(hvg, s, e2, scale, xOff, y+shortOff, shortHeight,
		color);
	    }
	s = e2;
	}
    if (e > tallEnd)
	{
	s2 = s;
        if (s2 < tallEnd) s2 = tallEnd;
	if (lf->highlightColor && (lf->highlightMode == highlightOutline))
	    {
	    drawScaledBox(hvg, s2, e, scale, xOff, y+shortOff, shortHeight, lf->highlightColor);
	    drawScaledBox(hvg, s2, e, scale, xOff+1, y+shortOff+1, shortHeight-2, color);
	    }
	else
	    {
	    drawScaledBox(hvg, s2, e, scale, xOff, y+shortOff, shortHeight,
		color);
	    }
	e = s2;
	}
    /* Draw "tall" portion of exon (or codon) */
    if (e > s)
	{
        if (drawOpt > baseColorDrawOff
        &&  e + 6 >= winStart
        &&  s - 6 <  winEnd
        &&  (e-s <= 3 || !baseColorNeedsCodons))
            baseColorDrawItem(tg, lf, sf->grayIx, hvg, xOff, y, scale, font, s, e, heightPer,
                              zoomedToCodonLevel, qSeq, qOffset, sf, psl, drawOpt, MAXPIXELS, winStart,
                              color);
        else
            {
	    if (lf->highlightColor && (lf->highlightMode == highlightOutline))
		{
		drawScaledBox(hvg, s, e, scale, xOff, y, heightPer, lf->highlightColor);
		drawScaledBox(hvg, s, e, scale, xOff+1, y+1, heightPer-2, color);
		}
	    else
		{
		if (tg->drawLabelInBox && 
                        !(tg->drawLabelInBoxNotDense && vis == tvDense))
                    {
		    drawScaledBoxLabel(hvg, s, e, scale, xOff, y, heightPer,
                                color, font, lf->name );
                    // exon arrows would overlap the text
                    exonArrowsAlways = FALSE;
                    exonArrows = FALSE;
                    }

		else
		    drawScaledBox(hvg, s, e, scale, xOff, y, heightPer, color);
                }

            /* Display barbs only if no intron is visible on the item.
               This occurs when the exon completely spans the window,
               or when it is the first or last intron in the feature and
               the following/preceding intron isn't visible */
            if (exonArrowsAlways
            || (  exonArrows
               && (sf->start <= winStart || sf->start == lf->start)
               && (sf->end   >= winEnd   || sf->end   == lf->end)))
                {
                Color barbColor = hvGfxContrastingColor(hvg, color);
                // This scaling of bases to an image window occurs in several places.
                // It should really be broken out into a function.
                if (s < winStart)
                    s = winStart;
                if (e > winEnd)
                    e = winEnd;
                x1 = round((double)((int)s-winStart)*scale) + xOff;
                x2 = round((double)((int)e-winStart)*scale) + xOff;
                w = x2-x1;
                clippedBarbs(hvg, x1+1, midY, x2-x1-2, tl.barbHeight, tl.barbSpacing,
                             lf->orientation, barbColor, TRUE);
                }
            }
	}
    }

if ((intronGap > 0) || chainLines)
    lfDrawSpecialGaps(lf, intronGap, chainLines, gapFactor,
		      tg, hvg, xOff, y, scale, color, bColor, vis);

if (vis != tvDense)
    {
    /* If highlighting differences between aligned sequence and genome when
     * zoomed way out, this must be done in a separate pass after exons are
     * drawn so that exons sharing the pixel don't overdraw differences. */
    baseColorOverdrawDiff(tg, lf, hvg, xOff, y, scale, heightPer,
			  qSeq, qOffset, psl, winStart, drawOpt);
    if (psl && (indelShowQueryInsert || indelShowPolyA))
	baseColorOverdrawQInsert(tg, lf, hvg, xOff, y, scale, heightPer,
				 qSeq, qOffset, psl, font, winStart, drawOpt,
				 indelShowQueryInsert, indelShowPolyA);
    }
}

static void lfSeriesDrawConnecter(struct linkedFeaturesSeries *lfs,
                                  struct hvGfx *hvg, int start, int end, double scale, int xOff,
                                  int midY, Color color, Color bColor, enum trackVisibility vis)
/* Draw connection between two sets of linked features. */
{
if (start != -1 && !lfs->noLine)
    {
    int x1 = round((double)((int)start-winStart)*scale) + xOff;
    int x2 = round((double)((int)end-winStart)*scale) + xOff;
    int w = x2-x1;
    if (w > 0)
        {
        if (vis == tvFull || vis == tvPack)
            clippedBarbs(hvg, x1, midY, w, tl.barbHeight, tl.barbSpacing,
                         lfs->orientation, bColor, TRUE);
        hvGfxLine(hvg, x1, midY, x2, midY, color);
        }
    }
}


void linkedFeaturesSeriesDrawAt(struct track *tg, void *item,
        struct hvGfx *hvg, int xOff, int y, double scale,
	    MgFont *font, Color color, enum trackVisibility vis)
/* Draw a linked features series item at position. */
{
struct linkedFeaturesSeries *lfs = item;
struct linkedFeatures *lf;
Color bColor;
int midY = y + (tg->heightPer>>1);
int prevEnd = lfs->start;
int saveColor = color;

if ((lf = lfs->features) == NULL)
    return;
if (sameString(tg->tdb->type, "coloredExon"))
    {
    color = blackIndex();
    bColor = color;
    }
else
    lfColors(tg, lf, hvg, &color, &bColor);
if (vis == tvDense && trackDbSetting(tg->tdb, EXP_COLOR_DENSE))
    color = saveColor;
for (lf = lfs->features; lf != NULL; lf = lf->next)
    {
    lfSeriesDrawConnecter(lfs, hvg, prevEnd, lf->start, scale, xOff, midY,
        color, bColor, vis);
    prevEnd = lf->end;
    linkedFeaturesDrawAt(tg, lf, hvg, xOff, y, scale, font, color, vis);
    if (tg->mapsSelf)
        {
        int x1 = round((double)((int)lf->start-winStart)*scale) + xOff;
        int x2 = round((double)((int)lf->end-winStart)*scale) + xOff;
	int w = x2-x1;
	tg->mapItem(tg, hvg, lf, lf->name, tg->mapItemName(tg, item), lf->start, lf->end, x1, y, w, tg->heightPer);
	}
    }
lfSeriesDrawConnecter(lfs, hvg, prevEnd, lfs->end, scale, xOff, midY,
	color, bColor, vis);
}

boolean highlightItem(struct track *tg, void *item)
/* Should this item be highlighted? */
{
char *mapName = NULL;
char *name = NULL;
char *chp;
boolean highlight = FALSE;
mapName = tg->mapItemName(tg, item);
name = tg->itemName(tg, item);

/* special process for KG, because of "hgg_prot" piggy back */
if (sameWord(tg->table, "knownGene"))
    {
    mapName = cloneString(mapName);
    chp = strstr(mapName, "&hgg_prot");
    if (chp != NULL) *chp = '\0';
    }
#ifndef GBROWSE
/* special case for PCR Results (may have acc+name mapItemName): */
else if (sameString(tg->table, PCR_RESULT_TRACK_NAME))
    mapName = pcrResultItemAccession(mapName);
#endif /* GBROWSE */

/* Only highlight if names are in the hgFindMatches hash with
   a 1. */
highlight = (hgFindMatches != NULL &&
	     ( hashIntValDefault(hgFindMatches, name, 0) == 1 ||
	       hashIntValDefault(hgFindMatches, mapName, 0) == 1));
return highlight;
}

double scaleForWindow(double width, int seqStart, int seqEnd)
/* Return the scale for the window. */
{
return width / (seqEnd - seqStart);
}

boolean nextItemCompatible(struct track *tg)
/* Check to see if we draw nextPrev item buttons on a track. */
{
return (withNextExonArrows && tg->nextExonButtonable && tg->nextPrevExon);
}

boolean exonNumberMapsCompatible(struct track *tg, enum trackVisibility vis)
/* Check to see if we draw exon and intron maps labeling their number. */
{
if (tg->tdb)
    {
    char *type = tg->tdb->type;
    if (sameString(type, "interact") || sameString(type, "bigInteract"))
        return FALSE;
    }

char *defVal = "off";
char *type = tg->tdb->type;
if (startsWith("bigGenePred", type) || startsWith("genePred", type))
    defVal = "on";

boolean exonNumbers = sameString(trackDbSettingOrDefault(tg->tdb, "exonNumbers", defVal), "on");
return (withExonNumbers && exonNumbers && (vis==tvSquish || vis==tvFull || vis==tvPack) && (winEnd - winStart < 400000)
 && (tg->nextPrevExon==linkedFeaturesNextPrevItem));
}

void genericMapItem(struct track *tg, struct hvGfx *hvg, void *item,
		    char *itemName, char *mapItemName, int start, int end,
		    int x, int y, int width, int height)
/* This is meant to be used by genericDrawItems to set to tg->mapItem in */
/* case tg->mapItem isn't set to anything already. */
{
// Don't bother if we are imageV2 and a dense child.
if (!theImgBox || tg->limitedVis != tvDense || !tdbIsCompositeChild(tg->tdb))
    {
    char *directUrl = trackDbSetting(tg->tdb, "directUrl");
    boolean withHgsid = (trackDbSetting(tg->tdb, "hgsid") != NULL);
    char *trackName = tg->track;
    if (tg->originalTrack != NULL)
        trackName = tg->originalTrack;
    mapBoxHgcOrHgGene(hvg, start, end, x, y, width, height, trackName,
                      mapItemName, itemName, directUrl, withHgsid, NULL);
    }
}

void genericDrawNextItemStuff(struct track *tg, struct hvGfx *hvg, enum trackVisibility vis,
                              struct slList *item, double scale, int x2, int x1, int textX, int y, int heightPer,
                              Color color, boolean doButtons)
/* After the item is drawn in genericDrawItems, draw next/prev item related */
/* buttons and the corresponding mapboxes. */
{
int buttonW = heightPer-1 + 2*NEXT_ITEM_ARROW_BUFFER;
int s = tg->itemStart(tg, item);
int e = tg->itemEnd(tg, item);
boolean rButton = FALSE;
boolean lButton = FALSE;
struct window *lastWindow = windows;
while (lastWindow->next)
    lastWindow = lastWindow->next;
// only process for first and last windows
/* Draw the actual triangles.  These are always at the edge of the window. */
if (doButtons && (s < winStart) && (currentWindow == windows)) // first window
    {
    lButton = TRUE;
    }
if (doButtons && (e > winEnd) && (currentWindow == lastWindow))
    {
    rButton = TRUE;
    }

if ((vis == tvFull) || (vis == tvPack))
    {
    /* Make the button mapboxes. */
    if (lButton)
        lButton = tg->nextPrevExon(tg, hvg, item, insideX, y, buttonW, heightPer, FALSE);
    if (rButton)
        rButton = tg->nextPrevExon(tg, hvg, item, insideX + insideWidth - buttonW, y, buttonW, heightPer, TRUE);
    }

/* Draw the actual triangles.  These are always at the edge of the window. */
if (lButton)
    {
    hvGfxNextItemButton(hvg, insideX + NEXT_ITEM_ARROW_BUFFER, y,
                        heightPer-1, heightPer-1, color, MG_WHITE, FALSE);
    }
if (rButton)
    {
    hvGfxNextItemButton(hvg, insideX + insideWidth - NEXT_ITEM_ARROW_BUFFER - heightPer,
                        y, heightPer-1, heightPer-1, color, MG_WHITE, TRUE);
    }

boolean compat = exonNumberMapsCompatible(tg, vis);
if (vis == tvSquish || vis == tvPack || (vis == tvFull && isTypeBedLike(tg)))
    {
    int w = x2-textX;
    if (lButton)
        { // if left-button, the label will be on far left, split out a map just for that label.
	tg->mapItem(tg, hvg, item, tg->itemName(tg, item), tg->mapItemName(tg, item),
                    s, e, textX, y, insideX-textX, heightPer);
	textX = insideX + buttonW; // continue on the right side of the left exon button
	w = x2-textX;
	}
    if (rButton)
        {
	w -= buttonW;
        }

    if (compat)
	{  // draw labeled exon/intron maps with exon/intron numbers
	linkedFeaturesItemExonMaps(tg, hvg, item, scale, y, heightPer, s, e, lButton, rButton, buttonW);
	x2 = x1;
	w = x2-textX;
	}
    // if not already mapped, pick up the label
    if (!(lButton && compat))
	{
        tg->mapItem(tg, hvg, item, tg->itemName(tg, item), tg->mapItemName(tg, item),
		s, e, textX, y, w, heightPer);
	}
    }
else if (vis == tvSquish)
    {
    int w = x2-textX;
    tg->mapItem(tg, hvg, item, tg->itemName(tg, item), tg->mapItemName(tg, item),
            s, e, textX, y, w, heightPer);
    }
else if (vis == tvFull)
    {
    int geneMapBoxX = insideX;
    int geneMapBoxW = insideWidth;
    /* Draw the first gene label mapbox, in the left margin. */
    int trackPastTabX = (withLeftLabels ? trackTabWidth : 0);
    tg->mapItem(tg, hvg, item, tg->itemName(tg, item), tg->mapItemName(tg, item),
                s, e, trackPastTabX, y, insideX - trackPastTabX, heightPer);
    /* Depending on which button mapboxes we drew, draw the remaining mapbox. */
    if (lButton)
        {
        geneMapBoxX += buttonW;
        geneMapBoxW -= buttonW;
        }
    if (rButton)
	{
        geneMapBoxW -= buttonW;
	}
    if (compat)
	{  // draw labeled exon/intron maps with exon/intron numbers
	linkedFeaturesItemExonMaps(tg, hvg, item, scale, y, heightPer, s, e, lButton, rButton, buttonW);
	if (!lButton)
	    {
	    int w = x1 - geneMapBoxX;
	    if (w > 0)
		tg->mapItem(tg, hvg, item, tg->itemName(tg, item), tg->mapItemName(tg, item),
		    s, e, geneMapBoxX, y, w, heightPer);
	    }
	if (!rButton)
	    {
	    int w = geneMapBoxX + geneMapBoxW - x2;
	    if (w > 0)
		tg->mapItem(tg, hvg, item, tg->itemName(tg, item), tg->mapItemName(tg, item),
		    s, e, x2, y, w, heightPer);
	    }
	}
    else
	{
	tg->mapItem(tg, hvg, item, tg->itemName(tg, item), tg->mapItemName(tg, item),
		    s, e, geneMapBoxX, y, geneMapBoxW, heightPer);
	}
    }
}


static void handleNonPropDrawItemAt(struct track *tg, struct spaceNode *sn, void *item, struct hvGfx *hvg,
        int xOff, int yOff, double scale,
        MgFont *font, Color color, enum trackVisibility vis)
/* Handle non-Proportional draw-at including clipping */
{

if (tg->nonPropDrawItemAt && tg->nonPropPixelWidth && !sn->noLabel)
    {
    hvGfxUnclip(hvg);
    hvGfxSetClip(hvg, fullInsideX, yOff, fullInsideWidth, tg->height);

    tg->nonPropDrawItemAt(tg, item, hvg, xOff, yOff, scale, font, color, vis);

    hvGfxUnclip(hvg);
    hvGfxSetClip(hvg, insideX, yOff, insideWidth, tg->height);

    }
}


static void genericDrawOverflowItem(struct track *tg, struct spaceNode *sn,
                                    struct hvGfx *hvg, int xOff, int yOff, int width,
                                    MgFont *font, Color color, double scale,
                                    int overflowRow, boolean firstOverflow)
/* genericDrawItems logic for drawing an overflow row */
{
/* change some state to paint it in the "overflow" row. */
enum trackVisibility origVis = tg->limitedVis;
int origLineHeight = tg->lineHeight;
int origHeightPer = tg->heightPer;
tg->limitedVis = tvDense;
tg->heightPer = tl.fontHeight;
tg->lineHeight = tl.fontHeight+1;
struct slList *item = sn->val;
int origRow = sn->row;
sn->row = overflowRow;
if (tg->itemNameColor != NULL)
    color = tg->itemNameColor(tg, item, hvg);
int y = yOff + origLineHeight * overflowRow;
tg->drawItemAt(tg, item, hvg, xOff, y, scale, font, color, tvDense);
// non-proportional track like gtexGene
// NOTE: Since this is being drawn in dense, perhaps it should not be called?
//  well, gtex probably does not need it, but maybe the others might
handleNonPropDrawItemAt(tg, sn, item, hvg, xOff, y, scale, font, color, tvDense);

sn->row = origRow;

/* Draw label for overflow row. */
if (withLeftLabels && firstOverflow && currentWindow == windows) // first window
    {
    int overflowCount = 0;
    // Do for all windows
    struct track *tgSave = tg;
    for(tg=tgSave; tg; tg=tg->nextWindow)
	{
	for (sn = tg->ss->nodeList; sn != NULL; sn = sn->next)
	    // do not count items with label turned off as they are dupes anyways.
	    if (sn->row >= overflowRow && !sn->noLabel)
		overflowCount++;
	}
    tg = tgSave;
    assert(hvgSide != NULL);
    hvGfxUnclip(hvgSide);
    hvGfxSetClip(hvgSide, leftLabelX, yOff, insideWidth, tg->height);
    char nameBuff[SMALLBUF];
    safef(nameBuff, sizeof(nameBuff), "Last Row: %d", overflowCount);
    mgFontStringWidth(font, nameBuff);
    hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, tg->lineHeight,
                   color, font, nameBuff);
    hvGfxUnclip(hvgSide);
    hvGfxSetClip(hvgSide, insideX, yOff, insideWidth, tg->height);
    }
/* restore state */
tg->limitedVis = origVis;
tg->heightPer = origHeightPer;
tg->lineHeight = origLineHeight;
}


void genericDrawItemLabel(struct track *tg, struct spaceNode *sn,
                                 struct hvGfx *hvg, int xOff, int y, int width,
                                 MgFont *font, Color color, Color labelColor, enum trackVisibility vis,
                                 double scale, boolean withLeftLabels)
/* Generic function for writing out an item label */
{
struct slList *item = sn->val;
int s = tg->itemStart(tg, item);
int sClp = (s < winStart) ? winStart : s;
int x1 = round((sClp - winStart)*scale) + xOff;
int textX = x1;

if (tg->drawLabelInBox)
    withLeftLabels = FALSE;

if (tg->itemNameColor != NULL)
    {
    color = tg->itemNameColor(tg, item, hvg);
    labelColor = color;
    if (withLeftLabels && isTooLightForTextOnWhite(hvg, color))
	labelColor = somewhatDarkerColor(hvg, color);
    }

/* pgSnpDrawAt may change withIndividualLabels between items */
boolean withLabels = (withLeftLabels && withIndividualLabels && ((vis == tvPack) || (vis == tvFull && isTypeBedLike(tg))) && (!sn->noLabel) && !tg->drawName);
if (withLabels)
    {
    char *name = tg->itemName(tg, item);
    int nameWidth = mgFontStringWidth(font, name);
    int dotWidth = tl.nWidth/2;
    boolean snapLeft = FALSE;
    boolean drawNameInverted = highlightItem(tg, item);
    textX -= nameWidth + dotWidth;
    snapLeft = (textX < fullInsideX);
    snapLeft |= (vis == tvFull && isTypeBedLike(tg));

    /* Special tweak for expRatio in pack mode: force all labels
     * left to prevent only a subset from being placed right: */
    snapLeft |= (startsWith("expRatio", tg->tdb->type));

#ifdef IMAGEv2_NO_LEFTLABEL_ON_FULL
    if (theImgBox == NULL && snapLeft)
#else///ifndef IMAGEv2_NO_LEFTLABEL_ON_FULL
    if (snapLeft)        /* Snap label to the left. */
#endif ///ndef IMAGEv2_NO_LEFTLABEL_ON_FULL
        {
        textX = leftLabelX;
        assert(hvgSide != NULL);
        int prevX, prevY, prevWidth, prevHeight;
        hvGfxGetClip(hvgSide, &prevX, &prevY, &prevWidth, &prevHeight);
        hvGfxUnclip(hvgSide);
        hvGfxSetClip(hvgSide, leftLabelX, y, fullInsideX - leftLabelX, tg->height);
        if(drawNameInverted)
            {
            int boxStart = leftLabelX + leftLabelWidth - 2 - nameWidth;
            hvGfxBox(hvgSide, boxStart, y, nameWidth+1, tg->heightPer - 1, color);
            hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, tg->heightPer,
                        MG_WHITE, font, name);
            }
        else
            hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, tg->heightPer,
                        labelColor, font, name);
        hvGfxUnclip(hvgSide);
        hvGfxSetClip(hvgSide, prevX, prevY, prevWidth, prevHeight);
        }
    else
        {
	// shift clipping to allow drawing label to left of currentWindow
	int pdfSlop=nameWidth/5;
        int prevX, prevY, prevWidth, prevHeight;
        hvGfxGetClip(hvgSide, &prevX, &prevY, &prevWidth, &prevHeight);
        hvGfxUnclip(hvg);
        hvGfxSetClip(hvg, textX-1-pdfSlop, y, nameWidth+1+pdfSlop, tg->heightPer);
        if(drawNameInverted)
            {
            hvGfxBox(hvg, textX - 1, y, nameWidth+1, tg->heightPer-1, color);
            hvGfxTextRight(hvg, textX, y, nameWidth, tg->heightPer, MG_WHITE, font, name);
            }
        else
            // usual labeling
            hvGfxTextRight(hvg, textX, y, nameWidth, tg->heightPer, labelColor, font, name);
        hvGfxUnclip(hvg);
        hvGfxSetClip(hvgSide, prevX, prevY, prevWidth, prevHeight);
        }
    }
withIndividualLabels = TRUE; /* reset in case done with pgSnp */
}

void genericItemMapAndArrows(struct track *tg, struct spaceNode *sn,
                                    struct hvGfx *hvg, int xOff, int y, int width,
                                    MgFont *font, Color color, Color labelColor, enum trackVisibility vis,
                                    double scale, boolean withLeftLabels)
/* Generic function for putting down a mapbox with a label and drawing exon arrows */
{
struct slList *item = sn->val;
int s = tg->itemStart(tg, item);
int e = tg->itemEnd(tg, item);
int sClp = (s < winStart) ? winStart : s;
int eClp = (e > winEnd)   ? winEnd   : e;
int x1 = round((sClp - winStart)*scale) + xOff;
int x2 = round((eClp - winStart)*scale) + xOff;
int textX = x1;

if (tg->drawLabelInBox)
    withLeftLabels = FALSE;

boolean withLabels = (withLeftLabels && withIndividualLabels && ((vis == tvPack) || (vis == tvFull && isTypeBedLike(tg))) && (!sn->noLabel) && !tg->drawName);
if (withLabels)
    {
    char *name = tg->itemName(tg, item);
    int nameWidth = mgFontStringWidth(font, name);
    int dotWidth = tl.nWidth/2;
    boolean snapLeft = FALSE;
    textX -= nameWidth + dotWidth;
    snapLeft = (textX < fullInsideX);
    snapLeft |= (vis == tvFull && isTypeBedLike(tg));

    /* Special tweak for expRatio in pack mode: force all labels
     * left to prevent only a subset from being placed right: */
    snapLeft |= (startsWith("expRatio", tg->tdb->type));

#ifdef IMAGEv2_NO_LEFTLABEL_ON_FULL
    if (theImgBox == NULL && snapLeft)
#else///ifndef IMAGEv2_NO_LEFTLABEL_ON_FULL
    if (snapLeft)        /* Snap label to the left. */
#endif ///ndef IMAGEv2_NO_LEFTLABEL_ON_FULL
        {
        textX = leftLabelX;
        assert(hvgSide != NULL);
        }
    }

if (!tg->mapsSelf)
    {
    int w = x2-textX;
    /* Arrows? */
    if (w > 0)
        {
        if (nextItemCompatible(tg))
            genericDrawNextItemStuff(tg, hvg, vis, item, scale, x2, x1, textX, y, tg->heightPer, color, TRUE);
        else if (exonNumberMapsCompatible(tg, vis))
            genericDrawNextItemStuff(tg, hvg, vis, item, scale, x2, x1, textX, y, tg->heightPer, color, FALSE);
        else
            {
            tg->mapItem(tg, hvg, item, tg->itemName(tg, item),
                        tg->mapItemName(tg, item), s, e, textX, y, w, tg->heightPer);
            }
        }
    }
}


static void genericDrawItem(struct track *tg, struct spaceNode *sn,
                            struct hvGfx *hvg, int xOff, int yOff, int width,
                            MgFont *font, Color color, Color labelColor, enum trackVisibility vis,
                            double scale, boolean withLeftLabels)
/* draw one non-overflow item */
{
struct slList *item = sn->val;

if (tg->drawLabelInBox)
    withLeftLabels = FALSE;

if (tg->itemNameColor != NULL)
    {
    color = tg->itemNameColor(tg, item, hvg);
    labelColor = color;
    if (withLeftLabels && isTooLightForTextOnWhite(hvg, color))
	labelColor = somewhatDarkerColor(hvg, color);
    }

// get y offset for item in pack mode
int yRow = 0;
if (tg->ss && tg->ss->rowSizes != NULL)
    {
    int i;
    for (i=0; i < sn->row; i++)
        yRow += tg->ss->rowSizes[i];
    int itemHeight = tg->itemHeight(tg, item);
    yRow += (tg->ss->rowSizes[sn->row] - itemHeight + 1);
    tg->heightPer = itemHeight;
    }
else
    yRow = tg->lineHeight * sn->row;
int y = yOff + yRow;

tg->drawItemAt(tg, item, hvg, xOff, y, scale, font, color, vis);

// GALT non-proportional track like gtexGene
handleNonPropDrawItemAt(tg, sn, item, hvg, xOff, y, scale, font, color, vis);

tg->drawItemLabel(tg, sn, hvg, xOff, y, width, font, color, labelColor, vis, scale, withLeftLabels);

// do mapping and arrows
// NB: I'd be happy to move the label mapbox draw from the next function into the preceding function,
// so that it sits with the label drawing and doesn't duplicate those calculations.  That's a problem
// to tackle another time though.

tg->doItemMapAndArrows(tg, sn, hvg, xOff, y, width, font, color, labelColor, vis, scale, withLeftLabels);
}

int normalizeCount(struct preDrawElement *el, double countFactor,
    double minVal, double maxVal, double sumData, double sumSquares)
/* Normalize statistics to be based on an integer number of valid bases.
 * Integer value is the smallest integer not less than countFactor. */
{
bits32 validCount = ceil(countFactor);
double normFactor = (double)validCount/countFactor;

el->count = validCount;
el->min = minVal;
el->max = maxVal;
el->sumData = sumData * normFactor;
el->sumSquares = sumSquares * normFactor;

return validCount;
}

static unsigned *countOverlaps(struct track *track)
/* Count up overlap of linked features. */
{
struct slList *items = track->items;
struct slList *item;
unsigned size = winEnd - winStart;
unsigned *counts = needHugeZeroedMem((1+ size) * sizeof(unsigned));
extern int linkedFeaturesItemStart(struct track *tg, void *item);
boolean isLinkedFeature = ( track->itemStart == linkedFeaturesItemStart);

for (item = items; item; item = item->next)
    {
    if (isLinkedFeature)
        {
        struct linkedFeatures *lf = (struct linkedFeatures *)item;
        struct simpleFeature *sf;

        for (sf = lf->components; sf != NULL; sf = sf->next)
            {
            unsigned start = sf->start;
            unsigned end = sf->end;
            if (start == end)
                end++;
            if (positiveRangeIntersection(start, end, winStart, winEnd) <= 0)
                continue;

            int x1 = max((int)start - (int)winStart, 0);
            int x2 = min((int)end - (int)winStart, size);

            for(; x1 < x2; x1++)
                counts[x1]++;
            }
        }
    else
        {
        unsigned start = track->itemStart(track, item);
        unsigned end = track->itemEnd(track, item);
        if (positiveRangeIntersection(start, end, winStart, winEnd) <= 0)
            continue;

        int x1 = max((int)start - (int)winStart, 0);
        int x2 = min((int)end - (int)winStart, size);

        for(; x1 < x2; x1++)
            counts[x1]++;
        }
    }

return counts;
}

static void countsToPixelsUp(unsigned *counts, struct preDrawContainer *pre)
/* Up sample counts into pixels. */
{
int preDrawZero = pre->preDrawZero;
unsigned size = winEnd - winStart;
double countsPerPixel = size / (double) insideWidth;
int pixel;

for (pixel=0; pixel<insideWidth; ++pixel)
    {
    struct preDrawElement *pe = &pre->preDraw[pixel + preDrawZero];
    unsigned index = pixel * countsPerPixel;
    pe->count = 1;
    pe->min = counts[index];
    pe->max = counts[index];
    pe->sumData = counts[index] ;
    pe->sumSquares = counts[index] * counts[index];
    }
}

static void countsToPixelsDown(unsigned *counts, struct preDrawContainer *pre)
/* Down sample counts into pixels. */
{
int preDrawZero = pre->preDrawZero;
unsigned size = winEnd - winStart;
double countsPerPixel = size / (double) insideWidth;
int pixel;

for (pixel=0; pixel<insideWidth; ++pixel)
    {
    struct preDrawElement *pe = &pre->preDraw[pixel + preDrawZero];
    double startReal = pixel * countsPerPixel;
    double endReal = (pixel + 1) * countsPerPixel;
    unsigned startUns = startReal;
    unsigned endUns = endReal;
    double realCount, realSum, realSumSquares, max, min;

    realCount = realSum = realSumSquares = 0.0;
    max = min = counts[startUns];

    assert(startUns != endUns);
    unsigned ceilUns = ceil(startReal);

    if (ceilUns != startUns)
	{
	/* need a fraction of the first count */
	double frac = (double)ceilUns - startReal;
	realCount = frac;
	realSum = frac * counts[startUns];
	realSumSquares = realSum * realSum;
	startUns++;
	}

    // add in all the counts that are totally in this pixel
    for(; startUns < endUns; startUns++)
	{
	realCount += 1.0;
	realSum += counts[startUns];
	realSumSquares += counts[startUns] * counts[startUns];
	if (max < counts[startUns])
	    max = counts[startUns];
	if (min > counts[startUns])
	    min = counts[startUns];
	}

    // add any fraction of the count that's only partially in this pixel
    double lastFrac = endReal - endUns;
    double lastSum = lastFrac * counts[endUns];
    if ((lastFrac > 0.0) && (endUns < size))
	{
	if (max < counts[endUns])
	    max = counts[endUns];
	if (min > counts[endUns])
	    min = counts[endUns];
	realCount += lastFrac;
	realSum += lastSum;
	realSumSquares += lastSum * lastSum;
	}

    pe->count = normalizeCount(pe, realCount, min, max, realSum, realSumSquares);
    }
}

static void countsToPixels(unsigned *counts, struct preDrawContainer *pre)
/* Sample counts into pixels. */
{
unsigned size = winEnd - winStart;
double countsPerPixel = size / (double) insideWidth;

if (countsPerPixel <= 1.0)
    countsToPixelsUp(counts, pre);
else
    countsToPixelsDown(counts, pre);
}

static void summaryToPixels(struct bbiSummaryElement *summary, struct preDrawContainer *pre)
/* Convert bbiSummaryElement array into a preDrawElement array */
{
struct preDrawElement *pe = &pre->preDraw[pre->preDrawZero];
struct preDrawElement *lastPe = &pe[insideWidth];

for (; pe < lastPe; pe++, summary++)
    {
    pe->count = summary->validCount;
    pe->min = summary->minVal;
    pe->max = summary->maxVal;
    pe->sumData = summary->sumData;
    pe->sumSquares = summary->sumSquares;
    }
}

static void genericDrawItemsWiggle(struct track *tg, int seqStart, int seqEnd,
                                       struct hvGfx *hvg, int xOff, int yOff, int width,
                                       MgFont *font, Color color, enum trackVisibility vis)
/* Draw a list of linked features into a wiggle. */
{
struct wigCartOptions *wigCart = tg->wigCartData;
struct preDrawContainer *pre = tg->preDrawContainer = initPreDrawContainer(insideWidth);
struct trackDb *tdb = tg->tdb;
boolean parentLevel = isNameAtParentLevel(tdb,tdb->track);

char *autoScale = cartOptionalStringClosestToHome(cart, tdb, parentLevel, AUTOSCALE);
if (autoScale == NULL)
    wigCart->autoScale =  wiggleScaleAuto;
char *windowingFunction = cartOptionalStringClosestToHome(cart, tdb, parentLevel, WINDOWINGFUNCTION);
if (windowingFunction == NULL)
    wigCart->windowingFunction = wiggleWindowingMean;
wigCart->alwaysZero = TRUE;

if (tg->summary)
    summaryToPixels(tg->summary, pre);
else
    {
    unsigned *counts = countOverlaps(tg);
    countsToPixels(counts, pre);
    freez(&counts);
    }

tg->colorShades = shadesOfGray;
hvGfxSetClip(hvg, insideX, yOff, insideWidth, tg->height);
tg->mapsSelf = FALSE; // some magic to turn off the link out
wigPreDrawPredraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis,
	       tg->preDrawContainer, tg->preDrawContainer->preDrawZero, tg->preDrawContainer->preDrawSize, &tg->graphUpperLimit, &tg->graphLowerLimit);
wigDrawPredraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis,
	       tg->preDrawContainer, tg->preDrawContainer->preDrawZero, tg->preDrawContainer->preDrawSize, tg->graphUpperLimit, tg->graphLowerLimit);
tg->mapsSelf = TRUE;
hvGfxUnclip(hvg);
}

static void genericDrawItemsPackSquish(struct track *tg, int seqStart, int seqEnd,
                                       struct hvGfx *hvg, int xOff, int yOff, int width,
                                       MgFont *font, Color color, enum trackVisibility vis)
/* genericDrawItems logic for pack and squish modes */
{
double scale = scaleForWindow(width, seqStart, seqEnd);
int lineHeight = tg->lineHeight;
struct spaceNode *sn;
int maxHeight = maximumTrackHeight(tg);
int overflowRow = (maxHeight - tl.fontHeight +1) / lineHeight;
boolean firstOverflow = TRUE;

hvGfxSetClip(hvg, insideX, yOff, insideWidth, tg->height);
assert(tg->ss);

/* Loop through and draw each item individually */
for (sn = tg->ss->nodeList; sn != NULL; sn = sn->next)
    {
    if (sn->row >= overflowRow)
        {
        genericDrawOverflowItem(tg, sn, hvg, xOff, yOff, width, font, color,
                                scale, overflowRow, firstOverflow);
        firstOverflow = FALSE;
        }
    else
        genericDrawItem(tg, sn, hvg, xOff, yOff, width, font, color, color, vis,
                        scale, withLeftLabels);
    }

maybeDrawQuickLiftLines(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
hvGfxUnclip(hvg);
}

void genericDrawNextItem(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y,
                            double scale, Color color, enum trackVisibility vis)
/* Draw next item buttons and map boxes */
//TODO: Use this to clean up genericDrawItemsFullDense (will require wading thru ifdefs)
{
boolean isNextItemCompatible = nextItemCompatible(tg);
boolean isExonNumberMapsCompatible = exonNumberMapsCompatible(tg, vis);
if (!isNextItemCompatible && !isExonNumberMapsCompatible)
    return;
boolean doButtons = (isExonNumberMapsCompatible ? FALSE: TRUE);

// Convert start/end coordinates to pix
int s = tg->itemStart(tg, item);
int e = tg->itemEnd(tg, item);
int sClp = (s < winStart) ? winStart : s;
int eClp = (e > winEnd)   ? winEnd   : e;
int x1 = round((sClp - winStart)*scale) + xOff;
int x2 = round((eClp - winStart)*scale) + xOff;
genericDrawNextItemStuff(tg, hvg, vis, item, scale, x2, x1, -1, y, tg->heightPer, color,
                            doButtons);
}

static void genericDrawItemsFullDense(struct track *tg, int seqStart, int seqEnd,
                                      struct hvGfx *hvg, int xOff, int yOff, int width,
                                      MgFont *font, Color color, enum trackVisibility vis)
/* genericDrawItems logic for full and dense modes */
{
double scale = scaleForWindow(width, seqStart, seqEnd);
struct slList *item;
int y = yOff;
for (item = tg->items; item != NULL; item = item->next)
    {
    if (tg->itemColor != NULL)
        color = tg->itemColor(tg, item, hvg);
    tg->drawItemAt(tg, item, hvg, xOff, y, scale, font, color, vis);
    if (vis == tvFull)
        {
        /* The doMapItems will make the mapboxes normally but make */
        /* them here if we're drawing nextItem buttons. */
        if (nextItemCompatible(tg))
#ifdef IMAGEv2_SHORT_MAPITEMS
            {
            // Convert start/end coordinates to pix
            int s = tg->itemStart(tg, item);
            int e = tg->itemEnd(tg, item);
            int sClp = (s < winStart) ? winStart : s;
            int eClp = (e > winEnd)   ? winEnd   : e;
            int x1 = round((sClp - winStart)*scale) + xOff;
            int x2 = round((eClp - winStart)*scale) + xOff;
        #ifdef IMAGEv2_NO_LEFTLABEL_ON_FULL
            if (theImgBox != NULL && vis == tvFull)  // dragScroll >1x has no bed full leftlabels,
                {                                    // but in image labels like pack.
                char *name = tg->itemName(tg, item);
                int nameWidth = mgFontStringWidth(font, name);
                int textX = round((s - winStart)*scale) + xOff;
                textX -= (nameWidth + 2);
                if (textX >= insideX && nameWidth > 0)
                    {
                    x1 = textX; // extends the map item to cover this label
                    Color itemNameColor = tg->itemNameColor(tg, item, hvg);
                    hvGfxTextRight(hvg,textX,y,nameWidth,tg->heightPer,itemNameColor,font,name);
                    }
                }
        #endif ///def IMAGEv2_NO_LEFTLABEL_ON_FULL
            genericDrawNextItemStuff(tg, hvg, vis, item, scale, x2, x1, x1, y, tg->heightPer, color, TRUE);
            }
#else//ifndef IMAGEv2_SHORT_MAPITEMS
            {
            // Convert start/end coordinates to pix
            int s = tg->itemStart(tg, item);
            int e = tg->itemEnd(tg, item);
            int sClp = (s < winStart) ? winStart : s;
            int eClp = (e > winEnd)   ? winEnd   : e;
            int x1 = round((sClp - winStart)*scale) + xOff;
            int x2 = round((eClp - winStart)*scale) + xOff;
            genericDrawNextItemStuff(tg, hvg, vis, item, scale, x2, x1, -1, y, tg->heightPer, color, TRUE); // was -1, -1, -1
	    }
        else if (exonNumberMapsCompatible(tg, vis))
            {
            // Convert start/end coordinates to pix
            int s = tg->itemStart(tg, item);
            int e = tg->itemEnd(tg, item);
            int sClp = (s < winStart) ? winStart : s;
            int eClp = (e > winEnd)   ? winEnd   : e;
            int x1 = round((sClp - winStart)*scale) + xOff;
            int x2 = round((eClp - winStart)*scale) + xOff;
            genericDrawNextItemStuff(tg, hvg, vis, item, scale, x2, x1, -1, y, tg->heightPer, color, FALSE); // was -1, -1, -1
	    }
#endif//ndef IMAGEv2_SHORT_MAPITEMS
        y += tg->lineHeight;
        }
    }
}

void genericDrawItems(struct track *tg, int seqStart, int seqEnd,
                      struct hvGfx *hvg, int xOff, int yOff, int width,
                      MgFont *font, Color color, enum trackVisibility vis)
/* Draw generic item list.  Features must be fixed height
 * and tg->drawItemAt has to be filled in. */
{
withIndividualLabels = TRUE;  // set this back to default just in case someone left it false (I'm looking at you pgSnp)

color = colorFromCart(tg, color);

if (tg->mapItem == NULL)
    tg->mapItem = genericMapItem;
if (vis != tvDense && baseColorCanDraw(tg))
    baseColorInitTrack(hvg, tg);
boolean doWiggle = checkIfWiggling(cart, tg);
if (doWiggle)
    {
    genericDrawItemsWiggle(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
				   font, color, vis);
    }
else if (vis == tvPack || vis == tvSquish || (vis == tvFull && isTypeBedLike(tg)))
    {
    genericDrawItemsPackSquish(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
                               font, color, vis);
    }
else
    genericDrawItemsFullDense(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
                              font, color, vis);
maybeDrawQuickLiftLines(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
}

void linkedFeaturesSeriesDraw(struct track *tg, int seqStart, int seqEnd,
                              struct hvGfx *hvg, int xOff, int yOff, int width,
                              MgFont *font, Color color, enum trackVisibility vis)
/* Draw linked features items. */
{
if (vis == tvDense && tg->colorShades)
    slSort(&tg->items, cmpLfsWhiteToBlack);
genericDrawItems(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
	font, color, vis);
}

void linkedFeaturesDraw(struct track *tg, int seqStart, int seqEnd,
                        struct hvGfx *hvg, int xOff, int yOff, int width,
                        MgFont *font, Color color, enum trackVisibility vis)
/* Draw linked features items. */
{
color = colorFromCart(tg, color);

if (tg->items == NULL && vis == tvDense && canDrawBigBedDense(tg))
    {
    bigBedDrawDense(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color);
    }
else
    {
    if (vis == tvDense && tg->colorShades)
	slSort(&tg->items, cmpLfWhiteToBlack);
    genericDrawItems(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
	    font, color, vis);
    }

maybeDrawQuickLiftLines(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
// put up the color key for the gnomAD pLI track
// If you change this code below, you must also change hgTracks.js:hideLegends
if (startsWith("pliBy", tg->track))
    doPliColors = TRUE;
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

void grayThreshold(UBYTE *pt, int count, Color *colors )
/* Convert from 0-4 representation to gray scale rep. */
{
Color b;
int i;

for (i=0; i<count; ++i)
    {
    b = pt[i];
    if (b == 0)
	colors[i] = shadesOfGray[0];
    else if (b == 1)
	colors[i] = shadesOfGray[2];
    else if (b == 2)
	colors[i] = shadesOfGray[4];
    else if (b == 3)
	colors[i] = shadesOfGray[6];
    else if (b >= 4)
	colors[i] = shadesOfGray[9];
    }
}

static void countBaseRangeUse(int x1, int x2, int width, UBYTE *useCounts)
/* increment base use counts for a pixels from a feature */
{
if (x1 < 0)
    x1 = 0;
if (x2 >= width)
    x2 = width-1;
int w = x2-x1;
if (w >= 0)
    {
    if (w == 0)
        w = 1;
    incRange(useCounts+x1, w);
    }
}

static void countLinkedFeaturesBaseUse(struct linkedFeatures *lf, int width, int baseWidth,
                                       UBYTE *useCounts, UBYTE *gapUseCounts, bool rc)
/* increment base use counts for a set of linked features */
{
/* Performence-sensitive code.  Most of the overhead is in the mapping of base
 * to pixel.  This was change from using roundingScale() to doing it all in
 * floating point, with the divide taken out of the loop. To avoid adding more
 * overhead when rendering gaps, the translation to pixel coordinates is done
 * here, and then we save the previous block coordinates for use in marking
 * the gap. */
struct simpleFeature *sf;
double scale = ((double)width)/((double)baseWidth);
int x1 = -1, x2 = -1, prevX1 = -1, prevX2 = -1;  /* pixel coords */
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    x1 = round(((double)(sf->start-winStart)) * scale);
    x2 = round(((double)(sf->end-winStart)) * scale);
    /* need to adjust x1/x2 before saving as prevX1/prevX2 */
    if (x1 < 0)
        x1 = 0;
    if (x2 >= width)
        x2 = width-1;
    if (rc)
        {
        int x1hold = x1;
        x1 = width-(x2+1);  // x2 is last base, n
        x2 = (width-x1hold)-1;
        }
    countBaseRangeUse(x1, x2, width, useCounts);
    /* line from previous block to this block, however blocks seem to be reversed
     * sometimes, not sure why, so just check. */
    if ((gapUseCounts != NULL) && (prevX1 >= 0))
        countBaseRangeUse(((prevX2 < x1) ? prevX2: x1),
                          ((prevX2 < x1) ? x2-1: prevX1),
                          width, gapUseCounts);

    prevX1 = x1;
    prevX2 = x2;
    }
/* last gap */
if ((gapUseCounts != NULL) && (prevX1 >= 0))
    countBaseRangeUse(((prevX2 < x1) ? prevX2: x1),
                      ((prevX2 < x1) ? x2-1: prevX1),
                      width, gapUseCounts);
}

static void countTrackBaseUse(struct track *tg, int width, int baseWidth,
                              UBYTE *useCounts, UBYTE *gapUseCounts, bool rc)
/* increment base use counts for a track */
{
struct linkedFeatures *lf;
for (lf = tg->items; lf != NULL; lf = lf->next)
    countLinkedFeaturesBaseUse(lf, width, baseWidth, useCounts, gapUseCounts, rc);

if (gapUseCounts != NULL)
    {
    /* don't overwrite other exons with lighter intron lines */
    int i;
    for (i = 0; i < width; i++)
        {
        if (useCounts[i] > 0)
            gapUseCounts[i] = 0;
        }
    }
}

static void linkedFeaturesDrawAverage(struct track *tg, int seqStart, int seqEnd,
                                      struct hvGfx *hvg, int xOff, int yOff, int width,
                                      MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense items doing color averaging items. */
{
int baseWidth = seqEnd - seqStart;
UBYTE *useCounts, *gapUseCounts = NULL;
int lineHeight = mgFontLineHeight(font);

AllocArray(useCounts, width);
/* limit adding gap lines to <= 25mb to improve performance */
if (baseWidth <= 25000000)
    AllocArray(gapUseCounts, width);
countTrackBaseUse(tg, width, baseWidth, useCounts, gapUseCounts, hvg->rc);

Color *colors = needMem(sizeof(Color) * width);
grayThreshold(useCounts, width, colors);
hvGfxVerticalSmear(hvg,xOff,yOff,width,lineHeight,colors,TRUE);
freeMem(useCounts);
if (gapUseCounts != NULL)
    {
    int midY = yOff + (tg->heightPer>>1);
    grayThreshold(gapUseCounts, width, colors);

    hvGfxVerticalSmear(hvg,xOff,midY,width,1,colors,TRUE);
    freeMem(gapUseCounts);
    }
}

void linkedFeaturesAverageDense(struct track *tg, int seqStart, int seqEnd,
                                struct hvGfx *hvg, int xOff, int yOff, int width,
                                MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense linked features items. */
{
if (vis == tvDense)
    linkedFeaturesDrawAverage(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
else
    linkedFeaturesDraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
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

void linkedFeaturesBoundsAndGrays(struct linkedFeatures *lf)
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

int linkedFeaturesItemStart(struct track *tg, void *item)
/* Return start chromosome coordinate of item. */
{
struct linkedFeatures *lf = item;
return lf->start;
}

int linkedFeaturesItemEnd(struct track *tg, void *item)
/* Return end chromosome coordinate of item. */
{
struct linkedFeatures *lf = item;
return lf->end;
}


void linkedFeaturesMapItem(struct track *tg, struct hvGfx *hvg, void *item,
				char *itemName, char *mapItemName, int start, int end,
				int x, int y, int width, int height)
/* Draw the mouseOver (aka statusLine) text from the mouseOver field of lf
 * Fallback to itemName if there is no mouseOver field.
 * (derived from genericMapItem) */
{
// Don't bother if we are imageV2 and a dense child.
if (theImgBox && tg->limitedVis == tvDense && tdbIsCompositeChild(tg->tdb))
    return;

struct linkedFeatures *lf = item;

char *newItemName   = (isEmpty(lf->mouseOver)) ? itemName: lf->mouseOver;

// copied from genericMapItem
char *directUrl = trackDbSetting(tg->tdb, "directUrl");
boolean withHgsid = (trackDbSetting(tg->tdb, "hgsid") != NULL);
char *trackName = tg->track;
if (tg->originalTrack != NULL)
    trackName = tg->originalTrack;
mapBoxHgcOrHgGene(hvg, start, end, x, y, width, height, trackName,
                  mapItemName, newItemName, directUrl, withHgsid, NULL);
}

int linkedFeaturesFixedTotalHeightNoOverflow(struct track *tg, enum trackVisibility vis)
{
return tgFixedTotalHeightOptionalOverflow(tg,vis, tl.fontHeight+1, tl.fontHeight, FALSE);
}


void linkedFeaturesMethods(struct track *tg)
/* Fill in track methods for linked features. */
{
tg->freeItems = linkedFeaturesFreeItems;
tg->drawItems = linkedFeaturesDraw;
tg->drawItemAt = linkedFeaturesDrawAt;
tg->itemName = linkedFeaturesName;
tg->drawItemLabel = genericDrawItemLabel;
tg->doItemMapAndArrows = genericItemMapAndArrows;
tg->mapItemName = linkedFeaturesName;
tg->mapItem = linkedFeaturesMapItem;
//tg->totalHeight = tgFixedTotalHeightNoOverflow;
tg->totalHeight = linkedFeaturesFixedTotalHeightNoOverflow;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = linkedFeaturesItemStart;
tg->itemEnd = linkedFeaturesItemEnd;
tg->itemNameColor = linkedFeaturesNameColor;
tg->nextPrevExon = linkedFeaturesNextPrevItem;
tg->nextPrevItem = linkedFeaturesLabelNextPrevItem;

if (trackDbSettingClosestToHomeOn(tg->tdb, "linkIdInName"))
    {
    tg->mapItemName = linkedFeaturesNameField1;
    tg->itemName = linkedFeaturesNameNotField1;
    }
}

int linkedFeaturesSeriesItemStart(struct track *tg, void *item)
/* Return start chromosome coordinate of item. */
{
struct linkedFeaturesSeries *lfs = item;
return lfs->start;
}

int linkedFeaturesSeriesItemEnd(struct track *tg, void *item)
/* Return end chromosome coordinate of item. */
{
struct linkedFeaturesSeries *lfs = item;
return lfs->end;
}

void linkedFeaturesSeriesMethods(struct track *tg)
/* Fill in track methods for linked features.series */
{
tg->freeItems = freeLinkedFeaturesSeriesItems;
tg->drawItems = linkedFeaturesSeriesDraw;
tg->drawItemAt = linkedFeaturesSeriesDrawAt;
tg->itemName = linkedFeaturesSeriesName;
tg->mapItemName = linkedFeaturesSeriesName;
tg->totalHeight = tgFixedTotalHeightNoOverflow;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = linkedFeaturesSeriesItemStart;
tg->itemEnd = linkedFeaturesSeriesItemEnd;
}

struct linkedFeatures *lfFromBedExtra(struct bed *bed, int scoreMin, int scoreMax)
/* Return a linked feature from a (full) bed. */
{
struct linkedFeatures *lf;
struct simpleFeature *sf, *sfList = NULL;
int grayIx = grayInRange(bed->score, scoreMin, scoreMax);
int *starts = bed->chromStarts, start;
int *sizes = bed->blockSizes;
int blockCount = bed->blockCount, i;

//assert(starts != NULL && sizes != NULL && blockCount > 0);

AllocVar(lf);
lf->grayIx = grayIx;
lf->name = cloneString(bed->name);
lf->orientation = orientFromChar(bed->strand[0]);
if (sizes == NULL)
    {
    AllocVar(sf);
    sf->start = bed->chromStart;
    sf->end = bed->chromEnd;
    sf->grayIx = grayIx;
    slAddHead(&sfList, sf);
    }
else
    {
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
    }
lf->components = sfList;
linkedFeaturesBoundsAndGrays(lf);
lf->tallStart = bed->thickStart;
lf->tallEnd = bed->thickEnd;
lf->score = bed->score;
return lf;
}

struct linkedFeaturesSeries *lfsFromColoredExonBed(struct bed *bed)
/* Convert a single BED 14 thing into a special linkedFeaturesSeries */
/* where each linkedFeatures is a colored block. */
{
struct linkedFeaturesSeries *lfs;
struct linkedFeatures *lfList = NULL;
int *starts = bed->chromStarts;
int *sizes = bed->blockSizes;
int blockCount = bed->blockCount, i;
int expCount = bed->expCount;
if (expCount != blockCount)
    errAbort("bed->expCount != bed->blockCount");
AllocVar(lfs);
lfs->name = cloneString(bed->name);
lfs->start = bed->chromStart;
lfs->end = bed->chromEnd;
lfs->orientation = orientFromChar(bed->strand[0]);
lfs->grayIx = grayInRange(bed->score, 0, 1000);
for (i = 0; i < blockCount; i++)
    {
    struct linkedFeatures *lf;
    struct simpleFeature *sf;
    AllocVar(lf);
    lf->name = cloneString(bed->name);
    lf->start = starts[i] + bed->chromStart;
    lf->end = lf->start + sizes[i];
    AllocVar(sf);
    sf->start = lf->start;
    sf->end = lf->end;
    lf->orientation = lfs->orientation;
    lf->components = sf;
    lf->grayIx = lfs->grayIx;
    /* Do some logic for thickStart/thickEnd. */
    lf->tallStart = lf->start;
    lf->tallEnd = lf->end;
    if ((bed->thickStart < lf->end) && (bed->thickStart >= lf->start))
	lf->tallStart = bed->thickStart;
    if ((bed->thickEnd < lf->end) && (bed->thickEnd >= lf->start))
	lf->tallEnd = bed->thickEnd;
    if (((bed->thickStart < lf->start) && (bed->thickEnd < lf->start)) ||
	((bed->thickStart > lf->end) && (bed->thickEnd > lf->end)))
	lf->tallStart = lf->end;
    /* Finally the business about the color. */
    lf->useItemRgb = TRUE;
    lf->filterColor = (unsigned)bed->expIds[i];
    slAddHead(&lfList, lf);
    }
slReverse(&lfList);
lfs->features = lfList;
lfs->noLine = FALSE;
return lfs;
}

struct linkedFeatures *lfFromBed(struct bed *bed)
{
return lfFromBedExtra(bed, 0, 1000);
}

struct linkedFeaturesSeries *lfsFromBed(struct lfs *lfsbed, char *tdbPslTable)
/* Create linked feature series object from database bed record */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row, rest[64];
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
    sqlSafef(rest, sizeof rest, "qName = '%s'", lfsbed->lfNames[i]);

    // use psl table from trackDb, if specified there
    char *pslTable = lfsbed->pslTable;
    if (tdbPslTable != NULL)
        pslTable = tdbPslTable;

    sr = hRangeQuery(conn, pslTable, lfsbed->chrom, lfsbed->lfStarts[i],
                     lfsbed->lfStarts[i] + lfsbed->lfSizes[i], rest, &rowOffset);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        struct psl *psl = pslLoad(row+rowOffset);
	lf = lfFromPsl(psl, FALSE);
	slAddHead(&lfList, lf);
	}
    sqlFreeResult(&sr);
    }
slReverse(&lfList);
sqlFreeResult(&sr);
hFreeConn(&conn);
lfs->features = lfList;
return lfs;
}

static struct linkedFeaturesSeries *lfsFromBedsInRange(struct track *tg, int start, int end,
                                                       char *chromName)
/* Return linked features from range of table. */
{
char *table = tg->track;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int rowOffset;
struct linkedFeaturesSeries *lfsList = NULL, *lfs;

char optionScoreStr[256]; /* Option -  score filter */
// Special case where getScoreFilterClause is too much trouble
safef(optionScoreStr, sizeof(optionScoreStr), "%s.%s", table,SCORE_FILTER);
// Special case where CloserToHome not appropriate
int optionScore = cartUsualInt(cart, optionScoreStr, 0);
if (optionScore > 0)
    {
    char extraWhere[128];
    sqlSafef(extraWhere, sizeof(extraWhere), "score >= %d", optionScore);
    sr = hOrderedRangeQuery(conn, table, chromName, start, end,
	extraWhere, &rowOffset);
    }
else
    {
    sr = hOrderedRangeQuery(conn, table, chromName, start, end,
	NULL, &rowOffset);
    }

char *pslTable = trackDbSetting(tg->tdb, "lfPslTable");
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct lfs *lfsbed = lfsLoad(row+rowOffset);
    lfs = lfsFromBed(lfsbed, pslTable);
    slAddHead(&lfsList, lfs);
    lfsFree(&lfsbed);
    }
slReverse(&lfsList);
sqlFreeResult(&sr);
hFreeConn(&conn);
return lfsList;
}

#ifndef GBROWSE
void loadBacEndPairs(struct track *tg)
/* Load up bac end pairs from table into track items. */
{
tg->items = lfsFromBedsInRange(tg, winStart, winEnd, chromName);
}

static Color dbRIPColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw dbRIP item */
{
struct dbRIP *thisItem = item;

if (startsWith("Other", thisItem->polySource))
    return tg->ixAltColor;
else
    return MG_BLUE;
}

static void loadDbRIP(struct track *tg)
/*	retroposons tracks load methods	*/
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
struct dbRIP *loadItem, *itemList = NULL;
struct dyString *query = dyStringNew(SMALLDYBUF);
char *option = NULL;
double freqLow = sqlFloat(cartCgiUsualString(cart, ALLELE_FREQ_LOW, "0.0"));
double freqHi = sqlFloat(cartCgiUsualString(cart, ALLELE_FREQ_HI, "1.0"));
boolean needJoin = FALSE;	/* need join to polyGenotype ?	*/

/*	safety check on bad user input, they may have set them illegally
 *	in which case reset them to defaults 0.0 <= f <= 1.0
 *	This reset also happens in hgTrackUi so they will see it reset there
 *	when they go back.
 */
if (! (freqLow < freqHi))
    {
    freqLow = 0.0;
    freqHi = 1.0;
    }
if ((freqLow > 0.0) || (freqHi < 1.0))
    needJoin = TRUE;

option = cartCgiUsualString(cart, ETHNIC_GROUP, ETHNIC_GROUP_DEFAULT);
if (differentString(option,ETHNIC_GROUP_DEFAULT))
    needJoin = TRUE;

if (needJoin)
    {
    sqlDyStringPrintf(query, "select %s.* from %s,polyGenotype where ",
	tg->table, tg->table);

    if (differentString(option,ETHNIC_GROUP_DEFAULT))
	{
	char *optionNot =
	    cartCgiUsualString(cart, ETHNIC_GROUP_EXCINC, ETHNIC_NOT_DEFAULT);
	if (sameWord(optionNot,"include"))
	    {
	    sqlDyStringPrintf(query, "%s.name=polyGenotype.name and "
		"polyGenotype.ethnicGroup=\"%s\" and ",
		    tg->table, option);
	    }
	    else
	    {
	    sqlDyStringPrintf(query, "%s.name=polyGenotype.name and "
		"polyGenotype.ethnicGroup!=\"%s\" and ",
		    tg->table, option);
	    }
	}
    if ((freqLow > 0.0) || (freqHi < 1.0))
	{
	    sqlDyStringPrintf(query,
		"polyGenotype.alleleFrequency>=\"%.1f\" and "
		    "polyGenotype.alleleFrequency<=\"%.1f\" and ",
			freqLow, freqHi);
	}
    }
else
    {
    sqlDyStringPrintf(query, "select * from %s where ", tg->table);
    }

hAddBinToQuery(winStart, winEnd, query);
sqlDyStringPrintf(query,
    "chrom=\"%s\" AND chromStart<%d AND chromEnd>%d ",
    chromName, winEnd, winStart);

option = cartCgiUsualString(cart, GENO_REGION, GENO_REGION_DEFAULT);

if (differentString(option,GENO_REGION_DEFAULT))
    sqlDyStringPrintf(query, " and genoRegion=\"%s\"", option);

option = cartCgiUsualString(cart, POLY_SOURCE, POLY_SOURCE_DEFAULT);
if (differentString(option,POLY_SOURCE_DEFAULT))
    {
    char *ucsc = "UCSC";
    char *other = "Other";
    char *which;
    if (sameWord(option,"yes"))
	which = ucsc;
    else
	which = other;
    sqlDyStringPrintf(query, " and polySource=\"%s\"", which);
    }

option = cartCgiUsualString(cart, POLY_SUBFAMILY, POLY_SUBFAMILY_DEFAULT);
if (differentString(option,POLY_SUBFAMILY_DEFAULT))
    sqlDyStringPrintf(query, " and polySubfamily=\"%s\"", option);

option = cartCgiUsualString(cart, dbRIP_DISEASE, DISEASE_DEFAULT);
if (differentString(option,DISEASE_DEFAULT))
    {
    if (sameWord(option,"no"))
	sqlDyStringPrintf(query, " and disease=\"NA\"");
    else
	sqlDyStringPrintf(query, " and disease!=\"NA\"");
    }

sqlDyStringPrintf(query, " group by %s.name", tg->table);

sr = sqlGetResult(conn, query->string);
rowOffset=1;

while ((row = sqlNextRow(sr)) != NULL)
    {
    loadItem = dbRIPLoad(row+rowOffset);
    slAddHead(&itemList, loadItem);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slSort(&itemList, bedCmp);
tg->items = itemList;
dyStringFree(&query);
}

static void atomDrawSimpleAt(struct track *tg, void *item,
                             struct hvGfx *hvg, int xOff, int y,
                             double scale, MgFont *font, Color color, enum trackVisibility vis);

int atomTotalHeight(struct track *tg, enum trackVisibility vis)
/* Most fixed height track groups will use this to figure out the height
 * they use. */
{
return tgFixedTotalHeightOptionalOverflow(tg,vis, 6*8, 6*8, FALSE);
}

int atomItemHeight(struct track *tg, void *item)
/* Return item height for fixed height track. */
{
return 6 * 8;
}

static void atomMethods(struct track *tg)
/* Fill in track methods for dbRIP tracks */
{
bedMethods(tg);
tg->drawItemAt = atomDrawSimpleAt;
tg->itemHeight = atomItemHeight;
tg->totalHeight = atomTotalHeight;
}

static void dbRIPMethods(struct track *tg)
/* Fill in track methods for dbRIP tracks */
{
bedMethods(tg);
tg->loadItems = loadDbRIP;
tg->itemColor = dbRIPColor;
tg->itemNameColor = dbRIPColor;
tg->itemLabelColor = dbRIPColor;
}

void bacEndPairsMethods(struct track *tg)
/* Fill in track methods for linked features.series */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadBacEndPairs;
}

#endif /* GBROWSE */


// The following few functions are shared by GAD, OMIM, DECIPHER, Superfamily.
// Those tracks need an extra label derived from item name -- the extra label
// is used as mouseover text for each item, and appears to the immediate left
// of the feature in full mode.
struct bedPlusLabel
{
    struct bed bed; // inline, so struct bedPlusLabel * can be cast to struct bed *.
    char *label;
};

typedef char *labelFromNameFunction(char *db, char *name);

static void bedPlusLabelLoad(struct track *tg, labelFromNameFunction func)
/* Load items from a bed table; if vis is pack or full, add extra label derived from item name. */
{
char *liftDb = cloneString(trackDbSetting(tg->tdb, "quickLiftDb"));

if (liftDb == NULL)
    {
    struct bedPlusLabel *itemList = NULL;
    struct sqlConnection *conn = hAllocConn(database);
    int rowOffset = 0;
    struct sqlResult *sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
    char **row = NULL;
    while ((row = sqlNextRow(sr)) != NULL)
        {
        struct bed *bed = bedLoad(row+rowOffset);
        struct bedPlusLabel *item = needMoreMem(bed, sizeof(struct bed), sizeof(struct bedPlusLabel));
        if (tg->visibility == tvPack || tg->visibility == tvFull)
            item->label = cloneString(func(database, item->bed.name));
        slAddHead(&itemList, item);
        }
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    slReverse(&itemList);
    slSort(&itemList, bedCmp);
    tg->items = itemList;
    }
else
    {
    /* if we're quicklifting get normal beds then add label */
    loadSimpleBedWithLoader(tg, bedLoad);
    struct bed *beds = tg->items;
    struct bedPlusLabel *bedLabels = NULL;

    struct bed *nextBed;
    for(; beds; beds = nextBed)
        {
        nextBed = beds->next;

        struct bedPlusLabel *bedLabel;
        AllocVar(bedLabel);

        bedLabel->bed = *beds;
        bedLabel->label = cloneString(func(liftDb, bedLabel->bed.name));
        slAddHead(&bedLabels, bedLabel);
        }
    slSort(&bedLabels, bedCmp);
    tg->items = bedLabels;
    }
}

void bedPlusLabelDrawAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y,
			       double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single bed item at position.  If vis is full, draw the associated label to the left
 * of the item. */
{
struct bedPlusLabel *bpl = item;
struct bed *bed = item;
int heightPer = tg->heightPer;
int s = max(bed->chromStart, winStart), e = min(bed->chromEnd, winEnd);
if (s > e)
    return;
int x1 = round((s-winStart)*scale) + xOff;
int x2 = round((e-winStart)*scale) + xOff;
int w = x2 - x1;
if (w < 1)
    w = 1;

if (tg->itemColor != NULL)
    color = tg->itemColor(tg, bed, hvg);

hvGfxBox(hvg, x1, y, w, heightPer, color);

// In full mode, draw bpl->label to the left of item:
if (vis == tvFull)
    {
    int textWidth = mgFontStringWidth(font, bpl->label);
    hvGfxTextRight(hvg, x1-textWidth-2, y, textWidth, heightPer, color, font, bpl->label);
    }
}

static void bedPlusLabelMapItem(struct track *tg, struct hvGfx *hvg, void *item,
				char *itemName, char *mapItemName, int start, int end,
				int x, int y, int width, int height)
/* Special mouseover text from item->label. (derived from genericMapItem) */
{
// Don't bother if we are imageV2 and a dense child.
if(!theImgBox || tg->limitedVis != tvDense || !tdbIsCompositeChild(tg->tdb))
    {
    struct bedPlusLabel *bpl = item;;
    char *mouseOverText = isEmpty(bpl->label) ? bpl->bed.name : bpl->label;
    mapBoxHc(hvg, start, end, x, y, width, height, tg->track, mapItemName, mouseOverText);
    }
}

static char *collapseRowsFromQuery(char *db, char *query, char *sep, int limit)
/* Return a string that is the concatenation of (up to limit) row[0]'s returned from query,
 * separated by sep.  Don't free the return value! */
{
static struct dyString *dy = NULL;
if (dy == NULL)
    dy = dyStringNew(0);
dyStringClear(dy);
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr = sqlMustGetResult(conn, query);
int i = 0;
char **row = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    eraseTrailingSpaces(row[0]);
    if (i != 0)
	dyStringAppend(dy, sep);
    dyStringAppend(dy, row[0]);
    if (i == limit)
	{
	dyStringAppend(dy, " ...");
	break;
	}
    i++;
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
return dy->string;
}
// end stuff shared by GAD, OMIM, DECIPHER, Superfamily


char *lfMapNameFromExtra(struct track *tg, void *item)
/* Return map name of item from extra field. */
{
struct linkedFeatures *lf = item;
return lf->extra;
}

#ifndef GBROWSE
static struct simpleFeature *sfFromGenePred(struct genePred *gp, int grayIx)
/* build a list of simpleFeature objects from a genePred */
{
struct simpleFeature *sfList = NULL, *sf;
unsigned *starts = gp->exonStarts;
unsigned *ends = gp->exonEnds;
int i, blockCount = gp->exonCount;

for (i=0; i<blockCount; ++i)
    {
    AllocVar(sf);
    sf->start = starts[i];
    sf->end = ends[i];
    sf->grayIx = grayIx;
    slAddHead(&sfList, sf);
    }
slReverse(&sfList);
return sfList;
}

struct linkedFeatures *linkedFeaturesFromGenePred(struct track *tg, struct genePred *gp, boolean extra)
/* construct a linkedFeatures object from a genePred */
{
int grayIx = maxShade;
struct linkedFeatures *lf;
AllocVar(lf);
lf->grayIx = grayIx;
lf->name = cloneString(gp->name);
if (extra && gp->name2)
    lf->extra = cloneString(gp->name2);
lf->orientation = orientFromChar(gp->strand[0]);

lf->components = sfFromGenePred(gp, grayIx);

if (tg->itemAttrTbl != NULL)
    lf->itemAttr = itemAttrTblGet(tg->itemAttrTbl, gp->name,
                                  gp->chrom, gp->txStart, gp->txEnd);

linkedFeaturesBoundsAndGrays(lf);

if (gp->cdsStart >= gp->cdsEnd)
    {
    lf->tallStart = gp->txEnd;
    lf->tallEnd = gp->txEnd;
    }
else
    {
    lf->tallStart = gp->cdsStart;
    lf->tallEnd = gp->cdsEnd;
    }
// Don't free gp; it might be used in the drawing phase by baseColor code.
lf->original = gp;
return lf;
}

static struct linkedFeatures *connectedLfFromGenePredInRangeExtra(
                                        struct track *tg, struct sqlConnection *conn, char *table,
                                        char *chrom, int start, int end, boolean extra)
/* Return linked features from range of a gene prediction table after
 * we have already connected to database. Optinally Set lf extra to
 * gene pred name2, to display gene name instead of transcript ID.*/
{
struct linkedFeatures *lfList = NULL;
struct genePredReader *gpr = NULL;
struct genePred *gp = NULL;
boolean nmdTrackFilter = sameString(trackDbSettingOrDefault(tg->tdb, "nmdFilter", "off"), "on");
char varName[SMALLBUF];
safef(varName, sizeof(varName), "%s.%s", table, HIDE_NONCODING_SUFFIX);
boolean hideNoncoding = cartUsualBoolean(cart, varName, HIDE_NONCODING_DEFAULT);  // TODO: Use cartUsualBooleanClosestToHome if tableName == tg->tdb->track
boolean doNmd = FALSE;
char buff[256];
safef(buff, sizeof(buff), "hgt.%s.nmdFilter",  tg->track);

/* Should we remove items that appear to be targets for nonsense
 * mediated decay? */
if(nmdTrackFilter)
    doNmd = cartUsualBoolean(cart, buff, FALSE);

if (tg->itemAttrTbl != NULL)
    itemAttrTblLoad(tg->itemAttrTbl, conn, chrom, start, end);

char noncodingClause[12024];
sqlSafef(noncodingClause, sizeof noncodingClause, "cdsStart != cdsEnd");
gpr = genePredReaderRangeQuery(conn, table, chrom, start, end, hideNoncoding ? noncodingClause : NULL);
while ((gp = genePredReaderNext(gpr)) != NULL)
    {
    if (doNmd && genePredNmdTarget(gp))
	{
	genePredFree(&gp);
	}
    else
        {
        slAddHead(&lfList, linkedFeaturesFromGenePred(tg, gp, extra));
        }
    }
slReverse(&lfList);
genePredReaderFree(&gpr);

if (tg->visibility != tvDense)
    slSort(&lfList, linkedFeaturesCmpStart);

return lfList;
}

struct linkedFeatures *connectedLfFromGenePredInRange(struct track *tg, struct sqlConnection *conn,
                                                      char *table, char *chrom, int start, int end)
/* Return linked features from range of a gene prediction table after
 * we have already connected to database. */
{
return connectedLfFromGenePredInRangeExtra(tg, conn, table, chrom,
                                                start, end, FALSE);
}

static void maybeLiftGenePred(struct track *tg, char *table, char *chrom, int start, int end, boolean extra)
/* Load a bunch of genePreds, perhaps quickLifting them. */
{
char *liftDb = cloneString(trackDbSetting(tg->tdb, "quickLiftDb"));

if (liftDb != NULL)
    {
    char *table;
    if (isCustomTrack(tg->table))
        {
        liftDb = CUSTOM_TRASH;
        table = trackDbSetting(tg->tdb, "dbTableName");
        }
    else
        table = tg->table;
    struct hash *chainHash = newHash(8);
    struct sqlConnection *conn = hAllocConn(liftDb);
    char *quickLiftFile = cloneString(trackDbSetting(tg->tdb, "quickLiftUrl"));

// using this loader on genePred tables with less than 15 fields may be a problem.
extern struct genePred *genePredExtLoad15(char **row);

    struct genePred *gpList = (struct genePred *)quickLiftSql(conn, quickLiftFile, table, chromName, winStart, winEnd,  NULL, NULL, (ItemLoader2)genePredExtLoad15, 0, chainHash);
    hFreeConn(&conn);

    calcLiftOverGenePreds( gpList, chainHash, 0.0, 1.0, TRUE, NULL, NULL,  TRUE, FALSE);
    struct genePred *gp = gpList;

    struct linkedFeatures *lfList = NULL;
    for(;gp; gp = gp->next)
        slAddHead(&lfList, linkedFeaturesFromGenePred(tg, gp, TRUE));
    slReverse(&lfList);
    tg->items = lfList;
    }
else
    {
    struct sqlConnection *conn = hAllocConn(database);
    tg->items = connectedLfFromGenePredInRangeExtra(tg, conn, tg->table,
                                            chromName, winStart, winEnd, extra);
    hFreeConn(&conn);
    }
}

struct linkedFeatures *lfFromGenePredInRange(struct track *tg, char *table,
                                             char *chrom, int start, int end)
/* Return linked features from range of a gene prediction table. */
{
maybeLiftGenePred(tg, tg->tdb->table, chrom, start, end, FALSE);
return tg->items;
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

char *genieName(struct track *tg, void *item)
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

void genieAltMethods(struct track *tg)
/* Make track of full length mRNAs. */
{
tg->itemName = genieName;
}

static struct dyString *genePredClassFilterBySetQuery(struct track *tg, char *classTable,
                                                      filterBy_t *filterBySet, struct linkedFeatures *lf)
/* construct the query for a standard genePred class filterBtSet */
{
char *clause = filterBySetClause(filterBySet);
if (clause == NULL)
    return NULL;

// don't care about a column value here, just if it exists, so get a constant
char *nameCol = trackDbSettingOrDefault(tg->tdb, GENEPRED_CLASS_NAME_COLUMN, GENEPRED_CLASS_NAME_COLUMN_DEFAULT);
struct dyString *dyQuery = sqlDyStringCreate("select 1 from %s where %s = \"%s\" and ", classTable, nameCol, lf->name);
dyStringAppend(dyQuery, clause);
freeMem(clause);
return dyQuery;
}

static boolean genePredClassFilterBySet(struct track *tg, char *classTable,
                                        filterBy_t *filterBySet, struct linkedFeatures *lf)
/* Check if an item passes a filterBySet filter  */
{
struct dyString *dyQuery = genePredClassFilterBySetQuery(tg, classTable, filterBySet, lf);
if (dyQuery == NULL)
    return TRUE;
struct sqlConnection *conn = hAllocConn(database);
boolean passesThroughFilter = sqlQuickNum(conn, dyQuery->string);
dyStringFree(&dyQuery);
hFreeConn(&conn);
return passesThroughFilter;
}

static boolean genePredClassFilterAcembly(struct track *tg, char *classTable,
                                          struct linkedFeatures *lf)
/* Check if an item passes a filterBySet filter  */
{
char *classString = addSuffix(tg->track, ".type");
char *classType = cartUsualString(cart, classString, acemblyEnumToString(0));
freeMem(classString);
enum acemblyOptEnum ct = acemblyStringToEnum(classType);
if (ct == acemblyAll)
    return TRUE;
struct sqlConnection *conn = hAllocConn(database);
char query[1024];
sqlSafef(query, sizeof(query),
      "select 1 from %s where (name = \"%s\") and (class = \"%s\")", classTable, lf->name, classType);
boolean passesThroughFilter = sqlQuickNum(conn, query);
hFreeConn(&conn);
return passesThroughFilter;
}

boolean genePredClassFilter(struct track *tg, void *item)
/* Returns true if an item should be added to the filter. */
{
struct linkedFeatures *lf = item;
char *classTable = trackDbSetting(tg->tdb, GENEPRED_CLASS_TBL);

if (classTable != NULL && hTableExists(database, classTable))
    {
    filterBy_t *filterBySet = filterBySetGet(tg->tdb,cart,NULL);
    if (filterBySet != NULL)
        {
        boolean passesThroughFilter = genePredClassFilterBySet(tg, classTable, filterBySet, lf);
        filterBySetFree(&filterBySet);
        return passesThroughFilter;
        }

    if (sameString(tg->table, "acembly"))
        {
        return genePredClassFilterAcembly(tg, classTable, lf);
        }
    }
return TRUE;
}

boolean knownGencodePseudoFilter(struct track *tg, void *item)
/* return TRUE is the user wants to see gencode pseudo genes. */
{
struct linkedFeatures *lf = item;
char buffer[1024];

sqlSafef(buffer, sizeof buffer, "kgId=\"%s\" and transcriptClass=\"pseudo\"", lf->name);
char *class = sqlGetField(database, "knownAttrs", "transcriptClass", buffer);

if (class != NULL)
    return TRUE;
return FALSE;
}

boolean knownGencodeClassFilter(struct track *tg, void *item)
{
struct linkedFeatures *lf = item;
char buffer[1024];

sqlSafef(buffer, sizeof buffer, "name=\"%s\" and value=\"basic\"", lf->name);
char *class = sqlGetField(database, "knownToTag", "value", buffer);

if (class != NULL)
    return TRUE;
return FALSE;
}

static void loadFrames(struct sqlConnection *conn, struct linkedFeatures *lf)
/* Load the CDS part of a genePredExt for codon display */
{
char query[4096];

for(; lf; lf = lf->next)
    {
    struct genePred *gp = lf->original;
    gp->optFields |= genePredExonFramesFld | genePredCdsStatFld | genePredCdsStatFld;
    sqlSafef(query, sizeof query, "select * from knownCds where name=\"%s\"", gp->name);

    struct sqlResult *sr = sqlMustGetResult(conn, query);
    char **row = NULL;
    int sizeOne;

    while ((row = sqlNextRow(sr)) != NULL)
	{
	gp->cdsStartStat = parseCdsStat(row[1]);
	gp->cdsEndStat = parseCdsStat(row[2]);
	int exonCount = sqlUnsigned(row[3]);
	if (exonCount != gp->exonCount)
	    errAbort("loadFrames: %s number of exonFrames (%d) != number of exons (%d)",
		     gp->name, exonCount, gp->exonCount);
	sqlSignedDynamicArray(row[4], &gp->exonFrames, &sizeOne);
	if (sizeOne != gp->exonCount)
	    errAbort("loadFrames: %s number of exonFrames (%d) != number of exons (%d)",
		     gp->name, sizeOne, gp->exonCount);
	}
    sqlFreeResult(&sr);
    }
}

void loadKnownGencode(struct track *tg)
/* Convert gene pred in window to linked feature. Include alternate name
 * in "extra" field (usually gene name) */
{
char varName[SMALLBUF];
safef(varName, sizeof(varName), "%s.show.comprehensive", tg->tdb->track);
boolean showComprehensive = cartUsualBoolean(cart, varName, FALSE);
safef(varName, sizeof(varName), "%s.show.pseudo", tg->tdb->track);
boolean showPseudo = cartUsualBoolean(cart, varName, FALSE);

struct sqlConnection *conn = hAllocConn(database);
tg->items = connectedLfFromGenePredInRangeExtra(tg, conn, tg->table,
                                        chromName, winStart, winEnd, TRUE);

/* filter items on selected criteria if filter is available */
if (!showComprehensive)
    filterItems(tg, knownGencodeClassFilter, "include");

if (!showPseudo)
    filterItems(tg, knownGencodePseudoFilter, "exclude");

/* if we're close enough to see the codon frames, we better load them! */
if (zoomedToCdsColorLevel)
    loadFrames(conn, tg->items);

hFreeConn(&conn);
}


void loadGenePredWithName2(struct track *tg)
/* Convert gene pred in window to linked feature. Include alternate name
 * in "extra" field (usually gene name) */
{
maybeLiftGenePred(tg, tg->tdb->table, chromName, winStart, winEnd, TRUE);

/* filter items on selected criteria if filter is available */
filterItems(tg, genePredClassFilter, "include");
}

void lookupKnownNames(struct linkedFeatures *lfList)
/* This converts the Genie ID to the HUGO name where possible. */
{
struct linkedFeatures *lf;
char query[256];
struct sqlConnection *conn = hAllocConn(database);

if (hTableExists(database, "knownMore"))
    {
    struct knownMore *km;
    struct sqlResult *sr;
    char **row;

    for (lf = lfList; lf != NULL; lf = lf->next)
	{
	sqlSafef(query, sizeof query, "select * from knownMore where transId = '%s'", lf->name);
	sr = sqlGetResult(conn, query);
	if ((row = sqlNextRow(sr)) != NULL)
	    {
	    km = knownMoreLoad(row);
	    lf->name = cloneString(km->name);
	    if (km->omimId)
	        lf->extra = km;
	    else
	        knownMoreFree(&km);
	    }
	sqlFreeResult(&sr);
	}
    }
else if (hTableExists(database, "knownInfo"))
    {
    for (lf = lfList; lf != NULL; lf = lf->next)
	{
	sqlSafef(query, sizeof query, "select name from knownInfo where transId = '%s'", lf->name);
	sqlQuickQuery(conn, query, lf->name, sizeof(lf->name));
	}
    }
hFreeConn(&conn);
}

void loadGenieKnown(struct track *tg)
/* Load up Genie known genes. */
{
tg->items = lfFromGenePredInRange(tg, "genieKnown", chromName, winStart, winEnd);
if (limitVisibility(tg) == tvFull)
    {
    lookupKnownNames(tg->items);
    }
}

Color genieKnownColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw known gene in. */
{
struct linkedFeatures *lf = item;

if (startsWith("AK.", lf->name))
    {
    static int colIx = 0;
    if (!colIx)
	colIx = hvGfxFindColorIx(hvg, 0, 120, 200);
    return colIx;
    }
else
    {
    return tg->ixColor;
    }
}

void genieKnownMethods(struct track *tg)
/* Make track of known genes. */
{
tg->loadItems = loadGenieKnown;
tg->itemName = genieName;
tg->itemColor = genieKnownColor;
}

char *hg17KgName(struct track *tg, void *item)
{
static char cat[128];
struct linkedFeatures *lf = item;
if (lf->extra != NULL)
    {
    safef(cat, sizeof cat, "%s",(char *)lf->extra);
    return cat;
    }
else
    return lf->name;
}

char *hg17KgMapName(struct track *tg, void *item)
/* Return un-abbreviated gene name. */
{
struct linkedFeatures *lf = item;
return lf->name;
}

void lookupHg17KgNames(struct linkedFeatures *lfList)
/* This converts the known gene ID to a gene symbol */
{
struct linkedFeatures *lf;
struct sqlConnection *conn = hAllocConn(database);
char *geneSymbol;
char *protDisplayId;
char *mimId;
char cond_str[256];
char *hg17KgLabel = cartUsualString(cart, "hg17Kg.label", "gene symbol");

boolean useGeneSymbol= sameString(hg17KgLabel, "gene symbol")
    || sameString(hg17KgLabel, "all");

boolean useKgId      = sameString(hg17KgLabel, "UCSC Known Gene ID")
    || sameString(hg17KgLabel, "all");

boolean useProtDisplayId = sameString(hg17KgLabel, "UniProt Display ID")
    || sameString(hg17KgLabel, "all");

boolean useMimId = sameString(hg17KgLabel, "OMIM ID")
    || sameString(hg17KgLabel, "all");

boolean useAll = sameString(hg17KgLabel, "all");

if (hTableExists(database, "kgXref"))
    {
    for (lf = lfList; lf != NULL; lf = lf->next)
	{
        struct dyString *name = dyStringNew(SMALLDYBUF);
        if (useGeneSymbol)
            {
            sqlSafef(cond_str, sizeof cond_str, "kgID='%s'", lf->name);
            geneSymbol = sqlGetField("hg17", "kgXref", "geneSymbol", cond_str);
            if (geneSymbol != NULL)
                {
                dyStringAppend(name, geneSymbol);
                if (useAll) dyStringAppendC(name, '/');
                }
            }
        if (useKgId)
            {
            dyStringAppend(name, lf->name);
            if (useAll) dyStringAppendC(name, '/');
	    }
        if (useProtDisplayId)
            {
	    sqlSafef(cond_str, sizeof(cond_str), "kgID='%s'", lf->name);
            protDisplayId = sqlGetField("hg17", "kgXref", "spDisplayID", cond_str);
            dyStringAppend(name, protDisplayId);
	    }
        if (useMimId && sqlTableExists(conn, refLinkTable))
            {
            sqlSafef(cond_str, sizeof(cond_str), "select cast(r.omimId as char) from kgXref,%s r where kgID = '%s' and kgXref.refseq = r.mrnaAcc and r.omimId != 0",refLinkTable, lf->name);
            mimId = sqlQuickString(conn, cond_str);
            if (mimId)
                dyStringAppend(name, mimId);
            }
        lf->extra = dyStringCannibalize(&name);
	}
    }
hFreeConn(&conn);
}

void loadHg17Kg(struct track *tg)
/* Load up known genes. */
{
enum trackVisibility vis = tg->visibility;
tg->items = lfFromGenePredInRange(tg, "hg17Kg", chromName, winStart, winEnd);
if (vis != tvDense)
    {
    lookupHg17KgNames(tg->items);
    }
limitVisibility(tg);
}

void hg17KgMethods(struct track *tg)
/* Make track of known genes. */
{
tg->loadItems   = loadHg17Kg;
tg->itemName    = hg17KgName;
tg->mapItemName = hg17KgMapName;
}

char *h1n1SeqName(struct track *tg, void *item)
{
struct linkedFeatures *lf = item;
struct sqlConnection *conn = hAllocConn(database);
char query[256], temp[256];
char *strain = NULL;
char *chp;

sqlSafef(query, sizeof(query), "select strain from h1n1SeqXref where seqId = '%s'", lf->name);
strain = sqlQuickString(conn, query);
chp = strstr(strain, "/2009");
if (chp != NULL) *chp = '\0';
hFreeConn(&conn);
safef(temp, sizeof(temp), "%s %s", strain+2, lf->name);
return(strdup(temp));
}

char *knownGeneName(struct track *tg, void *item)
{
static char cat[128];
struct linkedFeatures *lf = item;
if (lf->extra != NULL)
    {
    safef(cat, sizeof(cat), "%s",((struct knownGenesExtra *)(lf->extra))->name);
    return cat;
    }
else
    return lf->name;
}

char *knownGeneMapName(struct track *tg, void *item)
/* Return un-abbreviated gene name. */
{
char str2[255];
struct linkedFeatures *lf = item;
/* piggy back the protein ID (hgg_prot variable) on hgg_gene variable */
safef(str2, sizeof(str2), "%s&hgg_prot=%s", lf->name, ((struct knownGenesExtra *)(lf->extra))->hgg_prot);
return(cloneString(str2));
}

void lookupKnownGeneNames(struct track *tg, boolean isBigGenePred)
/* This converts the known gene ID to a gene symbol */
{
struct linkedFeatures *lfList = tg->items;
struct linkedFeatures *lf;
struct sqlConnection *conn = hAllocConn(database);
char *geneSymbol;
char *protDisplayId;
char *gencodeId;
char *mimId;
char cond_str[256];
boolean isGencode2 = trackDbSettingOn(tg->tdb, "isGencode2");

boolean useGeneSymbol= FALSE;
boolean useKgId      = FALSE;
boolean useProtDisplayId = FALSE;
boolean useMimId = FALSE;
boolean useGencodeId = FALSE;

struct hashEl *knownGeneLabels = cartFindPrefix(cart, "knownGene.label");
struct hashEl *label;
boolean labelStarted = FALSE;

if (isBigGenePred || hTableExists(database, "kgXref"))
    {
    char omimLabel[48];
    safef(omimLabel, sizeof(omimLabel), "omim%s", cartString(cart, "db"));

    if (knownGeneLabels == NULL)
        {
        useGeneSymbol = TRUE; /* default to gene name */
	/* set cart to match the default set */
        cartSetBoolean(cart, "knownGene.label.gene", TRUE);
        }

    for (label = knownGeneLabels; label != NULL; label = label->next)
        {
        if (endsWith(label->name, "gene") && differentString(label->val, "0"))
            useGeneSymbol = TRUE;
        else if (endsWith(label->name, "kgId") && differentString(label->val, "0"))
            useKgId = TRUE;
        else if (endsWith(label->name, "gencodeId") && differentString(label->val, "0"))
            useGencodeId = TRUE;
        else if (endsWith(label->name, "prot") && differentString(label->val, "0"))
            useProtDisplayId = TRUE;
        else if (endsWith(label->name, omimLabel) && differentString(label->val, "0"))
            useMimId = TRUE;
        else if (!endsWith(label->name, "gene") &&
                 !endsWith(label->name, "gencodeId") &&
                 !endsWith(label->name, "kgId") &&
                 !endsWith(label->name, "prot") &&
                 !endsWith(label->name, omimLabel) )
            {
            useGeneSymbol = TRUE;
            cartRemove(cart, label->name);
            }
        }

    for (lf = lfList; lf != NULL; lf = lf->next)
	{
        struct dyString *name = dyStringNew(SMALLDYBUF);
        struct knownGenesExtra *kgE;
        struct genePredExt *gp = lf->original;
        AllocVar(kgE);
        labelStarted = FALSE; /* reset between items */
        if (useGeneSymbol)
            {
            if ( isBigGenePred )
                {
                geneSymbol = gp->geneName;
                }
            else
                {
                sqlSafef(cond_str, sizeof cond_str,"kgID='%s'", lf->name);
                geneSymbol = sqlGetField(database, "kgXref", "geneSymbol", cond_str);
                }
            if (geneSymbol != NULL)
                {
                dyStringAppend(name, geneSymbol);
                }
            labelStarted = TRUE;
            }
        if (useGencodeId)
            {
            if (labelStarted) dyStringAppendC(name, '/');
            else labelStarted = TRUE;
            if ( isBigGenePred )
                {
                gencodeId = isGencode2 ? gp->name : gp->name2;
                }
            else
                {
                if (isGencode2)
                    gencodeId = lf->name;
                else
                    {
                    sqlSafef(cond_str, sizeof(cond_str), "name='%s'", lf->name);
                    gencodeId = sqlGetField(database, "knownGene", "alignID", cond_str);
                    }
                }
	    dyStringAppend(name, gencodeId);
	    }
        if (useKgId)
            {
            if (labelStarted) dyStringAppendC(name, '/');
            else labelStarted = TRUE;
            if (isGencode2)
                {
                sqlSafef(cond_str, sizeof(cond_str), "name='%s'", lf->name);
                char *ucId = sqlGetField(database, "knownGene", "alignID", cond_str);
                dyStringAppend(name, ucId);
                }
            else
                dyStringAppend(name, lf->name);
            }
        if (useProtDisplayId)
            {
            if (labelStarted) dyStringAppendC(name, '/');
            else labelStarted = TRUE;
            if ( isBigGenePred )
                {
                dyStringAppend(name, gp->geneName2);
                }
            else
                {
                if (lf->extra != NULL)
                    {
                    dyStringAppend(name, (char *)lf->extra);
                    }
                else
                    {
                    sqlSafef(cond_str, sizeof(cond_str), "kgID='%s'", lf->name);
                    protDisplayId = sqlGetField(database, "kgXref", "spDisplayID", cond_str);
                    dyStringAppend(name, protDisplayId);
                    }
                }
	    }
        if (useMimId && sqlTableExists(conn, refLinkTable))
            {
            if (labelStarted) dyStringAppendC(name, '/');
            else labelStarted = TRUE;
            sqlSafef(cond_str, sizeof(cond_str), "select cast(r.omimId as char) from kgXref,%s r where kgID = '%s' and kgXref.refseq = r.mrnaAcc and r.omimId != 0",refLinkTable, lf->name);
            mimId = sqlQuickString(conn, cond_str);
            if (mimId)
                dyStringAppend(name, mimId);
            }
        /* should this be a hash instead? */
        kgE->name = dyStringCannibalize(&name);
        kgE->hgg_prot = gp->name2;
        lf->extra = kgE;
        lf->label = kgE->name;
	}
    }
hFreeConn(&conn);
}

struct linkedFeatures *stripShortLinkedFeatures(struct linkedFeatures *list)
/* Remove linked features with no tall component from list. */
{
struct linkedFeatures *newList = NULL, *el, *next;
for (el = list; el != NULL; el = next)
    {
    next = el->next;
    if (el->tallStart < el->tallEnd)
        slAddHead(&newList, el);
    }
slReverse(&newList);
return newList;
}

#define BIT_BASIC       (1 << 0)        // transcript is in basic set
#define BIT_CANON       (1 << 1)        // transcript is in canonical set
#define BIT_PSEUDO      (1 << 2)        // transcript is a pseudogene

struct linkedFeatures *stripLinkedFeaturesWithBitInScore (struct linkedFeatures *list, unsigned bit)
/* Remove features that don't have this bit set in the score. */
{
struct linkedFeatures *newList = NULL, *el, *next;
for (el = list; el != NULL; el = next)
    {
    next = el->next;
    el->next = NULL;
    if (!((unsigned)el->score & bit))
        {
        slAddHead(&newList, el);
        }
    }
slReverse(&newList);
return newList;
}

struct linkedFeatures *stripLinkedFeaturesWithoutBitInScore (struct linkedFeatures *list, unsigned bit)
/* Remove features that don't have this bit set in the score. */
{
struct linkedFeatures *newList = NULL, *el, *next;
for (el = list; el != NULL; el = next)
    {
    next = el->next;
    el->next = NULL;
    if ((unsigned)el->score & bit)
        {
        slAddHead(&newList, el);
        }
    }
slReverse(&newList);
return newList;
}

struct linkedFeatures *stripLinkedFeaturesNotInHash(struct linkedFeatures *list, struct hash *hash)
/* Remove linked features not in hash from list. */
{
struct linkedFeatures *newList = NULL, *el, *next;
for (el = list; el != NULL; el = next)
    {
    next = el->next;
    if (hashLookup(hash, el->name))
        slAddHead(&newList, el);
    }
slReverse(&newList);
return newList;
}

static void loadKnownBigGenePred(struct track *tg, boolean isGencode)
/* Load knownGene features from a bigGenePred. */
{
int scoreMin = atoi(trackDbSettingClosestToHomeOrDefault(tg->tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingClosestToHomeOrDefault(tg->tdb, "scoreMax", "1000"));
struct linkedFeatures *lfList = NULL;
tg->parallelLoading = TRUE;  // set so bigBed code will look at bigDataUrl
bigBedAddLinkedFeaturesFromExt(tg, chromName, winStart, winEnd,
      scoreMin, scoreMax, TRUE, 12, &lfList, BIGBEDMAXIMUMITEMS);
slReverse(&lfList);
struct linkedFeatures *lf = lfList;
for(;lf;lf = lf->next)
    lf->isBigGenePred = TRUE;
struct linkedFeatures *newList = lfList;

if (isGencode)
    {
    char varName[SMALLBUF];
    safef(varName, sizeof(varName), "%s.show.comprehensive", tg->tdb->track);
    boolean showComprehensive = cartUsualBoolean(cart, varName, FALSE);
    if (!showComprehensive)
        newList = stripLinkedFeaturesWithoutBitInScore(lfList,  BIT_BASIC);
    safef(varName, sizeof(varName), "%s.show.pseudo", tg->tdb->track);
    boolean showPseudo = cartUsualBoolean(cart, varName, FALSE);
    if (!showPseudo)
        newList = stripLinkedFeaturesWithBitInScore(newList,  BIT_PSEUDO);
    }

slSort(&newList, linkedFeaturesCmp);
tg->items = newList;
tg->itemColor   = bigGenePredColor;
tg->itemNameColor = bigGenePredColor;
}

void loadKnownGene(struct track *tg)
/* Load up known genes. */
{
struct trackDb *tdb = tg->tdb;
boolean isGencode = trackDbSettingOn(tdb, "isGencode") || trackDbSettingOn(tdb, "isGencode2");
char *bigGenePred = trackDbSetting(tdb, "bigGeneDataUrl");
struct udcFile *file;
boolean isBigGenePred = FALSE;

if ((bigGenePred != NULL) && ((file = udcFileMayOpen(bigGenePred, udcDefaultDir())) != NULL))
    {
    isBigGenePred = TRUE;
    udcFileClose(&file);
    loadKnownBigGenePred(tg, isGencode);
    }
else if (!isGencode)
    loadGenePredWithName2(tg);
else
    loadKnownGencode(tg);

char varName[SMALLBUF];
safef(varName, sizeof(varName), "%s.show.noncoding", tdb->track);
boolean showNoncoding = cartUsualBoolean(cart, varName, TRUE);
safef(varName, sizeof(varName), "%s.show.spliceVariants", tdb->track);
boolean showSpliceVariants = cartUsualBoolean(cart, varName, TRUE);
if (!showNoncoding)
    tg->items = stripShortLinkedFeatures(tg->items);
if (!showSpliceVariants)
    {
    if (isBigGenePred)
        {
        tg->items = stripLinkedFeaturesWithoutBitInScore(tg->items,  BIT_CANON);
        }
    else
        {
        char *canonicalTable = trackDbSettingOrDefault(tdb, "canonicalTable", "knownCanonical");
        if (hTableExists(database, canonicalTable))
            {
            /* Create hash of items in canonical table in region. */
            struct sqlConnection *conn = hAllocConn(database);
            struct hash *hash = hashNew(0);
            char query[512];
            sqlSafef(query, sizeof(query),
                    "select transcript from %s where chrom=\"%s\" and chromStart < %d && chromEnd > %d",
                    canonicalTable, chromName, winEnd, winStart);
            struct sqlResult *sr = sqlGetResult(conn, query);
            char **row;
            while ((row = sqlNextRow(sr)) != NULL)
                hashAdd(hash, row[0], NULL);
            sqlFreeResult(&sr);
            hFreeConn(&conn);

            /* Get rid of non-canonical items. */
            tg->items = stripLinkedFeaturesNotInHash(tg->items, hash);
            hashFree(&hash);
            }
        }
    }
lookupKnownGeneNames(tg, isBigGenePred);
limitVisibility(tg);
}

Color knownGeneColorCalc(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw known gene in. */
{
struct linkedFeatures *lf = item;
int col = tg->ixColor;
struct rgbColor *normal = &(tg->color);
struct rgbColor lighter;
struct rgbColor lightest;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
char cond_str[256];
char *proteinID = NULL;
char *pdbID = NULL;
char *ans = NULL;
char *refAcc = NULL;

/* color scheme:

        Black:          If the gene has a corresponding PDB entry
        Dark blue:      If the gene has a corresponding SWISS-PROT entry
                        or has a corresponding "Reviewed" or "Validated" RefSeq entry
        Lighter blue:   If the gene has a corresponding RefSeq entry
        Lightest blue:  Eveything else
*/

lighter.r = (6*normal->r + 4*255) / 10;
lighter.g = (6*normal->g + 4*255) / 10;
lighter.b = (6*normal->b + 4*255) / 10;

lightest.r = (1*normal->r + 2*255) / 3;
lightest.g = (1*normal->g + 2*255) / 3;
lightest.b = (1*normal->b + 2*255) / 3;

/* set default to the lightest color */
col = hvGfxFindColorIx(hvg, lightest.r, lightest.g, lightest.b);

/* set color first according to RefSeq status (if there is a corresponding RefSeq) */
sqlSafef(cond_str, sizeof cond_str, "name='%s' ", lf->name);
refAcc = sqlGetField(database, "refGene", "name", cond_str);
if (refAcc != NULL)
    {
    if (sqlTableExists(conn, refSeqStatusTable))
        {
        sqlSafef(query, sizeof query, "select status from %s where mrnaAcc = '%s'", refSeqStatusTable, refAcc);
        sr = sqlGetResult(conn, query);
        if ((row = sqlNextRow(sr)) != NULL)
            {
	    if (startsWith("Reviewed", row[0]) || startsWith("Validated", row[0]))
                {
                /* Use the usual color */
                col = tg->ixColor;
                }
	    else
                {
                col = hvGfxFindColorIx(hvg, lighter.r, lighter.g, lighter.b);
                }
	    }
	sqlFreeResult(&sr);
	}
    }

/* set to dark blue if there is a corresponding Swiss-Prot protein */
sqlSafef(cond_str, sizeof cond_str, "name='%s'", lf->name);
proteinID= sqlGetField(database, "knownGene", "proteinID", cond_str);
if (proteinID != NULL && protDbName != NULL)
    {
    sqlSafef(cond_str, sizeof cond_str, "displayID='%s' AND biodatabaseID=1 ", proteinID);
    ans= sqlGetField(protDbName, "spXref3", "displayID", cond_str);
    if (ans != NULL)
        {
        col = tg->ixColor;
        }
    }

/* if a corresponding PDB entry exists, set it to black */
if (protDbName != NULL)
    {
    sqlSafef(cond_str, sizeof cond_str, "sp='%s'", proteinID);
    pdbID= sqlGetField(protDbName, "pdbSP", "pdb", cond_str);
    }

if (pdbID != NULL)
    {
    col = MG_BLACK;
    }

hFreeConn(&conn);
return(col);
}


Color knownGeneColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color for a known gene item - looking it up in table in
 * newer versions, and calculating it on fly in later versions. */
{
if (hTableExists(database, "kgColor"))
    {
    struct linkedFeatures *lf = item;
    int colIx = MG_BLUE;
    struct sqlConnection *conn = hAllocConn(database);
    char query[512];
    sqlSafef(query, sizeof(query), "select r,g,b from kgColor where kgID='%s'",
          lf->name);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row = sqlNextRow(sr);
    if (row != NULL)
         colIx = hvGfxFindColorIx(hvg, sqlUnsigned(row[0]), sqlUnsigned(row[1]), sqlUnsigned(row[2]));
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    return colIx;
    }
else
    return knownGeneColorCalc(tg, item, hvg);
}

void gencodeMethods(struct track *tg)
/* Make track of known genes. */
{
tg->loadItems   = loadKnownGene;
tg->itemName    = knownGeneName;
tg->mapItemName = knownGeneMapName;
tg->itemColor   = knownGeneColor;
}

void knownGeneMethods(struct track *tg)
/* Make track of known genes. */
{
boolean isGencode3 = trackDbSettingOn(tg->tdb, "isGencode3");

// knownGene is now a bigBed track
if (isGencode3)
    {
    char *words[3];
    words[0] = "bigGenePred";
    words[1] = "12";
    words[2] = 0;
    complexBedMethods(tg, tg->tdb, TRUE, 2, words);
    return;
    }

/* use loadGenePredWithName2 instead of loadKnownGene to pick up proteinID */
tg->loadItems   = loadKnownGene;
tg->itemName    = knownGeneName;
tg->mapItemName = knownGeneMapName;
tg->itemColor   = knownGeneColor;
}

void h1n1SeqMethods(struct track *tg)
/* Make track of known genes. */
{
/* use loadGenePredWithName2 instead of loadKnownGene to pick up proteinID */
tg->itemName    = h1n1SeqName;
}

char *superfamilyName(struct track *tg, void *item)
/* Return map name of the track item (used by hgc). */
{
char *name;
char *proteinName;
struct sqlConnection *conn = hAllocConn(database);
char conditionStr[256];

struct bed *sw = item;

// This is necessary because Ensembl kept changing their xref table definition
sqlSafef(conditionStr, sizeof conditionStr, "transcript_name='%s'", sw->name);
if (hTableExists(database, "ensGeneXref"))
    {
    proteinName = sqlGetField(database, "ensGeneXref", "translation_name", conditionStr);
    }
else if (hTableExists(database, "ensemblXref2"))
    {
    proteinName = sqlGetField(database, "ensemblXref2", "translation_name", conditionStr);
    }
else
    {
    if (hTableExists(database,  "ensemblXref"))
        {
        proteinName = sqlGetField(database, "ensemblXref", "translation_name", conditionStr);
        }
    else
	{
	if (hTableExists(database,  "ensTranscript"))
	    {
	    proteinName = sqlGetField(database,"ensTranscript","translation_name",conditionStr);
	    }
        else
            {
            if (hTableExists(database,  "ensemblXref3"))
                {
                sqlSafef(conditionStr, sizeof conditionStr, "transcript='%s'", sw->name);
                proteinName = sqlGetField(database, "ensemblXref3", "protein", conditionStr);
                }
            else
                {
                proteinName = cloneString("");
		}
	    }
	}
    }

name = cloneString(proteinName);
hFreeConn(&conn);

return(name);
}

static char *superfamilyNameLong(char *db, char *name)
/* Return domain names of an entry of a Superfamily track item,
   each item may have multiple names
   due to possibility of multiple domains. */
{
char query[256];
sqlSafef(query, sizeof(query), "select description from sfDescription where name='%s';", name);
return collapseRowsFromQuery(db, query, "; ", 100);
}

static void superfamilyLoad(struct track *tg)
/* Load superfamily items; in addition to items, store long description for mouseover/full mode. */
{
bedPlusLabelLoad(tg, superfamilyNameLong);
}

void superfamilyMethods(struct track *tg)
/* Fill in methods for (simple) bed tracks. */
{
tg->loadItems   = superfamilyLoad;
tg->drawItemAt  = bedPlusLabelDrawAt;
tg->mapItem     = bedPlusLabelMapItem;
tg->itemName    = superfamilyName;
tg->mapItemName = superfamilyName;
tg->nextPrevExon = simpleBedNextPrevEdge;
}

static char *gadDiseaseClassList(char *db, char *name)
/* Return list of diseases associated with a GAD entry */
{
char query[256];
sqlSafef(query, sizeof(query),
      "select distinct diseaseClassCode from gadAll "
      "where geneSymbol='%s' and association = 'Y' order by diseaseClassCode",
      name);
return collapseRowsFromQuery(db, query, ",", 20);
}

static char *gadDiseaseList(char *db, char *name)
/* Return list of diseases associated with a GAD entry */
{
char query[256];
sqlSafef(query, sizeof(query),
      "select distinct broadPhen from gadAll where geneSymbol='%s' and association = 'Y' "
      "order by broadPhen", name);
return collapseRowsFromQuery(db, query, "; ", 20);
}

static void gadLoad(struct track *tg)
/* Load GAD items as bed + label (used for mouseover; different label in draw routine!) */
{
bedPlusLabelLoad(tg, gadDiseaseList);
}

static void gadDrawAt(struct track *tg, void *item,
                      struct hvGfx *hvg, int xOff, int y,
                      double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single GAD item at position with extra label in full mode.
 * This is almost identical to bedPlusLabelDrawAt, but uses yet another function
 * to derive extra text in full mode. */
{
struct bed *bed = item;
int heightPer = tg->heightPer;
int x1 = round((double)((int)bed->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)bed->chromEnd-winStart)*scale) + xOff;
int w;

w = x2-x1;
if (w < 1)
    w = 1;
hvGfxBox(hvg, x1, y, w, heightPer, color);

if (vis == tvFull)
    {
    // New text for label in full mode:
    char *liftDb = cloneString(trackDbSetting(tg->tdb, "quickLiftDb"));
    char *db = (liftDb == NULL) ? database : liftDb;
    char *sDiseaseClasses = gadDiseaseClassList(db, bed->name);
    int textWidth = mgFontStringWidth(font, sDiseaseClasses);
    hvGfxTextRight(hvg, x1-textWidth-2, y, textWidth, heightPer, MG_BLACK, font, sDiseaseClasses);
    }
}

static char *decipherCnvsPhenotypeList(char *db, char *name)
/* Return list of diseases associated with a DECIPHER CNVs entry */
{
char query[256];
static char list[4096];
struct sqlConnection *conn = hAllocConn(database);
if (sqlFieldIndex(conn, "decipherRaw", "phenotypes") >= 0)
    {
    list[0] = '\0';
    sqlSafef(query, sizeof(query),
        "select phenotypes from decipherRaw where id='%s'", name);
    struct sqlResult *sr = sqlMustGetResult(conn, query);
    char **row = sqlNextRow(sr);
    if ((row != NULL) && strlen(row[0]) >= 1)
        {
        char *prettyResult = replaceChars(row[0], "|", "; ");
        safecpy(list, sizeof(list), prettyResult);
        // freeMem(prettyResult);
        }
    sqlFreeResult(&sr);
    }
else
    {
    sqlSafef(query, sizeof(query),
        "select distinct phenotype from decipherRaw where id='%s' order by phenotype", name);
    hFreeConn(&conn);
    return collapseRowsFromQuery(db, query, "; ", 20);
    }
hFreeConn(&conn);
return list;
}

void decipherCnvsLoad(struct track *tg)
/* Load DECIPHER CNVs items with extra labels from decipherPhenotypeList. */
{
bedPlusLabelLoad(tg, decipherCnvsPhenotypeList);
}

Color decipherCnvsColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw DECIPHER CNVs entry */
{
struct bed *bed = item;
int col = tg->ixColor;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
char cond_str[256];
char *decipherId = NULL;

/* So far, we can just remove "chr" from UCSC chrom names to get DECIPHER names */
char *decipherChrom = bed->chrom;
if (startsWithNoCase("chr", bed->chrom))
    decipherChrom += 3;

/* color scheme:
	RED:	If the entry is a deletion (mean ratio < 0)
	BLUE:	If the entry is a duplication (mean ratio > 0)
	GREY:	If the entry was not provided with a mean ratio value (or it's 0)
*/
sqlSafef(cond_str, sizeof(cond_str),"name='%s' ", bed->name);
decipherId = sqlGetField(database, "decipher", "name", cond_str);
if (decipherId != NULL)
    {
    if (hTableExists(database, "decipherRaw"))
        {
        sqlSafef(query, sizeof(query),
              "select mean_ratio from decipherRaw where id = '%s' and "
              "chr = '%s' and start = %d and end = %d",
	          decipherId, decipherChrom, bed->chromStart+1, bed->chromEnd);
	sr = sqlGetResult(conn, query);
        if ((row = sqlNextRow(sr)) != NULL)
            {
            if (isEmpty(row[0]) || (atof(row[0]) == 0.0))
                {
                col = MG_GRAY;
                }
	    else if (atof(row[0]) > 0)
                {
                col = MG_BLUE;
                }
	    else
		{
                col = MG_RED;
		}
	    }
	sqlFreeResult(&sr);
	}
    }
hFreeConn(&conn);
return(col);
}

static char *decipherSnvsPhenotypeList(char *db, char *name)
/* Return list of diseases associated with a DECIPHER SNVs entry */
{
char query[256];
static char list[4096];
struct sqlConnection *conn = hAllocConn(db);
if (sqlFieldIndex(conn, "decipherSnvsRaw", "phenotypes") >= 0)
    {
    list[0] = '\0';
    sqlSafef(query, sizeof(query),
        "select phenotypes from decipherSnvsRaw where id='%s'", name);
    struct sqlResult *sr = sqlMustGetResult(conn, query);
    char **row = sqlNextRow(sr);
    if ((row != NULL) && strlen(row[0]) >= 1)
        {
        char *prettyResult = replaceChars(row[0], "|", "; ");
        safecpy(list, sizeof(list), prettyResult);
        // freeMem(prettyResult);
        }
    sqlFreeResult(&sr);
    }
else
    {
    sqlSafef(query, sizeof(query),
        "select distinct phenotype from decipherSnvsRaw where id='%s' order by phenotype", name);
    hFreeConn(&conn);
    return collapseRowsFromQuery(db, query, "; ", 20);
    }
hFreeConn(&conn);
return list;
}

void decipherSnvsLoad(struct track *tg)
/* Load DECIPHER SNVs items with extra labels from decipherSnvsPhenotypeList. */
{
bedPlusLabelLoad(tg, decipherSnvsPhenotypeList);
}

Color decipherSnvsColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw DECIPHER SNV entry */
{
struct bed *bed = item;
int col = tg->ixColor;
char *db = database;
char *liftDb = cloneString(trackDbSetting(tg->tdb, "quickLiftDb"));
if (liftDb != NULL)
    db = liftDb;
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr;
char **row;
char query[256];
char cond_str[256];
char *decipherId = NULL;

/* So far, we can just remove "chr" from UCSC chrom names to get DECIPHER names */
char *decipherChrom = bed->chrom;
if (startsWithNoCase("chr", bed->chrom))
    decipherChrom += 3;

/* color scheme:
    BLACK:      If the entry is likely or definitely pathogenic
    DARK GRAY:  If the entry is uncertain or unknown
    LIGHT GRAY: If the entry is likely or definitely benign
*/

sqlSafef(cond_str, sizeof(cond_str),"name='%s' ", bed->name);
decipherId = sqlGetField(db, "decipherSnvs", "name", cond_str);

if (decipherId != NULL)
    {
    if (hTableExists(db, "decipherSnvsRaw"))
        {
        sqlSafef(query, sizeof(query), "select pathogenicity from decipherSnvsRaw where "
            "id = '%s' and chr = '%s' and start = '%d' and end = '%d'",
            decipherId, decipherChrom, bed->chromStart+1, bed->chromEnd);
	sr = sqlGetResult(conn, query);
        col = MG_GRAY;
        if ((row = sqlNextRow(sr)) != NULL)
            {
            char *ucPathogenicity = cloneString(row[0]);
            strUpper(ucPathogenicity);
	    if (endsWith(ucPathogenicity, "PATHOGENIC"))
                {
                col = MG_BLACK;
                }
	    else if (endsWith(ucPathogenicity, "BENIGN"))
		{
                col = MAKECOLOR_32(200,200,200);
		}
            // freeMem(ucPathogenicity);
	    }
	sqlFreeResult(&sr);
	}
    }
hFreeConn(&conn);
return(col);
}

void decipherCnvsMethods(struct track *tg)
/* Methods for DECIPHER CNVs track. */
{
tg->loadItems   = decipherCnvsLoad;
tg->itemColor   = decipherCnvsColor;
tg->itemNameColor = decipherCnvsColor;
tg->drawItemAt  = bedPlusLabelDrawAt;
tg->mapItem     = bedPlusLabelMapItem;
tg->nextPrevExon = simpleBedNextPrevEdge;
}

void decipherSnvsMethods(struct track *tg)
/* Methods for DECIPHER SNVs track. */
{
tg->loadItems   = decipherSnvsLoad;
tg->itemColor   = decipherSnvsColor;
tg->itemNameColor = decipherSnvsColor;
tg->drawItemAt  = bedPlusLabelDrawAt;
tg->mapItem     = bedPlusLabelMapItem;
tg->nextPrevExon = simpleBedNextPrevEdge;
}

char *emptyName(struct track *tg, void *item)
/* Return name of item. */
{
return("");
}

void rdmrMethods(struct track *tg)
/* Methods for R-DMR track. */
{
tg->itemName = emptyName;
}

void gadMethods(struct track *tg)
/* Methods for GAD track. */
{
tg->loadItems   = gadLoad;
tg->drawItemAt  = gadDrawAt;
tg->mapItem     = bedPlusLabelMapItem;
tg->nextPrevExon = simpleBedNextPrevEdge;
}

void rgdQtlDrawAt(struct track *tg, void *item,
                  struct hvGfx *hvg, int xOff, int y,
                  double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single rgdQtl item at position. */
{
struct bed *bed = item;
struct trackDb *tdb = tg->tdb;

if (tg->itemColor != NULL)
    color = tg->itemColor(tg, bed, hvg);
else if (tg->colorShades)
    {
    int scoreMin = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMin", "0"));
    int scoreMax = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMax", "1000"));
    color = tg->colorShades[grayInRange(bed->score, scoreMin, scoreMax)];
    }
if (color)
    {
    int heightPer = tg->heightPer;
    int s = max(bed->chromStart, winStart), e = min(bed->chromEnd, winEnd);
    if (s > e)
	return;
    int x1 = round((s-winStart)*scale) + xOff;
    int x2 = round((e-winStart)*scale) + xOff;
    int w = x2 - x1;
    if (w < 1)
	w = 1;
    hvGfxBox(hvg, x1, y, w, heightPer, color);
    if (tg->drawName && vis != tvSquish)
	{
	/* get description from rgdQtlLink table */
	struct sqlConnection *conn = hAllocConn(database);
	char cond_str[256];
	char linkTable[256];
	safef(linkTable, sizeof(linkTable), "%sLink", tg->table);
	sqlSafef(cond_str, sizeof(cond_str), "name='%s'", tg->itemName(tg, bed));
        char *s = sqlGetField(database, linkTable, "description", cond_str);
	hFreeConn(&conn);
	if (s == NULL)
	    s = bed->name;
	/* chop off text starting from " (human)" */
	char *chp = strstr(s, " (human)");
	if (chp != NULL) *chp = '\0';
	/* adjust range of text display to fit within the display window */
	int x3=x1, x4=x2;
	if (x3 < xOff) x3 = xOff;
	if (x4 > (insideWidth + xOff)) x4 = insideWidth + xOff;
	int w2 = x4 - x3;
	/* calculate how many characters we can squeeze into box */
	int boxWidth = w2 / mgFontCharWidth(font, 'm');
	int textWidth = strlen(s);
	if (boxWidth > 4)
	    {
	    Color textColor = hvGfxContrastingColor(hvg, color);
	    char *truncS = cloneString(s);
	    if (boxWidth < textWidth)
		strcpy(truncS+boxWidth-3, "...");
	    hvGfxTextCentered(hvg, x3, y, w2, heightPer, textColor, font, truncS);
	    freeMem(truncS);
	    }
	/* enable mouse over */
	char *directUrl = trackDbSetting(tdb, "directUrl");
	boolean withHgsid = (trackDbSetting(tdb, "hgsid") != NULL);
	mapBoxHgcOrHgGene(hvg, bed->chromStart, bed->chromEnd, x1, y, x2 - x1,
			  heightPer, tg->track, tg->mapItemName(tg, bed),
			  s, directUrl, withHgsid, NULL);
	}
    }
else
    errAbort("No color for track %s in rgdQtlDrawAt.", tg->track);
}

void rgdQtlMethods(struct track *tg)
/* Fill in methods for rgdQtl track. */
{
tg->drawItemAt  = rgdQtlDrawAt;
tg->drawName    = TRUE;
}

char *getOrganism(struct sqlConnection *conn, char *acc)
/* lookup the organism for an mrna, or NULL if not found */
{
// cache results, as this can be called a lot of times trying to pack tracks and test
// for row overflow
static struct hash *cache = NULL;
if (cache == NULL)
    cache = hashNew(0);
// N.B. NULL is a valid value in the cache
struct hashEl *cacheEl = hashLookup(cache, acc);
if (cacheEl == NULL)
    {
    char query[256];
    sqlSafef(query, sizeof query,
	"select o.name from %s g,%s o where g.acc = '%s' and g.organism = o.id", gbCdnaInfoTable, organismTable, acc);
    char *org = sqlQuickString(conn, query);
    if ((org != NULL) && (org[0] == '\0'))
        org = NULL;
    cacheEl = hashAdd(cache, acc, org);
    }
return cacheEl->val;
}

char *getOrganismShort(struct sqlConnection *conn, char *acc)
/* lookup the organism for an mrna, or NULL if not found.  This will
 * only return the genus, and only the first seven letters of that.
 * WARNING: static return */
{
return hOrgShortName(getOrganism(conn, acc));
}

char *getGeneName(struct sqlConnection *conn, char *acc)
/* get geneName from refLink or NULL if not found.
 * WARNING: static return */
{
static char nameBuf[256];
char query[256], *name = NULL;
if (sqlTableExists(conn,  refLinkTable))
    {
    /* remove the version number if any */
    static char accBuf[1024];
    safecpy(accBuf, sizeof accBuf, acc);
    chopSuffix(accBuf);

    sqlSafef(query, sizeof query, "select name from %s where mrnaAcc = '%s'", refLinkTable, accBuf);
    name = sqlQuickQuery(conn, query, nameBuf, sizeof(nameBuf));
    if ((name != NULL) && (name[0] == '\0'))
        name = NULL;
    }
return name;
}

char *getRgdGene2Symbol(struct sqlConnection *conn, char *acc)
/* get gene symbol from rgdGene2ToSymbol or NULL if not found.
 * WARNING: static return */
{
static char symbolBuf[256];
char query[256], *symbol = NULL;
if (hTableExists(database,  "rgdGene2ToSymbol"))
    {
    sqlSafef(query, sizeof query, "select geneSymbol from rgdGene2ToSymbol where rgdId = '%s'", acc);
    symbol = sqlQuickQuery(conn, query, symbolBuf, sizeof(symbolBuf));
    if ((symbol != NULL) && (symbol[0] == '\0'))
        symbol = NULL;
    }
return symbol;
}

char *rgdGene2Name(struct track *tg, void *item)
/* Get name to use for rgdGene2 item. */
{
struct linkedFeatures *lf = item;
if (lf->extra != NULL)
    return lf->extra;
else
    return lf->name;
}

char *rgdGene2MapName(struct track *tg, void *item)
/* Return un-abbreviated gene name. */
{
struct linkedFeatures *lf = item;
return lf->name;
}

void lookupRgdGene2Names(struct track *tg)
/* This converts the RGD Gene ID to a gene name where possible. */
{
struct linkedFeatures *lf;
struct sqlConnection *conn = hAllocConn(database);
boolean labelStarted = FALSE;
boolean useGeneName = FALSE;
boolean useAcc =  FALSE;

struct hashEl *rgdGene2Labels = cartFindPrefix(cart, "rgdGene2.label");
struct hashEl *label;

if (rgdGene2Labels == NULL)
    {
    useGeneName = TRUE; /* default to gene name */
    /* set cart to match the default set */
    cartSetBoolean(cart, "rgdGene2.label.gene", TRUE);
    }
for (label = rgdGene2Labels; label != NULL; label = label->next)
    {
    if (endsWith(label->name, "gene") && differentString(label->val, "0"))
        useGeneName = TRUE;
    else if (endsWith(label->name, "acc") && differentString(label->val, "0"))
        useAcc = TRUE;
    else if (!endsWith(label->name, "gene") &&
             !endsWith(label->name, "acc") )
        {
        useGeneName = TRUE;
        cartRemove(cart, label->name);
        }
    }

for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    struct dyString *name = dyStringNew(SMALLDYBUF);
    labelStarted = FALSE; /* reset for each item in track */
    if (useGeneName || useAcc)
        {
        char *org = getOrganismShort(conn, lf->name);
        if (org != NULL)
            dyStringPrintf(name, "%s ", org);
        }
    if (useGeneName)
        {
        char *gene = getRgdGene2Symbol(conn, lf->name);
        if (gene != NULL)
            {
            dyStringAppend(name, gene);
            }
        labelStarted = TRUE;
        }
    if (useAcc)
        {
        if (labelStarted) dyStringAppendC(name, '/');
        else labelStarted = TRUE;
        dyStringAppend(name, lf->name);
        }
    lf->extra = dyStringCannibalize(&name);
    }
hFreeConn(&conn);
}

void loadRgdGene2(struct track *tg)
/* Load up RGD genes. */
{
enum trackVisibility vis = tg->visibility;
tg->items = lfFromGenePredInRange(tg, tg->table, chromName, winStart, winEnd);
if (vis != tvDense)
    {
    lookupRgdGene2Names(tg);
    }
vis = limitVisibility(tg);
}

char *refGeneName(struct track *tg, void *item)
/* Get name to use for refGene item. */
{
struct linkedFeatures *lf = item;
if (lf->extra != NULL)
    return lf->extra;
else
    return lf->name;
}

char *ncbiRefGeneMapName(struct track *tg, void *item)
/* Return un-abbreviated gene name. */
{
struct linkedFeatures *lf = item;
char buffer[1024];
safecpy(buffer, sizeof buffer, lf->name);
return cloneString(buffer);
}

char *refGeneMapName(struct track *tg, void *item)
/* Return un-abbreviated gene name. */
{
struct linkedFeatures *lf = item;
char buffer[1024];
safecpy(buffer, sizeof buffer, lf->name);
chopSuffix(buffer);
return cloneString(buffer);
}

void lookupRefNames(struct track *tg)
/* This converts the refSeq accession to a gene name where possible. */
{
struct linkedFeatures *lf;
char *liftDb = cloneString(trackDbSetting(tg->tdb, "quickLiftDb"));
char *db = database;
if (liftDb)
    db = liftDb;
struct sqlConnection *conn = hAllocConn(db);
boolean isNative = !sameString(tg->table, "xenoRefGene");
boolean labelStarted = FALSE;
boolean useGeneName = FALSE;
boolean useAcc =  TRUE;
boolean useMim =  FALSE;
char trackLabel[1024];
char *labelString = tg->table;
boolean isRefGene = TRUE;

if (startsWith("ncbiRefSeq", labelString))
    {
    labelString="refSeqComposite";
    isRefGene = FALSE;
    }
else if (tdbIsCompositeChild(tg->tdb) && sameWord("refGene", labelString))
    {
    labelString="refSeqComposite";  // manage the case of existing refGene
    isRefGene = TRUE;               // track in composite without new tables
    }

safef(trackLabel, sizeof trackLabel, "%s.label", labelString);

struct hashEl *refGeneLabels = cartFindPrefix(cart, trackLabel);
struct hashEl *label;
char omimLabel[48];
safef(omimLabel, sizeof(omimLabel), "omim%s", cartString(cart, "db"));

if (refGeneLabels == NULL)
    {
    useGeneName = TRUE; /* default to gene name */
    }
for (label = refGeneLabels; label != NULL; label = label->next)
    {
    if (endsWith(label->name, "gene") && differentString(label->val, "0"))
        useGeneName = TRUE;
    else if (endsWith(label->name, "acc") && sameString(label->val, "0"))
        useAcc = FALSE;
    else if (endsWith(label->name, omimLabel) && differentString(label->val, "0"))
        useMim = TRUE;
    else if (!endsWith(label->name, "gene") &&
             !endsWith(label->name, "acc")  &&
             !endsWith(label->name, omimLabel) )
        {
        useGeneName = TRUE;
        }
    }
if (useGeneName)
    {
    /* set cart to match the default set */
    char setting[64];
    safef(setting, sizeof(setting), "%s.label.gene", labelString);
    cartSetBoolean(cart, setting, TRUE);
    }

for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    struct dyString *name = dyStringNew(SMALLDYBUF);
    labelStarted = FALSE; /* reset for each item in track */
    if ((useGeneName || useAcc || useMim) && !isNative)
        {
        char *org = getOrganismShort(conn, lf->name);
        if (org != NULL)
            dyStringPrintf(name, "%s ", org);
        }
    if (useGeneName)
        {
        char *gene;
        if  (isRefGene)
            gene = getGeneName(conn, lf->name);
        else
            gene = lf->extra;
        if (gene != NULL)
            {
            dyStringAppend(name, gene);
            }
        labelStarted = TRUE;
        }
    if (useAcc)
        {
        if (labelStarted) dyStringAppendC(name, '/');
        else labelStarted = TRUE;
        dyStringAppend(name, lf->name);
        }
    if (useMim)
        {
        char *mimId;
        char query[256];
        if  (isRefGene)
            {
            sqlSafef(query, sizeof(query), "select cast(omimId as char) from %s where mrnaAcc = '%s'", refLinkTable, lf->name);
            }
        else
            {
            sqlSafef(query, sizeof(query), "select omimId from ncbiRefSeqLink where id = '%s'", lf->name);
            }
        mimId = sqlQuickString(conn, query);
        if (labelStarted) dyStringAppendC(name, '/');
        else labelStarted = TRUE;
        if (mimId && differentString(mimId, "0"))
            dyStringAppend(name, mimId);
        }
    lf->extra = dyStringCannibalize(&name);
    }
hFreeConn(&conn);
}

void lookupProteinNames(struct track *tg)
/* This converts the knownGene accession to a gene name where possible. */
{
struct linkedFeatures *lf;
boolean useGene, useAcc, useSprot, usePos;
char geneName[SMALLBUF];
char accName[SMALLBUF];
char sprotName[SMALLBUF];
char posName[SMALLBUF];
char *blastRef;
char *buffer;
char *table = NULL;

safef(geneName, sizeof(geneName), "%s.geneLabel", tg->tdb->track);
safef(accName, sizeof(accName), "%s.accLabel", tg->tdb->track);
safef(sprotName, sizeof(sprotName), "%s.sprotLabel", tg->tdb->track);
safef(posName, sizeof(posName), "%s.posLabel", tg->tdb->track);
useGene= cartUsualBoolean(cart, geneName, TRUE);
useAcc= cartUsualBoolean(cart, accName, FALSE);
useSprot= cartUsualBoolean(cart, sprotName, FALSE);
usePos= cartUsualBoolean(cart, posName, FALSE);
blastRef = trackDbSettingOrDefault(tg->tdb, "blastRef", NULL);

if (blastRef != NULL)
    {
    char *thisDb = cloneString(blastRef);

    if ((table = strchr(thisDb, '.')) != NULL)
	{
	*table++ = 0;
	if (hTableExists(thisDb, table))
	    {
	    char query[256];
	    struct sqlResult *sr;
	    char **row;
	    struct sqlConnection *conn = hAllocConn(thisDb);
	    boolean added = FALSE;
	    char *ptr;

	    for (lf = tg->items; lf != NULL; lf = lf->next)
		{
		added = FALSE;

		buffer = needMem(strlen(lf->name) + 1);
		strcpy(buffer, lf->name);
		if ((char *)NULL != (ptr = strchr(buffer, '.')))
		    *ptr = 0;
		if (!startsWith("blastDm", tg->tdb->track))
		    sqlSafef(query, sizeof(query), "select geneId, refPos, extra1 from %s where acc = '%s'", blastRef, buffer);
		else
		    sqlSafef(query, sizeof(query), "select geneId, refPos from %s where acc = '%s'", blastRef, buffer);
		sr = sqlGetResult(conn, query);
		if ((row = sqlNextRow(sr)) != NULL)
		    {
		    lf->extra = needMem(strlen(lf->name) + strlen(row[0])+ strlen(row[1])+ strlen(row[2]) + 1);
		    if (useGene)
			{
			added = TRUE;
			strcat(lf->extra, row[0]);
			}
		    if (useAcc )
			{
			if (added)
			    strcat(lf->extra, "/");
			added = TRUE;
			strcat(lf->extra, lf->name);
			}
		    if (useSprot)
			{
			char *alias = uniProtFindPrimAcc(row[2]);

			if (added)
			    strcat(lf->extra, "/");
			added = TRUE;
			if (alias != NULL)
			    strcat(lf->extra, alias);
			else
			    strcat(lf->extra, row[2]);
			}
		    if (usePos)
			{
			char *startPos = strchr(row[1], ':');
			char *dash = strchr(row[1], '-');

			if ((startPos != NULL) && (dash != NULL))
			    {
			    *startPos++ = 0;
			    dash -= 3; /* divide by 1000 */
			    *dash = 0;
			    if (added)
				strcat(lf->extra, "/");
			    strcat(lf->extra, row[1]);
			    strcat(lf->extra, " ");
			    strcat(lf->extra, startPos);
			    strcat(lf->extra, "k");
			    }
			}
		    }
		sqlFreeResult(&sr);
		}
	    hFreeConn(&conn);
	    }
	}
    }
else
    for (lf = tg->items; lf != NULL; lf = lf->next)
	lf->extra = cloneString(lf->name);
}

void loadNcbiRefSeq(struct track *tg)
/* Load up RefSeq known genes. */
{
enum trackVisibility vis = tg->visibility;
loadGenePredWithName2(tg);
if (vis != tvDense)
    lookupRefNames(tg);
vis = limitVisibility(tg);
}

void loadRefGene(struct track *tg)
/* Load up RefSeq known genes. */
{
enum trackVisibility vis = tg->visibility;
tg->items = lfFromGenePredInRange(tg, tg->table, chromName, winStart, winEnd);
if (vis != tvDense)
    {
    lookupRefNames(tg);
    }
vis = limitVisibility(tg);
}

/* A spectrum from blue to red signifying the percentage of methylation */
Color bedMethylColorArray[] =
{
0xffff0000,  
0xffff4444,  
0xffaa4488,  
0xff884488,
0xff4444aa,
0xff0000ff,
};

void bedMethylMapItem(struct track *tg, struct hvGfx *hvg, void *item,
        char *itemName, char *mapItemName, int start, int end, int x, int y, int width, int height)
/* Return name of item */
{
struct bedMethyl *bed = item;
struct dyString *mouseOver = newDyString(4096);

dyStringPrintf(mouseOver,
    "<b>Coverage:</b> %s<br>"
    "<b>%%_modified:</b> %s<br>"
    "<b>N_modified:</b> %s<br>"
    "<b>N_canonical:</b> %s<br>"
    "<b>N_other:</b> %s<br>"
    "<b>N_delete:</b> %s<br>"
    "<b>N_fail:</b> %s<br>"
    "<b>N_diff:</b> %s<br>"
    "<b>N_nocall:</b> %s",
bed->nValidCov, bed->percMod, bed->nMod, bed->nCanon, bed->nOther, bed->nDelete, bed->nFail, bed->nDiff, bed->nNoCall);

mapBoxHgcOrHgGene(hvg, start, end, x, y, width, height, tg->track,
                  mapItemName, mouseOver->string, NULL, FALSE, NULL);
}

Color bedMethylColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw methylated site in. */
{
struct bedMethyl *bed = (struct bedMethyl *)item;

double percent = atof(bed->percMod) / 100.0;

unsigned index = percent * (sizeof(bedMethylColorArray) / sizeof(Color) - 1) + 0.5;

return bedMethylColorArray[index];
}

Color blastColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw protein in. */
{
struct linkedFeatures *lf = item;
int col = tg->ixColor;
char *acc;
char *colon, *pos;
char *buffer;
char cMode[SMALLBUF];
int colorMode;
char *blastRef;

if (baseColorGetDrawOpt(tg) > baseColorDrawOff)
    return tg->ixColor;

safef(cMode, sizeof(cMode), "%s.cmode", tg->tdb->track);
colorMode = cartUsualInt(cart, cMode, 0);

switch(colorMode)
    {
    case 0: /* pslScore */
	col = shadesOfGray[lf->grayIx];
	break;
    case 1: /* human position */
	acc = buffer = cloneString(lf->name);
	blastRef = trackDbSettingOrDefault(tg->tdb, "blastRef", NULL);
        if (blastRef != NULL)
	    {
	    char *thisDb = cloneString(blastRef);
	    char *table;

	    if ((table = strchr(thisDb, '.')) != NULL)
		{
		*table++ = 0;
		if (hTableExists(thisDb, table))
		    {
		    char query[256];
		    struct sqlResult *sr;
		    char **row;
		    struct sqlConnection *conn = hAllocConn(database);

		    if ((pos = strchr(acc, '.')) != NULL)
			*pos = 0;
		    sqlSafef(query, sizeof(query), "select refPos from %s where acc = '%s'", blastRef, buffer);
		    sr = sqlGetResult(conn, query);
		    if ((row = sqlNextRow(sr)) != NULL)
			{
			if (startsWith("chr", row[0]) && ((colon = strchr(row[0], ':')) != NULL))
			    {
			    *colon = 0;
			    col = getSeqColor(row[0], hvg);
			    }
			}
		    sqlFreeResult(&sr);
		    hFreeConn(&conn);
		    }
		}
	    }
	else
	    {
	    if ((pos = strchr(acc, '.')) != NULL)
		{
		pos += 1;
		if ((colon = strchr(pos, ':')) != NULL)
		    {
		    *colon = 0;
		    col = getSeqColor(pos, hvg);
		    }
		}
	    }
	break;
    case 2: /* black */
	col = MG_BLACK;
	break;
    }

tg->ixAltColor = col;
return(col);
}

Color refGeneColorByStatus(struct track *tg, char *name, struct hvGfx *hvg)
/* Get refseq gene color from refSeqStatus.
 * Reviewed, Validated -> normal, Provisional -> lighter,
 * Predicted, Inferred(other) -> lightest
 * If no refSeqStatus, color it normally.
 */
{
int col = tg->ixColor;
struct rgbColor *normal = &(tg->color);
struct rgbColor lighter, lightest;
char *liftDb = cloneString(trackDbSetting(tg->tdb, "quickLiftDb"));
char *db = (liftDb == NULL) ? database : liftDb;
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr;
char **row;
char query[256];

if (startsWith("ncbiRefSeq", trackHubSkipHubName(tg->table)))
    {
    sqlSafef(query, sizeof query, "select status from ncbiRefSeqLink where id = '%s'", name);
    }
else
    sqlSafef(query, sizeof query, "select status from %s where mrnaAcc = '%s'",
        refSeqStatusTable, name);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    if (startsWith("Reviewed", row[0]) || startsWith("Validated", row[0]))
        {
        /* Use the usual color */
        }
    else if (startsWith("Provisional", row[0]))
        {
        lighter.r = (6*normal->r + 4*255) / 10;
        lighter.g = (6*normal->g + 4*255) / 10;
        lighter.b = (6*normal->b + 4*255) / 10;
        lighter.a = normal->a;
        col = hvGfxFindRgb(hvg, &lighter);
        }
    else
        {
        lightest.r = (1*normal->r + 2*255) / 3;
        lightest.g = (1*normal->g + 2*255) / 3;
        lightest.b = (1*normal->b + 2*255) / 3;
        lightest.a = normal->a;
        col = hvGfxFindRgb(hvg, &lightest);
        }
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return col;
}

Color refGeneColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw refseq gene in. */
{
struct linkedFeatures *lf = item;

/* allow itemAttr to override coloring */
if (lf->itemAttr != NULL)
    return hvGfxFindColorIx(hvg, lf->itemAttr->colorR, lf->itemAttr->colorG, lf->itemAttr->colorB);

/* If refSeqStatus is available, use it to determine the color.
 * Reviewed, Validated -> normal, Provisional -> lighter,
 * Predicted, Inferred(other) -> lightest
 * If no refSeqStatus, color it normally.
 */
char *liftDb = cloneString(trackDbSetting(tg->tdb, "quickLiftDb"));
char *db = (liftDb == NULL) ? database : liftDb;
struct sqlConnection *conn = hAllocConn(db);
Color color = tg->ixColor;
if (sqlTableExists(conn,  refSeqStatusTable) || hTableExists(db,  "ncbiRefSeqLink"))
    color = refGeneColorByStatus(tg, lf->name, hvg);
hFreeConn(&conn);
return color;
}

void ncbiRefSeqMethods(struct track *tg)
/* Make NCBI Genes track */
{
tg->loadItems = loadNcbiRefSeq;
tg->itemName = refGeneName;
tg->mapItemName = ncbiRefGeneMapName;
tg->itemColor = refGeneColor;
}

void refGeneMethods(struct track *tg)
/* Make track of known genes from refSeq. */
{
tg->loadItems = loadRefGene;
tg->itemName = refGeneName;
tg->mapItemName = refGeneMapName;
tg->itemColor = refGeneColor;
}

void rgdGene2Methods(struct track *tg)
/* Make track of RGD genes. */
{
tg->loadItems = loadRgdGene2;
tg->itemName = rgdGene2Name;
tg->mapItemName = rgdGene2MapName;
}

boolean filterNonCoding(struct track *tg, void *item)
/* Returns TRUE if the item passes the filter */
{
char condStr[256];
char *bioType;
struct linkedFeatures *lf = item;
int i = 0;
struct sqlConnection *conn = hAllocConn(database);

/* Find the biotype for this item */
sqlSafef(condStr, sizeof condStr, "name='%s'", lf->name);
bioType = sqlGetField(database, "ensGeneNonCoding", "biotype", condStr);
hFreeConn(&conn);

/* check for this type in the array and use its index to find whether this
   type is in the cart and checked for inclusion */
for (i = 0; i < nonCodingTypeDataNameSize; i++)
    {
    cartCgiUsualBoolean(cart, nonCodingTypeIncludeStrings[i], TRUE);
    if (!sameString(nonCodingTypeDataName[i], bioType))
        {
        continue;
        }
    return nonCodingTypeIncludeCart[i];
    }
return TRUE;
}

void loadEnsGeneNonCoding(struct track *tg)
/* Load the ensGeneNonCoding items filtered according to types checked on
   the nonCodingUi */
{
int i = 0;
/* update cart variables based on checkboxes */
for (i = 0; i < nonCodingTypeIncludeCartSize; i++)
    {
    nonCodingTypeIncludeCart[i] = cartUsualBoolean(cart, nonCodingTypeIncludeStrings[i], nonCodingTypeIncludeDefault[i]);
    }
/* Convert genePred in window to linked feature */
tg->items = lfFromGenePredInRange(tg, tg->table, chromName, winStart, winEnd);
/* filter items on selected criteria if filter is available */
filterItems(tg, filterNonCoding, "include");
}

Color ensGeneNonCodingColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw Ensembl non-coding gene/pseudogene in. */
{
char condStr[255];
char *bioType;
Color color = {MG_GRAY};  /* Set default to gray */
struct rgbColor hAcaColor = {0, 128, 0, 255}; /* darker green, per request by Weber */
Color hColor;
struct sqlConnection *conn;
char *name;

conn = hAllocConn(database);
hColor = hvGfxFindColorIx(hvg, hAcaColor.r, hAcaColor.g, hAcaColor.b);
name = tg->itemName(tg, item);
sqlSafef(condStr, sizeof condStr, "name='%s'", name);
bioType = sqlGetField(database, "ensGeneNonCoding", "biotype", condStr);

if (sameWord(bioType, "miRNA"))    color = MG_RED;
if (sameWord(bioType, "misc_RNA")) color = MG_BLACK;
if (sameWord(bioType, "snRNA"))    color = MG_BLUE;
if (sameWord(bioType, "snoRNA"))   color = MG_MAGENTA;
if (sameWord(bioType, "rRNA"))     color = MG_CYAN;
if (sameWord(bioType, "scRNA"))    color = MG_YELLOW;
if (sameWord(bioType, "Mt_tRNA"))  color = MG_GREEN;
if (sameWord(bioType, "Mt_rRNA"))  color = hColor;

/* Pseudogenes in the zebrafish will be coloured brown */
if (sameWord(bioType, "pseudogene")) color = hvGfxFindColorIx(hvg, brownColor.r,
    brownColor.g, brownColor.b);

hFreeConn(&conn);
return(color);
}

void ensGeneNonCodingMethods(struct track *tg)
/* Make track of Ensembl predictions. */
{
tg->items = lfFromGenePredInRange(tg, tg->table, chromName, winStart, winEnd);
tg->itemColor = ensGeneNonCodingColor;
tg->loadItems = loadEnsGeneNonCoding;
}

int cDnaReadDirectionForMrna(struct sqlConnection *conn, char *acc)
/* Return the direction field from the mrna table for accession
   acc. Return -1 if not in table.*/
{
int direction = -1;
char query[512];
char buf[SMALLBUF], *s = NULL;
sqlSafef(query, sizeof query, "select direction from %s where acc='%s'", gbCdnaInfoTable, acc);
if ((s = sqlQuickQuery(conn, query, buf, sizeof(buf))) != NULL)
    {
    direction = atoi(s);
    }
return direction;
}

void orientEsts(struct track *tg)
/* Orient ESTs from the estOrientInfo table.  */
{
struct linkedFeatures *lf = NULL, *lfList = tg->items;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = 0;
struct estOrientInfo ei;
int estOrient = 0;
struct hash *orientHash = NULL;

if((lfList != NULL) && hTableExists(database,  "estOrientInfo"))
    {
    /* First load up a hash with the orientations. That
       way we only query the database once rather than
       hundreds or thousands of times. */
    int hashSize = (log(slCount(lfList))/log(2)) + 1;
    orientHash = newHash(hashSize);
    sr = hRangeQuery(conn, "estOrientInfo", chromName,
		     winStart, winEnd, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	estOrientInfoStaticLoad(row + rowOffset, &ei);
	hashAddInt(orientHash, ei.name, ei.intronOrientation);
	}
    sqlFreeResult(&sr);

    /* Now lookup orientation of each est. */
    for(lf = lfList; lf != NULL; lf = lf->next)
        {
        estOrient = hashIntValDefault(orientHash, lf->name, 0);
        if (estOrient < 0)
            lf->orientation = -1 * lf->orientation;
        else if (estOrient == 0)
            lf->orientation = 0;  // not known, don't display chevrons
        }
    hashFree(&orientHash);
    }
hFreeConn(&conn);
}

void linkedFeaturesAverageDenseOrientEst(struct track *tg, int seqStart, int seqEnd,
                                         struct hvGfx *hvg, int xOff, int yOff, int width,
                                         MgFont *font, Color color, enum trackVisibility vis)
/* Draw dense linked features items. */
{
if(vis == tvSquish || vis == tvPack || vis == tvFull)
    orientEsts(tg);

if (vis == tvDense)
    linkedFeaturesDrawAverage(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
else
    linkedFeaturesDraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
}



char *sanger22Name(struct track *tg, void *item)
/* Return Sanger22 name. */
{
struct linkedFeatures *lf = item;
char *full = lf->name;
static char abbrev[SMALLBUF];

strncpy(abbrev, full, sizeof(abbrev));
abbr(abbrev, "Em:");
abbr(abbrev, ".C22");
//abbr(abbrev, ".mRNA");
return abbrev;
}


void sanger22Methods(struct track *tg)
/* Make track of Sanger's chromosome 22 gene annotations. */
{
tg->itemName = sanger22Name;
}

Color vegaColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw vega gene/pseudogene in. */
{
struct linkedFeatures *lf = item;
Color col = tg->ixColor;
struct rgbColor *normal = &(tg->color);
struct rgbColor lighter, lightest;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
struct dyString *dy = dyStringNew(256);
struct dyString *dy2 = dyStringNew(256);
char *infoTable = NULL;
char *infoCol = NULL;
int tblIx;
boolean fieldExists = FALSE;

/* If vegaInfo is available, use it to determine the color:
 * Colors are based on those used by the RefSeq track.
 *  {Known, Novel_CDS, Novel_Transcript} - dark blue
 *  {Putative,Ig_Segment} - medium blue
 *  {Predicted_gene,Pseudogene,Ig_Pseudogene_Segment} - light blue
 *  None of the above - gray
 * If no vegaInfo, color it normally.
 * For Zebrafish, the info table is called vegaInfoZfish and the
 * categories are now different in zebrafish and in human from hg17 onwards.
 * KNOWN and NOVEL - dark blue
 * PUTATIVE - medium blue
 * PREDICTED - light blue
 * others e.g. UNCLASSIFIED will be gray.
 */

if (hTableExists(database,  "vegaInfo"))
    dyStringPrintf(dy, "%s", "vegaInfo");
else if (sameWord(organism, "Zebrafish") && hTableExists(database, "vegaInfoZfish"))
    dyStringPrintf(dy, "%s", "vegaInfoZfish");
if (dy != NULL)
    infoTable = dyStringCannibalize(&dy);

/* for some assemblies coloring is based on entries in the confidence column
   where vegaInfo has this column otherwise the method column is used.
   Check the field list for these columns. */

if (isNotEmpty(infoTable))
    {
    tblIx = sqlFieldIndex(conn, infoTable, "confidence");
    fieldExists = (tblIx > -1) ? TRUE : FALSE;
    if (fieldExists)
       dyStringPrintf(dy2, "%s", "confidence");
    else
       {
       tblIx = sqlFieldIndex(conn, infoTable, "method");
       fieldExists = (tblIx > -1) ? TRUE : FALSE;
       dyStringPrintf(dy2, "%s", "method");
       }
    }

if (isNotEmpty(infoTable)  && fieldExists)
    {
    /* use the value for infoCol defined above */
    infoCol = dyStringCannibalize(&dy2);
    sqlSafef(query, sizeof query, "select %s from %s where transcriptId = '%s'",
	     infoCol, infoTable, lf->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
	if (sameWord("Known", row[0]) ||
            sameWord("KNOWN", row[0]) ||
            sameWord("Novel_CDS", row[0]) ||
            sameWord("Novel_Transcript", row[0]) ||
            sameWord("NOVEL", row[0]))
	    {
	    /* Use the usual color (dark blue) */
	    }
	else if (sameWord("Putative", row[0]) ||
                 sameWord("Ig_Segment", row[0]) ||
                 sameWord("PUTATIVE", row[0]))
	    {
	    lighter.r = (6*normal->r + 4*255) / 10;
	    lighter.g = (6*normal->g + 4*255) / 10;
	    lighter.b = (6*normal->b + 4*255) / 10;
	    col = hvGfxFindRgb(hvg, &lighter);
	    }
	else if (sameWord("Predicted_gene", row[0]) ||
		 sameWord("Pseudogene", row[0]) ||
		 sameWord("Ig_Pseudogene_Segment", row[0]) ||
                 sameWord("PREDICTED", row[0]))
	    {
	    lightest.r = (1*normal->r + 2*255) / 3;
	    lightest.g = (1*normal->g + 2*255) / 3;
	    lightest.b = (1*normal->b + 2*255) / 3;
	    col = hvGfxFindRgb(hvg, &lightest);
	    }
	else
	    {
	    col = lightGrayIndex();
	    }
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
return(col);
}

char *bdgpGeneName(struct track *tg, void *item)
/* Return bdgpGene symbol. */
{
struct linkedFeatures *lf = item;
char *name = cloneString(lf->name);
char infoTable[128];
safef(infoTable, sizeof(infoTable), "%sInfo", tg->table);
if (hTableExists(database,  infoTable))
    {
    struct sqlConnection *conn = hAllocConn(database);
    char *symbol = NULL;
    char *ptr = strchr(name, '-');
    char query[256];
    char buf[SMALLBUF];
    if (ptr != NULL)
	*ptr = 0;
    sqlSafef(query, sizeof(query),
	  "select symbol from %s where bdgpName = '%s';", infoTable, name);
    symbol = sqlQuickQuery(conn, query, buf, sizeof(buf));
    hFreeConn(&conn);
    if (symbol != NULL)
	{
	char *ptr = stringIn("{}", symbol);
	if (ptr != NULL)
	    *ptr = 0;
	freeMem(name);
	name = cloneString(symbol);
	}
    }
return(name);
}

void bdgpGeneMethods(struct track *tg)
/* Special handling for bdgpGene items. */
{
tg->itemName = bdgpGeneName;
}

char *flyBaseGeneName(struct track *tg, void *item)
/* Return symbolic name for FlyBase gene with lookup table for name->symbol
 * specified in trackDb. */
{
struct linkedFeatures *lf = item;
char *name = cloneString(lf->name);
char *infoTable = trackDbSettingOrDefault(tg->tdb, "symbolTable", "");
if (isNotEmpty(infoTable) && hTableExists(database,  infoTable))
    {
    struct sqlConnection *conn = hAllocConn(database);
    char *symbol = NULL;
    char query[256];
    char buf[SMALLBUF];
    sqlSafef(query, sizeof(query),
	  "select symbol from %s where name = '%s';", infoTable, name);
    symbol = sqlQuickQuery(conn, query, buf, sizeof(buf));
    hFreeConn(&conn);
    if (isNotEmpty(symbol))
	{
	freeMem(name);
	name = cloneString(symbol);
	}
    }
return(name);
}

void flyBaseGeneMethods(struct track *tg)
/* Special handling for FlyBase genes. */
{
tg->itemName = flyBaseGeneName;
}

char *sgdGeneName(struct track *tg, void *item)
/* Return sgdGene symbol. */
{
struct sqlConnection *conn = hAllocConn(database);
struct linkedFeatures *lf = item;
char *name = lf->name;
char *symbol = NULL;
char query[256];
char buf[SMALLBUF];
sqlSafef(query, sizeof(query),
      "select value from sgdToName where name = '%s'", name);
symbol = sqlQuickQuery(conn, query, buf, sizeof(buf));
hFreeConn(&conn);
if (symbol != NULL)
    name = symbol;
return(cloneString(name));
}

void sgdGeneMethods(struct track *tg)
/* Special handling for sgdGene items. */
{
tg->itemName = sgdGeneName;
}

void adjustBedScoreGrayLevel(struct trackDb *tdb, struct bed *bed, int scoreMin, int scoreMax)
/* For each distinct trackName passed in, check cart for trackName.minGrayLevel; if
 * that is different from the gray level implied by scoreMin's place in [0..scoreMax],
 * then linearly transform bed->score from the range of [scoreMin,scoreMax] to
 * [(cartMinGrayLevel*scoreMax)/maxShade,scoreMax].
 * Note: this assumes that scoreMin and scoreMax are constant for each track. */
{
static char *prevTrackName = NULL;
static int scoreMinGrayLevel = 0;
static int cartMinGrayLevel = 0; /* from cart, or trackDb setting */
static float newScoreMin = 0;

if (tdb->track != prevTrackName)
    {
    scoreMinGrayLevel = scoreMin * maxShade/scoreMax;
    if (scoreMinGrayLevel <= 0)
        scoreMinGrayLevel = 1;
    char *setting = trackDbSettingClosestToHome(tdb, MIN_GRAY_LEVEL);
    cartMinGrayLevel = cartUsualIntClosestToHome(cart, tdb, FALSE, MIN_GRAY_LEVEL,
                                setting ? atoi(setting) : scoreMinGrayLevel);
    newScoreMin = cartMinGrayLevel * scoreMax/maxShade;
    prevTrackName = tdb->track;
    }
if (cartMinGrayLevel != scoreMinGrayLevel)
    {
    float realScore = (float)(bed->score - scoreMin) / (scoreMax - scoreMin);
    bed->score = newScoreMin + (realScore * (scoreMax - newScoreMin)) + 0.5;
    }
else if (scoreMin != 0 && scoreMax == 1000) // Changes gray level even when
    {                                       // UI does not allow selecting it.
    float realScore = (float)(bed->score) / 1000;
    bed->score = scoreMin + (realScore * (scoreMax - scoreMin)) + 0.5;
    }
}

static void bedLoadItemByQueryWhere(struct track *tg, char *table, char *query,
                                char *extraWhere, ItemLoader loader)
/* Generic itg->item loader, adding extra clause to hgRangeQuery if query is NULL
 * and extraWhere is not NULL */
{
struct sqlConnection *conn = hAllocConn(database);
int rowOffset = 0;
struct sqlResult *sr = NULL;
char **row = NULL;
struct slList *itemList = NULL, *item = NULL;

if (query == NULL)
    sr = hRangeQuery(conn, table, chromName,
		     winStart, winEnd, extraWhere, &rowOffset);
else
    sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    item = loader(row + rowOffset);
    slAddHead(&itemList, item);
    }
slSort(&itemList, bedCmp);
sqlFreeResult(&sr);
tg->items = itemList;
hFreeConn(&conn);
}

void bedLoadItemWhere(struct track *tg, char *table, char *extraWhere, ItemLoader loader)
/* Generic tg->item loader, adding extra clause to hgRangeQuery */
{
bedLoadItemByQueryWhere(tg, table, NULL, extraWhere, loader);
}

void bedLoadItemByQuery(struct track *tg, char *table, char *query, ItemLoader loader)
/* Generic tg->item loader. If query is NULL use generic hRangeQuery(). */
{
bedLoadItemByQueryWhere(tg, table, query, NULL, loader);
}

void bedLoadItem(struct track *tg, char *table, ItemLoader loader)
/* Generic tg->item loader. */
{
bedLoadItemByQuery(tg, table, NULL, loader);
}

void atomDrawSimpleAt(struct track *tg, void *item,
                      struct hvGfx *hvg, int xOff, int y,
                      double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single simple bed item at position. */
{
struct bed *bed = item;
int heightPer = tg->heightPer;
int x1 = round((double)((int)bed->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)bed->chromEnd-winStart)*scale) + xOff;
int w;
struct trackDb *tdb = tg->tdb;
int scoreMin = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMax", "1000"));
char *directUrl = trackDbSetting(tdb, "directUrl");
boolean withHgsid = (trackDbSetting(tdb, "hgsid") != NULL);
//boolean thickDrawItem = (trackDbSetting(tdb, "thickDrawItem") != NULL);
int colors[8] =
{
orangeColor,
greenColor,
blueColor,
brickColor,
darkBlueColor,
darkGreenColor,
1,
MG_RED} ;

if (tg->itemColor != NULL)
    color = tg->itemColor(tg, bed, hvg);
else
    {
    if (tg->colorShades)
	color = tg->colorShades[grayInRange(bed->score, scoreMin, scoreMax)];
    }

w = x2-x1;
if (w < 1)
    w = 1;
//if (color)
    {
    int ii;

    for(ii=0; ii < 8; ii++)
	{
	if (bed->score & (1 << ii))
	    hvGfxBox(hvg, x1, y+(ii*6), w, 6, colors[ii]);
	}
    if (tg->drawName && vis != tvSquish)
	{
	/* Clip here so that text will tend to be more visible... */
	char *s = tg->itemName(tg, bed);
	w = x2-x1;
	if (w > mgFontStringWidth(font, s))
	    {
	    Color textColor = hvGfxContrastingColor(hvg, color);
	    hvGfxTextCentered(hvg, x1, y, w, heightPer, textColor, font, s);
	    }
	mapBoxHgcOrHgGene(hvg, bed->chromStart, bed->chromEnd, x1, y, x2 - x1, heightPer,
                          tg->track, tg->mapItemName(tg, bed), NULL, directUrl, withHgsid, NULL);
	}
    }
}
#endif /* GBROWSE */

#ifndef GBROWSE
static void logoDrawSimple(struct track *tg, int seqStart, int seqEnd,
                           struct hvGfx *hvg, int xOff, int yOff, int width,
                           MgFont *font, Color color, enum trackVisibility vis)
/* Draw sequence logo */
{
struct dnaMotif *motif;
int count = seqEnd - seqStart;
int ii;
FILE *f;
struct tempName pngTn;
unsigned char *buf;;

if (tg->items == NULL)
	return;

motif = tg->items;
buf = needMem(width + 1);;
dnaMotifNormalize(motif);
makeTempName(&pngTn, "logo", ".pgm");
dnaMotifToLogoPGM(motif, width / count, width  , 50, NULL, "../trash", pngTn.forCgi);

f = mustOpen(pngTn.forCgi, "r");

/* get rid of header */
for(ii=0; ii < 4; ii++)
    while(fgetc(f) != '\n')
	;

hvGfxSetClip(hvg, xOff, yOff, width*2 , 52);

/* map colors from PGM to browser colors */
Color *colors = needMem(sizeof(Color) * width);
for(ii=0; ii < 52; ii++)
    {
    int jj;
    mustRead(f, buf, width);

    for(jj=0; jj < width + 2; jj++)
	{
	if (buf[jj] == 255) colors[jj] = MG_WHITE;
	else if (buf[jj] == 0x87) colors[jj] = MG_RED;
	else if (buf[jj] == 0x60) colors[jj] = greenColor;
	else if (buf[jj] == 0x7f) colors[jj] = blueColor;
	else if (buf[jj] == 0x62) colors[jj] = orangeColor;
	}

    hvGfxVerticalSmear(hvg,xOff,yOff+ii,width ,1, colors,TRUE);
    }
hvGfxUnclip(hvg);

fclose(f);
remove(pngTn.forCgi);
}

boolean tfbsConsSitesWeightFilterItem(struct track *tg, void *item,
	float cutoff)
/* Return TRUE if item passes filter. */
{
struct tfbsConsSites *el = item;

if (el->zScore < cutoff)
    return FALSE;
return TRUE;
}

void loadTfbsConsSites(struct track *tg)
/* Load conserved binding site track, all items that meet the cutoff. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
struct tfbsConsSites *ro, *list = NULL;
float tfbsConsSitesCutoff; /* Cutoff used for conserved binding site track. */

tfbsConsSitesCutoff =
    sqlFloat(cartUsualString(cart,TFBS_SITES_CUTOFF,TFBS_SITES_CUTOFF_DEFAULT));

sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ro = tfbsConsSitesLoad(row+rowOffset);
    if (tfbsConsSitesWeightFilterItem(tg,ro,tfbsConsSitesCutoff))
        slAddHead(&list, ro);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
tg->items = list;
}

void loadTfbsCons(struct track *tg)
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
char *lastName = NULL;
struct tfbsCons *tfbs, *list = NULL;

sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    tfbs = tfbsConsLoad(row+rowOffset);
    if ((lastName == NULL) || !sameString(lastName, tfbs->name))
	{
	slAddHead(&list, tfbs);
	freeMem(lastName);
	lastName = cloneString(tfbs->name);
	}
    }
freeMem(lastName);
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);
tg->items = list;
}

void tfbsConsSitesMethods(struct track *tg)
{
bedMethods(tg);
tg->loadItems = loadTfbsConsSites;
}

void tfbsConsMethods(struct track *tg)
{
bedMethods(tg);
tg->loadItems = loadTfbsCons;
}

void isochoreLoad(struct track *tg)
/* Load up isochores from database table to track items. */
{
bedLoadItem(tg, "isochores", (ItemLoader)isochoresLoad);
}

void isochoreFree(struct track *tg)
/* Free up isochore items. */
{
isochoresFreeList((struct isochores**)&tg->items);
}

char *isochoreName(struct track *tg, void *item)
/* Return name of gold track item. */
{
struct isochores *iso = item;
static char buf[SMALLBUF];
safef(buf, sizeof buf, "%3.1f%% GC", 0.1*iso->gcPpt);
return buf;
}

static void isochoreDraw(struct track *tg, int seqStart, int seqEnd,
                         struct hvGfx *hvg, int xOff, int yOff, int width,
                         MgFont *font, Color color, enum trackVisibility vis)
/* Draw isochore items. */
{
struct isochores *item;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1,x2,w;
boolean isFull = (vis == tvFull);
double scale = scaleForPixels(width);

for (item = tg->items; item != NULL; item = item->next)
    {
    x1 = round((double)((int)item->chromStart-winStart)*scale) + xOff;
    x2 = round((double)((int)item->chromEnd-winStart)*scale) + xOff;
    w = x2-x1;
    color = shadesOfGray[grayInRange(item->gcPpt, 340, 617)];
    if (w < 1)
	w = 1;
    hvGfxBox(hvg, x1, y, w, heightPer, color);
    mapBoxHc(hvg, item->chromStart, item->chromEnd, x1, y, w, heightPer, tg->track,
	item->name, item->name);
    if (isFull)
	y += lineHeight;
    }
}

void isochoresMethods(struct track *tg)
/* Make track for isochores. */
{
tg->loadItems = isochoreLoad;
tg->freeItems = isochoreFree;
tg->drawItems = isochoreDraw;
tg->colorShades = shadesOfGray;
tg->itemName = isochoreName;
}

/* Ewan's stuff */
/******************************************************************/
					/*Royden fun test code*/
/******************************************************************/

void loadCeleraDupPositive(struct track *tg)
/* Load up simpleRepeats from database table to track items. */
{
bedLoadItem(tg, "celeraDupPositive", (ItemLoader)celeraDupPositiveLoad);
if (tg->visibility == tvDense && slCount(tg->items) <= maxItemsInFullTrack)
    slSort(&tg->items, bedCmpScore);
else
    slSort(&tg->items, bedCmp);
}

void freeCeleraDupPositive(struct track *tg)
/* Free up isochore items. */
{
celeraDupPositiveFreeList((struct celeraDupPositive**)&tg->items);
}

Color celeraDupPositiveColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return name of gcPercent track item. */
{
/*struct celeraDupPositive *dup = item;*/
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

char *celeraDupPositiveName(struct track *tg, void *item)
/* Return full genie name. */
{
struct celeraDupPositive *gd = item;
char *full = gd->name;
static char abbrev[SMALLBUF];

strcpy(abbrev, skipChr(full));
abbr(abbrev, "om");
return abbrev;
}


void celeraDupPositiveMethods(struct track *tg)
/* Make track for simple repeats. */
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


void loadCeleraCoverage(struct track *tg)
/* Load up simpleRepeats from database table to track items. */
{
bedLoadItem(tg, "celeraCoverage", (ItemLoader)celeraCoverageLoad);
if (tg->visibility == tvDense && slCount(tg->items) <= maxItemsInFullTrack)
    slSort(&tg->items, bedCmpScore);
else
    slSort(&tg->items, bedCmp);
}

void freeCeleraCoverage(struct track *tg)
/* Free up isochore items. */
{
celeraCoverageFreeList((struct celeraCoverage**)&tg->items);
}

Color celeraCoverageColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return name of gcPercent track item. */
{
/*struct celeraDupPositive *dup = item; */
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

char *celeraCoverageName(struct track *tg, void *item)
/* Return full genie name. */
{
struct celeraCoverage *gd = item;
char *full = gd->name;
static char abbrev[SMALLBUF];

strcpy(abbrev, skipChr(full));
abbr(abbrev, "om");
return abbrev;
}


void celeraCoverageMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->loadItems = loadCeleraCoverage;
tg->freeItems = freeCeleraCoverage;
tg->itemName = celeraCoverageName;
tg->itemColor = celeraCoverageColor;
}
/******************************************************************/
		/*end of Royden test Code celeraCoverage */
/******************************************************************/

void loadGenomicSuperDups(struct track *tg)
/* Load up simpleRepeats from database table to track items. */
{
bedLoadItem(tg, "genomicSuperDups", (ItemLoader)genomicSuperDupsLoad);
if (tg->visibility == tvDense && slCount(tg->items) <= maxItemsInFullTrack)
    slSort(&tg->items, bedCmpScore);
else
    slSort(&tg->items, bedCmp);
}

void freeGenomicSuperDups(struct track *tg)
/* Free up isochore items. */
{
genomicSuperDupsFreeList((struct genomicSuperDups**)&tg->items);
}

Color genomicSuperDupsColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of duplication - orange for > .990, yellow for > .980,
 * red for verdict == "BAD". */
{
struct genomicSuperDups *dup = (struct genomicSuperDups *)item;
static bool gotColor = FALSE;
static Color red, orange, yellow;
int grayLevel;

if (!gotColor)
    {
    red      = hvGfxFindColorIx(hvg, 255,  51, 51);
    orange   = hvGfxFindColorIx(hvg, 230, 130,  0);
    yellow   = hvGfxFindColorIx(hvg, 210, 200,  0);
    gotColor = TRUE;
    }
if (sameString(dup->verdict, "BAD"))
    return red;
if (dup->fracMatch > 0.990)
    return orange;
else if (dup->fracMatch > 0.980)
    return yellow;
grayLevel = grayInRange((int)(dup->fracMatch * 1000), 900, 1000);
return shadesOfGray[grayLevel];
}

char *genomicSuperDupsName(struct track *tg, void *item)
/* Return full genie name. */
{
struct genomicSuperDups *gd = item;
char *full = gd->name;
static char abbrev[SMALLBUF];
char *s = strchr(full, '.');
if (s != NULL)
    full = s+1;
safef(abbrev, sizeof(abbrev), "%s", full);
abbr(abbrev, "chrom");
return abbrev;
}


void genomicSuperDupsMethods(struct track *tg)
/* Make track for simple repeats. */
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

/* Make track for Genomic Dups. */

void loadGenomicDups(struct track *tg)
/* Load up genomicDups from database table to track items. */
{
bedLoadItem(tg, "genomicDups", (ItemLoader)genomicDupsLoad);
if (limitVisibility(tg) == tvFull)
    slSort(&tg->items, bedCmpScore);
else
    slSort(&tg->items, bedCmp);
}

void freeGenomicDups(struct track *tg)
/* Free up isochore items. */
{
genomicDupsFreeList((struct genomicDups**)&tg->items);
}

Color dupPptColor(int ppt, struct hvGfx *hvg)
/* Return color of duplication - orange for > 990, yellow for > 980 */
{
static bool gotColor = FALSE;
static Color orange, yellow;
int grayLevel;
if (!gotColor)
    {
    orange = hvGfxFindColorIx(hvg, 230, 130, 0);
    yellow = hvGfxFindColorIx(hvg, 210, 200, 0);
    gotColor = TRUE;
    }
if (ppt > 990)
    return orange;
else if (ppt > 980)
    return yellow;
grayLevel = grayInRange(ppt, 900, 1000);
return shadesOfGray[grayLevel];
}

Color genomicDupsColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return name of gcPercent track item. */
{
struct genomicDups *dup = item;
return dupPptColor(dup->score, hvg);
}

char *genomicDupsName(struct track *tg, void *item)
/* Return full genie name. */
{
struct genomicDups *gd = item;
char *full = gd->name;
static char abbrev[SMALLBUF];

strcpy(abbrev, skipChr(full));
abbr(abbrev, "om");
return abbrev;
}


void genomicDupsMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->loadItems = loadGenomicDups;
tg->freeItems = freeGenomicDups;
tg->itemName = genomicDupsName;
tg->itemColor = genomicDupsColor;
}

Color jkDupliconColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of duplicon track item. */
{
struct bed *el = item;
int grayLevel = grayInRange(el->score, 600, 1000);
return shadesOfGray[grayLevel];
}

void jkDupliconMethods(struct track *tg)
/* Load up custom methods for duplicon. */
{
tg->itemColor = jkDupliconColor;
}


static void loadSimpleNucDiff(struct track *tg)
/* Load up simple diffs from database table to track items. */
{
bedLoadItem(tg, tg->table, (ItemLoader)simpleNucDiffLoad);
}

static char *simpleNucDiffName(struct track *tg, void *item)
/* Return name of simpleDiff item. */
{
static char buf[32];
struct simpleNucDiff *snd = item;
safef(buf, sizeof(buf), "%s<->%s", snd->tSeq, snd->qSeq);
return buf;
}

static void chimpSimpleDiffMethods(struct track *tg)
/* Load up custom methods for simple diff */
{
tg->loadItems = loadSimpleNucDiff;
tg->itemName = simpleNucDiffName;
}

char *simpleRepeatName(struct track *tg, void *item)
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

void loadSimpleRepeats(struct track *tg)
/* Load up simpleRepeats from database table to track items. */
{
bedLoadItem(tg, tg->table, (ItemLoader)simpleRepeatLoad);
}

void freeSimpleRepeats(struct track *tg)
/* Free up isochore items. */
{
simpleRepeatFreeList((struct simpleRepeat**)&tg->items);
}

void simpleRepeatMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->loadItems = loadSimpleRepeats;
tg->freeItems = freeSimpleRepeats;
tg->itemName = simpleRepeatName;
}

Color cpgIslandColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of cpgIsland track item. */
{
struct cpgIsland *el = item;
return (el->length < 300 ? tg->ixAltColor : tg->ixColor);
}

void loadCpgIsland(struct track *tg)
/* Load up simpleRepeats from database table to track items. */
{
bedLoadItem(tg, tg->table, (ItemLoader)cpgIslandLoad);
}

void freeCpgIsland(struct track *tg)
/* Free up isochore items. */
{
cpgIslandFreeList((struct cpgIsland**)&tg->items);
}

void cpgIslandMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->loadItems = loadCpgIsland;
tg->freeItems = freeCpgIsland;
tg->itemColor = cpgIslandColor;
}

char *rgdGeneItemName(struct track *tg, void *item)
/* Return name of RGD gene track item. */
{
static char name[32];
struct sqlConnection *conn = hAllocConn(database);
struct dyString *ds = dyStringNew(256);
struct linkedFeatures *lf = item;

sqlDyStringPrintf(ds, "select name from rgdGeneLink where refSeq = '%s'", lf->name);
sqlQuickQuery(conn, ds->string, name, sizeof(name));
dyStringFree(&ds);
hFreeConn(&conn);
return name;
}

void rgdGeneMethods(struct track *tg)
/* Make track for RGD genes. */
{
tg->itemName = rgdGeneItemName;
}

void loadGcPercent(struct track *tg)
/* Load up simpleRepeats from database table to track items. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
struct gcPercent *itemList = NULL, *item;
char query[256];

sqlSafef(query, sizeof query, "select * from %s where chrom = '%s' and chromStart<%u and chromEnd>%u", tg->table,
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

void freeGcPercent(struct track *tg)
/* Free up isochore items. */
{
gcPercentFreeList((struct gcPercent**)&tg->items);
}


char *gcPercentName(struct track *tg, void *item)
/* Return name of gcPercent track item. */
{
struct gcPercent *gc = item;
static char buf[32];

safef(buf, sizeof buf, "%3.1f%% GC", 0.1*gc->gcPpt);
return buf;
}

static int gcPercentMin = 320;
static int gcPercentMax = 600;

Color gcPercentColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return name of gcPercent track item. */
{
struct gcPercent *gc = item;
int ppt = gc->gcPpt;
int grayLevel;

grayLevel = grayInRange(ppt, gcPercentMin, gcPercentMax);
return shadesOfGray[grayLevel];
}

void gcPercentMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->loadItems = loadGcPercent;
tg->freeItems = freeGcPercent;
tg->itemName = gcPercentName;
tg->colorShades = shadesOfGray;
tg->itemColor = gcPercentColor;
}

char *recombRateMap;
enum recombRateOptEnum recombRateType;

boolean recombRateSetRate(struct track *tg, void *item)
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

void loadRecombRate(struct track *tg)
/* Load up recombRate from database table to track items. */
{
recombRateMap = cartUsualString(cart, "recombRate.type", rroeEnumToString(0));
recombRateType = rroeStringToEnum(recombRateMap);
bedLoadItem(tg, "recombRate", (ItemLoader)recombRateLoad);
filterItems(tg, recombRateSetRate, "include");
}

void freeRecombRate(struct track *tg)
/* Free up recombRate items. */
{
recombRateFreeList((struct recombRate**)&tg->items);
}

char *recombRateName(struct track *tg, void *item)
/* Return name of recombRate track item. */
{
struct recombRate *rr = item;
static char buf[32];

switch (recombRateType)
    {
    case rroeDecodeAvg: case rroeMarshfieldAvg: case rroeGenethonAvg:
	safef(buf, sizeof buf, "%3.1f cM/Mb (Avg)", rr->decodeAvg);
        break;
    case rroeDecodeFemale: case rroeMarshfieldFemale: case rroeGenethonFemale:
	safef(buf, sizeof buf, "%3.1f cM/Mb (F)", rr->decodeAvg);
        break;
    case rroeDecodeMale: case rroeMarshfieldMale: case rroeGenethonMale:
	safef(buf, sizeof buf, "%3.1f cM/Mb (M)", rr->decodeAvg);
        break;
    default:
	safef(buf, sizeof buf, "%3.1f cM/Mb (Avg)", rr->decodeAvg);
        break;
    }
return buf;
}

static int recombRateMin = 320;
static int recombRateMax = 600;

Color recombRateColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color for item in recombRate track item. */
{
struct recombRate *rr = item;
int rcr;
int grayLevel;

rcr = (int)(rr->decodeAvg * 200);
grayLevel = grayInRange(rcr, recombRateMin, recombRateMax);
return shadesOfGray[grayLevel];
}

void recombRateMethods(struct track *tg)
/* Make track for recombination rates. */
{
tg->loadItems = loadRecombRate;
tg->freeItems = freeRecombRate;
tg->itemName = recombRateName;
tg->colorShades = shadesOfGray;
tg->itemColor = recombRateColor;
}

char *recombRateRatMap;
enum recombRateRatOptEnum recombRateRatType;

boolean recombRateRatSetRate(struct track *tg, void *item)
/* Change the recombRateRat value to the one chosen */
{
struct recombRateRat *el = item;
switch (recombRateRatType)
    {
    case rrroeShrspAvg:
	return TRUE;
        break;
    case rrroeFhhAvg:
	el->shrspAvg = el->fhhAvg;
	return TRUE;
        break;
    default:
	return FALSE;
        break;
    }
}

void loadRecombRateRat(struct track *tg)
/* Load up recombRateRat from database table to track items. */
{
recombRateRatMap = cartUsualString(cart, "recombRateRat.type", rrroeEnumToString(0));
recombRateRatType = rrroeStringToEnum(recombRateRatMap);
bedLoadItem(tg, "recombRateRat", (ItemLoader)recombRateRatLoad);
filterItems(tg, recombRateRatSetRate, "include");
}

void freeRecombRateRat(struct track *tg)
/* Free up recombRateRat items. */
{
recombRateRatFreeList((struct recombRateRat**)&tg->items);
}

char *recombRateRatName(struct track *tg, void *item)
/* Return name of recombRateRat track item. */
{
struct recombRateRat *rr = item;
static char buf[32];

switch (recombRateRatType)
    {
    case rrroeShrspAvg: case rrroeFhhAvg:
	safef(buf, sizeof buf, "%3.1f cM/Mb (Avg)", rr->shrspAvg);
        break;
    default:
	safef(buf, sizeof buf, "%3.1f cM/Mb (Avg)", rr->shrspAvg);
        break;
    }
return buf;
}

Color recombRateRatColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color for item in recombRateRat track item. */
{
struct recombRateRat *rr = item;
int rcr;
int grayLevel;

rcr = (int)(rr->shrspAvg * 200);
grayLevel = grayInRange(rcr, recombRateMin, recombRateMax);
return shadesOfGray[grayLevel];
}

void recombRateRatMethods(struct track *tg)
/* Make track for rat recombination rates. */
{
tg->loadItems = loadRecombRateRat;
tg->freeItems = freeRecombRateRat;
tg->itemName = recombRateRatName;
tg->colorShades = shadesOfGray;
tg->itemColor = recombRateRatColor;
}


char *recombRateMouseMap;
enum recombRateMouseOptEnum recombRateMouseType;

boolean recombRateMouseSetRate(struct track *tg, void *item)
/* Change the recombRateMouse value to the one chosen */
{
struct recombRateMouse *el = item;
switch (recombRateMouseType)
    {
    case rrmoeWiAvg:
	return TRUE;
        break;
    case rrmoeMgdAvg:
	el->wiAvg = el->mgdAvg;
	return TRUE;
        break;
    default:
	return FALSE;
        break;
    }
}

void loadRecombRateMouse(struct track *tg)
/* Load up recombRateMouse from database table to track items. */
{
recombRateMouseMap = cartUsualString(cart, "recombRateMouse.type", rrmoeEnumToString(0));
recombRateMouseType = rrmoeStringToEnum(recombRateMouseMap);
bedLoadItem(tg, "recombRateMouse", (ItemLoader)recombRateMouseLoad);
filterItems(tg, recombRateMouseSetRate, "include");
}

void freeRecombRateMouse(struct track *tg)
/* Free up recombRateMouse items. */
{
recombRateMouseFreeList((struct recombRateMouse**)&tg->items);
}

char *recombRateMouseName(struct track *tg, void *item)
/* Return name of recombRateMouse track item. */
{
struct recombRateMouse *rr = item;
static char buf[32];

switch (recombRateMouseType)
    {
    case rrmoeWiAvg: case rrmoeMgdAvg:
	safef(buf, sizeof buf, "%3.1f cM/Mb (Avg)", rr->wiAvg);
        break;
    default:
	safef(buf, sizeof buf, "%3.1f cM/Mb (Avg)", rr->wiAvg);
        break;
    }
return buf;
}

Color recombRateMouseColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color for item in recombRateMouse track item. */
{
struct recombRateMouse *rr = item;
int rcr;
int grayLevel;

rcr = (int)(rr->wiAvg * 200);
grayLevel = grayInRange(rcr, recombRateMin, recombRateMax);
return shadesOfGray[grayLevel];
}

void recombRateMouseMethods(struct track *tg)
/* Make track for mouse recombination rates. */
{
tg->loadItems = loadRecombRateMouse;
tg->freeItems = freeRecombRateMouse;
tg->itemName = recombRateMouseName;
tg->colorShades = shadesOfGray;
tg->itemColor = recombRateMouseColor;
}


/* Chromosome 18 deletions track */
void loadChr18deletions(struct track *tg)
/* Load up chr18deletions from database table to track items. */
{
bedLoadItem(tg, "chr18deletions", (ItemLoader)chr18deletionsLoad);
}

void freeChr18deletions(struct track *tg)
/* Free up chr18deletions items. */
{
chr18deletionsFreeList((struct chr18deletions**)&tg->items);
}

static void drawChr18deletions(struct track *tg, int seqStart, int seqEnd,
                               struct hvGfx *hvg, int xOff, int yOff, int width,
                               MgFont *font, Color color, enum trackVisibility vis)
/* Draw chr18deletions items. */
{
struct chr18deletions *cds;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int shortOff = heightPer/4;
int shortHeight = heightPer - 2*shortOff;
int tallStart, tallEnd, shortStart, shortEnd;
boolean isFull = (vis == tvFull);
double scale = scaleForPixels(width);

if (vis == tvDense)
    {
    slSort(&tg->items, cmpLfsWhiteToBlack);
    }

for (cds = tg->items; cds != NULL; cds = cds->next)
    {
    int end, start, blocks;

    for (blocks = 0; blocks < cds->ssCount; blocks++)
        {
        tallStart = cds->largeStarts[blocks];
	tallEnd = cds->largeEnds[blocks];
	shortStart = cds->smallStarts[blocks];
	shortEnd = cds->smallEnds[blocks];

	if (shortStart < tallStart)
	    {
	    end = tallStart;
	    drawScaledBox(hvg, shortStart, end, scale, xOff, y+shortOff, shortHeight, color);
	    }
	if (shortEnd > tallEnd)
	    {
	    start = tallEnd;
	    drawScaledBox(hvg, start, shortEnd, scale, xOff, y+shortOff, shortHeight, color);
	    }
	if (tallEnd > tallStart)
	    {
	    drawScaledBox(hvg, tallStart, tallEnd, scale, xOff, y, heightPer, color);
	    }
	}
    if (isFull) y += lineHeight;
    }
}

void chr18deletionsMethods(struct track *tg)
/* Make track for recombination rates. */
{
tg->loadItems = loadChr18deletions;
tg->freeItems = freeChr18deletions;
tg->drawItems = drawChr18deletions;
}

void loadGenethon(struct track *tg)
/* Load up simpleRepeats from database table to track items. */
{
bedLoadItem(tg, "mapGenethon", (ItemLoader)mapStsLoad);
}

void freeGenethon(struct track *tg)
/* Free up isochore items. */
{
mapStsFreeList((struct mapSts**)&tg->items);
}

void genethonMethods(struct track *tg)
/* Make track for simple repeats. */
{
tg->loadItems = loadGenethon;
tg->freeItems = freeGenethon;
}

Color exoFishColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of exofish track item. */
{
struct exoFish *el = item;
int ppt = el->score;
int grayLevel;

grayLevel = grayInRange(ppt, -500, 1000);
return shadesOfSea[grayLevel];
}

void exoFishMethods(struct track *tg)
/* Make track for exoFish. */
{
tg->itemColor = exoFishColor;
}

void loadExoMouse(struct track *tg)
/* Load up exoMouse from database table to track items. */
{
bedLoadItem(tg, "exoMouse", (ItemLoader)roughAliLoad);
if (tg->visibility == tvDense && slCount(tg->items) < 1000)
    {
    slSort(&tg->items, bedCmpScore);
    }
}

void freeExoMouse(struct track *tg)
/* Free up isochore items. */
{
roughAliFreeList((struct roughAli**)&tg->items);
}

char *exoMouseName(struct track *tg, void *item)
/* Return what to display on left column of open track. */
{
struct roughAli *exo = item;
static char name[17];

strncpy(name, exo->name, sizeof(name)-1);
return name;
}


Color exoMouseColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of exoMouse track item. */
{
struct roughAli *el = item;
int ppt = el->score;
int grayLevel;

grayLevel = grayInRange(ppt, -100, 1000);
return shadesOfBrown[grayLevel];
}


void exoMouseMethods(struct track *tg)
/* Make track for exoMouse. */
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

char *xenoMrnaName(struct track *tg, void *item)
/* Return what to display on left column of open track:
 * In this case display 6 letters of organism name followed
 * by mRNA accession. */
{
struct linkedFeatures *lf = item;
char *name = lf->name;
struct sqlConnection *conn = hAllocConn(database);
char *org = getOrganism(conn, name);
hFreeConn(&conn);
if (org == NULL)
    return name;
else
    {
    static char compName[SMALLBUF];
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

void xenoMrnaMethods(struct track *tg)
/* Fill in custom parts of xeno mrna alignments. */
{
tg->itemName = xenoMrnaName;
tg->extraUiData = newMrnaUiData(tg->track, TRUE);
tg->totalHeight = tgFixedTotalHeightUsingOverflow;
}

void xenoRefGeneMethods(struct track *tg)
/* Make track of known genes from xenoRefSeq. */
{
tg->loadItems = loadRefGene;
tg->itemName = refGeneName;
tg->mapItemName = refGeneMapName;
tg->itemColor = refGeneColor;
}

void mrnaMethods(struct track *tg)
/* Make track of mRNA methods. */
{
tg->extraUiData = newMrnaUiData(tg->track, FALSE);
}

char *interProName(struct track *tg, void *item)
{
char condStr[255];
char *desc;
struct bed *it = item;
struct sqlConnection *conn;

conn = hAllocConn(database);
sqlSafef(condStr, sizeof condStr, "interProId='%s' limit 1", it->name);
desc = sqlGetField("proteome", "interProXref", "description", condStr);
hFreeConn(&conn);
return(desc);
}

void interProMethods(struct track *tg)
/* Make track of InterPro methods. */
{
tg->itemName    = interProName;
}

void estMethods(struct track *tg)
/* Make track of EST methods - overrides color handler. */
{
tg->drawItems = linkedFeaturesAverageDenseOrientEst;
tg->extraUiData = newMrnaUiData(tg->track, FALSE);
tg->totalHeight = tgFixedTotalHeightUsingOverflow;
}
#endif /* GBROWSE */

boolean isNonChromColor(Color color)
/* test if color is a non-chrom color (black or gray) */
{
return color == chromColor[0];
}

Color getChromColor(char *name, struct hvGfx *hvg)
/* Return color index corresponding to chromosome name (assumes that name
 * points to the first char after the "chr" prefix). */
{
int chromNum = 0;
Color colorNum = 0;
if (!chromosomeColorsMade)
    makeChromosomeShades(hvg);
if (atoi(name) != 0)
    {
    chromNum =  atoi(name);
    /* Tweaks for chimp and other apes with chrom names corresponding to fused human chr2
     * giving them back a distinct color to distinguish chr2B from chr2A.
     * panTro2 uses chr2a chr2b. panTro3 uses chr2A chr2B. */
    if (startsWith("2B", name) || startsWith("2b", name))
	chromNum = 26;
    }
else if (startsWith("U", name))
    chromNum = 26;
else if (startsWith("Y", name))
    chromNum = 24;
else if (startsWith("M", name))
    chromNum = 25;
else if (startsWith("XXI", name))
    chromNum = 21;
else if (startsWith("XX", name))
    chromNum = 20;
else if (startsWith("XIX", name))
    chromNum = 19;
else if (startsWith("XVIII", name))
    chromNum = 18;
else if (startsWith("XVII", name))
    chromNum = 17;
else if (startsWith("XVI", name))
    chromNum = 16;
else if (startsWith("XV", name))
    chromNum = 15;
else if (startsWith("XIV", name))
    chromNum = 14;
else if (startsWith("XIII", name))
    chromNum = 13;
else if (startsWith("XII", name))
    chromNum = 12;
else if (startsWith("XI", name))
    chromNum = 11;
else if (startsWith("X", name))
    /*   stickleback should be chr10   */
    chromNum = 23;
else if (startsWith("IX", name))
    chromNum = 9;
else if (startsWith("VIII", name))
    chromNum = 8;
else if (startsWith("VII", name))
    chromNum = 7;
else if (startsWith("VI", name))
    chromNum = 6;
else if (startsWith("V", name))
    chromNum = 5;
else if (startsWith("IV", name))
    chromNum = 4;
else if (startsWith("III", name))
    chromNum = 3;
else if (startsWith("II", name))
    chromNum = 2;
else if (startsWith("I", name))
    chromNum = 1;
if (chromNum > CHROM_COLORS) chromNum = 0;
colorNum = chromColor[chromNum];
return colorNum;
}

char *chromPrefixes[] = { "chr", "Group",
                          NULL };

char *scaffoldPrefixes[] = { "scaffold_", "contig_", "SCAFFOLD", "Scaffold",
    "Contig", "SuperCont", "super_", "scaffold", "Zv7_", "Scfld02_",
         NULL };

char *maybeSkipPrefix(char *name, char *prefixes[])
/* Return a pointer into name just past the first matching string from
 * prefixes[], if found.  If none are found, return NULL. */
{
char *skipped = NULL;
int i = 0;
for (i = 0;  prefixes[i] != NULL;  i++)
    {
    skipped = stringIn(prefixes[i], name);
    if (skipped != NULL)
	{
	skipped += strlen(prefixes[i]);
	break;
	}
    }
/* perhaps a contig name of some other prefix */
if (NULL == skipped && scaffoldPrefixes == prefixes)
    {
    skipped = cloneString(name); /* will be memory leak */
    chopSuffixAt(skipped, 'v');  /* remove the vNN version, usually v1 */
    chopSuffixAt(skipped, '.');  /* remove the vNN version, could be .1 */
    chopSuffixAt(skipped, ' ');  /* chain names have blank+N */
    eraseNonDigits(skipped);  /* strip characters, leave digits only */
    if (0 == strlen(skipped))  /* if none left, did not work */
       skipped = NULL;
    }
return skipped;
}

Color getScaffoldColor(char *scaffoldNum, struct hvGfx *hvg)
/* assign fake chrom color to scaffold/contig, based on number */
{
int scafNum = atoi(scaffoldNum) % SCAF_COLORS + 1;
if (!scafColorsMade)
    makeScaffoldShades(hvg);
if (scafNum < 0 || scafNum > SCAF_COLORS)
    scafNum = 0;
return scafColor[scafNum];
}

Color getSeqColorDefault(char *seqName, struct hvGfx *hvg, Color defaultColor)
/* Return color of chromosome/scaffold/contig/numeric string, or
 * defaultColor if seqName doesn't look like any of those. */
{
char *skipped = maybeSkipPrefix(seqName, chromPrefixes);
if (skipped != NULL)
    return getChromColor(skipped, hvg);
skipped = maybeSkipPrefix(seqName, scaffoldPrefixes);
if (skipped != NULL)
    return getScaffoldColor(skipped, hvg);
if (isdigit(seqName[0]))
    return getScaffoldColor(seqName, hvg);
return defaultColor;
}

Color getSeqColor(char *seqName, struct hvGfx *hvg)
/* Return color of chromosome/scaffold/contig/numeric string. */
{
return getSeqColorDefault(seqName, hvg, chromColor[0]);
}

Color lfChromColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of chromosome for linked feature type items
 * where the chromosome is listed somewhere in the lf->name. */
{
struct linkedFeatures *lf = item;
return getSeqColorDefault(lf->name, hvg, tg->ixColor);
}

#ifndef GBROWSE
void loadRnaGene(struct track *tg)
/* Load up rnaGene from database table to track items. */
{
bedLoadItem(tg, "rnaGene", (ItemLoader)rnaGeneLoad);
}

void freeRnaGene(struct track *tg)
/* Free up rnaGene items. */
{
rnaGeneFreeList((struct rnaGene**)&tg->items);
}

Color rnaGeneColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of rnaGene track item. */
{
struct rnaGene *el = item;

return (el->isPsuedo ? tg->ixAltColor : tg->ixColor);
}

char *rnaGeneName(struct track *tg, void *item)
/* Return RNA gene name. */
{
struct rnaGene *el = item;
char *full = el->name;
static char abbrev[SMALLBUF];
char *e;

strcpy(abbrev, skipChr(full));
subChar(abbrev, '_', ' ');
abbr(abbrev, " pseudogene");
if ((e = strstr(abbrev, "-related")) != NULL)
    strcpy(e, "-like");
return abbrev;
}

void rnaGeneMethods(struct track *tg)
/* Make track for rna genes . */
{
tg->loadItems = loadRnaGene;
tg->freeItems = freeRnaGene;
tg->itemName = rnaGeneName;
tg->itemColor = rnaGeneColor;
}


Color ncRnaColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of ncRna track item. */
{
char condStr[255];
char *rnaType;
Color color = {MG_GRAY};  /* Set default to gray */
struct rgbColor hAcaColor = {0, 128, 0, 255}; /* darker green, per request by Weber */
Color hColor;
struct sqlConnection *conn;
char *name;

conn = hAllocConn(database);
hColor = hvGfxFindColorIx(hvg, hAcaColor.r, hAcaColor.g, hAcaColor.b);

name = tg->itemName(tg, item);
sqlSafef(condStr, sizeof condStr, "name='%s'", name);
rnaType = sqlGetField(database, "ncRna", "type", condStr);

if (sameWord(rnaType, "miRNA"))    color = MG_RED;
if (sameWord(rnaType, "misc_RNA")) color = MG_BLACK;
if (sameWord(rnaType, "snRNA"))    color = MG_BLUE;
if (sameWord(rnaType, "snoRNA"))   color = MG_MAGENTA;
if (sameWord(rnaType, "rRNA"))     color = MG_CYAN;
if (sameWord(rnaType, "scRNA"))    color = MG_YELLOW;
if (sameWord(rnaType, "Mt_tRNA"))  color = MG_GREEN;
if (sameWord(rnaType, "Mt_rRNA"))  color = hColor;

hFreeConn(&conn);
return(color);
}

void ncRnaMethods(struct track *tg)
/* Make track for ncRna. */
{
tg->itemColor = ncRnaColor;
}

Color wgRnaColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of wgRna track item. */
{
char condStr[255];
char *rnaType;
Color color = {MG_BLACK};  /* Set default to black.  But, if we got black, something is wrong. */
Color hColor;
struct rgbColor hAcaColor = {0, 128, 0, 255}; /* darker green */
struct sqlConnection *conn;
char *name;

conn = hAllocConn(database);
hColor = hvGfxFindColorIx(hvg, hAcaColor.r, hAcaColor.g, hAcaColor.b);

name = tg->itemName(tg, item);
sqlSafef(condStr, sizeof condStr, "name='%s'", name);
rnaType = sqlGetField(database, "wgRna", "type", condStr);
if (sameWord(rnaType, "miRna"))   color = MG_RED;
if (sameWord(rnaType, "HAcaBox")) color = hColor;
if (sameWord(rnaType, "CDBox"))   color = MG_BLUE;
if (sameWord(rnaType, "scaRna"))  color = MG_MAGENTA;

hFreeConn(&conn);
return(color);
}

void wgRnaMethods(struct track *tg)
/* Make track for wgRna. */
{
tg->itemColor = wgRnaColor;
}

Color stsColor(struct hvGfx *hvg, int altColor,
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
        greenIx = hvGfxFindColorIx(hvg, 0, 200, 0);
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

void loadStsMarker(struct track *tg)
/* Load up stsMarkers from database table to track items. */
{
bedLoadItem(tg, "stsMarker", (ItemLoader)stsMarkerLoad);
}

void freeStsMarker(struct track *tg)
/* Free up stsMarker items. */
{
stsMarkerFreeList((struct stsMarker**)&tg->items);
}

Color stsMarkerColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of stsMarker track item. */
{
struct stsMarker *el = item;
return stsColor(hvg, tg->ixAltColor, el->genethonChrom, el->marshfieldChrom,
    el->fishChrom, el->score);
}

void stsMarkerMethods(struct track *tg)
/* Make track for sts markers. */
{
tg->loadItems = loadStsMarker;
tg->freeItems = freeStsMarker;
tg->itemColor = stsMarkerColor;
}

char *stsMapFilter;
char *stsMapMap;
enum stsMapOptEnum stsMapType;
int stsMapFilterColor = MG_BLACK;

boolean stsMapFilterItem(struct track *tg, void *item)
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


void loadStsMap(struct track *tg)
/* Load up stsMarkers from database table to track items. */
{
stsMapFilter = cartUsualString(cart, "stsMap.filter", "blue");
stsMapMap = cartUsualString(cart, "stsMap.type", smoeEnumToString(0));
stsMapType = smoeStringToEnum(stsMapMap);
bedLoadItem(tg, "stsMap", (ItemLoader)stsMapLoad);
filterItems(tg, stsMapFilterItem, stsMapFilter);
stsMapFilterColor = getFilterColor(stsMapFilter, MG_BLACK);
}

void loadStsMap28(struct track *tg)
/* Load up stsMarkers from database table to track items. */
{
stsMapFilter = cartUsualString(cart, "stsMap.filter", "blue");
stsMapMap = cartUsualString(cart, "stsMap.type", smoeEnumToString(0));
stsMapType = smoeStringToEnum(stsMapMap);
bedLoadItem(tg, "stsMap", (ItemLoader)stsMapLoad28);
filterItems(tg, stsMapFilterItem, stsMapFilter);
stsMapFilterColor = getFilterColor(stsMapFilter, MG_BLACK);
}

void freeStsMap(struct track *tg)
/* Free up stsMap items. */
{
stsMapFreeList((struct stsMap**)&tg->items);
}

Color stsMapColor(struct track *tg, void *item, struct hvGfx *hvg)
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


void stsMapMethods(struct track *tg)
/* Make track for sts markers. */
{
struct sqlConnection *conn = hAllocConn(database);
if (sqlCountColumnsInTable(conn, "stsMap") == 26)
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

char *stsMapMouseFilter;
char *stsMapMouseMap;
enum stsMapMouseOptEnum stsMapMouseType;
int stsMapMouseFilterColor = MG_BLACK;

boolean stsMapMouseFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct stsMapMouseNew *el = item;
switch (stsMapMouseType)
    {
    case smmoeGenetic:
       return el->wigChr[0] != '\0' || el->mgiChrom[0] != '\0';
       break;
    case smmoeWig:
	return el->wigChr[0] != '\0';
        break;
    case smmoeMgi:
	return el->mgiChrom[0] != '\0';
        break;
    case smmoeRh:
	return el->rhChrom[0] != '\0';
        break;
    default:
	return FALSE;
        break;
    }
}

void loadStsMapMouse(struct track *tg)
/* Load up stsMarkers from database table to track items. */
{
stsMapMouseFilter = cartUsualString(cart, "stsMapMouse.filter", "blue");
stsMapMouseMap = cartUsualString(cart, "stsMapMouse.type", smmoeEnumToString(0));
stsMapMouseType = smmoeStringToEnum(stsMapMouseMap);
bedLoadItem(tg, "stsMapMouseNew", (ItemLoader)stsMapMouseNewLoad);
filterItems(tg, stsMapMouseFilterItem, stsMapMouseFilter);
stsMapMouseFilterColor = getFilterColor(stsMapMouseFilter, MG_BLACK);

}

void freeStsMapMouse(struct track *tg)
/* Free up stsMap items. */
{
stsMapMouseNewFreeList((struct stsMapMouseNew**)&tg->items);
}

Color stsMapMouseColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of stsMap track item. */
{
struct stsMapMouseNew *el = item;
if (stsMapMouseFilterItem(tg, item))
    {
    if(el->score >= 900)
	return stsMapMouseFilterColor;
    else
	{
	switch(stsMapMouseFilterColor)
	    {
	    case 1:
		return MG_GRAY;
		break;
	    case 2:
		return (hvGfxFindColorIx(hvg, 240, 128, 128)); //Light red
		break;
	    case 3:
                return (hvGfxFindColorIx(hvg, 154, 205, 154)); // light green
		break;
	    case 4:
		return (hvGfxFindColorIx(hvg, 176, 226, 255)); // light blue
	    default:
		return MG_GRAY;
	    }
	}
    }
else
    {
    if(el->score < 900)
	return MG_GRAY;
    else
	return MG_BLACK;
    }
}

void stsMapMouseMethods(struct track *tg)
/* Make track for sts markers. */
{
tg->loadItems = loadStsMapMouse;
tg->freeItems = freeStsMapMouse;
tg->itemColor = stsMapMouseColor;
}

char *stsMapRatFilter;
char *stsMapRatMap;
enum stsMapRatOptEnum stsMapRatType;
int stsMapRatFilterColor = MG_BLACK;

boolean stsMapRatFilterItem(struct track *tg, void *item)
/* Return TRUE if item passes filter. */
{
struct stsMapRat *el = item;
switch (stsMapRatType)
    {
    case smroeGenetic:
       return el->fhhChr[0] != '\0' || el->shrspChrom[0] != '\0';
       break;
    case smroeFhh:
	return el->fhhChr[0] != '\0';
        break;
    case smroeShrsp:
	return el->shrspChrom[0] != '\0';
        break;
    case smroeRh:
	return el->rhChrom[0] != '\0';
        break;
    default:
	return FALSE;
        break;
    }
}


void loadStsMapRat(struct track *tg)
/* Load up stsMarkers from database table to track items. */
{
stsMapRatFilter = cartUsualString(cart, "stsMapRat.filter", "blue");
stsMapRatMap = cartUsualString(cart, "stsMapRat.type", smroeEnumToString(0));
stsMapRatType = smroeStringToEnum(stsMapRatMap);
bedLoadItem(tg, "stsMapRat", (ItemLoader)stsMapRatLoad);
filterItems(tg, stsMapRatFilterItem, stsMapRatFilter);
stsMapRatFilterColor = getFilterColor(stsMapRatFilter, MG_BLACK);
}

void freeStsMapRat(struct track *tg)
/* Free up stsMap items. */
{
stsMapRatFreeList((struct stsMapRat**)&tg->items);
}

Color stsMapRatColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of stsMap track item. */
{
if (stsMapRatFilterItem(tg, item))
    return stsMapRatFilterColor;
else
    {
    struct stsMapRat *el = item;
    if (el->score >= 900)
        return MG_BLACK;
    else
        return MG_GRAY;
    }
}

void stsMapRatMethods(struct track *tg)
/* Make track for sts markers. */
{
tg->loadItems = loadStsMapRat;
tg->freeItems = freeStsMapRat;
tg->itemColor = stsMapRatColor;
}

void loadGenMapDb(struct track *tg)
/* Load up genMapDb from database table to track items. */
{
bedLoadItem(tg, "genMapDb", (ItemLoader)genMapDbLoad);
}

void freeGenMapDb(struct track *tg)
/* Free up genMapDb items. */
{
genMapDbFreeList((struct genMapDb**)&tg->items);
}

void genMapDbMethods(struct track *tg)
/* Make track for GenMapDb Clones */
{
tg->loadItems = loadGenMapDb;
tg->freeItems = freeGenMapDb;
}

char *fishClonesFilter;
char *fishClonesMap;
enum fishClonesOptEnum fishClonesType;
int fishClonesFilterColor = MG_GREEN;

boolean fishClonesFilterItem(struct track *tg, void *item)
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

void loadFishClones(struct track *tg)
/* Load up fishClones from database table to track items. */
{
fishClonesFilter = cartUsualString(cart, "fishClones.filter", "green");
fishClonesMap = cartUsualString(cart, "fishClones.type", fcoeEnumToString(0));
fishClonesType = fcoeStringToEnum(fishClonesMap);
bedLoadItem(tg, "fishClones", (ItemLoader)fishClonesLoad);
filterItems(tg, fishClonesFilterItem, fishClonesFilter);
fishClonesFilterColor = getFilterColor(fishClonesFilter, 0);
}


void freeFishClones(struct track *tg)
/* Free up fishClones items. */
{
fishClonesFreeList((struct fishClones**)&tg->items);
}

Color fishClonesColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of fishClones track item. */
{
if ((fishClonesFilterItem(tg, item)) && (fishClonesFilterColor))
    return fishClonesFilterColor;
else
    return tg->ixColor;
}

void fishClonesMethods(struct track *tg)
/* Make track for FISH clones. */
{
tg->loadItems = loadFishClones;
tg->freeItems = freeFishClones;
tg->itemColor = fishClonesColor;
}

void loadSynteny(struct track *tg)
{
bedLoadItem(tg, tg->table, (ItemLoader)synteny100000Load);
slSort(&tg->items, bedCmp);
}

void freeSynteny(struct track *tg)
{
synteny100000FreeList((struct synteny100000**)&tg->items);
}

void loadMouseOrtho(struct track *tg)
{
bedLoadItem(tg, "mouseOrtho", (ItemLoader)mouseOrthoLoad);
slSort(&tg->items, bedCmpPlusScore);
}

void freeMouseOrtho(struct track *tg)
{
mouseOrthoFreeList((struct mouseOrtho**)&tg->items);
}

Color mouseOrthoItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of psl track item based on chromosome. */
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
return ((Color)getChromColor(chromStr, hvg));
}

void mouseOrthoMethods(struct track *tg)
{
char option[128];
char *optionStr ;
tg->loadItems = loadMouseOrtho;
tg->freeItems = freeMouseOrtho;

safef( option, sizeof(option), "%s.color", tg->track);
optionStr = cartUsualString(cart, option, "on");
if( sameString( optionStr, "on" )) /*use anti-aliasing*/
    tg->itemColor = mouseOrthoItemColor;
else
    tg->itemColor = NULL;
tg->drawName = TRUE;
}

void loadHumanParalog(struct track *tg)
{
bedLoadItem(tg, "humanParalog", (ItemLoader)humanParalogLoad);
slSort(&tg->items, bedCmpPlusScore);
}

void freeHumanParalog(struct track *tg)
{
humanParalogFreeList((struct humanParalog**)&tg->items);
}

Color humanParalogItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of psl track item based on chromosome. */
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
return ((Color)getChromColor(chromStr, hvg));
}

void humanParalogMethods(struct track *tg)
{
char option[128];
char *optionStr ;
tg->loadItems = loadHumanParalog;
tg->freeItems = freeHumanParalog;

safef( option, sizeof(option), "%s.color", tg->track);
optionStr = cartUsualString(cart, option, "on");
if( sameString( optionStr, "on" )) /*use anti-aliasing*/
    tg->itemColor = humanParalogItemColor;
else
    tg->itemColor = NULL;
tg->drawName = TRUE;
}

Color syntenyItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of psl track item based on chromosome. */
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
return ((Color)getChromColor(chromStr, hvg));
}

void syntenyMethods(struct track *tg)
{
tg->loadItems = loadSynteny;
tg->freeItems = freeSynteny;
tg->itemColor = syntenyItemColor;
tg->drawName = FALSE;
tg->subType = lfWithBarbs ;
}

void loadMouseSyn(struct track *tg)
/* Load up mouseSyn from database table to track items. */
{
bedLoadItem(tg, "mouseSyn", (ItemLoader)mouseSynLoad);
}


void freeMouseSyn(struct track *tg)
/* Free up mouseSyn items. */
{
mouseSynFreeList((struct mouseSyn**)&tg->items);
}

Color mouseSynItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of mouseSyn track item. */
{
char chromStr[20];
struct mouseSyn *ms = item;

strncpy(chromStr, ms->name+strlen("mouse "), 2);
chromStr[2] = '\0';
return ((Color)getChromColor(chromStr, hvg));
}

void mouseSynMethods(struct track *tg)
/* Make track for mouseSyn. */
{
tg->loadItems = loadMouseSyn;
tg->freeItems = freeMouseSyn;
tg->itemColor = mouseSynItemColor;
tg->drawName = TRUE;
}

void loadMouseSynWhd(struct track *tg)
/* Load up mouseSynWhd from database table to track items. */
{
bedLoadItem(tg, "mouseSynWhd", (ItemLoader)mouseSynWhdLoad);
}

void freeMouseSynWhd(struct track *tg)
/* Free up mouseSynWhd items. */
{
mouseSynWhdFreeList((struct mouseSynWhd**)&tg->items);
}

Color mouseSynWhdItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of mouseSynWhd track item. */
{
char chromStr[20];
struct mouseSynWhd *ms = item;

if (startsWith("chr", ms->name))
    {
    strncpy(chromStr, ms->name+strlen("chr"), 2);
    chromStr[2] = '\0';
    return((Color)getChromColor(chromStr, hvg));
    }
else
    {
    return(tg->ixColor);
    }
}

void mouseSynWhdMethods(struct track *tg)
/* Make track for mouseSyn. */
{
tg->loadItems = loadMouseSynWhd;
tg->freeItems = freeMouseSynWhd;
tg->itemColor = mouseSynWhdItemColor;
tg->subType = lfWithBarbs;
}

void lfChromColorMethods(struct track *tg)
/* Standard linked features methods + chrom-coloring (for contrib. synt). */
{
tg->itemColor = lfChromColor;
tg->itemNameColor = NULL;
}

Color bedChromColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of chromosome for bed type items
 * where the chromosome/scaffold is listed somewhere in the bed->name. */
{
struct bed *bed = item;
return getSeqColorDefault(bed->name, hvg, tg->ixColor);
}

void bedChromColorMethods(struct track *tg)
/* Standard simple bed methods + chrom-coloring (for contrib. synt). */
{
tg->itemColor = bedChromColor;
}

char *colonTruncName(struct track *tg, void *item)
/* Return name of bed track item, chopped at the first ':'. */
{
struct bed *bed = item;
char *ptr = NULL;
if (bed->name == NULL)
    return "";
ptr = strchr(bed->name, ':');
if (ptr != NULL)
    *ptr = 0;
return bed->name;
}

void deweySyntMethods(struct track *tg)
/* Standard simple bed methods + chrom-coloring + name-chopping. */
{
bedChromColorMethods(tg);
tg->itemName = colonTruncName;
}


void loadEnsPhusionBlast(struct track *tg)
/* Load up ensPhusionBlast from database table to track items. */
{
struct ensPhusionBlast *epb;
char *ptr;
char buf[16];

bedLoadItem(tg, tg->table, (ItemLoader)ensPhusionBlastLoad);
// for name, append abbreviated starting position to the xeno chrom:
for (epb=tg->items;  epb != NULL;  epb=epb->next)
    {
    ptr = strchr(epb->name, '.');
    if (ptr == NULL)
	ptr = epb->name;
    else
	ptr++;
    safef(buf, sizeof(buf), "%s %dk", ptr, (int)(epb->xenoStart/1000));
    free(epb->name);
    epb->name = cloneString(buf);
    }
}

void freeEnsPhusionBlast(struct track *tg)
/* Free up ensPhusionBlast items. */
{
ensPhusionBlastFreeList((struct ensPhusionBlast**)&tg->items);
}

Color ensPhusionBlastItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of ensPhusionBlast track item. */
{
struct ensPhusionBlast *epb = item;
return getSeqColorDefault(epb->name, hvg, tg->ixColor);
}

void ensPhusionBlastMethods(struct track *tg)
/* Make track for mouseSyn. */
{
tg->loadItems = loadEnsPhusionBlast;
tg->freeItems = freeEnsPhusionBlast;
tg->itemColor = ensPhusionBlastItemColor;
tg->subType = lfWithBarbs;
}

void tetWabaMethods(struct track *tg)
/* Make track for Tetraodon alignments. */
{
wabaMethods(tg);
tg->customPt = "_tet_waba";
}

void cbrWabaMethods(struct track *tg)
/* Make track for C briggsae alignments. */
{
wabaMethods(tg);
tg->customPt = "_wabaCbr";
cartSetInt(cart, "cbrWaba.start", winStart);
cartSetInt(cart, "cbrWaba.end", winEnd);
}

void bactigLoad(struct track *tg)
/* Load up bactigs from database table to track items. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
struct bactigPos *bactigList = NULL, *bactig;
int rowOffset;

/* Get the bactigs and load into tg->items. */
sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd,
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

void bactigFree(struct track *tg)
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

void bactigMethods(struct track *tg)
/* Make track for bactigPos */
{
tg->loadItems = bactigLoad;
tg->freeItems = bactigFree;
// tg->drawItems = bactigDraw;
// tg->canPack = FALSE;
}


void gapLoad(struct track *tg)
/* Load up clone alignments from database tables and organize. */
{
bedLoadItem(tg, "gap", (ItemLoader)agpGapLoad);
}

void gapFree(struct track *tg)
/* Free up gap items. */
{
agpGapFreeList((struct agpGap**)&tg->items);
}

char *gapName(struct track *tg, void *item)
/* Return name of gap track item. */
{
static char buf[24];
struct agpGap *gap = item;
safef(buf, sizeof buf, "%s %s", gap->type, gap->bridge);
return buf;
}

static void gapDrawAt(struct track *tg, void *item,
                      struct hvGfx *hvg, int xOff, int y, double scale,
                      MgFont *font, Color color, enum trackVisibility vis)
/* Draw gap items. */
{
struct agpGap *gap = item;
int heightPer = tg->heightPer;
int x1,x2,w;
int halfSize = heightPer/2;

x1 = round((double)((int)gap->chromStart-winStart)*scale) + xOff;
x2 = round((double)((int)gap->chromEnd-winStart)*scale) + xOff;
w = x2-x1;
if (w < 1)
    w = 1;
if (sameString(gap->bridge, "no"))
    hvGfxBox(hvg, x1, y, w, heightPer, color);
else  /* Leave white line in middle of bridged gaps. */
    {
    hvGfxBox(hvg, x1, y, w, halfSize, color);
    hvGfxBox(hvg, x1, y+heightPer-halfSize, w, halfSize, color);
    }
}

void gapMethods(struct track *tg)
/* Make track for positions of all gaps. */
{
tg->loadItems = gapLoad;
tg->freeItems = gapFree;
tg->drawItemAt = gapDrawAt;
tg->drawItems = genericDrawItems;
tg->itemName = gapName;
tg->mapItemName = gapName;
}
#endif /* GBROWSE */

#ifndef GBROWSE
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
lf->name = cloneString(psl->qName);
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
linkedFeaturesBoundsAndGrays(lf);
lf->start = psl->tStart;	/* Correct for rounding errors... */
lf->end = psl->tEnd;
return lf;
}

struct linkedFeatures *lfFromPslsWScoresInRange(char *table, int start, int end, char *chromName, boolean isXeno, float maxScore)
/* Return linked features from range of table with the scores scaled appropriately */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int rowOffset;
struct linkedFeatures *lfList = NULL, *lf;

sr = hRangeQuery(conn,table,chromName,start,end,NULL, &rowOffset);
while((row = sqlNextRow(sr)) != NULL)
    {
    struct pslWScore *pslWS = pslWScoreLoad(row+rowOffset);
    lf = lfFromPslWScore(pslWS, 1, FALSE, maxScore);
    slAddHead(&lfList, lf);
    pslWScoreFree(&pslWS);
    }
slReverse(&lfList);
sqlFreeResult(&sr);
hFreeConn(&conn);
return lfList;
}

void loadUniGeneAli(struct track *tg)
{
tg->items = lfFromPslsWScoresInRange("uniGene", winStart, winEnd,
	chromName,FALSE, 1.0);
}

void uniGeneMethods(struct track *tg)
/* Load up uniGene methods - a slight specialization of
 * linked features. */
{
linkedFeaturesMethods(tg);
tg->loadItems = loadUniGeneAli;
tg->colorShades = shadesOfGray;
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

void lfFromAncientRBed(struct track *tg)
/* filters the bedList stored at tg->items
into a linkedFeaturesSeries as determined by
minimum munber of aligned bases cutoff */
{
struct bed *bedList= NULL;
int ancientRMinLength = atoi(cartUsualString(cart, "ancientR.minLength", "50"));
bedList = tg->items;
tg->items = bedFilterMinLength(bedList, ancientRMinLength);
bedFreeList(&bedList);
}


void loadAncientR(struct track *tg)
/* Load up ancient repeats from database table to track items
 * filtering out those below a certain length threshold,
   in number of aligned bases. */
{
bedLoadItem(tg, "ancientR", (ItemLoader)bedLoad12);
lfFromAncientRBed(tg);
}


void ancientRMethods(struct track *tg)
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
if (maxDeviation == 0)
    errAbort("ERROR: hgTracksExample::getExprDataColor() maxDeviation can't be zero\n");

colorIndex = (int)(absVal * maxRGBShade/maxDeviation);

/* Return the correct color depending on color scheme and shades */
if (RG_COLOR_SCHEME)
    {
    if (val > 0)
        return shadesOfRed[colorIndex];
    else
        return shadesOfGreen[colorIndex];
    }
else
    {
    if (val > 0)
        return shadesOfRed[colorIndex];
    else
        return shadesOfBlue[colorIndex];
    }
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

void cghLoadTrack(struct track *tg)
/* Load up CGH tracks.  (Will query database during drawing for a change.) */
{
tg->items = makeCghItems();
}

static void cghDraw(struct track *tg, int seqStart, int seqEnd,
                    struct hvGfx *hvg, int xOff, int yOff, int width,
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
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int rowOffset;
struct cgh cghRecord;
char where[256];

/* Set up the shades of colors */
if (!exprBedColorsMade)
    makeRedGreenShades(hvg);

if (isFull)
    {
    /* Create tissue specific average track */
    struct hash *hash = newHash(6);

    for (cghi = tg->items; cghi != NULL; cghi = cghi->next)
        {
	cghi->yOffset = y;
	y += lineHeight;
	hashAdd(hash, cghi->class, cghi);
	}
    
    sqlSafef(where, sizeof where, "type = 2");
    sr = hRangeQuery(conn, "cgh", chromName, winStart, winEnd, where, &rowOffset);
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
	hvGfxBox(hvg, x1, cghi->yOffset, w, heightPer, col);
        }
    freeHash(&hash);
    }
else
    {
    sqlSafef(where, sizeof where, "type = 1");
    sr = hRangeQuery(conn, "cgh", chromName, winStart, winEnd, where, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	cghStaticLoad(row+rowOffset, &cghRecord);
	col = getExprDataColor((cghRecord.score * -1), 0.5, TRUE);
	x1 = roundingScale(cghRecord.chromStart-winStart, width, baseWidth)+xOff;
	x2 = roundingScale(cghRecord.chromEnd-winStart, width, baseWidth)+xOff;
	w = x2-x1;
	if (w <= 0)
	  w = 1;
	hvGfxBox(hvg, x1, yOff, w, heightPer, col);
        }
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void cghMethods(struct track *tg)
/* Make track for CGH experiments. */
{
repeatMethods(tg);
tg->loadItems = cghLoadTrack;
tg->drawItems = cghDraw;
tg->colorShades = shadesOfGray;
tg->totalHeight = tgFixedTotalHeightNoOverflow;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = tgItemNoStart;
tg->itemEnd = tgItemNoEnd;
}


void loadMcnBreakpoints(struct track *tg)
/* Load up MCN breakpoints from database table to track items. */
{
bedLoadItem(tg, "mcnBreakpoints", (ItemLoader)mcnBreakpointsLoad);
}

void freeMcnBreakpoints(struct track *tg)
/* Free up MCN Breakpoints items. */
{
mcnBreakpointsFreeList((struct mcnBreakpoints**)&tg->items);
}

void mcnBreakpointsMethods(struct track *tg)
/* Make track for mcnBreakpoints. */
{
tg->loadItems = loadMcnBreakpoints;
tg->freeItems = freeMcnBreakpoints;
}


static void drawTriangle(struct track *tg, int seqStart, int seqEnd,
                         struct hvGfx *hvg, int xOff, int yOff, int width,
                         MgFont *font, Color color, enum trackVisibility vis)
/* Draw triangle items.   Relies mostly on bedDrawSimple, but does put
 * a horizontal box connecting items in full mode. */
{
/* In dense mode try and draw golden background for promoter regions. */
if (vis == tvDense)
    {
    if (hTableExists(database,  "rnaCluster"))
        {
	int heightPer = tg->heightPer;
	Color gold = hvGfxFindColorIx(hvg, 250,190,60);
	int promoSize = 1000;
	int rowOffset;
	double scale = scaleForPixels(width);
	struct sqlConnection *conn = hAllocConn(database);
        struct sqlResult *sr = hRangeQuery(conn, "rnaCluster", chromName,
		winStart - promoSize, winEnd + promoSize, NULL, &rowOffset);
	char **row;
	// hvGfxBox(hvg, xOff, yOff, width, heightPer, gold);
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
	    drawScaledBox(hvg, start, end, scale, xOff, yOff, heightPer, gold);
	    }

	hFreeConn(&conn);
	}
    }
bedDrawSimple(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
}

void triangleMethods(struct track *tg)
/* Register custom methods for regulatory triangle track. */
{
tg->drawItems = drawTriangle;
}

static void drawEranModule(struct track *tg, int seqStart, int seqEnd,
                           struct hvGfx *hvg, int xOff, int yOff, int width,
                           MgFont *font, Color color, enum trackVisibility vis)
/* Draw triangle items.   Relies mostly on bedDrawSimple, but does put
 * a horizontal box connecting items in full mode. */
{
/* In dense mode try and draw golden background for promoter regions. */
if (vis == tvDense)
    {
    if (hTableExists(database,  "esRegUpstreamRegion"))
        {
	int heightPer = tg->heightPer;
	Color gold = hvGfxFindColorIx(hvg, 250,190,60);
	int rowOffset;
        double scale = scaleForPixels(width);
        struct sqlConnection *conn = hAllocConn(database);
        struct sqlResult *sr = hRangeQuery(conn, "esRegUpstreamRegion",
                                           chromName, winStart, winEnd, NULL, &rowOffset);
        char **row;
        while ((row = sqlNextRow(sr)) != NULL)
            {
	    int start, end;
	    row += rowOffset;
	    start = atoi(row[1]);
	    end = atoi(row[2]);
	    drawScaledBox(hvg, start, end, scale, xOff, yOff, heightPer, gold);
	    }
	hFreeConn(&conn);
	}
    }
bedDrawSimple(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
}

void eranModuleMethods(struct track *tg)
/* Register custom methods for eran regulatory module methods. */
{
tg->drawItems = drawEranModule;
}
#endif /* GBROWSE */

static struct trackDb *rGetTdbNamed(struct trackDb *tdb, char *name)
/* Return tdb of given name in self or children. */
{
if (sameString(name, tdb->track))
    return tdb;
struct trackDb *child;
for (child = tdb->subtracks; child != NULL; child = child->next)
    {
    struct trackDb *ret = rGetTdbNamed(child, name);
    if (ret != NULL)
        return ret;
    }
return NULL;
}

static struct trackDb *getSubtrackTdb(struct track *subtrack)
/* If subtrack->tdb is actually the composite tdb, return the tdb for
 * the subtrack so we can see its settings. */
{
struct trackDb *subTdb = rGetTdbNamed(subtrack->tdb, subtrack->track);
if (subTdb == NULL)
    errAbort("Can't find tdb for subtrack %s -- was getSubtrackTdb called on "
	     "non-subtrack?", subtrack->track);
return subTdb;
}

static bool subtrackEnabledInTdb(struct track *subtrack)
/* Return TRUE unless the subtrack was declared with "subTrack ... off". */
{
bool enabled = TRUE;
char *words[2];
char *setting;
/* Get the actual subtrack tdb so we can see its subTrack setting (not
 * composite tdb's):  */
struct trackDb *subTdb= getSubtrackTdb(subtrack);
if ((setting = trackDbLocalSetting(subTdb, "parent")) != NULL)
    {
    if (chopLine(cloneString(setting), words) >= 2)
        if (sameString(words[1], "off"))
            enabled = FALSE;
    }
return enabled;
}

bool isSubtrackVisible(struct track *subtrack)
/* Has this subtrack not been deselected in hgTrackUi or declared with
 * "subTrack ... off"?  -- assumes composite track is visible. */
{
if (subtrack->subTrackVisSet)
    return subtrack->subTrackVis;
boolean overrideComposite = (NULL != cartOptionalString(cart, subtrack->track));
if (subtrack->limitedVisSet && subtrack->limitedVis == tvHide)
    return FALSE;
bool enabledInTdb = subtrackEnabledInTdb(subtrack);
char option[4096];
safef(option, sizeof(option), "%s_sel", subtrack->track);
boolean enabled = cartUsualBoolean(cart, option, enabledInTdb);
if (overrideComposite)
    enabled = TRUE;
subtrack->subTrackVisSet = TRUE;
subtrack->subTrackVis = enabled;
return enabled;
}

static int subtrackCount(struct track *trackList)
/* Count the number of visible subtracks in (sub)trackList. */
{
struct track *subtrack;
int ct = 0;
for (subtrack = trackList; subtrack; subtrack = subtrack->next)
    if (isSubtrackVisible(subtrack))
        ct++;
return ct;
}

static boolean canWiggle(struct track *tg)
/* Is this a track type that can wiggle. */
{
return (tg->isBigBed && 
            !startsWith("bigInteract",tg->tdb->type) &&
            !startsWith("bigMaf",tg->tdb->type) &&
            !startsWith("bigLolly",tg->tdb->type))
        || startsWith("vcfTabix", tg->tdb->type)
        || startsWith("bam", tg->tdb->type);
}

enum trackVisibility limitVisibility(struct track *tg)
/* Return default visibility limited by number of items and
 * by parent visibility if part of a coposite track.
 * This also sets tg->height. */
{
if (forceWiggle && canWiggle(tg))
    {
    tg->limitWiggle = TRUE;
    }

if (!tg->limitedVisSet)
    {
    tg->limitedVisSet = TRUE;  // Prevents recursive loop!

    // optional setting to draw labels onto the feature boxes, not next to them
    char *setting = cartOrTdbString(cart, tg->tdb, "labelOnFeature", NULL);
    if (setting)
        {
        if (sameString(setting, "on") || sameString(setting, "true"))
            tg->drawLabelInBox = TRUE;
        else if (sameString(setting, "noDense"))
            {
            tg->drawLabelInBox = TRUE;
            tg->drawLabelInBoxNotDense = TRUE;
            }
        }
    enum trackVisibility vis = tg->visibility;
    int h;
    int maxHeight = maximumTrackHeight(tg);

    if (vis == tvHide)
        {
        tg->height = 0;
        tg->limitedVis = tvHide;
        return tvHide;
        }
    if (tg->subtracks != NULL)
        {
        struct track *subtrack;
        int subCnt = subtrackCount(tg->subtracks);
        maxHeight = maxHeight * max(subCnt,1);
        //if (subCnt > 4)
        //    maxHeight *= 2; // NOTE: Large composites should suffer an additional restriction.
        if (!tg->syncChildVisToSelf)
            {
            for (subtrack = tg->subtracks;  subtrack != NULL; subtrack = subtrack->next)
                limitVisibility(subtrack);
            }
        }
    if (canWiggle(tg))   // if this is a track type that can wiggle, we want to go straight to that rather than reduce visibility
        {
        if ((h = tg->totalHeight(tg, vis)) > maxHeight && vis != tvDense)
            {
            tg->limitWiggle = TRUE;
            tg->tdb->type = cloneString("wig");
            }
        if ( tg->limitWiggle)   // auto-density coverage is alway tvFull
            {
            if (tg->visibility == tvDense)
                tg->visibility = tg->limitedVis = tvDense;
            else
                tg->visibility = tg->limitedVis = tvFull;
            }
        else
            tg->limitedVis = (vis == tvShow) ? tvFull : vis;
        }
    else
        {
        while ((h = tg->totalHeight(tg, vis)) > maxHeight && vis != tvDense)
            {
            if (vis == tvFull && tg->canPack)
                vis = tvPack;
            else if (vis == tvPack)
                vis = tvSquish;
            else
                vis = tvDense;
            }

        if (tg->limitedVis == tvHide)
            tg->limitedVis = vis;
        else
            tg->limitedVis = tvMin(vis,tg->limitedVis);
        }

    tg->height = h;
    if (tg->syncChildVisToSelf)
        {
        struct track *subtrack;
	for (subtrack = tg->subtracks;  subtrack != NULL; subtrack = subtrack->next)
	    {
	    subtrack->visibility = tg->visibility;
	    subtrack->limitedVis = tg->limitedVis;
            subtrack->limitedVisSet = tg->limitedVisSet;
            }
        }
    else if (tdbIsComposite(tg->tdb)) // If a composite is restricted,
        {                             // it's children should be atleast as restricted.
        struct track *subtrack;
        for (subtrack = tg->subtracks;  subtrack != NULL; subtrack = subtrack->next)
            {
            if ( tg->limitWiggle) // these are always in tvFull
                subtrack->limitedVis = tvMin(subtrack->limitedVis, tg->limitedVis);
            // But don't prevent subtracks from being further restricted!
            //subtrack->limitedVisSet = tg->limitedVisSet;
            }
        }

    if (tg->height == 0 && tg->limitedVis != tvHide)
        tg->limitedVisSet = FALSE;  // Items may not be loaded yet, so going to need to check again
    }
return tg->limitedVis;
}

void compositeTrackVis(struct track *track)
/* set visibilities of subtracks */
{
struct track *subtrack;

if (track->visibility == tvHide)
    return;

/* Count visible subtracks. */
for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
    if (!subtrack->limitedVisSet)
	{
	if (isSubtrackVisible(subtrack))
	    subtrack->visibility = track->visibility;
	else
	    subtrack->visibility = tvHide;
	}
}

boolean colorsSame(struct rgbColor *a, struct rgbColor *b)
/* Return true if two colors are the same. */
{
return a->r == b->r && a->g == b->g && a->b == b->b;
}

#ifndef GBROWSE
void loadValAl(struct track *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct linkedFeatures *lfList = NULL, *lf;
struct bed *bed, *list = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;

sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, 15);
    slAddHead(&list, bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
//slReverse(&list);

for (bed = list; bed != NULL; bed = bed->next)
    {
    struct simpleFeature *sf;
    int i;
    lf = lfFromBed(bed);
    lf->grayIx = 9;
    slReverse(&lf->components);
    for (sf = lf->components, i = 0; sf != NULL && i < bed->expCount;
		sf = sf->next, i++)
	{
	sf->grayIx = bed->expIds[i];
	//sf->grayIx = grayInRange((int)(bed->expIds[i]),11,13);
	}
    slAddHead(&lfList,lf);
    }
tg->items = lfList;
}

void valAlDrawAt(struct track *tg, void *item,
                 struct hvGfx *hvg, int xOff, int y, double scale,
                 MgFont *font, Color color, enum trackVisibility vis)
/* Draw the operon at position. */
{
struct linkedFeatures *lf = item;
struct simpleFeature *sf;
int heightPer = tg->heightPer;
int x1,x2;
int s, e;
int midY = y + (heightPer>>1);
int w;

color = shadesOfGray[2];
x1 = round((double)((int)lf->start-winStart)*scale) + xOff;
x2 = round((double)((int)lf->end-winStart)*scale) + xOff;
w = x2-x1;
innerLine(hvg, x1, midY, w, color);
/*
if (vis == tvFull || vis == tvPack)
    {
    clippedBarbs(hvg, x1, midY, w, tl.barbHeight, tl.barbSpacing,
                 lf->orientation, color, FALSE);
    }
    */
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    int yOff;
    s = sf->start; e = sf->end;
    /* shade ORF (exon) based on the grayIx value of the sf */
    switch(sf->grayIx)
	{
	case 1:
	    continue;
	    heightPer = tg->heightPer>>2;
	    yOff = y;
	    color = hvGfxFindColorIx(hvg, 204, 204,204);
	    break;
	case 2:
	    heightPer = tg->heightPer;
	    yOff = y;
	    color = hvGfxFindColorIx(hvg, 252, 90, 90);
	    break;
	case 3:
	    yOff = midY -(tg->heightPer>>2);
	    heightPer = tg->heightPer>>1;
	    color = hvGfxFindColorIx(hvg, 0, 0,0);
	    break;
	default:
	    continue;
	}
    drawScaledBox(hvg, s, e, scale, xOff, yOff, heightPer,
			color );
    }
}

void valAlMethods(struct track *tg)
{
linkedFeaturesMethods(tg);
tg->loadItems = loadValAl;
tg->colorShades = shadesOfGray;
tg->drawItemAt = valAlDrawAt;
}

void pgSnpDrawScaledBox(struct hvGfx *hvg, int chromStart, int chromEnd,
        double scale, int xOff, int y, int height, Color color)
/* Draw a box scaled from chromosome to window coordinates.
 * Get scale first with scaleForPixels. */
{
int x1 = round((double)(chromStart-winStart)*scale) + xOff;
int x2 = round((double)(chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
if (w < 1)
    w = 1;
hvGfxBox(hvg, x1, y, w, height, color);
}

char *pgSnpName(struct track *tg, void *item)
/* Return name of pgSnp track item. */
{
struct pgSnp *myItem = item;
char *copy = cloneString(myItem->name);
char *name = NULL;
boolean cmpl = cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE);
if (revCmplDisp)
   cmpl = !cmpl;
/*complement can't handle the /'s */
if (cmpl)
    {
    char *list[8];
    int i;
    struct dyString *ds = dyStringNew(255);
    i = chopByChar(copy, '/', list, myItem->alleleCount);
    for (i=0; i < myItem->alleleCount; i++)
        {
        complement(list[i], strlen(list[i]));
        if (revCmplDisp)
            reverseComplement(list[i], strlen(list[i]));
        dyStringPrintf(ds, "%s", list[i]);
        if (i != myItem->alleleCount - 1)
            dyStringPrintf(ds, "%s", "/");
        }
    name = cloneString(ds->string);
    dyStringFree(&ds);
    }
else if (revCmplDisp)
    {
    char *list[8];
    int i;
    struct dyString *ds = dyStringNew(255);
    i = chopByChar(copy, '/', list, myItem->alleleCount);
    for (i=0; i < myItem->alleleCount; i++)
        {
        reverseComplement(list[i], strlen(list[i]));
        dyStringPrintf(ds, "%s", list[i]);
        if (i != myItem->alleleCount - 1)
            dyStringPrintf(ds, "%s", "/");
        }
    name = cloneString(ds->string);
    dyStringFree(&ds);
    }
/* if no changes needed return bed name */
if (name == NULL)
    name = cloneString(myItem->name);
return name;
}

void pgSnpTextRight(char *display, struct hvGfx *hvg, int x1, int y, int width, int height, Color color, MgFont *font, char *allele, int itemY, int lineHeight)
/* Put text anchored on right upper corner, doing separate colors if needed. */
{
struct hvGfx *hvgWhich = hvg;    // There may be a separate image for sideLabel!
int textX = x1 - width - 2;
boolean snapLeft = (textX < fullInsideX);
if (snapLeft)        /* Snap label to the left. */
    {
    if (hvgSide != NULL)
        hvgWhich = hvgSide;
    textX = leftLabelX;
    width = leftLabelWidth-1;
    }
int clipX, clipY, clipWidth, clipHeight;
hvGfxGetClip(hvgWhich, &clipX, &clipY, &clipWidth, &clipHeight);
hvGfxUnclip(hvgWhich);
hvGfxSetClip(hvgWhich, textX, y, width, height);
if (sameString(display, "freq"))
    {
    color = MG_BLACK;
    if (startsWith("A", allele))
        color = MG_RED;
    else if (startsWith("C", allele))
        color = MG_BLUE;
    else if (startsWith("G", allele))
        color = darkGreenColor;
    else if (startsWith("T", allele))
        color = MG_MAGENTA;
    }
hvGfxTextRight(hvgWhich, textX, y, width, height, color, font, allele);
hvGfxUnclip(hvgWhich);
hvGfxSetClip(hvgWhich, clipX, clipY, clipWidth, clipHeight);
}

void pgSnpDrawAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y, double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw the personal genome SNPs at a given position. */
{
struct pgSnp *myItem = item;
boolean cmpl = cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE);
char *display = "freq"; //cartVar?
if (revCmplDisp)
   cmpl = !cmpl;
if (vis == tvSquish || vis == tvDense || myItem->alleleCount > 2)
    {
    withIndividualLabels = TRUE; //haven't done label for this one
    bedDrawSimpleAt(tg, myItem, hvg, xOff, y, scale, font, color, vis);
    return;
    }

/* get allele(s) to display */
char *allele[8];
char *freq[8];
int allTot = 0;
int s = max(myItem->chromStart, winStart), e = min(myItem->chromEnd, winEnd);
if (s > e)
    return;
int x1 = round((s-winStart)*scale) + xOff;
int x2 = round((e-winStart)*scale) + xOff;
int w = x2-x1;
if (w < 1)
    w = 1;
char *nameCopy = cloneString(myItem->name);
char *allFreqCopy = cloneString(myItem->alleleFreq);
int cnt = chopByChar(nameCopy, '/', allele, myItem->alleleCount);
if (cnt != myItem->alleleCount)
    errAbort("Bad allele name '%s' (%s:%d-%d): expected %d /-sep'd alleles", myItem->name,
	     myItem->chrom, myItem->chromStart+1, myItem->chromEnd, myItem->alleleCount);
int fcnt = chopByChar(allFreqCopy, ',', freq, myItem->alleleCount);
if (fcnt != myItem->alleleCount && fcnt != 0)
    errAbort("Bad freq '%s' for '%s' (%s:%d-%d): expected %d ,-sep'd numbers",
	     myItem->alleleFreq, myItem->name,
	     myItem->chrom, myItem->chromStart+1, myItem->chromEnd,
	     myItem->alleleCount);
int i = 0;
for (i=0;i<fcnt;i++)
    allTot += atoi(freq[i]);
/* draw a tall box */
if (sameString(display, "freq"))
    {
    if (allTot == 0)
        {
        pgSnpDrawScaledBox(hvg, myItem->chromStart, myItem->chromEnd, scale,
            xOff, y, tg->heightPer, MG_GRAY);
        }
    else
        { /* stack boxes colored to match alleles */
        Color allC = MG_BLACK;
        int yCopy = y;
        for (i=0;i<myItem->alleleCount;i++)
            {
            double t = atoi(freq[i])/(double)allTot;
            int h = trunc(tg->heightPer*t);
            char *aCopy = cloneString(allele[i]);
            if (cmpl)
                complement(aCopy, strlen(aCopy));
            if (revCmplDisp)
                reverseComplement(aCopy, strlen(aCopy));
            if (startsWith("A", aCopy))
                allC = MG_RED;
            else if (startsWith("C", aCopy))
                allC = MG_BLUE;
            else if (startsWith("G", aCopy))
                allC = darkGreenColor;
            else if (startsWith("T", aCopy))
                allC = MG_MAGENTA;
            else
                allC = MG_BLACK;
            pgSnpDrawScaledBox(hvg, myItem->chromStart, myItem->chromEnd, scale,
                xOff, yCopy, h, allC);
            yCopy += h;
            }
        }
    }
else
    {
    pgSnpDrawScaledBox(hvg, myItem->chromStart, myItem->chromEnd, scale, xOff,
        y, tg->heightPer, color);

    }

/* determine graphics attributes for vgTextCentered */
int allHeight = trunc(tg->heightPer / 2);
int allWidth = mgFontStringWidth(font, allele[0]);
int all2Width = 0;
if (cnt > 1)
    {
    all2Width = mgFontStringWidth(font, allele[1]);
    if (all2Width > allWidth)
         allWidth = all2Width; /* use max */
    }
int yCopy = y + 1;

/* allele 1, should be insertion if doesn't fit */
if (sameString(display, "freq"))
    {
    if (cmpl)
        complement(allele[0], strlen(allele[0]));
    if (revCmplDisp)
        reverseComplement(allele[0], strlen(allele[0]));
    pgSnpTextRight(display, hvg, x1, yCopy, allWidth, allHeight, color, font, allele[0], y, tg->lineHeight);
    }
else
    {
    if (cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE))
        complement(allele[0], strlen(allele[0]));
    /* sequence in box automatically reversed with browser */
    spreadBasesString(hvg, x1, yCopy, w, allHeight, MG_WHITE, font, allele[0], strlen(allele[0]), FALSE);
    }

if (cnt > 1)
    { /* allele 2 */
    yCopy += allHeight;

    if (sameString(display, "freq"))
        {
        if (cmpl)
            complement(allele[1], strlen(allele[1]));
        if (revCmplDisp)
            reverseComplement(allele[1], strlen(allele[1]));
        pgSnpTextRight(display, hvg, x1, yCopy, allWidth, allHeight, color, font, allele[1], y, tg->lineHeight);
        }
    else
        {
        if (cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE))
            complement(allele[1], strlen(allele[1]));
        spreadBasesString(hvg, x1, yCopy, w, allHeight, MG_WHITE, font, allele[1], strlen(allele[1]), FALSE);
        }
    }
/* map box for link, when text outside box */
if (allWidth >= w || sameString(display, "freq"))
    {
    tg->mapItem(tg, hvg, item, tg->itemName(tg, item),
                tg->mapItemName(tg, item), myItem->chromStart, myItem->chromEnd,
                x1-allWidth-2, y+1, allWidth+w, tg->heightPer);
    }
withIndividualLabels = FALSE; //turn labels off, done already
}

int pgSnpHeight (struct track *tg, enum trackVisibility vis)
{
   int f = tl.fontHeight;
   int t = f + 1;
   if (vis != tvDense)
       {
       f = f*2;
       t = t*2;
       }
   return tgFixedTotalHeightOptionalOverflow(tg,vis, t, f, FALSE);
}

void freePgSnp(struct track *tg)
/* Free pgSnp items. */
{
pgSnpFreeList(((struct pgSnp **)(&tg->items)));
}

void loadBedMethyl(struct track *tg)
/* Load up bedMethyl type tracks */
{
struct customTrack *ct = tg->customPt;
char *table = tg->table;
struct sqlConnection *conn;
if (ct == NULL)
    conn = hAllocConn(database);
else
    {
    conn = hAllocConn(CUSTOM_TRASH);
    table = ct->dbTableName;
    }
struct dyString *query = sqlDyStringCreate("select * from %s where ", table);
hAddBinToQuery(winStart, winEnd, query);
// for the moment we're only loading the 'm's and not the 'h's
sqlDyStringPrintf(query, "chrom = '%s' and chromStart < %d and chromEnd > %d",
	       chromName, winEnd, winStart);
tg->items = bedMethylLoadByQuery(conn, query->string);

hFreeConn(&conn);
}

void loadPgSnp(struct track *tg)
/* Load up pgSnp (personal genome SNP) type tracks */
{
struct customTrack *ct = tg->customPt;
char *table = tg->table;
struct sqlConnection *conn;
if (ct == NULL)
    conn = hAllocConn(database);
else
    {
    conn = hAllocConn(CUSTOM_TRASH);
    table = ct->dbTableName;
    }
struct dyString *query = sqlDyStringCreate("select * from %s where ", table);
hAddBinToQuery(winStart, winEnd, query);
sqlDyStringPrintf(query, "chrom = '%s' and chromStart < %d and chromEnd > %d",
	       chromName, winEnd, winStart);
tg->items = pgSnpLoadByQuery(conn, query->string);
/* base coloring/display decision on count of items */
tg->customInt = slCount(tg->items);
hFreeConn(&conn);
}

void pgSnpMapItem(struct track *tg, struct hvGfx *hvg, void *item,
        char *itemName, char *mapItemName, int start, int end, int x, int y, int width, int height)
/* create a special map box item with different
 * pop-up statusLine with allele counts
 */
{
char *directUrl = trackDbSetting(tg->tdb, "directUrl");
boolean withHgsid = (trackDbSetting(tg->tdb, "hgsid") != NULL);
struct pgSnp *el = item;
char *nameCopy = cloneString(itemName); /* so chopper doesn't mess original */
char *cntCopy = cloneString(el->alleleFreq);
char *all[8];
char *freq[8];
struct dyString *ds = dyStringNew(255);
int i = 0;
chopByChar(nameCopy, '/', all, el->alleleCount);
if (differentString(el->alleleFreq, ""))
    chopByChar(cntCopy, ',', freq, el->alleleCount);

for (i=0; i < el->alleleCount; i++)
    {
    if (sameString(el->alleleFreq, "") || sameString(freq[i], "0"))
        freq[i] = "?";
    dyStringPrintf(ds, "%s:%s ", all[i], freq[i]);
    }
mapBoxHgcOrHgGene(hvg, start, end, x, y, width, height, tg->track,
                  mapItemName, ds->string, directUrl, withHgsid, NULL);
dyStringFree(&ds);
}

void pgSnpLeftLabels(struct track *tg, int seqStart, int seqEnd,
		     struct hvGfx *hvg, int xOff, int yOff, int width, int height,
		     boolean withCenterLabels, MgFont *font, Color color,
		     enum trackVisibility vis)
/* pgSnp draws its own left labels when it draws the item in pack or full mode.
 * We don't want the default left labels when in full mode because they can overlap
 * with the item-drawing labels, but we do still need dense mode left labels. */
{
if (vis == tvDense)
    {
    if (isCenterLabelIncluded(tg))
	yOff += mgFontLineHeight(font);
    hvGfxTextRight(hvg, leftLabelX, yOff, leftLabelWidth-1, tg->lineHeight,
		   color, font, tg->shortLabel);
    }
}

void bedMethylMethods (struct track *tg)
/* bedMethyl track methods */
{
bedMethods(tg);
tg->loadItems = loadBedMethyl;
tg->itemColor = bedMethylColor;
tg->mapItem= bedMethylMapItem;
tg->tdb->canPack = TRUE;
}

void pgSnpMethods (struct track *tg)
/* Personal Genome SNPs: show two alleles with stacked color bars for base alleles and
 * (if available) allele counts in mouseover. */
{
bedMethods(tg);
tg->loadItems = loadPgSnp;
tg->freeItems = freePgSnp;
tg->totalHeight = pgSnpHeight;
tg->itemName = pgSnpName;
tg->drawItemAt = pgSnpDrawAt;
tg->mapItem = pgSnpMapItem;
tg->nextItemButtonable = TRUE;
tg->nextPrevItem = linkedFeaturesLabelNextPrevItem;
tg->drawLeftLabels = pgSnpLeftLabels;
tg->canPack = TRUE;
}

void loadBlatz(struct track *tg)
{
enum trackVisibility vis = tg->visibility;
loadXenoPsl(tg);
if (vis != tvDense)
    {
    lookupProteinNames(tg);
    }
vis = limitVisibility(tg);
}

void loadBlast(struct track *tg)
{
enum trackVisibility vis = tg->visibility;
loadProteinPsl(tg);
if (vis != tvDense)
    {
    lookupProteinNames(tg);
    }
vis = limitVisibility(tg);
}

Color blastNameColor(struct track *tg, void *item, struct hvGfx *hvg)
{
return MG_BLACK;
}

void blatzMethods(struct track *tg)
/* blatz track methods */
{
tg->loadItems = loadBlatz;
tg->itemName = refGeneName;
tg->mapItemName = refGeneMapName;
tg->itemColor = blastColor;
tg->itemNameColor = blastNameColor;
}


char *cactusName(struct track *tg, void *item)
/* Get name to use for refGene item. */
{
return "";
}


void cactusDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
{
double scale = scaleForWindow(width, seqStart, seqEnd);
struct slList *item;
        color=MG_RED;
hvGfxSetClip(hvg, insideX, yOff, insideWidth, tg->height);
for (item = tg->items; item != NULL; item = item->next)
    {
    //if(tg->itemColor != NULL)
        //color = tg->itemColor(tg, item, hvg);
    /*
    if (color == MG_BLACK)
        color=MG_RED;
    else
        color=MG_BLACK;
        */
    char *name = tg->itemName(tg, item);
    name = strchr(name, '.');

    if (name != NULL)
        name++;
    int y = atoi(name) * tg->lineHeight + yOff;

    tg->drawItemAt(tg, item, hvg, xOff, y, scale, font, color, vis);
    }
hvGfxUnclip(hvg);
//linkedFeaturesDraw(tg, seqStart, seqEnd,
        //hvg, xOff, yOff, width,
        //font, color, vis);
}

void cactusDrawAt(struct track *tg, void *item,
	struct hvGfx *hvg, int xOff, int y, double scale,
	MgFont *font, Color color, enum trackVisibility vis)
{
linkedFeaturesDrawAt(tg, item,
	hvg, xOff, y, scale,
	font, color, vis);
}

int cactusHeight(struct track *tg, enum trackVisibility vis)
{
tg->height = 5 * tg->lineHeight;
return tg->height;
}

int cactusItemHeight(struct track *tg, void *item)
{
return tg->lineHeight;
}

Color cactusNameColor(struct track *tg, void *item, struct hvGfx *hvg)
{
return MG_WHITE;
}

void cactusLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width, int height,
	boolean withCenterLabels, MgFont *font, Color color,
	enum trackVisibility vis)
{
}

Color cactusColor(struct track *tg, void *item, struct hvGfx *hvg)
{
static boolean firstTime = TRUE;
static unsigned colorArray[5];

if (firstTime)
    {
    firstTime = FALSE;
    int ii;
    for(ii=0; ii < 5; ii++)
        colorArray[ii] = MG_RED;
    }

char *name = tg->itemName(tg, item);
name = strchr(name, '.');

if (name != NULL)
    name++;
int y = atoi(name);

if (colorArray[y] == MG_RED)
    colorArray[y] = MG_BLACK;
else
    colorArray[y] = MG_RED;
return colorArray[y];
}

void cactusBedMethods(struct track *tg)
/* cactus bed track methods */
{
tg->freeItems = linkedFeaturesFreeItems;
tg->drawItems = cactusDraw;
tg->drawItemAt = cactusDrawAt;
tg->mapItemName = linkedFeaturesName;
tg->totalHeight = cactusHeight;
tg->itemHeight = cactusItemHeight;
tg->itemStart = linkedFeaturesItemStart;
tg->itemEnd = linkedFeaturesItemEnd;
tg->itemNameColor = linkedFeaturesNameColor;
tg->nextPrevExon = linkedFeaturesNextPrevItem;
tg->nextPrevItem = linkedFeaturesLabelNextPrevItem;
tg->loadItems = loadGappedBed;
//tg->itemName = cactusName;
tg->itemName = linkedFeaturesName;
//tg->mapItemName = refGeneMapName;
tg->itemColor = cactusColor;
tg->itemNameColor = cactusNameColor;
tg->drawLeftLabels = cactusLeftLabels;
}

void blastMethods(struct track *tg)
/* blast protein track methods */
{
tg->loadItems = loadBlast;
tg->itemName = refGeneName;
tg->mapItemName = refGeneMapName;
tg->itemColor = blastColor;
tg->itemNameColor = blastNameColor;
}


static void drawTri(struct hvGfx *hvg, int x, int y1, int y2, Color color,
	char strand)
/* Draw traingle. */
{
struct gfxPoly *poly = gfxPolyNew();
int half = (y2 - y1) / 2;
if (strand == '-')
    {
    gfxPolyAddPoint(poly, x, y1+half);
    gfxPolyAddPoint(poly, x+half, y1);
    gfxPolyAddPoint(poly, x+half, y2);
    }
else
    {
    gfxPolyAddPoint(poly, x, y1);
    gfxPolyAddPoint(poly, x+half, y1+half);
    gfxPolyAddPoint(poly, x, y2);
    }
hvGfxDrawPoly(hvg, poly, color, TRUE);
gfxPolyFree(&poly);
}

static void triangleDrawAt(struct track *tg, void *item,
                           struct hvGfx *hvg, int xOff, int y, double scale,
                           MgFont *font, Color color, enum trackVisibility vis)
/* Draw a right- or left-pointing triangle at position.
 * If item has width > 1 or block/cds structure, those will be ignored --
 * this only draws a triangle (direction depending on strand). */
{
struct bed *bed = item;
int x1 = round((double)((int)bed->chromStart-winStart)*scale) + xOff;
int y2 = y + tg->heightPer-1;
struct trackDb *tdb = tg->tdb;
int scoreMin = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingClosestToHomeOrDefault(tdb, "scoreMax", "1000"));

if (tg->itemColor != NULL)
    color = tg->itemColor(tg, bed, hvg);
else
    {
    if (tg->colorShades)
	color = tg->colorShades[grayInRange(bed->score, scoreMin, scoreMax)];
    }

drawTri(hvg, x1, y, y2, color, bed->strand[0]);
}

void simpleBedTriangleMethods(struct track *tg)
/* Load up simple bed features methods, but use triangleDrawAt. */
{
bedMethods(tg);
tg->drawItemAt = triangleDrawAt;
}

void loadColoredExonBed(struct track *tg)
/* Load the items into a linkedFeaturesSeries. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
struct bed *bed;
struct linkedFeaturesSeries *lfsList = NULL, *lfs;
/* Use tg->tdb->track because subtracks inherit composite track's tdb
 * by default, and the variable is named after the composite track. */
char *scoreFilterClause = getScoreFilterClause(cart, tg->tdb,NULL);
if (scoreFilterClause != NULL)
    {
    sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd,scoreFilterClause, &rowOffset);
    freeMem(scoreFilterClause);
    }
else
    {
    sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
    }
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, 14);
    lfs = lfsFromColoredExonBed(bed);
    slAddHead(&lfsList, lfs);
    bedFree(&bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfsList);
slSort(&lfsList, linkedFeaturesSeriesCmp);
tg->items = lfsList;
}

void coloredExonMethods(struct track *tg)
/* For BED 14 type "coloredExon" tracks. */
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadColoredExonBed;
tg->canPack = TRUE;
}

static boolean abbrevKiddEichlerType(char *name)
/* If name starts with a recognized structural variation type, replace the
 * type with an abbreviation and return TRUE. */
{
if (startsWith("transchr", name) || startsWith("inversion", name) ||
    startsWith("insertion", name) || startsWith("deletion", name) ||
    startsWith("OEA", name))
    {
    struct subText *subList = NULL;
    slSafeAddHead(&subList, subTextNew("deletion", "del"));
    slSafeAddHead(&subList, subTextNew("insertion", "ins"));
    slSafeAddHead(&subList, subTextNew("_random", "_r"));
    slSafeAddHead(&subList, subTextNew("inversion", "inv"));
    slSafeAddHead(&subList, subTextNew("inversions", "inv"));
    slSafeAddHead(&subList, subTextNew("transchrm_chr", "transchr_"));
    /* Using name as in and out here which is OK because all substitutions
     * make the output smaller than the input. */
    subTextStatic(subList, name, name, strlen(name)+1);
    /* We don't free in hgTracks for speed: subTextFreeList(&subList); */
    return TRUE;
    }
return FALSE;
}

char *kiddEichlerItemName(struct track *tg, void *item)
/* If item starts with a long clone name, get rid of it.  Abbreviate the
 * type. */
{
struct linkedFeatures *lf = (struct linkedFeatures *)item;
char *name = cloneString(lf->name);
if (abbrevKiddEichlerType(name))
    return name;
char *comma = strrchr(name, ',');
if (comma != NULL)
    strcpy(name, comma+1);
abbrevKiddEichlerType(name);
return name;
}

void kiddEichlerMethods(struct track *tg)
/* For variation data from Kidd,...,Eichler '08. */
{
linkedFeaturesMethods(tg);
tg->itemName = kiddEichlerItemName;
}

boolean dgvFilter(struct track *tg, void *item)
/* For use with filterItems -- return TRUE if item's PubMed ID is in the list
 * specified by the user. */
{
static struct slName *filterPmIds = NULL;
struct linkedFeatures *lf = item;
struct sqlConnection *conn = hAllocConn(database);
char query[512];
sqlSafef(query, sizeof(query), "select pubmedId from %s where name = '%s'",
      tg->tdb->table, lf->name);
char buf[32];
char *pmId = sqlQuickQuery(conn, query, buf, sizeof(buf));
hFreeConn(&conn);
if (filterPmIds == NULL)
    {
    char cartVarName[256];
    safef (cartVarName, sizeof(cartVarName), "hgt_%s_filterPmId", tg->tdb->track);
    filterPmIds = cartOptionalSlNameList(cart, cartVarName);
    }
return slNameInList(filterPmIds, pmId);
}

void loadDgv(struct track *tg)
/* Load Database of Genomic Variants items, filtering by pubmedId if specified. */
{
loadBed9(tg);
char cartVarName[256];
safef (cartVarName, sizeof(cartVarName), "hgt_%s_filterType", tg->tdb->track);
char *incOrExc = cartUsualString(cart, cartVarName, NULL);
if (isNotEmpty(incOrExc))
    filterItems(tg, dgvFilter, incOrExc);
}

void dgvMethods(struct track *tg)
/* Database of Genomic Variants. */
{
linkedFeaturesMethods(tg);
tg->loadItems = loadDgv;
}

void loadGenePred(struct track *tg)
/* Convert gene pred in window to linked feature. */
{
tg->items = lfFromGenePredInRange(tg, tg->table, chromName, winStart, winEnd);
/* filter items on selected criteria if filter is available */
filterItems(tg, genePredClassFilter, "include");
}

static char *ensGeneName(struct track *tg, void *item)
{
static char cat[128];
struct linkedFeatures *lf = item;
if (lf->extra != NULL)
    {
    safef(cat, sizeof(cat), "%s", (char *)lf->extra);
    return cat;
    }
else
    return lf->name;
}

static void ensGeneAssignConfiguredName(struct track *tg)
/* Set name on genePred in "extra" field to gene symbol, ENSG id, or ENST id,
 * depending, on UI on all items in track */
{
char *geneLabel = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, "label","accession");
boolean otherGeneName =  sameString(geneLabel, "gene symbol");
boolean useGeneName =  sameString(geneLabel, "ENSG* identifier");
boolean useAcc = sameString(geneLabel, "ENST* identifier");
struct sqlConnection *conn = NULL;
if (otherGeneName)
   conn = hAllocConn(database);

struct linkedFeatures *lf;
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    struct dyString *name = dyStringNew(SMALLDYBUF);
    if (otherGeneName)
        {
        char buf[256];
        char query[256];
        sqlSafef(query, sizeof(query),
          "select value from ensemblToGeneName where name = \"%s\"", lf->name);
        char *ret = sqlQuickQuery(conn, query, buf, sizeof(buf));
        if (isNotEmpty(ret))
            dyStringAppend(name, ret);
        else
            dyStringAppend(name, lf->name);
        }
    else if (useGeneName)
        {
        if (isNotEmpty((char*)lf->extra))
            dyStringAppend(name, lf->extra);
        else
            dyStringAppend(name, lf->name);
        }
    else if (useAcc)
        {
        dyStringAppend(name, lf->name);
        }
    else
        lf->extra = NULL;
    if (dyStringLen(name))
        lf->extra = dyStringCannibalize(&name);
    dyStringFree(&name);
    }
if (otherGeneName)
    hFreeConn(&conn);
}

static void loadGenePredEnsGene(struct track *tg)
/* Convert gene pred in window to linked feature. */
{
loadGenePredWithName2(tg);
ensGeneAssignConfiguredName(tg);
tg->itemName = ensGeneName;
}

void genePredAssignConfiguredName(struct track *tg)
/* Set name on genePred in "extra" field to gene name, accession, or both,
 * depending, on UI on all items in track */
{
char *geneLabel = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, "label","gene");
boolean useGeneName =  sameString(geneLabel, "gene")
    || sameString(geneLabel, "name")
    || sameString(geneLabel, "both");
boolean useAcc = sameString(geneLabel, "accession") || sameString(geneLabel, "both");

struct linkedFeatures *lf;
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    struct dyString *name = dyStringNew(SMALLDYBUF);
    if (useGeneName && !isEmpty((char*)lf->extra))
        {
        dyStringAppend(name, lf->extra);
        if (useAcc)
            dyStringAppendC(name, '/');
        }
    if (useAcc)
        dyStringAppend(name, lf->name);
    if (dyStringLen(name))
        lf->extra = dyStringCannibalize(&name);
    dyStringFree(&name);
    }
}

void loadGenePredWithConfiguredName(struct track *tg)
/* Convert gene pred info in window to linked feature. Include name
 * in "extra" field (gene name, accession, or both, depending on UI) */
{
loadGenePredWithName2(tg);
genePredAssignConfiguredName(tg);
}

Color genePredItemAttrColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw a genePred in based on looking it up in a itemAttr
 * table. */
{
struct linkedFeatures *lf = item;
if (lf->itemAttr != NULL)
    return hvGfxFindColorIx(hvg, lf->itemAttr->colorR, lf->itemAttr->colorG, lf->itemAttr->colorB);
else
    return tg->ixColor;
}

Color genePredItemClassColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw a genePred based on looking up the gene class */
/* in an itemClass table. */
{
char *geneClasses = trackDbSetting(tg->tdb, GENEPRED_CLASS_VAR);
char *gClassesClone = NULL;
int class, classCt = 0;
char *classes[20];
char gClass[SMALLBUF];
char *classTable = trackDbSetting(tg->tdb, GENEPRED_CLASS_TBL);
char *nameCol = trackDbSettingOrDefault(tg->tdb, GENEPRED_CLASS_NAME_COLUMN, GENEPRED_CLASS_NAME_COLUMN_DEFAULT);
char *classCol = trackDbSettingOrDefault(tg->tdb, GENEPRED_CLASS_CLASS_COLUMN, GENEPRED_CLASS_CLASS_COLUMN_DEFAULT);
struct linkedFeatures *lf = item;
char *liftDb = cloneString(trackDbSetting(tg->tdb, "quickLiftDb"));
char *db = (liftDb == NULL) ? database : liftDb;
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr;
char **row = NULL;
char query[256];
boolean found = FALSE;
char *colorString = NULL, *colorClone = NULL;
struct rgbColor gClassColor;
int color = tg->ixColor; /* default color in trackDb */
int size = 3;
char *rgbVals[5];
char *sep = ",";

if (geneClasses == NULL)
   errAbort(
      "Track %s missing required trackDb setting: geneClasses", tg->track);
if (geneClasses)
   {
   gClassesClone = cloneString(geneClasses);
   classCt = chopLine(gClassesClone, classes);
   }
if (hTableExists(database, classTable))
    {
    sqlSafef(query, sizeof(query),
          "select %s from %s where %s = \"%s\"", classCol, classTable, nameCol, lf->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        /* scan through groups to find a match */
        for (class = 0; class < classCt; class++)
            {
            if (sameString(classes[class], row[0]))
                /* get color from trackDb settings hash */
                {
                found = TRUE;
                safef(gClass, sizeof(gClass), "%s%s", GENEPRED_CLASS_PREFIX, classes[class]);
                colorString = trackDbSetting(tg->tdb, gClass);
                if (!colorString)
                    found = FALSE;
                break;
                }
            }
        }
    sqlFreeResult(&sr);
    if (found)
        {
        /* need to convert color string to rgb */
        // check how these are found for trackDb
        colorClone = cloneString(colorString);
        chopString(colorClone, sep, rgbVals, size);
        gClassColor.r = (sqlUnsigned(rgbVals[0]));
        gClassColor.g = (sqlUnsigned(rgbVals[1]));
        gClassColor.b = (sqlUnsigned(rgbVals[2]));
        gClassColor.a = 0xff;

        /* find index for color */
        color = hvGfxFindRgb(hvg, &gClassColor);
        }
    }
hFreeConn(&conn);
/* return index for color to draw item */
return color;
}

void drawColorMethods(struct track *tg)
/* Fill in color track items based on chrom  */
{
char *optionStr ;
optionStr = cartUsualStringClosestToHome(cart, tg->tdb,FALSE,"color", "off");
tg->mapItemName = lfMapNameFromExtra;
if( sameString( optionStr, "on" )) /*use chromosome coloring*/
    tg->itemColor = lfChromColor;
else
    tg->itemColor = NULL;
linkedFeaturesMethods(tg);
tg->loadItems = loadGenePred;
}

void loadDless(struct track *tg)
/* Load dless items */
{
struct sqlConnection *conn = hAllocConn(database);
struct dless *dless, *list = NULL;
struct sqlResult *sr;
char **row;
int rowOffset;

sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd,
                 NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    dless = dlessLoad(row+rowOffset);
    slAddHead(&list, dless);
    }
slReverse(&list);
sqlFreeResult(&sr);
hFreeConn(&conn);
tg->items = list;
}

void freeDless(struct track *tg)
/* Free dless items. */
{
dlessFreeList(((struct dless **)(&tg->items)));
}

char *dlessName(struct track *tg, void *item)
/* Get name to use for dless item */
{
struct dless *dl = item;
if (sameString(dl->type, "conserved"))
    return dl->type;
else
    return dl->branch;
}

Color dlessColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color for dless item */
{
struct dless *dl = item;
char *rgb[4];
int count;
char *rgbStr;
static boolean gotColors = FALSE;
static Color consColor, gainColor, lossColor;

if (!gotColors)
{
    consColor = tg->ixColor;
    gainColor = hvGfxFindColorIx(hvg, 0, 255, 0);
    lossColor = hvGfxFindColorIx(hvg, 255, 0, 0);

    if ((rgbStr = trackDbSetting(tg->tdb, "gainColor")) != NULL)
        {
        count = chopString(rgbStr, ",", rgb, ArraySize(rgb));
        if (count == 3 && isdigit(rgb[0][0]) && isdigit(rgb[1][0]) &&
            isdigit(rgb[2][0]))
            gainColor = hvGfxFindColorIx(hvg, atoi(rgb[0]), atoi(rgb[1]), atoi(rgb[2]));
        }

    if ((rgbStr = trackDbSetting(tg->tdb, "lossColor")) != NULL)
        {
        count = chopString(rgbStr, ",", rgb, ArraySize(rgb));
        if (count == 3 && isdigit(rgb[0][0]) && isdigit(rgb[1][0]) &&
            isdigit(rgb[2][0]))
            lossColor = hvGfxFindColorIx(hvg, atoi(rgb[0]), atoi(rgb[1]), atoi(rgb[2]));
        }

    gotColors = TRUE;
}

if (sameString(dl->type, "conserved"))
    return consColor;
else if (sameString(dl->type, "gain"))
    return gainColor;
else
    return lossColor;
}

static void dlessMethods(struct track *tg)
/* Load custom methods for dless track */
{
tg->itemColor = dlessColor;
tg->loadItems = loadDless;
tg->itemName = dlessName;
tg->freeItems = freeDless;
}

char *vegaGeneName(struct track *tg, void *item)
{
static char cat[128];
struct linkedFeatures *lf = item;
if (lf->extra != NULL)
    {
    safef(cat, sizeof(cat), "%s", (char *)lf->extra);
    return cat;
    }
else
    return lf->name;
}

void vegaMethods(struct track *tg)
/* Special handling for vegaGene/vegaPseudoGene items. */
{
tg->loadItems = loadGenePredWithConfiguredName;
tg->itemColor = vegaColor;
tg->itemName = vegaGeneName;
}

Color gvColorByCount(struct track *tg, void *item, struct hvGfx *hvg)
/* color items by whether they are single position or multiple */
{
struct gvPos *el = item;
struct sqlConnection *conn = hAllocConn(database);
char *id = NULL;
char *multColor = NULL, *singleColor = NULL;
int num = 0;
char query[256];
if (el->id != NULL)
    id = el->id;
else
    id = el->name;
sqlSafef(query, sizeof(query), "select count(*) from gvPos where name = '%s'", id);
num = sqlQuickNum(conn, query);
hFreeConn(&conn);
singleColor = cartUsualString(cart, "gvColorCountSingle", "blue");
multColor = cartUsualString(cart, "gvColorCountMult", "green");
if (num == 1)
    {
    if (sameString(singleColor, "red"))
        return hvGfxFindColorIx(hvg, 221, 0, 0); /* dark red */
    else if (sameString(singleColor, "orange"))
        return hvGfxFindColorIx(hvg, 255, 153, 0);
    else if (sameString(singleColor, "green"))
        return hvGfxFindColorIx(hvg, 0, 153, 0); /* dark green */
    else if (sameString(singleColor, "gray"))
        return MG_GRAY;
    else if (sameString(singleColor, "purple"))
        return hvGfxFindColorIx(hvg, 204, 0, 255);
    else if (sameString(singleColor, "blue"))
        return MG_BLUE;
    else if (sameString(singleColor, "brown"))
        return hvGfxFindColorIx(hvg, 100, 50, 0); /* brown */
    else
        return MG_BLACK;
    }
else if (num > 1)
    {
    if (sameString(multColor, "red"))
        return hvGfxFindColorIx(hvg, 221, 0, 0); /* dark red */
    else if (sameString(multColor, "orange"))
        return hvGfxFindColorIx(hvg, 255, 153, 0);
    else if (sameString(multColor, "green"))
        return hvGfxFindColorIx(hvg, 0, 153, 0); /* dark green */
    else if (sameString(multColor, "gray"))
        return MG_GRAY;
    else if (sameString(multColor, "purple"))
        return hvGfxFindColorIx(hvg, 204, 0, 255);
    else if (sameString(multColor, "blue"))
        return MG_BLUE;
    else if (sameString(multColor, "brown"))
        return hvGfxFindColorIx(hvg, 100, 50, 0); /* brown */
    else
        return MG_BLACK;
    }
else
    return MG_BLACK;
}

Color gvColorByDisease(struct track *tg, void *item, struct hvGfx *hvg)
/* color items by whether they are known or likely to cause disease */
{
struct gvPos *el = item;
struct gvAttr *attr = NULL;
struct sqlConnection *conn = hAllocConn(database);
char *id = NULL;
char *useColor = NULL;
int index = -1;
char query[256];
if (el->id != NULL)
    id = el->id;
else
    id = el->name;
sqlSafef(query, sizeof(query), "select * from hgFixed.gvAttr where id = '%s' and attrType = 'disease'", id);
attr = gvAttrLoadByQuery(conn, query);
if (attr == NULL)
    {
    AllocVar(attr);
    attr->attrVal = cloneString("NULL");
    attr->id = NULL; /* so free will work */
    attr->attrType = NULL;
    }
index = stringArrayIx(attr->attrVal, gvColorDAAttrVal, gvColorDASize);
if (index < 0 || index >= gvColorDASize)
    {
    hFreeConn(&conn);
    return MG_BLACK;
    }
useColor = cartUsualString(cart, gvColorDAStrings[index], gvColorDADefault[index]);
gvAttrFreeList(&attr);
hFreeConn(&conn);
if (sameString(useColor, "red"))
    return hvGfxFindColorIx(hvg, 221, 0, 0); /* dark red */
else if (sameString(useColor, "orange"))
    return hvGfxFindColorIx(hvg, 255, 153, 0);
else if (sameString(useColor, "green"))
    return hvGfxFindColorIx(hvg, 0, 153, 0); /* dark green */
else if (sameString(useColor, "gray"))
    return MG_GRAY;
else if (sameString(useColor, "purple"))
    return hvGfxFindColorIx(hvg, 204, 0, 255);
else if (sameString(useColor, "blue"))
    return MG_BLUE;
else if (sameString(useColor, "brown"))
    return hvGfxFindColorIx(hvg, 100, 50, 0); /* brown */
else
    return MG_BLACK;
}

Color gvColorByType(struct track *tg, void *item, struct hvGfx *hvg)
/* color items by type */
{
struct gvPos *el = item;
struct gv *details = NULL;
struct sqlConnection *conn = hAllocConn(database);
char *typeColor = NULL;
int index = 5;
char *id = NULL;
char query[256];
if (el->id != NULL)
    id = el->id;
else
    id = el->name;

sqlSafef(query, sizeof(query), "select * from hgFixed.gv where id = '%s'", id);
details = gvLoadByQuery(conn, query);
index = stringArrayIx(details->baseChangeType, gvColorTypeBaseChangeType, gvColorTypeSize);
if (index < 0 || index >= gvColorTypeSize)
    {
    hFreeConn(&conn);
    return MG_BLACK;
    }
typeColor = cartUsualString(cart, gvColorTypeStrings[index], gvColorTypeDefault[index]);
gvFreeList(&details);
hFreeConn(&conn);
if (sameString(typeColor, "purple"))
    return hvGfxFindColorIx(hvg, 204, 0, 255);
else if (sameString(typeColor, "green"))
    return hvGfxFindColorIx(hvg, 0, 153, 0); /* dark green */
else if (sameString(typeColor, "orange"))
    return hvGfxFindColorIx(hvg, 255, 153, 0);
else if (sameString(typeColor, "blue"))
    return MG_BLUE;
else if (sameString(typeColor, "brown"))
    return hvGfxFindColorIx(hvg, 100, 50, 0); /* brown */
else if (sameString(typeColor, "gray"))
    return MG_GRAY;
else if (sameString(typeColor, "red"))
    return hvGfxFindColorIx(hvg, 221, 0, 0); /* dark red */
else
    return MG_BLACK;
}

Color gvColor(struct track *tg, void *item, struct hvGfx *hvg)
/* color items, multiple choices for determination */
{
char *choice = NULL;
choice = cartOptionalString(cart, "gvPos.filter.colorby");
if (choice != NULL && sameString(choice, "type"))
    return gvColorByType(tg, item, hvg);
else if (choice != NULL && sameString(choice, "count"))
    return gvColorByCount(tg, item, hvg);
else if (choice != NULL && sameString(choice, "disease"))
    return gvColorByDisease(tg, item, hvg);
else
    return gvColorByType(tg, item, hvg);
}

Color oregannoColor(struct track *tg, void *item, struct hvGfx *hvg)
/* color items by type for ORegAnno track */
{
struct oreganno *el = item;
struct oregannoAttr *details = NULL;
char *id = NULL;
Color itemColor = MG_BLACK;
if (el->id != NULL)
    id = el->id;
else
    id = el->name;

details = hashFindVal(tg->attrTable, id);
/* ORegAnno colors 666600 (Dark Green), CCCC66 (Tan), CC0033 (Red),
                   CCFF99 (Background Green)                        */
if (sameString(details->attrVal, "REGULATORY POLYMORPHISM"))
    itemColor = hvGfxFindColorIx(hvg, 204, 0, 51); /* red */
else if (sameString(details->attrVal, "TRANSCRIPTION FACTOR BINDING SITE"))
    itemColor = hvGfxFindColorIx(hvg, 165, 165, 65);  /* tan, darkened some */
else if (sameString(details->attrVal, "REGULATORY REGION"))
    itemColor = hvGfxFindColorIx(hvg, 102, 102, 0);  /* dark green */
/* New ORegAnno colors (colorblind friendly) */
else if (sameString(details->attrVal, "Regulatory Polymorphism"))
    itemColor = hvGfxFindColorIx(hvg, 0, 114, 178); /* Blue */
else if (sameString(details->attrVal, "Transcription Factor Binding Site"))
    itemColor = hvGfxFindColorIx(hvg, 230, 159, 0);  /* Orange */
else if (sameString(details->attrVal, "Regulatory Region"))
    itemColor = hvGfxFindColorIx(hvg, 86, 180, 233);  /* Sky Blue */
else if (sameString(details->attrVal, "Regulatory Haplotype"))
    itemColor = hvGfxFindColorIx(hvg, 213, 94, 0);  /* Vermillion */
else if (sameString(details->attrVal, "miRNA Binding Site"))
    itemColor = hvGfxFindColorIx(hvg, 0, 158, 115);  /* bluish Green */
return itemColor;
}

Color omiciaColor(struct track *tg, void *item, struct hvGfx *hvg)
/* color by confidence score */
{
struct bed *el = item;
if (sameString(tg->table, "omiciaHand"))
    return hvGfxFindColorIx(hvg, 0, 0, 0);
else if (el->score < 200)
    return MG_BLACK;
else if (el->score < 600)
    return hvGfxFindColorIx(hvg, 230, 130, 0); /* orange */
else
    return MG_GREEN;
/*
else if (el->score <= 100)
    return hvGfxFindColorIx(hvg, 0, 10, 0); //10.8
else if (el->score <= 125)
    return hvGfxFindColorIx(hvg, 0, 13, 0); //13.5
else if (el->score <= 150)
    return hvGfxFindColorIx(hvg, 0, 16, 0); //16.2
else if (el->score <= 250)
    return hvGfxFindColorIx(hvg, 0, 27, 0); //27
else if (el->score <= 400)
    return hvGfxFindColorIx(hvg, 0, 43, 0); //43.2
else if (el->score <= 550)
    return hvGfxFindColorIx(hvg, 0, 59, 0); //59.4
else if (el->score <= 700)
    return hvGfxFindColorIx(hvg, 0, 75, 0); //75.6
else if (el->score <= 850)
    return hvGfxFindColorIx(hvg, 0, 91, 0); //91.8
else if (el->score <= 1000)
    return hvGfxFindColorIx(hvg, 0, 108, 0);
else if (el->score <= 1300)
    return hvGfxFindColorIx(hvg, 0, 140, 0); //140.4
else if (el->score <= 1450)
    return hvGfxFindColorIx(hvg, 0, 156, 0); //156.6
else if (el->score <= 1600)
    return hvGfxFindColorIx(hvg, 0, 172, 0); //172.8
else if (el->score <= 2050)
    return hvGfxFindColorIx(hvg, 0, 221, 0); //221.4
else
    return hvGfxFindColorIx(hvg, 0, 253, 0); //253.8
*/
}

struct linkedFeatures *bedDetailLoadAsLf (char **row, int rowOffset, int fieldCount, boolean useItemRgb)
/* load as a linked features track and use default displays */
{
struct bed *bedPart = bedLoadN(row+rowOffset, fieldCount);
struct linkedFeatures *lf;
if (fieldCount < 12)
    bed8To12(bedPart);
lf = lfFromBed(bedPart);
if (useItemRgb)
    {
    lf->useItemRgb = TRUE;
    lf->filterColor=bedPart->itemRgb;
    }
return lf;
}

struct bed *bedDetailLoadAsBed (char **row, int rowOffset, int fieldCount)
/* load as a bed track and use default displays */
{
return bedLoadN(row+rowOffset, fieldCount);
}

void loadBedDetailSimple(struct track *tg)
/* Load bedDetails track as bed 4-7, other fields are for hgc clicks */
{
struct bed *list = NULL, *el;
struct sqlResult *sr;
char **row;
int rowOffset = 0;
struct customTrack *ct = tg->customPt;
struct sqlConnection *conn;
char *table = tg->table;
int bedSize = tg->bedSize; /* count of fields in bed part */

if (ct == NULL)
    conn = hAllocConn(database);
else
    {
    conn = hAllocConn(CUSTOM_TRASH);
    table = ct->dbTableName;
    tg->bedSize = ct->fieldCount - 2; /* field count of bed part */
    bedSize = tg->bedSize;
    }
sr = hRangeQuery(conn, table, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = bedDetailLoadAsBed(row, rowOffset, bedSize);
    slAddHead(&list, el);
    }
slReverse(&list);
sqlFreeResult(&sr);
hFreeConn(&conn);
tg->items = list;
}

void loadBedDetail(struct track *tg)
/* Load bedDetails type track as linked features, other fields are for hgc clicks */
{
struct linkedFeatures *list = NULL, *el;
struct sqlResult *sr;
char **row;
int rowOffset = 0;
struct sqlConnection *conn; // = hAllocConn(database);
char *table = tg->table;
int bedSize = tg->bedSize; /* field count of bed part */
boolean useItemRgb = bedItemRgb(tg->tdb);
struct customTrack *ct = tg->customPt;

if (ct == NULL)
    conn = hAllocConn(database);
else
    {
    conn = hAllocConn(CUSTOM_TRASH);
    table = ct->dbTableName;
    bedSize = ct->fieldCount - 2;
    useItemRgb = bedItemRgb(ct->tdb);
    }
sr = hRangeQuery(conn, table, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = bedDetailLoadAsLf(row, rowOffset, bedSize, useItemRgb);
    slAddHead(&list, el);
    }
slReverse(&list);
sqlFreeResult(&sr);
hFreeConn(&conn);
tg->items = list;
}

void loadProtVar(struct track *tg)
/* Load UniProt Variants with labels */
{
struct protVarPos *list = NULL, *el;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;

sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = protVarPosLoad(row);
    slAddHead(&list, el);
    }
slReverse(&list);
sqlFreeResult(&sr);
tg->items = list;
hFreeConn(&conn);
}

char *protVarName(struct track *tg, void *item)
/* Get name to use for gv item. */
{
struct protVarPos *el = item;
return el->label;
}

char *protVarMapName (struct track *tg, void *item)
/* return id for item */
{
struct protVarPos *el = item;
return el->name;
}


char *gvName(struct track *tg, void *item)
/* Get name to use for gv item. */
{
struct gvPos *el = item;
return el->name;
}

char *gvPosMapName (struct track *tg, void *item)
/* return id for item */
{
struct gvPos *el = item;
return el->id;
}

void gvMethods (struct track *tg)
/* Simple exclude/include filtering on human mutation items and color. */
{
tg->loadItems = loadGV;
tg->itemColor = gvColor;
tg->itemNameColor = gvColor;
tg->itemName = gvName;
tg->mapItemName = gvPosMapName;
tg->nextItemButtonable = TRUE;
tg->nextPrevItem = linkedFeaturesLabelNextPrevItem;
}

char *pgSnpCtMapItemName(struct track *tg, void *item)
/* Return composite item name for pgSnp custom tracks. */
{
struct pgSnp *myItem = item;
char *itemName = myItem->name;
static char buf[256];
if (strlen(itemName) > 0)
  safef(buf, sizeof(buf), "%s %s", ctFileName, itemName);
else
  safef(buf, sizeof(buf), "%s NoItemName", ctFileName);
return buf;
}

void bedDetailCtMethods (struct track *tg, struct customTrack *ct)
/* Load bed detail track from custom tracks as bed 4-12 */
{
if (ct != NULL && ct->fieldCount >= (9+2)) /* at least bed9 */
    {
    linkedFeaturesMethods(tg);
    tg->loadItems = loadBedDetail;
    }
else  /* when in doubt set up as simple bed */
    {
    bedMethods(tg);
    tg->loadItems = loadBedDetailSimple;
    }
tg->nextItemButtonable = TRUE;
tg->customPt = ct;
tg->canPack = TRUE;
}

void bedMethylCtMethods (struct track *tg)
/* Load pgSnp track from custom tracks */
{
bedMethylMethods(tg);
tg->canPack = TRUE;
}

void pgSnpCtMethods (struct track *tg)
/* Load pgSnp track from custom tracks */
{
/* start with the pgSnp methods */
pgSnpMethods(tg);
tg->mapItemName = pgSnpCtMapItemName;
tg->canPack = TRUE;
}

void bedDetailMethods (struct track *tg)
/* Load bed detail track as bed 4-12 */
{
struct trackDb *tdb = tg->tdb;
char *words[3];
int size, cnt = chopLine(cloneString(tdb->type), words);
if (cnt > 1)
    size = atoi(words[1]) - 2;
else
    size = 4;
tg->bedSize = size;
if (size >= 9) /* at least bed9 */
    {
    linkedFeaturesMethods(tg);
    tg->loadItems = loadBedDetail;
    }
else  /* when in doubt set up as simple bed */
    {
    bedMethods(tg);
    tg->loadItems = loadBedDetailSimple;
    }
tg->nextItemButtonable = TRUE;
tg->canPack = TRUE;
}

void protVarMethods (struct track *tg)
/* name vs id, next items */
{
tg->loadItems = loadProtVar;
tg->itemName = protVarName;
tg->mapItemName = protVarMapName;
tg->nextItemButtonable = TRUE;
tg->nextPrevItem = linkedFeaturesLabelNextPrevItem;
}

void oregannoMethods (struct track *tg)
/* load so can allow filtering on type */
{
tg->attrTable = NULL;
tg->loadItems = loadOreganno;
tg->itemColor = oregannoColor;
tg->itemNameColor = oregannoColor;
tg->nextItemButtonable = TRUE;
tg->nextPrevItem = linkedFeaturesLabelNextPrevItem;
}


static char *omimGetInheritanceCode(char *inhMode)
/* Translate inheritance mode strings into much shorter codes. */
{
static struct dyString *dy = NULL;  // re-use this string
if (dy == NULL)
    dy = dyStringNew(0);
dyStringClear(dy);

struct slName *modes = slNameListFromString(inhMode, ',');

for(; modes; modes = modes->next)
    {
    stripChar(modes->name, '?');
    char *mode = skipLeadingSpaces(modes->name);
    if (sameString(mode, "Autosomal dominant"))
        dyStringAppend(dy, "AD");
    else if (sameString(mode, "Autosomal recessive"))
        dyStringAppend(dy, "AR");
    else if (sameString(mode, "X-linked"))
        dyStringAppend(dy, "XL");
    else if (sameString(mode, "X-linked dominant"))
        dyStringAppend(dy, "XLD");
    else if (sameString(mode, "X-linked recessive"))
        dyStringAppend(dy, "XLR");
    else if (sameString(mode, "Y-linked"))
        dyStringAppend(dy, "YL");
    else if (!isEmpty(mode))
        dyStringAppend(dy, mode);

    if (modes->next)
        dyStringAppend(dy, "/");
    }
return dy->string;
}

static char *omimGene2DisorderList(char *db, char *name)
/* Return list of disorders associated with a OMIM entry.  Do not free result! */
{
static struct dyString *dy = NULL;
struct sqlConnection *conn;
char query[4096];

if (dy == NULL)
    dy = dyStringNew(0);
dyStringClear(dy);

// get gene symbol(s) first
conn = hAllocConn(database);
sqlSafef(query,sizeof(query),
        "select geneSymbol from omimGeneMap2 where omimId =%s", name);
char buf[4096];
char *ret = sqlQuickQuery(conn, query, buf, sizeof(buf));
if (isNotEmpty(ret))
    {
    struct slName *genes = slNameListFromString(ret, ',');
    dyStringPrintf(dy, "Gene: %s", genes->name);
    genes = genes->next;
    if (genes)
        {
        if (slCount(genes) > 1)
            dyStringPrintf(dy, ", Synonyms: ");
        else
            dyStringPrintf(dy, ", Synonym: ");
        for(; genes; genes = genes->next)
            {
            dyStringPrintf(dy, "%s", genes->name);
            if (genes->next)
                dyStringPrintf(dy, ",");
        }
    }

    // now phenotype information
    sqlSafef(query,sizeof(query),
            "select GROUP_CONCAT(omimPhenotype.description, '|',inhMode  , '|',omimPhenoMapKey SEPARATOR '$') from omimGene2, omimGeneMap, omimPhenotype where omimGene2.name=omimGeneMap.omimId and omimGene2.name=omimPhenotype.omimId and omimGene2.name =%s and omimGene2.chrom=\'%s\'", name, chromName);
    ret = sqlQuickQuery(conn, query, buf, sizeof(buf));

    if (ret)
        {
        struct slName *phenotypes = slNameListFromString(ret, '$');
        if (slCount(phenotypes))
            {
            if (slCount(phenotypes) > 1)
                dyStringPrintf(dy, ", Phenotypes: ");
            else
                dyStringPrintf(dy, ", Phenotype: ");

            for(; phenotypes; phenotypes = phenotypes->next)
                {
                struct slName *components = slNameListFromString(phenotypes->name, '|');
                dyStringPrintf(dy, "%s", components->name);
                components = components->next;

                char *inhCode = omimGetInheritanceCode(components->name);
                if (!isEmpty(inhCode))
                    dyStringPrintf(dy, ", %s", inhCode);
                components = components->next;

                if (!isEmpty(components->name))
                    dyStringPrintf(dy, ", %s", components->name);

                if (phenotypes->next)
                    dyStringPrintf(dy, "; ");
                }
            }
        }
    }

hFreeConn(&conn);
return(dy->string);
}

#include "omim.h"

boolean isOmimOtherClass(char *omimId)
/* check if this omimId belongs to the "Others" phenotype class */

/* NOTE: The definition of Others class is kind of tricky.

   The Other class is defined as:

	1. does not have class 1 or 2 or 3, or 4; some may have class '-1'.
	2. for an entry of omimId that the omimPhenotype table does not even have a row with omimId
*/
{
boolean result;
char answer[255];
struct sqlConnection *conn = hAllocConn(database);
char query[256];
sqlSafef(query,sizeof(query),
      "select %s from omimPhenotype where omimId =%s and (%s=1 or %s=2 or %s=3 or %s=4)",
      omimPhenotypeClassColName, omimId, omimPhenotypeClassColName, omimPhenotypeClassColName,
      omimPhenotypeClassColName, omimPhenotypeClassColName);
char *ret = sqlQuickQuery(conn, query, answer, sizeof(answer));

if (ret == NULL)
    {
    result = TRUE;
    }
else
    {
    result = FALSE;
    }
hFreeConn(&conn);
return(result);
}

int hasOmimPhenotypeClass(char *omimId, int targetClass)
/* Look up phenotypeClass for omimId, for filtering items.  Don't free result! */
{
int result;
char answer[255];
struct sqlConnection *conn = hAllocConn(database);
char query[256];
sqlSafef(query,sizeof(query),
      "select %s from omimPhenotype where omimId =%s and %s=%d", omimPhenotypeClassColName,omimId,omimPhenotypeClassColName,targetClass);

char *ret = sqlQuickQuery(conn, query, answer, sizeof(answer));

if (ret == NULL)
    {
    if (targetClass == -1)
        {
	result = -1;
	}
    else
        {
        result = 0;
	}
    }
else
    result = targetClass;
hFreeConn(&conn);
return(result);
}

boolean doThisOmimEntry(struct track *tg, char *omimId)
/* check if the specific class of this OMIM entry is selected by the user */
{
boolean doIt;
boolean gotClassLabel;
char labelName[255];
boolean doClass1 = TRUE;
boolean doClass2 = TRUE;
boolean doClass3 = TRUE;
boolean doClass4 = TRUE;
boolean doOthers = TRUE;

struct hashEl *omimLocationLabels;
struct hashEl *label;

safef(labelName, sizeof(labelName), "%s.label", tg->table);
omimLocationLabels = cartFindPrefix(cart, labelName);

gotClassLabel = FALSE;
for (label = omimLocationLabels; label != NULL; label = label->next)
	{
	if (strstr(label->name, "class") != NULL) gotClassLabel = TRUE;
	}
/* if user has not made selection(s) from the phenotype class filter, enable every item */
if (!gotClassLabel) return(TRUE);

/* check which classes have been selected */
for (label = omimLocationLabels; label != NULL; label = label->next)
    {
    if (endsWith(label->name, "class1") && sameString(label->val, "0"))
	{
	doClass1 = FALSE;
	}
    if (endsWith(label->name, "class2") && sameString(label->val, "0"))
	{
	doClass2 = FALSE;
	}
    if (endsWith(label->name, "class3") && sameString(label->val, "0"))
	{
	doClass3 = FALSE;
	}
    if (endsWith(label->name, "class4") && sameString(label->val, "0"))
	{
	doClass4 = FALSE;
	}
    if (endsWith(label->name, "others") && sameString(label->val, "0"))
	{
	doOthers = FALSE;
	}
    }

doIt = FALSE;

/* process regular class 1-4 first */
doIt = doIt || (doClass1 && (hasOmimPhenotypeClass(omimId, 1) == 1));
doIt = doIt || (doClass2 && (hasOmimPhenotypeClass(omimId, 2) == 2));
doIt = doIt || (doClass3 && (hasOmimPhenotypeClass(omimId, 3) == 3));
doIt = doIt || (doClass4 && (hasOmimPhenotypeClass(omimId, 4) == 4));

// if this is a regular (1-4) class and the result is to do it, return TRUE now
if (doIt) return(doIt);

/* process the tricky "Other" class here */
doIt = doOthers && isOmimOtherClass(omimId);

return(doIt);
}

static void omimFilter(struct track *tg)
/* Filter the already-loaded items in the omimGene2 or the omimLocation track */
{
struct bed *bed, *nextBed = NULL, *list = NULL;
for (bed = tg->items;  bed != NULL;  bed = nextBed)
    {
    nextBed = bed->next;
    /* check if user has selected the specific class for this OMIM entry */
    if (doThisOmimEntry(tg, bed->name))
	slAddHead(&list, bed);
    }
slReverse(&list);
tg->items = list;
}

static void omimGene2Load(struct track *tg)
/* Load and filter omimGene2 items, storing long label from omimGene2DisorderList. */
{
bedPlusLabelLoad(tg, omimGene2DisorderList);
omimFilter(tg);
}

Color omimGene2Color(struct track *tg, void *item, struct hvGfx *hvg)
/* set the color for omimLocation track items */
{
struct bed *el = item;
char *phenClass;
char query[256];
struct sqlResult *sr;
char **row;

struct rgbColor *normal = &(tg->color);
struct rgbColor lighter;
struct rgbColor lightest;

Color class1Clr, class2Clr, class3Clr, class4Clr, classOtherClr;

/* color scheme:

    Lighter Green:
        for Class 1 OMIM records
    Light Green:
        for Class 2 OMIM records
    Dark Green:
        for Class 3 OMIM records
    Purple:
        for Class 4 OMIM records
    Light Gray:
        for Others
*/

lighter.r = (6*normal->r + 4*255) / 10;
lighter.g = (6*normal->g + 4*255) / 10;
lighter.b = (6*normal->b + 4*255) / 10;
lighter.a = normal->a;

lightest.r = (1*normal->r + 2*255) / 3;
lightest.g = (1*normal->g + 2*255) / 3;
lightest.b = (1*normal->b + 2*255) / 3;
lightest.a = normal->a;


struct sqlConnection *conn = hAllocConn(database);

class1Clr = hvGfxFindColorIx(hvg, lightest.r, lightest.g, lightest.b);
class2Clr = hvGfxFindColorIx(hvg, lighter.r, lighter.g, lighter.b);
class3Clr = hvGfxFindColorIx(hvg, normal->r, normal->g, normal->b);
class4Clr = hvGfxFindColorIx(hvg, 105,50,155);
classOtherClr = hvGfxFindColorIx(hvg, 190, 190, 190);   // light gray

sqlSafef(query, sizeof(query),
      "select omimId, %s from omimPhenotype where omimId=%s order by %s desc",
      omimPhenotypeClassColName, el->name, omimPhenotypeClassColName);

sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);

hFreeConn(&conn);

if (row == NULL)
    {
    // set to gray if this entry does not have any disorder info
    sqlFreeResult(&sr);
    return classOtherClr;
    }
else
    {
    phenClass = row[1];

    if (sameWord(phenClass, "3"))
        {
	// set to dark green, the same color as omimGene2 track
	sqlFreeResult(&sr);
	return class3Clr;
        }
    else if (sameWord(phenClass, "2"))
        {
	// set to light green for class 2
	sqlFreeResult(&sr);
	return class2Clr;
        }
    else if (sameWord(phenClass, "1"))
        {
	// set to lighter green for class 1
	sqlFreeResult(&sr);
	return class1Clr;
        }
    else if (sameWord(phenClass, "4"))
	{
	// set to the color for phenClass 4
        sqlFreeResult(&sr);
        return class4Clr;
        }
    else
	{
	// set to the color for Others
        sqlFreeResult(&sr);
        return classOtherClr;
	}
    }
}

char * omimGene2Name(struct track *tg, void *item)
/* Returns a combination of OMIM ID and/or gene symbol(s) depending on user's selection */
{
struct bed *el = item;

struct sqlConnection *conn = hAllocConn(database);
boolean labelStarted = FALSE;
boolean useGeneSymbol = FALSE;
boolean useOmimId =  FALSE;
char *geneSymbol;
char *omimId;
struct hashEl *omimGene2Labels = cartFindPrefix(cart, "omimGene2.label");
struct hashEl *label;

if (omimGene2Labels == NULL)
    {
    useOmimId = TRUE; /* default to omim ID*/
    /* set cart to match the default set */
    cartSetBoolean(cart, "omimGene2.label.omimId", TRUE);
    }
else
    {
    for (label = omimGene2Labels; label != NULL; label = label->next)
        {
        if (endsWith(label->name, "gene") && differentString(label->val, "0"))
	    {
	    useGeneSymbol = TRUE;
	    }
        else if (endsWith(label->name, "omimId") && differentString(label->val, "0"))
            {
            useOmimId = TRUE;
            }
        }
    }

struct dyString *name = dyStringNew(SMALLDYBUF);
labelStarted = FALSE; /* reset for each item in track */

if (useOmimId)
    {
    omimId = strdup(el->name);
    if (omimId != NULL)
        {
        dyStringAppend(name, omimId);
        }
    labelStarted = TRUE;
    }

if (useGeneSymbol)
    {
    if (labelStarted)
        dyStringAppendC(name, '/');
    else
        labelStarted = TRUE;
    // get appoved gene symbol from omim2gene table first, if not available then get it from omimGeneMap2 table.
    char query[256];
    sqlSafef(query, sizeof(query), "select approvedGeneSymbol from omim2gene where omimId = %s", el->name);
    geneSymbol = sqlQuickString(conn, query);
    if (geneSymbol && differentString(geneSymbol, "-"))
        dyStringAppend(name, geneSymbol);
    else
        {
        char *chp;
        sqlSafef(query, sizeof(query), "select geneSymbol from omimGeneMap2 where omimId = %s", el->name);
        geneSymbol = sqlQuickString(conn, query);
        if (geneSymbol && differentString(geneSymbol, "0"))
            {
	    // pick the first one, if multiple gene symbols exist
            chp = strstr(geneSymbol, ",");
	    if (chp != NULL) *chp = '\0';
	    dyStringAppend(name, geneSymbol);
	    }
	}
    }

hFreeConn(&conn);
return(name->string);
}

static char *cosmicTissueList(char *db, char *name)
/* Return list of tumor tissues associated with a COSMIC entry.  Do not free result! */
{
static struct dyString *dy = NULL;
struct sqlConnection *conn = hAllocConn(database);

if (dy == NULL)
    dy = dyStringNew(0);
dyStringClear(dy);

char query[256];
sqlSafef(query,sizeof(query),
        "select concat(gene_name,' ',mut_syntax_aa) from cosmicRaw where cosmic_mutation_id ='%s'", name);
char buf[256];
char *ret = sqlQuickQuery(conn, query, buf, sizeof(buf));

if (isNotEmpty(ret))
    dyStringAppend(dy, ret);

sqlSafef(query, sizeof(query),
      "select sum(mutated_samples) from cosmicRaw where cosmic_mutation_id='%s'",
      name);
ret = sqlQuickQuery(conn, query, buf, sizeof(buf));
if (isNotEmpty(ret))
    {
    dyStringAppend(dy, " ");
    dyStringAppend(dy, ret);
    }

sqlSafef(query, sizeof(query),
      "select sum(examined_samples) from cosmicRaw where cosmic_mutation_id='%s'",
      name);
ret = sqlQuickQuery(conn, query, buf, sizeof(buf));
    {
    dyStringAppend(dy, "/");
    dyStringAppend(dy, ret);
    }

sqlSafef(query, sizeof(query),
      "select sum(mutated_samples)*100/sum(examined_samples) from cosmicRaw where cosmic_mutation_id='%s'",
      name);
ret = sqlQuickQuery(conn, query, buf, sizeof(buf));

char *chp = strstr(ret, ".");

if (isNotEmpty(ret))
    {
    // cut off digits after .xxx
    if ((chp != NULL) && (strlen(chp) > 3))
        {
        chp++;
        chp++;
        chp++;
        chp++;
        *chp = '\0';
        }
    dyStringAppend(dy, " (");
    dyStringAppend(dy, ret);
    dyStringAppend(dy, "\%)");
    }

sqlSafef(query,sizeof(query),
        "select tumour_site from cosmicRaw where cosmic_mutation_id ='%s' order by tumour_site", name);
char *disorders = collapseRowsFromQuery(db, query, ",", 4);
if (isNotEmpty(disorders))
    {
    dyStringAppend(dy, " ");
    dyStringAppend(dy, disorders);
    }
hFreeConn(&conn);
return(dy->string);
}

static void cosmicLoad(struct track *tg)
/* Load COSMIC items, storing long label from cosmicTissueList */
{
bedPlusLabelLoad(tg, cosmicTissueList);
}

void cosmicMethods (struct track *tg)
/* Methods for COSMIC track. */
{
tg->loadItems	  = cosmicLoad;
tg->drawItemAt    = bedPlusLabelDrawAt;
tg->mapItem       = bedPlusLabelMapItem;
tg->nextPrevExon  = simpleBedNextPrevEdge;
}

void omimGene2Methods (struct track *tg)
/* Methods for version 2 of OMIM Genes track. */
{
tg->loadItems	  = omimGene2Load;
tg->itemColor     = omimGene2Color;
tg->itemName	  = omimGene2Name;
tg->itemNameColor = omimGene2Color;
tg->drawItemAt    = bedPlusLabelDrawAt;
tg->mapItem       = bedPlusLabelMapItem;
tg->nextPrevExon = simpleBedNextPrevEdge;
}

static char *omimAvSnpAaReplacement(char *db, char *name)
/* Return replacement string associated with a OMIM AV (Allelic Variant) entry */
{
static char omimAvSnpBuffer[256];
struct sqlConnection *conn;
char query[256];
struct sqlResult *sr;
char **row;

omimAvSnpBuffer[0] = '\0';

conn = hAllocConn(db);
sqlSafef(query,sizeof(query),
        "select repl2, dbSnpId, description from omimAv where avId='%s'", name);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);

if (row != NULL)
    {
    safef(omimAvSnpBuffer, sizeof(omimAvSnpBuffer), "%s, %s: %s", row[0], row[1], row[2]);
    }

hFreeConn(&conn);
sqlFreeResult(&sr);
return(omimAvSnpBuffer);
}

static void omimAvSnpLoad(struct track *tg)
/* Load OMIM AV items, storing long label from omimAvSnpAaReplacement. */
{
bedPlusLabelLoad(tg, omimAvSnpAaReplacement);
}

void omimAvSnpMethods (struct track *tg)
/* Methods for OMIM AV (Allelic Variant) SNPs. */
{
tg->loadItems  = omimAvSnpLoad;
tg->drawItemAt = bedPlusLabelDrawAt;
tg->mapItem    = bedPlusLabelMapItem;
tg->nextPrevExon = simpleBedNextPrevEdge;
}


static char *omimLocationDescription(char *db, char *name)
/* Return description of an OMIM entry */
{
static char omimLocationBuffer[512];
struct sqlConnection *conn;
char query[256];

omimLocationBuffer[0] = '\0';

conn = hAllocConn(db);
sqlSafef(query,sizeof(query),
        "select geneName from omimGeneMap2 where omimId=%s", name);
(void)sqlQuickQuery(conn, query, omimLocationBuffer, sizeof(omimLocationBuffer));
hFreeConn(&conn);
return(omimLocationBuffer);
}

Color omimLocationColor(struct track *tg, void *item, struct hvGfx *hvg)
/* set the color for omimLocation track items */
{
struct bed *el = item;
char *phenClass;
char query[256];
struct sqlResult *sr;
char **row;

struct rgbColor *normal = &(tg->color);
struct rgbColor lighter;
struct rgbColor lightest;

Color class1Clr, class2Clr, class3Clr, class4Clr, classOtherClr;

/* color scheme:

    Lighter Green:
        for Class 1 OMIM records
    Light Green:
        for Class 2 OMIM records
    Dark Green:
        for Class 3 OMIM records
    Purple:
        for Class 4 OMIM records
    Light Gray:
        for Others
*/

lighter.r = (6*normal->r + 4*255) / 10;
lighter.g = (6*normal->g + 4*255) / 10;
lighter.b = (6*normal->b + 4*255) / 10;
lighter.a = normal->a;

lightest.r = (1*normal->r + 2*255) / 3;
lightest.g = (1*normal->g + 2*255) / 3;
lightest.b = (1*normal->b + 2*255) / 3;
lightest.a = normal->a;

class1Clr = hvGfxFindColorIx(hvg, lightest.r, lightest.g, lightest.b);
class2Clr = hvGfxFindColorIx(hvg, lighter.r, lighter.g, lighter.b);
class3Clr = hvGfxFindColorIx(hvg, normal->r, normal->g, normal->b);
class4Clr = hvGfxFindColorIx(hvg, 105,50,155);
classOtherClr = hvGfxFindColorIx(hvg, 190, 190, 190);   // light gray

char *liftDb = cloneString(trackDbSetting(tg->tdb, "quickLiftDb"));
char *db = (liftDb == NULL) ? database : liftDb;
struct sqlConnection *conn = hAllocConn(db);

sqlSafef(query, sizeof(query),
      "select omimId, %s from omimPhenotype where omimId=%s", omimPhenotypeClassColName, el->name);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);

hFreeConn(&conn);

if (row == NULL)
    {
    // set to gray if this entry does not have any disorder info
    sqlFreeResult(&sr);
    return classOtherClr;
    }
else
    {
    phenClass = row[1];

    if (sameWord(phenClass, "3"))
        {
	// set to dark green, the same color as omimGene2 track
	sqlFreeResult(&sr);
	return class3Clr;
        }
    else
        {
        if (sameWord(phenClass, "2"))
            {
	    // set to light green for class 2
	    sqlFreeResult(&sr);
            return class2Clr;
            }
	else
	    {
            if (sameWord(phenClass, "1"))
                {
		// set to lighter green for class 1
                sqlFreeResult(&sr);
                return class1Clr;
                }
            else if (sameWord(phenClass, "4"))
                {
                // set to the color for phenClass 4
                sqlFreeResult(&sr);
                return class4Clr;
                }
            else
                {
                // set to the color for Others
                sqlFreeResult(&sr);
                return classOtherClr;
                }
            }
        }
    }
}

static void omimLocationLoad(struct track *tg)
/* Load and filter OMIM Loci items, storing long label from omimLocationDescription. */
{
bedPlusLabelLoad(tg, omimLocationDescription);
omimFilter(tg);
}

void omimLocationMethods (struct track *tg)
/* Methods for OMIM Loci (Non-Gene Entry Cytogenetic Locations). */
{
tg->loadItems     = omimLocationLoad;
tg->itemColor     = omimLocationColor;
tg->itemNameColor = omimLocationColor;
tg->drawItemAt    = bedPlusLabelDrawAt;
tg->mapItem       = bedPlusLabelMapItem;
tg->nextPrevExon  = simpleBedNextPrevEdge;
}

char *omimGeneName(struct track *tg, void *item)
/* set name for omimGene track */
{
struct bed *el = item;
char query[256];
struct sqlConnection *conn = hAllocConn(database);
char *geneLabel = NULL;

char *omimGeneLabel = cartUsualString(cart, "omimGene.label", "OMIM ID");

if (sameWord(omimGeneLabel, "OMIM ID"))
    {
    geneLabel = el->name;
    }
else
    {
    if (sameWord(omimGeneLabel, "UCSC gene symbol"))
        {
        /* get the gene symbol of the exact KG that matches not only ID but also genomic position */
        sqlSafef(query, sizeof(query),
              "select x.geneSymbol from kgXref x, omimToKnownCanonical c, knownGene k, omimGene o"
              " where c.omimId='%s' and c.kgId=x.kgId and k.name=x.kgId and o.name=c.omimId"
              " and o.chrom=k.chrom and k.txStart=%d and k.txEnd=%d",
              el->name, el->chromStart, el->chromEnd);
        geneLabel = sqlQuickString(conn, query);
        }
    else
        {
        sqlSafef(query, sizeof(query),
	"select geneSymbol from omimGeneMap2 where omimId='%s'", el->name);
	geneLabel = sqlQuickString(conn, query);
	}

    if (geneLabel == NULL)
	{
	geneLabel = el->name;
	}
    }
hFreeConn(&conn);
return(cloneString(geneLabel));
}

Color omimGeneColor(struct track *tg, void *item, struct hvGfx *hvg)
/* set the color for omimGene track items */
{
struct bed *el = item;
char *geneSymbols;
char query[256];
struct sqlConnection *conn = hAllocConn(database);

/* set the color to red if the entry is listed in morbidmap */
sqlSafef(query, sizeof(query), "select geneSymbols from omimMorbidMap where omimId=%s", el->name);
geneSymbols = sqlQuickString(conn, query);
hFreeConn(&conn);
if (geneSymbols != NULL)
    {
    return hvGfxFindColorIx(hvg, 255, 0, 0);
    }
else
    {
    return hvGfxFindColorIx(hvg, 0, 0, 200);
    }
}

Color omimGeneColor2(struct track *tg, void *item, struct hvGfx *hvg)
/* set the color for omimGene track items */
{
struct bed *el = item;
char *geneSymbols;
char query[256];
struct sqlConnection *conn = hAllocConn(database);

/* set the color to red if the entry is listed in morbidmap */
sqlSafef(query, sizeof(query), "select geneSymbols from omimMorbidMap where omimId=%s", el->name);
geneSymbols = sqlQuickString(conn, query);
hFreeConn(&conn);
if (geneSymbols != NULL)
    {
    return hvGfxFindColorIx(hvg, 255, 0, 0);
    }
else
    {
    return hvGfxFindColorIx(hvg, 0, 0, 200);
    }
}

static char *omimGeneDiseaseList(char *db, char *name)
/* Return list of diseases associated with a OMIM entry */
{
char query[256];
sqlSafef(query,sizeof(query),
      "select distinct description from omimMorbidMap, omimGene "
      "where name='%s' and name=cast(omimId as char) order by description", name);
return collapseRowsFromQuery(db, query, "; ", 20);
}

static void omimGeneLoad(struct track *tg)
/* Load OMIM Genes, storing long label from omimGeneDiseaseList. */
{
bedPlusLabelLoad(tg, omimGeneDiseaseList);
}

void omimGeneMethods (struct track *tg)
/* Methods for original OMIM Genes track. */
{
tg->loadItems     = omimGeneLoad;
tg->itemColor     = omimGeneColor;
tg->itemNameColor = omimGeneColor;
tg->itemName      = omimGeneName;
tg->drawItemAt    = bedPlusLabelDrawAt;
tg->mapItem       = bedPlusLabelMapItem;
tg->nextPrevExon = simpleBedNextPrevEdge;
}

Color restColor(struct track *tg, void *item, struct hvGfx *hvg)
/* set the color for REST track items */
{
struct bed *el = item;

if (strstr(el->name, "ESC_only"))
    {
    return tg->ixAltColor;
    }
else
    {
    return tg->ixColor;
    }
}

void restMethods (struct track *tg)
{
tg->itemColor     = restColor;
tg->itemNameColor = restColor;
}

void omiciaMethods (struct track *tg)
/* color set by score */
{
tg->itemColor = omiciaColor;
tg->itemNameColor = omiciaColor;
}

static int lfExtraCmp(const void *va, const void *vb)
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
return strcmp(a->extra, b->extra);
}

void loadBed12Source(struct track *tg)
/* Load bed 12 with extra "source" column as lf with extra value.
 * Sort items by source. */
{
struct linkedFeatures *list = NULL;
int scoreMin = atoi(trackDbSettingClosestToHomeOrDefault(tg->tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingClosestToHomeOrDefault(tg->tdb, "scoreMax", "1000"));
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = 0;

sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd,
                 NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct bed12Source *el = bed12SourceLoad(row+rowOffset);
    struct bed *bedPart = (struct bed *)el;
    adjustBedScoreGrayLevel(tg->tdb, bedPart, scoreMin, scoreMax);
    struct linkedFeatures *lf = lfFromBed(bedPart);
    lf->extra = (void *)cloneString(el->source);
    bed12SourceFree(&el);
    slAddHead(&list, lf);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slSort(&list, lfExtraCmp);
tg->items = list;
}

char *jaxAlleleName(struct track *tg, void *item)
/* Strip off initial NM_\d+ from jaxAllele item name. */
{
static char truncName[128];
struct linkedFeatures *lf = item;
/* todo: make this check a cart variable so it can be hgTrackUi-tweaked. */
assert(lf->name != NULL);
if (startsWith("NM_", lf->name) || startsWith("XM_", lf->name))
    {
    char *ptr = lf->name + 3;
    while (isdigit(ptr[0]))
	ptr++;
    if (ptr[0] == '_')
	ptr++;
    safef(truncName, sizeof(truncName), "%s", ptr);
    return truncName;
    }
else
    return lf->name;
}

void jaxAlleleMethods(struct track *tg)
/* Fancy name fetcher for jaxAllele. */
{
linkedFeaturesMethods(tg);
tg->loadItems = loadBed12Source;
tg->itemName = jaxAlleleName;
}

char *jaxPhenotypeName(struct track *tg, void *item)
/* Return name (or source) of jaxPhenotype item. */
{
struct linkedFeatures *lf = item;
/* todo: make this check a cart variable so it can be hgTrackUi-tweaked. */
return (char *)lf->extra;
}

char *jaxPhenotypeMapName(struct track *tg, void *item)
/* Return name and source of jaxPhenotype item for linking to hgc.
 * This gets CGI-encoded, so we can't just plop in another CGI var --
 * hgc will have to parse it out. */
{
struct linkedFeatures *lf = item;
char buf[512];
safef(buf, sizeof(buf), "%s source=%s",
      lf->name, (char *)lf->extra);
return cloneString(buf);
}

void jaxPhenotypeMethods(struct track *tg)
/* Fancy name fetcher for jaxPhenotype. */
{
linkedFeaturesMethods(tg);
tg->loadItems = loadBed12Source;
tg->itemName = jaxPhenotypeName;
tg->mapItemName = jaxPhenotypeMapName;
}

char *igtcName(struct track *tg, void *item)
/* Return name (stripping off the source suffix) of IGTC item. */
{
struct linkedFeatures *lf = (struct linkedFeatures *)item;
char *name = cloneString(lf->name);
char *ptr = strrchr(name, '_');
if (ptr != NULL)
    *ptr = '\0';
/* Some names contain spaces.  IGTC has cgi-encoded names to get them through
 * BLAT and we keep that to get them through hgLoadPsl which splits on
 * whitespace not tabs.  Decode for display: */
cgiDecode(name, name, strlen(name));
return name;
}

Color igtcColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Color IGTC items by source. */
{
struct linkedFeatures *lf = (struct linkedFeatures *)item;
Color color = MG_BLACK;
char *source = strrchr(lf->name, '_');
if (source == NULL)
    return color;
source++;
/* reverse-alphabetical acronym rainbow: */
if (sameString(source, "HVG") || sameString(source, "BG"))
    color = hvGfxFindColorIx(hvg, 0x99, 0x00, 0xcc); /* purple */
else if (sameString(source, "CMHD"))
    color = hvGfxFindColorIx(hvg, 0x00, 0x00, 0xcc); /* dark blue */
else if (sameString(source, "EGTC"))
    color = hvGfxFindColorIx(hvg, 0x66, 0x99, 0xff); /* light blue */
else if (sameString(source, "ESDB"))
    color = hvGfxFindColorIx(hvg, 0x00, 0xcc, 0x00); /* green */
else if (sameString(source, "FHCRC"))
    color = hvGfxFindColorIx(hvg, 0xcc, 0x99, 0x00); /* dark yellow */
else if (sameString(source, "GGTC"))
    color = hvGfxFindColorIx(hvg, 0xff, 0x99, 0x00); /* orange */
else if (sameString(source, "SIGTR"))
    color = hvGfxFindColorIx(hvg, 0x99, 0x66, 0x00); /* brown */
else if (sameString(source, "TIGEM"))
    color = hvGfxFindColorIx(hvg, 0xcc, 0x00, 0x00); /* red */
else if (sameString(source, "TIGM"))
    color = hvGfxFindColorIx(hvg, 0xaa, 0x00, 0x66); /* Juneberry (per D.S.) */
return color;
}

void igtcMethods(struct track *tg)
/* International Gene Trap Consortium: special naming & coloring. */
{
tg->itemName = igtcName;
tg->itemColor = igtcColor;
tg->itemNameColor = igtcColor;
}

void loadVariome(struct track *tg)
/* Load the items from the variome table (based on wikiTrackLoadItems) */
{
struct bed *bed;
struct sqlConnection *conn = wikiConnect();
struct sqlResult *sr;
char **row;
int rowOffset;
char where[256];
struct linkedFeatures *lfList = NULL, *lf;
int scoreMin = 0;
int scoreMax = 99999;

sqlSafef(where, ArraySize(where), "db='%s'", database);

sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, where, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct variome *item = variomeLoad(row);
    AllocVar(bed);
    bed->chrom = cloneString(item->chrom);
    bed->chromStart = item->chromStart;
    bed->chromEnd = item->chromEnd;
    bed->name = cloneString(item->name);
    bed->score = item->score;
    safecpy(bed->strand, sizeof(bed->strand), item->strand);
    bed->thickStart = item->chromStart;
    bed->thickEnd = item->chromEnd;
    bed->itemRgb = bedParseRgb(item->color);
    bed8To12(bed);
    lf = lfFromBedExtra(bed, scoreMin, scoreMax);
    lf->useItemRgb = TRUE;
    lf->filterColor=bed->itemRgb;

    /* overload itemAttr fields to be able to pass id to hgc click box */
    struct itemAttr *id;
    AllocVar(id);
    id->chromStart = item->id;
    lf->itemAttr = id;
    slAddHead(&lfList, lf);
    variomeFree(&item);
    }
sqlFreeResult(&sr);

slSort(&lfList, linkedFeaturesCmp);

// add special item to allow creation of new entries
AllocVar(bed);
bed->chrom = chromName;
bed->chromStart = winStart;
bed->chromEnd = winEnd;
bed->name = cloneString("Make new entry");
bed->score = 100;
bed->strand[0] = ' ';  /* no barbs when strand is unknown */
bed->thickStart = winStart;
bed->thickEnd = winEnd;
bed->itemRgb = 0xcc0000;
bed8To12(bed);
lf = lfFromBedExtra(bed, scoreMin, scoreMax);
lf->useItemRgb = TRUE;
lf->filterColor=bed->itemRgb;
slAddHead(&lfList, lf);

tg->items = lfList;
wikiDisconnect(&conn);
}

void variomeMapItem(struct track *tg, struct hvGfx *hvg, void *item,
        char *itemName, char *mapItemName, int start, int end, int x, int y, int width, int height)
/* create a special map box item with different i=hgcClickName and
 * pop-up statusLine with the item name
 */
{
char *hgcClickName = tg->mapItemName(tg, item);
char *statusLine = tg->itemName(tg, item);
mapBoxHgcOrHgGene(hvg, start, end, x, y, width, height, tg->track,
                          hgcClickName, statusLine, NULL, FALSE, NULL);
}

char *variomeMapItemName(struct track *tg, void *item)
/* Return the unique id track item. */
{
struct itemAttr *ia;
char id[64];
int iid = 0;
struct linkedFeatures *lf = item;
if (lf->itemAttr != NULL)
    {
    ia = lf->itemAttr;
    iid = (int)(ia->chromStart);
    }
safef(id,ArraySize(id),"%d", iid);
return cloneString(id);
}

void addVariomeWikiTrack(struct track **pGroupList)
/* Add variome wiki track and append to group list. */
{
if (wikiTrackEnabled(database, NULL))
    {
    struct track *tg = trackNew();
    static char longLabel[80];
    struct trackDb *tdb;
    struct sqlConnection *wikiConn = wikiConnect();
    if (! sqlTableExists(wikiConn,"variome"))
        errAbort("variome table missing for wiki track");

    linkedFeaturesMethods(tg);
    AllocVar(tdb);
    tg->track = "variome";
    tg->table = "variome";
    tg->canPack = TRUE;
    tg->visibility = tvHide;
    tg->hasUi = TRUE;
    tg->shortLabel = cloneString("New variants");
    safef(longLabel, sizeof(longLabel), "New variant submission for microattribtution review");
    tg->longLabel = longLabel;
    tg->loadItems = loadVariome;
    tg->itemName = linkedFeaturesName;
    tg->mapItemName = variomeMapItemName;
    tg->mapItem = variomeMapItem;
    tg->priority = WIKI_TRACK_PRIORITY;
    tg->defaultPriority = WIKI_TRACK_PRIORITY;
    tg->groupName = cloneString("varRep");
    tg->defaultGroupName = cloneString("varRep");
    tg->exonArrows = FALSE;
    tg->nextItemButtonable = TRUE;
    tdb->track = cloneString(tg->track);
    tdb->table = cloneString(tg->table);
    tdb->shortLabel = cloneString(tg->shortLabel);
    tdb->longLabel = cloneString(tg->longLabel);
    tdb->useScore = 1;
    tdb->grp = cloneString(tg->groupName);
    tdb->priority = tg->priority;
    trackDbPolish(tdb);
    tg->tdb = tdb;

    slAddHead(pGroupList, tg);
    wikiDisconnect(&wikiConn);
    }
}

void variomeMethods (struct track *tg)
/* load variome track (wikiTrack) */
{
tg->loadItems = loadVariome;
tg->mapItemName = variomeMapItemName;
tg->mapItem = variomeMapItem;
}

void logoLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width, int height,
	boolean withCenterLabels, MgFont *font, Color color,
	enum trackVisibility vis)
{
}

/* load data for a sequence logo */
static void logoLoad(struct track *tg)
{
struct dnaMotif *motif;
int count = winEnd - winStart;
int ii;
char query[256];
struct sqlResult *sr;
long long offset = 0;
char *fileName = NULL;
struct sqlConnection *conn;
char **row;
FILE *f;
unsigned short *mem, *p;
boolean complementBases = cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE);

if (!zoomedToBaseLevel)
	return;

conn = hAllocConn(database);
sqlSafef(query, sizeof(query),
	"select `offset`,fileName from %s where chrom = '%s'", tg->table,chromName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    offset = sqlLongLong(row[0]);
    fileName = cloneString(row[1]);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);

if (offset == 0)
    return; /* we should have found a non-zero offset  */

AllocVar(motif);
motif->name = NULL;
motif->columnCount = count;
motif->aProb = needMem(sizeof(float) * motif->columnCount);
motif->cProb = needMem(sizeof(float) * motif->columnCount);
motif->gProb = needMem(sizeof(float) * motif->columnCount);
motif->tProb = needMem(sizeof(float) * motif->columnCount);

/* get data from data file specified in db */
f = mustOpen(fileName, "r");
offset += winStart * 2; /* file has 2 bytes per base */
fseek(f, offset, 0);

p = mem = needMem(count * 2);

/* read in probability data from file */
mustRead(f, mem, sizeof(unsigned short) * count);
fclose(f);

/* translate 5 bits for A,C,and G into real numbers for all bases */
for(ii=0; ii < motif->columnCount; ii++, p++)
    {
    motif->gProb[ii] = *p & 0x1f;
    motif->cProb[ii] = (*p >> 5)  & 0x1f;
    motif->aProb[ii] = (*p >> 10) & 0x1f;
    motif->tProb[ii] = 31 - motif->aProb[ii] - motif->cProb[ii] - motif->gProb[ii];
    }

if (complementBases) /* if bases are on '-' strand */
    {
    float *temp = motif->aProb;

    motif->aProb = motif->tProb;
    motif->tProb = temp;

    temp = motif->cProb;
    motif->cProb = motif->gProb;
    motif->gProb = temp;
    }

tg->items = motif;
}


int logoHeight(struct track *tg, enum trackVisibility vis)
/* set up size of sequence logo */
{
if (tg->items == NULL)
    tg->height = tg->lineHeight;
else
    tg->height = 52;

return tg->height;
}

void logoMethods(struct track *track, struct trackDb *tdb, int argc, char *argv[])
/* Load up logo type methods. */
{
track->loadItems = logoLoad;
track->drawLeftLabels = logoLeftLabels;

track->drawItems = logoDrawSimple;
track->totalHeight = logoHeight;
track->mapsSelf = TRUE;
}
#endif /* GBROWSE */

static void remoteDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
{
hvGfxTextCentered(hvg, xOff, yOff, width, tg->height, MG_BLACK, font, "loading...");
}

static void remoteLoadItems(struct track *tg)
{
tg->items = newSlName("remote");
}

static void remoteFreeItems(struct track *tg)
{
}

char *remoteName(struct track *tg, void *item)
{
return tg->track;
}

void remoteMethods(struct track *tg)
{
tg->freeItems = remoteFreeItems;
tg->loadItems = remoteLoadItems;
tg->drawItems = remoteDrawItems;
tg->itemName = remoteName;
tg->lineHeight = 10;
tg->totalHeight = tgFixedTotalHeightNoOverflow;
tg->itemHeight = tgFixedItemHeight;
tg->itemStart = tgItemNoStart;
tg->itemEnd = tgItemNoEnd;
tg->mapItemName = remoteName;
}

static void drawExampleMessageLine(struct track *tg, int seqStart, int seqEnd, struct hvGfx *hvg,
				   int xOff, int yOff, int width, MgFont *font, Color color,
				   enum trackVisibility vis)
/* Example, meant to be overloaded: draw a message in place of track items. */
{
char message[512];
safef(message, sizeof(message), "drawExampleMessageLine: copy me and put your own message here.");
Color yellow = hvGfxFindRgb(hvg, &undefinedYellowColor);
hvGfxBox(hvg, xOff, yOff, width, tg->heightPer, yellow);
hvGfxTextCentered(hvg, xOff, yOff, width, tg->heightPer, MG_BLACK, font, message);
}

void messageLineMethods(struct track *track)
/* Methods for drawing a single-height message line instead of track items,
 * e.g. if source was compiled without a necessary library. */
{
linkedFeaturesMethods(track);
track->loadItems = dontLoadItems;
track->drawItems = drawExampleMessageLine;
// Following few lines taken from hgTracks.c getTrackList, because this is called earlier
// but needs to know track vis from tdb+cart:
char *s = cartOptionalString(cart, track->track);
if (cgiOptionalString("hideTracks"))
    {
    s = cgiOptionalString(track->track);
    if (s != NULL && (hTvFromString(s) != track->tdb->visibility))
	{
	cartSetString(cart, track->track, s);
	}
    }
// end stuff copied from hgTracks.c
enum trackVisibility trackVis = track->tdb->visibility;
if (s != NULL)
    trackVis = hTvFromString(s);
if (trackVis != tvHide)
    {
    track->visibility = tvDense;
    track->limitedVis = tvDense;
    track->limitedVisSet = TRUE;
    }
track->nextItemButtonable = track->nextExonButtonable = FALSE;
track->nextPrevItem = NULL;
track->nextPrevExon = NULL;
}

static void bigMethylLoadItems(struct track *tg)
/* get items from bigBed in bedMethyl format */
{
loadSimpleBedWithLoader(tg, (bedItemLoader)bedMethylLoad);
}

static void bigMethylMethods(struct track *track)
/* methods for bigBed in bedMethyl format */
{
track->isBigBed = TRUE;
bedMethylMethods(track);
track->loadItems = bigMethylLoadItems;
}

void fillInFromType(struct track *track, struct trackDb *tdb)
/* Fill in various function pointers in track from type field of tdb. */
{
char *typeLine = tdb->type, *words[8], *type;
int wordCount;
if (typeLine == NULL)
    return;
wordCount = chopLine(cloneString(typeLine), words);
if (wordCount <= 0)
    return;
type = words[0];

track->drawItemLabel = genericDrawItemLabel;
track->doItemMapAndArrows = genericItemMapAndArrows;

#ifndef GBROWSE
if (sameWord(type, "bed"))
    {
    char *trackName = trackHubSkipHubName(track->track);

    complexBedMethods(track, tdb, FALSE, wordCount, words);
    /* bed.h includes genePred.h so should be able to use these trackDb
       settings. */
    if (trackDbSetting(track->tdb, GENEPRED_CLASS_TBL) !=NULL)
        track->itemColor = genePredItemClassColor;

    // FIXME: as long as registerTrackHandler doesn't accept wildcards,
    // this probably needs to stay here (it's in the wrong function)
    if (startsWith("pubs", trackName) && stringIn("Marker", trackName))
        pubsMarkerMethods(track);
    if (startsWith("pubs", trackName) && stringIn("Blat", trackName))
        pubsBlatMethods(track);
    if (startsWith("gtexEqtlCluster", trackName))
        gtexEqtlClusterMethods(track);
    if (startsWith("gtexEqtlTissue", trackName))
        gtexEqtlTissueMethods(track);
    }
/*
else if (sameWord(type, "bedLogR"))
    {
    wordCount++;
    words[1] = "9";
    complexBedMethods(track, tdb, FALSE, wordCount, words);
    //track->bedSize = 10;
    }
    */
else if (sameWord(type, "bedTabix"))
    {
    knetUdcInstall();
    tdb->canPack = TRUE;
    complexBedMethods(track, tdb, FALSE, wordCount, words);
    }
else if (sameWord(type, "longTabix"))
    {
    char *words[2];
    words[0] = type;
    words[1] = "5";
    knetUdcInstall();
    complexBedMethods(track, tdb, FALSE, 2, words);
    longRangeMethods(track, tdb);
    }
else if (sameWord(type, "mathWig"))
    {
    mathWigMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "bigBed"))
    {
    bigBedMethods(track, tdb, wordCount, words);
    if (startsWith("gtexEqtlTissue", track->track))
        gtexEqtlTissueMethods(track);
    }
else if (sameWord(type, "bigMaf"))
    {
    tdb->canPack = TRUE;
    wordCount++;
    words[1] = "3";
    wigMafMethods(track, tdb, wordCount, words);
    track->isBigBed = TRUE;
    }
else if (sameWord(type, "bigBarChart"))
    {
    tdb->canPack = TRUE;
    track->isBigBed = TRUE;
    barChartMethods(track);
    }
else if (sameWord(type, "bigRmsk"))
    {
    tdb->canPack = TRUE;
    track->isBigBed = TRUE;
    track->mapsSelf = TRUE;
    bigRmskMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "bigBaseView"))
    {
    track->isBigBed = TRUE;
    bigBaseViewMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "bigLolly"))
    {
    tdb->canPack = TRUE;
    track->isBigBed = TRUE;
    track->mapsSelf = TRUE;
    lollyMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "bigInteract"))
    {
    track->isBigBed = TRUE;
    interactMethods(track);
    }
else if (sameWord(type, "bigMethyl"))
    {
    bigMethylMethods(track);
    tdb->canPack = TRUE;
    }
else if (sameWord(type, "bigNarrowPeak"))
    {
    tdb->canPack = TRUE;
    track->isBigBed = TRUE;
    encodePeakMethods(track);
    track->loadItems = bigNarrowPeakLoadItems;
    }
else if (sameWord(type, "bigPsl"))
    {
    tdb->canPack = TRUE;
    wordCount++;
    words[1] = "12";
    commonBigBedMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "bigChain"))
    {
    tdb->canPack = TRUE;
    wordCount++;
    words[1] = "11";
    track->isBigBed = TRUE;
    chainMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "bigGenePred"))
    {
    tdb->canPack = TRUE;
    wordCount++;
    words[1] = "12";
    commonBigBedMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "bigDbSnp"))
    {
    tdb->canPack = TRUE;
    track->isBigBed = TRUE;
    bigDbSnpMethods(track);
    }
else if (sameWord(type, "bedGraph"))
    {
    bedGraphMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "bigWig"))
    {
    bigWigMethods(track, tdb, wordCount, words);
    }
else
#endif /* GBROWSE */
if (sameWord(type, "wig"))
    {
    wigMethods(track, tdb, wordCount, words);
    }
else if (startsWith("wigMaf", type))
    {
    wigMafMethods(track, tdb, wordCount, words);
    }
#ifndef GBROWSE
else if (sameWord(type, "sample"))
    {
    sampleMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "genePred"))
    {
    linkedFeaturesMethods(track);
    if (startsWith("ensGene", track->track))
        track->loadItems = loadGenePredEnsGene;
    else
        track->loadItems = loadGenePred;
    track->colorShades = NULL;
    if (track->itemAttrTbl != NULL)
        track->itemColor = genePredItemAttrColor;
    else if (trackDbSetting(track->tdb, GENEPRED_CLASS_TBL) !=NULL)
        track->itemColor = genePredItemClassColor;
    }
else if (sameWord(type, "logo"))
    {
    logoMethods(track, tdb, wordCount, words);
    }
#endif /* GBROWSE */
else if (sameWord(type, "psl"))
    {
    pslMethods(track, tdb, wordCount, words);

    // FIXME: registerTrackHandler doesn't accept wildcards, so this might be the only
    // way to get this done in a general way. If this was in loaded with registerTrackHandler
    // pslMethods would need the tdb object, which we don't have for these callbacks
    if (startsWith("pubs", track->track))
        pubsBlatPslMethods(track);
    }
#ifdef NOTNOW
else if (sameWord(type, "snake"))
    {
    snakeMethods(track, tdb, wordCount, words);
    }
#endif
else if (sameWord(type, "chain"))
    {
    chainMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "netAlign"))
    {
    netMethods(track);
    }
else if (sameWord(type, "maf"))
    {
    mafMethods(track);
    }
else if (sameWord(type, "bam"))
    {
    bamMethods(track);
    }
else if (sameWord(type, "hic"))
    {
    tdb->canPack = TRUE;
    hicMethods(track);
    }
#ifdef USE_HAL
else if (sameWord(type, "pslSnake"))
    {
    halSnakeMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "halSnake"))
    {
    halSnakeMethods(track, tdb, wordCount, words);
    }
#endif
else if (sameWord(type, "vcfPhasedTrio"))
    {
    vcfPhasedMethods(track);
    }
else if (sameWord(type, "vcfTabix"))
    {
    vcfTabixMethods(track);
    }
else if (sameWord(type, "vcf"))
    {
    vcfMethods(track);
    }
else if (startsWith(type, "bedDetail"))
    {
    bedDetailMethods(track);
    }
else if (sameWord(type, "pgSnp"))
    {
    pgSnpCtMethods(track);
    }
#ifndef GBROWSE
else if (sameWord(type, "coloredExon"))
    {
    coloredExonMethods(track);
    }
else if (sameWord(type, "axt"))
    {
    if (wordCount < 2)
        errAbort("Expecting 2 words in axt track type for %s", tdb->track);
    axtMethods(track, words[1]);
    }
else if (sameWord(type, "expRatio"))
    {
    expRatioMethodsFromDotRa(track);
    }
else if (sameWord(type, "encodePeak") || sameWord(type, "narrowPeak") ||
	 sameWord(type, "broadPeak") || sameWord(type, "gappedPeak"))
    {
    encodePeakMethods(track);
    }
else if (sameWord(type, "bed5FloatScore") ||
         sameWord(type, "bed5FloatScoreWithFdr"))
    {
    track->bedSize = 5;
    bedMethods(track);
    track->loadItems = loadSimpleBed;
    }
else if (sameWord(type, "bed6FloatScore"))
    {
    track->bedSize = 4;
    bedMethods(track);
    track->loadItems = loadSimpleBed;
    }
else if (sameWord(type, "encodeFiveC"))
    {
    track->bedSize = 3;
    bedMethods(track);
    track->loadItems = loadSimpleBed;
    }
else if (sameWord(type, "peptideMapping"))
    {
    track->bedSize = 6;
    bedMethods(track);
    track->loadItems = loadSimpleBed;
    }
else if (sameWord(type, "chromGraph"))
    {
    chromGraphMethods(track);
    }
else if (sameWord(type, "altGraphX"))
    {
    altGraphXMethods(track);
    }
else if (sameWord(type, "rmsk"))
    {
    repeatMethods(track);
    }
else if (sameWord(type, "ld2"))
    {
    ldMethods(track);
    }
else if (sameWord(type, "factorSource"))
    {
    factorSourceMethods(track);
    }
else if (sameWord(type, "remote"))
    {
    remoteMethods(track);
    }
else if (sameWord(type, "bamWig"))
    {
    bamWigMethods(track, tdb, wordCount, words);
    }
else if (sameWord(type, "gvf"))
    {
    gvfMethods(track);
    }
else if (sameWord(type, "barChart"))
    {
    barChartMethods(track);
    }
else if (sameWord(type, "interact"))
    {
    interactMethods(track);
    }
else if (sameWord(type, "bedMethyl"))
    {
    bedMethylMethods(track);
    }
/* add handlers for wildcard */
if (startsWith("peptideAtlas", track->track))
    peptideAtlasMethods(track);
else if (startsWith("gtexGene", track->track))
    gtexGeneMethods(track);
else if (startsWith("rnaStruct", track->track))
    rnaSecStrMethods(track);
#endif /* GBROWSE */
}

static void compositeLoad(struct track *track)
/* Load all subtracks */
{
struct track *subtrack;
long thisTime = 0, lastTime = 0;
for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (!subtrack->loadItems) // This could happen if track type has no handler (eg, for new types or mnissing a type s)
        errAbort("Error: No loadItems() handler for subtrack (%s) of composite track (%s) (was a type specified for this track?)\n", subtrack->track, track->track);
    if (isSubtrackVisible(subtrack) &&
	( limitedVisFromComposite(subtrack) != tvHide))
	{
	if (!subtrack->parallelLoading)
	    {
	    lastTime = clock1000();
	    checkIfWiggling(cart, subtrack);
	    subtrack->loadItems(subtrack);
	    if (measureTiming)
		{
		thisTime = clock1000();
		subtrack->loadTime = thisTime - lastTime;
		lastTime = thisTime;
		}
	    }
	}
    else
	{
	subtrack->limitedVis = tvHide;
	subtrack->limitedVisSet = TRUE;
	}
    }
}

static int compositeTotalHeight(struct track *track, enum trackVisibility vis)
/* Return total height of composite track and set subtrack->height's. */
{
struct track *subtrack;
int height = 0;
for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack))
        {
        limitVisibility(subtrack);
        enum trackVisibility minVis = vis;
        if (subtrack->limitedVisSet)
            minVis = tvMin(minVis, subtrack->limitedVis);
        int h = subtrack->totalHeight(subtrack, minVis);
        subtrack->height = h;
        height += h;
        }
    }
track->height = height;
return track->height;
}

int trackPriCmp(const void *va, const void *vb)
/* Compare for sort based on priority */
{
const struct track *a = *((struct track **)va);
const struct track *b = *((struct track **)vb);

double diff = a->priority - b->priority;
if (diff > 0)
    return 1;
else if (diff < 0)
    return -1;
return 0;
}


static bool isSubtrackVisibleTdb(struct cart *cart, struct trackDb *tdb)
/* Has this subtrack not been deselected in hgTrackUi or declared with
 *  * "subTrack ... off"?  -- assumes composite track is visible. */
{
boolean overrideComposite = (NULL != cartOptionalString(cart, tdb->track));
bool enabledInTdb = TRUE; // assume that this track is enabled in tdb
char option[1024];
safef(option, sizeof(option), "%s_sel", tdb->track);
boolean enabled = cartUsualBoolean(cart, option, enabledInTdb);
if (overrideComposite)
    enabled = TRUE;
return enabled;
}

void buildMathWig(struct trackDb *tdb)
/* Turn a mathWig composite into a mathWig track. */
{
char *aggregateFunc = cartOrTdbString(cart, tdb, "aggregate" , FALSE);

if ((aggregateFunc == NULL) || !(sameString("add", aggregateFunc) || sameString("subtract", aggregateFunc)))
    return;

struct trackDb *subTracks = tdb->subtracks;

tdb->subtracks = NULL;
tdb->type = "mathWig";

struct dyString *dy = dyStringNew(1024);

if (sameString("add", aggregateFunc))
    dyStringPrintf(dy, "+ ");
else // subtract
    dyStringPrintf(dy, "- ");
struct trackDb *subTdb;
for (subTdb=subTracks; subTdb; subTdb = subTdb->next)
    {
    if (!isSubtrackVisibleTdb(cart, subTdb) )
        continue;

    char *bigDataUrl = trackDbSetting(subTdb, "bigDataUrl");
    char *useDb;
    char *table;
    if (bigDataUrl != NULL)
        dyStringPrintf(dy, "%s ",bigDataUrl);
    else 
        {
        if (isCustomTrack(trackHubSkipHubName(subTdb->track)))
            {
            useDb = CUSTOM_TRASH;
            table = trackDbSetting(subTdb, "dbTableName");
            }
        else
            {
            useDb = database;
            table = trackDbSetting(subTdb, "table");
            }

        if (startsWith("bedGraph", subTdb->type))
            dyStringPrintf(dy, "^%s.%s ",useDb, table);
        else
            dyStringPrintf(dy, "$%s.%s ",useDb, table);
        }
    }

hashAdd(tdb->settingsHash, "mathDataUrl", dy->string);
}

#ifdef NOTNOW   /// for the moment, mathWigs are made at the composite level.  Since we may go back to having them at the view level I'm leaving this in
void fixupMathWigs(struct trackDb *tdb)
/* Look through a container to see if it has a mathWig view and convert it. */
{
struct trackDb *subTdb;

for(subTdb = tdb->subtracks; subTdb; subTdb = subTdb->next)
    {
    char *type;
    if ((type = trackDbSetting(subTdb, "container")) != NULL)
        {
        if (sameString(type, "mathWig"))
            {
            buildMathWig(subTdb);
            }
        }
    }
}
#endif

void makeCompositeTrack(struct track *track, struct trackDb *tdb)
/* Construct track subtrack list from trackDb entry.
 * Sets up color gradient in subtracks if requested */
{
buildMathWig(tdb);
unsigned char finalR = track->color.r, finalG = track->color.g,
                            finalB = track->color.b, finalA = track->color.a;
unsigned char altR = track->altColor.r, altG = track->altColor.g,
                            altB = track->altColor.b, altA = track->altColor.a;
unsigned char deltaR = 0, deltaG = 0, deltaB = 0, deltaA = 0;

struct slRef *tdbRef, *tdbRefList = trackDbListGetRefsToDescendantLeaves(tdb->subtracks);

struct trackDb *subTdb;
int subCount = slCount(tdbRefList);
int altColors = subCount - 1;
struct track *subtrack = NULL;
TrackHandler handler;
boolean smart = FALSE;

/* ignore if no subtracks */
if (!subCount)
    return;

char *compositeTrack = trackDbLocalSetting(tdb, "compositeTrack");
/* look out for tracks that manage their own subtracks */
if (startsWith("wig", tdb->type) || startsWith("bedGraph", tdb->type) ||
    (compositeTrack != NULL && rStringIn("smart", compositeTrack)))
        smart = TRUE;

/* setup function handlers for composite track */
handler = lookupTrackHandlerClosestToHome(tdb);
if (smart && handler != NULL)
    /* handles it's own load and height */
    handler(track);
else
    {
    track->loadItems = compositeLoad;
    track->totalHeight = compositeTotalHeight;
    }

if (altColors && (finalR || finalG || finalB))
    {
    /* not black -- make a color gradient for the subtracks,
                from black, to the specified color */
    deltaR = (finalR - altR) / altColors;
    deltaG = (finalG - altG) / altColors;
    deltaB = (finalB - altB) / altColors;
    // speculative - no harm, but there's no current way for a track to set its alpha,
    // so both final and altA should be 255
    deltaA = (finalA - altA) / altColors;
    }

/* fill in subtracks of composite track */
for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
    {
    subTdb = tdbRef->val;

    subtrack = trackFromTrackDb(subTdb);
    boolean avoidHandler = trackDbSettingOn(tdb, "avoidHandler");
    if (!avoidHandler && ( handler = lookupTrackHandlerClosestToHome(subTdb)) != NULL)
        handler(subtrack);

    /* Add subtrack settings (table, colors, labels, vis & pri).  This is only
     * needed in the "not noInherit" case that hopefully will go away soon. */
    subtrack->track = subTdb->track;
    subtrack->table = subTdb->table;
    subtrack->shortLabel = subTdb->shortLabel;
    subtrack->longLabel = subTdb->longLabel;
    subtrack->priority = subTdb->priority;
    subtrack->parent = track;

    /* Add color gradient. */
    if (finalR || finalG || finalB)
	{
	subtrack->color.r = altR;
	subtrack->altColor.r = (255+altR)/2;
	altR += deltaR;
	subtrack->color.g = altG;
	subtrack->altColor.g = (255+altG)/2;
	altG += deltaG;
	subtrack->color.b = altB;
	subtrack->altColor.b = (255+altB)/2;
	altB += deltaB;
	subtrack->color.a = altA;
	subtrack->altColor.a = altA;
	altA += deltaA;
	}
    else
	{
	subtrack->color.r = subTdb->colorR;
	subtrack->color.g = subTdb->colorG;
	subtrack->color.b = subTdb->colorB;
        subtrack->color.a = 255;
	subtrack->altColor.r = subTdb->altColorR;
	subtrack->altColor.g = subTdb->altColorG;
	subtrack->altColor.b = subTdb->altColorB;
        subtrack->altColor.a = 255;
	}
    slAddHead(&track->subtracks, subtrack);
    }
slSort(&track->subtracks, trackPriCmp);
}

struct track *trackFromTrackDb(struct trackDb *tdb)
/* Create a track based on the tdb */
{
struct track *track = NULL;
char *exonArrows;
char *nextItem;

if (!tdb)
    return NULL;
track = trackNew();
track->track = cloneString(tdb->track);
track->table = cloneString(tdb->table);
track->visibility = tdb->visibility;
track->shortLabel = cloneString(tdb->shortLabel);
if (sameWord(tdb->track, "ensGene"))
    {
    struct trackVersion *trackVersion = getTrackVersion(database, "ensGene");
    if ((trackVersion != NULL) && !isEmpty(trackVersion->version))
	{
	char longLabel[256];
        if (!isEmpty(trackVersion->dateReference) && differentWord("current", trackVersion->dateReference))
	    safef(longLabel, sizeof(longLabel), "Ensembl Gene Predictions - archive %s - %s", trackVersion->version, trackVersion->dateReference);
	else
	    safef(longLabel, sizeof(longLabel), "Ensembl Gene Predictions - %s", trackVersion->version);
	track->longLabel = cloneString(longLabel);
	}
    else
	track->longLabel = cloneString(tdb->longLabel);
    }
else if (startsWith("ncbiRef", tdb->track))
    {
    char *version = checkDataVersion(database, tdb);

    // if not in that file, check the trackVersion table
    if (version == NULL)
	{
	struct trackVersion *trackVersion = getTrackVersion(database, "ncbiRefSeq");
	if ((trackVersion != NULL) && !isEmpty(trackVersion->version))
	    {
	    version = cloneString(trackVersion->version);
	    }
	}
    if (version)
	{
	char longLabel[1024];
	safef(longLabel, sizeof(longLabel), "%s - Annotation Release %s", tdb->longLabel, version);
	track->longLabel = cloneString(longLabel);
	tdb->longLabel = cloneString(longLabel);
	}
    else
	track->longLabel = cloneString(tdb->longLabel);
    }
else
    track->longLabel = cloneString(tdb->longLabel);
track->color.r = tdb->colorR;
track->color.g = tdb->colorG;
track->color.b = tdb->colorB;
track->color.a = 255; // No way to specify alpha in tdb currently - assume it's fully opaque
track->altColor.r = tdb->altColorR;
track->altColor.g = tdb->altColorG;
track->altColor.b = tdb->altColorB;
track->altColor.a = 255;
track->lineHeight = tl.fontHeight+1;
track->heightPer = track->lineHeight - 1;
track->private = tdb->private;
track->defaultPriority = tdb->priority;
char lookUpName[256];
safef(lookUpName, sizeof(lookUpName), "%s.priority", tdb->track);
tdb->priority = cartUsualDouble(cart, lookUpName, tdb->priority);
track->priority = tdb->priority;
track->groupName = cloneString(tdb->grp);
/* save default priority and group so we can reset it later */
track->defaultGroupName = cloneString(tdb->grp);
track->canPack = tdb->canPack;
if (tdb->useScore)
    {
    /* Todo: expand spectrum opportunities. */
    if (colorsSame(&brownColor, &track->color))
        track->colorShades = shadesOfBrown;
    else if (colorsSame(&darkSeaColor, &track->color))
        track->colorShades = shadesOfSea;
    else
	track->colorShades = shadesOfGray;
    }
track->tdb = tdb;

/* Handle remote database settings - just a JK experiment at the moment. */
track->remoteSqlHost = trackDbSetting(tdb, "sqlHost");
track->remoteSqlUser = trackDbSetting(tdb, "sqlUser");
track->remoteSqlPassword = trackDbSetting(tdb, "sqlPassword");
track->remoteSqlDatabase = trackDbSetting(tdb, "sqlDatabase");
track->remoteSqlTable = trackDbSetting(tdb, "sqlTable");
track->isRemoteSql =  (track->remoteSqlHost != NULL && track->remoteSqlUser != NULL
			&& track->remoteSqlDatabase != NULL && track->remoteSqlTable !=NULL);

exonArrows = trackDbSetting(tdb, "exonArrows");
/* default exonArrows to on, except for tracks in regulation/expression group */
if (exonArrows == NULL)
    {
    if (sameString(tdb->grp, "regulation"))
       exonArrows = "off";
    else
       exonArrows = "on";
    }
track->exonArrows = sameString(exonArrows, "on");
track->nextExonButtonable = TRUE;
nextItem = trackDbSetting(tdb, "nextItemButton");
if (nextItem && sameString(nextItem, "off"))
    track->nextExonButtonable = FALSE;
track->nextItemButtonable = track->nextExonButtonable;
#ifndef GBROWSE
char *iatName = trackDbSetting(tdb, "itemAttrTbl");
if (iatName != NULL)
    track->itemAttrTbl = itemAttrTblNew(iatName);
#endif /* GBROWSE */
fillInFromType(track, tdb);
return track;
}

struct hash *handlerHash = NULL;

void registerTrackHandler(char *name, TrackHandler handler)
/* Register a track handling function. */
{
if (handlerHash == NULL)
    handlerHash = newHash(6);
if (hashLookup(handlerHash, name))
    warn("handler duplicated for track %s", name);
else
    hashAdd(handlerHash, name, handler);
}

#define registerTrackHandlerOnFamily(name, handler) registerTrackHandler(name, handler)
// Marker to show that are actually registering subtracks as well.  I'd like to redo
// this system a little.  It seems dangerous now.  There's no way to know in the .ra
// file that there is a handler,  and as composite tracks start to allow multiple types
// of subtracks,  the handler of the parent will still override _all_ of the children.
// If parents and children put in different handlers, there's no way to know which one
// the child will get.

static TrackHandler lookupTrackHandler(struct trackDb *tdb)
/* Lookup handler for track of give name.  Return NULL if none. */
{
if (tdb->errMessage != NULL)
    return (TrackHandler)bigWarnMethods;
if (handlerHash == NULL)
    return NULL;
TrackHandler handler = hashFindVal(handlerHash, tdb->table);
if (handler == NULL && sameString(trackHubSkipHubName(tdb->table), "cytoBandIdeo"))
    handler = hashFindVal(handlerHash, "cytoBandIdeo");
// if nothing found, try the "trackHandler" statement
if (handler == NULL)
    {
    char *handlerName = trackDbSetting(tdb, "trackHandler");
    if (handlerName != NULL)
        {
        handler = hashFindVal(handlerHash, handlerName);
        if (handler==NULL)
            errAbort("track %s defined a trackHandler '%s' in trackDb which does not exist",
                     tdb->track, handlerName);
        }
    }
return handler;
}

TrackHandler lookupTrackHandlerClosestToHome(struct trackDb *tdb)
/* Lookup handler for track of give name.  Try parents if
 * subtrack has a NULL handler.  Return NULL if none. */
{
TrackHandler handler = lookupTrackHandler(tdb);

// while handler is NULL and we have a parent, use the parent's handler
for( ; (handler == NULL) && (tdb->parent != NULL);  )
    {
    tdb = tdb->parent;
    handler = lookupTrackHandler(tdb);
    }
return handler;
}

void registerTrackHandlers()
/* Register tracks that include some non-standard methods. */
{
#ifndef GBROWSE
if (handlerHash) // these have already been registered.
    return;
registerTrackHandler("rgdGene", rgdGeneMethods);
registerTrackHandler("cgapSage", cgapSageMethods);
registerTrackHandler("cytoBand", cytoBandMethods);
registerTrackHandler("cytoBandIdeo", cytoBandIdeoMethods);

registerTrackHandler("bacEndPairs", bacEndPairsMethods);
registerTrackHandler("bacEndPairsBad", bacEndPairsMethods);
registerTrackHandler("bacEndPairsLong", bacEndPairsMethods);
registerTrackHandler("bacEndSingles", bacEndPairsMethods);
registerTrackHandler("fosEndPairs", bacEndPairsMethods);
registerTrackHandler("fosEndPairsBad", bacEndPairsMethods);
registerTrackHandler("fosEndPairsLong", bacEndPairsMethods);
registerTrackHandler("earlyRep", bacEndPairsMethods);
registerTrackHandler("earlyRepBad", bacEndPairsMethods);

registerTrackHandler("genMapDb", genMapDbMethods);
registerTrackHandler("cgh", cghMethods);
registerTrackHandler("mcnBreakpoints", mcnBreakpointsMethods);
registerTrackHandler("fishClones", fishClonesMethods);
registerTrackHandler("mapGenethon", genethonMethods);
registerTrackHandler("stsMarker", stsMarkerMethods);
registerTrackHandler("stsMap", stsMapMethods);
registerTrackHandler("stsMapMouseNew", stsMapMouseMethods);
registerTrackHandler("stsMapRat", stsMapRatMethods);
registerTrackHandler("snpMap", snpMapMethods);
registerTrackHandler("snp", snpMethods);
registerTrackHandler("snp125", snp125Methods);
registerTrackHandler("snp126", snp125Methods);
registerTrackHandler("snp127", snp125Methods);
registerTrackHandler("snp128", snp125Methods);
registerTrackHandler("snp129", snp125Methods);
registerTrackHandler("snp130", snp125Methods);
registerTrackHandler("snp131", snp125Methods);
registerTrackHandler("snp132", snp125Methods);
registerTrackHandler("snp132Common", snp125Methods);
registerTrackHandler("snp132Flagged", snp125Methods);
registerTrackHandler("snp132Mult", snp125Methods);
registerTrackHandler("snp134", snp125Methods);
registerTrackHandler("snp134Common", snp125Methods);
registerTrackHandler("snp134Flagged", snp125Methods);
registerTrackHandler("snp134Mult", snp125Methods);
registerTrackHandler("snp135", snp125Methods);
registerTrackHandler("snp135Common", snp125Methods);
registerTrackHandler("snp135Flagged", snp125Methods);
registerTrackHandler("snp135Mult", snp125Methods);
registerTrackHandler("snp137", snp125Methods);
registerTrackHandler("snp137Common", snp125Methods);
registerTrackHandler("snp137Flagged", snp125Methods);
registerTrackHandler("snp137Mult", snp125Methods);
registerTrackHandler("snp138", snp125Methods);
registerTrackHandler("snp138Common", snp125Methods);
registerTrackHandler("snp138Flagged", snp125Methods);
registerTrackHandler("snp138Mult", snp125Methods);
registerTrackHandler("snp139", snp125Methods);
registerTrackHandler("snp139Common", snp125Methods);
registerTrackHandler("snp139Flagged", snp125Methods);
registerTrackHandler("snp139Mult", snp125Methods);
registerTrackHandler("snp141", snp125Methods);
registerTrackHandler("snp141Common", snp125Methods);
registerTrackHandler("snp141Flagged", snp125Methods);
registerTrackHandler("snp141Mult", snp125Methods);
registerTrackHandler("snp142", snp125Methods);
registerTrackHandler("snp142Common", snp125Methods);
registerTrackHandler("snp142Flagged", snp125Methods);
registerTrackHandler("snp142Mult", snp125Methods);
registerTrackHandler("ld", ldMethods);
registerTrackHandler("cnpSharp", cnpSharpMethods);
registerTrackHandler("cnpSharp2", cnpSharp2Methods);
registerTrackHandler("cnpIafrate", cnpIafrateMethods);
registerTrackHandler("cnpIafrate2", cnpIafrate2Methods);
registerTrackHandler("cnpSebat", cnpSebatMethods);
registerTrackHandler("cnpSebat2", cnpSebat2Methods);
registerTrackHandler("cnpFosmid", cnpFosmidMethods);
registerTrackHandler("cnpRedon", cnpRedonMethods);
registerTrackHandler("cnpLocke", cnpLockeMethods);
registerTrackHandler("cnpTuzun", cnpTuzunMethods);
registerTrackHandler("delConrad", delConradMethods);
registerTrackHandler("delConrad2", delConrad2Methods);
registerTrackHandler("delMccarroll", delMccarrollMethods);
registerTrackHandler("delHinds", delHindsMethods);
registerTrackHandler("delHinds2", delHindsMethods);
registerTrackHandlerOnFamily("hapmapLd", ldMethods);
registerTrackHandler("illuminaProbes", illuminaProbesMethods);
registerTrackHandler("rertyHumanDiversityLd", ldMethods);
registerTrackHandler("recombRate", recombRateMethods);
registerTrackHandler("recombRateMouse", recombRateMouseMethods);
registerTrackHandler("recombRateRat", recombRateRatMethods);
registerTrackHandler("chr18deletions", chr18deletionsMethods);
registerTrackHandler("mouseSyn", mouseSynMethods);
registerTrackHandler("mouseSynWhd", mouseSynWhdMethods);
registerTrackHandler("ensRatMusHom", ensPhusionBlastMethods);
registerTrackHandler("ensRatMm4Hom", ensPhusionBlastMethods);
registerTrackHandler("ensRatMm5Hom", ensPhusionBlastMethods);
registerTrackHandler("ensRatMusHg17", ensPhusionBlastMethods);
registerTrackHandler("ensRn3MusHom", ensPhusionBlastMethods);
registerTrackHandler("syntenyMm4", syntenyMethods);
registerTrackHandler("syntenyMm3", syntenyMethods);
registerTrackHandler("syntenyRn3", syntenyMethods);
registerTrackHandler("syntenyHg15", syntenyMethods);
registerTrackHandler("syntenyHg16", syntenyMethods);
registerTrackHandler("syntenyHuman", syntenyMethods);
registerTrackHandler("syntenyMouse", syntenyMethods);
registerTrackHandler("syntenyRat", syntenyMethods);
registerTrackHandler("syntenyCow", syntenyMethods);
registerTrackHandler("synteny100000", syntenyMethods);
registerTrackHandler("syntenyBuild30", syntenyMethods);
registerTrackHandler("syntenyBerk", syntenyMethods);
registerTrackHandler("syntenyRatBerkSmall", syntenyMethods);
registerTrackHandler("syntenySanger", syntenyMethods);
registerTrackHandler("syntenyPevzner", syntenyMethods);
registerTrackHandler("syntenyBaylor", syntenyMethods);
registerTrackHandler("switchDbTss", switchDbTssMethods);
registerTrackHandler("zdobnovHg16", lfChromColorMethods);
registerTrackHandler("zdobnovMm3", lfChromColorMethods);
registerTrackHandler("zdobnovRn3", lfChromColorMethods);
registerTrackHandler("deweySyntHg16", deweySyntMethods);
registerTrackHandler("deweySyntMm3", deweySyntMethods);
registerTrackHandler("deweySyntRn3", deweySyntMethods);
registerTrackHandler("deweySyntPanTro1", deweySyntMethods);
registerTrackHandler("deweySyntGalGal2", deweySyntMethods);
registerTrackHandler("mouseOrtho", mouseOrthoMethods);
registerTrackHandler("mouseOrthoSeed", mouseOrthoMethods);
//registerTrackHandler("orthoTop4", drawColorMethods);
registerTrackHandler("humanParalog", humanParalogMethods);
registerTrackHandler("isochores", isochoresMethods);
registerTrackHandler("gcPercent", gcPercentMethods);
registerTrackHandler("gcPercentSmall", gcPercentMethods);
registerTrackHandler("ctgPos", contigMethods);
registerTrackHandler("ctgPos2", contigMethods);
registerTrackHandler("bactigPos", bactigMethods);
registerTrackHandler("gold", goldMethods);
registerTrackHandler("gap", gapMethods);
registerTrackHandler("genomicDups", genomicDupsMethods);
registerTrackHandler("clonePos", coverageMethods);
registerTrackHandler("genieKnown", genieKnownMethods);
registerTrackHandler("knownGene", knownGeneMethods);
registerTrackHandler("h1n1_0602Seq", h1n1SeqMethods);
registerTrackHandler("h1n1b_0514Seq", h1n1SeqMethods);
registerTrackHandler("hg17Kg", hg17KgMethods);
registerTrackHandler("superfamily", superfamilyMethods);
registerTrackHandler("gad", gadMethods);
registerTrackHandler("rdmr", rdmrMethods);
registerTrackHandler("decipherOld", decipherCnvsMethods);
registerTrackHandler("decipherSnvs", decipherSnvsMethods);
registerTrackHandler("rgdQtl", rgdQtlMethods);
registerTrackHandler("rgdRatQtl", rgdQtlMethods);
registerTrackHandler("refGene", refGeneMethods);
registerTrackHandler("ncbiRefSeq", ncbiRefSeqMethods);
registerTrackHandler("ncbiRefSeqCurated", ncbiRefSeqMethods);
registerTrackHandler("ncbiRefSeqPredicted", ncbiRefSeqMethods);
// registerTrackHandler("ncbiRefSeqOther", ncbiRefSeqMethods); has become bed12
registerTrackHandler("rgdGene2", rgdGene2Methods);
registerTrackHandler("blastMm6", blastMethods);
registerTrackHandler("blastDm1FB", blastMethods);
registerTrackHandler("blastDm2FB", blastMethods);
registerTrackHandler("blastCe3WB", blastMethods);
registerTrackHandler("blastHg16KG", blastMethods);
registerTrackHandler("blastHg17KG", blastMethods);
registerTrackHandler("blastHg18KG", blastMethods);
registerTrackHandler("blatHg16KG", blastMethods);
registerTrackHandler("blatzHg17KG", blatzMethods);
registerTrackHandler("atomHomIni20_1", atomMethods);
registerTrackHandler("atom97565", atomMethods);
registerTrackHandler("mrnaMapHg17KG", blatzMethods);
registerTrackHandler("blastSacCer1SG", blastMethods);
registerTrackHandler("tblastnHg16KGPep", blastMethods);
registerTrackHandler("xenoRefGene", xenoRefGeneMethods);
registerTrackHandler("sanger22", sanger22Methods);
registerTrackHandler("sanger22pseudo", sanger22Methods);
registerTrackHandlerOnFamily("vegaGene", vegaMethods);
registerTrackHandlerOnFamily("vegaPseudoGene", vegaMethods);
registerTrackHandlerOnFamily("vegaGeneComposite", vegaMethods);
registerTrackHandler("vegaGeneZfish", vegaMethods);
registerTrackHandler("bdgpGene", bdgpGeneMethods);
registerTrackHandler("bdgpNonCoding", bdgpGeneMethods);
registerTrackHandler("bdgpLiftGene", bdgpGeneMethods);
registerTrackHandler("bdgpLiftNonCoding", bdgpGeneMethods);
registerTrackHandler("flyBaseGene", flyBaseGeneMethods);
registerTrackHandler("flyBaseNoncoding", flyBaseGeneMethods);
registerTrackHandler("sgdGene", sgdGeneMethods);
registerTrackHandler("genieAlt", genieAltMethods);
registerTrackHandler("ensGeneNonCoding", ensGeneNonCodingMethods);
registerTrackHandler("mrna", mrnaMethods);
registerTrackHandler("intronEst", estMethods);
registerTrackHandler("est", estMethods);
registerTrackHandler("all_est", estMethods);
registerTrackHandler("all_mrna", mrnaMethods);
registerTrackHandler("interPro", interProMethods);
registerTrackHandler("tightMrna", mrnaMethods);
registerTrackHandler("tightEst", mrnaMethods);
registerTrackHandler("cpgIsland", cpgIslandMethods);
registerTrackHandler("cpgIslandExt", cpgIslandMethods);
registerTrackHandler("cpgIslandExtUnmasked", cpgIslandMethods);
registerTrackHandler("cpgIslandGgfAndy", cpgIslandMethods);
registerTrackHandler("cpgIslandGgfAndyMasked", cpgIslandMethods);
registerTrackHandler("exoMouse", exoMouseMethods);
registerTrackHandler("pseudoMrna", xenoMrnaMethods);
registerTrackHandler("pseudoMrna2", xenoMrnaMethods);
registerTrackHandler("mrnaBlastz", xenoMrnaMethods);
registerTrackHandler("mrnaBlastz2", xenoMrnaMethods);
registerTrackHandler("xenoBlastzMrna", xenoMrnaMethods);
registerTrackHandler("xenoBestMrna", xenoMrnaMethods);
registerTrackHandler("xenoMrna", xenoMrnaMethods);
registerTrackHandler("xenoEst", xenoMrnaMethods);
registerTrackHandler("exoFish", exoFishMethods);
registerTrackHandler("tet_waba", tetWabaMethods);
registerTrackHandler("wabaCbr", cbrWabaMethods);
registerTrackHandler("rnaGene", rnaGeneMethods);
registerTrackHandler("encodeRna", encodeRnaMethods);
registerTrackHandler("wgRna", wgRnaMethods);
registerTrackHandler("ncRna", ncRnaMethods);
registerTrackHandler("rmskLinSpec", repeatMethods);
registerTrackHandler("rmsk", repeatMethods);
registerTrackHandler("rmskNew", repeatMethods);
registerTrackHandler("joinedRmsk", rmskJoinedMethods);
registerTrackHandler("rmskJoinedBaseline", rmskJoinedMethods);
registerTrackHandler("rmskJoinedCurrent", rmskJoinedMethods);
registerTrackHandler("rmskCensor", repeatMethods);
registerTrackHandler("simpleRepeat", simpleRepeatMethods);
registerTrackHandler("chesSimpleRepeat", simpleRepeatMethods);
registerTrackHandler("uniGene",uniGeneMethods);
registerTrackHandler("perlegen",perlegenMethods);
registerTrackHandler("haplotype",haplotypeMethods);
registerTrackHandler("encodeErge5race",encodeErgeMethods);
registerTrackHandler("encodeErgeBinding",encodeErgeMethods);
registerTrackHandler("encodeErgeExpProm",encodeErgeMethods);
registerTrackHandler("encodeErgeHssCellLines",encodeErgeMethods);
registerTrackHandler("encodeErgeInVitroFoot",encodeErgeMethods);
registerTrackHandler("encodeErgeMethProm",encodeErgeMethods);
registerTrackHandler("encodeErgeStableTransf",encodeErgeMethods);
registerTrackHandler("encodeErgeSummary",encodeErgeMethods);
registerTrackHandler("encodeErgeTransTransf",encodeErgeMethods);
registerTrackHandlerOnFamily("encodeStanfordNRSF",encodeStanfordNRSFMethods);
registerTrackHandler("cghNci60", cghNci60Methods);
registerTrackHandler("rosetta", rosettaMethods);
registerTrackHandler("affy", affyMethods);
registerTrackHandler("ancientR", ancientRMethods );
registerTrackHandler("altGraphX", altGraphXMethods );
registerTrackHandler("triangle", triangleMethods );
registerTrackHandler("triangleSelf", triangleMethods );
registerTrackHandler("transfacHit", triangleMethods );
registerTrackHandler("esRegGeneToMotif", eranModuleMethods );
registerTrackHandler("leptin", mafMethods );
registerTrackHandler("igtc", igtcMethods );
registerTrackHandler("cactusBed", cactusBedMethods );
registerTrackHandler("variome", variomeMethods);
registerTrackHandler("bedMethyl", bedMethylMethods);
registerTrackHandler("bigMethyl", bigMethylMethods);
/* Lowe lab related */
#ifdef LOWELAB
registerTrackHandler("refSeq", archaeaGeneMethods);
registerTrackHandler("gbProtCode", gbGeneMethods);
registerTrackHandler("tigrCmrORFs", tigrGeneMethods);
registerTrackHandler("BlastPEuk",llBlastPMethods);
registerTrackHandler("BlastPBac",llBlastPMethods);
registerTrackHandler("BlastPpyrFur2",llBlastPMethods);
registerTrackHandler("codeBlast",codeBlastMethods);
registerTrackHandler("tigrOperons",tigrOperonMethods);
registerTrackHandler("rabbitScore",valAlMethods);
registerTrackHandler("armadilloScore",valAlMethods);
registerTrackHandler("rnaGenes",rnaGenesMethods);
registerTrackHandler("sargassoSea",sargassoSeaMethods);
registerTrackHandler("llaPfuPrintC2", loweExpRatioMethods);
registerTrackHandler("plFoldPlus", rnaPLFoldMethods);
registerTrackHandler("plFoldMinus", rnaPLFoldMethods);
registerTrackHandler("rnaHybridization", rnaHybridizationMethods);
#endif

#ifdef LOWELAB_WIKI
registerTrackHandler("wiki", wikiMethods);
registerTrackHandler("wikibme", wikiMethods);
#endif
if (wikiTrackEnabled(database, NULL))
    registerTrackHandler("wikiTrack", wikiTrackMethods);

/*Test for my own MA data
registerTrackHandler("llaPfuPrintCExps",arrayMethods);*/
registerTrackHandler("HMRConservation", humMusLMethods);
registerTrackHandler("humMusL", humMusLMethods);
registerTrackHandler("regpotent", humMusLMethods);
registerTrackHandler("mm3Rn2L", humMusLMethods);
registerTrackHandler("hg15Mm3L", humMusLMethods);
registerTrackHandler("zoo", zooMethods);
registerTrackHandler("zooNew", zooMethods);
registerTrackHandler("musHumL", humMusLMethods);
registerTrackHandler("mm3Hg15L", humMusLMethods);
registerTrackHandler("affyTranscriptome", affyTranscriptomeMethods);
registerTrackHandler("genomicSuperDups", genomicSuperDupsMethods);
registerTrackHandler("celeraDupPositive", celeraDupPositiveMethods);
registerTrackHandler("celeraCoverage", celeraCoverageMethods);
registerTrackHandler("jkDuplicon", jkDupliconMethods);
registerTrackHandler("affyTransfrags", affyTransfragsMethods);
registerTrackHandler("RfamSeedFolds", rnaSecStrMethods);
registerTrackHandler("RfamFullFolds", rnaSecStrMethods);
registerTrackHandler("rfamTestFolds", rnaSecStrMethods);
registerTrackHandler("rnaTestFolds", rnaSecStrMethods);
registerTrackHandler("rnaTestFoldsV2", rnaSecStrMethods);
registerTrackHandler("rnaTestFoldsV3", rnaSecStrMethods);
registerTrackHandler("evofold", rnaSecStrMethods);
registerTrackHandler("evofoldRaw", rnaSecStrMethods);
registerTrackHandler("encode_tba23EvoFold", rnaSecStrMethods);
registerTrackHandler("encodeEvoFold", rnaSecStrMethods);
registerTrackHandler("rnafold", rnaSecStrMethods);
registerTrackHandler("mcFolds", rnaSecStrMethods);
registerTrackHandler("rnaEditFolds", rnaSecStrMethods);
registerTrackHandler("altSpliceFolds", rnaSecStrMethods);
registerTrackHandler("chimpSimpleDiff", chimpSimpleDiffMethods);
registerTrackHandler("tfbsCons", tfbsConsMethods);
registerTrackHandler("tfbsConsSites", tfbsConsSitesMethods);
registerTrackHandler("pscreen", simpleBedTriangleMethods);
registerTrackHandler("jaxAllele", jaxAlleleMethods);
registerTrackHandler("jaxPhenotype", jaxPhenotypeMethods);
registerTrackHandler("jaxAlleleLift", jaxAlleleMethods);
registerTrackHandler("jaxPhenotypeLift", jaxPhenotypeMethods);
/* ENCODE related */
gencodeRegisterTrackHandlers();
registerTrackHandler("affyTxnPhase2b", affyTxnPhase2Methods);
registerTrackHandler("gvPos", gvMethods);
registerTrackHandlerOnFamily("pgSnp", pgSnpMethods);
registerTrackHandlerOnFamily("pgSnpHgwdev", pgSnpMethods);
registerTrackHandlerOnFamily("pgPop", pgSnpMethods);
registerTrackHandler("pgTest", pgSnpMethods);
registerTrackHandler("protVarPos", protVarMethods);
registerTrackHandler("oreganno", oregannoMethods);
registerTrackHandler("encodeDless", dlessMethods);
transMapRegisterTrackHandlers();
retroRegisterTrackHandlers();
registerTrackHandler("retroposons", dbRIPMethods);
registerTrackHandlerOnFamily("kiddEichlerDisc", kiddEichlerMethods);
registerTrackHandlerOnFamily("kiddEichlerValid", kiddEichlerMethods);
registerTrackHandler("dgv", dgvMethods);

registerTrackHandlerOnFamily("hapmapSnps", hapmapMethods);
registerTrackHandlerOnFamily("hapmapSnpsPhaseII", hapmapMethods);
registerTrackHandlerOnFamily("omicia", omiciaMethods);
registerTrackHandler("omimGene", omimGeneMethods);
registerTrackHandler("omimGene2", omimGene2Methods);
registerTrackHandler("omimAvSnp", omimAvSnpMethods);
registerTrackHandler("omimLocation", omimLocationMethods);
registerTrackHandler("omimComposite", omimGene2Methods);
registerTrackHandler("cosmic", cosmicMethods);
registerTrackHandler("rest", restMethods);
registerTrackHandler("lrg", lrgMethods);
#endif /* GBROWSE */
}

void createHgFindMatchHash()
/* Read from the cart the string assocated with matches and
   put the matching items into a hash for highlighting later. */
{
char *matchLine = NULL;
struct slName *nameList = NULL, *name = NULL;
matchLine = cartOptionalString(cart, "hgFind.matches");
if(matchLine == NULL)
    return;
nameList = slNameListFromString(matchLine,',');
hgFindMatches = newHash(5);
for(name = nameList; name != NULL; name = name->next)
    {
    hashAddInt(hgFindMatches, name->name, 1);
    }
slFreeList(&nameList);
}

