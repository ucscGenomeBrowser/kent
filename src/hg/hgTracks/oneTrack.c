/* oneTrack: load and draw a single track.  This is written for use in 
 * GBrowse plugins but could find other uses. */

#include "common.h"
#include "memgfx.h"
#include "cart.h"
#include "hdb.h"
#include "hCommon.h"
#include "hvGfx.h"
#include "hgTracks.h"

static char const rcsid[] = "$Id: oneTrack.c,v 1.1.4.1 2008/07/30 23:53:16 angie Exp $";

static int gfxBorder = hgDefaultGfxBorder;

void oneTrackInit()
/* Set up global variables using cart settings and initialize libs. */
{
hPrintDisable();
database = cartUsualString(cart, "db", "ce4");
hSetDb(database);
organism = hOrganism(database);
withLeftLabels = cartUsualBoolean(cart, "leftLabels", FALSE);
withCenterLabels = cartUsualBoolean(cart, "centerLabels", FALSE);
withGuidelines = cartUsualBoolean(cart, "guidelines", FALSE);
revCmplDisp = cartUsualBooleanDb(cart, database, REV_CMPL_DISP, FALSE);
position = cartUsualString(cart, "position", "default");
hgParseChromRange(position, &chromName, &winStart, &winEnd);
insideWidth = cartUsualInt(cart, "pix", 640);
leftLabelX = 0;
winBaseCount = winEnd - winStart;
basesPerPixel = ((float)winBaseCount) / ((float)insideWidth);
if (withLeftLabels)
    {
    trackTabWidth = cartUsualInt(cart, "trackTabWidth", trackTabWidth);
    }
initTl();
if (withLeftLabels)
    {
    /* taken from makeActiveImage */
    leftLabelX = gfxBorder;
    insideX = tl.leftLabelWidth + gfxBorder;
    leftLabelWidth = insideX - gfxBorder*3;
    if (trackTabWidth > 0)
	{
	int butOff = gfxBorder + trackTabWidth;
	leftLabelX += butOff;
	leftLabelWidth -= butOff;
	}
    tl.picWidth += insideX + gfxBorder;
    }
else
    {
    insideX = 0;
    leftLabelWidth = 0;
    }
zoomedToBaseLevel = (winBaseCount <= insideWidth / tl.mWidth);
zoomedToCodonLevel = (ceil(winBaseCount/3) * tl.mWidth) <= insideWidth;
zoomedToCdsColorLevel = (winBaseCount <= insideWidth*3);
seqBaseCount = hChromSize(chromName);
createHgFindMatchHash();
}

static int doOwnLeftLabels(struct track *track, struct hvGfx *hvg, 
                                                MgFont *font, int y)
/* Track draws its own, custom left labels. */
{
int pixWidth = tl.picWidth;
int fontHeight = mgFontLineHeight(font);
int tHeight = trackPlusLabelHeight(track, fontHeight);
Color labelColor = (track->labelColor ? track->labelColor : track->ixColor);
if (track->limitedVis == tvPack)
    hvGfxSetClip(hvg, gfxBorder+trackTabWidth+1, y, 
              pixWidth-2*gfxBorder-trackTabWidth-1, track->height);
else
    hvGfxSetClip(hvg, leftLabelX, y, leftLabelWidth, tHeight);
track->drawLeftLabels(track, winStart, winEnd,
		      hvg, leftLabelX, y, leftLabelWidth, tHeight,
		      isWithCenterLabels(track), font, labelColor, 
		      track->limitedVis);
hvGfxUnclip(hvg);
y += tHeight;
return y;
}

static void doLeftLabels(struct track *track, struct hvGfx *hvg, MgFont *font, 
			 int y)
/* Draw left labels. */
{
struct slList *prev = NULL;

/* for sample tracks */
double minRangeCutoff, maxRangeCutoff;
char minRangeStr[32];
char maxRangeStr[32];

int ymin, ymax;
int start;
int newy;
char o4[128];
char o5[128];
struct slList *item;
enum trackVisibility vis = track->limitedVis;
enum trackVisibility savedVis = vis;
Color labelColor = (track->labelColor ? 
                        track->labelColor : track->ixColor);
int fontHeight = mgFontLineHeight(font);
int tHeight = trackPlusLabelHeight(track, fontHeight);
if (vis == tvHide)
    return;

/*  if a track can do its own left labels, don't do it here */
if (track->drawLeftLabels != NULL)
    return;

/*	Wiggle tracks depend upon clipping.  They are reporting
 *	totalHeight artifically high by 1 so this will leave a
 *	blank area one pixel high below the track.
 */
if (sameString("wig",track->tdb->type))
    hvGfxSetClip(hvg, leftLabelX, y, leftLabelWidth, tHeight-1);
else
    hvGfxSetClip(hvg, leftLabelX, y, leftLabelWidth, tHeight);

safef( o4, sizeof(o4),"%s.min.cutoff", track->mapName);
safef( o5, sizeof(o5),"%s.max.cutoff", track->mapName);
minRangeCutoff = max( atof(cartUsualString(cart,o4,"0.0"))-0.1, 
                                track->minRange );
maxRangeCutoff = min( atof(cartUsualString(cart,o5,"1000.0"))+0.1, 
                                track->maxRange);
sprintf( minRangeStr, "%d", (int)round(minRangeCutoff));
sprintf( maxRangeStr, "%d", (int)round(maxRangeCutoff));

/* special label handling for wigMaf type tracks -- they
   display a left label in pack mode.  To use the full mode
   labeling, temporarily set visibility to full.
   Restore savedVis later */
if (startsWith("wigMaf", track->tdb->type) || startsWith("maf", track->tdb->type))
    vis = tvFull;

switch (vis)
    {
    case tvHide:
        break;	/* Do nothing; */
    case tvPack:
    case tvSquish:
	y += tHeight;
        break;
    case tvFull:
        if (isWithCenterLabels(track))
            y += fontHeight;
        start = 1;

	// wig and wigMaf use lfSubSample.
        if( track->subType == lfSubSample && track->items == NULL )
            y += track->height;

        for (item = track->items; item != NULL; item = item->next)
            {
            char *rootName;
            char *name = track->itemName(track, item);
            int itemHeight = track->itemHeight(track, item);
            newy = y;

            if (track->itemLabelColor != NULL)
                labelColor = track->itemLabelColor(track, item, hvg);
            
            /* Do some fancy stuff for sample tracks. 
             * Draw y-value limits for 'sample' tracks. */
            if(track->subType == lfSubSample )
                {
                
                if( prev == NULL )
                    newy += itemHeight;
                else
                    newy += sampleUpdateY(name, 
                            track->itemName(track, prev), itemHeight);
                if( newy == y )
                    continue;

                if( track->heightPer > (3 * fontHeight ) )
                    {
                    ymax = y - (track->heightPer / 2) + (fontHeight / 2);
                    ymin = y + (track->heightPer / 2) - (fontHeight / 2);
                    hvGfxTextRight(hvg, leftLabelX, ymin, leftLabelWidth-1,
                                itemHeight, track->ixAltColor, 
                                font, minRangeStr );
                    hvGfxTextRight(hvg, leftLabelX, ymax, leftLabelWidth-1,
                                itemHeight, track->ixAltColor, 
                                font, maxRangeStr );
                    }
                prev = item;

                rootName = cloneString(name);
                chopPrefix(rootName);
		hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth - 1, 
			       itemHeight, track->ixColor, font, rootName );
                freeMem( rootName );
                start = 0;
                y = newy;
                }
            else
                {
                /* standard item labeling */
		if (highlightItem(track, item))
		    {
		    int nameWidth = mgFontStringWidth(font, name);
		    int boxStart = leftLabelX + leftLabelWidth - 2 - nameWidth;
		    hvGfxBox(hvg, boxStart, y, nameWidth+1, itemHeight - 1,
			     labelColor);
		    hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth-1, 
				   itemHeight, MG_WHITE, font, name);
		    }
		else
		    hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth - 1, 
				   itemHeight, labelColor, font, name);
                y += itemHeight;
                }
            }
        break;
    case tvDense:
        if (isWithCenterLabels(track))
            y += fontHeight;
        /* draw y-value limits for 'sample' tracks (e.g. wig, wigMaf). 
         * (always puts 0-100% range)*/
        if( track->subType == lfSubSample && 
                track->heightPer > (3 * fontHeight ) )
            {
            ymax = y - (track->heightPer / 2) + (fontHeight / 2);
            ymin = y + (track->heightPer / 2) - (fontHeight / 2);
            hvGfxTextRight(hvg, leftLabelX, ymin, 
                        leftLabelWidth-1, track->lineHeight, 
                        track->ixAltColor, font, minRangeStr );
            hvGfxTextRight(hvg, leftLabelX, ymax, 
                        leftLabelWidth-1, track->lineHeight, 
                        track->ixAltColor, font, maxRangeStr );
            }
        hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth-1, 
                    track->lineHeight, labelColor, font, 
                    track->shortLabel);
        y += track->height;
        break;
    }
if (sameString(track->tdb->type, "wigMaf"))
    vis = savedVis;
hvGfxUnclip(hvg);
return;
}

struct hvGfx *oneTrackMakeTrackHvg(char *trackName, char *gifName)
/* Set up a single track, load it, draw it and return the graphics object: */
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

struct hvGfx *hvg = hvGfxOpenGif(tl.picWidth, height, gifName);
initColors(hvg);
findTrackColors(hvg, tg);

/* This is the only image we'll draw, so offset=(0,0). */
int xOff = (insideX+gfxBorder), yOff = 0;
MgFont *font = tl.font;
hvGfxSetClip(hvg, xOff, yOff, insideWidth, tg->height);
tg->drawItems(tg, winStart, winEnd, hvg, xOff, yOff, insideWidth, font,
	      hvGfxFindRgb(hvg, &tg->color), tg->limitedVis);

if (withLeftLabels)
    {
    if (isCompositeTrack(tg))
	{
	struct track *subtrack;
	for (subtrack = tg->subtracks; subtrack != NULL;
	     subtrack = subtrack->next)
	    if (isSubtrackVisible(subtrack))
		{
		if (subtrack->drawLeftLabels != NULL)
		    doOwnLeftLabels(subtrack, hvg, font, 0);
		else
		    doLeftLabels(subtrack, hvg, font, 0);
		}
	}
    else if (tg->drawLeftLabels != NULL)
	doOwnLeftLabels(tg, hvg, font, 0);
    else
	doLeftLabels(tg, hvg, font, 0);
    }

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

