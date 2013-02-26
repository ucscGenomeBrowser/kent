/* transparentTriangleTest - Test out a normalized transparency idea using transparent triangles.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "memgfx.h"
#include "rainbow.h"
#include "hvGfx.h"

int clWidth = 800, clHeight = 100;
int clLineCount = 10;

struct rgbColor lightRainbowAtPos(double pos);
/* Given pos, a number between 0 and 1, return a lightish rainbow rgbColor
 * where 0 maps to red,  0.1 is orange, and 0.9 is violet and 1.0 is back to red */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "transparentTriangleTest - Test out a normalized transparency idea using transparent triangles.\n"
  "usage:\n"
  "   transparentTriangleTest triangleCount output maxNorm\n"
  "options:\n"
  "   -width=N - width of image in pixels\n"
  "   -height=N - height of a single row of triangles in pixels\n"
  "   -lineCount=N - number of triangle rows\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"width", OPTION_INT},
   {"height", OPTION_INT},
   {"count", OPTION_INT},
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

double floatPicMinComponent(struct floatPic *pic)
/* Return minimum r, g, or b componenent in pic. */
{
long totalSize = pic->width * pic->height * 3;
float curMin = 1.0;
float *p = pic->image;
while (--totalSize >= 0)
    {
    float comp = *p++;
    if (comp < curMin)
         curMin = comp;
    }
return curMin;
}

double floatPicMinValue(struct floatPic *pic)
/* Return number between 0 and 1 where 0 is darkest, 1 is lightest */
{
float rScale = 0.3, gScale = 0.5, bScale = 0.2;
long totalSize = pic->width * pic->height;
float curMin = 1.0;
float *p = pic->image;
while (--totalSize >= 0)
    {
    float curVal = rScale * p[0] + gScale * p[1] + bScale * p[2];
    p += 3;
    if (curVal < curMin)
         curMin = curVal;
    }
return curMin;
}

void floatPicNormalize(struct floatPic *pic, float normFactor)
/* Adjust darkness while trying to keep same colors. */
{
long totalSize = pic->width * pic->height*3;
float *p = pic->image;
while (--totalSize >= 0)
    {
    *p = powf(*p, normFactor);
    p += 1;
    }
}

void floatPicIntoHvg(struct floatPic *pic, int xOff, int yOff, struct hvGfx *hvg)
/* Copy float pic into hvg at given offset. */
{
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
    hvGfxVerticalSmear(hvg, xOff, y + yOff, width, 1, lineBuf, TRUE);
    }
freez(&lineBuf);
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
    struct rgbColor rgb = veryLightRainbowAtPos(col);
    drawAlignedTriangle(pic, &rgb, x, yOff, oneWidth, h);
    col += dCol;
    x += dx;
    }
}

void transparentTriangleTest(char *countString, char *outFile, char *maxNormString)
/* transparentTriangleTest - Test out a normalized transparency idea using transparent triangles. */
{
double maxNorm = sqlDouble(maxNormString);
if (maxNorm < 0.0 || maxNorm > 1.0)
    errAbort("maxNorm parameter must be between 0 and 1");
int triangleCount = sqlUnsigned(countString);
if (triangleCount < 2)
  errAbort("Triangle count must be at least 2."); 
struct hvGfx *hvg  = hvGfxOpenPng(clWidth, clHeight*(clLineCount+1), outFile, FALSE);
struct floatPic *pic = floatPicNew(clWidth, clHeight);
uglyTime("Allocate picture");
int i;
double minNorm = 0.001;
double dNorm = 0.0;
if (clLineCount > 1)
    dNorm = (maxNorm - minNorm)/(clLineCount-1);
for (i=0; i<=clLineCount; ++i)
    {
    double desiredVal = maxNorm - i*dNorm;
    int yOff = i*clHeight;
    floatPicSet(pic, 1.0, 1.0, 1.0);
    uglyTime("set picture to white");
    drawTriangles(pic, triangleCount, 0, 0, clWidth, clHeight);
    uglyTime("Draw triangles");
    double currentVal = floatPicMinComponent(pic);
    uglyTime("Figure minComponent %g", currentVal);
    if (currentVal > 0 && i != clLineCount) /* Don't do it on last line or if impossible. */
	{
	double normFactor = log(desiredVal)/log(currentVal);
	floatPicNormalize(pic, normFactor);
	uglyTime("Normalize to %g", desiredVal);
	}
    floatPicIntoHvg(pic, 0, yOff, hvg);
    uglyTime("Convert to hvg");
    }
hvGfxClose(&hvg);
uglyTime("Save as PNG");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
clWidth = optionInt("width", clWidth);
clHeight = optionInt("height", clHeight);
clLineCount = optionInt("lineCount", clLineCount);
uglyTime(NULL);
transparentTriangleTest(argv[1], argv[2], argv[3]);
return 0;
}
