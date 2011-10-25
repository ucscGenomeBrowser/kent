/* rainbow - stuff to generate rainbow colors. */

#include "common.h"
#include "memgfx.h"
#include "rainbow.h"

struct rgbColor saturatedRainbowTable[28] = {
/* This table was built by hand for the default Autodesk Animator palette, and then edited
 * to reduce the amount of green colors that are indistinguishable. */
    {255, 0, 0},
    {255, 64, 0},
    {255, 128, 0},
    {255, 164, 0},
    {255, 210, 0},
    {255, 255, 0},
    {210, 255, 0},
    {164, 255, 0},
    {128, 255, 0},
    {0, 255, 0},
    {0, 255, 128},
    {0, 255, 164},
    {0, 255, 210},
    {0, 255, 255},
    {0, 210, 255},
    {0, 164, 255},
    {0, 128, 255},
    {0, 64, 255},
    {0, 0, 255},
    {64, 0, 255},
    {128, 0, 255},
    {164, 0, 255},
    {210, 0, 255},
    {255, 0, 255},
    {255, 0, 210},
    {255, 0, 164},
    {255, 0, 128},
    {255, 0, 64},
    };

static struct rgbColor lightRainbowTable[28] = {
/* This is a mixture of 1/2 white and 1/2 saturated rainbow.  It's good for rainbow
 * fringe graphs with a moderate amount of colors (up to about 10) */
   {255,128,128},
   {255,160,128},
   {255,192,128},
   {255,210,128},
   {255,233,128},
   {255,255,128},
   {233,255,128},
   {210,255,128},
   {192,255,128},
   {128,255,128},
   {128,255,192},
   {128,255,210},
   {128,255,233},
   {128,255,255},
   {128,233,255},
   {128,210,255},
   {128,192,255},
   {128,160,255},
   {128,128,255},
   {160,128,255},
   {192,128,255},
   {210,128,255},
   {233,128,255},
   {255,128,255},
   {255,128,233},
   {255,128,210},
   {255,128,192},
   {255,128,160},
};

static struct rgbColor veryLightRainbowTable[30] = {
/* This is a mixture of 2/3 white and 1/3 saturated rainbow.  It's good for rainbow
 * fringe graphs with a lot of colors (more than 10) */
   {255,174,174},
   {255,191,174},
   {255,207,174},
   {255,223,174},
   {255,239,174},
   {255,255,174},
   {239,255,174},
   {223,255,174},
   {207,255,174},
   {191,255,174},
   {174,255,174},
   {174,255,191},
   {174,255,207},
   {174,255,223},
   {174,255,239},
   {174,255,255},
   {174,239,255},
   {174,223,255},
   {174,207,255},
   {174,191,255},
   {174,174,255},
   {191,174,255},
   {207,174,255},
   {223,174,255},
   {239,174,255},
   {255,174,255},
   {255,174,239},
   {255,174,223},
   {255,174,207},
   {255,174,191},
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

struct rgbColor veryLightRainbowAtPos(double pos)
/* Given pos, a number between 0 and 1, return a light rainbow rgbColor
 * where 0 maps to red,  0.1 is orange, and 0.9 is violet and 1.0 is back to red */
{
return interpolatedHue(veryLightRainbowTable, ArraySize(veryLightRainbowTable), pos);
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

