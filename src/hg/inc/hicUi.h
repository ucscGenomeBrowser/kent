/* hic track UI */

/* Copyright (C) 2019 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef HIC_UI_H
#define HIC_UI_H

#include "hic.h"

/* Number of bins to split score values into, also the number of gradient
 * steps in the color values from lowest to highest.  1024 is already overkill
 * for hex-valued colors. */
#define HIC_SCORE_BINS              1024


/* Cart variables */

#define HIC_DRAW_MODE               "drawMode"
#define HIC_DRAW_MODE_TRIANGLE      "triangle"
#define HIC_DRAW_MODE_SQUARE        "square"
#define HIC_DRAW_MODE_ARC           "arc"
#define HIC_DRAW_MODE_DEFAULT       HIC_DRAW_MODE_SQUARE
#define HIC_NORMALIZATION           "normalization"
#define HIC_RESOLUTION              "resolution"
#define HIC_DRAW_AUTOSCALE          "autoscale"
#define HIC_DRAW_MAX_VALUE          "max"
#define HIC_DRAW_MAX_VALUE_DEFAULT  100
#define HIC_DRAW_COLOR              "color"
#define HIC_DRAW_BG_COLOR           "bgColor"
/* Default to drawing red on a white background */
#define HIC_DRAW_COLOR_DEFAULT      "#ff0000"
#define HIC_DRAW_BG_COLOR_DEFAULT   "#ffffff"


void hicCfgUi(char *database, struct cart *cart, struct trackDb *tdb, char *track,
                        char *title, boolean boxed);
/* Draw the list of track configuration options for Hi-C tracks */

char *hicUiFetchResolution(struct cart *cart, char *track, struct hicMeta *meta);
/* Return the current resolution selection, or the default if none
 * has been selected. */

int hicUiFetchResolutionAsInt(struct cart *cart, char *track, struct hicMeta *meta, int windowSize);
/* Return the current resolution selection as an integer.  If there is no selection, or if "Auto"
 * has been selected, return the largest available value that still partitions the window into at
 * least 5000 bins. */

char *hicUiFetchNormalization(struct cart *cart, char *track, struct hicMeta *meta);
/* Return the current normalization selection, or the default if none
 * has been selected.  Right now this is a hard-coded set specifically for
 * .hic files, but in the future this list might be dynamically determined by
 * the contents and format of the Hi-C file. */

char *hicUiFetchDrawMode(struct cart *cart, char *track);
/* Return the current draw mode selection, or the default if none
 * has been selected. */

char *hicUiFetchDrawColor(struct cart *cart, char *track);
/* Retrieve the HTML hex code for the color to draw the
 * track values in (e.g., #00ffa1) */

char *hicUiFetchBgColor(struct cart *cart, char *track);
/* Retrieve the HTML hex code of the background color for the 
 * track.  This is the color associated with scores at or close to 0. */

boolean hicUiFetchAutoScale(struct cart *cart, char *track);
/* Returns whether the track is configured to automatically scale its color range
 * depending on the scores present in the window (true), or if it should stick to a
 * gradient based on the user's selected maximum value (false). */

double  hicUiFetchMaxValue(struct cart *cart, char *track);
/* Retrieve the score value at which the draw color reaches its
 * its maximum intensity.  Any scores above this value will
 * share that same draw color. */

#endif
