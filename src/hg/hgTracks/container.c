/* A more-or-less generic container class that hopefully will someday encompass
 * the superTrack and subTrack systems.  Tracks that have the tag "container"
 * get sent here for processing. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "container.h"


void containerLoadItems(struct track *track)
/* containerLoadItems - call load routine on all children. */
{
struct track *subtrack;
for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack) && !subtrack->parallelLoading)
	subtrack->loadItems(subtrack);
    }
}

static void containerFree(struct track *track)
/* containerFree - call free routine on all children. */
{
struct track *subtrack;
for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
    subtrack->freeItems(subtrack);
}

void containerDrawItems(struct track *track, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw items in container. */
{
struct track *subtrack;
int y = yOff;
for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack))
	{
	subtrack->drawItems(subtrack, seqStart, seqEnd, hvg, xOff, y, width, font, color, 
		vis);
	y += subtrack->totalHeight(subtrack, subtrack->limitedVis);
	}
    }
}

static int containerTotalHeight(struct track *track, enum trackVisibility vis)
/* Return total height of container. */
{
int totalHeight = 0;
struct track *subtrack;
for (subtrack = track->subtracks; subtrack != NULL; subtrack = subtrack->next)
    {
    if (isSubtrackVisible(subtrack))
	{
	totalHeight += subtrack->totalHeight(subtrack, vis);
	track->lineHeight = subtrack->lineHeight;
	track->heightPer = subtrack->heightPer;
	}
    }
track->height = totalHeight;
return totalHeight;
}

char *parentContainerType(struct track *track)
/* Determine parent's container type if any or NULL */
{
if (track->parent && track->parent->tdb)
    return trackDbSetting(track->parent->tdb, "container");
else
    return NULL;
}

void makeContainerTrack(struct track *track, struct trackDb *tdb)
/* Construct track subtrack list from trackDb entry for container tracks. */
{
// uglyf("track->track=%s, track->table=%s, tdb->track=%s, tdb->table=%s<BR>\n", track->track,track->table, tdb->track, tdb->table);
/* Wrap tracks around child tdb's, maintaining same heirarchy as in tdb. */
struct trackDb *subtdb;
for (subtdb = tdb->subtracks; subtdb != NULL; subtdb = subtdb->next)
    {
    struct track *subtrack = trackFromTrackDb(subtdb);
    TrackHandler handler = lookupTrackHandler(subtdb->table);
    if (handler != NULL)
	handler(subtrack);
    slAddHead(&track->subtracks, subtrack);
    subtrack->parent = track;
    if (subtdb->subtracks != NULL)
	makeContainerTrack(subtrack, subtdb);
    }
slSort(&track->subtracks, trackPriCmp);

/* Set methods that may be shared by all containers. */
track->loadItems = containerLoadItems;
track->freeItems = containerFree;
track->drawItems = containerDrawItems;
track->totalHeight = containerTotalHeight;

/* Set methods specific to containers. */
char *containerType = trackDbSetting(tdb, "container");
if (sameString(containerType, "multiWig"))
    {
    multiWigContainerMethods(track);
    }
else
    errAbort("unknown container type %s in trackDb for %s", containerType, tdb->track);
}


