/* itemCoverageTester - a coverage graph built from feature runs, not one counter per base.
 *
 * A track drawn as a coverage graph used to allocate one unsigned counter per base of the
 * window, add one to every base of every feature, and read the whole array back to make about
 * 1,200 pixels: 100 MB per track on a 25 Mb view.  Since #38253 coverage is a step function,
 * two events per feature, so the work and the memory follow the number of features, not the
 * width of the view.  The arithmetic at each pixel was kept exactly, so the picture is the same.
 *
 * The first part prints the per-pixel statistics itemCoverageToPixels makes for small windows,
 * sampled up and down, from a bed-style track and a linked-feature track.  Those lines must be
 * the same on the old code and the new.  The second part is the perf check, which counts
 * memory rather than seconds: three features over a 250 Mb window with the address space
 * limited to 512 MB, where the old per-base array alone was 1 GB.
 *
 * refs #38253 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "errCatch.h"
#include "hgTracks.h"
#include "wigCommon.h"
#include <sys/resource.h>

/* Not in a header; simpleTracks.c declares them the same way. */
extern int linkedFeaturesItemStart(struct track *tg, void *item);
extern int linkedFeaturesItemEnd(struct track *tg, void *item);

struct span
/* A feature for the bed-style track. */
    {
    struct span *next;
    int start, end;
    };

static int spanStart(struct track *tg, void *item)
/* itemStart for a span. */
{
return ((struct span *)item)->start;
}

static int spanEnd(struct track *tg, void *item)
/* itemEnd for a span. */
{
return ((struct span *)item)->end;
}

static struct track *spanTrack(int *starts, int *ends, int count)
/* A track whose items are plain spans. */
{
struct track *tg;
AllocVar(tg);
tg->itemStart = spanStart;
tg->itemEnd = spanEnd;
int i;
for (i = 0;  i < count;  ++i)
    {
    struct span *sp;
    AllocVar(sp);
    sp->start = starts[i];
    sp->end = ends[i];
    slAddTail(&tg->items, sp);
    }
return tg;
}

static struct track *linkedTrack()
/* A linked-feature track: one two-block feature and one zero-length block. */
{
struct track *tg;
AllocVar(tg);
tg->itemStart = linkedFeaturesItemStart;
tg->itemEnd = linkedFeaturesItemEnd;
struct linkedFeatures *lf;
AllocVar(lf);
struct simpleFeature *sf;
AllocVar(sf);
sf->start = 110;
sf->end = 130;
slAddTail(&lf->components, sf);
AllocVar(sf);
sf->start = 150;
sf->end = 175;
slAddTail(&lf->components, sf);
AllocVar(sf);
sf->start = 160;
sf->end = 160;
slAddTail(&lf->components, sf);
slAddTail(&tg->items, lf);
return tg;
}

static void showPixels(char *what, struct track *tg, int start, int end, int width)
/* Set the window, run itemCoverageToPixels, and print each pixel. */
{
winStart = start;
winEnd = end;
insideWidth = width;
struct preDrawContainer *pre = initPreDrawContainer(insideWidth);
itemCoverageToPixels(tg, pre);
printf("%s, %d-%d over %d pixels\n", what, start, end, width);
struct preDrawElement *pe = &pre->preDraw[pre->preDrawZero];
int i;
for (i = 0;  i < insideWidth;  ++i, ++pe)
    printf("  %2d  count %g  min %g  max %g  sum %g  sumSquares %g\n", i, (double)pe->count,
           pe->min, pe->max, pe->sumData, pe->sumSquares);
}

static void wideWindow()
/* Three features over 250 Mb, with no more than 512 MB of address space. */
{
int starts[] = {1000, 100000000, 200000000}, ends[] = {2000, 100500000, 249000000};
struct track *tg = spanTrack(starts, ends, ArraySize(starts));
struct rlimit limit = {512L * 1024 * 1024, 512L * 1024 * 1024};
if (setrlimit(RLIMIT_AS, &limit) != 0)
    errnoAbort("setrlimit");
winStart = 0;
winEnd = 250000000;
insideWidth = 250;
struct preDrawContainer *pre = initPreDrawContainer(insideWidth);
printf("three features over 250 Mb, 250 pixels, address space limited to 512 MB\n");
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    itemCoverageToPixels(tg, pre);
    struct preDrawElement *pe = &pre->preDraw[pre->preDrawZero];
    int i, covered = 0;
    for (i = 0;  i < insideWidth;  ++i)
        if (pe[i].max > 0)
            ++covered;
    printf("  drawn: %d of %d pixels have coverage\n", covered, insideWidth);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    printf("  aborted: %s\n", trimSpaces(errCatch->message->string));
errCatchFree(&errCatch);
}

int main(int argc, char *argv[])
{
int starts[] = {100, 105, 140, 90}, ends[] = {120, 160, 141, 200};
showPixels("spans, sampled down", spanTrack(starts, ends, 4), 95, 195, 10);
showPixels("spans, sampled up", spanTrack(starts, ends, 4), 100, 110, 20);
showPixels("linked features, sampled down", linkedTrack(), 100, 200, 10);
wideWindow();
return 0;
}
