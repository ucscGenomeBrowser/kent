/* makeRgbRainbow - List RGB values needed for rainbow of given size.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "rainbow.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "makeRgbRainbow - List RGB values needed for rainbow of given size.\n"
  "usage:\n"
  "   makeRgbRainbow size output\n"
  "options:\n"
  "   -light - If set make pastel rainbow\n"
  "   -out=fmt Output format, one of 'tab' or 'C' or 'ra'\n"
  );
}

static struct optionSpec options[] = {
   {"light", OPTION_BOOLEAN},
   {"out", OPTION_STRING},
   {NULL, 0},
};

boolean light = FALSE;
char *out = "tab";

void makeRgbRainbow(char *sizeString, char *outFile)
/* makeRgbRainbow - List RGB values needed for rainbow of given size.. */
{
int i, size = sqlUnsigned(sizeString);
double pos, step = 1.0/size;
FILE *f = mustOpen(outFile, "w");
struct rgbColor (*colAtPos)(double pos) = (light ? lightRainbowAtPos : saturatedRainbowAtPos);

for (i=0, pos=0; i<size; ++i, pos+=step)
    {
    struct rgbColor c = colAtPos(pos);
    if (sameString(out, "C"))
	fprintf(f, "{%d,%d,%d},\n", c.r, c.g, c.b);
    else if (sameString(out, "tab"))
        fprintf(f, "%d\t%d\t%d\n", c.r, c.g, c.b);
    else if (sameString(out, "ra"))
        fprintf(f, "color %d,%d,%d\n", c.r, c.g, c.b);
    else
        errAbort("Unrecognized out type '%s'", out);
    }
    
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
light = optionExists("light");
out = optionVal("out", out);
makeRgbRainbow(argv[1], argv[2]);
return 0;
}
