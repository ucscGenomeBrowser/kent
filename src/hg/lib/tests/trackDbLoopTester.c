/* trackDbLoopTester - check that a loop in trackDb parent settings is refused.
 *
 * Each input file is a trackDb.ra.  The tester links its tracks into the forest the browser
 * draws from and prints that forest, or the error the linking stopped with.
 *
 * A trackDb in which two tracks name each other as parent makes a loop.  Before #38471 the
 * linking checked only that a track does not name itself, so a two-track loop was linked
 * without a message: the tracks in it, and any subtrack hanging off them, were missing from
 * the forest.  Now the link that would close the loop stops with an error that names the
 * track and its parent.
 *
 * refs #38471 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "errCatch.h"
#include "trackDb.h"

static int cmpTrackName(const void *va, const void *vb)
/* Compare two tracks by name.  The label fields are not filled in until after linking. */
{
const struct trackDb *a = *((struct trackDb **)va);
const struct trackDb *b = *((struct trackDb **)vb);
return strcmp(a->track, b->track);
}

static void printTree(struct trackDb *tdb, int depth)
/* Print a track and its subtracks, one per line, indented by depth.  The depth cap keeps
 * a loop that slipped through from printing forever. */
{
if (depth > 10)
    {
    printf("%*s... deeper than 10 levels\n", depth * 2, "");
    return;
    }
printf("%*s%s\n", depth * 2, "", tdb->track);
slSort(&tdb->subtracks, cmpTrackName);
struct trackDb *sub;
for (sub = tdb->subtracks; sub != NULL; sub = sub->next)
    printTree(sub, depth + 1);
}

static void testOne(char *raFile)
/* Link the tracks in raFile and print the forest or the error. */
{
printf("%s\n", raFile);
struct trackDb *tdbList = trackDbFromRa(raFile, NULL, NULL);
struct trackDb *forest = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    forest = trackDbLinkUpGenerations(tdbList);
errCatchEnd(errCatch);
if (errCatch->gotError)
    printf("  error: %s", errCatch->message->string);
else
    {
    slSort(&forest, cmpTrackName);
    struct trackDb *tdb;
    for (tdb = forest; tdb != NULL; tdb = tdb->next)
        printTree(tdb, 1);
    }
errCatchFree(&errCatch);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 2)
    errAbort("usage: trackDbLoopTester file.ra ...");
int i;
for (i = 1; i < argc; i++)
    testOne(argv[i]);
return 0;
}
