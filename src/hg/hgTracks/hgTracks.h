/* hgTracks.h - stuff private to hgTracks, but shared with
 * individual tracks. */

#ifndef HGTRACKS_H
#define HGTRACKS_H

#ifndef VGFX_H
#include "vGfx.h"
#endif

#ifndef HUI_H
#include "hui.h"
#endif

#ifndef CART_H
#include "cart.h"
#endif

struct trackGroup
/* Structure that displays a group of tracks. The central data structure
 * of the graphical genome browser. */
    {
    struct trackGroup *next;   /* Next on list. */
    char *mapName;             /* Database track name. Name on image map etc. */
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
    /* loadItems loads up items for the chromosome range indicated.   */

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


    void (*mapItem)(struct trackGroup *tg, void *item, 
    	char *itemName, int start, int end, 
	int x, int y, int width, int height); 
    /* Write out image mapping for a given item */

    boolean hasUi;	/* True if has an extended UI page. */
    void *extraUiData;	/* Pointer for track specific filter etc. data. */

    void (*trackFilter)(struct trackGroup *tg);	
    /* Stuff to handle user interface parts. */

    void *customPt;  /* Misc pointer variable unique to group. */
    int customInt;   /* Misc int variable unique to group. */
    int subType;     /* Variable to say what subtype this is for similar groups
	              * to share code. */

    unsigned short private;	/* True(1) if private, false(0) otherwise. */
    int bedSize;		/* Number of fields if a bed file. */
    float priority; /* Tracks are drawn in priority order. */
    };

struct simpleFeature
/* Minimal feature - just stores position in browser coordinates. */
    {
    struct simpleFeature *next;
    int start, end;			/* Start/end in browser coordinates. */
    int grayIx;                         /* Level of gray usually. */
    };

/* Some details of how to draw linked features. */
enum {lfSubXeno = 1};
enum {lfSubSample = 2};
enum {lfWithBarbs = 3}; /* Turn on barbs to show direction based on 
                         * strand field */

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
    void *extra;			/* Extra info that varies with type. */
    char popUp[128];			/* text for popup */
    };

struct trackLayout
/* This structure controls the basic dimensions of display. */
    {
    MgFont *font;		/* What font to use. */
    int leftLabelWidth;		/* Width of left labels. */
    int trackWidth;		/* Width of tracks. */
    int picWidth;		/* Width of entire picture. */
    };

extern struct trackLayout tl;

extern struct cart *cart; /* The cart where we keep persistent variables. */
extern char *chromName;	  /* Name of chromosome sequence . */
extern char *database;	  /* Name of database we're using. */
extern char *organism;	  /* Name of organism we're working on. */
extern int winStart;	  /* Start of window in sequence. */
extern int winEnd;	  /* End of window in sequence. */
extern int seqBaseCount;  /* Number of bases in sequence. */
extern int winBaseCount;  /* Number of bases in window. */

extern int maxShade;		  /* Highest shade in a color gradient. */
extern Color shadesOfGray[10+1];  /* 10 shades of gray from white to black
                                   * Red is put at end to alert overflow. */
extern Color shadesOfBrown[10+1]; /* 10 shades of brown from tan to tar. */
extern struct rgbColor brownColor;
extern struct rgbColor tanColor;
extern struct rgbColor guidelineColor;

extern Color shadesOfSea[10+1];       /* Ten sea shades. */
extern struct rgbColor darkSeaColor;
extern struct rgbColor lightSeaColor;


void hPrintf(char *format, ...);
/* Printf that can be suppressed if not making html. 
 * All cgi output should go through here, hPuts, hPutc
 * or hWrites. */

void hPuts(char *string);
/* Puts that can be suppressed if not making html. */

void hPutc(char c);
/* putc than can be suppressed if not makeing html. */

void hWrites(char *string);
/* Write string with no '\n' if not suppressed. */

void printHtmlComment(char *format, ...);
/* Function to print output as a comment so it is not seen in the HTML
 * output but only in the HTML source. */
    
int orientFromChar(char c);
/* Return 1 or -1 in place of + or - */

char charFromOrient(int orient);
/* Return + or - in place of 1 or -1 */

enum trackVisibility limitVisibility(struct trackGroup *tg, void *items);
/* Return default visibility limited by number of items. */

char *hgcNameAndSettings();
/* Return path to hgc with variables to store UI settings. */

void mapBoxHc(int start, int end, int x, int y, int width, int height, 
	char *group, char *item, char *statusLine);
/* Print out image map rectangle that would invoke the htc (human track click)
 * program. */

double scaleForPixels(double pixelWidth);
/* Return what you need to multiply bases by to
 * get to scale of pixel coordinates. */

void drawScaledBox(struct vGfx *vg, int chromStart, int chromEnd, 
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

int percentGrayIx(int percent);
/* Return gray shade corresponding to a number from 50 - 100 */

int vgFindRgb(struct vGfx *vg, struct rgbColor *rgb);
/* Find color index corresponding to rgb color. */

void vgMakeColorGradient(struct vGfx *vg, 
    struct rgbColor *start, struct rgbColor *end,
    int steps, Color *colorIxs);
/* Make a color gradient that goes smoothly from start
 * to end colors in given number of steps.  Put indices
 * in color table in colorIxs */

Color contrastingColor(struct vGfx *vg, int backgroundIx);
/* Return black or white whichever would be more visible over
 * background. */

Color getChromColor(char *name, struct vGfx *vg);
/* Return color index corresponding to chromosome name. */

void clippedBarbs(struct vGfx *vg, int x, int y, 
	int width, int barbHeight, int barbSpacing, int barbDir, Color color,
	boolean needDrawMiddle);
/* Draw barbed line.  Clip it to fit the window first though since
 * some barbed lines will span almost the whole chromosome, and the
 * clipping at the lower level is not efficient since we added
 * PostScript output support. */

void innerLine(struct vGfx *vg, int x, int y, int w, Color color);
/* Draw a horizontal line of given width minus a pixel on either
 * end.  This pixel is needed for PostScript only, but doesn't
 * hurt elsewhere. */


/* Some little functional stubs to fill in track group
 * function pointers with if we have nothing to do. */
boolean tgLoadNothing(struct trackGroup *tg);
void tgDrawNothing(struct trackGroup *tg);
void tgFreeNothing(struct trackGroup *tg);
int tgItemNoStart(struct trackGroup *tg, void *item);
int tgItemNoEnd(struct trackGroup *tg, void *item);

int tgFixedItemHeight(struct trackGroup *tg, void *item);
/* Return item height for fixed height track. */

int tgFixedTotalHeight(struct trackGroup *tg, enum trackVisibility vis);
/* Most fixed height track groups will use this to figure out the height 
 * they use. */

void linkedFeaturesFreeList(struct linkedFeatures **pList);
/* Free up a linked features list. */

int linkedFeaturesCmpStart(const void *va, const void *vb);
/* Help sort linkedFeatures by starting pos. */

void linkedFeaturesBoundsAndGrays(struct linkedFeatures *lf);
/* Calculate beginning and end of lf from components, etc. */

void linkedFeaturesDraw(struct trackGroup *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis);
/* Draw linked features items. */

void linkedFeaturesMethods(struct trackGroup *tg);
/* Fill in track group methods for linked features. 
 * Many other methods routines will call this first
 * to get a reasonable set of defaults. */

Color lfChromColor(struct trackGroup *tg, void *item, struct vGfx *vg);
/* Return color of chromosome for linked feature type items
 * where the chromosome is listed somewhere in the lf->name. */

char *lfMapNameFromExtra(struct trackGroup *tg, void *item);
/* Return map name of item from extra field. */

void chainMethods(struct trackGroup *tg);
/* Return name of item from extra field. */

void netMethods(struct trackGroup *tg);
/* Make track group for chain/net alignment. */

#endif /* HGTRACKS_H */

