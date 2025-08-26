/* hgTracks.h - stuff private to hgTracks, but shared with
 * individual tracks. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef HGTRACKS_H
#define HGTRACKS_H

#ifndef HVGFX_H
#include "hvGfx.h"
#endif

#ifndef HUI_H
#include "hui.h"
#endif

#include "jsHelper.h"
#include "imageV2.h"

#ifndef CART_H
#include "cart.h"
#endif

#ifndef PSL_H
#include "psl.h"
#endif

#ifndef BED_H
#include "bed.h"
#endif

#ifndef TRACKLAYOUT_H
#include "trackLayout.h"
#endif /* TRACKLAYOUT_H */

#ifndef HPRINT_H
#include "hPrint.h"
#endif /* HPRINT_H */

#ifndef GBROWSE
#ifndef ITEMATTR_H
#include "itemAttr.h"
#endif /* ITEMATTR_H */
#endif /* GBROWSE */

#include "soTerm.h"

/* A few hgGenome cart constant defaults copied from */
#define hggPrefix "hgGenome_"
#define hggGraphPrefix hggPrefix "graph"

/* trackDb setting for expRatio tracks */
#define EXP_COLOR_DENSE "expColorDense"

#ifdef LOWELAB
#define MAXPIXELS 60000
#else
#define MAXPIXELS 14000
#endif

#define BIGBEDMAXIMUMITEMS 1000000

#define MULTI_REGION_VIRTUAL_CHROM_NAME "multi"
// original name was 'virt'

#define MULTI_REGION_CFG_BUTTON_TOP     "multiRegionButtonTop"

/* for botDelay call, 10 second for warning, 20 second for immediate exit */
#define delayFraction   0.25
extern long enteredMainTime;

#include "lolly.h"
#include "spaceSaver.h"

struct track
/* Structure that displays of tracks. The central data structure
 * of the graphical genome browser. */
    {
    struct track *next;   /* Next on list. */
    char *track;             /* Track symbolic name. Name on image map etc. Same as tdb->track. */
    char *table;             /* Table symbolic name. Name of database table. Same as tdb->table.*/
    enum trackVisibility visibility; /* How much of this want to see. */
    enum trackVisibility limitedVis; /* How much of this actually see. */
    boolean limitedVisSet;	     /* Is limited visibility set? */

    char *longLabel;           /* Long label to put in center. */
    char *shortLabel;          /* Short label to put on side. */

    bool mapsSelf;          /* True if system doesn't need to do map box. */
    bool drawName;          /* True if BED wants name drawn in box. */

    Color *colorShades;	       /* Color scale (if any) to use. */
    struct rgbColor color;     /* Main color. */
    Color ixColor;             /* Index of main color. */
    Color *altColorShades;     /* optional alternate color scale */
    struct rgbColor altColor;  /* Secondary color. */
    Color ixAltColor;

    void (*loadItems)(struct track *tg);
    /* loadItems loads up items for the chromosome range indicated.   */

    void *items;               /* Some type of slList of items. */
    struct decoratorGroup *decoratorGroup;   /* Some type of slList of decoration sets for items. */

    char *(*itemName)(struct track *tg, void *item);
    /* Return name of one of an item to display on left side. */

    char *(*mapItemName)(struct track *tg, void *item);
    /* Return name to associate on map. */

    int (*totalHeight)(struct track *tg, enum trackVisibility vis);
	/* Return total height. Called before and after drawItems.
	 * Must set the following variables. */
    int height;                /* Total height - must be set by above call. */
    int lineHeight;            /* Height per item line including border. */
    int heightPer;             /* Height per item line minus border. */

    int (*itemHeight)(struct track *tg, void *item);
    /* Return height of one item in pixels. */

    int (*itemHeightRowsForPack)(struct track *tg, void *item);
    /* If set, allows items to claim they a particular number of rows while being packed.
     * Useful for tracks with variable-height items.  */

    int (*itemRightPixels)(struct track *tg, void *item);
    /* Return number of pixels needed to right of item for additional labeling. (Optional) */

    void (*preDrawMultiRegion)(struct track *tg);
    /* Handle multi-region issues after predraw and before draw such as autoScale for wigs. */

    void (*preDrawItems)(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis);
    /* Do PreDraw item list, one per track. */

    void (*drawItems)(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis);
    /* Draw item list, one per track. */

    void (*drawItemAt)(struct track *tg, void *item, struct hvGfx *hvg,
        int xOff, int yOff, double scale,
	MgFont *font, Color color, enum trackVisibility vis);
    /* Draw a single item.  This is optional, but if it's here
     * then you can plug in genericDrawItems into the drawItems,
     * which takes care of all sorts of things including packing. */

    void (*drawItemLabel)(struct track *tg, struct spaceNode *sn,
                          struct hvGfx *hvg, int xOff, int y, int width,
                          MgFont *font, Color color, Color labelColor, enum trackVisibility vis,
                          double scale, boolean withLeftLabels);
    /* Draw the label for an item */

    void (*doItemMapAndArrows)(struct track *tg, struct spaceNode *sn,
                               struct hvGfx *hvg, int xOff, int y, int width,
                               MgFont *font, Color color, Color labelColor, enum trackVisibility vis,
                               double scale, boolean withLeftLabels);
    /* Handle item map and exon arrows for an item */

    int (*itemStart)(struct track *tg, void *item);
    /* Return start of item in base pairs. */

    int (*itemEnd)(struct track *tg, void *item);
    /* Return end of item in base pairs. */

    void (*freeItems)(struct track *tg);
    /* Free item list. */

    struct hash *attrTable;
    /* Persistent table to speed up lookup of attributes in secondary tables (optional). */

    Color (*itemColor)(struct track *tg, void *item, struct hvGfx *hvg);
    /* Get color of item (optional). */

    Color (*itemNameColor)(struct track *tg, void *item, struct hvGfx *hvg);
    /* Get color for the item's name (optional). */

    Color (*itemLabelColor)(struct track *tg, void *item, struct hvGfx *hvg);
    /* Get color for the item's label (optional). */

    void (*mapItem)(struct track *tg, struct hvGfx *hvg, void *item,
                    char *itemName, char *mapItemName, int start, int end,
                    int x, int y, int width, int height);
    /* Write out image mapping for a given item */

    boolean hasUi;	/* True if has an extended UI page. */
    void *wigCartData;  /* pointer to wigCart */
    void *extraUiData;	/* Pointer for track specific filter etc. data. */

    void (*trackFilter)(struct track *tg);
    /* Stuff to handle user interface parts. */

    void *customPt;  /* Misc pointer variable unique to track. */
    int customInt;   /* Misc int variable unique to track. */
    int subType;     /* Variable to say what subtype this is for similar tracks
	              * to share code. */

    /* Stuff for the various wig incarnations - sample, wig, bigWig */
    float minRange, maxRange;	  /*min and max range for sample tracks 0.0 to 1000.0*/
    float scaleRange;             /* What to scale samples by to get logical 0-1 */
    double graphUpperLimit, graphLowerLimit;	/* Limits of actual data in window for wigs. */
    struct preDrawContainer *preDrawContainer;  /* Numbers to graph in wig, one per pixel */
    struct preDrawContainer *(*loadPreDraw)(struct track *tg, int seqStart, int seqEnd, int width);
    struct wigGraphOutput *wigGraphOutput;  /* Where to draw wig - different for transparency */
    /* Do bits that load the predraw buffer.  Called to set preDrawContainer */

    struct bbiFile *bbiFile;	/* Associated bbiFile for bigWig or bigBed. */

    int bedSize;		/* Number of fields if a bed file. */
    boolean isBigBed;		/* If a bed, is it a bigBed? */

    boolean isRemoteSql;	/* Is using a remote mySQL connection. */
    char *remoteSqlHost;	/* Host machine name for remote DB. */
    char *remoteSqlUser;	/* User name for remote DB. */
    char *remoteSqlPassword;	/* Password for remote DB. */
    char *remoteSqlDatabase;	/* Database in remote DB. */
    char *remoteSqlTable;	/* Table name in remote DB. */

    char *otherDb;		/* Other database for an axt track. */

    unsigned short private;	/* True(1) if private, false(0) otherwise. */
    float priority;   /* Tracks are drawn in priority order. */
    float defaultPriority;   /* Tracks are drawn in priority order. */
    char *groupName;	/* Name of group if any. */
    struct group *group;  /* Group this track is associated with. */
    char *defaultGroupName;  /* default Group this track is associated with. */
    boolean canPack;	/* Can we pack the display for this track? */
    struct spaceSaver *ss;  /* Layout when packed. */

    struct trackDb *tdb; /*todo:change visibility, etc. to use this */

    float expScale;	/* What to scale expression tracks by. */
    char *expTable;	/* Expression table in hgFixed. */

    long sourceCount;	/* Number of sources for factorSource tracks. */
    struct expRecord **sources;  /* Array of sources */
    int sourceRightPixels;	/* Number of pixels to right we'll need. */

    boolean exonArrows;	/* Draw arrows on exons? */
    boolean exonArrowsAlways;	/* Draw arrows on exons even with introns showing? */
    boolean nextExonButtonable; /* Use the next-exon buttons? */
    boolean nextItemButtonable; /* Use the next-gene buttons? */
    struct itemAttrTbl *itemAttrTbl;  /* relational attributes for specific
                                         items (color) */

    /* fill in left label drawing area */
    Color labelColor;   /* Fixed color for the track label (optional) */
    void (*drawLeftLabels)(struct track *tg, int seqStart, int seqEnd,
                           struct hvGfx *hvg, int xOff, int yOff, int width, int height,
                           boolean withCenterLabels, MgFont *font,
                           Color color, enum trackVisibility vis);

    struct track *subtracks;   /* list of subsidiary tracks that are
                                loaded and drawn by this track.  This
                                is used for "composite" tracks, such
                                as "mafWiggle */
    struct track *parent;	/* Parent track if any */
    struct track *prevTrack;    // if not NULL, points to track immediately above in the image.
                                //    Needed by ConditionalCenterLabel logic

    boolean (*nextPrevExon)(struct track *tg, struct hvGfx *hvg, void *item, int x, int y, int w, int h, boolean next);
    /* Function will draw the button on a track item and assign a map */
    /* box to it as well, so that a click will move the browser window */
    /* to the next (or previous if next==FALSE) item. This is meant to */
    /* jump to parts of an item already partially in the window but is */
    /* hanging off the edge... e.g. the next exon in a gene. */

    void (*nextPrevItem)(struct track *tg, boolean next);
    /* If this function is given, it can dictate where the browser loads */
    /* up based on whether a next-item button on the longLabel line of */
    /* the track was pressed (as opposed to the next-item buttons on the */
    /* track items themselves... see nextPrevExon() ). This is meant for */
    /* going to the next/previous item currently unseen in the browser, */
    /* e.g. the next gene. */

    char *(*itemDataName)(struct track *tg, char *itemName);
    /* If not NULL, function to translated an itemName into a data name.
     * This is can be used for looking up sequence, CDS, etc. It is used
     * to support item names that have uniqueness identifiers added to deal
     * with multiple alignments.  The resulting value should *not* be freed,
     * and it should be assumed that it might only remain valid for a short
     * period of time.*/

    int loadTime;	/* Time it takes to load (for performance tuning) */
    int drawTime;	/* Time it takes to draw (for performance tuning) */

    enum enumBool remoteDataSource; /* The data for this track is from a remote source */
                   /* Slow retrieval means image can be rendered via an AJAX callback. */
    boolean customTrack; /* Need to explicitly declare this is a custom track */
    boolean syncChildVisToSelf;	/* If TRUE sync visibility to of children to self. */
    char *networkErrMsg;        /* Network layer error message */
    boolean parallelLoading;    /* If loading in parallel, usually network resources. */
    struct bbiSummaryElement *summary;  /* for bigBed */
    struct bbiSummaryElement *sumAll;   /* for bigBed */
    boolean drawLabelInBox;     /* draw labels into the features instead of next to them */
    boolean drawLabelInBoxNotDense;    /* don't draw labels in dense mode, (needed only when drawLabelInBox set */
    
    struct track *nextWindow;   /* Same track in next window's track list. */
    struct track *prevWindow;   /* Same track in prev window's track list. */

    // Fixed-width non-proportional tracks
    void (*nonPropDrawItemAt)(struct track *tg, void *item, struct hvGfx *hvg,
        int xOff, int yOff, double scale,
	MgFont *font, Color color, enum trackVisibility vis);
    /* Draw a single Non-proportional fixed-width item.  Such as gtexGene.
     * This is method is optional, but if it's here
     * then you can plug in genericDrawItems into the drawItems,
     * which takes care of all sorts of things including packing. */

    int (*nonPropPixelWidth)(struct track *tg, void *item);
    /* Return the width in pixels of the non-proportional part of track, e.g. gtexGene graphic */


    struct slInt *labelColumns; /* The columns in a bigBed that can be used for labels. */
    boolean subTrackVisSet;     /* have we calculated visibility on this subtrack */
    boolean subTrackVis;        /* if we calculated it, what is it */

    struct lollyCartOptions *lollyCart;
    double squishyPackPoint;    /* the value at which we switch to squish mode. */
    char * originalTrack;       /* the name of the original track if this a pseduo-duplicate track used for squishyPack */
    boolean limitWiggle;       /* TRUE if this track is drawing in coverage mode because of limited visibility */
    boolean isColorBigBed;      /* True if this track is a fake track used for wiggle coloring. */
    void (*loadSummary)(struct track *tg); /* loadSumary data for bigBed etc.   */
    };

struct window  // window in multiwindow image
    {
    struct window *next;   // Next on list.

    // These two were experimental and will be removed soon:
    char *organism;        /* Name of organism */
    char *database;        /* Name of database */

    char *chromName;
    int winStart;           // in bases
    int winEnd;
    int insideX;            // in pixels
    int insideWidth;
    long virtStart;         // in bases on virt chrom
    long virtEnd;

    boolean regionOdd;      // window comes from odd region? or even? for window separator coloring

    struct track *trackList;   // track list for window
    };

extern boolean forceWiggle; // we've run out of space so all tracks become coverage tracks

typedef void (*TrackHandler)(struct track *tg);

int trackPriCmp(const void *va, const void *vb);
/* Compare for sort based on priority */

boolean trackIsCompositeWithSubtracks(struct track *track);
/* Temporary function until all composite tracks point to their own children */

struct trackRef
/* A reference to a track. */
    {
    struct trackRef *next;	/* Next in list. */
    struct track *track;	/* Underlying track. */
    };

struct group
/* A group of related tracks. */
    {
    struct group *next;	   /* Next group in list. */
    char *name;		   /* Symbolic name. */
    char *label;	   /* User visible name. */
    float priority;        /* Display order, 0 is on top. */
    float defaultPriority; /* original priority before reordering */
    struct trackRef *trackList;  /* List of tracks. */
    boolean defaultIsClosed; /* close the track group by default. */
    char *errMessage;      /* any error messages that came up during trackDb parsing. */
    };

struct simpleFeature
/* Minimal feature - just stores position in browser coordinates. */
    {
    struct simpleFeature *next;
    int start, end;			/* Start/end in browser coordinates. */
    int qStart, qEnd;			/* query start/end */
    int grayIx;                         /* Level of gray usually. */
    int codonIndex;                     /* 1-based codon index (ignored if 0) */
    };

/* Some details of how to draw linked features. */
enum {lfSubXeno = 1};
enum {lfSubSample = 2};
enum {lfWithBarbs = 3}; /* Turn on barbs to show direction based on
                         * strand field */
enum {lfSubChain = 4};
enum {lfNoIntronLines = 5}; /* Draw no lines between exon blocks */

enum highlightMode
    {
    highlightNone=0,
    highlightBackground=1,
    highlightOutline=2
    };

struct linkedFeatures
/* A linked set of features - drawn as a bunch of boxes (often exons)
 * connected by horizontal lines (often introns).  About 75% of
 * the browser tracks end up as linkedFeatures. */
    {
    struct linkedFeatures *next;
    int start, end;			/* Start/end in browser coordinates. */
    int tallStart, tallEnd;		/* Start/end of fat display. */
    int grayIx;				/* Average of components. */
    int filterColor;			/* Filter color (-1 for none) */
    float score;                        /* score for this feature */
    char *name;				/* Accession of query seq. Usually also the label. */
    int orientation;                    /* Orientation. */
    struct simpleFeature *components;   /* List of component simple features. Usually exons. */
    struct simpleFeature *codons;       /* If zoomed to CDS or codon level.*/
    boolean useItemRgb;                 /* If true, use rgb from item. */
    void *extra;			/* Extra info that varies with type. */
    void *original;			/* The structure that was converted
					   into this (when needed later).  */
    struct itemAttr *itemAttr;          /* itemAttr object for this lf, or NULL */
    unsigned highlightColor;            /* highlight color,0 if no highlight */
    enum highlightMode highlightMode;   /* highlight mode,0 if no highlight */
    char* mouseOver;                    /* mouse over text */
    char* cds;                          /* CDS in NCBI format */
#ifdef USE_HAL
    boolean isHalSnake;
    struct hal_target_dupe_list_t *dupeList;
#endif
    boolean isBigGenePred;
    char *label;                        /* Label for bigBeds. */
    int qSize;				/* Query size for chain/bigChain */
    double squishyPackVal;              /* the squishyPoint value for this item. */
    struct snakeInfo *snakeInfo;           /* if we're in snake mode, here's the deets */
    };

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

struct knownGenesExtra
/* need more than 1 string in linkedFeatures extra field */
    {
    char *hgg_prot;             /* protein ID */
    char *name;                 /* name to be used on label */
    };

struct gsidSubj
/* global GSID subject list */
    {
    struct gsidSubj  *next;
    char *subjId;
    };

struct gsidSeq
/* global GSID sequence list */
    {
    struct gsidSeq  *next;
    char *seqId;
    char *subjId;
    };


struct virtRegion
/* virtual chromosome structure */
    {
    struct virtRegion *next;
    char *chrom;
    int start;
    int end;
    char strand[2];	/* + or - for strand */
    };

struct virtChromRegionPos
/* virtual chromosome region position*/
    {
    long virtPos;
    struct virtRegion *virtRegion;
    };

struct positionMatch
/* virtual chroom position that matches or overlaps search query chrom,start,end */
 {
 struct positionMatch *next;
 long virtStart;
 long virtEnd;
 };

/* mouseOver business declared in hgTracks.c */
extern boolean enableMouseOver;
extern struct tempName *mouseOverJsonFile;
extern struct jsonWrite *mouseOverJson;

struct wigMouseOver
    {
    struct wigMouseOver *next;
    int x1;	/* beginning of a rectangle for this value */
    int x2;	/* end of the rectangle */
    double value;	/* data value for this region */
    int valueCount;	/* number of data values in this rectangle */
    };

extern struct virtRegion *virtRegionList;
extern struct virtChromRegionPos *virtChrom; // Array
extern int virtRegionCount;
extern long virtWinStart;  // start of virtual window in bases
extern long virtWinEnd;    //   end of virtual window in bases
extern long defaultVirtWinStart;  // default start of virtual window in bases
extern long defaultVirtWinEnd;    // default end   of virtual window in bases
extern long virtWinBaseCount;  /* Number of bases in windows, also virtWinEnd - virtWinStart. */
extern long virtSeqBaseCount; // all bases in virt chrom
//extern char *virtPosition;          /* Name of virtual position. TODO Remove? */
extern char *virtChromName;         /* Name of virtual chrom */
extern boolean virtMode;            /* Are we in virtual chrom mode? */
extern boolean virtChromChanged;     /* Has the virtChrom changed? */
extern boolean emAltHighlight;      /* Highlight alternativing regions in virt view? */
extern int emPadding;               /* # bases padding for exon-mostly regions */
extern int gmPadding;               /* # bases padding for gene-mostly regions */
extern char *emGeneTable;           /* Gene table to use for exon mostly */
extern struct track *emGeneTrack;   /* Track for gene table for exon mostly */
extern struct rgbColor vertWindowSeparatorColor; /* color for vertical windows separator */
extern char *multiRegionsBedUrl;       /* URL to bed regions list */

// is genome RNA?
extern boolean genomeIsRna;

// demo2
extern int demo2NumWindows;
extern int demo2WindowSize;
extern int demo2StepSize;

// singleTrans (single transcript)
extern char *singleTransId; 

// singleAltHaplos (one alternate haplotype)
extern char *singleAltHaploId; 

extern char *virtModeType;         /* virtual chrom mode type */
extern char *lastVirtModeType;
extern char *virtModeShortDescr;   /* short description of virt mode */
extern char *virtModeExtraState;   /* Other settings that affect the virtMode state such as padding or some parameter */
extern char *lastVirtModeExtraState;
extern struct cart *lastDbPosCart;     /* store settings for use in lastDbPos and hgTracks.js setupHistory */

extern char *excludeVars[];
extern struct trackLayout tl;
extern struct jsonElement *jsonForClient;

/* multiple windows */
extern struct window *windows;        // list of windows in image
extern struct window *currentWindow;  // current window
extern bool trackLoadingInProgress;  // flag to delay ss layout until all windows are ready.
extern int fullInsideX;      // full-image insideX 
extern int fullInsideWidth;  // full-image insideWidth 

extern struct cart *cart; /* The cart where we keep persistent variables. */
extern struct hash *oldVars;       /* List of vars from previous cart. */
extern struct track *trackList;    /* List of all tracks. */
extern struct hash *trackHash; /* Hash of the tracks by their name. */
extern char *chromName;	  /* Name of chromosome sequence . */
extern char *displayChromName;	  /* Name of chromosome sequence to display . */
extern char *database;	  /* Name of database we're using. */
extern char *organism;	  /* Name of organism we're working on. */
extern char *browserName;              /* Test or public browser */
extern char *organization;             /* UCSC or MGC */
extern int winStart;	  /* Start of window in sequence. */
extern int winEnd;	  /* End of window in sequence. */
extern int maxItemsInFullTrack;  /* Maximum number of items displayed in full */
extern char *position; 		/* Name of position. */
extern int gfxBorder;		/* Width of graphics border. */
extern int insideWidth;		/* Width of area to draw tracks in in pixels. */
extern int insideX;		/* Start of area to draw track in in pixels. */
extern int leftLabelX;		/* Start of area to draw left labels on. */
extern int leftLabelWidth;	/* Width of area to draw left labels on. */
extern boolean withLeftLabels;		/* Display left labels? */
extern boolean withCenterLabels;	/* Display center labels? */
extern boolean withGuidelines;		/* Display guidelines? */
extern boolean withNextExonArrows;	/* Display next exon navigation buttons near center labels? */
extern boolean withExonNumbers;	/* Display exon and intron numbers in mouseOver instead of item name */
extern struct hvGfx *hvgSide;  // An extra pointer to side label image that can be built if needed

extern int seqBaseCount;  /* Number of bases in sequence. */
extern int winBaseCount;  /* Number of bases in window. */
extern float basesPerPixel;       /* bases covered by a pixel; a measure of zoom */
extern boolean zoomedToBaseLevel; /* TRUE if zoomed so we can draw bases. */
extern boolean zoomedToCodonLevel; /* TRUE if zoomed so we can print codon text in genePreds*/
extern boolean zoomedToCodonNumberLevel; /* TRUE if zoomed so we can print codons and exon number text in genePreds*/
extern boolean zoomedToCdsColorLevel; /* TRUE if zoomed so we cancolor each codon*/

extern char *ctFileName;	/* Custom track file. */
extern struct customTrack *ctList;  /* Custom tracks. */
extern struct slName *browserLines; /* Custom track "browser" lines. */

extern int rulerMode;         /* on, off, full */
extern boolean withLeftLabels;		/* Display left labels? */
extern boolean withCenterLabels;	/* Display center labels? */
extern boolean withPriorityOverride;    /* enable track reordering? */
extern boolean revCmplDisp;             /* reverse-complement display */
extern boolean measureTiming;	/* Flip this on to display timing
                                 * stats on each track at bottom of page. */

extern struct hash *hgFindMatches; /* The matches found by hgFind that should be highlighted. */

extern int maxShade;		  /* Highest shade in a color gradient. */
extern Color shadesOfGray[10+1];  /* 10 shades of gray from white to black
                                   * Red is put at end to alert overflow. */
extern Color shadesOfBrown[10+1]; /* 10 shades of brown from tan to tar. */
extern struct rgbColor guidelineColor;
extern struct rgbColor multiRegionAltColor;
extern struct rgbColor undefinedYellowColor;
extern Color darkGreenColor;

extern Color shadesOfSea[10+1];       /* Ten sea shades. */

/* 16 shades from black to fully saturated of red/green/blue for
 * expression data. */
#define EXPR_DATA_SHADES 16
extern Color shadesOfGreen[EXPR_DATA_SHADES];
extern Color shadesOfRed[EXPR_DATA_SHADES];
extern Color shadesOfBlue[EXPR_DATA_SHADES];
extern Color shadesOfYellow[EXPR_DATA_SHADES];

extern Color shadesOfGreenOnWhite[EXPR_DATA_SHADES];
extern Color shadesOfRedOnWhite[EXPR_DATA_SHADES];
extern Color shadesOfBlueOnWhite[EXPR_DATA_SHADES];
extern Color shadesOfYellowOnWhite[EXPR_DATA_SHADES];

extern Color shadesOfRedOnYellow[EXPR_DATA_SHADES];
extern Color shadesOfBlueOnYellow[EXPR_DATA_SHADES];

extern boolean chromosomeColorsMade; /* Have the 3 shades of 8 chromosome colors been allocated? */
extern boolean doPliColors; /* Put up the color legend for the gnomAD pLI track */
extern boolean exprBedColorsMade; /* Have the shades of Green, Red, and Blue been allocated? */
extern int maxRGBShade;
extern int colorLookup[256]; /* Common to variation.c and rnaPLFoldtrack.c */

extern boolean trackImgOnly;           /* caller wants just the track image and track table html */

/* used in MAF display */
#define UNALIGNED_SEQ 'o'
#define MAF_DOUBLE_GAP '='

void abbr(char *s, char *fluff);
/* Cut out fluff from s. */

struct track *getTrackList(struct group **pGroupList, int vis);
/* Return list of all tracks, organizing by groups.
 * If vis is -1, restore default groups to tracks.
 * Shared by hgTracks and configure page. */

void groupTrackListAddSuper(struct cart *cart, struct group *group, struct hash *superHash);
/* Construct a new track list that includes supertracks, sort by priority,
 * and determine if supertracks have visible members.
 * Replace the group track list with this new list.
 * Shared by hgTracks and configure page to expand track list,
 * in contexts where no track display functions (which don't understand
 * supertracks) are invoked.  */

void removeTrackFromGroup(struct track *track);
/* Remove track from group it is part of. */

struct sqlConnection *remoteTrackConnection(struct track *tg);
/* Get a connection to remote database as specified in remoteSql settings... */

int orientFromChar(char c);
/* Return 1 or -1 in place of + or - */

enum trackVisibility limitVisibility(struct track *tg);
/* Return default visibility limited by number of items. */

char *hgcNameAndSettings();
/* Return path to hgc with variables to store UI settings. */

void mapBoxHc(struct hvGfx *hvg, int start, int end, int x, int y, int width, int height,
	char *group, char *item, char *statusLine);
/* Print out image map rectangle that would invoke the htc (human track click)
 * program. */

void mapBoxReinvoke(struct hvGfx *hvg, int x, int y, int width, int height,
		    struct track *track, boolean toggle, char *chrom,
		    long start, long end, char *message, char *extra);
/* Print out image map rectangle that would invoke this program again.
 * If track is non-NULL then put that track's id in the map item.
 * if toggle is true, then toggle track between full and dense.
 * If chrom is non-null then jump to chrom:start-end.
 * Add extra string to the URL if it's not NULL */

void mapBoxToggleVis(struct hvGfx *hvg, int x, int y, int width, int height,
	struct track *curGroup);
/* Print out image map rectangle that would invoke this program again.
 * program with the current track expanded. */

void mapBoxJumpTo(struct hvGfx *hvg, int x, int y, int width, int height, struct track *toggleGroup,
		  char *newChrom, long newStart, long newEnd, char *message);
/* Print out image map rectangle that would invoke this program again
 * at a different window. */

void mapBoxHgcOrHgGene(struct hvGfx *hvg, int start, int end, int x, int y, int width, int height,
                       char *track, char *item, char *statusLine, char *directUrl, boolean withHguid,
                       char *extra);
/* Print out image map rectangle that would invoke the hgc (human genome click)
 * program. */

void genericDrawItemLabel(struct track *tg, struct spaceNode *sn,
                    struct hvGfx *hvg, int xOff, int y, int width,
                    MgFont *font, Color color, Color labelColor, enum trackVisibility vis,
                    double scale, boolean withLeftLabels);
/* Generic function for writing out an item label */


void genericItemMapAndArrows(struct track *tg, struct spaceNode *sn,
                    struct hvGfx *hvg, int xOff, int y, int width,
                    MgFont *font, Color color, Color labelColor, enum trackVisibility vis,
                    double scale, boolean withLeftLabels);
/* Generic function for putting down a mapbox with a label and drawing exon arrows */


void genericMapItem(struct track *tg, struct hvGfx *hvg, void *item,
		    char *itemName, char *mapItemName, int start, int end,
		    int x, int y, int width, int height);
/* This is meant to be used by genericDrawItems to set to tg->mapItem in */
/* case tg->mapItem isn't set to anything already. */

void mapStatusMessage(char *format, ...)
/* Write out stuff that will cause a status message to
 * appear when the mouse is over this box. */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;

double scaleForWindow(double width, int seqStart, int seqEnd);
/* Return the scale for the window. */

double scaleForPixels(double pixelWidth);
/* Return what you need to multiply bases by to
 * get to scale of pixel coordinates. */

boolean scaledBoxToPixelCoords(int chromStart, int chromEnd, double scale, int xOff,
                               int *pX1, int *pX2);
/* Convert chrom coordinates to pixels. Clip to window to prevent integer overflow.
 * For special case of a SNP insert location with width==0, set width=1.
 * Returns FALSE if it does not intersect the window, or if it would have a negative width. */

void drawScaledBox(struct hvGfx *hvg, int chromStart, int chromEnd,
	double scale, int xOff, int y, int height, Color color);
/* Draw a box scaled from chromosome to window coordinates.
 * Get scale first with scaleForPixels. */

void drawScaledBoxLabel(struct hvGfx *hvg,
     int chromStart, int chromEnd, double scale,
     int xOff, int y, int height, Color color, MgFont *font,  char *label);
/* Draw a box scaled from chromosome to window coordinates and draw a label onto it. */

typedef enum {
    GLYPH_CIRCLE,
    GLYPH_TRIANGLE,
    GLYPH_INV_TRIANGLE,
    GLYPH_SQUARE,
    GLYPH_DIAMOND,
    GLYPH_OCTAGON,
    GLYPH_STAR,
    GLYPH_PENTAGRAM
    } glyphType;

void drawScaledGlyph(struct hvGfx *hvg, int chromStart, int chromEnd, double scale, int xOff, int y,
                      int heightPer, glyphType glyph, boolean filled, Color outlineColor, Color fillColor);
/* Draw a glyph as a circle/polygon.  If filled, draw as with fillColor,
 * which may have transparency.
 */

glyphType parseGlyphType(char *glyphStr);
/* Return the enum glyph type for a string specifying a glyph.
 * Defaults to GLYPH_CIRCLE if the string is unrecognized. */

Color whiteIndex();
/* Return index of white. */

Color blackIndex();
/* Return index of black. */

Color grayIndex();
/* Return index of gray. */

Color lightGrayIndex();
/* Return index of light gray. */

Color veryLightGrayIndex();
/* Return index of very light gray. */

int grayInRange(int val, int minVal, int maxVal);
/* Return gray shade corresponding to a number from minVal - maxVal */

int pslGrayIx(struct psl *psl, boolean isXeno, int maxShade);
/* Figure out gray level for an RNA block. */

Color getSeqColor(char *name, struct hvGfx *hvg);
/* Return color index corresponding to chromosome/scaffold name. */

Color darkerColor(struct hvGfx *hvg, Color color);
/* Get darker shade of a color - half way between this color and black */

Color somewhatDarkerColor(struct hvGfx *hvg, Color color);
/* Get a somewhat lighter shade of a color - 1/3 of the way towards black. */

Color slightlyDarkerColor(struct hvGfx *hvg, Color color);
/* Get a slightly darker shade of a color - 1/4 of the way towards black. */

Color lighterColor(struct hvGfx *hvg, Color color);
/* Get lighter shade of a color - half way between this color and white */

Color somewhatLighterColor(struct hvGfx *hvg, Color color);
/* Get a somewhat lighter shade of a color - 1/3 of the way towards white. */

Color slightlyLighterColor(struct hvGfx *hvg, Color color);
/* Get slightly lighter shade of a color - closer to gray actually  */

void clippedBarbs(struct hvGfx *hvg, int x, int y,
	int width, int barbHeight, int barbSpacing, int barbDir, Color color,
	boolean needDrawMiddle);
/* Draw barbed line.  Clip it to fit the window first though since
 * some barbed lines will span almost the whole chromosome, and the
 * clipping at the lower level is not efficient since we added
 * PostScript output support. */

void innerLine(struct hvGfx *hvg, int x, int y, int w, Color color);
/* Draw a horizontal line of given width minus a pixel on either
 * end.  This pixel is needed for PostScript only, but doesn't
 * hurt elsewhere. */

void grayThreshold(UBYTE *pt, int count, Color *colors);
/* Convert from 0-4 representation to gray scale rep. */

/* Some little functional stubs to fill in track group
 * function pointers with if we have nothing to do. */
void tgLoadNothing(struct track *tg);
void tgDrawNothing(struct track *tg);
void tgFreeNothing(struct track *tg);
int tgItemNoStart(struct track *tg, void *item);
int tgItemNoEnd(struct track *tg, void *item);

int tgFixedItemHeight(struct track *tg, void *item);
/* Return item height for fixed height track. */

int tgFixedTotalHeightOptionalOverflow(struct track *tg, enum trackVisibility vis,
			       int lineHeight, int heightPer, boolean allowOverflow);
/* Most fixed height track groups will use this to figure out the height
 * they use. */

int tgFixedTotalHeightNoOverflow(struct track *tg, enum trackVisibility vis);
/* Most fixed height track groups will use this to figure out the height
 * they use. */

void changeTrackVis(struct group *groupList, char *groupTarget, int changeVis);
/* Change track visibilities. If groupTarget is
 * NULL then set visibility for tracks in all groups.  Otherwise,
 * just set it for the given group.  If vis is -2, then visibility is
 * unchanged.  If -1 then set visibility to default, otherwise it should
 * be tvHide, tvDense, etc.
 */

void genericDrawItems(struct track *tg, int seqStart, int seqEnd,
                      struct hvGfx *hvg, int xOff, int yOff, int width,
                      MgFont *font, Color color, enum trackVisibility vis);
/* Draw generic item list.  Features must be fixed height
 * and tg->drawItemAt has to be filled in. */

void bedDrawSimpleAt(struct track *tg, void *item,
                     struct hvGfx *hvg, int xOff, int y,
                     double scale, MgFont *font, Color color, enum trackVisibility vis);
/* Draw a single simple bed item at position. */

void bedDrawSimple(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis);
/* Draw simple Bed items. */

typedef struct slList *(*ItemLoader)(char **row);

typedef struct bed *(*bedItemLoader)(char **row);

void bedLoadItemByQuery(struct track *tg, char *table, char *query, ItemLoader loader);
/* Generic tg->item loader. If query is NULL use generic hRangeQuery(). */

void bedLoadItemWhere(struct track *tg, char *table, char *extraWhere, ItemLoader loader);
/* Generic tg->item loader, adding extra clause to hgRangeQuery. */

void bedLoadItem(struct track *tg, char *table, ItemLoader loader);
/* Generic tg->item loader. */

boolean simpleBedNextPrevEdge(struct track *tg, struct hvGfx *hvg, void *item, int x, int y, int w,
			   int h, boolean next);
/* Like linkedFeaturesNextPrevItem, but for simple bed which has no block structure so
 * this simply zaps us to the right/left edge of the feature.  Arrows have already been
 * drawn; here we figure out coords and draw a mapBox. */

void loadLinkedFeaturesWithLoaders(struct track *tg, struct slList *(*itemLoader)(char **row),
				   struct linkedFeatures *(*lfFromWhatever)(struct slList *item),
				   char *scoreColumn, char *moreWhere,
                                   boolean (*itemFilter)(struct slList *item));
/* Make a linkedFeatures loader by providing three functions: (1) a regular */
/* item loader found in all autoSql modules, (2) a custom myStruct->linkedFeatures */
/* translating function, and (3) a function to free the thing loaded in (1). */

struct linkedFeatures *linkedFeaturesFromGenePred(struct track *tg, struct genePred *gp, boolean extra);
/* construct a linkedFeatures object from a genePred */

struct linkedFeatures *bedMungToLinkedFeatures(struct bed **pBed, struct trackDb *tdb,
	int fieldCount, int scoreMin, int scoreMax, boolean useItemRgb);
/* Convert bed to a linkedFeature, destroying bed in the process. */

struct bigBedInterval *bigBedSelectRangeExt(struct track *track,
	char *chrom, int start, int end, struct lm *lm, int maxItems);
/* Return list of intervals in range. */

#define bigBedSelectRange(track, chrom, start, end, lm)  \
    bigBedSelectRangeExt(track, chrom,start, end, lm,  min(BIGBEDMAXIMUMITEMS, maximumTrackItems(track)))
/* Return list of intervals in range. */

void bigBedAddLinkedFeaturesFromExt(struct track *track,
	char *chrom, int start, int end, int scoreMin, int scoreMax, boolean useItemRgb,
	int fieldCount, struct linkedFeatures **pLfList, int maxItems);
/* Read in items in chrom:start-end from bigBed file named in track->bbiFileName, convert
 * them to linkedFeatures, and add to head of list. */

#define bigBedAddLinkedFeaturesFrom(track, chrom, start, end, scoreMin, scoreMax, useItemRgb, fieldCount, pLfList) \
    bigBedAddLinkedFeaturesFromExt(track, chrom, start, end, scoreMin, scoreMax, useItemRgb, fieldCount, pLfList, min(BIGBEDMAXIMUMITEMS, maximumTrackItems(track))) 

char *bigBedItemName(struct track *tg, void *item) ;
// return label for simple beds

char *bigLfItemName(struct track *tg, void *item);
// return label for linked features

char *makeLabel(struct track *track,  struct bigBedInterval *bb);
// Build a label for a bigBedTrack from the requested label fields.
//
boolean canDrawBigBedDense(struct track *tg);
/* Return TRUE if conditions are such that can do the fast bigBed dense data fetch and
 * draw. */

void bigBedDrawDense(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color);
/* Use big-bed summary data to quickly draw bigBed. */

void adjustBedScoreGrayLevel(struct trackDb *tdb, struct bed *bed, int scoreMin, int scoreMax);
/* For each distinct trackName passed in, check cart for trackName.minGrayLevel; if
 * that is different from the gray level implied by scoreMin's place in [0..scoreMax],
 * then linearly transform bed->score from the range of [scoreMin,scoreMax] to
 * [(cartMinGrayLevel*scoreMax)/maxShade,scoreMax].
 * Note: this assumes that scoreMin and scoreMax are constant for each track. */

struct linkedFeatures *lfFromBedExtra(struct bed *bed, int scoreMin, int scoreMax);
/* Return a linked feature from a (full) bed. */

struct linkedFeatures *lfFromBed(struct bed *bed);
/* Return a linked feature from a (full) bed. */

void loadSimpleBedAsLinkedFeaturesPerBase(struct track *tg);
/* bed list not freed as pointer to it is stored in 'original' field */

void loadSimpleBed(struct track *tg);
/* Load the items in one track - just move beds in window... */

void loadSimpleBedWithLoader(struct track *tg, bedItemLoader loader);
/* Load the items in one track using specified loader - just move beds in window... */

void loadBed8(struct track *tg);
/* Convert bed 8 info in window to linked feature. */

void loadBed9(struct track *tg);
/* Convert bed 9 info in window to linked feature.  (to handle itemRgb)*/

void loadGappedBed(struct track *tg);
/* Convert bed info in window to linked feature. */

void linkedFeaturesFreeList(struct linkedFeatures **pList);
/* Free up a linked features list. */

void freeLinkedFeaturesSeries(struct linkedFeaturesSeries **pList);
/* Free up a linked features series list. */

int simpleFeatureCmp(const void *va, const void *vb);
/* Compare to sort based on start. */

int linkedFeaturesCmpScore(const void *va, const void *vb);
/* Help sort linkedFeatures by score (descending), then by starting pos. */

int linkedFeaturesCmp(const void *va, const void *vb);
/* Compare to sort based on chrom,chromStart. */

int linkedFeaturesCmpName(const void *va, const void *vb);
/* Help sort linkedFeatures by name. */

int linkedFeaturesCmpStart(const void *va, const void *vb);
/* Help sort linkedFeatures by starting pos. */

void linkedFeaturesBoundsAndGrays(struct linkedFeatures *lf);
/* Calculate beginning and end of lf from components, etc. */

int lfCalcGrayIx(struct linkedFeatures *lf);
/* Calculate gray level from components. */

void linkedFeaturesDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis);
/* Draw linked features items. */

void linkedFeaturesAverageDense(struct track *tg,
	int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis);
/* Draw dense linked features items. */

void linkedFeaturesAverageDenseOrientEst(struct track *tg,
	int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis);
/* Draw dense linked features items. */

void linkedFeaturesMethods(struct track *tg);
/* Fill in track group methods for linked features.
 * Many other methods routines will call this first
 * to get a reasonable set of defaults. */

Color lfChromColor(struct track *tg, void *item, struct hvGfx *hvg);
/* Return color of chromosome for linked feature type items
 * where the chromosome is listed somewhere in the lf->name. */

char *lfMapNameFromExtra(struct track *tg, void *item);
/* Return map name of item from extra field. */

char *linkedFeaturesName(struct track *tg, void *item);
/* Return name of item. */

int getFilterColor(char *type, int colorIx);
/* Get color corresponding to type - MG_RED for "red" etc. */

void spreadBasesString(struct hvGfx *hvg, int x, int y, int width, int height,
	Color color, MgFont *font, char *s, int count, bool isCodon);
/* Draw evenly spaced base letters in string. */

void spreadAlignString(struct hvGfx *hvg, int x, int y, int width, int height,
                       Color color, MgFont *font, char *s,
                       char *match, int count, bool dots, bool isCodon);
/* Draw evenly spaced letters in string.  For multiple alignments,
 * supply a non-NULL match string, and then matching letters will be colored
 * with the main color, mismatched letters will have alt color.
 * Draw a vertical bar in light yellow where sequence lacks gaps that
 * are in reference sequence (possible insertion) -- this is indicated
 * by an escaped ('/') insert count in the sequence.
 * If "dots" is set, matching bases are displayed as a dot. */

void spreadAlignStringProt(struct hvGfx *hvg, int x, int y, int width, int height,
                        Color color, MgFont *font, char *s,
                        char *match, int count, bool dots, bool isCodon, int initialColorIndex, int mafOrigOffset);
/* similar to spreadAlignString, but it is used for protein sequences. */

void contigMethods(struct track *tg);
/* Make track for contig */

void goldMethods(struct track *tg);
/* Make track for golden path (assembly track). */

void cgapSageMethods(struct track *tg);
/* Make CGAP SAGE track. */

#define CGAP_SAGE_DENSE_GOVERNOR 3000000
/* Size of browser window to dense the CGAP SAGE track at. */

void coverageMethods(struct track *tg);
/* Make track for golden path positions of all frags. */

void cytoBandIdeoMethods(struct track *tg);
/* Draw ideogram of chromosome. */

void cytoBandMethods(struct track *tg);
/* Make track for simple repeats. */

#ifdef USE_HAL
void halSnakeMethods(struct track *track, struct trackDb *tdb,
                                int wordCount, char *words[]);
/* Make track group for hal-based snake alignment. */
#endif

void longRangeMethods(struct track *track, struct trackDb *tdb);
/* Make track group for long range connections . */

void snakeMethods(struct track *track, struct trackDb *tdb,
                                int wordCount, char *words[]);
/* Make track group for chain-based snake alignment. */

void quickLiftChainMethods(struct track *tg, struct trackDb *tdb,
	int wordCount, char *words[]);
/* Fill in custom parts of quickLift chains */

void chainMethods(struct track *track, struct trackDb *tdb,
                                int wordCount, char *words[]);
/* Make track group for chain alignment. */

void netMethods(struct track *tg);
/* Make track group for chain/net alignment. */

void mafMethods(struct track *tg);
/* Make track group for maf multiple alignment. */

void bamMethods(struct track *track);
/* Methods for BAM alignment files. */

void vcfPhasedMethods(struct track *track);
/* Load items from a VCF of one individuals phased genotypes */

void vcfTabixMethods(struct track *track);
/* Methods for Variant Call Format compressed & indexed by tabix. */

void vcfMethods(struct track *track);
/* Methods for Variant Call Format. */

void altGraphXMethods(struct track *tg);
/* setup special methods for altGraphX track */

void wabaMethods(struct track *tg);
/* Return track with fields shared by waba-based
 * alignment tracks filled in. */

void axtMethods(struct track *tg, char *otherDb);
/* Make track group for axt alignments. */

void repeatMethods(struct track *tg);
/* Make track for repeats. */

void affyTransfragsMethods(struct track *tg);
/* Substitute a new load method that filters based on score. Also add
   a new itemColor() method that draws transfrags that overlap dups
   and pseudoGenes in a different color. */

struct repeatItem
/* A repeat track item. */
    {
    struct repeatItem *next;
    char *class;
    char *className;
    int yOffset;
    Color color;
    };

void pslMethods(struct track *track, struct trackDb *tdb,
	int argc, char *argv[]);
/* Load up psl type methods. */

void loadXenoPsl(struct track *tg);
/* Load a xeno psl */

void loadProteinPsl(struct track *tg);
/* Load a protein psl */

struct linkedFeatures *lfFromPslx(struct psl *psl,
	int sizeMul, boolean isXeno, boolean nameGetsPos, struct track *tg);
/* Create a linked feature item from pslx.  Pass in sizeMul=1 for DNA,
 * sizeMul=3 for protein.
 * Don't free psl afterwards!  (may be used by baseColor code) */


struct simpleFeature *sfFromPslX(struct psl *psl,int grayIx, int
                sizeMul);

struct linkedFeatures *lfFromPsl(struct psl *psl, boolean isXeno);
/* Create a linked feature item from psl.
 * Don't free psl afterwards!  (may be used by baseColor code) */

struct linkedFeatures *lfFromPslsWScoresInRange(char *table, int start, int end, char *chromName, boolean isXeno, float maxScore);
/* Return linked features from range of table with the scores scaled appropriately */

void ctWigLoadItems(struct track *tg);
/*	load custom wiggle track data	*/
void wigLoadItems(struct track *tg);
/*	load wiggle track data from database	*/
void wigMethods(struct track *track, struct trackDb *tdb,
                                int wordCount, char *words[]);
/* Set up wig pointers and do some other precalculations on a wig type track. */
void bedGraphMethods(struct track *track, struct trackDb *tdb,
	int wordCount, char *words[]);
void bigWigMethods(struct track *track, struct trackDb *tdb,
	int wordCount, char *words[]);
/* Make track group for wig - wiggle tracks. */

void mathWigMethods(struct track *track, struct trackDb *tdb, 
	int wordCount, char *words[]);
/* mathWig load and draw methods. */

void bigRmskMethods(struct track *track, struct trackDb *tdb,
                                int wordCount, char *words[]);
/* Set up bigRmsk methods. */

void commonBigBedMethods(struct track *track, struct trackDb *tdb,
                                int wordCount, char *words[]);
/* Set up common bigBed methods used by several track types that depend on the bigBed format. */

void bigBedMethods(struct track *track, struct trackDb *tdb,
                                int wordCount, char *words[]);
/* Set up bigBed methods for tracks that are type bigBed. */

void chromGraphMethods(struct track *tg);
/* Fill in chromGraph methods for built in track. */

void chromGraphMethodsCt(struct track *tg);
/* Fill in chromGraph methods for custom track. */

void factorSourceMethods(struct track *track);
/* Set up special methods for factorSource type tracks. */

void makeItemsMethods(struct track *track);
/* Set up special methods for makeItems type tracks. */

void makeItemsJsCommand(char *command, struct track *trackList, struct hash *trackHash);
/* Execute some command sent to us from the javaScript.  All we know for sure is that
 * the first word of the command is "makeItems."  We expect it to be of format:
 *    makeItems <trackName> <chrom> <chromStart> <chromEnd>
 * If it is indeed of this form then we'll create a new makeItemsItem that references this
 * location and stick it in the named track. */

void wigMafPMethods(struct track *track, struct trackDb *tdb,
                                int wordCount, char *words[]);
void wigMafMethods(struct track *track, struct trackDb *tdb,
                                int wordCount, char *words[]);
int wigTotalHeight(struct track *tg, enum trackVisibility vis);

/* Wiggle track will use this to figure out the height they use
   as defined in the cart */
/* Make track for wig - wiggle tracks. */

/* Make track group for maf track with wiggle. */


void rnaSecStrMethods(struct track *tg);
/* Make track which visualizes RNA secondary structure annotation. */

void sampleMethods(struct track *track, struct trackDb *tdb, int wordCount, char *words[]);
/* Load up methods for a generic sample type track. */

int sampleUpdateY( char *name, char *nextName, int lineHeight );
/* only increment height when name root (minus the period if
 * there is one) is different from previous one.
  *This assumes that the entries are sorted by name as they would
  *be if loaded by hgLoadSample*/

void samplePrintYAxisLabel( struct hvGfx *hvg, int y, struct track *track, char *labelString,
        double min0, double max0 );
/*print a label for a horizontal y-axis line*/

int whichSampleBin( double num, double thisMin, double thisMax,
	double binCount );
/* Get bin value from num. */

double whichSampleNum( double bin, double thisMin, double thisMax,
	double binCount );
/* gets range nums. from bin values*/

void humMusLMethods( struct track *tg );
/* Overide the humMusL load function to look for zoomed out tracks. */

void zooMethods( struct track *tg );
/* Overide the zoo sample type load function to look for zoomed out tracks. */

void expRatioMethods(struct track *tg);
/* Set up methods for expRatio type tracks in general. */

void expRatioMethodsFromCt(struct track *tg);
/* Set up methods for expRatio type tracks from custom track. */

void loweExpRatioMethods(struct track *tg);
/* Set up methods for expRatio type tracks in general. */

void affyTranscriptomeMethods(struct track *tg);
/* Overide the load function to look for zoomed out tracks. */

void rosettaMethods(struct track *tg);
/* methods for Rosetta track using bed track */

void nci60Methods(struct track *tg);
/* set up special methods for NCI60 track and tracks with multiple
   scores in general */

void affyMethods(struct track *tg);
/* set up special methods for NCI60 track and tracks with multiple
   scores in general */

void expRatioMethodsFromDotRa(struct track *tg);
/* Special methods for tracks using new microarrayGroups.ra files. */

void affyRatioMethods(struct track *tg);
/* set up special methods for NCI60 track and tracks with multiple
   scores in general */

void affyUclaMethods(struct track *tg);
/* set up special methods for affyUcla track and tracks with multiple
   scores in general */

void affyUclaNormMethods(struct track *tg);
/* Set up special methods for the affyUcla normal tissue track
   scores in general */

void cghNci60Methods(struct track *tg);
/* set up special methods for CGH NCI60 track */

char *getOrganism(struct sqlConnection *conn, char *acc);
/* lookup the organism for an mrna, or NULL if not found.
 * WARNING: static return */

char *getOrganismShort(struct sqlConnection *conn, char *acc);
/* lookup the organism for an mrna, or NULL if not found.  This will
 * only return the genus, and only the first seven letters of that.
 * WARNING: static return */

char *getGeneName(struct sqlConnection *conn, char *acc);
/* get geneName from refLink or NULL if not found.
 * WARNING: static return */

char *refGeneName(struct track *tg, void *item);
/* Get name to use for refGene item. */

char *refGeneMapName(struct track *tg, void *item);
/* Return un-abbreviated gene name. */

#define uglyh printHtmlComment
/* Debugging aid. */

int linkedFeaturesSeriesCmp(const void *va, const void *vb);
/* Compare to sort based on chrom,chromStart. */

void lfDrawSpecialGaps(struct linkedFeatures *lf,
		       int intronGap, boolean chainLines, int gapFactor,
		       struct track *tg, struct hvGfx *hvg, int xOff, int y,
		       double scale, Color color, Color bColor,
		       enum trackVisibility vis);
/* If applicable, draw something special for the gap following this block.
 * If intronGap has been specified, draw exon arrows only if the target gap
 * length is at least intronGap.
 * If chainLines, draw a double-line gap if both target and query have a gap
 * (mismatching sequence). */

void bamWigMethods(struct track *track, struct trackDb *tdb,
	int wordCount, char *words[]);
/* Set up bamWig methods. */

void bamLinkedFeaturesDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis);
/* Draw linked features items. */

void bamLinkedFeaturesSeriesDraw(struct track *tg, int seqStart, int seqEnd,
			      struct hvGfx *hvg, int xOff, int yOff, int width,
			      MgFont *font, Color color, enum trackVisibility vis);
/* Draw BAM linked features series items. */

void chainDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis);
/* Draw chained features. This loads up the simple features from
 * the chainLink table, calls linkedFeaturesDraw, and then
 * frees the simple features again. */

void linkedFeaturesSeriesDraw(struct track *tg, int seqStart, int seqEnd,
			      struct hvGfx *hvg, int xOff, int yOff, int width,
			      MgFont *font, Color color, enum trackVisibility vis);
/* Draw linked features series items. */

struct linkedFeaturesSeries *lfsFromColoredExonBed(struct bed *bed);
/* Convert a single BED 14 thing into a special linkedFeaturesSeries */
/* where each linkedFeatures is a colored block. */

void makeRedGreenShades(struct hvGfx *hvg);
/* Makes some colors for the typical red/green microarray spectrum. */

void linkedFeaturesSeriesMethods(struct track *tg);

void lfsMapItemName(struct track *tg, struct hvGfx *hvg, void *item, char *itemName,
                    char *mapItemName, int start, int end,
		    int x, int y, int width, int height);

struct track *trackFromTrackDb(struct trackDb *tdb);
/* Create a track based on the tdb */

int spreadStringCharWidth(int width, int count);

Color getOrangeColor();
/* Return color used for insert indicators in multiple alignments */

Color getBlueColor();
Color getChromBreakBlueColor();
Color getChromBreakGreenColor();

void linkedFeaturesDrawAt(struct track *tg, void *item,
                          struct hvGfx *hvg, int xOff, int y, double scale,
                          MgFont *font, Color color, enum trackVisibility vis);
/* Draw a single simple bed item at position. */

Color lighterColor(struct hvGfx *hvg, Color color);
/* Get lighter shade of a color */

struct track *chromIdeoTrack(struct track *trackList);
/* Find chromosome ideogram track */

void setRulerMode();
/* Set the rulerMode variable from cart. */


#define configHideAll "hgt_doConfigHideAll"
#define configShowAll "hgt_doConfigShowAll"
#define configDefaultAll "hgt_doDefaultShowAll"
#define configHideAllGroups "hgt_doConfigHideAllGroups"
#define configShowAllGroups "hgt_doConfigShowAllGroups"
#define configHideEncodeGroups "hgt_doConfigHideEncodeGroups"
#define configShowEncodeGroups "hgt_doConfigShowEncodeGroups"
#define configGroupTarget "hgt_configGroupTarget"
#define configPriorityOverride "hgt_priorityOverride"
#define hgtJsCommand "hgt_doJsCommand"

void doMiddle(struct cart *theCart);
/* Print the body of html file.   */

void initTl();
/* Initialize layout around small font and a picture about 800 pixels
 * wide. */

void setLayoutGlobals();
/* Figure out basic dimensions of display.  */

struct hash *makeGlobalTrackHash(struct track *trackList);
/* Create a global track hash and returns a pointer to it. */

void makeActiveImage(struct track *trackList, char *psOutput);
/* Make image and image map. */

void configPage();
/* Put up configuration page. */

void configPageSetTrackVis(int vis);
/* Do config page after setting track visibility. If vis is -2, then visibility
 * is unchanged.  If -1 then set visibility to default, otherwise it should
 * be tvHide, tvDense, etc. */

void configMultiRegionPage();
/* Put up multi-region configuration page. */

struct track *trackNew();
/* Allocate track . */

void bedMethods(struct track *tg);
/* Fill in methods for (simple) bed tracks. */

void bed9Methods(struct track *tg);
/* Fill in methods for bed9 tracks. */

void complexBedMethods(struct track *track, struct trackDb *tdb, boolean isBigBed,
                                int wordCount, char *words[]);
/* Fill in methods for more complex bed tracks. */

void makeCompositeTrack(struct track *track, struct trackDb *tdb);
/* Construct track subtrack list from trackDb entry.
 * Sets up color gradient in subtracks if requested */

void makeContainerTrack(struct track *track, struct trackDb *tdb);
/* Construct track subtrack list from trackDb entry for container tracks. */

bool isSubtrackVisible(struct track *tg);
/* Should this subtrack be displayed? */

void compositeTrackVis(struct track *track);
/* set visibilities of subtracks */

boolean isWithCenterLabels(struct track *track);
/* Special cases: inhibit center labels of subtracks in dense mode, and
 * of composite track in non-dense mode.
 * BUT if track->tdb has a centerLabelDense setting, let subtracks go with
 * the default and inhibit composite track center labels in all modes.
 * Otherwise use the global boolean withCenterLabels. */

boolean isCenterLabelConditional(struct track *track);
/* Dense subtracks and pack subtracks (when centerLabelsPack off set)
 *      show center labels depending on vis of previous track */

boolean isCenterLabelConditionallySeen(struct track *track);
/* Returns FALSE if track and prevTrack have same parent, and are both conditional
 * i.e. dense subtrack or pack subtrack with centerLabelsPack off set
 */

boolean isCenterLabelsPackOff(struct track *track);
/* Check for trackDb setting to suppress center labels of composite in pack mode */

boolean isCenterLabelIncluded(struct track *track);
/* Center labels may be conditionally included */

boolean doHideEmptySubtracks(struct track *track, char **multiBedFile, char **subtrackIdFile);
/* Suppress display of empty subtracks. Initial support only for bed's. */

Color maybeDarkerLabels(struct track *track, struct hvGfx *hvg, Color color);
/* For tracks having light track display but needing a darker label */

void affyTxnPhase2Methods(struct track *track);
/* Methods for dealing with a composite transcriptome tracks. */

void loadGenePred(struct track *tg);
/* Convert gene pred in window to linked feature. */

void genePredAssignConfiguredName(struct track *tg);
/* Set name on genePred in "extra" field to gene name, accession, or both,
 * depending, on UI on all items in track */

void loadGenePredWithConfiguredName(struct track *tg);
/* Convert gene pred info in window to linked feature. Include name
 * in "extra" field (gene name, accession, or both, depending on UI) */

boolean highlightItem(struct track *tg, void *item);
/* Should this item be highlighted? */

void linkedFeaturesSeriesDrawAt(struct track *tg, void *item,
        struct hvGfx *hvg, int xOff, int y, double scale,
	MgFont *font, Color color, enum trackVisibility vis);
/* Draw a linked features series item at position. */

#define NEXT_ITEM_ARROW_BUFFER 1
/* Space around "next item" arrow (in pixels). */

void addWikiTrack(struct track **pGroupList);
/* Add wiki track and append to group list. */

void wikiTrackMethods(struct track *tg);
/* establish loadItems function for wiki track */

struct bed *wikiTrackGetBedRange(char *mapName, char *chromName,
	int start, int end);
/* fetch wiki track items as simple bed 3 list in given range */

void addVariomeWikiTrack(struct track **pGroupList);
/* Add variome wiki track and append to group list. */

void bed8To12(struct bed *bed);
/* Turn a bed 8 into a bed 12 by defining one block. */

char *collapseGroupVar(char *name);
/* Construct cart variable name for collapsing group */

boolean isCollapsedGroup(struct group *grp);
/* Determine if group is collapsed */

void collapseGroupGoodies(boolean isOpen, boolean wantSmallImage,
                            char **img, char **indicator, char **otherState);
/* Get image, char representation of image, and 'otherState' (1 or 0)
 * for a group, based on whether it is collapsed, and whether we want
 * larger or smaller image for collapse box */

void parseSs(char *ss, char **retPsl, char **retFa);
/* Parse out ss variable into components. */

boolean ssFilesExist(char *ss);
/* Return TRUE if both files in ss exist. */

int maximumTrackItems(struct track *tg);
/* Return the maximum number of items allowed in track. */

int maximumTrackHeight(struct track *tg);
/* Return the maximum track height allowed in pixels. */

struct dyString *uiStateUrlPart(struct track *toggleGroup);
/* Return a string that contains all the UI state in CGI var
 * format.  If toggleGroup is non-null the visibility of that
 * track will be toggled in the string. */

boolean nextItemCompatible(struct track *tg);
/* Check to see if we draw nextPrev item buttons on a track. */

void linkedFeaturesLabelNextPrevItem(struct track *tg, boolean next);
/* Default next-gene function for linkedFeatures.  Changes winStart/winEnd
 * and updates position in cart. */

void createHgFindMatchHash();
/* Read from the cart the string assocated with matches and
   put the matching items into a hash for highlighting later. */

TrackHandler lookupTrackHandlerClosestToHome(struct trackDb *tdb);
/* Lookup handler for track of give name.  Try parents if
 * subtrack has a NULL handler.  Return NULL if none. */

void registerTrackHandlers();
/* Register tracks that include some non-standard methods. */

void initColors(struct hvGfx *hvg);
/* Initialize the shades of gray etc. */

void findTrackColors(struct hvGfx *hvg, struct track *trackList);
/* Find colors to draw in. */

char *getItemDataName(struct track *tg, char *itemName);
/* Translate an itemName to its itemDataName, using tg->itemDataName if is not
 * NULL. The resulting value should *not* be freed, and it should be assumed
 * that it will only remain valid until the next call of this function.*/

void registerTrackHandler(char *name, TrackHandler handler);
/* Register a track handling function. */

void doSearchTracks(struct group *groupList);

boolean superTrackHasVisibleMembers(struct trackDb *tdb);

enum trackVisibility limitedVisFromComposite(struct track *subtrack);
/* returns the subtrack visibility which may be limited by composite with multi-view dropdowns. */

INLINE enum trackVisibility actualVisibility(struct track *track)
// return actual visibility for this track (limited to limitedVis if appropriate)
{
return track->limitedVisSet ? track->limitedVis : track->visibility;
}

char *getScoreFilterClause(struct cart *cart,struct trackDb *tdb,char *scoreColumn);
// Returns "score >= ..." extra where clause if one is needed

/* useful for declaring small arrays */
#define SMALLBUF 128
#define LARGEBUF 256
/* and for dyStringNew */
#define SMALLDYBUF 64

char *trackUrl(char *mapName, char *chromName);
/* Return hgTrackUi url; chromName is optional. */

void bedDetailCtMethods (struct track *tg, struct customTrack *ct);
/* Load bedDetail track from custom tracks as bed or linked features */

void pgSnpMethods (struct track *tg);
/* Personal Genome SNPs: show two alleles with stacked color bars for base alleles and
 * (if available) allele counts in mouseover. */

int pgSnpHeight(struct track *tg, enum trackVisibility vis);

void pgSnpCtMethods (struct track *tg);
/* Load pgSnp track from custom tracks */

void pgSnpMapItem(struct track *tg, struct hvGfx *hvg, void *item, char *itemName,
		  char *mapItemName, int start, int end, int x, int y, int width, int height);
/* create a special map box item with different pop-up statusLine with allele counts */

void gvfMethods(struct track *tg);
/* Load GVF variant data. */

void gtexGeneMethods(struct track *tg);
/* Gene-Tissue Expression (GTEX) gene track*/

void messageLineMethods(struct track *track);
/* Methods for drawing a single-height message line instead of track items,
 * e.g. if source was compiled without a necessary library. */

void rmskJoinedMethods(struct track *track);
/* construct track for detailed repeat visualization */

void lrgMethods(struct track *tg);
/* Locus Reference Genomic (bigBed 12 +) handlers. */

void peptideAtlasMethods(struct track *tg);
/* PeptideAtlas (bed 12+) handlers */

void barChartMethods(struct track *tg);
/* Bar Chart track type: draw fixed width chart of colored bars over a BED item */

void barChartCtMethods(struct track *tg);
/* Bar Chart track methods for custom track */

void gtexEqtlClusterMethods(struct track *tg);
/* GTEx eQTL Cluster (bigBed 5 +) tracks */

void gtexEqtlTissueMethods(struct track *tg);
/* Install handler for GTEx eQTL Tissues track */

void instaPortMethods(struct track *track, struct trackDb *tdb,
                                int wordCount, char *words[]);
/* instaPort track type methods */

void bigBaseViewMethods(struct track *track, struct trackDb *tdb,
                                int wordCount, char *words[]);
/* bigBaseView track type methods */

void lollyMethods(struct track *track, struct trackDb *tdb,
                                int wordCount, char *words[]);
/* Lollipop track type methods */

void interactMethods(struct track *tg);
/* Interact track type methods */

void interactCtMethods(struct track *tg);
/* Interact track methods for custom track */

void hicMethods(struct track *tg);
/* Methods for Hi-C interaction data */

void hicCtMethods(struct track *tg);
/* Hi-C track methods for custom track */

void bedMethylCtMethods(struct track *tg);
/* bedMethyl track methods for custom track */

void loraxMethods(struct track *tg);
/* Lorax track type methods */

void parentChildCartCleanup(struct track *trackList,struct cart *newCart,struct hash *oldVars);
/* When composite/view settings changes, remove subtrack specific vis
   When superTrackChild is found and selected, shape superTrack to match. */

void dontLoadItems(struct track *tg);
/* No-op loadItems when we aren't going to try. */

void filterItems(struct track *tg, boolean (*filter)(struct track *tg, void *item), 
                char *filterType);
/* Filter out items from track->itemList. */

void filterItemsOnNames(struct track *tg);
/* Only keep items with a name in the .nameFilter cart var. 
 * Not using filterItems(), because filterItems has no state at all. */

int gCmpPriority(const void *va, const void *vb);
/* Compare groups based on priority. */

int tgCmpPriority(const void *va, const void *vb);
/* Compare to sort based on priority; use shortLabel as secondary sort key. */

void printMenuBar();
/* Put up the menu bar. */

boolean winTooBigDoWiggle(struct cart *cart, struct track *tg);
/* return true if we wiggle because the window size exceeds a certain threshold */

boolean checkIfWiggling(struct cart *cart, struct track *tg);
/* Check to see if a track should be drawing as a wiggle. */

boolean isTypeBedLike(struct track *track);
/* Check if track type is BED-like packable thing (but not rmsk or joinedRmsk) */

boolean isTypeUseItemNameAsKey(struct track *track);
/* Check if track type is like expRatio and key is just item name. */

boolean isTypeUseMapItemNameAsKey(struct track *track);
/* Check if track type is like interact and uses map item name to link across multi regions */

void setEMGeneTrack();
/* Find the track for the gene table to use for exonMostly and geneMostly. */

void findBestEMGeneTable(struct track *trackList);
/* Find the best gene table to use for exonMostly */

struct window *makeWindowListFromVirtChrom(long virtWinStart, long virtWinEnd);
/* make list of windows from virtual position on virtualChrom */

struct convertRange
    {
    struct convertRange *next;
    char *chrom;
    int start;
    int end;
    long vStart;
    long vEnd;
    boolean found;
    boolean skipIt;
    };


void linkedFeaturesNextPrevExonFind(struct track *tg, boolean next, struct convertRange *crList);
/* Find next-exon function for linkedFeatures.  Finds corresponding position on virtual chrom for new winStart/winEnd of exon non-virt position,
 * and returns it. This function was cloned from linkedFeaturesLabelNextPrevItem and modified. */

boolean virtualSingleChrom();
/* Return TRUE if using virtual single chromosome mode */

void parseVPosition(char *position, char **pChrom, long *pStart, long *pEnd);
/* parse Virt position */

char *undisguisePosition(char *position); // UN-DISGUISE VMODE
/* Find the virt position
 * position should be real chrom span. 
 * Limitation: can only convert things in the current windows set. */


char *disguisePositionVirtSingleChrom(char *position); // DISGUISE VMODE
/* Hide the virt position, convert to real single chrom span.
 * position should be virt chrom span. 
 * Can handle anything in the virt single chrom. */

#define measureTime uglyTime

#define SUPPORT_CONTENT_TYPE 1

struct bbiFile *fetchBbiForTrack(struct track *track);
/* Fetch bbiFile from track, opening it if it is not already open. */

void genericDrawNextItem(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y,
                            double scale, Color color, enum trackVisibility vis);
/* Draw next item buttons and map boxes */

struct spaceSaver *findSpaceSaver(struct track *tg, enum trackVisibility vis);
/* Find SpaceSaver in list. Return spaceSaver found or NULL. */

void labelTrackAsFilteredNumber(struct track *tg, unsigned numOut);
/* add text to track long label to indicate filter is active and how many items were deleted */

void labelTrackAsFiltered(struct track *tg);
/* add text to track long label to indicate filter is active */

void labelTrackAsHideEmpty(struct track *tg);
/* add text to track long label to indicate empty subtracks are hidden */

void labelTrackAsDensity(struct track *tg);
/* Add text to track long label to indicate density mode */

void labelTrackAsDensityWindowSize(struct track *tg);
/* Add text to track long label to indicate density mode because window size exceeds some threshold */

void setupHotkeys(boolean gotExtTools);
/* setup keyboard shortcuts and a help dialog for it */

void calcWiggleOrdering(struct cart *cart, struct flatTracks *flatTracks);

void bedPlusLabelDrawAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y,
			       double scale, MgFont *font, Color color, enum trackVisibility vis);
/* Draw a single bed item at position.  If vis is full, draw the associated label to the left
 * of the item. */

Color blackItemNameColor(struct track *tg, void *item, struct hvGfx *hvg);
/* Force item name (label) color to black */

void linkedFeaturesMapItem(struct track *tg, struct hvGfx *hvg, void *item,
				char *itemName, char *mapItemName, int start, int end,
				int x, int y, int width, int height);

boolean recTrackSetsEnabled();
/* Return TRUE if feature is available */

boolean recTrackSetsChangeDetectEnabled();
/* Return TRUE if feature is available, in hgConf */

int recTrackSetsForDb();
/* Return number of recommended track sets for this database */

boolean hasRecTrackSet(struct cart *cart);
/* Check if currently loaded session is in the recommended track set */

void printRecTrackSets();
/* Create dialog with list of recommended track sets */

Color colorFromSoTerm(enum soTerm term);
/* Assign a Color according to soTerm: red for non-synonymous, green for synonymous, blue for
 * UTR/noncoding, black otherwise. */

void maybeNewFonts(struct hvGfx *hvg);
/* Check to see if we want to use the alternate font engine (FreeType2). */

Color colorFromCart(struct track *tg, Color color);
/* Return the RGB color from the cart setting 'colorOverride' or just return color */

unsigned getParaLoadTimeout();
// get the parallel load timeout in seconds (defaults to 90)

void maybeDrawQuickLiftLines( struct track *tg, int seqStart, int seqEnd,
                      struct hvGfx *hvg, int xOff, int yOff, int width,
                      MgFont *font, Color color, enum trackVisibility vis);
/* Draw the indel regions as a highlight. */

unsigned findBiggest(unsigned num);
/* find biggest font not bigger than num */
#endif /* HGTRACKS_H */

