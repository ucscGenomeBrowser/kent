
#include <limits.h>
#include "common.h"
#include "multiRegionSpace.h"
#include "hgTracks.h"
#include "bigBed.h"
#include "chain.h"
#include "bigChain.h"
#include "chainNetDbLoad.h"

struct baseRange
{
struct baseRange *next;
char *chrom;
unsigned start, end;
unsigned qStart, qEnd;
char strand;	/* '-' when the region is laid down reverse-complemented, else '+'. */
};

static void rangeSynToSrc(struct baseRange *range, int start, int end,
                          unsigned *retQStart, unsigned *retQEnd)
/* The source bases under synthetic [start,end), which must lie inside range.  A block is
 * ungapped and the same length on both sides;  on a reversed region the source runs
 * the other way, so the left end of the window is the high end of the source.
 * refs #37788 */
{
if (range->strand == '-')
    {
    *retQStart = range->qEnd - (end - range->start);
    *retQEnd = range->qEnd - (start - range->start);
    }
else
    {
    *retQStart = range->qStart + (start - range->start);
    *retQEnd = *retQStart + (end - start);
    }
}

static void rangeSrcToSyn(struct baseRange *range, unsigned qStart, unsigned qEnd,
                          unsigned *retStart, unsigned *retEnd)
/* The synthetic bases over source [qStart,qEnd), which must lie inside range.  The
 * inverse of rangeSynToSrc(). */
{
if (range->strand == '-')
    {
    *retStart = range->start + (range->qEnd - qEnd);
    *retEnd = range->start + (range->qEnd - qStart);
    }
else
    {
    *retStart = range->start + (qStart - range->qStart);
    *retEnd = *retStart + (qEnd - qStart);
    }
}

static int baseRangeCmp(const void *va, const void *vb)
/* Sort baseRanges into reference order. */
{
const struct baseRange *a = *((struct baseRange **)va);
const struct baseRange *b = *((struct baseRange **)vb);
return a->start - b->start;
}

struct baseRange *loadBaseRangesFromChain(char *chainFile, char *chrom, int start, int end)
/* Return the source-coordinate ranges overlapping [start,end), read off the quickLift
 * chain that maps this assembly back to its source.  Used to drive the main base position
 * ruler in source coordinates.
 *
 * The chain already holds this mapping (one aligned block per region of the synthetic
 * assembly), so a synthetic assembly needs no second file to number its ruler.  It also
 * carries something a bigBaseView file never did: the source sequence NAME, off the chain
 * header, which is what lets the position box name a real chromosome. */
{
long startMs = measureTiming ? clock1000() : 0;
char *linkFileName = bigChainGetLinkFile(chainFile);
struct chain *chain, *chainList = chainLoadIdRangeHub(NULL, chainFile, linkFileName,
                                                      chrom, start, end, -1);
struct baseRange *rangeList = NULL;

for (chain = chainList; chain != NULL; chain = chain->next)
    {
    struct cBlock *cb;
    for (cb = chain->blockList; cb != NULL; cb = cb->next)
        {
        struct baseRange *range;
        AllocVar(range);
        range->chrom = cloneString(chain->qName);
        range->start = cb->tStart;
        range->end = cb->tEnd;
        // a '-' strand chain keeps its query coordinates in the reversed space
        if (chain->qStrand == '-')
            {
            range->qStart = chain->qSize - cb->qEnd;
            range->qEnd = chain->qSize - cb->qStart;
            }
        else
            {
            range->qStart = cb->qStart;
            range->qEnd = cb->qEnd;
            }
        range->strand = (chain->qStrand == '-') ? '-' : '+';
        slAddHead(&rangeList, range);
        }
    }
// blocks ascend within a chain, but several chains can cover one window
slSort(&rangeList, baseRangeCmp);
chainFreeList(&chainList);
/* Every read of the chain in this file comes through here, so one line each says
 * how many there were and how much of the chain each one walked.  refs #37788 */
if (measureTiming)
    uglyTime("multiRegion: read %d chain blocks over %s:%d-%d in %ld millis",
             slCount(rangeList), chrom, start+1, end, clock1000() - startMs);
return rangeList;
}

void drawSourceRangeSeparators(struct hvGfx *hvg, char *chainFile, char *chrom,
                               int start, int end, int insideX, int width, int height,
                               boolean altShade)
/* Draw a light red vertical line at each boundary between two source ranges, the same
 * separator multi-region view puts between its windows.  With altShade, shade every
 * other region light blue instead, the way multi-region view does when the user asks for
 * alternating region highlights (emAltHighlight).
 *
 * A synthetic assembly is browsed as an ordinary assembly, so hgTracks is not in virtMode
 * and would otherwise lay its evenly spaced blue guidelines over the image.  Those say
 * nothing about the thing that matters here, which is where one source region ends and the
 * next begins.  The boundaries come off the same chain that numbers the ruler. */
{
double scale = scaleForWindow(width, start, end);
if (altShade)
    {
    /* Multi-region view shades a region by the parity of its place in the whole region
     * list, so the shading does not change as the window moves.  Read the chain from the
     * start of the sequence to count the regions in front of the window.  This option
     * is off by default, so only a user who asked for it pays for the longer read. */
    struct baseRange *range, *ranges = loadBaseRangesFromChain(chainFile, chrom, 0, end);
    Color lightBlue = hvGfxFindRgb(hvg, &multiRegionAltColor);
    int i = 0;
    for (range = ranges; range != NULL; range = range->next, ++i)
        {
        int s = max((int)range->start, start);
        int e = min((int)range->end, end);
        if ((i % 2 == 0) || (s >= e))
            continue;
        int x1 = insideX + round((double)(s - start) * scale);
        int x2 = insideX + round((double)(e - start) * scale);
        hvGfxBox(hvg, x1, 0, x2 - x1, height, lightBlue);
        }
    return;
    }
struct baseRange *range, *ranges = loadBaseRangesFromChain(chainFile, chrom, start, end);
Color lightRed = hvGfxFindRgb(hvg, &vertWindowSeparatorColor);
int lastX = -1;

for (range = ranges; range != NULL; range = range->next)
    {
    /* Each range begins a new region.  Skip a boundary that lands on the left edge, the
     * way virtMode skips its first window, and skip a repeat when two boundaries round to
     * the same pixel. */
    int x = insideX + round((double)((int)range->start - start) * scale);
    if ((x > insideX) && (x != lastX))
        hvGfxBox(hvg, x, 0, 1, height, lightRed);
    lastX = x;
    }
}

void mapSourceRangePositions(struct imgTrack *imgTrack, char *chainFile, char *chrom,
                             int start, int end, int insideX, int width, int y1, int y2)
/* Put a map box over the ruler for each source range in the window, whose mouseover is
 * the source position of the part on screen.  Multi-region view does this for each of its
 * windows;  a space has one window, so it takes the regions off the chain instead. */
{
struct baseRange *range, *ranges = loadBaseRangesFromChain(chainFile, chrom, start, end);
double scale = scaleForWindow(width, start, end);

for (range = ranges; range != NULL; range = range->next)
    {
    int s = max((int)range->start, start);
    int e = min((int)range->end, end);
    if (s >= e)
        continue;
    unsigned qs, qe;
    rangeSynToSrc(range, s, e, &qs, &qe);
    /* A hub's sequence names can be as long as HDB_MAX_CHROM_STRING. */
    char position[HDB_MAX_CHROM_STRING + 32];
    safef(position, sizeof position, "%s:%u-%u", range->chrom, qs+1, qe);
    int x1 = insideX + round((double)(s - start) * scale);
    int x2 = insideX + round((double)(e - start) * scale);
    if (revCmplDisp)
        {
        int flip = tl.picWidth - x2;
        x2 = tl.picWidth - x1;
        x1 = flip;
        }
    imgTrackAddMapItem(imgTrack, "#", position, x1, y1, x2, y2, RULER_TRACK_NAME, NULL);
    }
}

void loadBaseView(struct track *tg)
{
struct lm *lm = lmInit(0);
struct bbiFile *bbi =  fetchBbiForTrack(tg);
//struct asObject *as = bigBedAsOrDefault(bbi);
struct bigBedInterval *bb, *bbList =  bigBedIntervalQuery(bbi, chromName, winStart, winEnd, 0, lm);
char *bedRow[bbi->fieldCount];
char startBuf[16], endBuf[16];
struct baseRange *rangeList = NULL;

for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));

    struct baseRange *range;
    AllocVar(range);
    range->chrom = chromName;
    range->start = atoi(bedRow[1]);
    range->end = atoi(bedRow[2]);
    range->qStart = atoi(bedRow[3]);
    range->qEnd = atoi(bedRow[4]);

    slAddHead(&rangeList, range);
    }
slReverse(&rangeList);
tg->items = rangeList;
}

char *sourceChromForWindow(char *chainFile, char *chrom, int start, int end)
/* The source sequence this window maps back to, or NULL when it spans more than one of
 * them.  The ruler numbers in source coordinates, so the label beside it should name the
 * sequence those numbers belong to; naming the synthetic assembly there is wrong. */
{
struct baseRange *range, *ranges = loadBaseRangesFromChain(chainFile, chrom, start, end);
char *name = NULL;

for (range = ranges; range != NULL; range = range->next)
    {
    if (name == NULL)
        name = range->chrom;
    else if (!sameOk(name, range->chrom))
        return NULL;
    }
return name;
}

static struct baseRange *wholeSequenceRanges(char *chainFile, char *chrom)
/* Every block of one synthetic sequence, loaded once and kept.
 *
 * The caller below needs the whole sequence rather than a window, and it is asked
 * once for the window and then once per highlight.  Each load walks the chain and
 * allocates a struct and a name per block (tens of thousands of them on a
 * chromosome's exon list), so reloading per call cost about 1.8 MB and 30 to 54 ms
 * EACH, and the number of highlights comes from the cart, so nothing bounded it.
 *
 * Per thread, for the same reason multiRegionSpace.c's file caches are:  a bbiFile
 * has one read position.  In practice only the main thread asks.  refs #37788 */
{
static __thread struct hash *rangeCache = NULL;
if (rangeCache == NULL)
    rangeCache = hashNew(0);
char key[PATH_MAX + 256];
safef(key, sizeof key, "%s\t%s", chainFile, chrom);
struct hashEl *hel = hashLookup(rangeCache, key);
if (hel != NULL)
    return hel->val;
struct baseRange *ranges = loadBaseRangesFromChain(chainFile, chrom, 0,
                                                   multiRegionSpaceChromSize(chainFile, chrom));
hashAdd(rangeCache, key, ranges);
return ranges;
}

static struct baseRange **wholeSequenceRangeArray(char *chainFile, char *chrom, int *retCount)
/* The blocks of wholeSequenceRanges() as an array in synthetic order, so a position can
 * be found by binary search.  A page asks this once per item it links to hgc, and exon
 * view has tens of thousands of blocks, so walking the list each time is too slow.
 * Kept per thread for the reason wholeSequenceRanges() gives. */
{
static __thread char *cachedKey = NULL;
static __thread struct baseRange **cachedArray = NULL;
static __thread int cachedCount = 0;
char key[PATH_MAX + 256];
safef(key, sizeof key, "%s\t%s", chainFile, chrom);
if (cachedKey == NULL || !sameString(cachedKey, key))
    {
    struct baseRange *range, *ranges = wholeSequenceRanges(chainFile, chrom);
    int count = slCount(ranges), i = 0;
    struct baseRange **array = NULL;
    if (count > 0)	/* a chrom not in the space has no ranges, and needMem(0) aborts */
        AllocArray(array, count);
    for (range = ranges; range != NULL; range = range->next)
        array[i++] = range;
    freeMem(cachedKey);
    freeMem(cachedArray);
    cachedKey = cloneString(key);
    cachedArray = array;
    cachedCount = count;
    }
*retCount = cachedCount;
return cachedArray;
}

boolean syntheticToSourceRange(char *chainFile, char *chrom, int start, int end,
                               char **retChrom, int *retStart, int *retEnd)
/* Map a synthetic range back to the source.  See hgTracks.h. */
{
int count;
struct baseRange **array = wholeSequenceRangeArray(chainFile, chrom, &count);

/* The first block that ends after start. */
int lo = 0, hi = count;
while (lo < hi)
    {
    int mid = (lo + hi) / 2;
    if ((int)array[mid]->end <= start)
        lo = mid + 1;
    else
        hi = mid;
    }

if (start == end)
    {
    if (lo == count || (int)array[lo]->start > start)
        return FALSE;
    *retChrom = array[lo]->chrom;
    if (array[lo]->strand == '-')
        *retStart = *retEnd = array[lo]->qEnd - (start - (int)array[lo]->start);
    else
        *retStart = *retEnd = array[lo]->qStart + (start - (int)array[lo]->start);
    return TRUE;
    }

char *srcChrom = NULL;
unsigned qStart = 0, qEnd = 0;
int i;
for (i = lo; i < count && (int)array[i]->start < end; i++)
    {
    struct baseRange *range = array[i];
    if (srcChrom == NULL)
        srcChrom = range->chrom;
    else if (!sameString(srcChrom, range->chrom))
        return FALSE;
    /* Clip to the range asked for. */
    unsigned s, e;
    rangeSynToSrc(range, max(start, (int)range->start), min(end, (int)range->end), &s, &e);
    if (i == lo)
        {
        qStart = s;
        qEnd = e;
        }
    else
        {
        qStart = min(qStart, s);
        qEnd = max(qEnd, e);
        }
    }
if (srcChrom == NULL)
    return FALSE;
*retChrom = srcChrom;
*retStart = qStart;
*retEnd = qEnd;
return TRUE;
}

void multiRegionSpaceLinkPosition(char **pChrom, int *pWinStart, int *pWinEnd,
                                  int *pStart, int *pEnd)
/* In a multi-region space, rewrite the chrom, window and item coordinates a details link
 * carries into source coordinates.  See hgTracks.h. */
{
char *chainFile = multiRegionSpaceForDb(database);
if (chainFile == NULL)
    return;
char *srcChrom;
int srcStart, srcEnd;
if (!syntheticToSourceRange(chainFile, *pChrom, *pStart, *pEnd, &srcChrom, &srcStart, &srcEnd))
    return;
char *winChrom;
int winSrcStart, winSrcEnd;
if (!syntheticToSourceRange(chainFile, *pChrom, *pWinStart, *pWinEnd,
                            &winChrom, &winSrcStart, &winSrcEnd)
    || !sameString(winChrom, srcChrom))
    {
    /* The window reaches past this item's source sequence, so it has no one source
     * span to report.  The item's own range is the part of it that is certainly on
     * the sequence the link names. */
    winSrcStart = srcStart;
    winSrcEnd = srcEnd;
    }
*pChrom = srcChrom;
*pWinStart = winSrcStart;
*pWinEnd = winSrcEnd;
*pStart = srcStart;
*pEnd = srcEnd;
}

boolean sourceToSyntheticRange(char *chainFile, char *srcChrom, int srcStart, int srcEnd,
                               char **retChrom, int *retStart, int *retEnd)
/* Map a source-coordinate range onto this synthetic assembly.  Returns FALSE when
 * nothing in the synthetic assembly covers it.
 *
 * The position box shows source coordinates (sourceSpanPosition below), so a
 * position typed into it is a source position and has to come back through the
 * chain.  Without this it would be read as a synthetic coordinate, which is a
 * legal position on the synthetic sequence, so it would land somewhere arbitrary
 * and say nothing about it.
 *
 * The synthetic sequence to search is the space's one sequence, which is named
 * after the source sequence only when every region came from it (see below), so
 * the qChrom of each range is what is checked. */
{
if (chainFile == NULL || srcChrom == NULL)
    return FALSE;
/* Ask the SPACE, not the assembly.
 *
 * hgOfficialChromName() was the wrong question:  it falls through to the chromAlias
 * file, whose ucsc index answers "chr5" for chr5 on any hg38, space or no space.
 * hChromSize() then reached mustGetChromInfo() and errAborted the request.  A typed
 * position or a stored highlight can name any chromosome, and a space holds only the
 * sequences its own regions came from, so this is the common case rather than a
 * strange one.
 *
 * The synthetic sequence is NOT always named after the source one:  a space built
 * from regions on several sequences is called "multi".  So load the space's one
 * sequence and let the blocks say which source sequence each came from, rather than
 * assuming the two names match. */
char *synChrom = multiRegionSpaceSoleChrom(chainFile);
if (synChrom == NULL)
    return FALSE;
struct baseRange *range;
struct baseRange *ranges = wholeSequenceRanges(chainFile, synChrom);
boolean gotOne = FALSE;
unsigned tStart = 0, tEnd = 0;
for (range = ranges; range != NULL; range = range->next)
    {
    if (!sameOk(range->chrom, srcChrom))
        continue;
    if (range->qEnd <= srcStart || range->qStart >= srcEnd)
        continue;
    /* Clip to the part of this block the caller actually asked for.  A block is
     * ungapped and the same length on both sides, so a source offset within it
     * is the same offset on the synthetic side.  Taking the whole block instead
     * would snap the view outward to a whole region: ask for chr1:1,000,000 and
     * you would land at 999,686 because that is where the containing exon
     * region starts, and a zoom would never stay where it was put. */
    unsigned lo, hi;
    rangeSrcToSyn(range, max((unsigned)srcStart, range->qStart),
                  min((unsigned)srcEnd, range->qEnd), &lo, &hi);
    if (!gotOne)
        {
        tStart = lo;
        tEnd = hi;
        gotOne = TRUE;
        }
    else
        {
        if (lo < tStart)
            tStart = lo;
        if (hi > tEnd)
            tEnd = hi;
        }
    }
if (!gotOne)
    return FALSE;
*retChrom = cloneString(synChrom);
*retStart = tStart;
*retEnd = tEnd;
return TRUE;
}

boolean sourceToNearestSyntheticRange(char *chainFile, char *srcChrom, int srcStart, int srcEnd,
                                      int *retStart, int *retEnd)
/* For a source range that nothing in the space covers:  a window of the same width on
 * the space, at the block from the same source sequence that comes nearest to it.  The
 * window starts at that block's edge and runs into the block, the rule the virtual
 * chromosome used (virtChromSearchForPosition() with findNearest).  FALSE when no block
 * came from that sequence. */
{
if (chainFile == NULL || srcChrom == NULL)
    return FALSE;
char *synChrom = multiRegionSpaceSoleChrom(chainFile);
if (synChrom == NULL)
    return FALSE;
struct baseRange *range, *nearest = NULL;
unsigned nearestBase = 0, nearestDist = 0;
for (range = wholeSequenceRanges(chainFile, synChrom); range != NULL; range = range->next)
    {
    if (!sameOk(range->chrom, srcChrom))
        continue;
    unsigned dist, base;
    if (range->qStart >= (unsigned)srcEnd)
        {
        dist = range->qStart - srcEnd;
        base = range->qStart;
        }
    else if (range->qEnd <= (unsigned)srcStart)
        {
        dist = srcStart - range->qEnd;
        base = range->qEnd - 1;
        }
    else
        continue;   /* overlaps:  sourceToSyntheticRange() would have found it */
    if (nearest == NULL || dist < nearestDist)
        {
        nearest = range;
        nearestDist = dist;
        nearestBase = base;
        }
    }
if (nearest == NULL)
    return FALSE;
unsigned lo, hi;
rangeSrcToSyn(nearest, nearestBase, nearestBase + 1, &lo, &hi);
int width = srcEnd - srcStart;
int synSize = hChromSize(database, synChrom);
int start, end;
if (lo == nearest->start)
    {
    /* the block's first synthetic base:  run forward into it */
    start = lo;
    end = min(synSize, start + width);
    }
else
    {
    end = hi;
    start = max(0, end - width);
    }
*retStart = start;
*retEnd = end;
return TRUE;
}

static char *sourceSpan(char *chainFile, char *chrom, int start, int end, boolean firstSeqOnly)
/* The source-coordinate span of this window, "chrom:start+1-end".  A window that maps
 * back to more than one source sequence gives NULL, or with firstSeqOnly the span on
 * the first of them.  NULL when it maps back to none. */
{
struct baseRange *range, *ranges = loadBaseRangesFromChain(chainFile, chrom, start, end);
if (ranges == NULL)
    return NULL;
char *name = ranges->chrom;
boolean gotOne = FALSE;
unsigned qStart = 0, qEnd = 0;
for (range = ranges; range != NULL; range = range->next)
    {
    if (!sameOk(name, range->chrom))
        {
        if (firstSeqOnly)
            continue;
        return NULL;
        }
    /* Clip to the window.  chainLoadIdRangeHub() returns whole blocks that
     * OVERLAP [start,end), so using them as they come would report the whole
     * region the window sits inside rather than the window itself. */
    int s = max(start, (int)range->start);
    int e = min(end, (int)range->end);
    if (e <= s)
        continue;
    unsigned lo, hi;
    rangeSynToSrc(range, s, e, &lo, &hi);
    if (!gotOne)
        {
        qStart = lo;
        qEnd = hi;
        gotOne = TRUE;
        }
    else
        {
        if (lo < qStart)
            qStart = lo;
        if (hi > qEnd)
            qEnd = hi;
        }
    }
if (!gotOne)
    return NULL;
char buf[HDB_MAX_CHROM_STRING + 32];   /* a hub's sequence names can be this long */
safef(buf, sizeof buf, "%s:%u-%u", name, qStart + 1, qEnd);
return cloneString(buf);
}

char *sourceSpanPosition(char *chainFile, char *chrom, int start, int end)
/* The source-coordinate span of this window, "chrom:start+1-end", or NULL when the
 * window maps back to more than one source sequence or to none.
 *
 * This is the synthetic-assembly counterpart of windowsSpanPosition()
 * (hgTracks.c:2976), which is what multi-region view puts in the position box: the
 * span from the first window's start to the last window's end, in source
 * coordinates.  The base count printed beside it stays the number of bases actually
 * drawn, exactly as multi-region view prints virtWinEnd - virtWinStart. */
{
return sourceSpan(chainFile, chrom, start, end, FALSE);
}

char *sourceFirstSeqPosition(char *chainFile, char *chrom, int start, int end)
/* The source-coordinate span of this window on the first source sequence it shows,
 * or NULL when it shows none.  The synthetic-assembly counterpart of
 * nonVirtPositionFromWindows(), which is where the virtual-chromosome path goes back
 * to when the user leaves multi-region view. */
{
return sourceSpan(chainFile, chrom, start, end, TRUE);
}

void drawSourceCoordRuler(struct hvGfx *hvg, struct baseRange *rangeList,
                          int seqStart, int seqEnd, int yOff, int rulerHeight, MgFont *font)
/* Number the ruler in source coordinates.
 *
 * Each range is numbered on its own, which is what multi-region view does: it makes a
 * window per region and lets the ordinary ruler draw inside each one.  A synthetic
 * assembly has no windows (that is the point of it), so the ranges take their place,
 * but the numbering is the same call the ordinary ruler makes, so it looks the same.
 *
 * Two ranges are merged only when they are contiguous on BOTH sides.  A synthetic
 * assembly is contiguous on its own side everywhere, so merging on that alone would run
 * one set of numbers straight across a jump in the source. */
{
struct baseRange *range, *mergedList = NULL, *merged = NULL;
double scale = (double)insideWidth / (seqEnd - seqStart);

for (range = rangeList; range != NULL; range = range->next)
    {
    if ((merged != NULL) && (range->start == merged->end) && sameOk(range->chrom, merged->chrom)
        && (range->strand == merged->strand))
        {
        /* contiguous on the source too:  forward, the next range starts where this one
         * ended;  reversed, it ends where this one started */
        if (range->strand != '-' && range->qStart == merged->qEnd)
            {
            merged->end = range->end;
            merged->qEnd = range->qEnd;
            continue;
            }
        if (range->strand == '-' && range->qEnd == merged->qStart)
            {
            merged->end = range->end;
            merged->qStart = range->qStart;
            continue;
            }
        }
    AllocVar(merged);
    merged->chrom = range->chrom;
    merged->start = range->start;
    merged->end = range->end;
    merged->qStart = range->qStart;
    merged->qEnd = range->qEnd;
    merged->strand = range->strand;
    slAddHead(&mergedList, merged);
    }
slReverse(&mergedList);

for (merged = mergedList; merged != NULL; merged = merged->next)
    {
    /* clip to the window, carrying the source start along with the clip */
    int rangeStart = max((int)merged->start, winStart);
    int rangeEnd = min((int)merged->end, winEnd);
    if (rangeStart >= rangeEnd)
        continue;
    unsigned qStart, qEnd;
    rangeSynToSrc(merged, rangeStart, rangeEnd, &qStart, &qEnd);
    int x = insideX + round((rangeStart - winStart) * scale);
    int w = round((rangeEnd - rangeStart) * scale);

    /* hvGfxDrawRulerBumpText computes niceNumTicks as width/35 and then divides by it in
     * figureTickSpan, so a narrower segment divides by zero and takes hgTracks with it.
     * A segment this narrow has no room for a number anyway; the region separator already
     * marks where it is, which is what multi-region view shows for a narrow window. */
    #define MIN_RULER_SEGMENT 35
    if (w < MIN_RULER_SEGMENT)
        continue;
    if (merged->strand == '-')
        {
        /* A reversed region numbers downward, left to right.  Draw it the way a
         * reverse-complemented display draws its ruler:  flip the image's sense for this
         * one segment, at the mirrored x, so the lowest number lands on the right. */
        hvg->rc = !hvg->rc;
        hvGfxDrawRulerBumpText(hvg, hvg->width - x - w, yOff, rulerHeight, w, MG_BLACK,
                               font, qStart, rangeEnd - rangeStart, 0, 1);
        hvg->rc = !hvg->rc;
        }
    else
        hvGfxDrawRulerBumpText(hvg, x, yOff, rulerHeight, w, MG_BLACK, font,
                               qStart, rangeEnd - rangeStart, 0, 1);
    }
}

void drawBaseView(struct track *tg, int seqStart, int seqEnd, struct hvGfx *hvg,
                 int xOff, int yOff, int width, MgFont *font, Color color,
                 enum trackVisibility vis)
{
unsigned rulerHeight = mgFontLineHeight(font);
struct baseRange *rangeList = tg->items, *range;
unsigned prevBase = seqStart;
double scale = (double) insideWidth / (seqEnd - seqStart) ;

struct baseRange *newRangeList = NULL;

unsigned start = rangeList->start;
unsigned lastEnd = rangeList->end;
unsigned qStart = rangeList->qStart;
unsigned lastQEnd = rangeList->qEnd;
for(range = rangeList->next; range; range= range->next)
    {
    if (range->start > lastEnd)
        {
        unsigned tmp = range->start - lastEnd;
        if (tmp * scale > 20.0) // flush
            {
            struct baseRange *newRange;
            AllocVar(newRange);
            newRange->start = start;
            newRange->end = lastEnd;
            newRange->qStart = qStart;
            newRange->qEnd = lastQEnd;
            slAddHead(&newRangeList, newRange);

            start = range->start;
            qStart = range->qStart;
            }
        lastEnd = range->end;
        lastQEnd = range->qEnd;
        }
    }

struct baseRange *newRange;
AllocVar(newRange);
newRange->start = start;
newRange->end = lastEnd;
newRange->qStart = qStart;
newRange->qEnd = lastQEnd;
slAddHead(&newRangeList, newRange);

slReverse(&newRangeList);

for(range = newRangeList; range; range= range->next)
    {
    if (range->start > winEnd)
        break;

    unsigned rangeStart = (range->start < winStart) ? winStart : range->start;
    if (range->start < winStart)
        {
        unsigned offset = winStart - range->start;
        range->qStart += offset;
        }
    unsigned rangeEnd = (range->end > winEnd) ? winEnd : range->end;
    unsigned rangeWidth = rangeEnd - rangeStart;
    unsigned rangeScreenWidth = scale * rangeWidth;

    if (prevBase < rangeStart)
        {
        int x = (prevBase - winStart) * scale + insideX;
        int width = (rangeStart - prevBase) * scale;
        //hvGfxBox(hvg, x, yOff, width, rulerHeight, 0xffFFBF00);
        hvGfxBox(hvg, x, yOff, width, rulerHeight, 0xff00BFFF);
        }

    unsigned rangeQStart = range->qStart;
    if (rangeScreenWidth > 50)
    hvGfxDrawRulerBumpText(hvg, insideX + (rangeStart - winStart) * scale, yOff, rulerHeight, rangeScreenWidth, MG_BLACK,
                               font, rangeQStart, rangeWidth, 0, 1);
    prevBase = rangeEnd;
    }
}

static int baseViewHeight(struct track *tg, enum trackVisibility vis)
{
MgFont *font = tl.font;
return mgFontLineHeight(font);
}

void bigBaseViewMethods(struct track *track, struct trackDb *tdb,
                                int wordCount, char *words[])
/* Fill in methods for (simple) bed tracks. */
{
track->loadItems = loadBaseView;
track->drawItems = drawBaseView;
track->totalHeight = baseViewHeight; 
}
