/* parallelLoadVisTester - a composite child's visibility is settled before its loader starts.
 *
 * A composite child's visibility used to be written lazily, the first time anything asked for
 * it, by limitedVisFromComposite().  The parallel loader threads start, and then the main
 * thread runs compositeLoad() over every subtrack, which performs that write, while a worker
 * may already be reading the field.  So the same request could draw two different images
 * (#38254: clinvarSubLolly at 128 or 85 pixels, and whole rows hidden).  Now
 * findLeavesForParallelLoad, which already worked out the value, writes it before any thread
 * starts.
 *
 * A race is not something a test can make happen on demand, so this checks the ordering
 * instead: right after findLeavesForParallelLoad returns, which is where the threads would
 * start, every child it handed out must already hold its settled visibility.  Each child
 * starts with a sentinel visibility that no ancestor rule would give it, and the value is printed
 * beside the one limitedVisFromComposite, the lazy writer, assigns it.
 *
 * refs #38254 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "cart.h"
#include "hui.h"
#include "hgTracks.h"

static struct trackDb *makeTdb(char *name, char *vis, struct trackDb *parent)
/* A trackDb for a remote bigBed, or a composite when parent is NULL. */
{
struct trackDb *tdb;
AllocVar(tdb);
tdb->track = name;
tdb->table = name;
tdb->shortLabel = name;
tdb->longLabel = name;
tdb->settingsHash = hashNew(0);
tdb->visibility = hTvFromString(vis);
hashAdd(tdb->settingsHash, "visibility", vis);
if (parent == NULL)
    {
    tdb->type = "bigBed 6";
    hashAdd(tdb->settingsHash, "compositeTrack", "on");
    tdbMarkAsComposite(tdb);
    }
else
    {
    tdb->type = "bigBed 6";
    tdb->parent = parent;
    hashAdd(tdb->settingsHash, "parent", parent->track);
    hashAdd(tdb->settingsHash, "bigDataUrl", "https://hgdownload.soe.ucsc.edu/x/regress38254.bb");
    tdbMarkAsCompositeChild(tdb);
    slAddTail(&parent->subtracks, tdb);
    }
return tdb;
}

static struct track *makeTrack(struct trackDb *tdb, enum trackVisibility vis)
/* A track for tdb at the given visibility. */
{
struct track *track;
AllocVar(track);
track->track = tdb->track;
track->tdb = tdb;
track->visibility = vis;
return track;
}

static void showComposite(char *what, char *compVis, char *aCart, char *bCart)
/* Build a composite and two children, set each child's cart visibility if it is not NULL, find
 * the leaves to load, and print each child.  A child's own trackDb visibility is not used by the
 * browser (tdbVisLimitedByAncestors), so the children are set through the cart. */
{
cart = cartOfNothing();
if (aCart != NULL)
    cartSetString(cart, "regress38254a", aCart);
if (bCart != NULL)
    cartSetString(cart, "regress38254b", bCart);
struct trackDb *compTdb = makeTdb("regress38254comp", compVis, NULL);
struct track *comp = makeTrack(compTdb, hTvFromString(compVis));
char *names[] = {"regress38254a", "regress38254b"};
int i;
for (i = 0;  i < ArraySize(names);  ++i)
    {
    struct trackDb *tdb = makeTdb(names[i], "dense", compTdb);
    /* tvShow is the sentinel: no ancestor rule makes a child "show" */
    slAddTail(&comp->subtracks, makeTrack(tdb, tvShow));
    }
struct paraFetchData *pfdList = NULL;
findLeavesForParallelLoad(comp, &pfdList, FALSE);
printf("%s: composite %s, %d handed to loaders\n", what, compVis, slCount(pfdList));
struct track *sub;
for (sub = comp->subtracks; sub != NULL; sub = sub->next)
    {
    if (sub->parallelLoading)
        {
        enum trackVisibility settled = sub->visibility;
        /* what the lazy write in limitedVisFromComposite would have made it */
        enum trackVisibility lazy = tdbVisLimitedByAncestors(cart, sub->tdb, TRUE, TRUE);
        printf("  %s  loaded; visibility before the threads start: %s, the lazy value %s\n",
               sub->track, settled == tvShow ? "not settled" : hStringFromTv(settled),
               hStringFromTv(lazy));
        }
    else
        printf("  %s  not loaded\n", sub->track);
    }
}

int main(int argc, char *argv[])
{
database = "hg38";
showComposite("no cart settings", "full", NULL, NULL);
showComposite("no cart settings", "dense", NULL, NULL);
showComposite("one child set to pack and one hidden", "full", "pack", "hide");
showComposite("a child set to full under a dense composite", "dense", "full", NULL);
return 0;
}
