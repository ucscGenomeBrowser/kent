/* Stuff to handle chromGraph tracks, especially custom ones. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "trackDb.h"
#include "chromGraph.h"
#include "customTrack.h"

#include "hgGenome.h"


boolean isChromGraph(struct trackDb *track)
/* Return TRUE if it's a chromGraph track */
{
if (track && track->type)
    return startsWithWord("chromGraph", track->type);
else
    return FALSE;
}


