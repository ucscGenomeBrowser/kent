/* hgColors - colors used in UCSC Genome Browser pages. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hgColors.h"
#include "hgConfig.h"


void hMakeGrayShades(struct hvGfx *hvg, Color *shades, int maxShade)
/* Make up gray scale with 0 = white, and maxShade = black. 
 * Shades needs to have maxShade+1 colors. */
{
int i;
for (i=0; i<=maxShade; ++i)
    {
    struct rgbColor rgb;
    int level = 255 - (255*i/maxShade);
    if (level < 0) level = 0;
    rgb.r = rgb.g = rgb.b = level;
    rgb.a = 255;
    shades[i] = hvGfxFindRgb(hvg, &rgb);
    }
}

int hGrayInRange(int oldVal, int oldMin, int oldMax, int newMax)
/* Return oldVal, which lies between oldMin and oldMax, to
 * equivalent number between 1 and newMax. The way this does it
 * is perhaps a little odd, forcing 0 go to 1, but visually it works
 * out nicely when 0 is white. */
{
int range = oldMax - oldMin;
int newVal = ((oldVal-oldMin)*newMax + (range>>1))/range;
if (newVal <= 0) newVal = 1;
if (newVal > newMax) newVal = newMax;
return newVal;
}

void hInsideStyleToWhite()
/* Override a few old styling things to get a white instead of cream background */
{
printf("<style>");
printf("body.cgi {\n");
printf("   background: #F0F0F0;\n");
printf("}\n");
printf("table.hgInside {\n");
printf("   background: #FFFFFF;\n");
printf("}\n");
printf("</style>");
}

