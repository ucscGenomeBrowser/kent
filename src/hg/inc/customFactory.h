/* customFactory - a polymorphic object for handling
 * creating various types of custom tracks. */

#ifndef CUSTOMFACTORY_H
#define CUSTOMFACTORY_H

#ifndef CUSTOMPP_H
#include "customPp.h"
#endif

#ifndef CUSTOMTRACK_H
#include "customTrack.h"
#endif

struct customFactory
/* customFactory - a polymorphic object for handling
 * creating various types of custom tracks. */
    {
    struct customFactory *next;
    char *name;		/* Name - psl, bed, wig, chromGraph, etc. */

    boolean (*recognizer)(struct customFactory *fac,
    	struct customPp *cpp, char *type, 
    	struct customTrack *track);
    /* Called by custom tracks framework.  Track line if any will
     * be read in already and parsed into type and track.
     * Type may be NULL, filled in from type= part of track line.
     * settings is all the this=that in the track line.
     * This routine is allowed to fill in some of track structure if
     * it returns TRUE. */

    boolean (*loader)(struct customFactory *fac, 
    	struct hash *chromHash,  /* Hash to store chrom names, filled in here */
    	struct customPp *cpp, 	 /* Source of input */
	struct customTrack *track, /* Skeleton of track, filled in here */
	boolean dbRequested);	 /* If true load into temp database */
    /* Load up a custom track. The track line is already parsed (and
     * available in the settings hash).  This routine reads from cpp
     * until the next track line or until end of file. If dbRequested is
     * TRUE it will attempt to load track into a MySQL database. 
     * Returns FALSE if it couldn't load. */
    };

struct customFactory *customFactoryFind(struct customPp *cpp,
	char *type, struct customTrack *track);
/* Figure out factory that can handle this track.  The track is
 * loaded from the track line if any, and type is the type element
 * if any from that track. */

struct customFactory *customFactoryMustFind(struct customPp *cpp,
	char *type, struct customTrack *track);
/* Like customFactoryFind, but prints error and aborts if a problem */

void customFactoryAdd(struct customFactory *fac);
/* Add factory to global custom track factory list. */

struct customTrack *customFactoryParse(char *text, boolean isFile,
	struct slName **retBrowserLines);
/* Parse text into a custom set of tracks.  Text parameter is a
 * file name if 'isFile' is set.*/

#endif /* CUSTOMFACTORY_H */


