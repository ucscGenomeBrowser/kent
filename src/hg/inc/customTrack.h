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
#include "portable.h"

#define CT_PREFIX       "ct_"
#define CT_DEFAULT_TRACK_NAME    "User Track"
#define CT_DEFAULT_TRACK_DESCR   "User Supplied Track"

/* TODO: Remove this when hgCustom is ready for release. This cart variable is
 * only used to preserve old behavior during testing */
#ifndef CT_APPEND_DEFAULT
#define CT_APPEND_OK_VAR        "hgct_appendOk"
#endif

struct customTrack
/* A custom track.  */
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
    char *dbTableName;		/* name of table in trash database */
    char *dbTrackType;		/* type of data in this db table */
    boolean dbDataLoad;		/* FALSE == failed loading */
    char *wigFile;		/* name of .wig file in trash */
    char *wibFile;		/* name of .wib file in trash */
    char *wigAscii;		/* wiggle ascii data file name in trash .wia */
    struct gffFile *gffHelper;	/* Used while processing GFF files. */
    int offset;			/* Base offset. */
    char *groupName;		/* Group name if any. */
    };

/* cart/cgi variables */
#define CT_CUSTOM_TEXT_VAR      "hgt.customText"
#define CT_CUSTOM_TEXT_ALT_VAR  "hgct_customText"
#define CT_CUSTOM_FILE_VAR      "hgt.customFile"
#define CT_CUSTOM_FILE_NAME_VAR "hgt.customFile__filename"

#define CT_REMOVE_VAR           "hgct_remove"
#define CT_DO_REMOVE_VAR        "hgct_doRemoveCustomTrack"

struct customTrack *customTracksParseCart(struct cart *cart,
					  struct slName **retBrowserLines,
					  char **retCtFileName);
/* Parse custom tracks from cart */

/* Another method of creating customTracks is customFactoryParse. */

struct customTrack *customTracksParseCartDetailed(struct cart *cart,
					  struct slName **retBrowserLines,
					  char **retCtFileName,
                                          struct customTrack **retReplacedCts,
                                          char **retErr);
/* Figure out from cart variables where to get custom track text/file.
 * Parse text/file into a custom set of tracks.  Lift if necessary.  
 * If retBrowserLines is non-null then it will return a list of lines 
 * starting with the word "browser".  If retCtFileName is non-null then  
 * it will return the custom track filename.  If any existing custom tracks
 * are replaced with new versions, they are included in replacedCts.
 *
 * If there is a syntax error in the custom track this will report the
 * error, clear the custom track from the cart,  and return NULL.  It 
 * will also leak memory. */

void customTrackSave(struct customTrack *trackList, char *fileName);
/* Save out custom tracks. */

void customTrackLift(struct customTrack *trackList, struct hash *ctgPosHash);
/* Lift tracks based on hash of ctgPos. */

boolean customTrackNeedsLift(struct customTrack *trackList);
/* Return TRUE if any track in list needs a lift. */

char *customTrackTableFromLabel(char *label);
/* Convert custom track short label to table name. */

boolean customTrackTest();
/* Tests module - returns FALSE and prints warning message on failure. */

#define CUSTOM_TRASH	"customTrash"
/*	custom tracks database name	*/

boolean ctDbAvailable(char *tableName);
/*	determine if custom tracks database is available
 *	and if tableName non-NULL, verify table exists
 */

boolean ctDbUseAll();
/* check if hg.conf says to try DB loaders for all incoming data tracks */


void ctAddToSettings(struct customTrack *ct, char *name, char *val);
/*	add a variable to tdb settings */

void ctRemoveFromSettings(struct customTrack *ct, char *name);
/*	remove a variable from tdb settings */

void customTrackTrashFile(struct tempName *tn, char *suffix);
/*	obtain a customTrackTrashFile name	*/

struct trackDb *customTrackTdbDefault();
/* Return default custom table: black, dense, etc. */

boolean isCustomTrack(char *track);
/* determine if track name refers to a custom track */

void  customTrackDump(struct customTrack *track);
/* Write out info on custom track to stdout */

struct customTrack *customTrackAddToList(struct customTrack *ctList,
                                         struct customTrack *addCts,
                                         struct customTrack **retReplacedCts);
/* add new tracks to the custom track list, removing older versions,
 * and saving the replaced tracks in a list for the caller */

void customTrackHandleLift(struct customTrack *ctList);
/* lift any tracks with contig coords */

#endif
