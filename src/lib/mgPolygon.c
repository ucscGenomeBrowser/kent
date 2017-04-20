/* mgPolygon - stuff to draw polygons in memory.  This code dates
 * back to 1984, was used in the Aegis Animator for the Atari ST. 
 * It was an exceedingly speed critical routine for that program.
 * For us it may be overkill, but the major speed tweak - the special
 * case for convex polygons - is only 1/3 of the module, so I'm leaving
 * it in.  I've lightly adapted the code to current conventions, but
 * you can still see some relics in the local and static variable names of 
 * other coding conventions.  
 * This file is copyright 1984-2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "bits.h"
#include "memgfx.h"
#include "gfxPoly.h"


void mgDrawPolyOutline(struct memGfx *mg, struct gfxPoly *poly, Color color)
/* Draw a singe pixel line around polygon. */
{
struct gfxPoint *a, *b, *end;

a = end = poly->ptList;
b = a->next;
for (;;)
    {
    mgDrawLine(mg, a->x, a->y, b->x, b->y, color);
    a = b;
    b = b->next;
    if (a == end)
        break;
    }
}

/* ----- Stuff for convex polygons that may contain crossing lines ----- 
 * This code works by allocating a bitmap big enough to hold the polygon
 * and drawing lines on the bitmap in a special fashion, and then
 * scanning the bitmap to connect together the dots with horizontal lines. */

#define UPDIR 1
#define DOWNDIR 0

static UBYTE *on_off_buf;	/* Our bitplane. */

static int pxmin, pxmax, pymin, pymax;	/* Bounds of polygon. */

static void find_pminmax(struct gfxPoly *poly)
{
struct gfxPoint *pointpt;
int i;

pointpt = poly->ptList;
pxmin = pxmax = pointpt->x;
pymin = pymax = pointpt->y;
pointpt = pointpt->next;

i = poly->ptCount;
while (--i > 0)
   {
   if (pxmin > pointpt->x) pxmin = pointpt->x;
   if (pxmax < pointpt->x) pxmax = pointpt->x;
   if (pymin > pointpt->y) pymin = pointpt->y;
   if (pymax < pointpt->y) pymax = pointpt->y;
   pointpt = pointpt->next;
   }
}

static void xor_pt(int bpr, int x, int y)
/* Xor in a bit at given location. */
{
UBYTE rot;

rot = ((unsigned)0x80) >> (x&7);
on_off_buf[ bpr*y + (x>>3) ] ^= rot;
}


static void y_xor_line(int bpr, int x1, int y1, int x2, int y2)
/* Xor in a line onto on_off_buf, only plotting when we hit a new y-value. */
{
UBYTE *imagept = on_off_buf;
UBYTE rot;
int   duty_cycle;
int   delta_x, delta_y;
int dots;
int swap;

if (x1 > x2)
    {
    swap = x1;
    x1 = x2;
    x2 = swap;
    swap = y1;
    y1 = y2;
    y2 = swap;
    }
delta_y = y2-y1;
delta_x = x2-x1;
rot = ((unsigned)0x80) >> (x1&7);
imagept += bpr*y1 + (x1>>3);


if (delta_y < 0) 
    {
    delta_y = -delta_y;
    bpr = -bpr;
    }
duty_cycle = (delta_x - delta_y)/2;
*(imagept) ^= rot;
if (delta_x < delta_y)
    {
    dots = delta_y;
    while (--dots >= 0)
	{
	*(imagept) ^= rot;
	duty_cycle += delta_x;	  /* update duty cycle */
	imagept += bpr;
	if (duty_cycle > 0)
	    {
	    duty_cycle -= delta_y;
	    rot >>= 1;
	    if (rot == 0)
		{
		imagept++;
		rot = 0x80;
		}
	    }
	}
    }
else
    {
    dots = delta_x;
    while (--dots >= 0)
	{
	duty_cycle -= delta_y;	  /* update duty cycle */
	if (duty_cycle < 0)
	    {
	    *(imagept) ^= rot;
	    duty_cycle += delta_x;
	    imagept += bpr;
	    }
	rot >>= 1;
	if (rot == 0)
	    {
	    imagept++;
	    rot = 0x80;
	    }
	}
    }
}

 
static void drawAfterOnOff(struct memGfx *mg, Color color, int bpr, int width, 
	int height)
/* Examine bitmap and draw horizontal lines between set bits. */
{
UBYTE *linept = on_off_buf;
int y;

for (y=0; y<height; ++y)
	{
	int start = 0;
	int end;
	for (;;)
	    {
	    start = bitFindSet(linept, start, width);
	    if (start >= width)
	        break;
	    end = bitFindSet(linept, start+1, width);
	    mgLineH(mg, y+pymin, start+pxmin, end+pxmin, color);
	    start = end+1;
	    }
	linept += bpr;
	}
}

static void fillConcave(struct memGfx *mg, struct gfxPoly *poly, Color color)
/* Draw concave polygon */
{
struct gfxPoint *pointpt;
int x,y;
int ox,oy;
int lastdir;
int i;
int bpr;	/* Bytes per row */
int width, height;
long size;

find_pminmax(poly);
if (pymin==pymax)  /*Complex code can't cope with trivial case*/
    {
    mgLineH(mg, pymin, pxmin, pxmax,color);
    return;
    }
width = pxmax - pxmin + 1;
height = pymax - pymin + 1;
bpr = ((width+7)>>3);
size = (long)bpr*height;
if ((on_off_buf = needMem(size)) == NULL)	
    return;
pointpt = poly->ptList;
x = pointpt->x;
y = pointpt->y;

do
	{
	pointpt = pointpt->next;
	ox = pointpt->x;
	oy = pointpt->y;
	}
while (oy == y);

if (oy>y)
	lastdir = UPDIR;
else
	lastdir = DOWNDIR;

i = poly->ptCount;
while (--i >= 0)
   {
   pointpt = pointpt->next;
   x = pointpt->x;
   y = pointpt->y;
   if (y!=oy)
	  {
	  y_xor_line(bpr,ox-pxmin,oy-pymin,x-pxmin,y-pymin);
	  if (y>oy)
		 if (lastdir == UPDIR)
			xor_pt(bpr,ox-pxmin,oy-pymin);
		 else
			lastdir = UPDIR;
	  else
		 if (lastdir == DOWNDIR)
			xor_pt(bpr,ox-pxmin,oy-pymin);
		 else
			lastdir = DOWNDIR;
	  }
   ox = x;
   oy = y;
   }

drawAfterOnOff(mg, color, bpr, width, height);
freez(&on_off_buf);
return;
}


void mgDrawPoly(struct memGfx *mg, struct gfxPoly *poly, Color color,
	boolean filled)
/* Draw polygon, possibly filled in color. */
{
if (filled)
    fillConcave(mg, poly, color);
mgDrawPolyOutline(mg, poly, color);
}

