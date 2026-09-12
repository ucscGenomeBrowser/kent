/* trackDbExpand - build subtracks from a metadata file instead of from trackDb rows.
 *
 * A container track that carries a subtrackTemplate setting names two files: a
 * tab separated metadata file with one row per sample, and a template file with
 * one stanza per data type.  Every row of the metadata file crossed with every
 * stanza of the template makes one subtrack.  The subtracks are built when
 * trackDb is loaded, before the list is cached, so every consumer of trackDb
 * sees the same list it would have seen if the stanzas were in trackDb. */

#ifndef TRACKDBEXPAND_H
#define TRACKDBEXPAND_H

#ifndef TRACKDB_H
#include "trackDb.h"
#endif

struct trackDb *trackDbExpandSubtracks(struct trackDb *tdbList, struct dyString *incFiles);
/* Expand every track in the list that has a subtrackTemplate setting into its
 * generated subtracks.  Returns the list with the new tracks spliced in.  If
 * incFiles is not NULL, the metadata and template files are added to it so that
 * the trackDb cache expires when either of them changes. */

#endif /* TRACKDBEXPAND_H */
