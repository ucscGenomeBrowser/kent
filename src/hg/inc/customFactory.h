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
    	char *genomeDb, struct customPp *cpp, char *type, 
    	struct customTrack *track);
    /* Called by custom tracks framework.  Track line if any will
     * be read in already and parsed into type and track.
     * Type may be NULL, filled in from type= part of track line.
     * settings is all the this=that in the track line.
     * This routine is allowed to fill in some of track structure if
     * it returns TRUE. */

     struct customTrack * (*loader)(struct customFactory *fac, 
        char *genomeDb,                             
    	struct hash *chromHash,  /* Hash to store chrom names, filled in here */
    	struct customPp *cpp, 	 /* Source of input */
	struct customTrack *track, /* Skeleton of track, filled in here */
	boolean dbRequested);	 /* If true load into temp database */
    /* Load up a custom track. The track line is already parsed (and
     * available in the settings hash).  This routine reads from cpp
     * until the next track line or until end of file. If dbRequested is
     * TRUE it will attempt to load track into a MySQL database. 
     * Returns track (or in some cases tracks) that it loads. */
    };

/*** Some externally defined factories. ***/
extern struct customFactory chromGraphFactory;

/*** Some helper functions factories can call. ***/

char *customFactoryNextTilTrack(struct customPp *cpp);
/* Return next line.  Return NULL at end of input or at line starting with
 * "track." */

char *customFactoryNextRealTilTrack(struct customPp *cpp);
/* Return next "real" line (not blank, not comment).
 * Return NULL at end of input or at line starting with
 * "track." */

char *customTrackTempDb();
/* Get custom database.  If first time set up some
 * environment variables that the loaders will need. */

void customFactorySetupDbTrack(struct customTrack *track);
/* Fill in fields most database-resident custom tracks need. */

/*** Interface to custom factory system. ***/

struct customFactory *customFactoryFind(char *genomeDb, struct customPp *cpp,
	char *type, struct customTrack *track);
/* Figure out factory that can handle this track.  The track is
 * loaded from the track line if any, and type is the type element
 * if any from that track. */

void customFactoryAdd(struct customFactory *fac);
/* Add factory to global custom track factory list. */

struct customTrack *customFactoryParse(char *genomeDb, char *text, boolean isFile,
	struct slName **retBrowserLines);
/* Parse text into a custom set of tracks.  Text parameter is a
 * file name if 'isFile' is set.  Die if the track is not for hGetDb(). */

struct customTrack *customFactoryParseAnyDb(char *genomeDb, char *text, boolean isFile,
					    struct slName **retBrowserLines);
/* Parse text into a custom set of tracks.  Text parameter is a
 * file name if 'isFile' is set.  Track does not have to be for hGetDb(). */

void customFactoryTestExistence(char *genomeDb, char *fileName,
                                boolean *retGotLive, boolean *retGotExpired);
/* Test existence of custom track fileName.  If it exists, parse it just 
 * enough to tell whether it refers to database tables and if so, whether 
 * they are alive or have expired.  If they are live, touch them to keep 
 * them active. */

/*  HACK ALERT - The table browser needs to be able to encode its wiggle
 *	data.  This function is temporarily global until a proper method
 *	is used to work this business into the table browser custom
 *	tracks.  Currently this is also called from customSaveTracks()
 *	in customTrack.c in violation of this object's hidden methods.
 */
void wigLoaderEncoding(struct customTrack *track, char *wigAscii,
	boolean dbRequested);
/* encode wigAscii file into .wig and .wib files */

char *customDocParse(char *text);
/* Return description text, expanding URLs as for custom track data */

char *ctInitialPosition(struct customTrack *ct);
/* return initial position plucked from browser lines, or
 * first item in track, or NULL if no items or browser line */

char *ctDataUrl(struct customTrack *ct);
/* return URL where data can be reloaded, if any */

char *ctHtmlUrl(struct customTrack *ct);
/* return URL where doc can be reloaded, if any */

char *ctInputType(struct customTrack *ct);
/* return type of input */

int ctItemCount(struct customTrack *ct);
/* return number of 'items' in track, or -1 if unknown */

char *ctFirstItemPos(struct customTrack *ct);
/* return position of first item in track, or NULL if wiggle or
 * other "non-item" track */

char *ctGenome(struct customTrack *ct);
/* return database setting */

char *ctOrigTrackLine(struct customTrack *ct);
/* return initial setting by user for track line */

void customTrackUpdateFromConfig(struct customTrack *ct, char *genomeDb,
                                 char *config, struct slName **retBrowserLines);
/* update custom track from config containing track line and browser lines 
 * Return browser lines */

char *customTrackUserConfig(struct customTrack *ct);
/* return user-defined track line and browser lines */

#endif /* CUSTOMFACTORY_H */


