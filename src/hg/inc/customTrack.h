/* Data structure for dealing with custom tracks in the browser. */
#ifndef CUSTOMTRACK_H
#define CUSTOMTRACK_H

#ifndef BED_H
#include "bed.h"
#endif

#ifndef TRACKDB_H
#include "trackDb.h"
#endif

#include "cart.h"

struct customTrack
/* A custom track. */
    {
    struct customTrack *next;	/* Next in list. */
    struct trackDb *tdb;	/* TrackDb description of track. */
    struct bed *bedList;	/* List of beds. */
    int fieldCount;		/* Number of fields in bed. */
    int maxChromName;		/* max chromName length	*/
    boolean needsLift;		/* True if coordinates need lifting. */
    boolean fromPsl;		/* Track was derived from psl file. */
    boolean wiggle;		/* This is a wiggle track */
    boolean dbTrack;		/* This track is in the trash database */
    char *dbTrackName;		/* name of table in trash database */
    char *dbTrackType;		/* type of data in this db table */
    boolean dbDataLoad;		/* FALSE == failed loading */
    char *wigFile;		/* name of .wig file in trash */
    char *wibFile;		/* name of .wib file in trash */
    char *wigAscii;		/* wiggle ascii data file name in trash .wia */
    struct gffFile *gffHelper;	/* Used while processing GFF files. */
    int offset;			/* Base offset. */
    char *groupName;		/* Group name if any. */
    };

struct customTrack *customTracksFromText(char *text);
/* Parse text into a custom set of tracks. */

struct customTrack *customTracksFromFile(char *text);
/* Parse file into a custom set of tracks. */

struct customTrack *customTracksParse(char *text, boolean isFile, 
	struct slName **retBrowserLines);
/* Parse text into a custom set of tracks.  Text parameter is a
 * file name if 'isFile' is set.  If retBrowserLines is non-null
 * then it will return a list of lines starting with the word "browser". */

struct customTrack *customTracksParseCart(struct cart *cart,
					  struct slName **retBrowserLines,
					  char **retCtFileName);
/* Figure out from cart variables where to get custom track text/file.
 * Parse text/file into a custom set of tracks.  Lift if necessary.  
 * If retBrowserLines is non-null then it will return a list of lines 
 * starting with the word "browser".  If retCtFileName is non-null then 
 * it will return the custom track filename. */

void customTrackSave(struct customTrack *trackList, char *fileName);
/* Save out custom tracks. */

void customTrackLift(struct customTrack *trackList, struct hash *ctgPosHash);
/* Lift tracks based on hash of ctgPos. */

boolean customTrackNeedsLift(struct customTrack *trackList);
/* Return TRUE if any track in list needs a lift. */

struct bed *customTrackBed(char *row[13], int wordCount, struct hash *chromHash, int lineIx);
/* Convert a row of strings to a bed. */

char *customTrackTableFromLabel(char *label);
/* Convert custom track short label to table name. */

boolean customTrackTest();
/* Tests module - returns FALSE and prints warning message on failure. */

#define CUSTOM_TRASH	"customTrash"
/*	custom tracks database name	*/

boolean ctDbAvailable();
/*	determine if custom tracks database is available	*/

void ctAddToSettings(struct trackDb *tdb, char *format, ...);
/*	add a variable to tdb->settings string	*/

#endif
