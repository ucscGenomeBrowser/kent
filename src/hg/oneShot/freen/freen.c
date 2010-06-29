/* freen - My Pet Freen. */
#include <unistd.h>
#include <math.h>

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "memgfx.h"

#define TRAVERSE FALSE

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

struct rgbaColor
    {
    unsigned char r, g, b, a;
    };


struct rgbaColor rgbaLightRainbowColor(double pos)
/* Giving a position 0.0 to 1.0 in rainbow, return color. */
{
double maxHue = 360.0;
struct hslColor hsl = {pos*maxHue, 1000, 750};
struct rgbColor rgb = mgHslToRgb(hsl);
struct rgbaColor rgba = {rgb.r, rgb.g, rgb.b, 255};
return rgba;
}




void freen(char *countString)
/* Test some hair-brained thing. */
{
int count = atoi(countString);
if (count < 1)
   errAbort("count of colors must be positive");
double stepSize = 1.0/count;
int i;
for (i=0; i<count; ++i)
    {
    struct rgbaColor col = rgbaLightRainbowColor(stepSize*i);
    printf("%d,%d,%d\n", col.r, col.g, col.b);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
// optionInit(&argc, argv, options);

if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
