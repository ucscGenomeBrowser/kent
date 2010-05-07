/* A more-or-less generic container class that hopefully will someday encompass
 * the superTrack and subTrack systems.  Tracks that have the tag "container"
 * get sent here for processing. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"


static void containerLoad(struct track *tg)
/* containerLoad - call load routine on all children. This one is generic for all containers. */
{
struct track *subtrack;
for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
    subtrack->loadItems(subtrack);
}

static void containerFree(struct track *tg)
/* containerFree - call free routine on all children. */
{
struct track *subtrack;
for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
    subtrack->freeItems(subtrack);
}

static void multiWigDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw items in container. */
{
// hvGfxBox(hvg, xOff, yOff, width, tg->height, MG_BLUE);
struct track *subtrack;
for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack))
	subtrack->drawItems(subtrack, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
    }
}

static int multiWigTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return total height of container. */
{
int totalHeight =  wigTotalHeight(tg, vis);
struct track *subtrack;
for (subtrack = tg->subtracks; subtrack != NULL; subtrack = subtrack->next)
    subtrack->totalHeight(subtrack, vis);
return totalHeight;
}

void multiWigLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width, int height,
	boolean withCenterLabels, MgFont *font, Color color,
	enum trackVisibility vis)
/* Draw left labels - by deferring to first subtrack. */
{
if (tg->subtracks)
    tg->drawLeftLabels(tg->subtracks, seqStart, seqEnd, hvg, xOff, yOff, 
    	width, height, withCenterLabels, font, color, vis);
}

void makeContainerTrack(struct track *track, struct trackDb *tdb)
/* Construct track subtrack list from trackDb entry for container tracks. */
{
/* Wrap tracks around child tdb's, maintaining same heirarchy as in tdb. */
struct trackDb *subtdb;
for (subtdb = tdb->subtracks; subtdb != NULL; subtdb = subtdb->next)
    {
    struct track *subtrack = trackFromTrackDb(subtdb);
    TrackHandler handler = lookupTrackHandler(subtdb->tableName);
    if (handler != NULL)
	handler(subtrack);
    slAddHead(&track->subtracks, subtrack);
    if (subtdb->subtracks != NULL)
	makeContainerTrack(subtrack, subtdb);
    }
slSort(&track->subtracks, trackPriCmp);

/* Set methods shared by all containers. */
track->loadItems = containerLoad;
track->freeItems = containerFree;

/* Set methods specific to containers. */
char *containerType = trackDbSetting(tdb, "container");
if (sameString(containerType, "multiWig"))
    {
    track->totalHeight = multiWigTotalHeight;
    track->drawItems = multiWigDraw;
    track->drawLeftLabels = multiWigLeftLabels;
    }
else
    errAbort("unknown container type %s in trackDb for %s", containerType, tdb->tableName);
}


