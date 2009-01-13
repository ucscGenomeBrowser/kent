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

#ifndef MJP
#define MJP(v) verbose((v),"%s[%3d]: ", __func__, __LINE__)
#endif

// need to handle Accept commands and negotiate content types.
// - for example, if app asks for JSON, send this: 
//#define okSendHeader() fprintf(stdout,"Content-type: application/json\n\n") // this triggers download by browser client
// - otherwise if browser, then send same content but this type:
#define okSendHeader(format) fprintf(stdout,"Content-type: text/%s\n\n", ((format) ? (format) : "plain"))
#define FMT_JSON_ANNOJ "json-annoj"

// Error responses
#define ERR_INVALID_COMMAND(cmd) errClientCode(400, "Invalid request %s", (cmd))
#define ERR_NO_DATABASE	errClientStatus(420, "Request error", "Database required")
#define ERR_NO_TRACK	errClientStatus(420, "Request error", "Track required")
#define ERR_NO_DB_CONNECTION(db) errClientStatus(420, "Request error", "Could not connect to database %s", (db))
#define ERR_NO_DBS_FOUND errClientStatus(420, "Request error", "No databases found") // maybe this should be a server error
#define ERR_DB_NOT_FOUND(db) errClientStatus(420, "Request error", "Database %s not found", (db))
#define ERR_NO_CHROM errClientStatus(420, "Request error", "Chrom required")
#define ERR_CHROM_NOT_FOUND(db,chrom) errClientStatus(420, "Request error", "Chrom %s not found in database %s", (chrom), (db))
#define ERR_TRACK_NOT_FOUND(track, db) errClientStatus(420, "Request error", "Track %s not found in database %s", (track), (db))
#define ERR_TRACK_INFO_NOT_FOUND(track, db) errClientStatus(420, "Request error", "Track info for %s not found in database %s", (track), (db))
#define ERR_TABLE_NOT_FOUND(table, chrom, tableRoot, db) errClientStatus(420, "Request error", "Table %s not found using chrom %s and tableRoot %s in database %s", (table), (chrom), (tableRoot), (db))
#define ERR_BAD_FORMAT(format) errClientStatus(420, "Request error", "Format %s is not supported", (format))
#define ERR_BAD_ACTION(action, track, db) errClientStatus(420, "Request error", "Action %s unknown for track %s in database %s", (action), (track), (db))
#define ERR_BAD_TRACK_TYPE(track, type) errClientStatus(420, "Request error", "Track %s of type %s is not supported", (track), (type))

/* Global Variables */
char quoteBuf[1024];

/* Structs */
struct dbDbClade
/* Description of annotation database including clade */
    {
    struct dbDbClade *next;  /* Next in singly linked list. */
    char *name; /* Short name of database.  'hg8' or the like */
    char *description;  /* Short description - 'Aug. 8, 2001' or the like */
    char *organism;     /* Common name of organism - first letter capitalized */
    char *genome;       /* Unifying genome collection to which an assembly belongs */
    char *scientificName;       /* Genus and species of the organism; e.g. Homo sapiens */
    char *sourceName;   /* Source build/release/version of the assembly */
    char *clade;        /* organism clade, eg. mammal */
    char *defaultPos;   /* Default starting position */
    int orderKey;       /* Int used to control display order within a genome */
    float priority;     /* Source build/release/version of the assembly */
    };

/* functions */

char *quote(char *field);
// format field surrounded by quotes "field" in buf and return buf
// return 'null' without quotes if field is NULL

struct chromInfo *getAllChromInfo(char *db);
/* Query db.chromInfo for all chrom info. */

void dbDbCladeFreeList(struct dbDbClade **pList);
/* Free a list of dynamically allocated dbDbClade's */

struct dbDbClade *hGetIndexedDbClade(char *db);
/* Get list of active databases and clade
 * Only get details for one 'db' unless NULL
 * in which case get all databases.
 * Dispose of this with dbDbCladeFreeList. */

void errClientStatus(int code, char *status, char *format, ...);
// create a HTTP response code 400-499 and status, 
// with format specifying the error message content

void errClientCode(int code, char *format, ...);
// create a HTTP response code 400-499 with standard status,
// and format specifying the error message content

void errClient(char *format, ...);
// create a HTTP response code "400 Bad Request",
// and format specifying the error message content

void printBedAsAnnoj(struct bed *b, struct hTableInfo *hti);
// print out rows of bed data formatted as AnnoJ nested model

void printBed(int n, struct bed *b, char *db, char *track, char *type, char *chrom, int start, int end, struct hTableInfo *hti);
// print out rows of bed data, each row as a list of columns

void printBedByColumn(struct bed *b, struct hTableInfo *hti);
// print out a list of bed records by column

void printDb(struct dbDbClade *db);
// print information for one database

void printDbs(struct dbDbClade *db);
// print an array of all databases

void printChrom(struct chromInfo *ci);
// print a chromosome 

void printChroms(struct chromInfo *ci);
// print an array of all chromosomes

void printGenomeAsAnnoj(struct dbDbClade *db, struct chromInfo *ci);
// print information for a genome - the database and all its chromosomes
// using AnnoJ format (http://www.annoj.org)

void printGenome(struct dbDbClade *db, struct chromInfo *ci);
// print information for a genome - the database and all its chromosomes

void printTrackInfo(char *db, char *track, struct trackDb *tdb);
// print database and track information


#endif /* HGTRACKS_H */
