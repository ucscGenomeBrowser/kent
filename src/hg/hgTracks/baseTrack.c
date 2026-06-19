
#include "common.h"
#include "hgTracks.h"
#include "bigBed.h"

struct baseRange
{
struct baseRange *next;
char *chrom;
unsigned start, end;
unsigned qStart, qEnd;
};

static struct baseRange *baseRangesFromBbi(struct bbiFile *bbi, char *chrom, int start, int end)
/* Query a baseView bigBed (bed4+1: synthetic start/end -> source qStart/qEnd) and
 * return the overlapping ranges. */
{
struct lm *lm = lmInit(0);
struct bigBedInterval *bb, *bbList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
char *bedRow[bbi->fieldCount];
char startBuf[16], endBuf[16];
struct baseRange *rangeList = NULL;

for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, chrom, startBuf, endBuf, bedRow, ArraySize(bedRow));

    struct baseRange *range;
    AllocVar(range);
    range->chrom = cloneString(chrom);
    range->start = atoi(bedRow[1]);
    range->end = atoi(bedRow[2]);
    range->qStart = atoi(bedRow[3]);
    range->qEnd = atoi(bedRow[4]);

    slAddHead(&rangeList, range);
    }
slReverse(&rangeList);
lmCleanup(&lm);
return rangeList;
}

struct baseRange *loadBaseRangesFromUrl(char *bigDataUrl, char *chrom, int start, int end)
/* Open a baseView bigBed by url and return source-coordinate ranges overlapping
 * [start,end).  Used to drive the main base position ruler in source coords. */
{
struct bbiFile *bbi = bigBedFileOpen(bigDataUrl);
struct baseRange *ranges = baseRangesFromBbi(bbi, chrom, start, end);
bbiFileClose(&bbi);
return ranges;
}

void loadBaseView(struct track *tg)
{
struct bbiFile *bbi = fetchBbiForTrack(tg);
tg->items = baseRangesFromBbi(bbi, chromName, winStart, winEnd);
}

void drawSourceCoordRuler(struct hvGfx *hvg, struct baseRange *rangeList,
                          int seqStart, int seqEnd, int yOff, int rulerHeight, MgFont *font)
/* Draw a ruler that numbers in source coordinates.  rangeList maps positions in
 * [seqStart,seqEnd) (synthetic) to source coordinates (qStart/qEnd); a ruler segment
 * is drawn for each coalesced range and blue boxes fill the gaps between them. */
{
struct baseRange *range;
unsigned prevBase = seqStart;
double scale = (double) insideWidth / (seqEnd - seqStart) ;

if (rangeList == NULL)
    return;

/* coalesce adjacent ranges, flushing a new range whenever the gap on screen is
 * wide enough to be worth its own numbering */
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
        hvGfxBox(hvg, x, yOff, width, rulerHeight, 0xff00BFFF);
        }

    unsigned rangeQStart = range->qStart;
    if (rangeScreenWidth > 50)
    hvGfxDrawRulerBumpText(hvg, insideX + (rangeStart - winStart) * scale, yOff, rulerHeight, rangeScreenWidth, MG_BLACK,
                               font, rangeQStart, rangeWidth, 0, 1);
    prevBase = rangeEnd;
    }
}

void drawBaseView(struct track *tg, int seqStart, int seqEnd, struct hvGfx *hvg,
                 int xOff, int yOff, int width, MgFont *font, Color color,
                 enum trackVisibility vis)
{
unsigned rulerHeight = mgFontLineHeight(font);
drawSourceCoordRuler(hvg, tg->items, seqStart, seqEnd, yOff, rulerHeight, font);
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
