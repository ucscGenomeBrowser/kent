/* Browser graphics - stuff for drawing graphics displays that
 * are reasonably browser (human and intronerator) specific. */

#include "common.h"
#include "memgfx.h"

static long figureTickSpan(long totalLength, int maxNumTicks)
/* Figure out whether ticks on ruler should be 1, 5, 10, 50, 100, 500, 
 * 1000, etc. units apart.  */
{
int roughTickLen = totalLength/maxNumTicks;
int i;
int tickLen = 1;

for (i=0; i<9; ++i)
    {
    if (roughTickLen < tickLen)
    	return tickLen;
    tickLen *= 5;
    if (roughTickLen < tickLen)
	return tickLen;
    tickLen *= 2;
    }
return 1000000000;
}

static void numLabelString(int num, char *label)
/* Returns a numerical labeling string. */
{
char *sign = "";
if (num < 0)
    {
    num = -num;
    sign = "-";
    }
sprintf(label, "%s%d", sign, num);
}

void mgDrawRulerBumpText(struct memGfx *mg, int xOff, int yOff, 
	int height, int width,
        Color color, MgFont *font,
        int startNum, int range, int bumpX, int bumpY)
/* Draw a ruler inside the indicated part of mg with numbers that start at
 * startNum and span range.  Bump text positions slightly. */
{
int tickSpan;
int tickPos;
double scale;
int firstTick;
int remainder;
int end = startNum + range;
int x;
char tbuf[14];
int numWid;
int goodNumTicks;
int niceNumTicks = width/35;

numLabelString(startNum+range, tbuf);
numWid = mgFontStringWidth(font, tbuf)+4+bumpX;
goodNumTicks = width/numWid;
if (goodNumTicks < 1) goodNumTicks = 1;
if (goodNumTicks > niceNumTicks) goodNumTicks = niceNumTicks;

tickSpan = figureTickSpan(range, goodNumTicks);

scale = (double)width / range;

firstTick = startNum + tickSpan;
remainder = firstTick % tickSpan;
firstTick -= remainder;
for (tickPos=firstTick; tickPos<end; tickPos += tickSpan)
    {
    numLabelString(tickPos, tbuf);
    numWid = mgFontStringWidth(font, tbuf)+4;
    x = (int)((tickPos-startNum) * scale) + xOff;
    mgDrawBox(mg, x, yOff, 1, height, color);
    if (x - numWid >= xOff)
        {
        mgTextCentered(mg, x-numWid + bumpX, yOff + bumpY, numWid, 
	    height, color, font, tbuf);
        }
    }
}

void mgDrawRuler(struct memGfx *mg, int xOff, int yOff, int height, int width,
        Color color, MgFont *font,
        int startNum, int range)
/* Draw a ruler inside the indicated part of mg with numbers that start at
 * startNum and span range.  */
{
mgDrawRulerBumpText(mg, xOff, yOff, height, width, color, font,
    startNum, range, 0, 0);
}

#define sign(a)((a>=0)?(1):(-1))

void mgFilledSlopedLine( struct memGfx *mg, Color *pt1,
            Color *pt1Home, double slope, int mult, int w, double h, 
            Color *colors, int colRange, Color *pt1Base )
/*Fills in the space below the line of width w and slope slope
and height h with a fixed color of light gray for now.*/
{
int j;
int yUpper, yLower;
int pUpper, pLower;
double sum = 0.0;
int prevX = -1;

if( abs(slope) < 1 )
    {
    for (j=0; j<w; j += 1)
        {
        sum += slope;
        yUpper = min(abs((int)sum)+1,(int)h);
        yLower = min(abs((int)sum),(int)h);
        pUpper = (int)(colRange * ((double)min(fabs(sum),h) - (double)yLower));
        pLower = (int)colRange - pUpper;

        if( pLower != colRange ) 
            pt1 = (mult * yUpper) + pt1Home;
        else
            pt1 = (mult * yLower) + pt1Home;

        while( pt1 < pt1Base - (sign(slope)*mult) )
            {
            pt1 += sign(slope)*mult;
            pt1[j] = colors[3];
            }
        }
    }
else
    {
    for (j=1; j<=h; j += 1)
        {
        sum += 1.0 / slope;
        pt1 = (mult * j) + pt1Home;

        yLower = min(abs((int)sum),w);
        yUpper = min(abs((int)sum)+1,w);
        pUpper = (int)(colRange * ((double)min(fabs(sum),w) - (double)yLower));
        pLower = (int)colRange - pUpper;

        while( pt1 < pt1Base - (sign(slope)*mult) )
            {
            pt1 += sign(slope)*mult;
            pt1[yLower] = colors[3];
            }

        if( prevX != yUpper && yLower != yUpper  )
            while( pt1 < pt1Base - (sign(slope)*mult) )
                {
                pt1 += sign(slope)*mult;
                pt1[yUpper] = colors[3];
                }

        prevX = yUpper;
       }


    }

}

void mgDrawXSlopedLineAntiAlias( struct memGfx *mg,   Color *pt1,
        Color *pt1Home, double slope, int mult, int w, double h, Color
        *colors, double colRange, Color *pt1Base, int aa, int fill )
/* Draws a sloped line that is dominated by x-component movement
 * with anti-aliasing in the sense that for the y-value 0.3 with a
 * 1 pixel thick line the shading is 70% in the lower pixel and
 * 30% in the top pixel. A value such as 2.0 would only occupy one
 * pixel with 100% shading.*/
{
int fillStart;
int j;
int yLower, pLower;
int yUpper, pUpper;
double sum = 0.0;
for (j=0; j<w; j += 1)
    {
    sum += slope;

    yLower = min(abs((int)sum),(int)h);
    yUpper = min(abs((int)sum)+1,(int)h);
    pUpper = (int)(colRange * ((double)min(fabs(sum),h) - (double)yLower));
    pLower = (int)colRange - pUpper;

    if( aa == 1 )  /* anti-aliasing is on */
        {
        if( pUpper != colRange )
            {
            pt1 = (mult * yLower) + pt1Home;
            pt1[j] = colors[max(pLower,3)];
            }

        if( pLower != colRange )
            {
            pt1 = (mult * yUpper) + pt1Home;
            pt1[j] = colors[max(pUpper,3)];
            }
        }
    else
        {
        if( pLower != colRange ) 
            {
            pt1 = (mult * yUpper) + pt1Home;
            pt1[j] = colors[(int)colRange];
            }
        else
            {
            pt1 = (mult * yLower) + pt1Home;
            pt1[j] = colors[(int)colRange];
            }
        }
    }
}


void mgDrawYSlopedLineAntiAlias( struct memGfx *mg,   Color *pt1,
        Color *pt1Home, double slope, int mult, int w, int h, Color
        *colors, double colRange, Color *pt1Base, int aa, int fill )
/* Draws a sloped line that is dominated by y-component movement
 * with anti-aliasing. See mgDrawXSlopedLineAntiAlias above.*/
{

int j;
int yLower, pLower;
int yUpper, pUpper;
double sum = 0.0;
int prevX = -1;

for (j=1; j<=h; j += 1)
    {
    sum += 1.0 / slope;
    pt1 = (mult * j) + pt1Home;

    yLower = min(abs((int)sum),w);
    yUpper = min(abs((int)sum)+1,w);
    pUpper = (int)(colRange * ((double)min(fabs(sum),w) - (double)yLower));
    pLower = (int)colRange - pUpper;

    if( aa == 1 )  /* anti-aliasing is on */
        {
        if( pUpper != colRange )
            pt1[yLower] = colors[max(pLower,3)];

        if( pLower != colRange )
            pt1[yUpper] = colors[max(pUpper,3)];
        }
    else
        {
        if( pLower != colRange )
            pt1[yUpper] = colors[(int)colRange];
        else
            pt1[yLower] = colors[(int)colRange];
        }
    }
}

void mgConnectingLine( struct memGfx *mg, int x1, double y1d, int x2,
    double y2d, Color *colors, int ybase, int aa, int fill )
/*Draw a line between two points, (x1,y1) to (x2,y2). Will be used
 * with wiggle tracks to interpolate between samples, connecting the
 * end of one block to the beginning of the next one.   */
{
int yLower, pLower;
int yUpper, pUpper;
int mult;

int y1i;
int y2i;
    
int minY;
int maxY;
Color *pt1;
Color *pt1Home;
Color *pt1Base;
int j;
int offset;
double sum, slope;
int bpr = _mgBpr(mg);
double h;
double colRange = 9.0;


/* adjust y if x is cutoff  */
slope = (double)(y2d - y1d)/(double)(x2-x1);
if (x1 <= mg->clipMinX)
    {
    y1d += ((double)(mg->clipMinX - x1)) * slope;
    x1 = mg->clipMinX;
    }
if (x2 >= mg->clipMaxX)
    {
    y2d += -((double)(x2 - mg->clipMaxX)) * slope;
    x2 = mg->clipMaxX;
    }

if ((x2 - x1) <= 0)
    return;

h = fabs(y2d-y1d);

y1i = (int)y1d;
y2i = (int)y2d;

minY = mg->clipMinY;
maxY = mg->clipMaxY;
pt1Home = pt1 = _mgPixAdr(mg,x1,y1i);
pt1Base = _mgPixAdr(mg,x1,ybase);
offset = 0;

slope = (double)(y2d - y1d)/(double)(x2-x1);
mult = bpr * sign(slope);
sum = 0.0;

if (minY <= y1i && y1i < maxY
     && minY <= y2i && y2i < maxY)
    {

    if( fabs( slope ) > 1.0 )  /*y-dominated movement*/
        mgDrawYSlopedLineAntiAlias( mg, pt1, pt1Home, slope, mult,
            x2-x1,(int)h, colors, colRange, pt1Base, aa, fill );
    else                    /*x-dominated movement*/
        mgDrawXSlopedLineAntiAlias( mg, pt1, pt1Home, slope, mult,
            x2-x1, h, colors, colRange, pt1Base, aa, fill );

    /*fill in area below the line */
    if( fill )
        mgFilledSlopedLine( mg, pt1, pt1Home, slope, mult, 
            x2-x1, h, colors, colRange, pt1Base );
    }
}

void mgBarbedHorizontalLine(struct memGfx *mg, int x, int y, 
	int width, int barbHeight, int barbSpacing, int barbDir, Color color,
	boolean needDrawMiddle)
/* Draw a horizontal line starting at xOff, yOff of given width.  Will
 * put barbs (successive arrowheads) to indicate direction of line.  
 * BarbDir of 1 points barbs to right, of -1 points them to left. */
{
int x1, x2, yHi = y + barbHeight, yLo = y - barbHeight;
int offset, startOffset, endOffset, barbAdd;

if (barbDir < 0)
    {
    startOffset = 0;
    endOffset = width - barbHeight;
    barbAdd = barbHeight;
    }
else
    {
    startOffset = barbHeight;
    endOffset = width;
    barbAdd = -barbHeight;
    }

for (offset = startOffset; offset < endOffset; offset += barbSpacing)
    {
    x1 = x + offset;
    x2 = x1 + barbAdd;
    mgDrawLine(mg, x1, y, x2, yHi, color);
    mgDrawLine(mg, x1, y, x2, yLo, color);
    }
}

