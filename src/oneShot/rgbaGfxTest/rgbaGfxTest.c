/* rgbaGfxTest - Test out rgbaGfx system.. */
#include "png.h"
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: rgbaGfxTest.c,v 1.1 2010/04/29 22:42:27 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rgbaGfxTest - Test out rgbaGfx system.\n"
  "usage:\n"
  "   rgbaGfxTest XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct rgbaColor
    {
    unsigned char r, g, b, a;
    };

struct rgbaGfx
/* Structure you can draw on in red/green/blue/alpha (transparency). */
    {
    struct rgbaColor *pixels;
    int width, height;
    int clipMinX, clipMaxX;
    int clipMinY, clipMaxY;
    };

struct rgbaGfx *rgbaGfxNew(int width, int height)
/* Return a new image to draw one. */
{
struct rgbaGfx *rg;
AllocVar(rg);
rg->width = rg->clipMaxX = width;
rg->height = rg->clipMaxY = height;
rg->pixels = needHugeMem(width*height*sizeof(rg->pixels[0]));
return rg;
}

void rgbaGfxSetToSolidWhite(struct rgbaGfx *rg)
/* Set all pixels (quickly) to non-transparent white. */
{
int count = rg->width * rg->height;
bits32 solid = 0xFFFFFFFF;
bits32 *pt = (bits32*)rg->pixels;
int i;
for (i=0; i<count; ++i)
   *pt++ = solid;
}

#define _rgBpr(rg) ((rg)->width)
/* Get what to add to get to next line */

#define _rgPixAdr(rg,x,y) ((rg)->pixels + (rg)->width * (y) + (x))
/* Get pixel address */

#define _rgPutDot(rg, x, y, color) (*_rgPixAdr(rg,x,y) = (color))
/* Unclipped plot a dot */

#define _rgGetDot(rg, x, y) (*_rgPixAdr(rg,x,y))
/* Unclipped get a dot, you do not want to use this, this is special for
 * verticalText only */

#define rgPutDot(rg,x,y,color) if ((x)>=(rg)->clipMinX && (x) < (rg)->clipMaxX && (y)>=(rg)->clipMinY  && (y) < (rg)->clipMaxY) _rgPutDot(rg,x,y,color)
/* Clipped put dot */

#define rgGetDot(rg,x,y) ((x)>=(rg)->clipMinX && (x) < (rg)->clipMaxX && (y)>=(rg)->clipMinY  && (y) < (rg)->clipMaxY) ? _rgGetDot(rg,x,y) : 0
/* Clipped get dot, you do not want to use this, this is special for
 * verticalText only */

struct rgbaColor rgbaTransparentBlend(struct rgbaColor a, struct rgbaColor b)
/* Return blend of two rgba format colors.   Does not actually touch the alpha
 * channel.  It does blend the colors though, treating them as if they were both 
 * transparent filters. */
{
struct rgbaColor res;
res.a = (a.a + b.a)>>1;
res.r = (a.r * b.r)/255;
res.g = (a.g * b.g)/255;
res.b = (a.b * b.b)/255;
return res;
}

void rgDrawBox(struct rgbaGfx *rg, int x, int y, int width, int height, struct rgbaColor color)
/* Draw a box. */
{
int i;
struct rgbaColor *pt;
int x2 = x + width;
int y2 = y + height;
int wrapCount;

if (x < rg->clipMinX)
    x = rg->clipMinX;
if (y < rg->clipMinY)
    y = rg->clipMinY;
if (x2 > rg->clipMaxX)
    x2 = rg->clipMaxX;
if (y2 > rg->clipMaxY)
    y2 = rg->clipMaxY;
width = x2-x;
height = y2-y;
if (width > 0 && height > 0)
    {
    pt = _rgPixAdr(rg,x,y);
    wrapCount = rg->width - width;
    while (--height >= 0)
	{
	i = width;
	while (--i >= 0)
	    {
	    *pt = rgbaTransparentBlend(*pt, color);
	    pt += 1;
	    }
	pt += wrapCount;
	}
    }
}

static void pngAbort(png_structp png, png_const_charp errorMessage)
/* type png_error wrapper around errAbort */
{
errAbort("%s", (char *)errorMessage);
}

static void pngWarn(png_structp png, png_const_charp warningMessage)
/* type png_error wrapper around warn */
{
warn("%s", (char *)warningMessage);
}

boolean saveToPng(FILE *f, struct rgbaGfx *rg)
/* Save RGBA PNG to an already open file.
 * Reference: http://libpng.org/pub/png/libpng-1.2.5-manual.html */
{
png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
					  NULL, // don't need pointer to data for err/warn handlers
					  pngAbort, pngWarn);
if (!png)
    {
    errAbort("png_write_struct failed");
    return FALSE;
    }
png_infop info = png_create_info_struct(png);
if (!info)
    {
    png_destroy_write_struct(&png, NULL);
    errAbort("png create_info_struct failed");
    return FALSE;
    }

// If setjmp returns nonzero, it means png_error is returning control here.
// But that should not happen because png_error should call pngAbort which calls errAbort.
if (setjmp(png_jmpbuf(png)))
    {
    png_destroy_write_struct(&png, &info);
    fclose(f);
    errAbort("pngwrite: setjmp nonzero.  "
	     "why didn't png_error..pngAbort..errAbort stop execution before this errAbort?");
    return FALSE;
    }

// Configure PNG output params:
png_init_io(png, f);
png_set_IHDR(png, info, rg->width, rg->height, 8, // 8=bit_depth
	     PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
	     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

// Write header/params, write pixels, close and clean up.
// PNG wants a 2D array of pointers to byte offsets into palette/colorMap.
// rg has a 1D array of byte offsets.  Make row pointers for PNG:
png_byte **row_pointers = needMem(rg->height * sizeof(png_byte *));
int i;
for (i = 0;  i < rg->height;  i++)
    row_pointers[i] = (png_byte*)(&(rg->pixels[i*rg->width]));
png_set_rows(png, info, row_pointers);
png_write_png(png, info, PNG_TRANSFORM_IDENTITY, // no transform
	      NULL); // unused as of PNG 1.2
png_destroy_write_struct(&png, &info);
return TRUE;
}

void savePng(char *filename, struct rgbaGfx *rg)
{
FILE *pngFile = mustOpen(filename, "wb");
if (!saveToPng(pngFile, rg))
    {
    remove(filename);
    errAbort("Couldn't save %s", filename);
    }
if (fclose(pngFile) != 0)
    errnoAbort("fclose failed");
}


void rgBrezy(struct rgbaGfx *rg, int x1, int y1, int x2, int y2, struct rgbaColor color,
	int yBase, boolean fillFromBase)
/* Brezenham line algorithm.  Optionally fill in under line. */
{
if (x1 == x2)
    {
    int y,height;
    if (y1 > y2)
	{
	y = y2;
	height = y1-y2+1;
	}
    else
        {
	y = y1;
	height = y2-y1+1;
	}
    if (fillFromBase)
        {
	if (y < yBase)
	    rgDrawBox(rg, x1, y, 1, yBase-y, color);
	}
    else
        rgDrawBox(rg, x1, y, 1, height, color);
    }
else if (y1 == y2)
    {
    int x,width;
    if (x1 > x2)
        {
	x = x2;
	width = x1-x2+1;
	}
    else
        {
	x = x1;
	width = x2-x1+1;
	}
    if (fillFromBase)
        {
	if (y1 < yBase)
	    rgDrawBox(rg, x, y1, width, yBase - y1, color);
	}
    else
        {
	rgDrawBox(rg, x, y1, width, 1, color);
	}
    }
else
    {
    int duty_cycle;
    int incy;
    int delta_x, delta_y;
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
    if (delta_x < 0) 
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
	    if (fillFromBase)
		{
		if (y1 < yBase)
		    rgDrawBox(rg,x1,y1,1,yBase-y1,color);
		}
	    else
		rgPutDot(rg,x1,y1,color);
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
	    if (fillFromBase)
		{
		if (y1 < yBase)
		    rgDrawBox(rg,x1,y1,1,yBase-y1,color);
		}
	    else
		rgPutDot(rg,x1,y1,color);
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
}

void rgDrawLine(struct rgbaGfx *rg, int x1, int y1, int x2, int y2, struct rgbaColor color)
/* Draw a line from one point to another. */
{
rgBrezy(rg, x1, y1, x2, y2, color, 0, FALSE);
}

void rgFillUnder(struct rgbaGfx *rg, int x1, int y1, int x2, int y2, 
	int bottom, struct rgbaColor color)
/* Draw a 4 sided filled figure that has line x1/y1 to x2/y2 at
 * it's top, a horizontal line at bottom as it's bottom, and
 * vertical lines from the bottom to y1 on the left and bottom to
 * y2 on the right. */
{
rgBrezy(rg, x1, y1, x2, y2, color, bottom, TRUE);
}


void rgbaGfxTest(char *a)
/* rgbaGfxTest - Test out rgbaGfx system.. */
{
struct rgbaGfx *rg = rgbaGfxNew(500, 500);
rgbaGfxSetToSolidWhite(rg);

struct rgbaColor red = {255, 128, 128, 255};
struct rgbaColor green = {128, 255, 128, 255};
struct rgbaColor blue = {128, 128, 255, 255};
// struct rgbaColor weakBlue = {0,0,100,255};

rgDrawBox(rg, 50, 100, 200, 200, red);
rgDrawBox(rg, 100, 50, 200, 200, blue);
rgDrawBox(rg, 150, 0, 50, 500, green);

savePng(a, rg);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
rgbaGfxTest(argv[1]);
return 0;
}
