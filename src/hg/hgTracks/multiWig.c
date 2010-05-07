/* A container for multiple wiggles. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "container.h"
#include "wigCommon.h"

static void multiWigDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw items in container. */
{
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

static boolean graphLimitsAllSame(struct track *trackList)
/* Return TRUE if graphUpperLimit and graphLowerLimit same for all tracks. */
{
struct track *firstTrack = trackList;
if (firstTrack == NULL)
    return FALSE;
struct track *track;
for (track = firstTrack->next; track != NULL; track = track->next)
    if (track->graphUpperLimit != firstTrack->graphUpperLimit 
      || track->graphLowerLimit != firstTrack->graphLowerLimit)
        return FALSE;
return TRUE;
}

static void multiWigLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width, int height,
	boolean withCenterLabels, MgFont *font, Color color,
	enum trackVisibility vis)
/* Draw left labels - by deferring to first subtrack. */
{
boolean showNumbers = graphLimitsAllSame(tg->subtracks);
wigLeftAxisLabels(tg, seqStart, seqEnd, hvg, xOff, yOff, width, height, withCenterLabels,
	font, color, vis, tg->shortLabel, tg->subtracks->graphUpperLimit, 
	tg->subtracks->graphLowerLimit, showNumbers);
}

void multiWigContainerMethods(struct track *track)
/* Override general container methods for multiWig. */
{
track->totalHeight = multiWigTotalHeight;
track->drawItems = multiWigDraw;
track->drawLeftLabels = multiWigLeftLabels;
}
