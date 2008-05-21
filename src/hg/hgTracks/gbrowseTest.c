/* Test harness: dump a gif of ce4.gc5Base, chrI:4001-5000. */

#include "common.h"
#include "memgfx.h"
#include "cart.h"
#include "hdb.h"
#include "hvGfx.h"
#include "hgTracks.h"

struct cart *cart = NULL;
struct track *trackList = NULL;

void gbrInit()
/* Set up global variables including cart and initialize libs. 
 * Call cgiSpoof first. */
{
hPrintDisable();
cart = cartNewEmpty(0, 0, NULL, NULL);
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

void gbrMakeTrackGif(char *trackName, char *gifName)
/* Set up a single track, load it, draw it and dump gif: */
{
char *visStr = cloneString(
	cartUsualString(cart, trackName, hTrackOpenVis(trackName)) );
cartSetString(cart, trackName, visStr);
struct trackDb *tdb = hTrackDbForTrack(trackName);
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

hvGfxClose(&hvg);
}

int main(int argc, char **argv)
/* Test harness -- dump gif to
 * http://hgwdev.cse.ucsc.edu/~angie/gbrowseTest.gif */
{
cgiSpoof(&argc, argv);
gbrInit();
/* Now set up a single track, load it and draw it: */
char *trackName = cartUsualString(cart, "gbrowseTrack", "multiz5way");
gbrMakeTrackGif(trackName, "/cluster/home/angie/public_html/gbrowseTest.gif");
return 0;
}
