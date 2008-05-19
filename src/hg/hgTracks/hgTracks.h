/* hgTracks.h - stuff private to hgTracks, but shared with
 * individual tracks. */

#ifndef HGTRACKS_H
#define HGTRACKS_H

#ifndef HVGFX_H
#include "hvGfx.h"
#endif

#ifndef HUI_H
#include "hui.h"
#endif

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

#ifndef ITEMATTR_H
#include "itemAttr.h"
#endif /* ITEMATTR_H */

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

struct track
/* Structure that displays of tracks. The central data structure
 * of the graphical genome browser. */
    {
    struct track *next;   /* Next on list. */
    char *mapName;             /* Database track name. Name on image map etc. */
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

    char *(*itemName)(struct track *tg, void *item);
    /* Return name of one of an item to display on left side. */

    char *(*mapItemName)(struct track *tg, void *item);
    /* Return name to associate on map. */

    int (*totalHeight)(struct track *tg, enum trackVisibility vis);
	/* Return total height. Called before and after drawItems. 
	 * Must set the following variables. */
    int height;                /* Total height - must be set by above call. */
    int lineHeight;            /* Height per track including border. */
    int heightPer;             /* Height per track minus border. */

    int (*itemHeight)(struct track *xtg, void *item);
    /* Return height of one item. */

    int (*itemRightPixels)(struct track *tg, void *item);
    /* Return number of pixels needed to right of item for additional labeling. (Optional) */

    void (*drawItems)(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width, 
	MgFont *font, Color color, enum trackVisibility vis);
    /* Draw item list, one per track. */

    void (*drawItemAt)(struct track *tg, void *item, struct hvGfx *hvg, 
        int xOff, int yOff, double scale, 
	MgFont *font, Color color, enum trackVisibility vis);
    /* Draw a single option.  This is optional, but if it's here
     * then you can plug in genericDrawItems into the drawItems,
     * which takes care of all sorts of things. */

    int (*itemStart)(struct track *tg, void *item);
    /* Return start of item in base pairs. */

    int (*itemEnd)(struct track *tg, void *item);
    /* Return end of item in base pairs. */

    void (*freeItems)(struct track *tg);
    /* Free item list. */

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
    void *extraUiData;	/* Pointer for track specific filter etc. data. */

    void (*trackFilter)(struct track *tg);	
    /* Stuff to handle user interface parts. */

    void *customPt;  /* Misc pointer variable unique to track. */
    int customInt;   /* Misc int variable unique to track. */
    int subType;     /* Variable to say what subtype this is for similar tracks
	              * to share code. */

    float minRange, maxRange;	  /*min and max range for sample tracks 0.0 to 1000.0*/
    float scaleRange;             /* What to scale samples by to get logical 0-1 */

    int bedSize;		/* Number of fields if a bed file. */

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

    int sourceCount;	/* Number of sources for factorSource tracks. */
    struct expRecord **sources;  /* Array of sources */
    int sourceRightPixels;	/* Number of pixels to right we'll need. */

    boolean exonArrows;	/* Draw arrows on exons? */
    boolean exonArrowsAlways;	/* Draw arrows on exons even with introns showing? */
    boolean nextItemButtonable; /* Use the next-exon buttons? */ 
    boolean labelNextItemButtonable; /* Use the next-gene buttons? */ 
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

    void (*nextPrevItem)(struct track *tg, struct hvGfx *hvg, void *item, int x, int y, int w, int h, boolean next);    
    /* Function will draw the button on a track item and assign a map */
    /* box to it as well, so that a click will move the browser window */
    /* to the next (or previous if next==FALSE) item. This is meant to */
    /* jump to parts of an item already partially in the window but is */
    /* hanging off the edge... e.g. the next exon in a gene. */ 

    void (*labelNextPrevItem)(struct track *tg, boolean next);
    /* If this function is given, it can dictate where the browser loads */
    /* up based on whether a next-item button on the longLabel line of */
    /* the track was pressed (as opposed to the next-item buttons on the */
    /* track items themselves... see nextPrevItem() ). This is meant for */
    /* going to the next/previous item currently unseen in the browser, */
    /* e.g. the next gene. SO FAR THIS IS UNIMPLEMENTED. */

    char *(*itemDataName)(struct track *tg, char *itemName);
    /* If not NULL, function to translated an itemName into a data name.
     * This is can be used for looking up sequence, CDS, etc. It is used
     * to support item names that have uniqueness identifiers added to deal
     * with multiple alignments.  The resulting value should *not* be freed,
     * and it should be assumed that it might only remain valid for a short
     * period of time.*/

    int loadTime;	/* Time it takes to load (for performance tuning) */
    int drawTime;	/* Time it takes to draw (for performance tuning) */
    };


typedef void (*TrackHandler)(struct track *tg);

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
    };

struct simpleFeature
/* Minimal feature - just stores position in browser coordinates. */
    {
    struct simpleFeature *next;
    int start, end;			/* Start/end in browser coordinates. */
    int qStart, qEnd;			/* query start/end */
    int grayIx;                         /* Level of gray usually. */
    };

/* Some details of how to draw linked features. */
enum {lfSubXeno = 1};
enum {lfSubSample = 2};
enum {lfWithBarbs = 3}; /* Turn on barbs to show direction based on 
                         * strand field */
enum {lfSubChain = 4};
enum {lfNoIntronLines = 5}; /* Draw no lines between exon blocks */

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
    char name[64];			/* Accession of query seq. */
    int orientation;                    /* Orientation. */
    struct simpleFeature *components;   /* List of component simple features. */
    struct simpleFeature *codons;       /* If zoomed to CDS or codon level.*/
    void *extra;			/* Extra info that varies with type. */
    void *original;			/* The structure that was converted 
					   into this (when needed later).  */
    struct itemAttr *itemAttr;          /* itemAttr object for this lf, or NULL */
    char popUp[128];			/* text for popup */
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

/* global GSID subject list */
struct gsidSubj
    {
    struct gsidSubj  *next;
    char *subjId;
    };

/* global GSID sequence list */
struct gsidSeq
    {
    struct gsidSeq  *next;
    char *seqId;
    char *subjId;
    };

extern struct trackLayout tl;

extern struct cart *cart; /* The cart where we keep persistent variables. */
extern struct track *trackList;    /* List of all tracks. */
struct hash *trackHash; /* Hash of the tracks by their name. */
extern char *chromName;	  /* Name of chromosome sequence . */
extern char *database;	  /* Name of database we're using. */
extern char *organism;	  /* Name of organism we're working on. */
extern char *browserName;              /* Test or public browser */
extern char *organization;             /* UCSC or MGC */
extern int winStart;	  /* Start of window in sequence. */
extern int winEnd;	  /* End of window in sequence. */
extern int maxItemsInFullTrack;  /* Maximum number of items displayed in full */
extern char *position; 		/* Name of position. */
extern int trackTabWidth;
extern int gfxBorder;		/* Width of graphics border. */
extern int insideWidth;		/* Width of area to draw tracks in in pixels. */
extern int insideX;		/* Start of area to draw track in in pixels. */
extern int leftLabelX;		/* Start of area to draw left labels on. */
extern int leftLabelWidth;	/* Width of area to draw left labels on. */
extern boolean withLeftLabels;		/* Display left labels? */
extern boolean withCenterLabels;	/* Display center labels? */
extern boolean withGuidelines;		/* Display guidelines? */
extern boolean withNextExonArrows;	/* Display next exon navigation buttons near center labels? */

extern int seqBaseCount;  /* Number of bases in sequence. */
extern int winBaseCount;  /* Number of bases in window. */
extern float basesPerPixel;       /* bases covered by a pixel; a measure of zoom */
extern boolean zoomedToBaseLevel; /* TRUE if zoomed so we can draw bases. */
extern boolean zoomedToCodonLevel; /* TRUE if zoomed so we can print codon text in genePreds*/
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
extern struct rgbColor undefinedYellowColor;

extern Color shadesOfSea[10+1];       /* Ten sea shades. */

/* 16 shades from black to fully saturated of red/green/blue for
 * expression data. */
#define EXPR_DATA_SHADES 16
extern Color shadesOfGreen[EXPR_DATA_SHADES];
extern Color shadesOfRed[EXPR_DATA_SHADES];
extern Color shadesOfBlue[EXPR_DATA_SHADES];
extern Color shadesOfYellow[EXPR_DATA_SHADES];

extern boolean chromosomeColorsMade; /* Have the 3 shades of 8 chromosome colors been allocated? */
extern boolean exprBedColorsMade; /* Have the shades of Green, Red, and Blue been allocated? */
extern int maxRGBShade;

/* used in MAF display */
#define UNALIGNED_SEQ 'o'
#define MAF_DOUBLE_GAP '='

void abbr(char *s, char *fluff);
/* Cut out fluff from s. */

struct track *getTrackList(struct group **pGroupList, int vis);
/* Return list of all tracks, organizing by groups. 
 * If vis is -1, restore default groups to tracks. 
 * Shared by hgTracks and configure page. */

void groupTrackListAddSuper(struct cart *cart, struct group *group);
/* Construct a new track list that includes supertracks, sort by priority,
 * and determine if supertracks have visible members.
 * Replace the group track list with this new list.
 * Shared by hgTracks and configure page to expand track list,
 * in contexts where no track display functions (which don't understand
 * supertracks) are invoked.  */

void removeTrackFromGroup(struct track *track);
/* Remove track from group it is part of. */

int orientFromChar(char c);
/* Return 1 or -1 in place of + or - */

enum trackVisibility estimateVisibility(struct track *tg);
/* Return estimate of what visibility will be without setting it. */

enum trackVisibility limitVisibility(struct track *tg);
/* Return default visibility limited by number of items. */

char *hgcNameAndSettings();
/* Return path to hgc with variables to store UI settings. */

void mapBoxHc(struct hvGfx *hvg, int start, int end, int x, int y, int width, int height, 
	char *group, char *item, char *statusLine);
/* Print out image map rectangle that would invoke the htc (human track click)
 * program. */

void mapBoxReinvoke(struct hvGfx *hvg, int x, int y, int width, int height, 
		    struct track *toggleGroup, char *chrom,
		    int start, int end, char *message, char *extra);
/* Print out image map rectangle that would invoke this program again.
 * If toggleGroup is non-NULL then toggle that track between full and dense.
 * If chrom is non-null then jump to chrom:start-end.
 * Add extra string to the URL if it's not NULL */

void mapBoxToggleVis(struct hvGfx *hvg, int x, int y, int width, int height, 
	struct track *curGroup);
/* Print out image map rectangle that would invoke this program again.
 * program with the current track expanded. */

void mapBoxJumpTo(struct hvGfx *hvg, int x, int y, int width, int height, 
		  char *newChrom, int newStart, int newEnd, char *message);
/* Print out image map rectangle that would invoke this program again
 * at a different window. */

void mapBoxHgcOrHgGene(struct hvGfx *hvg, int start, int end, int x, int y, int width, int height, 
                       char *track, char *item, char *statusLine, char *directUrl, boolean withHguid,
                       char *extra);
/* Print out image map rectangle that would invoke the hgc (human genome click)
 * program. */

void genericMapItem(struct track *tg, struct hvGfx *hvg, void *item,
		    char *itemName, char *mapItemName, int start, int end,
		    int x, int y, int width, int height);
/* This is meant to be used by genericDrawItems to set to tg->mapItem in */
/* case tg->mapItem isn't set to anything already. */

void mapStatusMessage(char *format, ...);
/* Write out stuff that will cause a status message to
 * appear when the mouse is over this box. */

double scaleForWindow(double width, int seqStart, int seqEnd);
/* Return the scale for the window. */

double scaleForPixels(double pixelWidth);
/* Return what you need to multiply bases by to
 * get to scale of pixel coordinates. */

void drawScaledBox(struct hvGfx *hvg, int chromStart, int chromEnd, 
	double scale, int xOff, int y, int height, Color color);
/* Draw a box scaled from chromosome to window coordinates. 
 * Get scale first with scaleForPixels. */

Color whiteIndex();
/* Return index of white. */

Color blackIndex();
/* Return index of black. */

Color grayIndex();
/* Return index of gray. */

Color lightGrayIndex();
/* Return index of light gray. */

int grayInRange(int val, int minVal, int maxVal);
/* Return gray shade corresponding to a number from minVal - maxVal */

int pslGrayIx(struct psl *psl, boolean isXeno, int maxShade);
/* Figure out gray level for an RNA block. */

Color getSeqColor(char *name, struct hvGfx *hvg);
/* Return color index corresponding to chromosome/scaffold name. */

Color lighterColor(struct hvGfx *hvg, Color color);
/* Get lighter shade of a color */ 

Color slightlyLighterColor(struct hvGfx *hvg, Color color);
/* Get slightly lighter shade of a color */ 

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

void grayThreshold(UBYTE *pt, int count);
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

void genericDrawItems(struct track *tg, 
	int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis);
/* Draw generic item list.  Features must be fixed height
 * and tg->drawItemAt has to be filled in. */

void bedDrawSimpleAt(struct track *tg, void *item, 
	struct hvGfx *hvg, int xOff, int y, 
	double scale, MgFont *font, Color color, enum trackVisibility vis);
/* Draw a single simple bed item at position. */

typedef struct slList *(*ItemLoader)(char **row);

void bedLoadItemByQuery(struct track *tg, char *table, 
			char *query, ItemLoader loader);
/* Generic tg->item loader. If query is NULL use generic
 hRangeQuery(). */

void bedLoadItem(struct track *tg, char *table, ItemLoader loader);
/* Generic tg->item loader. */

void loadLinkedFeaturesWithLoaders(struct track *tg, struct slList *(*itemLoader)(char **row), 
				   struct linkedFeatures *(*lfFromWhatever)(struct slList *item),
				   void (*freeWhatever)(struct slList **pItem), char *scoreColumn, 
				   char *moreWhere, boolean (*itemFilter)(struct slList *item));
/* Make a linkedFeatures loader by providing three functions: (1) a regular */
/* item loader found in all autoSql modules, (2) a custom myStruct->linkedFeatures */
/* translating function, and (3) a function to free the thing loaded in (1). */

struct linkedFeatures *lfFromBedExtra(struct bed *bed, int scoreMin, int scoreMax);
/* Return a linked feature from a (full) bed. */

struct linkedFeatures *lfFromBed(struct bed *bed);
/* Return a linked feature from a (full) bed. */

void linkedFeaturesFreeList(struct linkedFeatures **pList);
/* Free up a linked features list. */

void freeLinkedFeaturesSeries(struct linkedFeaturesSeries **pList);
/* Free up a linked features series list. */

int linkedFeaturesCmp(const void *va, const void *vb);
/* Compare to sort based on chrom,chromStart. */

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

void chainMethods(struct track *track, struct trackDb *tdb, 
                                int wordCount, char *words[]);
/* Make track group for chain alignment. */

void netMethods(struct track *tg);
/* Make track group for chain/net alignment. */

void mafMethods(struct track *tg);
/* Make track group for maf multiple alignment. */

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
 * sizeMul=3 for protein. */


struct simpleFeature *sfFromPslX(struct psl *psl,int grayIx, int
                sizeMul);

struct linkedFeatures *lfFromPsl(struct psl *psl, boolean isXeno);
/* Create a linked feature item from psl. */

struct linkedFeatures *lfFromPslsWScoresInRange(char *table, int start, int end, char *chromName, boolean isXeno, float maxScore);
/* Return linked features from range of table with the scores scaled appropriately */

void ctWigLoadItems(struct track *tg);
/*	load custom wiggle track data	*/
void wigLoadItems(struct track *tg);
/*	load wiggle track data from database	*/
void wigMethods(struct track *track, struct trackDb *tdb, 
                                int wordCount, char *words[]);
void bedGraphMethods(struct track *track, struct trackDb *tdb, 
	int wordCount, char *words[]);

/* Make track group for wig - wiggle tracks. */

void chromGraphMethods(struct track *tg);
/* Fill in chromGraph methods for built in track. */

void chromGraphMethodsCt(struct track *tg);
/* Fill in chromGraph methods for custom track. */

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

char *orgShortForDb(char *db);
/* look up the short organism name given an organism db.
 * WARNING: static return */

char *orgShortName(char *org);
/* Get the short name for an organism.  Returns NULL if org is NULL.
 * WARNING: static return */

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

struct linkedFeaturesSeries *lfsFromColoredExonBed(struct bed *bed);
/* Convert a single BED 14 thing into a special linkedFeaturesSeries */
/* where each linkedFeatures is a colored block. */

void makeRedGreenShades(struct hvGfx *hvg);
/* Makes some colors for the typical red/green microarray spectrum. */

void linkedFeaturesSeriesMethods(struct track *tg);

void lfsMapItemName(struct track *tg, struct hvGfx *hvg, void *item, char *itemName, char *mapItemName, int start, int end, 
		    int x, int y, int width, int height);

void drawScaledBoxSample(struct hvGfx *hvg,
        int chromStart, int chromEnd, double scale,
        int xOff, int y, int height, Color color,
        int score);
/* Draw a box scaled from chromosome to window coordinates. */

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

void configPage();
/* Put up configuration page. */

void configPageSetTrackVis(int vis);
/* Do config page after setting track visibility. If vis is -2, then visibility 
 * is unchanged.  If -1 then set visibility to default, otherwise it should 
 * be tvHide, tvDense, etc. */

struct track *trackNew();
/* Allocate track . */

void bedMethods(struct track *tg);
/* Fill in methods for (simple) bed tracks. */

void makeCompositeTrack(struct track *track, struct trackDb *tdb);
/* Construct track subtrack list from trackDb entry.
 * Sets up color gradient in subtracks if requested */

bool isCompositeTrack(struct track *track);
/* Determine if this is a composite track. This is currently defined
 * as a top-level dummy track, with a list of subtracks of the same type.
 * Need to check trackDb, as we need to ignore wigMaf's which have
 * subtracks but aren't composites */

boolean isSubtrack(struct track *track);
/* Return TRUE if track is a subtrack of a composite track. */
/* Subtracks usually inherit their parent track's tdb, so their tdbs may 
 * appear composite, but their mapNames will not be the same as tdb->tableName 
 * in that case. */

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

void affyTxnPhase2Methods(struct track *track);
/* Methods for dealing with a composite transcriptome tracks. */

void loadGenePred(struct track *tg);
/* Convert gene pred in window to linked feature. */

boolean highlightItem(struct track *tg, void *item);
/* Should this item be highlighted? */

void linkedFeaturesSeriesDrawAt(struct track *tg, void *item, 
        struct hvGfx *hvg, int xOff, int y, double scale,
	MgFont *font, Color color, enum trackVisibility vis);
/* Draw a linked features series item at position. */

#define NEXT_ITEM_ARROW_BUFFER 5
/* Space around "next item" arrow (in pixels). */

void addWikiTrack(struct track **pGroupList);
/* Add wiki track and append to group list. */

void wikiTrackMethods(struct track *tg);
/* establish loadItems function for wiki track */

struct bed *wikiTrackGetBedRange(char *mapName, char *chromName,
	int start, int end);
/* fetch wiki track items as simple bed 3 list in given range */

void factorSourceMethods(struct track *track);
/* Set up special methods for factorSource type tracks. */

void bed8To12(struct bed *bed);
/* Turn a bed 8 into a bed 12 by defining one block. */

char *collapseGroupVar(char *name);
/* Construct cart variable name for collapsing group */

boolean isCollapsedGroup(char *name);
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

int maximumTrackHeight(struct track *tg);
/* Return the maximum track height allowed in pixels. */

struct dyString *uiStateUrlPart(struct track *toggleGroup);
/* Return a string that contains all the UI state in CGI var
 * format.  If toggleGroup is non-null the visibility of that
 * track will be toggled in the string. */

boolean nextItemCompatible(struct track *tg);
/* Check to see if we draw nextPrev item buttons on a track. */

void createHgFindMatchHash();
/* Read from the cart the string assocated with matches and 
   put the matching items into a hash for highlighting later. */

TrackHandler lookupTrackHandler(char *name);
/* Lookup handler for track of give name.  Return NULL if none. */

void registerTrackHandlers();
/* Register tracks that include some non-standard methods. */

void initColors(struct hvGfx *hvg);
/* Initialize the shades of gray etc. */

char *getItemDataName(struct track *tg, char *itemName);
/* Translate an itemName to its itemDataName, using tg->itemDataName if is not
 * NULL. The resulting value should *not* be freed, and it should be assumed
 * that it will only remain valid until the next call of this function.*/

void registerTrackHandler(char *name, TrackHandler handler);
/* Register a track handling function. */

#endif /* HGTRACKS_H */

