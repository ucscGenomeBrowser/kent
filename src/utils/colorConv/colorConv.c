/* colorConv - Color conversion program. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "memgfx.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "colorConv - Color conversion program\n"
  "usage:\n"
  "   colorConv (rgb|hsv|hsl) x,y,x\n"
  "   colorConv (rgb|hsv|hsl) x y x\n"
  "\n"
  "   colorConv rgbTransformHsl|rgbTransformHsv|rgbMatrixTransformHsv   r,g,b   h s [v|l]\n"
  "   colorConv rgbTransformHsl|rgbTransformHsv|rgbMatrixTransformHsv   r g b   h s [v|l]\n"
  "        rgbTransformHsl = transformation of r,g,b value conversion to hsl and scaling by h,s,l factors\n"
  "        rgbTransformHsv = transformation of r,g,b value conversion to hsv and scaling by h,s,v factors\n"
  "                      h = hue shift in degrees (0-360, 0.0=none)\n"
  "                      s = saturation scaling factor (1.0=none)\n"
  "                      l/v = lightness/value scaling factor (1.0=none)\n"
  "\n"
  "   colorConv test\n"
//  "options:\n"
  );
}

int eps, maxErrs;
char *factors[3]; // identity

static struct optionSpec options[] = {
   {"eps",  OPTION_INT},
   {"maxErrs",  OPTION_INT},
   {NULL, 0},
};


boolean eq(int a, int b, int eps)
{
return abs(a-b) <= eps;
}


struct hsvColor parseHsv(char *vals[])
{
struct hsvColor hsv;
hsv.h = atoi(vals[0]);
hsv.s = atoi(vals[1]);
hsv.v = atoi(vals[2]);
return hsv;
}


struct hslColor parseHsl(char *vals[])
{
struct hslColor hsl;
hsl.h = atoi(vals[0]);
hsl.s = atoi(vals[1]);
hsl.l = atoi(vals[2]);
return hsl;
}


struct rgbColor parseRgb(char *vals[])
{
struct rgbColor rgb;
rgb.r = atoi(vals[0]);
rgb.g = atoi(vals[1]);
rgb.b = atoi(vals[2]);
return rgb;
}


void rgbConv(char *vals[3])
/* Convert rgb to hsv and hsl */
{
struct rgbColor rgb = parseRgb(vals);
struct hsvColor hsv = mgRgbToHsv(rgb);
struct hslColor hsl = mgRgbToHsl(rgb);
printf("hsv %d,%d,%d\nhsl %d,%d,%d\n", (int)hsv.h, hsv.s, hsv.v, (int)hsl.h, hsl.s, hsl.l);
}


void hslConv(char *vals[3])
/* Convert hsl to rgb and hsv */
{
struct hslColor hsl = parseHsl(vals);
struct rgbColor rgb = mgHslToRgb(hsl);
struct hsvColor hsv = mgRgbToHsv(rgb);
printf("rgb %d,%d,%d\nhsv %d,%d,%d\n", rgb.r, rgb.g, rgb.b, (int)hsv.h, hsv.s, hsv.v);
}


void hsvConv(char *vals[3])
/* Convert hsv to rgb and hsl */
{
struct hsvColor hsv = parseHsv(vals);
struct rgbColor rgb = mgHsvToRgb(hsv);
struct hslColor hsl = mgRgbToHsl(rgb);
printf("rgb %d,%d,%d\nhsl %d,%d,%d\n", rgb.r, rgb.g, rgb.b, (int)hsl.h, hsl.s, hsl.l);
}


void rgbTransformHsl(char *vals[3])
/* transform rgb to hsl, then scale by hsl factors, and transform back to rgb */
{
struct rgbColor rgb  = parseRgb(vals);
struct rgbColor rgb2 = mgRgbTransformHsl(rgb, atof(factors[0]), atof(factors[1]), atof(factors[2]));
printf("rgb %d,%d,%d   hsv %f,%f,%f\nrgbTransformHsl %d,%d,%d\n", rgb.r, rgb.g, rgb.b, 
	atof(factors[0]), atof(factors[1]), atof(factors[2]),
	rgb2.r, rgb2.g, rgb2.b);
}


void rgbTransformHsv(char *vals[3])
/* transform rgb to hsv, then scale by hsv factors, and transform back to rgb */
{
struct rgbColor rgb  = parseRgb(vals);
struct rgbColor rgb2 = mgRgbTransformHsv(rgb, atof(factors[0]), atof(factors[1]), atof(factors[2]));
printf("rgb %d,%d,%d   hsv %f,%f,%f\nrgbTransformHsv %d,%d,%d\n", rgb.r, rgb.g, rgb.b, 
	atof(factors[0]), atof(factors[1]), atof(factors[2]),
	rgb2.r, rgb2.g, rgb2.b);
}


void testTransform(char *label, struct rgbColor (*transformRgb)(struct rgbColor in, double h, double s, double x), 
    double h, double s, double x, int rStart, int gStart, int bStart, int eps, int maxErrs)
{
long start, end, success = 0, failed = 0;
int r, g, b;
struct rgbColor rgbIn, rgbOut;
start = clock1000();
for (r = rStart ; r<256 ; ++r)
    for (g = gStart ; g<256 ; ++g)
	for (b = bStart ; b<256 ; ++b)
	    {
	    rgbIn  =  (struct rgbColor){r, g, b};
	    rgbOut = (*transformRgb)(rgbIn, h, s, x);
	    if (eq(rgbIn.r,rgbOut.r, eps) && eq(rgbIn.g, rgbOut.g, eps) && eq(rgbIn.b, rgbOut.b, eps))
		{
		++success;
		}
	    else
		{
		++failed;
		if (failed <= maxErrs)
		    warn("%s conversion failed: {%d,%d,%d} != {%d,%d,%d}", 
			label, rgbIn.r, rgbIn.g, rgbIn.b, rgbOut.r, rgbOut.g, rgbOut.b);
		}
	    }
end = clock1000();
warn("%s: %ld / %ld OK (%.5f%%) (%ld failed) %ld per second.", label, success, success+failed, 
    100.0*success/(success+failed), failed, 1000L*((256-rStart)*(256-gStart)*(256-bStart)/(end-start)));
}


struct rgbColor convRgbHslRgb(struct rgbColor in)
{
return mgHslToRgb(mgRgbToHsl(in));
}


struct rgbColor convRgbHsvRgb(struct rgbColor in)
{
return mgHsvToRgb(mgRgbToHsv(in));
}


void testConversion(char *label, struct rgbColor (*convRgb)(struct rgbColor), 
    int rStart, int gStart, int bStart, int eps, int maxErrs)
{
long start, end, success = 0, failed = 0;
int r, g, b;
struct rgbColor rgbIn, rgbOut;
start = clock1000();
for (r = rStart ; r<256 ; ++r)
    for (g = gStart ; g<256 ; ++g)
	for (b = bStart ; b<256 ; ++b)
	    {
	    rgbIn  =  (struct rgbColor){r, g, b};
	    rgbOut = (*convRgb)(rgbIn);
	    if (eq(rgbIn.r,rgbOut.r, eps) && eq(rgbIn.g, rgbOut.g, eps) && eq(rgbIn.b, rgbOut.b, eps))
		{
		++success;
		}
	    else
		{
		++failed;
		if (failed <= maxErrs)
		    warn("%s conversion failed: {%d,%d,%d} != {%d,%d,%d}", 
			label, rgbIn.r, rgbIn.g, rgbIn.b, rgbOut.r, rgbOut.g, rgbOut.b);
		}
	    }
end = clock1000();
warn("%s: %ld / %ld OK (%.5f%%) (%ld failed) %ld per second.", label, success, success+failed, 
    100.0*success/(success+failed), failed, 1000L*((256-rStart)*(256-gStart)*(256-bStart)/(end-start)));
}


void test(char *vals[3])
{
int r0 = atoi(vals[0]),  g0 = atoi(vals[1]),  b0 = atoi(vals[2]);
testConversion("rgb->hsl->rgb", &convRgbHslRgb, r0, g0, b0, eps, maxErrs);
testConversion("rgb->hsv->rgb", &convRgbHsvRgb, r0, g0, b0, eps, maxErrs);
testTransform("rgbTransformHsl", &mgRgbTransformHsl, 0.0, 1.0, 1.0, r0, g0, b0, eps, maxErrs);
testTransform("rgbTransformHsv", &mgRgbTransformHsv, 0.0, 1.0, 1.0, r0, g0, b0, eps, maxErrs);
}


int main(int argc, char *argv[])
/* Process command line. */
{
struct hash *handler = newHash(0);
void (*convert)(char *words[3]);
char *words[3] = {"0", "0", "0"};
optionInit(&argc, argv, options);
--argc; ++argv;
hashAdd(handler, "rgb", &rgbConv);
hashAdd(handler, "hsv", &hsvConv);
hashAdd(handler, "hsl", &hslConv);
hashAdd(handler, "rgbTransformHsl", &rgbTransformHsl);
hashAdd(handler, "rgbTransformHsv", &rgbTransformHsv);
hashAdd(handler, "test", &test);
eps = optionInt("eps",0);
maxErrs = optionInt("maxErrs",10);
if (argc == 1)
    ; // use default 0,0,0
else if (argc == 2)
    chopByChar(argv[1], ',', words, 3);
else if (argc == 4)
    {
    words[0] = argv[1];
    words[1] = argv[2];
    words[2] = argv[3];
    }
else if (argc == 5)
    {
    chopByChar(argv[1], ',', words, 3);
    factors[0] = argv[2];
    factors[1] = argv[3];
    factors[2] = argv[4];
    }
else if (argc == 7)
    {
    words[0] = argv[1];
    words[1] = argv[2];
    words[2] = argv[3];
    factors[0] = argv[4];
    factors[1] = argv[5];
    factors[2] = argv[6];
    }
else
    usage();

if (!(convert = hashFindVal(handler, argv[0])))
    errAbort("Cannot convert %s types\n", argv[0]);
convert(words);
return 0;
}
