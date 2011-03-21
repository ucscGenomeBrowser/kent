/* rainbow - stuff to generate rainbow colors. */

#include "common.h"
#include "memgfx.h"
#include "rainbow.h"

struct rgbColor saturatedRainbowTable[30] = {
/* This table was built by hand for the default Autodesk Animator palette. */
    {252, 12, 60},
    {252, 12, 12},
    {252, 60, 12},
    {252, 108, 12},
    {252, 156, 12},
    {252, 204, 12},
    {252, 252, 12},
    {204, 252, 12},
    {156, 252, 12},
    {108, 252, 12},
    {60, 252, 12},
    {12, 252, 12},
    {12, 252, 60},
    {12, 252, 108},
    {12, 252, 156},
    {12, 252, 204},
    {12, 252, 252},
    {12, 204, 252},
    {12, 156, 252},
    {12, 108, 252},
    {12, 60, 252},
    {12, 12, 252},
    {60, 12, 252},
    {108, 12, 252},
    {156, 12, 252},
    {204, 12, 252},
    {252, 12, 252},
    {252, 12, 204},
    {252, 12, 156},
    {252, 12, 108},
    };

static struct rgbColor lightRainbowTable[30] = {
/* This one is processed some to make the lightness of the colors more even across the
 * spectrum, and also just lighter in general.  It's good for rainbow fringe graphs. */
    {239, 126, 148},
    {249, 131, 131},
    {235, 145, 123},
    {222, 158, 116},
    {210, 170, 110},
    {200, 181, 105},
    {190, 190, 100},
    {178, 197, 103},
    {165, 203, 107},
    {150, 210, 110},
    {135, 218, 114},
    {118, 226, 118},
    {114, 218, 135},
    {110, 210, 150},
    {107, 203, 165},
    {103, 197, 178},
    {100, 190, 190},
    {105, 181, 200},
    {110, 170, 210},
    {116, 158, 222},
    {123, 145, 235},
    {131, 131, 249},
    {148, 126, 239},
    {165, 121, 230},
    {180, 116, 222},
    {194, 112, 214},
    {207, 108, 207},
    {214, 112, 194},
    {222, 116, 180},
    {230, 121, 165},
    };

static struct rgbColor interpolatedHue(struct rgbColor *table, int tableSize, double pos)
/* Given pos, a number between 0 and 1, return interpolated color, doing interpolation
 * between first and last color for numbers close to 1. */
{
double wrappedPos = pos - floor(pos);	/* Make it so numbers higher than 1 keep circling rainbow */
double scaledPos = tableSize * wrappedPos;
double startSlot = floor(scaledPos);	
double endFactor = scaledPos - startSlot;
double startFactor = 1.0 - endFactor;
int startIx = startSlot;
int endIx = startIx + 1;
if (endIx == tableSize)
    endIx = 0;

struct rgbColor *start = table+startIx;
struct rgbColor *end = table+endIx;
struct rgbColor col;
col.r = start->r * startFactor + end->r * endFactor;
col.g = start->g * startFactor + end->g * endFactor;
col.b = start->b * startFactor + end->b * endFactor;
return col;
}

struct rgbColor lightRainbowAtPos(double pos)
/* Given pos, a number between 0 and 1, return a lightish rainbow rgbColor
 * where 0 maps to red,  0.1 is orange, and 0.9 is violet and 1.0 is back to red */
{
return interpolatedHue(lightRainbowTable, ArraySize(lightRainbowTable), pos);
}

struct rgbColor saturatedRainbowAtPos(double pos)
/* Given pos, a number between 0 and 1, return a saturated rainbow rgbColor
 * where 0 maps to red,  0.1 is orange, and 0.9 is violet and 1.0 is back to red */
{
return interpolatedHue(saturatedRainbowTable, ArraySize(saturatedRainbowTable), pos);
}

