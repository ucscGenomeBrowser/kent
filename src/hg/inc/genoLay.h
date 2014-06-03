/* genoLay - genome layout. Arranges chromosomes so that they
 * tend to fit together nicely on a single page. */

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef GENOLAY_H
#define GENOLAY_H

#ifndef HVGFX_H
#include "hvGfx.h"
#endif

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

#define genoLayTwoPerLine "two per line"
#define genoLayOnePerLine "one per line"
#define genoLayAllOneLine "all in one line"
/* The "how" parameter to genoLayNew should be one of these 
 * above three. */

struct genoLay
/* This has information on how to lay out chromosomes. */
    {
    MgFont *font;			/* Font used for labels */
    struct genoLayChrom *chromList;	/* List of all chromosomes. */
    struct hash *chromHash;		/* Hash of chromosomes. */
    int picWidth;			/* Total picture width */
    int picHeight;			/* Total picture height */
    int margin;				/* Blank area around sides */
    int spaceWidth;			/* Width of a space. */
    struct slRef *leftList;		/* Left chromosomes. */
    struct slRef *rightList;		/* Right chromosomes. */
    struct slRef *bottomList;		/* Sex chromosomes are on bottom. */
    int lineCount;			/* Number of chromosome lines. */
    int leftLabelWidth, rightLabelWidth;/* Pixels for left/right labels */
    int lineHeight;			/* Height for one line */
    int betweenChromHeight;		/* Height between chromosomes. */
    int betweenChromOffsetY;		/* Start of area between chroms. */
    int chromHeight;			/* Height of chromosome ideograms. */
    int chromOffsetY;			/* Start of chromosome ideograms. */
    int totalHeight;			/* Total width/height in pixels */
    double basesPerPixel;		/* Bases per pixel */
    double pixelsPerBase;		/* Pixels per base. */
    char *how;				/* How parameter from genoLayNew. */
    boolean allOneLine;			/* True if draw all on one line. */
    };

void genoLayDump(struct genoLay *gl);
/* Print out info on genoLay */

int genoLayChromCmpName(const void *va, const void *vb);
/* Compare two chromosome names so as to sort numerical part
 * by number.. */

struct genoLayChrom *genoLayDbChromsExt(struct sqlConnection *conn, 
	boolean withRandom, boolean abortOnErr);
/* Get chrom info list. */

struct genoLayChrom *genoLayDbChroms(struct sqlConnection *conn, 
	boolean withRandom);
/* Get chrom info list. */

struct genoLay *genoLayNew(struct genoLayChrom *chromList,
	MgFont *font, int picWidth, int betweenChromHeight,
	int minLeftLabelWidth, int minRightLabelWidth,
	char *how);
/* Figure out layout.  For human and most mammals this will be
 * two columns with sex chromosomes on bottom.  This is complicated
 * by the platypus having a bunch of sex chromosomes.  The how
 * parameter should be one of the string constants defined above. */

void genoLayDrawChromLabels(struct genoLay *gl, struct hvGfx *hvg, int color);
/* Draw chromosomes labels in image */

void genoLayDrawSimpleChroms(struct genoLay *gl,
	struct hvGfx *hvg, int color);
/* Draw boxes for all chromosomes in given color */

void genoLayDrawBandedChroms(struct genoLay *gl, struct hvGfx *hvg, char *db,
	struct sqlConnection *conn, Color *shadesOfGray, int maxShade, 
	int defaultColor);
/* Draw chromosomes with centromere and band glyphs. 
 * Get the band data from the database.  If the data isn't
 * there then draw simple chroms in default color instead */
/* Draw boxes for all chromosomes in given color */

#endif /* GENOLAY_H */

