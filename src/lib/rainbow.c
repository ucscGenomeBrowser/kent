/* rainbow - stuff to generate rainbow colors. 
 * This file is copyright 1984-2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

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


static struct rgbColor greyScaleRainbowTable[30] = {
/* This is a rainbow from white to black. There are no colors, only varying
 * shades of grey. It is good for displays that are not meant to draw attention. */
   {255,255,255},
   {247,247,247},
   {238,238,238},
   {230,230,230},
   {221,221,221},
   {213,213,213},
   {204,204,204},
   {196,196,196},
   {187,187,187},
   {179,179,179},
   {170,170,170},
   {162,162,162},
   {153,153,153},
   {145,145,145},
   {136,136,136},
   {128,128,128},
   {119,119,119},
   {111,111,111},
   {102,102,102},
   {93,93,93},
   {85,85,85},
   {76,76,76},
   {68,68,68},
   {59,59,59},
   {50,50,50},
   {42,42,42},
   {33,33,33},
   {25,25,25},
   {16,16,16},
   {0,0,0},
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

struct rgbColor greyScaleRainbowAtPos(double pos)
/* Given pos, a number between 0 and 1, return a blackToWhite rainbow rgbColor
 * where 0 maps to white,  0.1 is grey, and 1 is black. */
{
return interpolatedHue(greyScaleRainbowTable, ArraySize(greyScaleRainbowTable), pos);
}

struct rgbColor *getRainbow(struct rgbColor (*rainbowAtPos)(double pos), int size)
/* Return array filled with rainbow of colors */
{
struct rgbColor *colors;
AllocArray(colors, size);
double invCount = 1.0/size;
double pos;
int i;
for (i = 0; i < size; i++)
    {
    pos = invCount * i;
    colors[i] = (*rainbowAtPos)(pos);
    }
return colors;
}

