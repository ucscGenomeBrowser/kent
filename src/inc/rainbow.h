/* rainbow - stuff to generate rainbow colors. */

#ifndef RAINBOW_H
#define RAINBOW_H

#ifndef MEMGFX_H
#include "memgfx.h"
#endif

struct rgbColor saturatedRainbowAtPos(double pos);
/* Given pos, a number between 0 and 1, return a saturated rainbow rgbColor
 * where 0 maps to red,  0.1 is orange, and 0.9 is violet and 1.0 is back to red */

struct rgbColor lightRainbowAtPos(double pos);
/* Given pos, a number between 0 and 1, return a lightish rainbow rgbColor
 * where 0 maps to red,  0.1 is orange, and 0.9 is violet and 1.0 is back to red */

struct rgbColor veryLightRainbowAtPos(double pos);
/* Given pos, a number between 0 and 1, return a light rainbow rgbColor
 * where 0 maps to red,  0.1 is orange, and 0.9 is violet and 1.0 is back to red */

struct rgbColor greyScaleRainbowAtPos(double pos); 
/* Given pos, a number between 0 and 1, return a blackToWhite rainbow rgbColor
 * where 0 maps to white,  0.1 is grey, and 1 is black. */

struct rgbColor *getRainbow(struct rgbColor (*rainbowAtPos)(double), int size);
/* Return array filled with rainbow of colors */

#endif /* RAINBOW_H */
