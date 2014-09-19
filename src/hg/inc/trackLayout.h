/* trackLayout - this controls the dimensions of the graphic
 * for the genome browser.  Also used for the genome view. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef TRACKLAYOUT_H
#define TRACKLAYOUT_H

struct trackLayout
/* This structure controls the basic dimensions of display. */
    {
    char *textSize;		/* Symbolic name of text size. */
    MgFont *font;		/* What font to use. */
    int leftLabelWidth;		/* Width of left labels. */
    int trackWidth;		/* Width of tracks. */
    int picWidth;		/* Width of entire picture. */
    int mWidth;			/* Width of 'M' in font. */
    int nWidth;			/* Width of 'N' in font. */
    int fontHeight;		/* Height of font. */
    int barbHeight;		/* Height of arrows on introns. */
    int barbSpacing;		/* Space between arrows on introns. */
    };

void trackLayoutInit(struct trackLayout *tl, struct cart *cart);
/* Initialize layout around small font and a picture about 600 pixels
 * wide, but this can be overridden by cart. */

void trackLayoutSetPicWidth(struct trackLayout *tl, char *s);
/* Set pixel width from ascii string. */

boolean trackLayoutInclFontExtras();
/* Check if fonts.extra is set to use "yes" in the config.  This enables
 * extra fonts and related options that are not part of the public browser */

#define textSizeVar "textSize"	/* Variable name used for text size. */
#define MAX_DISPLAY_PIXEL_WIDTH 5000

#endif /* TRACKLAYOUT_H */

