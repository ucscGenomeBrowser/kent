/* Stuff lifted from hgTables that should be libified. */

#ifndef LIBIFYME_H
#define LIBIFYME_H

#include "annoFormatVep.h"
#include "annoStreamVcf.h"
#include "annoGrator.h"

boolean lookupPosition(struct cart *cart, char *cartVar);
/* Look up position if it is not already seq:start-end.
 * Return FALSE if it has written out HTML showing multiple search results.
 * If webGotWarnings() is true after this returns FALSE, no match was found
 * and a warning box was displayed, in which case it's good to reset position
 * to cart's lastPosition before proceeding. */

boolean hasCustomTracks(struct cart *cart);
/* Return TRUE if cart has custom tracks for the current db. */

void nbSpaces(int count);
/* Print some non-breaking spaces. */

void initGroupsTracksTables(struct cart *cart,
			    struct trackDb **retFullTrackList, struct grp **retFullGroupList);
/* Get lists of all tracks and of groups that actually have tracks in them. */

struct annoAssembly *getAnnoAssembly(char *db);
/* Make annoAssembly for db. */

struct annoStreamer *streamerFromTrack(struct annoAssembly *assembly, char *selTable,
				       struct trackDb *tdb, char *chrom, int maxOutRows);
/* Figure out the source and type of data and make an annoStreamer. */

struct annoGrator *gratorFromBigDataFileOrUrl(char *fileOrUrl, struct annoAssembly *assembly,
					      int maxOutRows, enum annoGratorOverlap overlapRule);
/* Determine what kind of big data file/url we have, make an annoStreamer & in annoGrator. */

struct annoGrator *gratorFromTrackDb(struct annoAssembly *assembly, char *selTable,
				     struct trackDb *tdb, char *chrom, int maxOutRows,
				     struct asObject *primaryAsObj,
				     enum annoGratorOverlap overlapRule);
/* Figure out the source and type of data, make an annoStreamer & wrap in annoGrator.
 * If not NULL, primaryAsObj is used to determine whether we can make an annoGratorGpVar. */

#endif//ndef LIBIFYME_H
