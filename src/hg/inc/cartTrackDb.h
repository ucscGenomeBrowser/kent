/* cartTrackDb - Combine custom, hub & sql trackDb's to get unified groups/tracks/tables */

#include "cart.h"
#include "trackDb.h"
#include "grp.h"

void cartTrackDbInit(struct cart *cart, struct trackDb **retFullTrackList,
                     struct grp **retFullGroupList, boolean useAccessControl);
/* Get lists of all tracks and of groups that actually have tracks in them.
 * If useAccessControl, exclude tracks with 'tableBrowser off' nor tables listed
 * in the table tableAccessControl. */

boolean cartTrackDbIsAccessDenied(char *db, char *table);
/* Return TRUE if useAccessControl=TRUE was passed to cartTrackDbInit and
 * if access to table is denied (at least on this host) by 'tableBrowser off'
 * or by the tableAccessControl table. */

boolean cartTrackDbIsNoGenome(char *db, char *table);
/* Return TRUE if range queries, but not genome-queries, are permitted for this table. */

struct slName *cartTrackDbTablesForTrack(char *db, struct trackDb *track, boolean useJoiner);
/* Return list of all tables associated with track.  If useJoiner, the result can include
 * non-positional tables that are related to track by all.joiner. */
