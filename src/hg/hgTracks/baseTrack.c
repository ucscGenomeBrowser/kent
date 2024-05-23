
#include "common.h"
#include "hgTracks.h"

struct baseRange
{
struct baseRange *next;
char *chrom;
unsigned start, end;
unsigned qStart, qEnd;
};

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

void drawBaseView(struct track *tg, int seqStart, int seqEnd, struct hvGfx *hvg,
                 int xOff, int yOff, int width, MgFont *font, Color color,
                 enum trackVisibility vis)
{
unsigned rulerHeight = mgFontLineHeight(font);
struct baseRange *rangeList = tg->items, *range;
unsigned prevBase = seqStart;
printf("seqStart %d seqEnd %d width %d\n", seqStart, seqEnd, width);
double scale = (double) insideWidth / (seqEnd - seqStart) ;

//unsigned prevEnd = range->end;
//struct range *next;
struct baseRange *newRangeList = NULL;

unsigned start = rangeList->start;
unsigned lastEnd = rangeList->end;
unsigned qStart = rangeList->qStart;
unsigned lastQEnd = rangeList->qEnd;
for(range = rangeList->next; range; range= range->next)
    {
    //next = range->next;

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
        //else
            {
            lastEnd = range->end;
            lastQEnd = range->qEnd;
            }
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

    //unsigned rangeStart = (range->start < winStart) ? winStart : range->start;
    unsigned rangeStart = (range->start < winStart) ? winStart : range->start;
    if (range->start < winStart)
        {
        unsigned offset = winStart - range->start;
        range->qStart += offset;
        }
    unsigned rangeEnd = (range->end > winEnd) ? winEnd : range->end;
    unsigned rangeWidth = rangeEnd - rangeStart;
    unsigned rangeScreenWidth = scale * rangeWidth;
    printf("start %d end %d width %d screenWidth %d scale %g\n", rangeStart, rangeEnd, rangeWidth, rangeScreenWidth, scale);

    if (prevBase < rangeStart)
        {
        printf("box start %d rangeStart %d\n", prevBase, rangeStart);
        //int x = (prevBase - winStart) * scale + insideX;
        //int width = (rangeStart - prevBase) * scale;
        //hvGfxBox(hvg, x, yOff, width, rulerHeight, 0xff333333);
        }

    unsigned rangeQStart = range->qStart;
    //startBase = rangeStart;
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
// hvGfxDrawRulerBumpText
}

void bigBaseViewMethods(struct track *track, struct trackDb *tdb,
                                int wordCount, char *words[])
/* Fill in methods for (simple) bed tracks. */
{
track->loadItems = loadBaseView;
track->drawItems = drawBaseView;
track->totalHeight = baseViewHeight; 
}
