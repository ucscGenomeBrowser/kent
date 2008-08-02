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
#define CT_FILE_VAR_PREFIX       "ctfile_"
#define CT_DEFAULT_TRACK_NAME    "User Track"
#define CT_DEFAULT_TRACK_DESCR   "User Supplied Track"

#define CT_MANAGE_BUTTON_LABEL   "manage custom tracks"
#define CT_ADD_BUTTON_LABEL      "add custom tracks"

/* setting used when creating custom tracks in table browser -- indicates
 * custom track file in trash has not been through the factory parser */
#define CT_UNPARSED              "unparsed"

/* TODO: Remove this when hgCustom is ready for release. This cart variable is
 * only used to preserve old behavior during testing */

struct customTrack
/* A custom track.  */
/* NOTE: if you add any *File members to struct customTrack, please update 
 * cart.c cartCopyCustomTracks() accordingly! */
    {
    struct customTrack *next;	/* Next in list. */
    struct trackDb *tdb;	/* TrackDb description of track. */
    char *genomeDb;             /* Genome database the track is associated */
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
    char *dbStderrFile;		/* trash file to receive stderr of loaders */
    char *wigFile;		/* name of .wig file in trash */
    char *wibFile;		/* name of .wib file in trash */
    char *wigAscii;		/* wiggle ascii data file name in trash .wia */
    char *htmlFile;             /* name of .html file in trash */
    struct gffFile *gffHelper;	/* Used while processing GFF files. */
    int offset;			/* Base offset. */
    char *groupName;		/* Group name if any. */
    };

/* cart/cgi variables */
#define CT_CUSTOM_TEXT_VAR      "hgt.customText"
    /* alternate variable, compatible with javascript */
#define CT_CUSTOM_TEXT_ALT_VAR  "hgct_customText"
#define CT_CUSTOM_FILE_VAR      "hgt.customFile"
#define CT_CUSTOM_FILE_NAME_VAR "hgt.customFile__filename"
#define CT_CUSTOM_DOC_TEXT_VAR  "hgct_docText"
#define CT_CUSTOM_DOC_FILE_VAR  "hgct_docFile"
#define CT_CUSTOM_DOC_FILE_NAME_VAR "hgct_docFile__filename"

#define CT_DO_REMOVE_VAR        "hgct_doRemoveCustomTrack"
#define CT_SELECTED_TABLE_VAR   "hgct_table"
#define CT_UPDATED_TABLE_VAR    "hgct_updatedTable"

struct customTrack *customTracksParseCart(char *genomeDb, struct cart *cart,
					  struct slName **retBrowserLines,
					  char **retCtFileName);
/* Parse custom tracks from cart */

/* Another method of creating customTracks is customFactoryParse. */

struct customTrack *customTracksParseCartDetailed(char *genomeDb, struct cart *cart,
					  struct slName **retBrowserLines,
					  char **retCtFileName,
                                          struct customTrack **retReplacedCts,
                                          int *retNumAdded,
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

void customTracksSaveCart(char *genomeDb, struct cart *cart, struct customTrack *ctList);
/* Save custom tracks to trash file for database in cart */

void customTracksSaveFile(char *genomeDb, struct customTrack *trackList, char *fileName);
/* Save out custom tracks. This is just used by internally  */

char *customTrackFileVar(char *database);
/* return CGI var name containing custom track filename for a database */

void customTrackLift(struct customTrack *trackList, struct hash *ctgPosHash);
/* Lift tracks based on hash of ctgPos. */

boolean customTrackNeedsLift(struct customTrack *trackList);
/* Return TRUE if any track in list needs a lift. */

char *customTrackTableFromLabel(char *label);
/* Convert custom track short label to table name. */

#define CUSTOM_TRASH	"customTrash"
/*	custom tracks database name	*/
#define CT_META_INFO	"metaInfo"
/*	table name in customTrash for last accessed memory and other data */
#define CT_EXTFILE	"extFile"
/*      table with references to external files  */

void ctTouchLastUse(struct sqlConnection *conn, char *table,
	boolean status);
/* for status==TRUE - update metaInfo information for table
 * for status==FALSE - delete entry for table from metaInfo table
 */

boolean verifyWibExists(struct sqlConnection *conn, char *table);
/* given a ct database wiggle table, see if the wib file is there */

boolean ctDbTableExists(struct sqlConnection *conn, char *table);
/* verify if custom trash db table exists, touch access stats */

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

struct trackDb *customTrackTdbDefault();
/* Return default custom table: black, dense, etc. */

boolean isCustomTrack(char *track);
/* determine if track name refers to a custom track */

boolean customTrackIsCompressed(char *fileName);
/* test for file suffix indicating compression */

void  customTrackDump(struct customTrack *track);
/* Write out info on custom track to stdout */

struct customTrack *customTrackAddToList(struct customTrack *ctList,
                                         struct customTrack *addCts,
                                         struct customTrack **retReplacedCts,
                                         boolean makeDefaultUnique);
/* add new tracks to the custom track list, removing older versions,
 * and saving the replaced tracks in a list for the caller */

void customTrackHandleLift(char *db, struct customTrack *ctList);
/* lift any tracks with contig coords */

boolean customTracksExist(struct cart *cart, char **retCtFileName);
/* determine if there are any custom tracks.  Cleanup from expired tracks */

boolean ctConfigUpdate(char *filename);
/* CT update is needed if database has been enabled since
 * the custom tracks in this file were created.  The only way to check is
 * by file mod time, unless we add the enable time to
 * browser metadata somewhere */

#endif
