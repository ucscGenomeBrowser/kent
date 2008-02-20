/* hgColors - colors used in UCSC Genome Browser pages. */

#include "common.h"
#include "hgColors.h"

static char const rcsid[] = "$Id: hgColors.c,v 1.2 2008/02/20 00:42:31 markd Exp $";

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

