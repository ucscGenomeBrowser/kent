/**************************************************
 * Copyright Jim Kent 2000.  All Rights Reserved. * 
 * Use permitted only by explicit agreement with  *
 * Jim Kent (jim_kent@pacbell.net).               *
 **************************************************/

#include "common.h"
#include "memgfx.h"

void mgSetDefaultColorMap(cmap)
struct rgbColor *cmap;
{
    zeroBytes(cmap, 256*3);

    cmap[MG_WHITE].r = 255;
    cmap[MG_WHITE].g = 255;
    cmap[MG_WHITE].b = 255;

    cmap[MG_RED].r = 255;
    cmap[MG_GREEN].g = 255;
    cmap[MG_BLUE].b = 255;
}


struct memGfx *mgNew(width, height)
int width, height;
{
struct memGfx *mg;

if ((mg = (struct memGfx*)calloc(1, sizeof(*mg))) == NULL)
    return NULL;
if ((mg->pixels = (Color *)malloc(width*height)) == NULL)
    {
    free(mg);
    return NULL;
    }
mg->width = width;
mg->height = height;
mgSetDefaultColorMap(mg->colorMap);
mg->colorsUsed = MG_BLUE+1;
return mg;
}

/* Set all pixels to background. */
void mgClearPixels(mg)
struct memGfx *mg;
{
    zeroBytes(mg->pixels, mg->width*mg->height);
}

void mgFree(pmg)
struct memGfx **pmg;
{
struct memGfx *mg = *pmg;
if (mg != NULL)
    {
    if (mg->pixels != NULL)
	free(mg->pixels);
    zeroBytes(mg, sizeof(*mg));
    free(mg);
    }
*pmg = NULL;
}

void mgDrawBox(mg, x, y, width, height, color)
struct memGfx *mg;
int x,y,width,height;
Color color;
{
int end, over;
int wrapCount;
int i;
Color *pt;

if (x < 0)
    {
    width += x;
    x = 0;
    }
if (y < 0)
    {
    height += y;
    y = 0;
    }
end = x+width;
over = end - mg->width;
if (over > 0)
    width -= over;
end = y+height;
over = end - mg->height;
if (over > 0)
    height -= over;
if (width > 0 && height > 0)
    {
    pt = mg->pixels + y*mg->width + x;
    wrapCount = mg->width - width;
    while (--height >= 0)
	{
	i = width;
	while (--i >= 0)
	    *pt++ = color;
	pt += wrapCount;
	}
    }
}

void mgDrawLine(mg, x1, y1, x2, y2, color)
struct memGfx *mg;
int x1,y1,x2,y2;
Color color;
{
int   duty_cycle;
int incy;
int   delta_x, delta_y;
int dots;

delta_y = y2-y1;
delta_x = x2-x1;
if (delta_y < 0) 
    {
    delta_y = -delta_y;
    incy = -1;
    }
else
    {
    incy = 1;
    }
if ((delta_x) < 0) 
    {
    delta_x = -delta_x;
    incy = -incy;
    x1 = x2;
    y1 = y2;
    }
duty_cycle = (delta_x - delta_y)/2;
if (delta_x >= delta_y)
    {
    dots = delta_x+1;
    while (--dots >= 0)
	{
	mgPutDot(mg,x1,y1,color);
	duty_cycle -= delta_y;
	x1 += 1;
	if (duty_cycle < 0)
	    {
	    duty_cycle += delta_x;	  /* update duty cycle */
	    y1+=incy;
	    }
	}
    }
else
    {
    dots = delta_y+1;
    while (--dots >= 0)
	{
	mgPutDot(mg,x1,y1,color);
	duty_cycle += delta_x;
	y1+=incy;
	if (duty_cycle > 0)
	    {
	    duty_cycle -= delta_y;	  /* update duty cycle */
	    x1 += 1;
	    }
	}
    }
}
