/* quickLiftMafSummaryTester - what a quickLifted maf track reads over a wide window.
 *
 * A lifted maf track never used its summary table, so a wide window read and lifted every
 * alignment block: 50 Mb of hg19 multiz100way lifted to hg38 ran hgTracks out of memory.  Two
 * things were in the way.  The quickLift hub rewrites every file setting into a path under the
 * hub, the summary among them, so the summary table was never found; quickLiftSummaryTable now
 * takes the last part of the setting.  And every source range was read with 100 kb of padding
 * on each side, which a range query does not need; quickLiftSourceRangesExact reads none.
 * Both are behind browser.quickLiftMafSummary.
 *
 * The first half asks inSummaryMode (hgTracks/mafTrack.h) about a lifted wigMaf whose summary
 * is a hub path, with the gate set on and off in the cart.  It reads hg19 to see that
 * multiz100waySummary is there.  The second half is a perf check that counts work: the source
 * bases asked for over one window of the small chain file in input/quickLift, with and without
 * padding.
 *
 * refs #38513 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "cart.h"
#include "trackDb.h"
#include "hui.h"
#include "hvGfx.h"
#include "quickLift.h"

struct track;
#include "mafTrack.h"

static struct trackDb *liftedMaf(char *type, char *summary)
/* A lifted maf track from hg19, as the quickLift hub writes it. */
{
struct trackDb *tdb;
AllocVar(tdb);
tdb->track = "multiz100way";
tdb->type = type;
tdb->settingsHash = hashNew(0);
hashAdd(tdb->settingsHash, "quickLiftUrl", "/gbdb/hg38/quickLift/hg19ToHg38.bb");
hashAdd(tdb->settingsHash, "quickLiftDb", "hg19");
if (summary != NULL)
    hashAdd(tdb->settingsHash, "summary", summary);
return tdb;
}

static void showSummaryMode(char *what, struct trackDb *tdb, int winSize)
/* Print whether the track reads its summary, with the gate on and off. */
{
struct cart *cart = cartOfNothing();
cartSetString(cart, "browser.quickLiftMafSummary", "on");
boolean on = inSummaryMode(cart, tdb, winSize);
cartSetString(cart, "browser.quickLiftMafSummary", "off");
boolean off = inSummaryMode(cart, tdb, winSize);
printf("  %-52s gate on: %-8s gate off: %s\n", what, on ? "summary" : "blocks",
       off ? "summary" : "blocks");
}

static long sourceBases(struct quickLiftRange *rangeList)
/* Total bases in a list of source ranges. */
{
long total = 0;
struct quickLiftRange *range;
for (range = rangeList; range != NULL; range = range->next)
    total += range->end - range->start;
return total;
}

static void showSourceBases(char *liftFile, int start, int end)
/* Print the source bases asked for over chrRef:start-end, with and without padding. */
{
printf("\nsource bases asked for over chrRef:%d-%d\n", start, end);
struct hash *chainHash = newHash(4);
struct quickLiftRange *padded = quickLiftSourceRanges(liftFile, "chrRef", start, end, chainHash);
printf("  %-52s %ld\n", "quickLiftSourceRanges, padded", sourceBases(padded));
chainHash = newHash(4);
struct quickLiftRange *exact = quickLiftSourceRangesExact(liftFile, "chrRef", start, end,
                                                          chainHash);
printf("  %-52s %ld\n", "quickLiftSourceRangesExact", sourceBases(exact));
}

int main(int argc, char *argv[])
{
if (argc != 2)
    errAbort("usage: quickLiftMafSummaryTester lift.bb");
char *hubSummary = "/gbdb/hg38/quickLift/hub_regress/multiz100waySummary";

printf("inSummaryMode for a lifted track at 50 Mb\n");
showSummaryMode("wigMaf, summary as the hub writes it",
                liftedMaf("wigMaf 0.0 1.0", hubSummary), 50000000);
showSummaryMode("wigMaf, summary as a plain table name",
                liftedMaf("wigMaf 0.0 1.0", "multiz100waySummary"), 50000000);
showSummaryMode("wigMaf, summary table missing in hg19",
                liftedMaf("wigMaf 0.0 1.0", "/gbdb/hub/noSuchSummary38513"), 50000000);
showSummaryMode("bigMaf, which reads its blocks",
                liftedMaf("bigMaf", hubSummary), 50000000);
showSummaryMode("wigMaf with no summary setting", liftedMaf("wigMaf 0.0 1.0", NULL), 50000000);
printf("at 500 kb, under the summary window\n");
showSummaryMode("wigMaf, summary as the hub writes it",
                liftedMaf("wigMaf 0.0 1.0", hubSummary), 500000);

showSourceBases(argv[1], 1500, 2500);
showSourceBases(argv[1], 5200, 5300);
return 0;
}
