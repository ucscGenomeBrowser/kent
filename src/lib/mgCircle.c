/* mgCircle.c - Simple stepping draw a circle algorithm.  
	Don't even correct aspect ratio. */

#include "common.h"
#include "memgfx.h"


void mgCircle(struct memGfx *mg, int xCen, int yCen, int rad, 
	Color color, boolean filled)
/* Draw a circle using a stepping algorithm.  Doesn't correct
 * for non-square pixels. */
{
int err;
int derr, yerr, xerr;
int aderr, ayerr, axerr;
register int x,y;
int lasty;

if (rad <= 0)
    {
    mgPutDot(mg, xCen, yCen, color);
    return;
    }
err = 0;
x = rad;
lasty = y = 0;
for (;;)
    {
    if (filled)
	{
	if (y == 0)
	    mgLineH(mg, yCen, xCen-x, xCen+x, color);
	else
	    {
	    if (lasty != y)
		{
		mgLineH(mg, yCen-y, xCen-x, xCen+x, color);
		mgLineH(mg, yCen+y, xCen-x, xCen+x, color);
		lasty = y;
		}
	    }
	}
    else
	{
	/* draw 4 quadrandts of a circle */
	mgPutDot(mg, xCen+x, yCen+y, color);
	mgPutDot(mg, xCen+x, yCen-y, color);
	mgPutDot(mg, xCen-x, yCen+y, color);
	mgPutDot(mg, xCen-x, yCen-y, color);
	}
    axerr = xerr = err -x-x+1;
    ayerr = yerr = err +y+y+1;
    aderr = derr = yerr+xerr-err;
    if (aderr < 0)
	aderr = -aderr;
    if (ayerr < 0)
	ayerr = -ayerr;
    if (axerr < 0)
	axerr = -axerr;
    if (aderr <= ayerr && aderr <= axerr)
	{
	err = derr;
	x -= 1;
	y += 1;
	}
    else if (ayerr <= axerr)
	{
	err = yerr;
	y += 1;
	}
    else
	{
	err = xerr;
	x -= 1;
	}
    if (x < 0)
	break;
    }
}
