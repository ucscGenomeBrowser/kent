/* hgColors - colors used in UCSC Genome Browser pages. */

#ifndef HGCOLORS_H
#define HGCOLORS_H

#ifndef HVGFX_H
#include "hvGfx.h"
#endif

#define HG_COL_HOTLINKS "2636D1"
#define HG_COL_HEADER "D9E4F8"
#define HG_COL_OUTSIDE "FFF9D2"
#define HG_CL_OUTSIDE 0xFFF9D2
#define HG_COL_INSIDE "FFFEE8"
#define HG_CL_INSIDE 0xFFFEE8
#define HG_COL_BORDER "888888"
#define HG_COL_TABLE "D9F8E4"
#define HG_COL_LOCAL_TABLE "D9E4FF"
#define HG_COL_TABLE_LABEL "1616D1"

void hMakeGrayShades(struct hvGfx *hvg, Color *shades, int maxShade);
/* Make up gray scale with 0 = white, and maxShade = black. 
 * Shades needs to have maxShade+1 colors. */

int hGrayInRange(int oldVal, int oldMin, int oldMax, int newMax);
/* Return oldVal, which lies between oldMin and oldMax, to
 * equivalent number between 1 and newMax. The way this does it
 * is perhaps a little odd, forcing 0 go to 1, but visually it works
 * out nicely when 0 is white. */

char *hgColOutside();
/* get color to use from body background, as a string of hex digits */

#endif /* HGCOLORS_H */
