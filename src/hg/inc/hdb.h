/* hdb - human genome browser database. */

#ifndef HDB_H
#define HDB_H

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif

#ifndef DYSTRING_H
#include "dystring.h"
#endif

#ifndef JKSQL_H
#include "jksql.h"
#endif 

#ifndef TRACKDB_H
#include "trackDb.h"
#endif

struct blatServerTable
/* Information about a BLAT server. */
{
    char *db;		/* Database name. */
    char *genome;	/* Genome name. */
    boolean isTrans;	/* Is tranlated to protein? */
    char *host;		/* Name of machine hosting server. */
    char *port;		/* Port that hosts server. */
    char *nibDir;	/* Directory of sequence files. */
};

struct hTableInfo
/* Some info to track table. */
    {
    struct hTableInfo *next;	/* Next in list. */
    char *rootName;		/* Name without chrN_. */
    boolean isPos;		/* True if table is positional. */
    boolean isSplit;		/* True if table is split. */
    boolean hasBin;		/* True if table starts with field. */
    char chromField[32];		/* Name of chromosome field. */
    char startField[32];		/* Name of chromosome start field. */
    char endField[32];		/* Name of chromosome end field. */
    };

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

void hFreeConn(struct sqlConnection **pConn);
/* Put back connection for reuse. */

struct sqlConnection *hConnectCentral();
/* Connect to central database where user info and other info
 * not specific to a particular genome lives.  Free this up
 * with hDisconnectCentral(). */

void hDisconnectCentral(struct sqlConnection **pConn);
/* Put back connection for reuse. */

boolean hTableExists(char *table);
/* Return TRUE if a table exists in database. */

int hChromSize(char *chromName);
/* Return size of chromosome. */

struct dnaSeq *hChromSeq(char *chrom, int start, int end);
/* Return lower case DNA from chromosome. */

struct dnaSeq *hDnaFromSeq(char *seqName, 
	int start, int end, enum dnaCase dnaCase);
/* Fetch DNA in a variety of cases.  */

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
/* List of all database versions that are online (database
 * names only).  See also hDbDbList. */

struct dbDb *hDbDbList();
/* Return list of databases that are actually online. 
 * The list includes the name, description, and where to
 * find the nib-formatted DNA files. Free this with dbDbFree. */

boolean hIsPrivateHost();
/* Return TRUE if this is running on private web-server. */

boolean hTrackOnChrom(struct trackDb *tdb, char *chrom);
/* Return TRUE if track exists on this chromosome. */

struct trackDb *hTrackDb(char *chrom);
/* Load tracks associated with current chromosome (which may
 * be NULL */

struct hTableInfo *hFindTableInfo(char *chrom, char *rootName);
/* Find table information.  Return NULL if no table. */

boolean hFindChromStartEndFields(char *table, 
	char retChrom[32], char retStart[32], char retEnd[32]);
/* Given a table return the fields for selecting chromosome, start, and end. */

boolean hIsBinned(char *table);
/* Return TRUE if a table is binned. */

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

boolean hgParseChromRange(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd);
/* Parse something of form chrom:start-end into pieces. */

boolean hgIsChromRange(char *spec);
/* Returns TRUE if spec is chrom:N-M for some human
 * chromosome chrom and some N and M. */

struct trackDb *hTrackInfo(struct sqlConnection *conn, char *trackName);
/* Look up track in database. */

struct dbDb *hGetIndexedDatabases();
/* Get list of databases for which there is a nib dir. 
 * Dispose of this with dbDbFreeList. */

struct dbDb *hGetBlatIndexedDatabases();
/* Get list of databases for which there is a BLAT index. 
 * Dispose of this with dbDbFreeList. */

boolean hIsBlatIndexedDatabase(char *db);
/* Return TRUE if have a BLAT server on sequence corresponding 
 * to give database. */

struct blatServerTable *hFindBlatServer(char *db, boolean isTrans);
/* return the blat server information corresponding to the database */

char *hOrganism(char *database);
/* Return organism associated with database.   Use freeMem on
 * return value when done. */

char *hLookupStringVars(char *in, char *database);
/* Expand $ORGANISM and other variables in input. */

void hLookupStringsInTdb(struct trackDb *tdb, char *database);
/* Lookup strings in track database. */

#endif /* HDB_H */
