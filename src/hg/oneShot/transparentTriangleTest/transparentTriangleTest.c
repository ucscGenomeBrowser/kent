/* transparentTriangleTest - Test out a normalized transparency idea using transparent triangles.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "memgfx.h"
#include "rainbow.h"
#include "hvGfx.h"

int imageWidth = 400, imageHeight = 120;

struct rgbColor lightRainbowAtPos(double pos);
/* Given pos, a number between 0 and 1, return a lightish rainbow rgbColor
 * where 0 maps to red,  0.1 is orange, and 0.9 is violet and 1.0 is back to red */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "transparentTriangleTest - Test out a normalized transparency idea using transparent triangles.\n"
  "usage:\n"
  "   transparentTriangleTest triangleCount output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct floatPic
/* A picture that stores RGB values in floating point. */
    {
    float **lines;   /* points to start of each line of image */
    float *image;
    int width;	/* Width in pixels. */
    int height; /* Heigh in pixels. */
    };

struct floatPic *floatPicNew(int width, int height)
/* Return a new floatPic. */
{
long lineSize = 3L * width;
long imageSize = lineSize * height;
struct floatPic *pic = needMem(sizeof(struct floatPic));
pic->width = width;
pic->height = height;
pic->image = needHugeMem(imageSize * sizeof(float));

/* Create and initialize line start array */
AllocArray(pic->lines, height);
int i = height;
float *line = pic->image;
float **lines = pic->lines;
while (--i >= 0)
    {
    *lines++ = line;
    line += lineSize;
    }
return pic;
}

void floatPicSet(struct floatPic *pic, float r, float g, float b)
/* Set full image to a single color */
{
long totalSize = pic->width * pic->height;
float *p = pic->image;
while (--totalSize >= 0)
    {
    *p++ = r;
    *p++ = g;
    *p++ = b;
    }
}

void floatPicSetHorizontal(struct floatPic *pic, int x, int y, int count, float r, float g, float b)
/* Set count pixels in a horizontal line starting at x,y */
{
float *p = pic->lines[y] + x*3;
while (--count >= 0)
    {
    *p++ = r;
    *p++ = g;
    *p++ = b;
    }
}

void floatPicMulHorizontal(struct floatPic *pic, int x, int y, int count, float r, float g, float b)
/* Multiply pixels with existing content in horizongal line starting at x,y. */
{
float *p = pic->lines[y] + x*3;
while (--count >= 0)
    {
    *p++ *= r;
    *p++ *= g;
    *p++ *= b;
    }
}

void floatPicToPng(struct floatPic *pic, char *fileName)
/* Write out picture to PNG of given file name. */
{
uglyf("floatPicToPng(%dx%d lines=%p %s)\n", pic->width, pic->height, pic->lines, fileName);
struct hvGfx *hvg  = hvGfxOpenPng(pic->width, pic->height, fileName, FALSE);
int width = pic->width, height = pic->height;
Color *lineBuf;
AllocArray(lineBuf, width);
int y;
for (y=0; y<height; ++y)
    {
    float *fp = pic->lines[y];
    Color *cp = lineBuf;
    int i = width;
    while (--i >= 0)
        {
	int red = fp[0]*255.9;
	int green = fp[1]*255.9;
	int blue = fp[2]*255.9;
	*cp++ = MAKECOLOR_32(red, green, blue);
	fp += 3;
	}
    hvGfxVerticalSmear(hvg, 0, y, width, 1, lineBuf, TRUE);
    }
hvGfxClose(&hvg);
}

void drawAlignedTriangle(struct floatPic *pic,  struct rgbColor *rgb, int xOff, int yOff, 
    int width, int height)
/* Draw an isocoles triangle with symmetric peak at top */
{
float red = rgb->r / 255.9;
float green = rgb->g / 255.9;
float blue = rgb->b / 255.9;
int y;
double invHeight = 1.0/height;
double xCen = xOff + width*0.5;
for (y=0; y<=height; ++y)
    {
    int curWidth = y*width*invHeight;
    int x1 = xCen - curWidth/2;
    floatPicMulHorizontal(pic, x1, y+yOff, curWidth, red, green, blue);
    }
}

void drawTriangles(struct floatPic *pic, int count, int xOff, int yOff, int width, int height)
/* Draw count # of evenly spaced triangles starting at xOff, yOff, and spanning in total 
 * width and height in image. */
{
int w = width - 1;
int h = height - 1;
int oneWidth = h;
int dx = (w - oneWidth)/count;
int x = xOff;
int i;
double col = 0, dCol = (1.0/count);
for (i=0; i<count; ++i)
    {
    struct rgbColor rgb = lightRainbowAtPos(col);
    drawAlignedTriangle(pic, &rgb, x, yOff, oneWidth, h);
    col += dCol;
    x += dx;
    }
}

void transparentTriangleTest(char *countString, char *outFile)
/* transparentTriangleTest - Test out a normalized transparency idea using transparent triangles. */
{
int triangleCount = sqlUnsigned(countString);
if (triangleCount < 2)
  errAbort("Triangle count must be at least 2."); 
struct floatPic *pic = floatPicNew(imageWidth, imageHeight);
floatPicSet(pic, 1.0, 1.0, 1.0);
drawTriangles(pic, triangleCount, 0, 0, imageWidth, imageHeight);
floatPicToPng(pic, outFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
transparentTriangleTest(argv[1], argv[2]);
return 0;
}
