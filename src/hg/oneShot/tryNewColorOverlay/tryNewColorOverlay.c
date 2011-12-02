/* tryNewColorOverlay - Experiment with a new method for transparent overlay.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "hvGfx.h"
#include "bigWig.h"
#include "bits.h"
#include "math.h"


/* Dimensions of image overall and our portion within it. */
int imageWidth = 800, imageHeight = 120;
int trackWidth = 700, trackHeight = 100;
int trackXoff = 50, trackYoff = 10;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tryNewColorOverlay - Experiment with a new method for transparent overlay.\n"
  "usage:\n"
  "   tryNewColorOverlay inList chrom start end output.png\n"
  "where inList is a file in 5 column format: <red> <green> <blue> <scale> <bigWigFile>\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct rgbWig
/* A color and a wig file */
    {
    struct rgbWig *next;
    int r,g,b;		/* The color component */
    double scale;	/* Scaling factor for bigWig. */
    char *bigWigName;	/* A bigWig file name */
    struct bbiSummaryElement *summary;  /* Summary of what's in view*/
    };

struct rgbWig *rgbWigLoadAll(char *fileName)
/* Read in file of rgbWigs and return as list. */
{
struct rgbWig *el, *list = NULL;
char *row[5];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, row))
    {
    AllocVar(el);
    el->r = lineFileNeedNum(lf, row, 0);
    el->g = lineFileNeedNum(lf, row, 1);
    el->b = lineFileNeedNum(lf, row, 2);
    el->scale = lineFileNeedDouble(lf, row, 3);
    el->bigWigName = cloneString(row[4]);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

inline int countBitsInBytes(UBYTE *bytes, int count)
/* Return number of bits set in given count of bytes. */
{
int total = 0;
while (--count >= 0)
    total += bitsInByte[*bytes++];
return total;
}

void calcIndirectRgbSum(UBYTE *layerPt, int layerCount, struct rgbColor *colors, 
	double *retR, double *retG, double *retB)
{
UBYTE bitMask = 1, byte = *layerPt;
double r=0, g = 0, b = 0;
int i;
for (i=0; i<layerCount; ++i)
    {
    if (byte&bitMask)
        {
	struct rgbColor *col = &colors[i];
	r += col->r;
	g += col->g;
	b += col->b;
	}

    /* Move to next bit for next layer. */
    bitMask <<= 1;
    if (bitMask == 0)
        {
	++layerPt;
	byte = *layerPt;
	bitMask = 1;
	}
    }
*retR = r;
*retG = g;
*retB = b;
}


void renderPseudoTransparency(struct hvGfx *hvg, int xOff, int yOff, int width, int height,
	struct rgbColor *colors, UBYTE *layers, int layerCount, int bytesPerPixel, int bytesPerLine)
/* Convert representation where there is a bit for each layer to one that merges together
 * the layer colors. */
{
/* Initially let's just count up the number of bits that are set and use this as a grayscale. */
int x, y;
bitsInByteInit();
double invLayerCount = 1.0/layerCount;
UBYTE *layerLine = layers;
for (y=0; y<height; ++y)
    {
    UBYTE *layerPt = layerLine;
    for (x=0; x<width;++x)
        {
	int bitCount = countBitsInBytes(layerPt, bytesPerPixel);
	double gray = 1.0 -  invLayerCount * bitCount;
	if (bitCount > 0)
	    {
	    double invBitCount = 1.0/bitCount;
	    double sumR, sumG, sumB;
	    calcIndirectRgbSum(layerPt, layerCount, colors, &sumR, &sumG, &sumB);
	    struct rgbColor aveRgb;
	    aveRgb.r = sumR *invBitCount;
	    aveRgb.g = sumG * invBitCount;
	    aveRgb.b = sumB * invBitCount;
	    struct hsvColor hsv = mgRgbToHsv(aveRgb);
	    hsv.v = gray * hsvValMax;
	    struct rgbColor rgb = mgHsvToRgb(hsv);
	    // int col = hvGfxFindRgb(hvg, &rgbGray);
	    int col = hvGfxFindRgb(hvg, &rgb);
	    hvGfxDot(hvg, x+xOff, height-1-y+yOff, col);
	    }

	layerPt += bytesPerPixel;
	}
    layerLine += bytesPerLine;
    }
}

void fakeSummaryArrayFromSine(struct bbiSummaryElement *summary, int sumCount,
	double period, double scale, double offset)
/* Just make a sine wave. */
{
int i;
double theta = 0;
double dTheta = 2*M_PI/period;
for (i=0; i<sumCount; ++i)
    {
    summary[i].validCount = 1;
    summary[i].sumData = sin(theta)*scale + offset;
    theta += dTheta;
    }
}

void tryNewColorOverlay(char *inTabFile, char *chrom, char *startString, char *endString, 
	char *outPng)
/* tryNewColorOverlay - Experiment with a new method for transparent overlay.. */
{
int winStart = sqlUnsigned(startString);
int winEnd = sqlUnsigned(endString);
struct rgbWig *wigEl, *wigList = rgbWigLoadAll(inTabFile);
int wigCount = slCount(wigList);
printf("Read %d from %s.  Plotting at %s:%d-%d\n", wigCount, inTabFile, chrom, winStart, winEnd);
struct hvGfx *hvg  = hvGfxOpenPng(imageWidth, imageHeight, outPng, FALSE);
hvGfxBox(hvg, 0, 0, imageWidth, imageHeight, MG_BLUE);
hvGfxBox(hvg, trackXoff, trackYoff, trackWidth, trackHeight, MG_WHITE);

int bytesPerPixel = (wigCount + 7)/8;
int bytesPerLine = trackWidth * bytesPerPixel;
long bytesPerImage = bytesPerLine * trackHeight;
printf("%d overlays, %d bytes per pixel, %d bytes per line\n", wigCount, bytesPerPixel, bytesPerLine);

uglyTime(NULL);

/* Stuff for data from bigWig */
#ifdef SOON
double sinScale = 0.5;
for (wigEl = wigList; wigEl != NULL; wigEl = wigEl->next)
    {
    AllocArray(wigEl->summary, trackWidth);
    fakeSummaryArrayFromSine(wigEl->summary, trackWidth, trackWidth*0.16666667, sinScale, 0.5);
    sinScale *= 0.8;
    }
#endif /* SOON */
for (wigEl = wigList; wigEl != NULL; wigEl = wigEl->next)
    {
    struct bbiFile *bbi = bigWigFileOpen(wigEl->bigWigName);
    AllocArray(wigEl->summary, trackWidth);
    if (!bigWigSummaryArrayExtended(bbi, chrom, winStart, winEnd, trackWidth, wigEl->summary))
        errAbort("Couldn't get summary for %s", wigEl->bigWigName);
    bigWigFileClose(&bbi);
    }
uglyTime("Read from bigWigs");

/* Stuff to keep one bit per possible overlay */
UBYTE *layers = needLargeZeroedMem(bytesPerImage);
AllocArray(layers, bytesPerImage);
int byteOffset = 0;
UBYTE bitMask = 1;
uglyTime("Allocated layers");

for (wigEl = wigList; wigEl != NULL; wigEl = wigEl->next)
    {
    int i;
    struct bbiSummaryElement *summary = wigEl->summary;
    for (i=0; i<trackWidth; ++i)
        {
	UBYTE *colStart = layers+(i*bytesPerPixel)+byteOffset;
	struct bbiSummaryElement *sum = &summary[i];
	if (sum->validCount)
	    {
	    double mean = sum->sumData/sum->validCount;
	    double scaled = mean * wigEl->scale;
	    int y = round(scaled * trackHeight);
	    if (y >= trackHeight)
	        y = trackHeight-1;
	    if (y < 0)
	        y = 0;
	    UBYTE *posInLine = colStart + y*bytesPerLine;
	    do
	        {
		*posInLine |= bitMask;
		posInLine -= bytesPerLine;
		}
	    while (--y >= 0);
	    }
	}

    /* Move to next bit for next layer. */
    bitMask <<= 1;
    if (bitMask == 0)
        {
	byteOffset += 1;
	bitMask = 1;
	}
    }
uglyTime("Filled out layers");

/* Make array of colors as input to layer renderer */
struct rgbColor colors[wigCount];
int wigIx = 0;
for (wigEl = wigList, wigIx=0; wigEl != NULL; wigEl = wigEl->next, ++wigIx)
    {
    struct rgbColor *col = &colors[wigIx];
    col->r = wigEl->r;
    col->g = wigEl->g;
    col->b = wigEl->b;
    }
renderPseudoTransparency(hvg, trackXoff, trackYoff, trackWidth, trackHeight, 
	colors, layers, wigCount, bytesPerPixel, bytesPerLine);
uglyTime("Rendered pseudoTransparency");

/* Clean up. */
freez(&layers);
hvGfxClose(&hvg);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
tryNewColorOverlay(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
