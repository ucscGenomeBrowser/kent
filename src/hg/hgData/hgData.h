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
#define okSendHeader() fprintf(stdout,"Content-type: text/plain\n\n")

// Error responses
#define ERR_INVALID_COMMAND(cmd) errClientCode(400, "Invalid request %s", (cmd))
#define ERR_NO_DATABASE	errClientStatus(420, "Request error", "Please supply database")
#define ERR_NO_DBS_FOUND errClientStatus(420, "Request error", "No databases found") // maybe this should be a server error
#define ERR_DB_NOT_FOUND(db) errClientStatus(420, "Request error", "Database %s not found", (db));
#define ERR_NO_CHROM errClientStatus(420, "Request error", "Please supply chrom")
#define ERR_CANT_PARSE_POSITION(pos, db) errClientStatus(420, "Request error", "Cant parse %s into chrom:start-end in database %s", (pos), (db));
#define ERR_TRACK_NOT_FOUND(track, db) errClientStatus(420, "Request error", "Cant find track %s in database %s", (track), (db));
#define ERR_TABLE_NOT_FOUND(root, table, db) errClientStatus(420, "Request error", "Cant find root table %s for table %s in database %s", (root), (table), (db));

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

void errClientStatus(int code, char *status, char *format, ...);
// create a HTTP response code 400-499 and status, 
// with format specifying the error message content

void errClientCode(int code, char *format, ...);
// create a HTTP response code 400-499 with standard status,
// and format specifying the error message content

void errClient(char *format, ...);
// create a HTTP response code "400 Bad Request",
// and format specifying the error message content

void printBed(struct bed *b, struct hTableInfo *hti);
// print out rows of bed data, each row as a list of columns

void printBedByColumn(struct bed *b, struct hTableInfo *hti);
// print out a list of bed records by column

void printDb(struct dbDbClade *db);
// print an array of all active data in dbDb,clade table

void printOneDb(struct dbDbClade *db);
// print one db from dbDb table

void printDbByColumn(struct dbDbClade *db);
// print an array of all values in the db,clade tables by column

void printTrackDb(struct trackDb *tdb);
// print a list of rows of trackDb data for every track

void printOneTrackDb(struct trackDb *tdb);
// print trackDb data for a single track

void printTrackDbByColumn(struct trackDb *db);
// print a list of trackDb data by column

void printChroms(struct chromInfo *ci);
// print a list of rows of chromInfo for every chrom

void printOneChrom(struct chromInfo *ci);
// print chromInfo for a single chrom

void printChromsByColumn(struct chromInfo *ci);
// print a list of chromInfo data by column

void dbDbCladeFreeList(struct dbDbClade **pList);
/* Free a list of dynamically allocated dbDbClade's */

struct dbDbClade *hGetIndexedDbClade(char *db);
/* Get list of active databases and clade
 * Only get details for one 'db' unless NULL
 * in which case get all databases.
 * Dispose of this with dbDbCladeFreeList. */


#endif /* HGTRACKS_H */
