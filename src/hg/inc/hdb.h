/* hdb - human genome browser database. */

#ifndef HDB_H
#define HDB_H

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif

#ifndef DYSTRING_H
#include "dystring.h"
#endif

#ifndef TRACKDB_H
#include "trackDb.h"
#endif

void hDefaultConnect();
/* read the default settings from the config file */

void hSetDbConnect(char* host, char *db, char *user, char *password);
/* set the connection information for the database */

void hSetDb(char *dbName);
/* Set the database name. */

char *hGetDb();
/* Return the current database name. */

char *hGetDbHost();
/* Return the current database host. */

char *hGetDbName();
/* Return the current database name. */

char *hGetDbUser();
/* Return the current database user. */

char *hGetDbPassword();
/* Return the current database password. */

struct sqlConnection *hAllocConn();
/* Get free connection if possible. If not allocate a new one. */

struct sqlConnection *hFreeConn(struct sqlConnection **pConn);
/* Put back connection for reuse. */

boolean hTableExists(char *table);
/* Return TRUE if a table exists in database. */

int hChromSize(char *chromName);
/* Return size of chromosome. */

struct dnaSeq *hDnaFromSeq(char *seqName, 
	int start, int end, enum dnaCase dnaCase);
/* Fetch DNA */

struct dnaSeq *hLoadChrom(char *chromName);
/* Fetch entire chromosome into memory. */

void hNibForChrom(char *chromName, char retNibName[512]);
/* Get .nib file associated with chromosome. */

struct slName *hAllChromNames();
/* Get list of all chromosomes. */

struct dnaSeq *hExtSeq(char *acc);
/* Return sequence for external seq. */

struct dnaSeq *hRnaSeq(char *acc);
/* Return sequence for RNA. */

struct hash *hCtgPosHash();
/* Return hash of ctgPos from current database keyed by contig name. */

char *hFreezeFromDb(char *database);
/* return the freeze for the database version. 
   For example: "hg6" returns "Dec 12, 2000". If database
   not recognized returns NULL */

char *hDbFromFreeze(char *freeze);
/* Return database version from freeze name. */

struct slName *hDbList();
/* List of all database versions. */

boolean hTrackOnChrom(struct trackDb *tdb, char *chrom);
/* Return TRUE if track exists on this chromosome. */

boolean hFindChromStartEndFields(char *table, 
	char retChrom[32], char retStart[32], char retEnd[32]);
/* Given a table return the fields for selecting chromosome, start, and end. */

boolean hFindFieldsAndBin(char *table, 
	char retChrom[32], char retStart[32], char retEnd[32],
	boolean *retBinned);
/* Given a table return the fields for selecting chromosome, start, 
 * and whether it's binned . */

boolean hFindSplitTable(char *chrom, char *rootName, 
	char retTableBuf[64], boolean *hasBin);
/* Find name of table that may or may not be split across chromosomes. 
 * Return FALSE if table doesn't exist.  */

int hBinLevels();
/* Return number of levels to bins. */

int hBinFirstShift();
/* Return amount to shift a number to get to finest bin. */

int hBinNextShift();
/* Return amount to shift a numbe to get to next coarser bin. */

int hFindBin(int start, int end);
/* Given start,end in chromosome coordinates assign it
 * a bin.   There's a bin for each 128k segment, for each
 * 1M segment, for each 8M segment, for each 64M segment,
 * and for each chromosome (which is assumed to be less than
 * 512M.)  A range goes into the smallest bin it will fit in. */

void hAddBinToQuery(int start, int end, struct dyString *query);
/* Add clause that will restrict to relevant bins to query. */

struct sqlResult *hRangeQuery(struct sqlConnection *conn,
	char *rootTable, char *chrom,
	int start, int end, char *extraWhere, int *retRowOffset);
/* Construct and make a query to tables that may be split and/or
 * binned. */

struct sqlResult *hOrderedRangeQuery(struct sqlConnection *conn,
	char *rootTable, char *chrom,
	int start, int end, char *extraWhere, int *retRowOffset);
/* Construct and make a query to tables that may be split and/or
 * binned. Forces return values to be sorted by chromosome start. */

struct sqlResult *hChromQuery(struct sqlConnection *conn,
	char *rootTable, char *chrom,
	char *extraWhere, int *retRowOffset);
/* Construct and make a query across whole chromosome to tables 
 * that may be split and/or
 * binned. */

int hOffsetPastBin(char *chrom, char *table);
/* Return offset into a row of table that skips past bin
 * field if any. */

enum trackVisibility 
/* How to look at a track. */
    {
    tvHide=0, 		/* Hide it. */
    tvDense=1,        /* Squish it together. */
    tvFull=2        /* Expand it out. */
    };  

boolean hgParseChromRange(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd);
/* Parse something of form chrom:start-end into pieces. */

boolean hgIsChromRange(char *spec);
/* Returns TRUE if spec is chrom:N-M for some human
 * chromosome chrom and some N and M. */

struct trackDb *hTrackInfo(struct sqlConnection *conn, char *trackName);
/* Look up track in database. */

#endif /* HDB_H */
