/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "vGfx.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen file");
}

void freen(char *fileName)
/* Print status code. */
{
struct vGfx *vg = vgOpenPostScript(500, 500, fileName);
// struct vGfx *vg = vgOpenGif(500, 500, fileName);
MgFont *smallFont = mgSmallFont();
MgFont *bigFont = mgLargeFont();
int smallHeight = mgFontPixelHeight(smallFont);
int bigHeight = mgFontPixelHeight(bigFont);
int grayIx = vgFindColorIx(vg, 200, 150, 150);
int blueIx = vgFindColorIx(vg, 150, 150, 200);
vgBox(vg, 100, 100, 300, smallHeight, blueIx);
vgText(vg, 100, 100, MG_BLACK, smallFont, "Help Small World");
vgBox(vg, 100, 300, 300, bigHeight, blueIx);
vgText(vg, 100, 300, MG_BLACK, bigFont, "Help BIG World");
vgBox(vg, 25, 25, 50, 50, grayIx);
vgLine(vg, 25, 25, 75, 75, MG_RED);
vgLine(vg, 25, 75, 75, 25, MG_RED);
vgClose(&vg);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
}
