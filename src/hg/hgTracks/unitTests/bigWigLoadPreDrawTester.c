/* bigWigLoadPreDrawTester - a quickLifted bigWig over a window where its chain has no block.
 *
 * chainLoadIdRangeHub returns every chain whose span overlaps the window, but only the links
 * that fall inside it, so a chain can reach across the window with no aligned block in it.
 * chainSubsetOnT then has nothing to subset and returns NULL, and before #38236
 * bigWigLoadPreDraw read through that NULL and hgTracks died with SIGSEGV: the narrow views of
 * a quickLift from hg38 to hs1 came up blank.
 *
 * input/quickLiftGap holds one chain over chrRef:1000-9000 with blocks only at 1000-2000 and
 * 8000-9000, and a bigWig on chrSrc with a value in each block and one in the gap
 * (makeFixtures.sh says how they were made).  This loads the predraw buffer over a window with
 * a block in it and over one inside the gap, and prints each pixel.
 *
 * refs #38236 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "bigWig.h"
#include "hgTracks.h"
#include "wigCommon.h"

/* Public in bigWigTrack.c, but in no header. */
struct preDrawContainer *bigWigLoadPreDraw(struct track *tg, int seqStart, int seqEnd, int width);

static void showWindow(char *what, char *dir, int start, int end, int width)
/* Load the lifted bigWig over chrRef:start-end into width pixels and print them. */
{
char chainFile[PATH_LEN], bwFile[PATH_LEN];
safef(chainFile, sizeof chainFile, "%s/gap.bb", dir);
safef(bwFile, sizeof bwFile, "%s/src.bw", dir);
struct track *tg;
AllocVar(tg);
AllocVar(tg->tdb);
tg->tdb->track = "regress38236";
tg->tdb->settingsHash = hashNew(0);
hashAdd(tg->tdb->settingsHash, "quickLiftUrl", chainFile);
tg->bbiFile = bigWigFileOpen(bwFile);
chromName = "chrRef";
winStart = start;
winEnd = end;
struct preDrawContainer *pre = bigWigLoadPreDraw(tg, start, end, width);
printf("%s, chrRef:%d-%d over %d pixels\n", what, start, end, width);
if (tg->networkErrMsg != NULL)
    printf("  error: %s\n", tg->networkErrMsg);
struct preDrawElement *pe = &pre->preDraw[pre->preDrawZero];
int i;
for (i = 0;  i < width;  ++i, ++pe)
    printf("  %2d  count %g  max %g\n", i, (double)pe->count, pe->max);
}

int main(int argc, char *argv[])
{
if (argc != 2)
    errAbort("usage: bigWigLoadPreDrawTester input/quickLiftGap");
showWindow("half in the first block", argv[1], 1500, 2500, 10);
showWindow("inside the gap, no block", argv[1], 4000, 5000, 10);
showWindow("the second block", argv[1], 7500, 9500, 10);
return 0;
}
