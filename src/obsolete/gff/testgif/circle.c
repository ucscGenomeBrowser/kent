
/* circle.c - Simple stepping draw a circle algorithm.  
	Don't even correct aspect ratio. */

#include "jimk.h"


ccircle(xcen, ycen, rad, dotout, hlineout, filled)
int xcen, ycen, rad;
Vector dotout;	/* some function to pass x/y parameters to for each dot */
Vector hlineout;	/* some function to pass x/y parameters to for each dot */
int filled;
{
int err;
int derr, yerr, xerr;
int aderr, ayerr, axerr;
register int x,y;
int lasty;

if (rad <= 0)
	{
	if (filled)
		(*hlineout)(ycen,xcen,xcen);
	else
		(*dotout)(xcen,ycen);
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
			(*hlineout)(ycen, xcen-x, xcen+x);
		else
			{
			if (lasty != y)
				{
				(*hlineout)(ycen-y, xcen-x, xcen+x);
				(*hlineout)(ycen+y, xcen-x, xcen+x);
				lasty = y;
				}
			}
		}
	else
		{
		/* draw 4 quadrandts of a circle */
		(*dotout)(xcen+x,ycen+y);
		(*dotout)(xcen+x,ycen-y);
		(*dotout)(xcen-x,ycen+y);
		(*dotout)(xcen-x,ycen-y);
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
