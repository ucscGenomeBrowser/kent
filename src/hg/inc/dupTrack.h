/* Stuff to help handle a track that is a duplicate of a another - sharing data
 * and type, but having it's own cart settings. */

#ifndef DUPTRACK_H
#define DUPTRACK_H

#define DUP_TRACK_PREFIX       "dup_"
#define DUP_TRACKS_VAR DUP_TRACK_PREFIX "tracks"

struct dupTrack 
/* A duplicate track */
    {
    struct dupTrack *next;
    char *name;		/* Usually will look like dup_1_track_name */
    char *source;	/* Will look like track_name */
    struct slPair *tagList;  /* List of tags */
    };

struct dupTrack *dupTrackReadAll(char *fileName);
/* Read in ra file and return it as a list of dupTracks */

struct trackDb *dupTdbFrom(struct trackDb *sourceTdb, struct dupTrack *dup);
/* Generate a duplicate trackDb */

struct dupTrack *dupTrackListFromCart(struct cart *cart);
/* Consult cart for dup track variable and if it's there return
 * list of dupes */

struct dupTrack *dupTrackFindInList(struct dupTrack *list, char *name);
/* Return matching element in list */

char *dupTrackInCartAndTrash(char *sourceTrack, struct cart *cart, struct trackDb *sourceTdb);
/* Update cart vars to reflect existance of duplicate of sourceTrack.
 * Also write out or append to dupe track trash file */

void undupTrackInCartAndTrash(char *sourceTrack, struct cart *cart);
/* Update cart vars to reflect removal of dupTrack.  Also reduce or 
 * remove trash file */

boolean isDupTrack(char *track);
/* determine if track name refers to a custom track */

char *dupTrackSkipToSourceName(char *dupeTrackName);
/* If it looks like it's a dupe track then skip over duppy part 
 * in particular skip over dup_N_ form prefix for numerical N. */

boolean dupTrackEnabled();
/* Return true if we allow tracks to be duped. */

#endif /* DUPTRACK_H */
