/* oneTrack: load and draw a single track.  This is written for use in 
 * GBrowse plugins but could find other uses. */

#include "common.h"
#include "memgfx.h"
#include "cart.h"
#include "hdb.h"
#include "hvGfx.h"
#include "hgTracks.h"

static char const rcsid[] = "$Id: oneTrack.c,v 1.2 2008/08/27 19:16:57 tdreszer Exp $";

void oneTrackInit()
/* Set up global variables using cart settings and initialize libs. */
{
hPrintDisable();
database = cartUsualString(cart, "db", "ce4");
hSetDb(database);
organism = hOrganism(database);
withLeftLabels = FALSE; /* Left labels are not supported. */
withCenterLabels = cartUsualBoolean(cart, "centerLabels", FALSE);
withGuidelines = cartUsualBoolean(cart, "guidelines", FALSE);
revCmplDisp = cartUsualBooleanDb(cart, database, REV_CMPL_DISP, FALSE);
position = cartUsualString(cart, "position", "chrI:4001-5000");
hgParseChromRange(position, &chromName, &winStart, &winEnd);
insideX = 0; /* Left labels are not supported. */
insideWidth = cartUsualInt(cart, "pix", 640);
leftLabelX = 0;
leftLabelWidth = 0; /* Left labels are not supported. */
winBaseCount = winEnd - winStart;
basesPerPixel = ((float)winBaseCount) / ((float)insideWidth);
zoomedToCdsColorLevel = (winBaseCount <= insideWidth*3);
seqBaseCount = hChromSize(chromName);
initTl();
zoomedToBaseLevel = (winBaseCount <= insideWidth / tl.mWidth);
zoomedToCodonLevel = (ceil(winBaseCount/3) * tl.mWidth) <= insideWidth;
createHgFindMatchHash();
}

struct hvGfx *oneTrackMakeTrackHvg(char *trackName, char *gifName)
/* Set up a single track, load it, draw it and return the graphics object: */
{
char *visStr = cloneString(
	cartUsualString(cart, trackName, hTrackOpenVis(trackName)) );
cartSetString(cart, trackName, visStr);
struct trackDb *tdb = hTrackDbForTrack(trackName);
tdbSortPrioritiesFromCart(cart, &tdb);
if(tdb->subtracks)
    tdbSortPrioritiesFromCart(cart, &(tdb->subtracks));
struct track *tg = trackFromTrackDb(tdb);
tg->visibility = hTvFromString(visStr);
trackList = tg;

tg->loadItems(tg);
limitVisibility(tg);
int height = tg->totalHeight(tg, tg->limitedVis);

struct hvGfx *hvg = hvGfxOpenGif(insideWidth, height, gifName);
initColors(hvg);
findTrackColors(hvg, tg);

/* This is the only image we'll draw, so offset=(0,0). */
int xOff = 0, yOff = 0;
MgFont *font = tl.font;
hvGfxSetClip(hvg, xOff, yOff, insideWidth, tg->height);
tg->drawItems(tg, winStart, winEnd, hvg, xOff, yOff, insideWidth, font,
	      hvGfxFindRgb(hvg, &tg->color), tg->limitedVis);

/* MEMORY LEAK -- Freeing items slows hgTracks down unacceptably so we
 * don't do it, and it has become vestigial.  For example, wigFreeItems 
 * is a no-op because the author noted that it is never invoked. */
#ifdef SLOW
tg->freeItems(tg);
#endif /* SLOW */

return hvg;
}

static void packC(unsigned char *vec, int *pI, unsigned char val)
/* Emulate perl's pack("C"): unsigned char. */
{
vec[(*pI)++] = val;
}

static void packn(unsigned char *vec, int *pI, unsigned short val)
/* Emulate perl's pack("n"): "network order" (big-endian) short. */
{
vec[(*pI)++] = (val >> 8) & 0xff;
vec[(*pI)++] = val & 0xff;
}

static void packN(unsigned char *vec, int *pI, unsigned long val)
/* Emulate perl's pack("N"): "network order" (big-endian) long. */
{
vec[(*pI)++] = (val >> 24) & 0xff;
vec[(*pI)++] = (val >> 16) & 0xff;
vec[(*pI)++] = (val >> 8) & 0xff;
vec[(*pI)++] = val & 0xff;
}

unsigned char *oneTrackHvgToUcscGlyph(struct hvGfx *hvg, int *retVecSize)
/* Translate the memGfx implementation of hvGfx into a byte vector which 
 * GBrowse's Bio::Graphics::Glyph::ucsc_glyph will render into the main
 * GBrowse image. */
{
struct memGfx *mg = (struct memGfx *)(hvg->vg->data);
int vecSize = 3*4 + mg->width*mg->height + 2 + mg->colorsUsed*3;
unsigned char *vec = needLargeMem(vecSize);
unsigned long magicNum = 0xb1d3ab30;
int i = 0, x, y;
packN(vec, &i, magicNum);
packN(vec, &i, mg->width);
packN(vec, &i, mg->height);
for (y = 0;  y < mg->height;  y++)
    for (x = 0;  x < mg->width;  x++)
	{
	packC(vec, &i, mg->pixels[y*mg->width + x]);
	}
packn(vec, &i, mg->colorsUsed);
for (x = 0;  x < mg->colorsUsed;  x++)
    {
    struct rgbColor rgb = mg->colorMap[x];
    packC(vec, &i, rgb.r);
    packC(vec, &i, rgb.g);
    packC(vec, &i, rgb.b);
    }
if (i != vecSize)
    errAbort("oneTrackHvgToByteVector: vector should be %d bytes long "
	     "but it's %d.\n", vecSize, i);
if (retVecSize != NULL)
    *retVecSize = vecSize;
return vec;
}

void oneTrackCloseAndWriteImage(struct hvGfx **pHvg)
/* Write out the graphics object to the filename with which it was created.
 * This also destroys the graphics object! */
{
hvGfxClose(pHvg);
}

