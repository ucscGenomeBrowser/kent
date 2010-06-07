/* testPoly - Test polygon drawing code. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "memgfx.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testPoly - Test polygon drawing code\n"
  "usage:\n"
  "   testPoly outGif\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void testPoly(char *outGif)
/* testPoly - Test polygon drawing code. */
{
struct memGfx *mg = mgNew(500, 300);
struct gfxPoly *poly = gfxPolyNew();
mgCircle(mg, 250, 150, 100, MG_GREEN, TRUE);
gfxPolyAddPoint(poly, 25, 25);
gfxPolyAddPoint(poly, 475, 25);
gfxPolyAddPoint(poly, 475, 275);
gfxPolyAddPoint(poly, 25, 275);
mgDrawPoly(mg, poly, MG_RED, FALSE);
gfxPolyFree(&poly);

poly = gfxPolyNew();
gfxPolyAddPoint(poly, 50, 50);
gfxPolyAddPoint(poly, 100, 100);
gfxPolyAddPoint(poly, 50, 150);
gfxPolyAddPoint(poly, 75, 100);
mgDrawPoly(mg, poly, MG_BLUE, TRUE);
gfxPolyFree(&poly);

poly = gfxPolyNew();
gfxPolyAddPoint(poly, 150, 50);
gfxPolyAddPoint(poly, 200, 100);
gfxPolyAddPoint(poly, 250, 50);
gfxPolyAddPoint(poly, 200, 75);
mgDrawPoly(mg, poly, MG_GRAY, TRUE);
gfxPolyFree(&poly);

mgSaveGif(mg, outGif);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
testPoly(argv[1]);
return 0;
}
