#ifndef HGDATA_H
#define HGDATA_H

#ifndef HDB_H
#include "hdb.h"
#endif

#ifndef BED_H
#include "bed.h"
#endif

#ifndef TRACKDB_H
#include "trackDb.h"
#endif

#ifndef CHROMINFO_H
#include "chromInfo.h"
#endif

#ifndef BASEMASKCOMMON_H
#include "baseMaskCommon.h"
#endif

#ifndef CHEAPCGI_H
#include "cheapcgi.h"
#endif

#ifndef HGFIND_H
#include "hgFind.h"
#endif

#ifdef boolean
#undef boolean
// common.h defines boolean as int; json.h typedefs boolean as int.
#include <json/json.h>
#endif

#define TRACK_EXPIRES 15*60   // 15 minutes for track data & metadata
#define GENOME_EXPIRES 60*60  // 60 minutes for genomes 
#define INDEX_EXPIRES 60*60   // 60 minutes for index page
#define SEARCH_EXPIRES 0      // dont put expires header for search for now

#define PREFIX      "/g/"
#define GENOMES_CMD "genomes"
#define TRACKS_CMD  "tracks"
#define COUNT_CMD   "count"
#define RANGE_CMD   "range"
#define SEARCH_CMD  "search"
#define META_SEARCH_CMD "meta_search"
#define CMD_ARG     "cmd"

#define GENOME_VAR "{genome}"
#define GENOME_ARG "genome"
#define TRACK_VAR "{track}"
#define TRACK_ARG "track"
#define TERM_VAR "{term}"
#define TERM_ARG "term"
#define CHROM_VAR "{chrom}"
#define CHROM_ARG "chrom"
#define STRAND_VAR "{strand}"
#define STRAND_ARG "strand"
#define START_VAR "{start}"
#define START_ARG "start"
#define END_VAR "{end}"
#define END_ARG "end"
#define FORMAT_ARG "format"
#define ANNOJ_FLAT_FMT ".annoj-flat"
#define ANNOJ_NESTED_FMT ".annoj-nested"
#define ALLCHROMS_ARG "all_chroms"

#define ALLCHROMS_ARG "all_chroms"

#define MAX_CHROM_COUNT 10000 // max number of chromosomes to display automatically in genome query


#ifndef MJP
#define MJP(v) verbose((v),"%s[%3d]: ", __func__, __LINE__)
#endif

// need to handle Accept commands and negotiate content types.
// - for example, if app asks for JSON, send this: 
//#define okSendHeader() fprintf(stdout,"Content-type: application/json\n\n") // this triggers download by browser client
// - otherwise if browser, then send same content but this type:
//#define okSendHeader(format) fprintf(stdout,"Content-type: %s\n\n", ((format) ? (format) : "application/json"))

// Error responses
#define ERR_INVALID_COMMAND(cmd) errClientCode(400, "Invalid request [%s]", (cmd))
#define ERR_NO_GENOME	errClientStatus(420, "Request error", "Genome required")
#define ERR_NO_TRACK	errClientStatus(420, "Request error", "Track required")
#define ERR_NO_GENOME_DB_CONNECTION(db) errClientStatus(420, "Request error", "Could not connect to genome database %s", (db))
#define ERR_NO_GENOMES_FOUND errClientStatus(420, "Request error", "No genome databases found") // maybe this should be a server error
#define ERR_GENOME_NOT_FOUND(db) errClientStatus(404, "Request error", "Genome %s not found", (db))
#define ERR_NO_CHROM errClientStatus(420, "Request error", "Chrom required")
#define ERR_CHROM_NOT_FOUND(db,chrom) errClientStatus(420, "Request error", "Chrom %s not found in genome %s", (chrom), (db))
#define ERR_NO_CHROMS_FOUND(db) errClientStatus(420, "Request error", "No chroms found in genome %s", (db))
#define ERR_START_AFTER_END(start, end) errClientStatus(420, "Request error", "Start position %d after end %d", (start), (end))
#define ERR_TRACK_NOT_FOUND(track, db) errClientStatus(420, "Request error", "Track %s not found in genome %s", (track), (db))
#define ERR_TRACK_INFO_NOT_FOUND(track, db) errClientStatus(420, "Request error", "Track info for %s not found in genome %s", (track), (db))
#define ERR_TABLE_NOT_FOUND(table, chrom, tableRoot, db) errClientStatus(420, "Request error", "Table %s not found using chrom %s and tableRoot %s in genome %s", (table), (chrom), (tableRoot), (db))
#define ERR_TABLE_INFO_NOT_FOUND(table, chrom, tableRoot, db) errClientStatus(420, "Request error", "Table information for %s not found using chrom %s and tableRoot %s in genome %s", (table), (chrom), (tableRoot), (db))
#define ERR_BAD_FORMAT(format) errClientStatus(420, "Request error", "Format %s unknown", (format))
#define ERR_BAD_ACTION(action, track, db) errClientStatus(420, "Request error", "Action %s unknown for track %s in genome %s", (action), (track), (db))
#define ERR_BAD_TRACK_TYPE(track, type) errClientStatus(420, "Request error", "Track %s of type %s is not supported", (track), (type))
#define ERR_NOT_IMPLEMENTED(feature) errClientStatus(420, "Request error", "%s not implemented", (feature))

/* Global Variables */
char quoteBuf[1024];

/* Structs */

struct coords {
  int len, mid, left, right, leftIn10x, rightIn10x, leftOut10x, rightOut10x;
};

struct dbDbClade
/* Description of annotation database including clade */
    {
    struct dbDbClade *next;  /* Next in singly linked list. */
    char *clade;             /* organism clade, eg. mammal */
    char *genome;            /* Unifying genome collection to which an assembly belongs */
    char *description;       /* Short description - 'Aug. 8, 2001' or the like */
    char *name;              /* Short name of database.  'hg8' or the like */
    char *organism;          /* Common name of organism - first letter capitalized */
    char *scientificName;    /* Genus and species of the organism; e.g. Homo sapiens */
    int taxId;               /* NCBI Taxon Id */
    char *sourceName;        /* Source build/release/version of the assembly */
    char *defaultPos;        /* Default starting position */
    };

/* functions */
char *nullOrVal(char *val);
// return the value if not null, or else the string "(null)"

char *valOrVariable(char *val, char *variable);
// Return val if not NULL, else return variable

int stripToInt(char *string);
// Convert a string with digits, commas, and decimals into an integer
// by stripping the commas and decimals

char *etag(time_t modified);
// Convert modification time to ETag
// Returned value must be freed by caller

time_t strToTime(char *time, char *format);
// Convert human time to unix time using format

char *gmtimeToStr(time_t time, char *format);
// Convert unix time to human time using format
// Returned value must be freed by caller

char *gmtimeToHttpStr(time_t timeVal);
// Convert unix time to human time using HTTP format
// Returned value must be freed by caller

boolean notModifiedResponse(char *reqEtag, time_t reqModified, time_t modified);
// Returns TRUE if request is not modified and sends a 304 Not Modified HTTP header
// Otherwise request is modified so return FALSE

void okSendHeader(time_t modified, int expireSecs);
// Send a 200 OK header
// If modified > 0, set Last-Modified date (and ETag) based on this
// If expireSecs > 0, set Expires header to now+expireSecs

void send2xxHeader(int status, time_t modified, int expireSecs, char *contentType);
// Send a 2xx header
// If modified > 0 set Last-Modified date (and ETag based on this)
// if contentType is NULL, defaults to Content-Type: application/json

void send3xxHeader(int status, time_t modified, int expireSecs, char *contentType);
// Send a 3xx header
// If modified > 0 set Last-Modified date (and ETag based on this)
// if contentType is NULL, defaults to Content-Type: application/json

void errClientStatus(int code, char *status, char *format, ...);
// create a HTTP response code 400-499 and status, 
// with format specifying the error message content

void errClientCode(int code, char *format, ...);
// create a HTTP response code 400-499 with standard status,
// and format specifying the error message content

void errClient(char *format, ...);
// create a HTTP response code "400 Bad Request",
// and format specifying the error message content

///////////////////////

struct chromInfo *getAllChromInfo(char *db);
/* Query db.chromInfo for all chrom info. */

void dbDbCladeFreeList(struct dbDbClade **pList);
/* Free a list of dynamically allocated dbDbClade's */

struct dbDbClade *hGetIndexedDbClade(char *db);
/* Get list of active genome databases and clade
 * Only get details for one 'db' unless NULL
 * in which case get all genome databases.
 * Dispose of this with dbDbCladeFreeList. */

time_t hGetLatestUpdateTimeDbClade();
// return the latest time that any of the relevant tables were changed

time_t hGetLatestUpdateTimeChromInfo(char *db);
// return the latest time that the chromInfo table was changed

time_t oneTrackDbUpdateTime(char *db, char *tblSpec);
/* get latest update time for a trackDb table, including handling profiles:tbl. 
 * Returns 0 if table doesnt exist
  */

time_t trackDbLatestUpdateTime(char *db);
/* Get latest update time from each trackDb table. */

time_t findSpecLatestUpdateTime(char *db);
/* Get latest update time from each hgFindSpec table. */

time_t validateGenome(char *genome);
// Validate that genome is active (must not be null)
// Return update time of chromInfo table in genome database

void validateChrom(char *genome, char *chrom, struct chromInfo **pCi);
// Validate that chrom exists (must not be null)
// Return chrom info for chrom

time_t validateLoadTrackDb(char *genome, char *track, struct trackDb **pTdb);
// Validate that track exists by loading track Db to find track
// Genome and track must not be null
// Return update time of table in genome database, and trackDb data

////////////////////////
void printItemAsAnnoj(char *db, char *track, char *type, char *term);
// print out a description for a track item

void printItem(char *db, char *track, char *type, char *term);
// print out a description for a track item

//////////////////
void jsonAddTableInfoOneTrack(struct json_object *o, struct hTableInfo *hti);
// Add table info such as columns names keyed off 'table_properties'

void printGenomes(char *genome, char *chrom, struct dbDbClade *db, struct chromInfo *ci, time_t modified);
// print an array of all genomes in list,
// print genome hierarchy for all genomes
// if only one genome in list, 
//   print array of chromosomes in ci list (or empty list if null)
// modified is latest update (unix) time of all relevant tables 

void printTrackInfo(char *genome, char *track, struct trackDb *tdb, struct hTableInfo *hti, time_t modified);
// Print genome and track information for the genome
// If only one track is specified, print full details including html description page
//
// tracks => {_genome_ => [{ _track_name_ => {_track_details_},... }] }
// groups => {_genome_ => [{_group_name_ => [{_track_name_: _track_priority_},... ]}] }

struct json_object *addGenomeUrl(struct json_object *o, char *url_name, char *genome, char *chrom);
// Add a genome url to object o with name 'url_name'.
// Genome required if chrom specified, otherwise can be NULL
// Chrom can be NULL
// Returns object o

struct json_object *addTrackUrl(struct json_object *o, char *url_name, char *genome, char *track);
// Add a track url to object o with name 'url_name'.
// Genome must be supplied if track is supplied, otherwise can be NULL
// Track can be NULL
// Returns object o

struct json_object *addCountChromUrl(struct json_object *o, char *url_name, char *track, char *genome, char *chrom);
// Add a count url to object o with name 'url_name'.
// Genome required if chrom specified, otherwise can be NULL
// Chrom can be NULL
// Returns object o

struct json_object *addCountUrl(struct json_object *o, char *url_name, char *track, char *genome, char *chrom, int start, int end);
// Add a count url to object o with name 'url_name'.
// Genome required if chrom specified, otherwise can be NULL
// Chrom can be NULL
// If start or end not required specify -1 
// Returns object o

void addRangeUrls(struct json_object *d, char *track, char *genome, char *chrom, int start, int end);
// generic range URL templates

struct json_object *addRangeChromUrl(struct json_object *o, char *url_name, char *extension, char *track, char *genome, char *chrom);
// Add a range url to object o with name 'url_name'.
// Genome required if chrom specified, otherwise can be NULL
// Chrom can be NULL
// Returns object o

struct json_object *addRangeUrl(struct json_object *o, char *url_name, char *extension, char *track, char *genome, char *chrom, int start, int end);
// Add a range url to object o with name 'url_name'.
// Genome required if chrom specified, otherwise can be NULL
// Chrom can be NULL
// If start or end not required specify -1 
// Returns object o

void printBedCount(char *genome, char *track, char *chrom, int start, int end, int chromSize, struct hTableInfo *hti, int n, time_t modified);
void printBed(char *genome, char *track, char *type, char *chrom, int start, int end, int chromSize, struct hTableInfo *hti, int n, struct bed *b, char *format, time_t modified);

void printMaf(char *genome, char *track, char *chrom, int chromSize, int start, int end, char *strand);
// print maf records which intersect this start-end range

struct coords navigate(int start, int end, int chromSize);
// Calculate navigation coordinates including window left, window right
// Zoom in 10x, Zoom out 10x

void printUsage(char *reqEtag, time_t reqModified);

struct json_object *addSearchGenomeUrl(struct json_object *o, char *url_name, char *genome, char *term);
// Add a search url to object o with name 'url_name'.
// Genome can be NULL
// Item can be NULL
// Returns object o

struct json_object *addSearchGenomeTrackUrl(struct json_object *o, char *url_name, char *genome, char *track, char *term);
// Add a search url to object o with name 'url_name'.
// Genome must be supplied if track is supplied, otherwise can be NULL
// Track can be NULL
// Item can be NULL
// Returns object o

void searchTracks(time_t modified, struct hgPositions *hgp, char *genome, char *track, char *query);
// search for data within tracks or whole genomes

#endif /* HGDATA_H */
