/* Stuff to handle chromGraph tracks, especially custom ones. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "trackDb.h"
#include "chromGraph.h"
#include "customTrack.h"

#include "hgGenome.h"

static char const rcsid[] = "$Id: chromGraph.c,v 1.2 2008/09/17 18:36:35 galt Exp $";

boolean isChromGraph(struct trackDb *track)
/* Return TRUE if it's a chromGraph track */
{
if (track && track->type)
    return startsWithWord("chromGraph", track->type);
else
    return FALSE;
}


