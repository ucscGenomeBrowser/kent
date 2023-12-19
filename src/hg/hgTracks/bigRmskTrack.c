/* bigRmskTrack - A comprehensive RepeatMasker visualization track
 *                handler for bigRmsk file ( BED9+5 ) track hubs.
 *                Based on previous visualization work with table
 *                supported rmskJoined tracks.
 *
 *
 *  Written by Robert Hubley 10/2021
 *
 *  Modifications:
 *     6/6/22  : Added doLeftLabels functionality to Full/Pack modes
 *
 */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "bigRmskUi.h"

/* Track Development Notes:
 *
 * Summary of concerns:
 *      1. tg->loadItems:
 *            Should be responsible for loading data "items".
 *            In our case they will be bigRmskRecords.
 *      2. tg->totalHeight:
 *            Should be responsible for layout calculations
 *            that determine the totalHeight of the track at the
 *            current scale and/or visual style ( 'pack', 'full' etc. )
 *            NOTE: This doesn't include the track center label (if present)
 *      3. tg->preDrawItems:
 *            Not sure the use case for this. 
 *      4. tg->drawItems:
 *            Pretty self explanatory.
 *      5. tg->drawLeftLabels:
 *            Draw labels to left of track (if desired).
 *
 * Detailed track flow:
 *      mainMain:
 *        cartHtmlShell ... doMiddle()
 *        
 *      hgTrack:
 *        doMiddle() ### called by mainMain ###
 *            tracksDisplay()
 *                doTrackForm() 
 *                    remoteParallelLoad() 
 *                        Call track->loadItems()
 *                    -------------or--------------
 *                    Call track->loadItems() 
 *
 *                    printTrackInitJavascript() 
 *                        makeActiveImage()
 *                            doLeftLabels()
 *                                trackPlusLabelHeight()
 *                                    Call subtrack->totalHeight()
 *                            doCenterLabels()
 *                                Call track->totalHeight()
 *                            doPreDrawItems()
 *                            doDrawitems() 
 *                                Call track->drawItems()
 *                                Call track->totalHeight()
 *                            Call track->drawLeftLabels()
 *                            doTrackMap()
 *                                Call subtrack->totalHeight()
 */


/* Structure to hold repeat class names/colors */
struct classRecord
    {
      struct classes *next; // just in case we want to use it as a slList
      char *className;
      int  layoutLevel;
      Color color;
    };

/* The primary "item" record used by this track */
struct bigRmskRecord
    {
    struct bigRmskRecord *next;  /* Next in singly linked list. */
    char *chrom;                 /* Reference sequence chromosome or scaffold */
    unsigned chromStart;         /* Start position of feature in chromosome */
    unsigned chromEnd;           /* End position feature in chromosome */
    char *name;                  /* Annotation name [ typically TE family identifier ] */
    unsigned score;              /* Score from 0-1000 */
    char strand[2];              /* + or - */
    unsigned alignStart;         /* Start position of aligned portion of feature */
    unsigned alignEnd;           /* End position of aligned portion of feature */
    unsigned reserved;           /* Used as itemRgb */
    int blockCount;              /* Number of blocks */
    int *blockSizes;             /* Comma separated list of block sizes */
    int *blockRelStarts;         /* Start positions rel. to chromStart or -1 for unaligned blocks */
    unsigned id;                 /* ID to bed used in URL to link back */
    char *description;           /* Long description of item for the details page */
    unsigned visualStart;        /* For use by layout routines -- calc visual start (in bp coord)*/
    unsigned visualEnd;          /* For use by layout routines -- calc visual end (in bp coord)*/
    int layoutLevel;             /* For use by layout routines -- layout row */
    int leftLabel;               /* For use by visualization routines -- left side labels */
    };

/* Datastructure for graph-based layout */
struct Graph {
    // the number of nodes in the graph
    int n;
    // an array of edges for each node, size n^2
    //     access: g->edges[(i * n) + j]
    int *edges;
    // an array of degrees for each node
    //     access: g->degrees[i]
    int *degrees;
};

/* DETAIL_VIEW_MAX_SCALE: The size ( window bp ) at which the
 * detailed view of repeats is turned allowed, otherwise it reverts
 * back to the original UCSC glyph. I chose not to make this a function of
 * data density so that switching between views could be more predictable.
 */
#define DETAIL_VIEW_MAX_SCALE 45000

/* MAX_UNALIGNED_PIXEL_LEN: The maximum size of unaligned sequence
 * to draw before switching to the unscaled view: ie. "----/###/---"
 * This is measured in pixels *not* base pairs due to the
 * mixture of scaled ( lines ) and unscaled ( text ) glyph
 * components.
 */
#define MAX_UNALIGNED_PIXEL_LEN 150

/* LABEL_PADDING: The amount of space to reserve to the left
 * of the label (in px).
 */
#define LABEL_PADDING 20 

/* MINHEIGHT: The minimum height for a track row.  
 */
#define MINHEIGHT 24

/* MAX_PACK_ITEMS: The maximum number of items to show in pack mode.  Over
 * this value and the track switches to dense mode.
 */
#define MAX_PACK_ITEMS 500

/* MIN_VISIBLE_EXT_PIXELS: Minimum unextended region to show (in pixels).  If
 * under this value the vertical and horizontal arms are completely omitted.
 */
#define MIN_VISIBLE_EXT_PIXELS 4

/* Hash of the repeatItems ( tg->items ) for this track.
 *   This is used to hold display names for the lines of
 *   the track, the line heights, and class colors.
 */
static struct hash *classHashBR = NULL;

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
              // Current             // Previously
0xff1f77b4,   // SINE - blue         // SINE - red
0xffff7f0e,   // LINE - orange       // LINE - lime
0xff2ca02c,   // LTR - green         // LTR - maroon
0xffd62728,   // DNA - red           // DNA - fuchsia
0xff9467bd,   // Simple - purple     // Simple - yellow
0xff8c564b,   // LowComplex - brown  // LowComplex - olive
0xffe377c2,   // Satellite - pink    // Satellite - blue
0xff7f7f7f,   // RNA - grey          // RNA - green
0xffbcbd22,   // Other - lime        // Other - teal
0xff17becf,   // Unknown - aqua      // Unknown - aqua
};


static int cmpStartEnd(const void *va, const void *vb)
/* Sort repeats by alignStart (thickStart) ascending and alignEnd
 * (thickEnd) descending. ie. sort by position (longest first)
 */
{
struct bigRmskRecord *a = *((struct bigRmskRecord **) va);
struct bigRmskRecord *b = *((struct bigRmskRecord **) vb);

unsigned startDiff = (a->alignStart - b->alignStart);
if ( startDiff != 0 ) 
  return (startDiff);
return (b->alignEnd - a->alignEnd);
}


static void getVisualExtentsInBP(struct bigRmskRecord *rm, float pixelsPerBase, unsigned *pStart, unsigned *pEnd, bool showUnalignedExtents, bool showLabels)
/* Calculate the glyph visual extents in genome coordinate
 * space ( ie. bp )
 *
 * It is not straightforward to determine the extent
 * of this track's glyph due to the mixture of graphic
 * text elements within the glyph.
 */
{
int start;
int end;
char lenLabel[20];

// Start position is anchored by the alignment start
// coordinates.  Then we subtract from that space for
// the label.
if ( showLabels ) 
    {
    start = rm->alignStart -
            (int) ((mgFontStringWidth(tl.font, rm->name) +
            LABEL_PADDING) / pixelsPerBase);
    // Fix for redmine #29473
    if ( start < 0 )
        start = 0;
    }
else
    {
    start = rm->alignStart;
    }

// Now subtract out space for the unaligned sequence
// bar starting off the graphics portion of the glyph.
if ( showUnalignedExtents ) 
    {
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
    }

end = rm->alignEnd;

if ( showUnalignedExtents )
    {
    if ((rm->blockSizes[rm->blockCount - 1] * pixelsPerBase) > MAX_UNALIGNED_PIXEL_LEN)
        {
        safef(lenLabel, sizeof(lenLabel), "%d",
              rm->blockSizes[rm->blockCount - 1]);
        end += (int) (MAX_UNALIGNED_PIXEL_LEN / pixelsPerBase) +
               (int) ((mgFontStringWidth(tl.font, lenLabel) +
               LABEL_PADDING) / pixelsPerBase);
        }
    else
        {
        end += rm->blockSizes[rm->blockCount - 1];
        }
    }

*pStart = start;
*pEnd = end;
}


bool overlap(struct bigRmskRecord *a, struct bigRmskRecord *b, int tolerance) 
/* Determine if two glyphs will overlap */
{
return ((a->visualStart - tolerance) <= b->visualEnd) && ((a->visualEnd + tolerance) >= b->visualStart);
}


struct Graph *newGraph(struct bigRmskRecord **recs, int n) 
/* Create a new layout graph */
{
struct Graph *g = malloc(sizeof(struct Graph));
g->n = n;
g->edges = malloc((n * n) * sizeof(int));
g->degrees = malloc((n) * sizeof(int));

for (int i = 0; i < n; i++) 
    {
    g->degrees[i] = 0;
    for (int j = 0; j < n; j++) 
        {
        g->edges[i * n + j] = -1;
        }
    }

// all vs all comparison of all nodes to check for overlap
for (int idxA = 0; idxA < n; idxA++) 
    {
    struct bigRmskRecord annA = *recs[idxA];
    for (int idxB = 0; idxB < n; idxB++) 
        {
        struct bigRmskRecord annB = *recs[idxB];
        if (idxA == idxB)
            continue;
        if (overlap(&annA, &annB, 0)) 
            {
            g->edges[(idxA * n) + g->degrees[idxA]] = idxB;
            g->degrees[idxA]++;
            }
        }
    }
return (g);
}


void removeIdx(int idx, int *arr, int nElements) 
/* Remove entry in list by overwriting element at position idx and
 * shifting all remaining elements down by one position.
 * Support routine for detailedLayout(). 
 */
{
for (int i = idx; i < nElements - 1; i++) 
    {
    arr[i] = arr[i + 1];
    }
arr[nElements - 1] = -1;
}


void swap(int *xp, int *yp) 
/* Swap elements in a list.  Support routine
 * for detailedLayout()
 */
{
int temp = *xp;
*xp = *yp;
*yp = temp;
}


void sortRemainingByWidth(int *remaining, struct bigRmskRecord **recs, int n) 
{
int i, j, maxIdx;
struct bigRmskRecord *a, *b;
for (i = 0; i < n - 1; i++) 
    {
    maxIdx = i;
    for (j = i + 1; j < n; j++) 
        {
        a = recs[remaining[j]];
        b = recs[remaining[maxIdx]];
        if ((a->alignEnd - a->alignStart) > (b->alignEnd - b->alignStart))
            maxIdx = j;
        }
    swap(&remaining[maxIdx], &remaining[i]);
    }
}


void detailedLayout(struct bigRmskRecord *firstRec, double pixelsPerBase) {
/*
 * Compute a non-overlapping layout for a list of annotations
 *
 * We define a graph in which each annotation is represented as a node.
 * Nodes share an edge if the annotations they represent horizontally overlap.
 *
 * In each iteration, this algorithm picks the first available node and then
 * greedily grows an independent set. The nodes in each independent set are
 * assigned a unique color/row. Once a node is assigned a color, it is permanently
 * removed from further consideration.
 *
 * For book keeping, we keep two lists: a list of remaining uncolored node
 * indices, and a boolean list that describes whether or not a node is available
 * during the current iteration.
 *
 * This layout routine was written by Jack Roddy
 */
int nRecs = slCount(firstRec);
struct bigRmskRecord **recs = malloc(nRecs * sizeof(struct bigRmskRecord *));
struct bigRmskRecord *rec = firstRec;

boolean showUnalignedExtents = cartUsualBoolean(cart, BIGRMSK_SHOW_UNALIGNED_EXTENTS, 
                                                BIGRMSK_SHOW_UNALIGNED_EXTENTS_DEFAULT);

boolean showLabels = cartUsualBoolean(cart, BIGRMSK_SHOW_LABELS, BIGRMSK_SHOW_LABELS_DEFAULT);

int n = 0;
while (rec != NULL) 
    {
    getVisualExtentsInBP(rec, pixelsPerBase, &(rec->visualStart),&(rec->visualEnd), 
                         showUnalignedExtents, showLabels);
    recs[n++] = rec;
    rec = rec->next;
    }

struct Graph *g = newGraph(recs, n);

int nextColor = 0;
int nRemaining = n;
int *remaining = malloc(sizeof(int) * n);
bool *available = malloc(sizeof(bool) * n);

for (int i = 0; i < n; i++) 
    {
    remaining[i] = i;
    available[i] = TRUE;
    }

sortRemainingByWidth(remaining, recs, n);
while (nRemaining > 0) 
    {
    // at the start of every iteration, set everything remaining to available
    for (int i = 0; i < nRemaining; i++) 
        {
        available[remaining[i]] = TRUE;
        }
    // for every remaining node
    for (int i = 0; i < nRemaining; i++) 
        {
        // find the index in the rec array
        int uIdx = remaining[i];
        if (available[uIdx]) 
            {
            // if node u is available, assign a color/row
            recs[uIdx]->layoutLevel = nextColor;
            int *uEdges = &(g->edges[uIdx * n]);
            int uDegree = g->degrees[uIdx];
            // iterate across u's edges and set them to unavailable for this iteration
            for (int j = 0; j < uDegree; j++) 
                {
                int vIdx = uEdges[j];
                available[vIdx] = FALSE;
                }
            // u is unavailable since we just placed it
            available[uIdx] = FALSE;
            // remove uIdx from remaining
            removeIdx(i, remaining, nRemaining);
            nRemaining--;
            i--;
            }
        }
    nextColor++;
    }

free(remaining);
free(available);
free(recs);
}


void bigRmskLayoutItems(struct track *tg)
{
/* After the items have been loaded and before any rendering occurs, 
 * this function delegates the layout based on the current visualization
 * type.  
 *
 * I am not sure how permissible it is to access winStart/winEnd/insideWidth
 * as globals.  This seems to be the pattern that I see used in other tracks
 * but it makes me uncomfortable.
 */
int baseWidth = winEnd - winStart;
double pixelsPerBase = scaleForPixels(insideWidth);
struct bigRmskRecord *items = tg->items;

boolean showLabels = cartUsualBoolean(cart, BIGRMSK_SHOW_LABELS, BIGRMSK_SHOW_LABELS_DEFAULT);
boolean origPackViz = cartUsualBoolean(cart, BIGRMSK_ORIG_PACKVIZ, BIGRMSK_ORIG_PACKVIZ_DEFAULT);

if ((tg->visibility == tvSquish) || (tg->visibility == tvDense))
  showLabels = FALSE;

if (tg->visibility == tvFull && baseWidth <= DETAIL_VIEW_MAX_SCALE)
    {
    // Perform the most detailed glyph layout aka "Full" mode.
    detailedLayout(items, pixelsPerBase);
    } 
else
    {
    // Either class based or single row...dense
    char class[256];
    struct classRecord *cr;

    if ( origPackViz ) 
        {
        // Original "pack" visualization displays greyscale box annotations
        // for each "unjoined" feature on a repeat class specific row.
        while (items)
            {
            struct bigRmskRecord *rr = items;
            items = items->next;
 
            // Extract the class from the annotation name
            //   - Simplify repClass for lookup: strip trailing '?',
            //     simplify *RNA to RNA:
            char *poundPtr = index(rr->name, '#');
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
            cr = hashFindVal(classHashBR, class);
            if (cr == NULL)
                cr = hashFindVal(classHashBR, "Other");
            rr->layoutLevel = cr->layoutLevel;
            }
        }
    else 
        {
        // New "pack" visualization are joined features with labels.
        slSort(&items, cmpStartEnd);
        // If you slSort you reset the linked list and it's head.
        tg->items = items;
            
        int inLevel = 0;
        while (items)
            {
            struct bigRmskRecord *rr = items;
            items = items->next;
    
            if ( rr->layoutLevel < 0 )
                {
                unsigned rrChromStart, rrChromEnd;
                getVisualExtentsInBP(rr, pixelsPerBase, &rrChromStart, &rrChromEnd, 0, showLabels);
                rr->layoutLevel = inLevel;
    
                struct bigRmskRecord *rm = NULL;
                unsigned rmChromStart, rmChromEnd;
                rm = items;
                while (rm)
                    {
                        if ( rm->layoutLevel < 0 )
                        {
                        getVisualExtentsInBP(rm, pixelsPerBase, &rmChromStart, &rmChromEnd, 0, showLabels);
                        if (rmChromStart > rrChromEnd)
                            {
                            rm->layoutLevel = inLevel;
                            rrChromEnd = rmChromEnd;
                            }
                        }
                    rm = rm->next;
                    } // while ( rm )
                inLevel++;
                } // if ( rr->layoutLevel...
            } // while ( items )
        } // if(...
    }
}


static void bigRmskLoadItems(struct track *tg)
/* 
 * Loading is done first given the region the display is currently centered on.
 * This function is responsible for fetching the data *and* defining the number
 * of "items" that will be displayed.  An item represents a row of the track.
 * Therefore, while this is a "loading" function it also needs to perform enough
 * of the layout to determine how many "items" to generate.  
 */
{
int i;
if ( classHashBR == NULL )
    {
    struct classRecord *cr;
    int numClasses = ArraySize(rptClasses);
    classHashBR = newHash(6);
    for (i = 0; i < numClasses; ++i)
        {
        AllocVar(cr);
        cr->className = rptClassNames[i];
        cr->layoutLevel = i;
        unsigned int colorInt = rmskJoinedClassColors[i];
        cr->color = MAKECOLOR_32(((colorInt >> 16) & 0xff),((colorInt >> 8) & 0xff),((colorInt >> 0) & 0xff));
        hashAdd(classHashBR, rptClasses[i], cr);
        }
    }

struct bigRmskRecord *detailList = NULL;
if (tg->isBigBed)
    {
    struct bbiFile *bbi = fetchBbiForTrack(tg);
    if (bbi->fieldCount < 14)
        errAbort("track %s has a bigBed being read as a bed14 that has %d columns.", tg->track, bbi->fieldCount);
    struct lm *lm = lmInit(0);
    struct bigBedInterval *bb, *bbList =  bigBedIntervalQuery(bbi, chromName, winStart, winEnd, 0, lm);
    char *bedRow[bbi->fieldCount];
    /* One might ask, "Why didn't you use a linkedFeature set here?" Good question!  I am not using
       a standard encoding of blockStarts as would be expected by a BED12 validator, and therefore
       I must use BED9+ and handle the blocks myself. */
    char startBuf[16], endBuf[16];

    char *filterString = cartUsualString(cart, BIGRMSK_NAME_FILTER, BIGRMSK_NAME_FILTER_DEFAULT);
    boolean isFilterRegexp = cartUsualBoolean(cart, BIGRMSK_REGEXP_FILTER, BIGRMSK_REGEXP_FILTER_DEFAULT);
    regex_t regEx;

    if (isFilterRegexp)
        regcomp(&regEx, filterString, REG_NOSUB);

    for (bb = bbList; bb != NULL; bb = bb->next)
        {
        bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));

        if ( (isFilterRegexp && (regexec(&regEx,bedRow[3], 0, NULL,0 ) == 0)) ||
             (!isFilterRegexp && wildMatch(filterString, bedRow[3])) )

            {
            struct bigRmskRecord *rm = NULL;
            AllocVar(rm);
            rm->next = NULL;
            rm->chrom = cloneString(bedRow[0]);
            rm->chromStart = sqlUnsigned(bedRow[1]);
            rm->chromEnd = sqlUnsigned(bedRow[2]);
            rm->name = cloneString(bedRow[3]);
            rm->score = sqlUnsigned(bedRow[4]);
            safef(rm->strand, sizeof(rm->strand), "%s", bedRow[5]);
            rm->alignStart = sqlUnsigned(bedRow[6]);
            rm->alignEnd = sqlUnsigned(bedRow[7]);
            // RGB is unused? bedRow[8]
            rm->blockCount = sqlUnsigned(bedRow[9]);
            {
            int sizeOne;
            sqlSignedDynamicArray(bedRow[10], &rm->blockSizes, &sizeOne);
            assert(sizeOne == rm->blockCount);
            }
            {
            int sizeOne;
            sqlSignedDynamicArray(bedRow[11], &rm->blockRelStarts, &sizeOne);
            assert(sizeOne == rm->blockCount);
            }
            rm->id = sqlUnsigned(bedRow[12]);
            rm->description = cloneString(bedRow[13]);
            rm->layoutLevel = -1;  // Indicates layout has not been calculated.
            rm->leftLabel = 0;     // Flag for labels that need to be leftLabeled
            slAddHead(&detailList, rm);
            }
        } // for(bb = bbList...
    bbiFileClose(&bbi);
    lmCleanup(&lm);
    } // if (tg->isBigBed...
      //Could also...have DB loader here in the future.
    tg->items = detailList;
    // Man, I went back and forth on this one.  I *wish* there was
    // an API for layout in between the loadItems() and totalHeight()
    // stubs. The totalHeight() method is called multiple times which
    // isn't ideal for layout operations since the layout logic needs
    // to detect if it needs to really re-layout the data (expensive).
    // For now, this is the only way to be assured that it gets called
    // once for real changes in the visual appearance.
    bigRmskLayoutItems(tg);
}
  

int bigRmskItemHeight(struct track *tg, void *item)
{
enum trackVisibility vis = tg->visibility;
if (vis == tvDense) 
    tg->heightPer = tl.fontHeight;
else if (vis == tvPack)
    tg->heightPer = tl.fontHeight;
else if (vis == tvSquish)
    tg->heightPer = tl.fontHeight/2;
else if (vis == tvFull )
    {
    if ( winBaseCount <= DETAIL_VIEW_MAX_SCALE)
        tg->heightPer = max(tl.fontHeight, MINHEIGHT);
    else 
        tg->heightPer = tl.fontHeight;
    }
tg->lineHeight = tg->heightPer + 1;

return(tg->heightPer);
}


int bigRmskTotalHeight(struct track *tg, enum trackVisibility vis)
{
boolean origPackViz = cartUsualBoolean(cart, BIGRMSK_ORIG_PACKVIZ, BIGRMSK_ORIG_PACKVIZ_DEFAULT);

// Ensure that lineHeight is set correctly
bigRmskItemHeight(tg, (void *)NULL);

int count = slCount(tg->items);
if ( count > MAX_PACK_ITEMS || vis == tvDense ) 
    {
    tg->height = tg->lineHeight;
    return(tg->height);
    }
else
    {
    // Count the layout depth 
    struct bigRmskRecord *cr;
    int numLevels = 0;
    for ( cr = tg->items; cr != NULL; cr = cr->next )
        {
        if ( cr->layoutLevel > numLevels )
            numLevels = cr->layoutLevel;
        }
    numLevels++;

    if (tg->limitedVis == tvFull && winBaseCount <= DETAIL_VIEW_MAX_SCALE)
        {
        tg->height = numLevels * max(tg->lineHeight, MINHEIGHT);
        }
    else 
        {
        if ( origPackViz ) 
            {
            // Original class-per-line track
            int numClasses = ArraySize(rptClasses);
            tg->height = numClasses * tg->lineHeight;
            }
        else
            {
            // Color pack track
            tg->height = numLevels * tg->lineHeight;
            }
        }
    return(tg->height);
    }
}


static void drawDenseGlyphs(struct track *tg, int seqStart, int seqEnd,
         struct hvGfx *hvg, int xOff, int yOff, int width,
         MgFont * font, Color color, enum trackVisibility vis)
/* draw 'dense' mode track */
{
int x1, x2, w;
int baseWidth = seqEnd - seqStart;
int heightPer = tg->heightPer;
struct classRecord *ri;
Color col;

struct bigRmskRecord *cr;
for (cr = tg->items; cr != NULL; cr = cr->next)
    {
    // Break apart the name and get the class of the
    // repeat.
    char class[256];
    // Simplify repClass for lookup: strip trailing '?',
    // simplify *RNA to RNA:
    char *poundPtr = index(cr->name, '#');
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
    ri = hashFindVal(classHashBR, class);
    if (ri == NULL)
        ri = hashFindVal(classHashBR, "Other");
    col = ri->color;

    int idx = 0;
    for (idx = 0; idx < cr->blockCount; idx++)
        {
        if (cr->blockRelStarts[idx] >= 0)
            {
            int blockStart = cr->chromStart + cr->blockRelStarts[idx];
            int blockEnd =
                    cr->chromStart + cr->blockRelStarts[idx] + cr->blockSizes[idx];

            x1 = roundingScale(blockStart - winStart,
                               width, baseWidth) + xOff;
            x1 = max(x1, 0);
            x2 = roundingScale(blockEnd - winStart, width,
                               baseWidth) + xOff;
            w = x2 - x1;
            if (w <= 0)
                w = 1;
            hvGfxBox(hvg, x1, yOff, w, heightPer, col);
            }
        }
    }
}


static void drawOrigPackGlyphs(struct track *tg, int seqStart, int seqEnd,
         struct hvGfx *hvg, int xOff, int yOff, int width,
         MgFont * font, Color color, enum trackVisibility vis)
/* Do grayscale representation spread out individual class-specific rows */
{
int y = yOff;
int percId;
int grayLevel;
Color col;
int x1, x2, w;
int baseWidth = seqEnd - seqStart;
int heightPer = tg->heightPer;
// Added per ticket #29469
char idStr[80];
char statusLine[128];

// bugfix ticket #28644
int trackHeight = bigRmskTotalHeight(tg, vis);
hvGfxSetClip(hvg, xOff, yOff, width, trackHeight);

struct bigRmskRecord *cr;
for (cr = tg->items; cr != NULL; cr = cr->next)
    {
    percId = 1000 - cr->score;
    grayLevel = grayInRange(percId, 500, 1000);
    col = shadesOfGray[grayLevel];

    // Get id for click handler
    sprintf(idStr,"%d",cr->id);

    int idx = 0;
    for (idx = 0; idx < cr->blockCount; idx++)
        {
        if (cr->blockRelStarts[idx] >= 0)
            {
            int blockStart = cr->chromStart + cr->blockRelStarts[idx];
            int blockEnd = cr->chromStart + cr->blockRelStarts[idx] +
                       cr->blockSizes[idx];

            x1 = roundingScale(blockStart - winStart, width,
                           baseWidth) + xOff;
            x1 = max(x1, 0);
            x2 = roundingScale(blockEnd - winStart, width,
                           baseWidth) + xOff;
            y = yOff + (cr->layoutLevel * tg->lineHeight);
            w = x2 - x1;
            if (w <= 0)
                w = 1;
            hvGfxBox(hvg, x1, y, w, heightPer, col);

            // feature change ticket #29469
            safef(statusLine, sizeof(statusLine), "%s", cr->name);
            mapBoxHc(hvg, cr->alignStart, cr->alignEnd, x1, y,
                w, heightPer, tg->track, idStr, statusLine);
            }
        }
    }//for (cr = ri->itemData...
}


static void drawPackGlyphs(struct track *tg, int seqStart, int seqEnd,
         struct hvGfx *hvg, int xOff, int yOff, int width,
         MgFont * font, Color color, enum trackVisibility vis)
/* Do color representation packed closely */
{
boolean showLabels = cartUsualBoolean(cart, BIGRMSK_SHOW_LABELS, BIGRMSK_SHOW_LABELS_DEFAULT);
if ( vis == tvSquish ) 
  showLabels = FALSE;
int y = yOff;
Color col;
Color black = hvGfxFindColorIx(hvg, 0, 0, 0);
int x1, x2, w;
int baseWidth = seqEnd - seqStart;
int heightPer = tg->heightPer;
struct bigRmskRecord *cr;
struct classRecord *ri;
int fontHeight = mgFontLineHeight(font);
// Added per ticket #29469
char idStr[80];
char statusLine[128];

// bugfix ticket #28644
int trackHeight = bigRmskTotalHeight(tg, vis);
hvGfxSetClip(hvg, xOff, yOff, width, trackHeight);

for (cr = tg->items; cr != NULL; cr = cr->next)
    {
    // Break apart the name and get the class of the
    // repeat.
    char family[256];
    char class[256];
    // Simplify repClass for lookup: strip trailing '?',
    // simplify *RNA to RNA:
    char *poundPtr = index(cr->name, '#');
    if (poundPtr)
        {
        safecpy(family, sizeof(family), cr->name);
        poundPtr = index(family, '#');
        *poundPtr = '\0';
        safecpy(class, sizeof(class), poundPtr + 1);
        char *slashPtr = index(class, '/');
        if (slashPtr)
            *slashPtr = '\0';
        }
    else
        {
        safecpy(family, sizeof(family), 0);
        safecpy(class, sizeof(class), "Unknown");
        }
    char *p = &(class[strlen(class) - 1]);
    if (*p == '?')
        *p = '\0';
    if (endsWith(class, "RNA"))
        safecpy(class, sizeof(class), "RNA");

    // Lookup the class to get the color scheme
    ri = hashFindVal(classHashBR, class);
    if (ri == NULL)
        ri = hashFindVal(classHashBR, "Other");
    col = ri->color;

    if ( showLabels ) 
        {
        int stringWidth = mgFontStringWidth(font, family) + LABEL_PADDING;

        x1 = roundingScale(cr->alignStart - winStart, width,
                               baseWidth); 
        // Remove labels that overlap the window and use leftLabel instead
        if ( x1-stringWidth > 0 ) 
            {
            y = yOff + (cr->layoutLevel * tg->lineHeight);
            hvGfxTextCentered(hvg, x1 + xOff - stringWidth,
                      heightPer - fontHeight + y,
                      stringWidth, fontHeight, MG_BLACK, font,
                      family);
            cr->leftLabel = 0;
            }else
            { 
            x1 = roundingScale(cr->alignEnd - winStart, width,
                               baseWidth);
            if ( x1 > 0 ) 
                cr->leftLabel = 1;
            }
        }

    // Get id for click handler
    sprintf(idStr,"%d",cr->id);

    int idx = 0;
    int prevBlockEnd = -1;
    for (idx = 0; idx < cr->blockCount; idx++)
        {
        if (cr->blockRelStarts[idx] >= 0)
            {
            int blockStart = cr->chromStart + cr->blockRelStarts[idx];
            int blockEnd = cr->chromStart + cr->blockRelStarts[idx] +
                       cr->blockSizes[idx];

            x1 = roundingScale(blockStart - winStart, width,
                           baseWidth) + xOff;
            x1 = max(x1, 0);
            x2 = roundingScale(blockEnd - winStart, width,
                           baseWidth) + xOff;
            y = yOff + (cr->layoutLevel * tg->lineHeight);
            w = x2 - x1;
            if (w <= 0)
                w = 1;
            hvGfxBox(hvg, x1, y, w, heightPer, col);

            // feature change ticket #29469
            safef(statusLine, sizeof(statusLine), "%s", cr->name);
            mapBoxHc(hvg, cr->alignStart, cr->alignEnd, x1, y,
                w, heightPer, tg->track, idStr, statusLine);

            int midY = y + (heightPer>>1);
            int dir = 0;
            if (cr->strand[0] == '+')
                dir = 1;
            else if (cr->strand[0] == '-')
                dir = -1;
            Color textColor = hvGfxContrastingColor(hvg, col);
            clippedBarbs(hvg, x1, midY, w, tl.barbHeight, tl.barbSpacing,
                         dir, textColor, TRUE);
            if ( prevBlockEnd > 0 )
                {
                int px1 = roundingScale(prevBlockEnd - winStart, width,
                           baseWidth) + xOff;
                px1 = max(px1, 0);
                hvGfxLine(hvg, px1, midY, x1, midY, black);
                }
            prevBlockEnd = blockEnd;
            }
        }
    }//for (cr = ri->itemData...
}
 

static void bigRmskFreeItems(struct track *tg)
/* Free up rmskJoinedMasker items. */
{
slFreeList(&tg->items);
}

static char * bigRmskItemName(struct track *tg, void *item)
// This is really not necessary...but placed it here anyway...
{
  return "";
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
// RMH: Fixed 8/4/22 : One-off error in height/width to x/y calcs
int y2 = y1 + height - 1;
int x2 = x1 + width - 1;
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
clippedBarbs(hvg, x1 + 1, midY, width - 2, ((height >> 1) - 2), 5,
          dir, white, TRUE);
}


static void drawRMGlyph(struct hvGfx *hvg, int y, int heightPer,
        int width, int baseWidth, int xOff, struct bigRmskRecord *rm,
            enum trackVisibility vis, boolean showUnalignedExtents, boolean showLabels)
/*
 *  Draw a detailed RepeatMasker annotation glyph given
 *  a single bigRmskRecord structure.
 *
 *  A couple of caveats about the use of the bigRmskRecord
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
int idx;
int lx1, lx2;
int cx1, cx2;
int end;
char lenLabel[20];
int w;
struct classRecord *ri;
// heightPer is the God given height of a single
// track item...respect your overlord.
int alignedBlockHeight = heightPer * 0.5;
int alignedBlockOffset = heightPer - alignedBlockHeight;
int unalignedBlockHeight = heightPer * 0.75;
int unalignedBlockOffset = heightPer - unalignedBlockHeight;
float pixelsPerBase = (float)width / (float)baseWidth;

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
ri = hashFindVal(classHashBR, class);
if (ri == NULL)
    ri = hashFindVal(classHashBR, "Other");
// Pick the fill color based on the divergence
int percId = 1000 - rm->score;
int grayLevel = grayInRange(percId, 500, 1000);
fillColor = shadesOfGray[grayLevel];
outlineColor = ri->color;

// Draw from left to right
for (idx = 0; idx < rm->blockCount; idx++)
    {
    int fragGStart = rm->blockRelStarts[idx];

    //  Assumptions about blockCount
    //   - first aligned block = 1, the block 0 is
    //     the unaligned consensus either 5' or 3' of
    //     this block.
    //   - The last aligned block is blockCount - 1;
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
                // Start with the assumption that we don't show
                // unaligned extentions.
            lx1 = roundingScale(rm->chromStart +
                     rm->blockRelStarts[idx + 1] -
                     winStart, width, baseWidth) + xOff;

            if ( showUnalignedExtents ) 
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
                if ((rm->blockSizes[idx] * pixelsPerBase) > MAX_UNALIGNED_PIXEL_LEN)
                    {
                    lx1 = roundingScale(rm->chromStart + 
                          (rm->blockRelStarts[idx + 1] -
                            (int) (MAX_UNALIGNED_PIXEL_LEN /
                                   pixelsPerBase)) -
                          winStart, width, baseWidth) + xOff;
                    // Line Across
                    drawDashedHorizLineWHash(hvg, lx1, lx2,
                                             y + unalignedBlockOffset,
                                             5, 5, black, rm->blockSizes[idx]);
                    // Line down to box
                    hvGfxLine(hvg, lx2, y + alignedBlockOffset, lx2,
                              y + unalignedBlockOffset, black);
                    // extension cap "|---"
                    hvGfxLine(hvg, lx1, y + unalignedBlockOffset - 3, lx1,
                              y + unalignedBlockOffset + 3, black);
     
                    }
                else
                    {
                    lx1 = roundingScale(rm->chromStart - winStart,
                                        width, baseWidth) + xOff;
                    // Only display extension if the resolution is high enough to
                    // fully see the extension line and cap distinctly: eg. "|--" 
                    if ( lx2-lx1 >= MIN_VISIBLE_EXT_PIXELS)
                        {
                        // Line Across
                        drawDashedHorizLine(hvg, lx1, lx2,
                                            y + unalignedBlockOffset, 5, 5, black);
                        // Line down to box
                        hvGfxLine(hvg, lx2, y + alignedBlockOffset, lx2,
                                  y + unalignedBlockOffset, black);
                        // extension cap "|---"
                        hvGfxLine(hvg, lx1, y + unalignedBlockOffset - 3, lx1,
                                  y + unalignedBlockOffset + 3, black);
                        }
                    }
               }

            if ( showLabels ) 
                {
                MgFont *font = tl.font;
                int fontHeight = tl.fontHeight;
                int stringWidth =
                     mgFontStringWidth(font, rm->name) + LABEL_PADDING;

                if ( lx1-stringWidth-xOff >= -(LABEL_PADDING/2) ) 
                    {
                    hvGfxTextCentered(hvg, lx1 - stringWidth,
                              heightPer - fontHeight + y,
                              stringWidth, fontHeight, MG_BLACK, font,
                              rm->name);
                    rm->leftLabel = 0;
                    }else
                    { 
                    end = rm->alignEnd;

                    if ( showUnalignedExtents )
                        {
                        if ((rm->blockSizes[rm->blockCount - 1] * pixelsPerBase) > MAX_UNALIGNED_PIXEL_LEN)
                            {
                            safef(lenLabel, sizeof(lenLabel), "%d",
                                  rm->blockSizes[rm->blockCount - 1]);
                            end += (int) (MAX_UNALIGNED_PIXEL_LEN / pixelsPerBase) +
                                   (int) ((mgFontStringWidth(tl.font, lenLabel) +
                                   LABEL_PADDING) / pixelsPerBase);
                            }
                        else
                            {
                            end += rm->blockSizes[rm->blockCount - 1];
                            }
                        }

                    // RMH: Should be no need to scale this.  Both numbers are
                    //      in bp coordinates.
                    //int endx = roundingScale(rm->visualEnd - winStart,
                    //                    width, baseWidth);
                    //if ( endx > 0 ) 
                    if ( rm->visualEnd >= winStart )
                        rm->leftLabel = 1;
                    }
                }
            }
        else if (idx == (rm->blockCount - 1))
            {
            if ( showUnalignedExtents )
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
                if ((rm->blockSizes[idx] * pixelsPerBase) > MAX_UNALIGNED_PIXEL_LEN)
                    {
                    lx2 = roundingScale(rm->chromStart +
                                        rm->blockRelStarts[idx - 1] +
                                        rm->blockSizes[idx - 1] +
                                        (int)(MAX_UNALIGNED_PIXEL_LEN /
                                        pixelsPerBase) - winStart,
                                        width, baseWidth) + xOff;
                    // Line Across
                    drawDashedHorizLineWHash(hvg, lx1, lx2, y +
                                             unalignedBlockOffset,
                                             5, 5, black, rm->blockSizes[idx]);
                    // Line down to box
                    hvGfxLine(hvg, lx1, y + alignedBlockOffset, lx1,
                              y + unalignedBlockOffset, black);
                    // Cap "--|"
                    hvGfxLine(hvg, lx2, y + unalignedBlockOffset - 3, lx2,
                              y + unalignedBlockOffset + 3, black);
     
                    }
                else
                    {
                    lx2 = roundingScale(rm->chromStart +
                                        rm->blockRelStarts[idx - 1] +
                                        rm->blockSizes[idx - 1] +
                                        rm->blockSizes[idx] -
                                        winStart, width, baseWidth) + xOff;
                    if ( lx2-lx1 >= MIN_VISIBLE_EXT_PIXELS)
                        {
                        // Line Across
                        drawDashedHorizLine(hvg, lx1, lx2,
                                            y + unalignedBlockOffset, 5, 5, black);
                        // Line down to box
                        hvGfxLine(hvg, lx1, y + alignedBlockOffset, lx1,
                                  y + unalignedBlockOffset, black);
                        // Cap "--|"
                        hvGfxLine(hvg, lx2, y + unalignedBlockOffset - 3, lx2,
                                  y + unalignedBlockOffset + 3, black);
                        }
                    }
                }
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
                                    rm->blockRelStarts[idx - 1] +
                                    rm->blockSizes[idx - 1] +
                                    unaSize - winStart, width,
                                    baseWidth) + xOff;
                lx1 = max(lx1, 0);
                lx2 = roundingScale(rm->chromStart +
                         rm->blockRelStarts[idx + 1] -
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
        } // if ( fragStart .. else {
    } // for(idx=..
}


static void bigRmskDrawItems(struct track *tg, int seqStart, int seqEnd,
         struct hvGfx *hvg, int xOff, int yOff, int width,
         MgFont * font, Color color, enum trackVisibility vis)
/* Main callback for displaying this track in the viewport
 * of the browser.
 */
{
int baseWidth = seqEnd - seqStart;
/*
 * Its unclear to me why heightPer is not updated to the
 * value set in bigRmskItemHeight() at the time this callback
 * is invoked.  Getting the correct value myself.
 * was:
 * int heightPer = tg->heightPer;
 */
int heightPer = bigRmskItemHeight(tg, NULL);
struct bigRmskRecord *rm;
boolean showUnalignedExtents = cartUsualBoolean(cart, BIGRMSK_SHOW_UNALIGNED_EXTENTS, BIGRMSK_SHOW_UNALIGNED_EXTENTS_DEFAULT);
boolean showLabels = cartUsualBoolean(cart, BIGRMSK_SHOW_LABELS, BIGRMSK_SHOW_LABELS_DEFAULT);
boolean origPackViz = cartUsualBoolean(cart, BIGRMSK_ORIG_PACKVIZ, BIGRMSK_ORIG_PACKVIZ_DEFAULT);

// If we are in full view mode and the scale is sufficient,
// display the detailed visualization.
if (vis == tvFull && baseWidth <= DETAIL_VIEW_MAX_SCALE)
    {
    int level;
    for (rm = tg->items; rm != NULL; rm = rm->next)
        {
        level = yOff + (rm->layoutLevel * tg->lineHeight);
        drawRMGlyph(hvg, level, heightPer, width, baseWidth, 
                    xOff, rm, vis, showUnalignedExtents, showLabels );
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

        char idStr[80];
        sprintf(idStr,"%d",rm->id);
        mapBoxHc(hvg, rm->alignStart, rm->alignEnd, ss1, level,
                w, heightPer, tg->track, idStr, statusLine);
        }
    }
else
    {
    // Experiment with further context sensitivity
    int count = slCount(tg->items);
    if ( count > MAX_PACK_ITEMS || vis == tvDense ) 
        {
        // Draw the single line view
        drawDenseGlyphs(tg, seqStart, seqEnd,
                         hvg,  xOff, yOff, width,
                         font, color,  vis);
        }
    else
        {
        if ( origPackViz )
            // Draw the stereotypical class view
            drawOrigPackGlyphs(tg, seqStart, seqEnd,
                                        hvg,  xOff, yOff, width,
                                        font, color,  vis);
        else
            // Draw new pack view
            drawPackGlyphs(tg, seqStart, seqEnd,
                                hvg,  xOff, yOff, width,
                                font, color,  vis);
        }
    }
}


void bigRmskLeftLabels(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, int height,
        boolean withCenterLabels, MgFont *font, Color color,
        enum trackVisibility vis)
/* draw the labels on the left margin */
{
boolean origPackViz = cartUsualBoolean(cart, BIGRMSK_ORIG_PACKVIZ, BIGRMSK_ORIG_PACKVIZ_DEFAULT);
Color labelColor = tg->labelColor;
labelColor = maybeDarkerLabels(tg, hvg, labelColor);
int heightPer = bigRmskItemHeight(tg, NULL);
int titleFontHeight = tl.fontHeight;
int labelFontHeight = mgFontLineHeight(font);
int yOffAfterTitle = yOff + titleFontHeight;
int fontOffset = heightPer - labelFontHeight;
int y = yOffAfterTitle;

if (vis == tvDense)
    {
    hvGfxTextRight(hvg, xOff, y, width - 1,
                   heightPer, labelColor, font, "Repeats");
    }
else if ( vis == tvPack && origPackViz ) 
    {
    int i;
    int numClasses = ArraySize(rptClasses);
    for (i = 0; i < numClasses; ++i)
        {
        hvGfxTextRight(hvg, xOff, y + fontOffset, width - 1,
                       heightPer, labelColor, font, rptClassNames[i]);
        y += heightPer;
        }
     }
else 
    {
    struct bigRmskRecord *cr;
int highestLevel = 0;
    for (cr = tg->items; cr != NULL; cr = cr->next)
        {
if ( cr->layoutLevel > highestLevel )
  highestLevel = cr->layoutLevel;
        if ( cr->leftLabel ) 
            {
            // Break apart the name and get the class of the
            // repeat.
            char family[256];
            // Simplify repClass for lookup: strip trailing '?',
            // simplify *RNA to RNA:
            safecpy(family, sizeof(family), cr->name);
            // RMH: For now allow the full name to show in left column.
            //      If this ends up being too wide we can make it an option
            //      to only show the identifier as was originally coded:
            //char *poundPtr = index(cr->name, '#');
            //if (poundPtr)
            //    {
            //    poundPtr = index(family, '#');
            //    *poundPtr = '\0';
            //    }
            y = yOffAfterTitle + (cr->layoutLevel * tg->lineHeight) + fontOffset;
            hvGfxSetClip(hvgSide, leftLabelX, y, insideWidth, tg->height);
            hvGfxTextRight(hvgSide, leftLabelX, y, leftLabelWidth-1, 
                           heightPer-fontOffset, color, font, family);
            hvGfxUnclip(hvgSide);
            }
        }
    }
}


void bigRmskMethods(struct track *track, struct trackDb *tdb,
                  int wordCount, char *words[])
{
bigBedMethods(track, tdb, wordCount, words);
track->loadItems = bigRmskLoadItems;
track->totalHeight = bigRmskTotalHeight;
track->drawItems = bigRmskDrawItems;
track->drawLeftLabels = bigRmskLeftLabels;
track->freeItems = bigRmskFreeItems;
track->colorShades = shadesOfGray;
track->itemName = bigRmskItemName;
track->mapItemName = bigRmskItemName;
track->itemHeight = bigRmskItemHeight;
track->itemStart = tgItemNoStart;
track->itemEnd = tgItemNoEnd;
track->mapsSelf = TRUE;
track->canPack = TRUE;
}
