/* joinedRmskTrack - A comprehensive RepeatMasker visualization track
 *                   handler. This is an extension of the original
 *                   rmskTrack.c written by UCSC.
 *
 *  Written by Robert Hubley 10/2012-01/2015
 *
 *  Modifications:
 *
 *  1/2015
 *     Request to revert back to only Full/Dense modes.
 *
 *  11/2014
 *     Request for traditional pack/squish modes.
 *
 *  7/2014
 *     With the help of Jim Kent we modified the
 *     glyph design to more closely resemble the
 *     existing browser styles.
 */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "rmskJoined.h"


/* DETAIL_VIEW_MAX_SCALE: The size ( max bp ) at which the
 * detailed view of repeats is turned on, otherwise it reverts
 * back to the original UCSC glyph.
 */
#define DETAIL_VIEW_MAX_SCALE 30000

/* MAX_UNALIGNED_PIXEL_LEN: The maximum size of unaligned sequence
 * to draw before switching to the unscaled view: ie. "----/###/---"
 * This is measured in pixels *not* base pairs due to the
 * mixture of scaled ( lines ) and unscaled ( text ) glyph
 * components.
 */
#define MAX_UNALIGNED_PIXEL_LEN 150

// TODO: Document
#define LABEL_PADDING 5
#define MINHEIGHT 24

// TODO: Document
static float pixelsPerBase = 1.0;


struct windowGlobals
{
struct windowGlobals *next;
struct window *window;
/* These are the static globals needed for each window: */
struct hash *classHash;
struct repeatItem *otherRepeatItem;
struct hash *subTracksHash;
};

static struct windowGlobals *wgWindows = NULL;
static struct windowGlobals *wgWindow = NULL;
static struct windowGlobals *wgWindowOld = NULL;

static bool setWgWindow()
/* scan genoCache windows. create new one if not found */
{
if (wgWindow && wgWindow->window == currentWindow)
    return FALSE;
wgWindowOld = wgWindow;
for (wgWindow = wgWindows; wgWindow; wgWindow =  wgWindow->next)
    {
    if (wgWindow->window == currentWindow)
	{
	return TRUE;
	}
    }
AllocVar(wgWindow);
wgWindow->window = currentWindow;
slAddTail(&wgWindows, wgWindow);
return TRUE;
}

// per window static vars act like local global vars, need one per window

/* Hash of the repeatItems ( tg->items ) for this track.
 *   This is used to hold display names for the lines of
 *   the track, the line heights, and class colors.
 */
struct hash *classHash = NULL;
static struct repeatItem *otherRepeatItem = NULL;

/* Hash of subtracks
 *   The joinedRmskTrack is designed to be used as a composite track.
 *   When rmskJoinedLoadItems is called this hash is populated with the
 *   results of one or more table queries
 */
struct hash *subTracksHash = NULL;


static void setWg()
/* set up per-window globals for the current window */
{
if (setWgWindow())
    {
    if (wgWindowOld)
	{
	wgWindowOld->classHash       = classHash;
	wgWindowOld->otherRepeatItem = otherRepeatItem;
    	wgWindowOld->subTracksHash   = subTracksHash;
	}
    classHash       = wgWindow->classHash;
    otherRepeatItem = wgWindow->otherRepeatItem;
    subTracksHash   = wgWindow->subTracksHash;
    }

}



struct subTrack
{
  // The number of display levels used in levels[]
int levelCount;
  // The rmskJoined records from table query
struct rmskJoined **levels;
};


// Repeat Classes: Display names
static char *rptClassNames[] = {
"SINE", "LINE", "LTR", "DNA", "Simple", "Low Complexity", "Satellite",
"RNA", "Other", "Unknown",
};

// Repeat Classes: Data names
static char *rptClasses[] = {
"SINE", "LINE", "LTR", "DNA", "Simple_repeat", "Low_complexity",
"Satellite", "RNA", "Other", "Unknown",
};


/* Repeat class to color mappings.
 *   - Currently borrowed from snakePalette.
 *
 *  NOTE: If these are changed, do not forget to update the
 *        help table in joinedRmskTrack.html
 */
static Color rmskJoinedClassColors[] = {
0xff1f77b4,			// SINE - red
0xffff7f0e,			// LINE - lime
0xff2ca02c,			// LTR - maroon
0xffd62728,			// DNA - fuchsia
0xff9467bd,			// Simple - yellow
0xff8c564b,			// LowComplex - olive
0xffe377c2,			// Satellite - blue
0xff7f7f7f,			// RNA - green
0xffbcbd22,			// Other - teal
0xff17becf,			// Unknown - aqua
};


static void getExtents(struct rmskJoined *rm, int *pStart, int *pEnd)
/* Calculate the glyph extents in genome coordinate
 * space ( ie. bp )
 *
 * It is not straightforward to determine the extent
 * of this track's glyph.  This is due to the mixture
 * of graphic and text elements within the glyph.
 *
 */
{
int start;
int end;
char lenLabel[20];

// Start position is anchored by the alignment start
// coordinates.  Then we subtract from that space for
// the label.
start = rm->alignStart -
    (int) ((mgFontStringWidth(tl.font, rm->name) +
	    LABEL_PADDING) / pixelsPerBase);

// Now subtract out space for the unaligned sequence
// bar starting off the graphics portion of the glyph.
if ((rm->blockSizes[0] * pixelsPerBase) > MAX_UNALIGNED_PIXEL_LEN)
    {
    // Unaligned sequence bar needs length label -- account for that.
    safef(lenLabel, sizeof(lenLabel), "%d", rm->blockSizes[0]);
    start -= (int) (MAX_UNALIGNED_PIXEL_LEN / pixelsPerBase) +
	(int) ((mgFontStringWidth(tl.font, lenLabel) +
		LABEL_PADDING) / pixelsPerBase);
    }
else
    {
    start -= rm->blockSizes[0];
    }

end = rm->alignEnd;

if ((rm->blockSizes[rm->blockCount - 1] * pixelsPerBase) >
    MAX_UNALIGNED_PIXEL_LEN)
    {
    safef(lenLabel, sizeof(lenLabel), "%d",
	   rm->blockSizes[rm->blockCount - 1]);
    end +=
	(int) (MAX_UNALIGNED_PIXEL_LEN / pixelsPerBase) +
	(int) ((mgFontStringWidth(tl.font, lenLabel) +
		LABEL_PADDING) / pixelsPerBase);
    }
else
    {
    end += rm->blockSizes[rm->blockCount - 1];
    }

*pStart = start;
*pEnd = end;
}

// A better way to organize the display
static int cmpRepeatDiv(const void *va, const void *vb)
/* Sort repeats by divergence. TODO: Another way to do this
   would be to sort by containment.  How many annotations
   are contained within the repeat.  This would require
   a "contained_count" be precalculated for each annotation.
 */
{
struct rmskJoined *a = *((struct rmskJoined **) va);
struct rmskJoined *b = *((struct rmskJoined **) vb);

return (b->score - a->score);
}

struct repeatItem *classRIList = NULL;
struct repeatItem *fullRIList = NULL;

static struct repeatItem * makeJRepeatItems()
/* Initialize the track */
{
setWg();
int i;
struct repeatItem *ri = NULL;

// Build a permanent hash for mapping
// class names to colors.  Used by glyph
// drawing routine.  Also build the
// classRIList primarily for use in tvDense
// mode.
if (!classHash)
    {
    classHash = newHash(6);
    int numClasses = ArraySize(rptClasses);
    for (i = 0; i < numClasses; ++i)
        {
        AllocVar(ri);
        ri->class = rptClasses[i];
        ri->className = rptClassNames[i];
        // New color attribute
        ri->color = rmskJoinedClassColors[i];
        slAddHead(&classRIList, ri);
        // Hash now prebuilt to hold color attributes
        hashAdd(classHash, ri->class, ri);
        if (sameString(rptClassNames[i], "Other"))
            otherRepeatItem = ri;
        }
    slReverse(&classRIList);
    }
return classRIList;
}



static void rmskJoinedLoadItems(struct track *tg)
/* We do the query(ies) here so we can report how deep the track(s)
 * will be when rmskJoinedTotalHeight() is called later on.
 */
{
  /*
   * Initialize the subtracks hash - This will eventually contain
   * all the repeat data for each displayed subtrack.
   */

setWg();

if (!subTracksHash)
    subTracksHash = newHash(20);

//tg->items = makeJRepeatItems();
 makeJRepeatItems();

int baseWidth = winEnd - winStart;
pixelsPerBase = (float) insideWidth / (float) baseWidth;
//Disabled
//if ((tg->visibility == tvFull || tg->visibility == tvSquish ||
//     tg->visibility == tvPack) && baseWidth <= DETAIL_VIEW_MAX_SCALE)
if (tg->visibility == tvFull && baseWidth <= DETAIL_VIEW_MAX_SCALE)
    {
    struct repeatItem *ri = NULL;
    struct subTrack *st = NULL;
    AllocVar(st);
    st->levelCount = 0;
    struct rmskJoined *rm = NULL;
    char **row;
    int rowOffset;
    struct sqlConnection *conn = hAllocConn(database);
    struct sqlResult *sr = hRangeQuery(conn, tg->table, chromName,
                                        winStart, winEnd, NULL,
                                        &rowOffset);
    struct rmskJoined *detailList = NULL;
    while ((row = sqlNextRow(sr)) != NULL)
        {
        rm = rmskJoinedLoad(row + rowOffset);
        slAddHead(&detailList, rm);
        }
    slSort(&detailList, cmpRepeatDiv);

    sqlFreeResult(&sr);
    hFreeConn(&conn);

    if (detailList)
	AllocArray( st->levels, slCount(detailList));

    while (detailList)
        {
        st->levels[st->levelCount++] = detailList;

        struct rmskJoined *cr = detailList;
        detailList = detailList->next;
        cr->next = NULL;
        int crChromStart, crChromEnd;
        int rmChromStart, rmChromEnd;
        struct rmskJoined *prev = NULL;
        rm = detailList;

        getExtents(cr, &crChromStart, &crChromEnd);

        if ( tg->visibility == tvFull )
	    {
	    AllocVar(ri);
	    ri->className = cr->name;
	    slAddHead(&fullRIList, ri);
	    }

        //Disabled
        //if ( tg->visibility == tvSquish || tg->visibility == tvPack )
        if ( tg->visibility == tvFull )
             {
             while (rm)
                 {
                 getExtents(rm, &rmChromStart, &rmChromEnd);

                 if (rmChromStart > crChromEnd)
                     {
                     cr->next = rm;
                     cr = rm;
                     crChromEnd = rmChromEnd;
                     if (prev)
                         prev->next = rm->next;
                     else
                         detailList = rm->next;

                     rm = rm->next;
                     cr->next = NULL;
                     }
                 else
                     {
                     // Save for a lower level
                     prev = rm;
                     rm = rm->next;
                     }
                 } // while ( rm )
             } // else if ( tg->visibility == tvSquish ...
        } // while ( detailList )
    // Create Hash Entry
    hashReplace(subTracksHash, tg->table, st);
    slReverse(&fullRIList);
    tg->items = fullRIList;
    } // if ((tg->visibility == tvFull || ...
else
   tg->items = classRIList;
}

static void rmskJoinedFreeItems(struct track *tg)
/* Free up rmskJoinedMasker items. */
{
slFreeList(&tg->items);
}

static char * rmskJoinedName(struct track *tg, void *item)
/* Return name of rmskJoined item track. */
{
struct repeatItem *ri = item;
  /*
   * In detail view mode the items represent different packing
   * levels.  No need to display a label at each level.  Instead
   * Just return a label for the first level.
   */
//Disabled
//if ((tg->visibility == tvSquish ||
//     tg->visibility == tvPack) && winBaseCount <= DETAIL_VIEW_MAX_SCALE)
if (tg->visibility == tvFull && winBaseCount <= DETAIL_VIEW_MAX_SCALE)
    {
    if (strcmp(ri->className, "SINE") == 0)
	return "Repeats";
    else
	return "";
    }
return ri->className;
}


int rmskJoinedItemHeight(struct track *tg, void *item)
{
  // Are we in full view mode and at the scale needed to display
  // the detail view?
//Disabled
//if (tg->limitedVis == tvSquish && winBaseCount <= DETAIL_VIEW_MAX_SCALE )
//    {
//    if ( tg->heightPer < (MINHEIGHT/2) )
//      return (MINHEIGHT/2);
//    else
//      return tg->heightPer;
//    }
//else if ((tg->limitedVis == tvFull || tg->limitedVis == tvPack) && winBaseCount <= DETAIL_VIEW_MAX_SCALE)
if (tg->limitedVis == tvFull && winBaseCount <= DETAIL_VIEW_MAX_SCALE)
    {
    return max(tg->heightPer, MINHEIGHT);
    }
else
    {
      return tgFixedItemHeight(tg, item);
    }
}

int rmskJoinedTotalHeight(struct track *tg, enum trackVisibility vis)
{
setWg();
  // Are we in full view mode and at the scale needed to display
  // the detail view?
//Disabled
//if ((tg->limitedVis == tvFull || tg->limitedVis == tvSquish ||
//     tg->limitedVis == tvPack) && winBaseCount <= DETAIL_VIEW_MAX_SCALE)
if (tg->limitedVis == tvFull && winBaseCount <= DETAIL_VIEW_MAX_SCALE)
    {
    // Lookup the depth of this subTrack and report it
    struct subTrack *st = hashFindVal(subTracksHash, tg->table);
    if (st)
        {
        tg->height = (st->levelCount * rmskJoinedItemHeight(tg, NULL));
        return tg->height;
        }
    else
        {
        tg->height = rmskJoinedItemHeight(tg, NULL);
        return tg->height;	// Just display one line
        }
    }
else
    {
    if ( vis == tvDense )
      {
      tg->height = tgFixedTotalHeightNoOverflow(tg, tvDense );
      return tgFixedTotalHeightNoOverflow(tg, tvDense );
      }
    else
      {
      tg->height = tgFixedTotalHeightNoOverflow(tg, tvFull );
      return tgFixedTotalHeightNoOverflow(tg, tvFull );
      }
    }
}

static void drawDashedHorizLine(struct hvGfx *hvg, int x1, int x2,
		     int y, int dashLen, int gapLen, Color lineColor)
// ie.    - - - - - - - - - - - - - - - -
{
int cx1 = x1;
int cx2;
while (1)
    {
    cx2 = cx1 + dashLen;
    if (cx2 > x2)
        cx2 = x2;
    hvGfxLine(hvg, cx1, y, cx2, y, lineColor);
    cx1 += (dashLen + gapLen);
    if (cx1 > x2)
        break;
    }
}

static void drawDashedHorizLineWHash(struct hvGfx *hvg, int x1, int x2,
			  int y, int dashLen, int gapLen, Color lineColor,
			  int unalignedLen)
// ie.    - - - - - - -\234\- - - - - - - - -
{
int cx1 = x1;
int cx2;

char lenLabel[20];
safef(lenLabel, sizeof(lenLabel), "%d", unalignedLen);
MgFont *font = tl.font;
int fontHeight = tl.fontHeight;
int stringWidth = mgFontStringWidth(font, lenLabel) + LABEL_PADDING;

int glyphWidth = x2 - x1;

if (glyphWidth < stringWidth + 6 + (2 * dashLen))
    stringWidth = 0;

int midX = ((glyphWidth) / 2) + x1;
int startHash = midX - (stringWidth * 0.5);
int midPointDrawn = 0;

  /*
   * Degrade Gracefully:
   *   Too little space to draw dashes or even
   *   hash marks, give up.
   */
if (glyphWidth < 6 + dashLen)
    {
    hvGfxLine(hvg, x1, y, x2, y, lineColor);
    return;
    }
if (glyphWidth < 6 + (2 * dashLen))
    {
    midX -= 3;
    hvGfxLine(hvg, x1, y, midX, y, lineColor);
    hvGfxLine(hvg, midX, y - 3, midX + 3, y + 3, lineColor);
    hvGfxLine(hvg, midX + 3, y - 3, midX + 6, y + 3, lineColor);
    hvGfxLine(hvg, midX + 6, y, x2, y, lineColor);
    return;
    }

while (1)
    {
    cx2 = cx1 + dashLen;
    if (cx2 > x2)
        cx2 = x2;

    if (!midPointDrawn && cx2 > startHash)
	{
	// Draw double slashes "\\" instead of dash
	hvGfxLine(hvg, cx1, y - 3, cx1 + 3, y + 3, lineColor);
	if (stringWidth)
	    {
	    hvGfxTextCentered(hvg, cx1 + 3, y - (fontHeight/2), stringWidth,
			       fontHeight, MG_BLACK, font, lenLabel);
	    cx1 += stringWidth;
	    }
	hvGfxLine(hvg, cx1 + 3, y - 3, cx1 + 6, y + 3, lineColor);
	cx1 += 6;
	midPointDrawn = 1;
	}
    else
	{
	// Draw a dash
	hvGfxLine(hvg, cx1, y, cx2, y, lineColor);
	cx1 += dashLen;
	}

    if (!midPointDrawn && cx1 + gapLen > midX)
	{
	// Draw double slashes "\\" instead of gap
	hvGfxLine(hvg, cx1, y - 3, cx1 + 3, y + 3, lineColor);
	if (stringWidth)
	    {
	    hvGfxTextCentered(hvg, cx1 + 3, y - 3, stringWidth,
			       fontHeight, MG_BLACK, font, lenLabel);
	    cx1 += stringWidth;
	    }
	hvGfxLine(hvg, cx1 + 3, y - 3, cx1 + 6, y + 3, lineColor);
	cx1 += 6;
	midPointDrawn = 1;
	}
    else
	{
	cx1 += gapLen;
	}

    if (cx1 > x2)
        break;
    }
}

static void drawNormalBox(struct hvGfx *hvg, int x1, int y1,
	       int width, int height, Color fillColor, Color outlineColor)
{
struct gfxPoly *poly = gfxPolyNew();
int y2 = y1 + height;
int x2 = x1 + width;
  /*
   *     1,5--------------2
   *       |              |
   *       |              |
   *       |              |
   *       4--------------3
   */
gfxPolyAddPoint(poly, x1, y1);
gfxPolyAddPoint(poly, x2, y1);
gfxPolyAddPoint(poly, x2, y2);
gfxPolyAddPoint(poly, x1, y2);
gfxPolyAddPoint(poly, x1, y1);
hvGfxDrawPoly(hvg, poly, fillColor, TRUE);
hvGfxDrawPoly(hvg, poly, outlineColor, FALSE);
gfxPolyFree(&poly);
}

static void drawBoxWChevrons(struct hvGfx *hvg, int x1, int y1,
		  int width, int height, Color fillColor, Color outlineColor,
		  char strand)
{
drawNormalBox(hvg, x1, y1, width, height, fillColor, outlineColor);
static Color white = 0xffffffff;
int dir = (strand == '+' ? 1 : -1);
int midY = y1 + ((height) >> 1);
clippedBarbs(hvg, x1 + 1, midY, width, ((height >> 1) - 2), 5,
	      dir, white, TRUE);
}

static void drawRMGlyph(struct hvGfx *hvg, int y, int heightPer,
	    int width, int baseWidth, int xOff, struct rmskJoined *rm,
            enum trackVisibility vis)
/*
 *  Draw a detailed RepeatMasker annotation glyph given
 *  a single rmskJoined structure.
 *
 *  A couple of caveats about the use of the rmskJoined
 *  structure to hold a RepeatMasker annotation.
 *
 *  chromStart/chromEnd:  These represent genomic coordinates
 *                        that mark the extents of graphics
 *                        annotation on the genome.  I.e
 *                        aligned blocks + unaligned consensus
 *                        blocks.  The code below may not
 *                        draw to these extents in some cases.
 *
 *  score:  This represents the average divergence of the
 *          individual aligned blocks.  It is a percentage
 *          * 100.  Ie. 23.11 % is encoded as the integer 2311.
 *
 *  blockRelStarts:  Two types of blocks are interwoven in this
 *                format.  Aligned and unaligned blocks.  Aligned
 *                blocks will have a value >= 0 representing the
 *                relative position from chromStart for this
 *                start of this block.  A value of -1 for chromStart
 *                is indicative of unaligned block and starts from
 *                the end of the last aligned block on the chromosome
 *                or from chromStart if it's the first block in the list.
 *
 *  name:  Stores the id#class/family
 *
 *  rgb:  Unused at this time
 *
 *  description: Unused but the intention would be to store
 *               the RepeatMasker *.out lines which make up
 *               all the aligned sections of this joined
 *               annotation.
 *
 *  blockSizes:   As you would expect.
 *
 *  Here is an example:
 *                ie.
 *                A forward strand RM annotation from chr1:186-196
 *                which is aligned to a 100 bp repeat from 75-84
 *                in the consensus would be represented as:
 *
 *                chromStart: 111
 *                chromEnd: 212
 *                blockRelStarts: -1, 75, -1
 *                blockSizes: 75, 10, 16
 *
 */
{
setWg();
int idx;
int lx1, lx2;
int cx1, cx2;
int w;
struct repeatItem *ri;
  /*
   * heightPer is the God given height of a single
   * track item...respect your overlord.
   */
int alignedBlockHeight = heightPer * 0.5;
int alignedBlockOffset = heightPer - alignedBlockHeight;
int unalignedBlockHeight = heightPer * 0.75;
int unalignedBlockOffset = heightPer - unalignedBlockHeight;

Color black = hvGfxFindColorIx(hvg, 0, 0, 0);
Color fillColor = shadesOfGray[5];
Color outlineColor = rmskJoinedClassColors[0];

  // Break apart the name and get the class of the
  // repeat.
char class[256];
  // Simplify repClass for lookup: strip trailing '?',
  // simplify *RNA to RNA:
char *poundPtr = index(rm->name, '#');
if (poundPtr)
    {
    safecpy(class, sizeof(class), poundPtr + 1);
    char *slashPtr = index(class, '/');
    if (slashPtr)
        *slashPtr = '\0';
    }
else
    {
    safecpy(class, sizeof(class), "Unknown");
    }
char *p = &(class[strlen(class) - 1]);
if (*p == '?')
    *p = '\0';
if (endsWith(class, "RNA"))
    safecpy(class, sizeof(class), "RNA");

  // Lookup the class to get the color scheme
ri = hashFindVal(classHash, class);
if (ri == NULL)
    ri = otherRepeatItem;

  // Pick the fill color based on the divergence
int percId = 10000 - rm->score;
int grayLevel = grayInRange(percId, 6000, 10000);
fillColor = shadesOfGray[grayLevel];
outlineColor = ri->color;

  // Draw from left to right
for (idx = 0; idx < rm->blockCount; idx++)
    {
    int fragGStart = rm->blockRelStarts[idx];

    /*
     *  Assumptions about blockCount
     *   - first aligned block = 1, the block 0 is
     *     the unaligned consnesus either 5' or 3' of
     *     this block.
     *   - The last aligned block is blockCount - 1;
     */
    if (fragGStart > -1)
	{
	// Aligned Block
	int fragSize = rm->blockSizes[idx];
	fragGStart += rm->chromStart;
	int fragGEnd = fragGStart + fragSize;
	lx1 = roundingScale(fragGStart - winStart, width, baseWidth) + xOff;
	lx1 = max(lx1, 0);
	lx2 = roundingScale(fragGEnd - winStart, width, baseWidth) + xOff;
	w = lx2 - lx1;
	if (w <= 0)
	    w = 1;

	if (idx == 1 && rm->blockCount == 3)
	    {
	    // One and only one aligned block
	    drawBoxWChevrons(hvg, lx1, y + alignedBlockOffset, w,
			      alignedBlockHeight, fillColor,
			      outlineColor, rm->strand[0]);
	    }
	else if (idx == 1)
	    {
	    // First block
	    if (rm->strand[0] == '-')
		{
		// First block on negative strand is the point block
		drawBoxWChevrons(hvg, lx1,
				  y + alignedBlockOffset, w,
				  alignedBlockHeight, fillColor,
				  outlineColor, rm->strand[0]);
		}
	    else
		{
		// First block on the positive strand is the tail block
		drawBoxWChevrons(hvg, lx1,
				  y + alignedBlockOffset, w,
				  alignedBlockHeight, fillColor,
				  outlineColor, rm->strand[0]);

		}
	    }
	else if (idx == (rm->blockCount - 2))
	    {
	    // Last block
	    if (rm->strand[0] == '-')
		{
		// Last block on the negative strand is the tail block
		drawBoxWChevrons(hvg, lx1,
				  y + alignedBlockOffset, w,
				  alignedBlockHeight, fillColor,
				  outlineColor, rm->strand[0]);

		}
	    else
		{
		// Last block on the positive strand is the poitn block
		drawBoxWChevrons(hvg, lx1,
				  y + alignedBlockOffset, w,
				  alignedBlockHeight, fillColor,
				  outlineColor, rm->strand[0]);

		}
	    }
	else
	    {
	    // Intermediate aligned blocks are drawn as rectangles
	    drawBoxWChevrons(hvg, lx1, y + alignedBlockOffset, w,
			      alignedBlockHeight, fillColor,
			      outlineColor, rm->strand[0]);
	    }

	}
    else
	{
	// Unaligned Block
	int relStart = 0;
	if (idx == 0)
	    {
	    /*
	     * Unaligned sequence at the start of an annotation
	     * Draw as:
	     *     |-------------     or |------//------
	     *                  |                      |
	     *                  >>>>>>>                >>>>>>>>
	     */
	    lx2 = roundingScale(rm->chromStart +
				 rm->blockRelStarts[idx + 1] -
				 winStart, width, baseWidth) + xOff;
	    if ((rm->blockSizes[idx] * pixelsPerBase) >
		MAX_UNALIGNED_PIXEL_LEN)
		{
		lx1 = roundingScale(rm->chromStart +
				     (rm->blockRelStarts[idx + 1] -
				      (int)
				      (MAX_UNALIGNED_PIXEL_LEN /
				       pixelsPerBase)) -
				     winStart, width, baseWidth) + xOff;
		// Line Across
		drawDashedHorizLineWHash(hvg, lx1, lx2,
					  y +
					  unalignedBlockOffset,
					  5, 5, black, rm->blockSizes[idx]);
		}
	    else
		{
		lx1 = roundingScale(rm->chromStart - winStart,
				     width, baseWidth) + xOff;
		// Line Across
		drawDashedHorizLine(hvg, lx1, lx2,
				     y + unalignedBlockOffset, 5, 5, black);
		}
	    // Line down
	    hvGfxLine(hvg, lx2, y + alignedBlockOffset, lx2,
		       y + unalignedBlockOffset, black);
	    hvGfxLine(hvg, lx1, y + unalignedBlockOffset - 3, lx1,
		       y + unalignedBlockOffset + 3, black);

	    // Draw labels
	    //Disabled
	    //if ( vis != tvSquish && vis != tvFull )
            //     {
	         MgFont *font = tl.font;
	         int fontHeight = tl.fontHeight;
	         int stringWidth =
	     	     mgFontStringWidth(font, rm->name) + LABEL_PADDING;
	         hvGfxTextCentered(hvg, lx1 - stringWidth,
			       heightPer - fontHeight + y,
			       stringWidth, fontHeight, MG_BLACK, font,
			       rm->name);
             //    }




	    }
	else if (idx == (rm->blockCount - 1))
	    {
	    /*
	     * Unaligned sequence at the end of an annotation
	     * Draw as:
	     *       -------------|   or        ------//------|
	     *       |                          |
	     *  >>>>>                    >>>>>>>
	     */
	    lx1 = roundingScale(rm->chromStart +
				 rm->blockRelStarts[idx - 1] +
				 rm->blockSizes[idx - 1] - winStart,
				 width, baseWidth) + xOff;
	    if ((rm->blockSizes[idx] * pixelsPerBase) >
		MAX_UNALIGNED_PIXEL_LEN)
		{
		lx2 = roundingScale(rm->chromStart +
				     rm->blockRelStarts[idx -
							1] +
				     rm->blockSizes[idx - 1] +
				     (int)
				     (MAX_UNALIGNED_PIXEL_LEN /
				      pixelsPerBase) - winStart,
				     width, baseWidth) + xOff;
		// Line Across
		drawDashedHorizLineWHash(hvg, lx1, lx2,
					  y +
					  unalignedBlockOffset,
					  5, 5, black, rm->blockSizes[idx]);
		}
	    else
		{
		lx2 = roundingScale(rm->chromStart +
				     rm->blockRelStarts[idx -
							1] +
				     rm->blockSizes[idx - 1] +
				     rm->blockSizes[idx] -
				     winStart, width, baseWidth) + xOff;
		// Line Across
		drawDashedHorizLine(hvg, lx1, lx2,
				     y + unalignedBlockOffset, 5, 5, black);
		}
	    // Line down
	    hvGfxLine(hvg, lx1, y + alignedBlockOffset, lx1,
		       y + unalignedBlockOffset, black);
	    hvGfxLine(hvg, lx2, y + unalignedBlockOffset - 3, lx2,
		       y + unalignedBlockOffset + 3, black);
	    }
	else
	    {
	    /*
	     * Middle Unaligned
	     * Draw unaligned sequence between aligned fragments
	     * as:       .........
	     *          /         \
	     *     >>>>>           >>>>>>
	     *
	     * or:
	     *         .............
	     *         \           /
	     *     >>>>>           >>>>>>
	     *
	     * Also use ....//.... to indicate not to scale if
	     * necessary.
	     *
	     */
	    int alignedGapSize = rm->blockRelStarts[idx + 1] -
		(rm->blockRelStarts[idx - 1] + rm->blockSizes[idx - 1]);
	    int unaSize = rm->blockSizes[idx];

	    int alignedOverlapSize = unaSize - alignedGapSize;

	    if (unaSize < 0)
		{
		relStart =
		    rm->blockRelStarts[idx - 1] +
		    rm->blockSizes[idx - 1] - (alignedOverlapSize / 2);

		if (abs(unaSize) > rm->blockSizes[idx - 1])
		    unaSize = -rm->blockSizes[idx - 1];

		lx1 = roundingScale(rm->chromStart +
				     rm->blockRelStarts[idx -
							1] +
				     rm->blockSizes[idx - 1] +
				     unaSize - winStart, width,
				     baseWidth) + xOff;
		lx1 = max(lx1, 0);
		lx2 = roundingScale(rm->chromStart +
				     rm->blockRelStarts[idx +
							1] -
				     winStart, width, baseWidth) + xOff;

		int midPoint = ((lx2 - lx1) / 2) + lx1;

		// Block Intersection Line
		hvGfxLine(hvg, lx1, y + alignedBlockOffset, lx1,
			   y + alignedBlockOffset + alignedBlockHeight,
			   black);

		// Line Up
		hvGfxLine(hvg, lx1, y + alignedBlockOffset,
			   midPoint, y + unalignedBlockOffset, black);

		// Line Down
		hvGfxLine(hvg, lx2, y + alignedBlockOffset,
			   midPoint, y + unalignedBlockOffset, black);

		}
	    else if (alignedOverlapSize > 0 &&
		     ((alignedOverlapSize * 0.5) >
		      (0.3 * rm->blockSizes[idx - 1])
		      || (alignedOverlapSize * 0.5) >
		      (0.3 * rm->blockSizes[idx + 1])))
		{
		// Need to shorten unaligned length
		int smallOverlapLen = 0;
		smallOverlapLen = (0.3 * rm->blockSizes[idx - 1]);
		if (smallOverlapLen > (0.3 * rm->blockSizes[idx + 1]))
		    smallOverlapLen = (0.3 * rm->blockSizes[idx + 1]);
		unaSize = (smallOverlapLen * 2) + alignedGapSize;
		relStart =
		    rm->blockRelStarts[idx - 1] +
		    rm->blockSizes[idx - 1] - smallOverlapLen;
		lx1 = roundingScale(relStart + rm->chromStart -
				     winStart, width, baseWidth) + xOff;
		lx1 = max(lx1, 0);
		lx2 = roundingScale(relStart + rm->chromStart +
				     unaSize - winStart, width,
				     baseWidth) + xOff;
		// Line Up
		cx1 = roundingScale(rm->chromStart +
				     rm->blockRelStarts[idx -
							1] +
				     rm->blockSizes[idx - 1] -
				     winStart, width, baseWidth) + xOff;
		hvGfxLine(hvg, cx1, y + alignedBlockOffset, lx1,
			   y + unalignedBlockOffset, black);

		// Line Across
		drawDashedHorizLineWHash(hvg, lx1, lx2,
					  y +
					  unalignedBlockOffset,
					  5, 5, black, rm->blockSizes[idx]);
		// Line Down
		cx2 = roundingScale(rm->chromStart +
				     rm->blockRelStarts[idx +
							1] -
				     winStart, width, baseWidth) + xOff;
		hvGfxLine(hvg, cx2, y + alignedBlockOffset, lx2,
			   y + unalignedBlockOffset, black);
		}
	    else
		{
		// Adequate to display full length
		relStart =
		    rm->blockRelStarts[idx - 1] +
		    rm->blockSizes[idx - 1] - (alignedOverlapSize / 2);
		lx1 = roundingScale(relStart + rm->chromStart -
				     winStart, width, baseWidth) + xOff;
		lx1 = max(lx1, 0);
		lx2 = roundingScale(relStart + rm->chromStart +
				     unaSize - winStart, width,
				     baseWidth) + xOff;
		// Line Up
		int cx1 =
		    roundingScale(rm->chromStart +
				   rm->blockRelStarts[idx - 1] +
				   rm->blockSizes[idx - 1] - winStart,
				   width, baseWidth) + xOff;
		hvGfxLine(hvg, cx1, y + alignedBlockOffset, lx1,
			   y + unalignedBlockOffset, black);
		drawDashedHorizLine(hvg, lx1, lx2,
				     y + unalignedBlockOffset, 5, 5, black);
		// Line Down
		cx2 = roundingScale(rm->chromStart +
				     rm->blockRelStarts[idx +
							1] -
				     winStart, width, baseWidth) + xOff;
		hvGfxLine(hvg, cx2, y + alignedBlockOffset, lx2,
			   y + unalignedBlockOffset, black);
		}
	    }
	}
    }
}

static void origRepeatDraw(struct track *tg, int seqStart, int seqEnd,
		struct hvGfx *hvg, int xOff, int yOff, int width,
		MgFont * font, Color color, enum trackVisibility vis)
/* This is the original repeat drawing routine, modified
 * to handle the new rmskJoined table structure.
 */
{
setWg();
int baseWidth = seqEnd - seqStart;
struct repeatItem *ri;
int y = yOff;
int heightPer = tg->heightPer;
int lineHeight = tg->lineHeight;
int x1, x2, w;
Color col;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row;
int rowOffset;

//Disabled
//if (vis == tvFull || vis == tvSquish || vis == tvPack)
if (vis == tvFull)
    {
    /*
     * Do grayscale representation spread out among tracks.
     */
    struct hash *hash = newHash(6);
    struct rmskJoined *ro;
    int percId;
    int grayLevel;

    for (ri = tg->items; ri != NULL; ri = ri->next)
	{
	ri->yOffset = y;
	y += lineHeight;
	hashAdd(hash, ri->class, ri);
	}
    sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL,
		      &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	ro = rmskJoinedLoad(row + rowOffset);
	char class[256];
	// Simplify repClass for lookup: strip trailing '?',
	// simplify *RNA to RNA:
	char *poundPtr = index(ro->name, '#');
	if (poundPtr)
	    {
	    safecpy(class, sizeof(class), poundPtr + 1);
	    char *slashPtr = index(class, '/');
	    if (slashPtr)
	        *slashPtr = '\0';
	    }
	else
	    {
	    safecpy(class, sizeof(class), "Unknown");
	    }
	char *p = &(class[strlen(class) - 1]);
	if (*p == '?')
	    *p = '\0';
	if (endsWith(class, "RNA"))
	    safecpy(class, sizeof(class), "RNA");
	ri = hashFindVal(hash, class);
	if (ri == NULL)
	    ri = otherRepeatItem;
	percId = 10000 - ro->score;
	grayLevel = grayInRange(percId, 6000, 10000);
	col = shadesOfGray[grayLevel];

	int idx = 0;
	for (idx = 0; idx < ro->blockCount; idx++)
	    {
	    if (ro->blockRelStarts[idx] > 0)
		{
		int blockStart = ro->chromStart + ro->blockRelStarts[idx];
		int blockEnd =
		    ro->chromStart + ro->blockRelStarts[idx] +
		    ro->blockSizes[idx];

		x1 = roundingScale(blockStart - winStart, width,
				    baseWidth) + xOff;
		x1 = max(x1, 0);
		x2 = roundingScale(blockEnd - winStart, width,
				    baseWidth) + xOff;
		w = x2 - x1;
		if (w <= 0)
		    w = 1;
		hvGfxBox(hvg, x1, ri->yOffset, w, heightPer, col);
		}
	    }
	rmskJoinedFree(&ro);
	}
    freeHash(&hash);
    }
else
    {
    char table[64];
    boolean hasBin;
    struct dyString *query = newDyString(1024);
    /*
     * Do black and white on single track.  Fetch less than
     * we need from database.
     */
    if (hFindSplitTable(database, chromName, tg->table, table, &hasBin))
	{
	sqlDyStringPrintf(query,
			"select chromStart,blockCount,blockSizes,"
			"blockRelStarts from %s where ", table);
	if (hasBin)
	    hAddBinToQuery(winStart, winEnd, query);
	sqlDyStringPrintf(query, "chromStart<%u and chromEnd>%u ",
			winEnd, winStart);
	/*
	 * if we're using a single rmsk table, add chrom to the where clause
	 */
	if (startsWith("rmskJoined", table))
	    sqlDyStringPrintf(query, " and chrom = '%s' ", chromName);
	sr = sqlGetResult(conn, query->string);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    int idx = 0;
	    int blockCount = sqlSigned(row[1]);
	    int sizeOne;
	    int *blockSizes;
	    int *blockRelStarts;
	    int chromStart = sqlUnsigned(row[0]);
	    sqlSignedDynamicArray(row[2], &blockSizes, &sizeOne);
	    assert(sizeOne == blockCount);
	    sqlSignedDynamicArray(row[3], &blockRelStarts, &sizeOne);
	    assert(sizeOne == blockCount);

	    for (idx = 1; idx < blockCount - 1; idx++)
		{
		if (blockRelStarts[idx] >= 0)
		    {
		    int blockStart = chromStart + blockRelStarts[idx];
		    int blockEnd =
			chromStart + blockRelStarts[idx] + blockSizes[idx];

		    x1 = roundingScale(blockStart - winStart,
					width, baseWidth) + xOff;
		    x1 = max(x1, 0);
		    x2 = roundingScale(blockEnd - winStart, width,
					baseWidth) + xOff;
		    w = x2 - x1;
		    if (w <= 0)
		        w = 1;
		    hvGfxBox(hvg, x1, yOff, w, heightPer, MG_BLACK);
		    }
		}
	    }
	}
    dyStringFree(&query);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

/* Main callback for displaying this track in the viewport
 * of the browser.
 */
static void rmskJoinedDrawItems(struct track *tg, int seqStart, int seqEnd,
	     struct hvGfx *hvg, int xOff, int yOff, int width,
	     MgFont * font, Color color, enum trackVisibility vis)
{
setWg();
int baseWidth = seqEnd - seqStart;

// TODO: Document
pixelsPerBase = (float) width / (float) baseWidth;
  /*
   * Its unclear to me why heightPer is not updated to the
   * value set in rmskJoinedItemHeight() at the time this callback
   * is invoked.  Getting the correct value myself.
   * was:
   * int heightPer = tg->heightPer;
   */
int heightPer = rmskJoinedItemHeight(tg, NULL);
//boolean isFull = (vis == tvFull);
struct rmskJoined *rm;

  // If we are in full view mode and the scale is sufficient,
  // display the new visualization.
//Disabled
//if ((vis == tvFull || vis == tvSquish || vis == tvPack) && baseWidth <= DETAIL_VIEW_MAX_SCALE)
if (vis == tvFull && baseWidth <= DETAIL_VIEW_MAX_SCALE)
    {
    int level = yOff;
    struct subTrack *st = hashFindVal(subTracksHash, tg->table);
    if (!st)
	return;

    int lidx = st->levelCount;
    int currLevel = 0;
    for (currLevel = 0; currLevel < lidx; currLevel++)
	{
	rm = st->levels[currLevel];
	while (rm)
	    {
	    drawRMGlyph(hvg, level, heightPer, width, baseWidth, xOff, rm, vis );

	    char statusLine[128];
	    int ss1 = roundingScale(rm->alignStart - winStart,
				     width, baseWidth) + xOff;

	    safef(statusLine, sizeof(statusLine), "%s", rm->name);

	    int x1 = roundingScale(rm->alignStart - winStart, width,
				    baseWidth) + xOff;
	    x1 = max(x1, 0);
	    int x2 = roundingScale(rm->alignEnd - winStart, width,
				    baseWidth) + xOff;
	    int w = x2 - x1;
	    if (w <= 0)
	        w = 1;

	    mapBoxHc(hvg, rm->alignStart, rm->alignEnd, ss1, level,
   	             w, heightPer, tg->track, rm->id, statusLine);
	    rm = rm->next;
	    }
	level += heightPer;
	rmskJoinedFreeList(&(st->levels[currLevel]));
        }

    }
else
    {
    // Draw the stereotypical view
    origRepeatDraw(tg, seqStart, seqEnd,
		    hvg, xOff, yOff, width, font, color, vis);
    }
}

void rmskJoinedMethods(struct track *tg)
{
tg->loadItems = rmskJoinedLoadItems;
tg->freeItems = rmskJoinedFreeItems;
tg->drawItems = rmskJoinedDrawItems;
tg->colorShades = shadesOfGray;
tg->itemName = rmskJoinedName;
tg->mapItemName = rmskJoinedName;
tg->totalHeight = rmskJoinedTotalHeight;
tg->itemHeight = rmskJoinedItemHeight;
tg->itemStart = tgItemNoStart;
tg->itemEnd = tgItemNoEnd;
tg->mapsSelf = TRUE;
//tg->canPack = TRUE;
}
