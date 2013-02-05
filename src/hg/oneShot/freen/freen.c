/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "ra.h"
#include "jksql.h"
#include "trackDb.h"
#include "hui.h"
#include "rainbow.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

struct rgbColor saturatedRainbowTable[28] = {
/* This table was built by hand for the default Autodesk Animator palette. */
    {255, 0, 64},
    {255, 0, 0},
    {255, 64, 0},
    {255, 128, 0},
    {255, 164, 0},
    {255, 210, 0},
    {255, 255, 0},
    {210, 255, 0},
    {164, 255, 0},
    {128, 255, 0},
    {0, 255, 0},
    {0, 255, 128},
    {0, 255, 164},
    {0, 255, 210},
    {0, 255, 255},
    {0, 210, 255},
    {0, 164, 255},
    {0, 128, 255},
    {0, 64, 255},
    {0, 0, 255},
    {64, 0, 255},
    {128, 0, 255},
    {164, 0, 255},
    {210, 0, 255},
    {255, 0, 255},
    {255, 0, 210},
    {255, 0, 164},
    {255, 0, 128},
    };

int lighten(int col)
/* Return shade blended with 50% parts 255. */
{
return round(col * 0.5 + 255 * 0.5);
}

void freen(char *input)
/* Test some hair-brained thing. */
{
int i;
for (i=0; i<28; ++i)
   {
   struct rgbColor *c = &saturatedRainbowTable[i];
   printf("   {%d,%d,%d},\n", lighten(c->r), lighten(c->g), lighten(c->b));
   }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
