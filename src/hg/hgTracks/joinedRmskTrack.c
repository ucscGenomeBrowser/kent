/* joinedRmskTrack - A comprehensive RepeatMasker visualization track 
 *                   handler. This is an extension of the original
 *                   rmskTrack.c written by UCSC.  
 *
 *  Written by Robert Hubley 10/2012
 */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "rmskJoined.h"

/* winBaseCount size ( or below ) at which the detailed view
 * of repeats is turned on.
 */
#define REPEAT_DETAIL_VIEW 100000

/* The maximum size of unaligned sequence to draw before 
 * switching to the unscaled view: ie. "----//---"
 */
#define MAX_UNALIGNED_LEN 200

/* classHash is the hash of the repeatItems ( tg->items ) for this track.  
 *   This is used to hold display names for the lines of
 *   the track, the line heights, and class colors.
 *
 * the subTracksHash is a hash of subtracks
 *   The joinedRmskTrack is designed to be used as a composite track.
 *   When jRepeatLoad is called this hash is populated with the 
 *   results of one or more table queries 
 */
struct itemSpecifics
/* extra information to go with each track */
    {
    struct hash *classHash;
    struct repeatItem *otherRepeatItem;
    struct hash *subTracksHash;
    };

struct subTrack
    {
    /* The number of display levels used in levels[] */
    int levelCount;
    /* The rmskJoined records from table query */
    struct rmskJoined *levels[30];
    };   

// Display names
static char *rptClassNames[] = {
  "SINE", "LINE", "LTR", "DNA", "Simple", "Low Complexity", "Satellite",
  "RNA", "Other", "Unknown",
};

// Data names
static char *rptClasses[] = {
  "SINE", "LINE", "LTR", "DNA", "Simple_repeat", "Low_complexity",
  "Satellite", "RNA", "Other", "Unknown",
};

/* Need better class to color mappings.  I took these from a 
 * online color dictionary website: 
 *
 *     http://people.csail.mit.edu/jaffer/Color/Dictionaries
 *  
 *  I used the html4 catalog and tried to the 10 most distinct
 *  colors.  
 *  
 *  NOTE: If these are changed, do not forget to update the 
 *        help table in joinedRmskTrack.html
 */
static Color jRepeatClassColors[] = {
  0xff0000ff,			// SINE - red
  0xff00ff00,			// LINE - lime
  0xff000080,			// LTR - maroon
  0xffff00ff,			// DNA - fuchsia
  0xff00ffff,			// Simple - yellow
  0xff008080,			// LowComplex - olive
  0xffff0000,			// Satellite - blue
  0xff008000,			// RNA - green
  0xff808000,			// Other - teal
  0xffffff00,			// Unknown - aqua
};

/* Sort repeats by display start position.  Note: We
 * account for the fact we may not start the visual
 * display at chromStart.  See MAX_UNALIGNED_LEN.
 */
static int
cmpRepeatVisStart(const void *va, const void *vb)
{
  const struct rmskJoined *a = *((struct rmskJoined **) va);
  const struct rmskJoined *b = *((struct rmskJoined **) vb);
  int aStart = a->chromStart;
  if (a->blockSizes[0] > MAX_UNALIGNED_LEN)
    aStart = a->chromStart + (a->blockSizes[0] - MAX_UNALIGNED_LEN);
  int bStart = b->chromStart;
  if (b->blockSizes[0] > MAX_UNALIGNED_LEN)
    bStart = b->chromStart + (b->blockSizes[0] - MAX_UNALIGNED_LEN);
  return (aStart - bStart);
}

/* Initialize the track */
static void makeJRepeatItems(struct track *tg)
  /* Initialize the subtracks hash - This will eventually contain
   * all the repeat data for each displayed subtrack 
   */
{
  struct itemSpecifics *extraData;
  AllocVar(extraData);

  extraData->classHash = newHash(6);
  extraData->otherRepeatItem = NULL;
  extraData->subTracksHash = newHash(10);
  tg->extraUiData = extraData;

  struct repeatItem *ri, *riList = NULL;
  int i;
  int numClasses = ArraySize(rptClasses);
  for (i = 0; i < numClasses; ++i)
  {
    AllocVar(ri);
    ri->class = rptClasses[i];
    ri->className = rptClassNames[i];
    // New color attribute
    ri->color = jRepeatClassColors[i];
    slAddHead(&riList, ri);
    // Hash now prebuilt to hold color attributes
    hashAdd(extraData->classHash, ri->class, ri);
    if (sameString(rptClassNames[i], "Other"))
      extraData->otherRepeatItem = ri;
  }
  slReverse(&riList);
  tg->items = riList;
}

static void jRepeatLoad(struct track *tg)
/* We do the query(ies) here so we can report how deep the track(s)
 * will be when jRepeatTotalHeight() is called later on 
 */ 
{
  makeJRepeatItems(tg);
  struct itemSpecifics *extraData = tg->extraUiData;

  int baseWidth = winEnd - winStart;
  if ( tg->visibility == tvFull && baseWidth <= REPEAT_DETAIL_VIEW)
  {
    struct subTrack *st = NULL;
    AllocVar(st);
    if ( st )
    { 
      st->levels[0] = NULL;
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
        if (detailList)
          slAddHead(detailList, rm);
        else
          detailList = rm;
      }
      slSort(&detailList, cmpRepeatVisStart);

      sqlFreeResult(&sr);
      hFreeConn(&conn);

      int crChromStart, crChromEnd;
      while (detailList)
      {
        st->levels[st->levelCount++] = detailList;
        struct rmskJoined *cr = detailList;
        detailList = detailList->next;
        cr->next = NULL;
        struct rmskJoined *prev = NULL;
        rm = detailList;
        int rmChromStart, rmChromEnd;
        crChromStart = cr->chromStart;
        if (cr->blockSizes[0] > MAX_UNALIGNED_LEN)
          crChromStart =
            cr->chromStart + (cr->blockSizes[0] - MAX_UNALIGNED_LEN);
        crChromEnd = cr->chromEnd;
        if (cr->blockSizes[cr->blockCount - 1] > MAX_UNALIGNED_LEN)
          crChromEnd -=
            (cr->blockSizes[cr->blockCount - 1] - MAX_UNALIGNED_LEN);
        while (rm)
        {
          rmChromStart = rm->chromStart;
          if (rm->blockSizes[0] > MAX_UNALIGNED_LEN)
            rmChromStart =
              rm->chromStart + (rm->blockSizes[0] - MAX_UNALIGNED_LEN);
          rmChromEnd = rm->chromEnd;
          if (rm->blockSizes[rm->blockCount - 1] > MAX_UNALIGNED_LEN)
            rmChromEnd -=
              (rm->blockSizes[rm->blockCount - 1] - MAX_UNALIGNED_LEN);
  
          if (rmChromStart > crChromEnd)
          {
            cr->next = rm;
            cr = rm;
            if (prev)
            {
              prev->next = rm->next;
            }
            else
            {
              detailList = rm->next;
            }
            rm = rm->next;
            cr->next = NULL;
            crChromStart = cr->chromStart;
            if (cr->blockSizes[0] > MAX_UNALIGNED_LEN)
              crChromStart =
                cr->chromStart + (cr->blockSizes[0] - MAX_UNALIGNED_LEN);
            crChromEnd = cr->chromEnd;
            if (cr->blockSizes[cr->blockCount - 1] > MAX_UNALIGNED_LEN)
              crChromEnd -=
                (cr->blockSizes[cr->blockCount - 1] - MAX_UNALIGNED_LEN);
          }
          else
          {
            prev = rm;
            rm = rm->next;
          }
        } // while ( rm )
      } // while ( detailList )
      // Create Hash Entry
      hashReplace(extraData->subTracksHash, tg->table, st);
    } // if ( st )
  } // if ( tg->visibility == tvFull
}

static void
jRepeatFree(struct track *tg)
/* Free up jRepeatMasker items. */
{
  slFreeList(&tg->items);
}

static char *
jRepeatName(struct track *tg, void *item)
/* Return name of jRepeat item track. */
{
  static char empty = '\0';
  struct repeatItem *ri = item;
  /*
   * In detail view mode the items represent different packing 
   * levels.  No need to display a label at each level.  Instead
   * Just return a label for the first level.
   */
  if (tg->limitedVis == tvFull && winBaseCount <= REPEAT_DETAIL_VIEW)
  {
    if (strcmp(ri->className, "SINE") == 0)
    {
      return ("Repeats");
    }
    else
    {
      return &empty;
    }
  }
  return ri->className;
}

int
jRepeatTotalHeight(struct track *tg, enum trackVisibility vis)
{
  struct itemSpecifics *extraData = tg->extraUiData;
  // Are we in full view mode and at the scale needed to display
  // the detail view?
  if (tg->limitedVis == tvFull && winBaseCount <= REPEAT_DETAIL_VIEW)
     {
        // Lookup the depth of this subTrack and report it
        struct subTrack *st = hashFindVal(extraData->subTracksHash, tg->table );
        if ( st )
          return ( (st->levelCount+1) * 24 );
        else
          // Just display one line
          return ( 24 );
     }
  else
    return tgFixedTotalHeightNoOverflow(tg, vis);
}

int
jRepeatItemHeight(struct track *tg, void *item)
{
  // Are we in full view mode and at the scale needed to display
  // the detail view?
  if (tg->limitedVis == tvFull && winBaseCount <= REPEAT_DETAIL_VIEW)
    return 24;
  else
    return tgFixedItemHeight(tg, item);
}

static void
drawDashedHorizLine(struct hvGfx *hvg, int x1, int x2,
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

static void
drawShortDashedHorizLine(struct hvGfx *hvg, int x1, int x2,
			 int y, int dashLen, int gapLen, Color lineColor)
// ie.    - - - - - - -\\- - - - - - - - - 
{
  int cx1 = x1;
  int cx2;

  int midX = ((x2 - x1) / 2) + x1;
  int midPointDrawn = 0;

  while (1)
  {
    cx2 = cx1 + dashLen;
    if (cx2 > x2)
      cx2 = x2;

    if (!midPointDrawn && cx2 > midX)
    {
      // Draw double slashes "\\" instead of dash
      hvGfxLine(hvg, cx1, y - 3, cx1 + 3, y + 3, lineColor);
      hvGfxLine(hvg, cx1 + 3, y - 3, cx1 + 6, y + 3, lineColor);
      midPointDrawn = 1;
    }
    else
    {
      // Draw a dash
      hvGfxLine(hvg, cx1, y, cx2, y, lineColor);
    }
    cx1 += dashLen;

    if (!midPointDrawn && cx1 + gapLen > midX)
    {
      // Draw double slashes "\\" instead of gap
      hvGfxLine(hvg, cx1, y - 3, cx1 + 3, y + 3, lineColor);
      hvGfxLine(hvg, cx1 + 3, y - 3, cx1 + 6, y + 3, lineColor);
      midPointDrawn = 1;
    }
    cx1 += gapLen;

    if (cx1 > x2)
      break;
  }
}

static void
drawNormalBox(struct hvGfx *hvg, int x1, int y1,
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

static void
drawTailBox(struct hvGfx *hvg, int x1, int y1,
	    int width, int height,
	    Color fillColor, Color outlineColor, char strand)
{
  struct gfxPoly *poly = gfxPolyNew();
  int y2 = y1 + height;
  int x2 = x1 + width;
  int half = (y2 - y1) / 2;
  if (strand == '-')
  {

    /*
     *      1,6-------------2
     *        |            / 
     *        |           3   
     *        |            \   
     *        5-------------4
     */
    gfxPolyAddPoint(poly, x1, y1);
    gfxPolyAddPoint(poly, x2, y1);
    gfxPolyAddPoint(poly, x2 - half, y1 + half);
    gfxPolyAddPoint(poly, x2, y2);
    gfxPolyAddPoint(poly, x1, y2);
    gfxPolyAddPoint(poly, x1, y1);
  }
  else
  {
    /*
     *     1,6--------------2
     *       \              |
     *        5             | 
     *       /              | 
     *      4---------------3
     */
    gfxPolyAddPoint(poly, x1, y1);
    gfxPolyAddPoint(poly, x2, y1);
    gfxPolyAddPoint(poly, x2, y2);
    gfxPolyAddPoint(poly, x1, y2);
    gfxPolyAddPoint(poly, x1 + half, y1 + half);
    gfxPolyAddPoint(poly, x1, y1);
  }
  hvGfxDrawPoly(hvg, poly, fillColor, TRUE);
  hvGfxDrawPoly(hvg, poly, outlineColor, FALSE);
  gfxPolyFree(&poly);
}

static void
drawPointBox(struct hvGfx *hvg, int x1, int y1,
	     int width, int height,
	     Color fillColor, Color outlineColor, char strand)
{
  struct gfxPoly *poly = gfxPolyNew();
  int y2 = y1 + height;
  int x2 = x1 + width;
  int half = (y2 - y1) / 2;
  if (strand == '-')
  {

    /*
     *      1,6-------------2
     *       /              | 
     *      5               |   
     *       \              |   
     *        4-------------3
     */
    gfxPolyAddPoint(poly, x1 + half, y1);
    gfxPolyAddPoint(poly, x2, y1);
    gfxPolyAddPoint(poly, x2, y2);
    gfxPolyAddPoint(poly, x1 + half, y2);
    gfxPolyAddPoint(poly, x1, y1 + half);
    gfxPolyAddPoint(poly, x1 + half, y1);
  }
  else
  {
    /*
     *     1,6--------------2
     *       |               \
     *       |                3
     *       |               /
     *       5--------------4
     */
    gfxPolyAddPoint(poly, x1, y1);
    gfxPolyAddPoint(poly, x2 - half, y1);
    gfxPolyAddPoint(poly, x2, y1 + half);
    gfxPolyAddPoint(poly, x2 - half, y2);
    gfxPolyAddPoint(poly, x1, y2);
    gfxPolyAddPoint(poly, x1, y1);
  }
  hvGfxDrawPoly(hvg, poly, fillColor, TRUE);
  hvGfxDrawPoly(hvg, poly, outlineColor, FALSE);
  gfxPolyFree(&poly);
}

static void
drawDirBox(struct hvGfx *hvg, int x1, int y1, int width, int height,
	   Color fillColor, Color outlineColor, char strand)
{
  struct gfxPoly *poly = gfxPolyNew();
  int y2 = y1 + height;
  int x2 = x1 + width;
  int half = (y2 - y1) / 2;
  if (strand == '-')
  {

    /*
     *      1,7-------------2
     *       /             / 
     *      6             3   
     *       \             \   
     *        5-------------4
     */
    gfxPolyAddPoint(poly, x1 + half, y1);
    gfxPolyAddPoint(poly, x2, y1);
    gfxPolyAddPoint(poly, x2 - half, y1 + half);
    gfxPolyAddPoint(poly, x2, y2);
    gfxPolyAddPoint(poly, x1 + half, y2);
    gfxPolyAddPoint(poly, x1, y1 + half);
    gfxPolyAddPoint(poly, x1 + half, y1);
  }
  else
  {
    /*
     *     1,7--------------2
     *       \               \
     *        6               3
     *       /               /
     *      5---------------4
     */
    gfxPolyAddPoint(poly, x1, y1);
    gfxPolyAddPoint(poly, x2 - half, y1);
    gfxPolyAddPoint(poly, x2, y1 + half);
    gfxPolyAddPoint(poly, x2 - half, y2);
    gfxPolyAddPoint(poly, x1, y2);
    gfxPolyAddPoint(poly, x1 + half, y1 + half);
    gfxPolyAddPoint(poly, x1, y1);
  }
  hvGfxDrawPoly(hvg, poly, fillColor, TRUE);
  hvGfxDrawPoly(hvg, poly, outlineColor, FALSE);
  gfxPolyFree(&poly);
}

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
static void
drawRMGlyph(struct hvGfx *hvg, int y, int heightPer,
   int width, int baseWidth, int xOff, struct rmskJoined *rm,
      struct itemSpecifics *extraData)
{
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
  Color outlineColor = jRepeatClassColors[0];

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
  ri = hashFindVal(extraData->classHash, class);
  if (ri == NULL)
    ri = extraData->otherRepeatItem;

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
	drawDirBox(hvg, lx1, y + alignedBlockOffset, w,
		   alignedBlockHeight, fillColor, outlineColor,
		   rm->strand[0]);
      }
      else if (idx == 1)
      {
	// First block
	if (rm->strand[0] == '-')
	{
	  // First block on negative strand is the point block
	  drawPointBox(hvg, lx1, y + alignedBlockOffset, w,
		       alignedBlockHeight, fillColor, outlineColor,
		       rm->strand[0]);
	}
	else
	{
	  // First block on the positive strand is the tail block
	  drawTailBox(hvg, lx1, y + alignedBlockOffset, w,
		      alignedBlockHeight, fillColor, outlineColor,
		      rm->strand[0]);

	}
      }
      else if (idx == (rm->blockCount - 2))
      {
	// Last block
	if (rm->strand[0] == '-')
	{
	  // Last block on the negative strand is the tail block
	  drawTailBox(hvg, lx1, y + alignedBlockOffset, w,
		      alignedBlockHeight, fillColor, outlineColor,
		      rm->strand[0]);

	}
	else
	{
	  // Last block on the positive strand is the poitn block
	  drawPointBox(hvg, lx1, y + alignedBlockOffset, w,
		       alignedBlockHeight, fillColor, outlineColor,
		       rm->strand[0]);

	}
      }
      else
      {
	// Intermediate aligned blocks are drawn as rectangles
	drawNormalBox(hvg, lx1, y + alignedBlockOffset, w,
		      alignedBlockHeight, fillColor, outlineColor);
      }

    }
    else
    {
      // Unaligned Block 
      int relStart = 0;
      if (idx == 0)
      {
	/*
	   Unaligned sequence at the start of an annotation
	   * Draw as:
	   *     |-------------     or |------//------
	   *                  |                      |
	   *                  >>>>>>>                >>>>>>>>
	 */
	lx2 = roundingScale(rm->chromStart + rm->blockRelStarts[idx + 1] -
			    winStart, width, baseWidth) + xOff;
	if (rm->blockSizes[idx] > MAX_UNALIGNED_LEN)
	{
	  lx1 = roundingScale(rm->chromStart +
			      (rm->blockRelStarts[idx + 1] -
			       MAX_UNALIGNED_LEN) - winStart, width,
			      baseWidth) + xOff;
	  // Line Across
	  drawShortDashedHorizLine(hvg, lx1, lx2,
				   y + unalignedBlockOffset, 5, 5, black);
	}
	else
	{
	  lx1 = roundingScale(rm->chromStart - winStart, width,
			      baseWidth) + xOff;
	  // Line Across
	  drawDashedHorizLine(hvg, lx1, lx2,
			      y + unalignedBlockOffset, 5, 5, black);
	}
	// Line down
	hvGfxLine(hvg, lx2, y + alignedBlockOffset, lx2,
		  y + unalignedBlockOffset, black);
	hvGfxLine(hvg, lx1, y + unalignedBlockOffset - 3, lx1,
		  y + unalignedBlockOffset + 3, black);

      }
      else if (idx == (rm->blockCount - 1))
      {
	/*
	   Unaligned sequence at the end of an annotation
	   * Draw as:
	   *       -------------|   or        ------//------|
	   *       |                          |
	   *  >>>>>                    >>>>>>>     
	 */
	lx1 = roundingScale(rm->chromStart + rm->blockRelStarts[idx - 1] +
			    rm->blockSizes[idx - 1] - winStart, width,
			    baseWidth) + xOff;
	if (rm->blockSizes[idx] > MAX_UNALIGNED_LEN)
	{
	  lx2 =
	    roundingScale(rm->chromStart + rm->blockRelStarts[idx - 1] +
			  rm->blockSizes[idx - 1] +
			  MAX_UNALIGNED_LEN - winStart, width,
			  baseWidth) + xOff;
	  // Line Across
	  drawShortDashedHorizLine(hvg, lx1, lx2,
				   y + unalignedBlockOffset, 5, 5, black);
	}
	else
	{
	  lx2 =
	    roundingScale(rm->chromStart + rm->blockRelStarts[idx - 1] +
			  rm->blockSizes[idx - 1] +
			  rm->blockSizes[idx] - winStart, width,
			  baseWidth) + xOff;
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
	   Middle Unaligned
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
	    rm->blockRelStarts[idx - 1] + rm->blockSizes[idx - 1] -
	    (alignedOverlapSize / 2);

	  if (abs(unaSize) > rm->blockSizes[idx - 1])
	    unaSize = -rm->blockSizes[idx - 1];

	  lx1 =
	    roundingScale(rm->chromStart +
			  rm->blockRelStarts[idx - 1] +
			  rm->blockSizes[idx - 1] +
			  unaSize - winStart, width, baseWidth) + xOff;
	  lx1 = max(lx1, 0);
	  lx2 =
	    roundingScale(rm->chromStart + rm->blockRelStarts[idx + 1] -
			  winStart, width, baseWidth) + xOff;

	  int midPoint = ((lx2 - lx1) / 2) + lx1;

	  // Block Intersection Line
	  hvGfxLine(hvg, lx1, y + alignedBlockOffset, lx1, y +
		    alignedBlockOffset + alignedBlockHeight, black);

	  // Line Up
	  hvGfxLine(hvg, lx1, y + alignedBlockOffset, midPoint, y +
		    unalignedBlockOffset, black);

	  // Line Down
	  hvGfxLine(hvg, lx2, y + alignedBlockOffset, midPoint,
		    y + unalignedBlockOffset, black);

	}
	else
	  if (alignedOverlapSize > 0 &&
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
	    rm->blockRelStarts[idx - 1] + rm->blockSizes[idx - 1] -
	    smallOverlapLen;
	  lx1 =
	    roundingScale(relStart + rm->chromStart - winStart,
			  width, baseWidth) + xOff;
	  lx1 = max(lx1, 0);
	  lx2 =
	    roundingScale(relStart + rm->chromStart + unaSize -
			  winStart, width, baseWidth) + xOff;
	  // Line Up
	  cx1 =
	    roundingScale(rm->chromStart + rm->blockRelStarts[idx - 1] +
			  rm->blockSizes[idx - 1] - winStart, width,
			  baseWidth) + xOff;
	  hvGfxLine(hvg, cx1, y + alignedBlockOffset, lx1,
		    y + unalignedBlockOffset, black);

	  // Line Across
	  drawShortDashedHorizLine(hvg, lx1, lx2,
				   y + unalignedBlockOffset, 5, 5, black);
	  // Line Down
	  cx2 =
	    roundingScale(rm->chromStart + rm->blockRelStarts[idx + 1] -
			  winStart, width, baseWidth) + xOff;
	  hvGfxLine(hvg, cx2, y + alignedBlockOffset, lx2,
		    y + unalignedBlockOffset, black);
	}
	else
	{
	  // Adequate to display full length
	  relStart =
	    rm->blockRelStarts[idx - 1] + rm->blockSizes[idx - 1] -
	    (alignedOverlapSize / 2);
	  lx1 =
	    roundingScale(relStart + rm->chromStart - winStart,
			  width, baseWidth) + xOff;
	  lx1 = max(lx1, 0);
	  lx2 =
	    roundingScale(relStart + rm->chromStart + unaSize -
			  winStart, width, baseWidth) + xOff;
	  // Line Up
	  int cx1 =
	    roundingScale(rm->chromStart + rm->blockRelStarts[idx - 1] +
			  rm->blockSizes[idx - 1] - winStart,
			  width, baseWidth) + xOff;
	  hvGfxLine(hvg, cx1, y + alignedBlockOffset, lx1, y +
		    unalignedBlockOffset, black);
	  drawDashedHorizLine(hvg, lx1, lx2,
			      y + unalignedBlockOffset, 5, 5, black);
	  // Line Down
	  cx2 =
	    roundingScale(rm->chromStart + rm->blockRelStarts[idx + 1] -
			  winStart, width, baseWidth) + xOff;
	  hvGfxLine(hvg, cx2, y + alignedBlockOffset, lx2,
		    y + unalignedBlockOffset, black);
	}
      }
    }
  }

}

/* This is the original repeat drawing routine, modified
 * to handle the new rmskJoined table structure.  
 */
static void
origRepeatDraw(struct track *tg, int seqStart, int seqEnd,
	       struct hvGfx *hvg, int xOff, int yOff, int width,
	       MgFont * font, Color color, enum trackVisibility vis)
{
  struct itemSpecifics *extraData = tg->extraUiData;
  int baseWidth = seqEnd - seqStart;
  struct repeatItem *ri;
  int y = yOff;
  int heightPer = tg->heightPer;
  int lineHeight = tg->lineHeight;
  int x1, x2, w;
  boolean isFull = (vis == tvFull);
  Color col;
  struct sqlConnection *conn = hAllocConn(database);
  struct sqlResult *sr = NULL;
  char **row;
  int rowOffset;

  if (isFull)
  {
    /*
       Do gray scale representation spread out among tracks. 
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
	ri = extraData->otherRepeatItem;
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
	    ro->chromStart + ro->blockRelStarts[idx] + ro->blockSizes[idx];

	  x1 = roundingScale(blockStart - winStart, width, baseWidth) + xOff;
	  x1 = max(x1, 0);
	  x2 = roundingScale(blockEnd - winStart, width, baseWidth) + xOff;
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
      dyStringPrintf(query,
		     "select chromStart,blockCount,blockSizes,"
                     "blockRelStarts from %s where ",
		     table);
      if (hasBin)
	hAddBinToQuery(winStart, winEnd, query);
      dyStringPrintf(query, "chromStart<%u and chromEnd>%u ", winEnd,
		     winStart);
      /*
       * if we're using a single rmsk table, add chrom to the where clause 
       */
      if (startsWith("rmskJoined", table))
	dyStringPrintf(query, " and chrom = '%s' ", chromName);
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
	    int blockEnd = chromStart + blockRelStarts[idx] + blockSizes[idx];

	    x1 =
	      roundingScale(blockStart - winStart, width, baseWidth) + xOff;
	    x1 = max(x1, 0);
	    x2 = roundingScale(blockEnd - winStart, width, baseWidth) + xOff;
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
static void
jRepeatDraw(struct track *tg, int seqStart, int seqEnd,
	    struct hvGfx *hvg, int xOff, int yOff, int width,
	    MgFont * font, Color color, enum trackVisibility vis)
{
  struct itemSpecifics *extraData = tg->extraUiData;
  int baseWidth = seqEnd - seqStart;
  /*
   * Its unclear to me why heightPer is not updated to the
   * value set in jRepeatItemHeight() at the time this callback
   * is invoked.  Getting the correct value myself.
   * was:
   * int heightPer = tg->heightPer;
   */
  int heightPer = jRepeatItemHeight(tg, NULL);
  boolean isFull = (vis == tvFull);
  struct rmskJoined *rm;

  // If we are in full view mode and the scale is sufficient,
  // display the new visualization.
  if (isFull && baseWidth <= REPEAT_DETAIL_VIEW)
  {
    int level = yOff;

    struct subTrack *st = hashFindVal(extraData->subTracksHash, tg->table );
    if ( ! st )
      return;
    int lidx = st->levelCount;
    int currLevel = 0;
    for (currLevel = 0; currLevel < lidx; currLevel++)
    {
      rm = st->levels[currLevel];
      while (rm)
      {
	drawRMGlyph(hvg, level, heightPer, width, baseWidth, xOff, rm, extraData);

	char statusLine[128];
	int ss1 = roundingScale(rm->alignStart - winStart,
				width, baseWidth) + xOff;
        
	safef(statusLine, sizeof(statusLine), "name: %s", rm->name );

	int x1 =
	  roundingScale(rm->alignStart - winStart, width, baseWidth) + xOff;
	x1 = max(x1, 0);
	int x2 =
	  roundingScale(rm->alignEnd - winStart, width, baseWidth) + xOff;
	int w = x2 - x1;
	if (w <= 0)
	  w = 1;

	mapBoxHc(hvg, rm->alignStart, rm->alignEnd, ss1, level, w, heightPer,
        		 tg->track, rm->id, statusLine);
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

void
jRepeatMethods(struct track *tg)
{
  tg->loadItems = jRepeatLoad;
  tg->freeItems = jRepeatFree;
  tg->drawItems = jRepeatDraw;
  tg->colorShades = shadesOfGray;
  tg->itemName = jRepeatName;
  tg->mapItemName = jRepeatName;
  tg->totalHeight = jRepeatTotalHeight;
  tg->itemHeight = jRepeatItemHeight;
  tg->itemStart = tgItemNoStart;
  tg->itemEnd = tgItemNoEnd;
  tg->mapsSelf = TRUE;
}
