/* hdb - human genome browser database. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef HDB_H
#define HDB_H

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif

#ifndef DNASEQ_H
#include "dnaseq.h"
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

#ifndef HGRELATE_H
#include "hgRelate.h"
#endif

#ifndef BED_H
#include "bed.h"
#endif

#ifndef CUSTOMTRACK_H
#include "customTrack.h"
#endif
struct chromInfo;

/* At or below this number of sequences, allow split tables: */
#define HDB_MAX_SEQS_FOR_SPLIT 100

/* Statically-allocated string lengths (max supported len incl. final \0): */
#define HDB_MAX_CHROM_STRING 255
#define HDB_MAX_BAND_STRING 64
#define HDB_MAX_FIELD_STRING 32
#define HDB_MAX_TABLE_STRING 128
#define HDB_MAX_PATH_STRING 512


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
/* Some info on a track table, extracted from its field names. */
    {
    struct hTableInfo *next;	/* Next in list. */
    char *rootName;		/* Name without chrN_. */
    boolean isPos;		/* True if table is positional. */
    boolean isSplit;		/* True if table is split. */
    boolean hasBin;		/* True if table starts with field. */
    char chromField[HDB_MAX_FIELD_STRING];	/* Name of chromosome field. */
    char startField[HDB_MAX_FIELD_STRING];	/* Name of chromosome start field. */
    char endField[HDB_MAX_FIELD_STRING];		/* Name of chromosome end field. */
    char nameField[HDB_MAX_FIELD_STRING];		/* Name of item name field. */
    char scoreField[HDB_MAX_FIELD_STRING];	/* Name of score field. */
    char strandField[HDB_MAX_FIELD_STRING];	/* Name of strand field. */
    char cdsStartField[HDB_MAX_FIELD_STRING];	/* Name of cds(thick)Start field. */
    char cdsEndField[HDB_MAX_FIELD_STRING];	/* Name of cds(thick)End field. */
    char countField[HDB_MAX_FIELD_STRING];	/* Name of exon(block)Count field. */
    char startsField[HDB_MAX_FIELD_STRING];	/* Name of exon(block)Starts field. */
    char endsSizesField[HDB_MAX_FIELD_STRING];	/* Name of exon(block)Ends(Sizes) field. */
    char spanField[HDB_MAX_FIELD_STRING];		/* Name of span field. (wiggle) */
    boolean hasCDS;		/* True if it has cdsStart,cdsEnd fields. */
    boolean hasBlocks;		/* True if it has count,starts,endsSizes. */
    char *type;			/* A guess at the trackDb type for this. */
    };

struct slName *hTrackDbList(void);
/* Return list of trackDb tables from the config file.  Free list when done. */

void hSetTrackDbName(char *trackDbName);
/* Override the hg.conf db.trackDb setting. */

struct slName *hTrackTablesOfType(struct sqlConnection *conn, char *type);
/* get list of tables in trackDb list with type */

char *hPdbFromGdb(char *genomeDb);
/* return the name of the proteome database given the genome database name */

boolean hDbExists(char *database);
/* Function to check if this is a valid db name */

boolean hDbIsActive(char *database);
/* Function to check if this is a valid and active db name */

char *hDefaultDb(void);
/* Return the default db if all else fails */

char *hDbForTaxon(int taxon);
/* Get database associated with NCBI taxon number if any. */

char *hDbForSciName(char *sciName);
/* Get default db for scientific name */

char *hDefaultChrom(char *db);
/* Return some sequence named in chromInfo from the given db, or NULL if db
 * has no chromInfo. */

int hChromCount(char *db);
/* Return the number of chromosomes (scaffolds etc.) in the given db. */

char *hNcbiGcfId(char *db);
/* Return the NCBI RefSeq assembly+annotations ID (GCF_...) for db, or NULL if we don't know it. */

char *hNcbiGcaId(char *db);
/* Return the NCBI GenBank assembly id (GCA_...) for db, or NULL if we don't know it. */

struct sqlConnection *hAllocConn(char *db);
/* Get free connection if possible. If not allocate a new one. */

struct sqlConnection *hAllocConnMaybe(char *db);
/* Get free connection if possible. If not allocate a new one. Return
 * NULL if db doesn't exist or can't be connected to. */

char *getTrackProfileName(struct trackDb *tdb);
/* get profile is associated with a track, return it, otherwise NULL */

struct sqlConnection *hAllocConnProfile(char *profileName, char *db);
/* Get free connection, specifying a profile and/or a database. If none
 * is available, allocate a new one. */

struct sqlConnection *hAllocConnProfileMaybe(char *profileName, char *db);
/* Get free connection, specifying a profile and/or a database. If none is
 * available, allocate a new one.  Return NULL if database doesn't exist. */

struct sqlConnection *hAllocConnTrack(char *db, struct trackDb *tdb);
/* Get free connection for accessing tables associated with the specified
 * track and database. If none is available, allocate a new one. */

struct sqlConnection *hAllocConnProfileTbl(char *db, char *spec, char **tableRet);
/* Allocate a connection to db, spec can either be in the form `table' or
 * `profile:table'.  If it contains profile, connect via that profile.  Also
 * returns pointer to table in spec string. */

struct sqlConnection *hAllocConnProfileTblMaybe(char *db, char *spec, char **tableRet);
/* Allocate a connection to db, spec can either be in the form `table' or
 * `profile:table'.  If it contains profile, connect via that profile.  Also
 * returns pointer to table in spec string. Return NULL if database doesn't exist */

struct sqlConnection *hAllocConnDbTbl(char *spec, char **tableRet, char *defaultDb);
/* Allocate a connection to db and table, spec is in form `db.table'; if
 * defaultDb is not NULL, 'table' can also be used.  Also returns pointer to
 * table in spec string. */

void hFreeConn(struct sqlConnection **pConn);
/* Put back connection for reuse. */

struct sqlConnection *hConnectCentral(void);
/* Connect to central database where user info and other info
 * not specific to a particular genome lives.  Free this up
 * with hDisconnectCentral(). */

void hDisconnectCentral(struct sqlConnection **pConn);
/* Put back connection for reuse. */

struct sqlConnection *hConnectCart(void);
/* Connect to cart database.  Defaults to the central connection
 * unless cart.db or cart.host are configured. Free this
 * up with hDisconnectCart(). */

void hDisconnectCart(struct sqlConnection **pConn);
/* Put back connection for reuse. */

char *hgOfficialChromName(char *db, char *name);
/* Returns "cannonical" name of chromosome or NULL
 * if not a chromosome. */

boolean hgIsOfficialChromName(char *db, char *name);
/* Determine if name is exact (case-sensitive) match with
 * a chromosome in the current assembly */

boolean hgNearOk(char *database);
/* Return TRUE if ok to put up familyBrowser (hgNear)
 * on this database. */

boolean hgPcrOk(char *database);
/* Return TRUE if ok to put up hgPcr on this database. */

boolean hTableExists(char *db, char *table);
/* Return TRUE if a table exists in database. */

boolean hTableOrSplitExists(char *db, char *table);
/* Return TRUE if table (or a chrN_table) exists in database. */

char *hTableForTrack(char *db, char *trackName);
/* Return a table for a track in db. Returns one of the split
 * tables, or main table if not split */

char *hReplaceGbdbLocal(char* fileName);
 /* Returns a gbdb filename, potentially rewriting it according to hg.conf's gbdbLoc1 */
 /* Result has to be freed */

char *hReplaceGbdb(char* fileName);
/* clone and change a filename that can be located in /gbdb to somewhere else
 * according to hg.conf's "gbdbLoc1" and "gbdbLoc2". Result has to be freed. */

char *hReplaceGbdbSeqDir(char *path, char *db);
/* similar to hReplaceGbdb, but accepts a nib or 2bit "directory" (basename) under
 * gbdb, like /gbdb/hg19 (for jkLib that means /gbdb/hg19/hg19.2bit). */


char* hReplaceGbdbMustDownload(char* path);
/* given a location in /gbdb, rewrite it to the new location using gbdbLoc1 and download it
 * if needed from gbdbLoc2. */

void hParseTableName(char *db, char *table, char trackName[HDB_MAX_TABLE_STRING],
		     char chrom[HDB_MAX_CHROM_STRING]);
/* Parse an actual table name like "chr17_random_blastzWhatever" into
 * the track name (blastzWhatever) and chrom (chr17_random). */

void hParseDbDotTable(char *dbIn, char *dbDotTable, char *dbOut, size_t dbOutSize,
                      char *tableOut, size_t tableOutSize);
/* If dbDotTable contains a '.', then assume it is db.table and parse out into dbOut and tableOut.
 * If not, then it's just a table; copy dbIn into dbOut and dbDotTable into tableOut. */

int hChromSize(char *db, char *chromName);
/* Return size of chromosome. */

struct chromInfo *hGetChromInfo(char *db, char *chrom);
/* Get chromInfo for named chromosome (case-insens.) from db.
 * Return NULL if no such chrom. */

struct dnaSeq *hFetchSeq(char *fileName, char *seqName, int start, int end);
/* Fetch sequence from file.  If it is a .2bit file then fetch the named sequence.
If it is .nib then just ignore seqName. */

struct dnaSeq *hFetchSeqMixed(char *fileName, char *seqName, int start, int end);
/* Fetch mixed case sequence. */

struct dnaSeq *hChromSeq(char *db, char *chrom, int start, int end);
/* Return lower case DNA from chromosome. */

struct dnaSeq *hChromSeqFromPath(char *nibPath, char *db, char *chrom, 
				 int start, int end);
/* Return lower case DNA from chromosome. */

struct dnaSeq *hChromSeqMixed(char *db, char *chrom, int start, int end);
/* Return mixed case (repeats in lower case) DNA from chromosome. */

struct dnaSeq *hChromSeqMixedFromPath(char *nibPath, char *db, char *chrom, 
				      int start, int end);
/* Return mixed case (repeats in lower case) DNA from chromosome, given an
 * input nib path. */

struct dnaSeq *hSeqForBed(char *db, struct bed *bed);
/* Get the sequence associated with a particular bed concatenated together. */

boolean hChromBand(char *db, char *chrom, int pos, char retBand[HDB_MAX_BAND_STRING]);
/* Fill in text string that says what band pos is on.
 * Return FALSE if not on any band, or table missing. */

boolean hChromBandConn(struct sqlConnection *conn,
	char *chrom, int pos, char retBand[HDB_MAX_BAND_STRING]);
/* Fill in text string that says what band pos is on.
 * Return FALSE if not on any band, or table missing. */

boolean hScaffoldPos(char *db, char *chrom, int start, int end,
                     char **retScaffold, int *retStart, int *retEnd);
/* Return the scaffold, and start end coordinates on a scaffold, for
 * a chromosome range.  If the range extends past end of a scaffold,
 * it is truncated to the scaffold end.
 * Return FALSE if unable to convert */

struct dnaSeq *hDnaFromSeq(char *db, char *seqName,
	int start, int end, enum dnaCase dnaCase);
/* Fetch DNA in a variety of cases.  */

struct dnaSeq *hLoadChrom(char *db, char *chromName);
/* Fetch entire chromosome into memory. */

void hNibForChrom(char *db, char *chromName, char retNibName[HDB_MAX_PATH_STRING]);
/* Get .nib file associated with chromosome. */

void hNibForChromFromPath(char *nibPath, char *db, char *chromName, 
			  char retNibName[HDB_MAX_PATH_STRING]);
/* Get .nib file associated with chromosome, given a nib file path. */

struct slName *hAllChromNames(char *db);
/* Get list of all chromosomes in database. */

char *hExtFileNameC(struct sqlConnection *conn, char *extFileTable, unsigned extFileId);
/* Get external file name from table and ID.  Typically
 * extFile table will be 'extFile' or 'gbExtFile'
 * Abort if the id is not in the table or if the file
 * fails size check.  Please freeMem the result when you
 * are done with it. (requires conn passed in) */

char *hExtFileName(char *db, char *extFileTable, unsigned extFileId);
/* Get external file name from table and ID.  Typically
 * extFile table will be 'extFile' or 'gbExtFile'
 * Abort if the id is not in the table or if the file
 * fails size check.  Please freeMem the result when you
 * are done with it. */

struct dnaSeq *hDnaSeqGet(char *db, char *acc, char *seqTbl, char *extFileTbl);
/* Get a cDNA or DNA sequence from the specified seq and extFile tables.  The
 * seqTbl/extFileTbl arguments may include the database, in which case they
 * override what is in db (which could even be NULL). Return NULL if not
 * found. */

struct dnaSeq *hDnaSeqMustGet(char *db, char *acc, char *seqTbl, char *extFileTbl);
/* Get a cDNA or DNA sequence from the specified seq and extFile tables.  The
 * seqTbl/extFileTbl arguments may include the database, in which case they
 * override what is in db (which could even be NULL).
 * Abort if not found. */

struct dnaSeq *hDnaSeqGetConn(struct sqlConnection *conn, char *acc, char *seqTbl, char *extFileTbl);
/* Get a cDNA or DNA sequence from the specified seq and extFile tables. Return NULL if not
 * found. */

struct dnaSeq *hDnaSeqMustGetConn(struct sqlConnection *conn, char *acc, char *seqTbl, char *extFileTbl);
/* Get a cDNA or DNA sequence from the specified seq and extFile tables. 
 * Abort if not found. */

aaSeq *hPepSeqGet(char *db, char *acc, char *seqTbl, char *extFileTbl);
/* Get a peptide sequence from the specified seq and extFile tables.   The
 * seqTbl/extFileTbl arguments may include the database, in which case they
 * override what is in db (which could even be NULL). Return
 * NULL if not found. */

aaSeq *hPepSeqMustGet(char *db, char *acc, char *seqTbl, char *extFileTbl);
/* Get a peptide sequence from the specified seq and extFile tables. Abort if
 * not found. */

int hRnaSeqAndIdx(char *acc, struct dnaSeq **retSeq, HGID *retId, struct sqlConnection *conn);
/* Return sequence for RNA and  it's database ID. Return -1 if not found. */

char* hGetSeqAndId(struct sqlConnection *conn, char *acc, HGID *retId);
/* Return sequence as a fasta record in a string and it's database ID, or
 * NULL if not found. */

struct dnaSeq *hExtSeq(char *db, char *acc);
/* Return sequence for external seq. */

struct dnaSeq *hExtSeqPart(char *db, char *acc, int start, int end);
/* Return part of external sequence. */

struct dnaSeq *hRnaSeq(char *db, char *acc);
/* Return sequence for RNA. */

aaSeq *hPepSeq(char *db, char *acc);
/* Return sequence for a peptide. */

boolean hGenBankHaveSeq(char *db, char *acc, char *compatTable);
/* Get a GenBank or RefSeq mRNA or EST sequence or NULL if it doesn't exist.
 * This handles compatibility between pre-incremental genbank databases where
 * refSeq sequences were stored in tables and the newer scheme that keeps all
 * sequences in external files.  If compatTable is not NULL and the table
 * exists, it is used to obtain the sequence.  Otherwise the seq and gbSeq
 * tables are checked.
 */

struct dnaSeq *hGenBankGetMrna(char *db, char *acc, char *compatTable);
/* Get a GenBank or RefSeq mRNA or EST sequence or NULL if it doesn't exist.
 * This handles compatibility between pre-incremental genbank databases where
 * refSeq sequences were stored in tables and the newer scheme that keeps all
 * sequences in external files.  If compatTable is not NULL and the table
 * exists, it is used to obtain the sequence.  Otherwise the seq and gbSeq
 * tables are checked.
 */

struct dnaSeq *hGenBankGetMrnaC(struct sqlConnection *conn, char *acc, char *compatTable);
/* Same as above, but can pass in connection to any db */

aaSeq *hGenBankGetPep(char *db, char *acc, char *compatTable);
/* Get a RefSeq peptide sequence or NULL if it doesn't exist.  This handles
 * compatibility between pre-incremental genbank databases where refSeq
 * sequences were stored in tables and the newer scheme that keeps all
 * sequences in external files.  If compatTable is not NULL and the table
 * exists, it is used to obtain the sequence.  Otherwise the seq and gbSeq
 * tables are checked.
 */

aaSeq *hGenBankGetPepC(struct sqlConnection *conn, char *acc, char *compatTable);
/* Same as above, but can pass in connection to any db */

char *hGenBankGetDesc(char *db, char *acc, boolean native);
/* Get a description for a genbank or refseq mRNA. If native is TRUE, an
 * attempt is made to get a more compact description that doesn't include
 * species name. Acc may optionally include the version.  NULL is returned if
 * a description isn't available.  Free string when done. */

struct bed *hGetCtBedRange(char *db, char *browserDb, char *table, char *chrom, int chromStart,
			   int chromEnd, char *sqlConstraints);
/* Return a bed list of all items (that match sqlConstraints, if nonNULL)
 * in the given range in table.  If chromEnd is 0, omit the range (whole chrom).
 * WARNING: this does not use the bin column and maybe slower than you would like. */

struct bed *hGetBedRange(char *db, char *table, char *chrom, int chromStart,
			 int chromEnd, char *sqlConstraints);
/* Return a bed list of all items (that match sqlConstraints, if nonNULL)
 * in the given range in table.  If chromEnd is 0, omit the range (whole chrom).
 * WARNING: this does not use the bin column and maybe slower than you would like.*/

int hGetBedRangeCount(char *db, char *table, char *chrom, int chromStart,
                         int chromEnd, char *sqlConstraints);
/* Return a count of all the items (that match sqlConstraints, if nonNULL)
 * in the given range in table.  If chromEnd is 0, omit the range (whole chrom).
 * WARNING: this does not use the bin column and maybe slower than you would like.
 * C.f. hGetBedRange() but returns only the result of SELECT COUNT(*) FROM ...  */

struct bed *hGetFullBed(char *db, char *table);
/* Return a genome-wide bed list of the table. */
/* WARNING: This isn't designed for CGI use. It's a looped call to */
/* hGetBedRange() which has its own warning. */

struct hash *hCtgPosHash(void);
/* Return hash of ctgPos from current database keyed by contig name. */

char *hFreezeFromDb(char *database);
/* return the freeze for the database version.
   For example: "hg6" returns "Dec 12, 2000". If database
   not recognized returns NULL */

char *hDbFromFreeze(char *freeze);
/* Return database version from freeze name. */

struct slName *hDbList(void);
/* List of all database versions that are online (database
 * names only).  See also hDbDbList. */

struct dbDb *hDbDb(char *database);
/* Return dbDb entry for a database */

struct dbDb *hDbDbList(void);
/* Return list of databases that are actually online.
 * The list includes the name, description, and where to
 * find the nib-formatted DNA files. Free this with dbDbFree. */

struct dbDb *hDbDbListMaybeCheck(boolean doCheck);
/* Return list of databases in dbDb.  If doCheck, check database existence.
 * The list includes the name, description, and where to
 * find the nib-formatted DNA files. Free this with dbDbFree. */

struct hash *hDbDbHash();
/* The hashed-up version of the entire dbDb table, keyed on the db */

int hDbDbCmpOrderKey(const void *va, const void *vb);
/* Compare to sort based on order key */

char *hDbDbNibPath(char *database);
/* return nibPath from dbDb for database */

char *hHttpHost();
/* return http host from apache or hostname if run from command line  */

char *hLocalHostCgiBinUrl();
/* Return the current full absolute URL of the cgi-bin directory of the local
 * server in the format http(s)://<host>/<cgiBinDir>, e.g.
 * https://genome.ucsc.edu/cgi-bin/.
 * The <host> is coming from the SERVER_NAME variable, which is
 * the ServerName setting in the Apache config file.
 * The cgi-bin directory is coming from the SCRIPT_NAME variable, the relative
 * location of the cgi program relative to Apache's DOCUMENT_ROOT.
 * Https is used if the variable HTTPS is set by Apache.
 *
 * If login.relativeLink=on is set, return only the empty string. 
 * (This is used on the CIRM server, as it has no way of knowing what its
 * actual server name is, it is behind a reverse proxy)
 * Result has to be free'd. */

char *hLoginHostCgiBinUrl();
/* Return the current full absolute URL of the cgi-bin directory of the host
 * used for logins. Genome-euro/genome-asia use genome.ucsc.edu for the login,
 * as we have only one single server for user accounts.
 * Returns a string in the format
 * http(s)://<host>/cgi-bin/ e.g. http://genome.ucsc.edu/cgi-bin/
 * - the <host> is coming from the wiki.host variable in hg.conf.
 * - https is used unless login.useHttps=off in hg.conf
 *
 * If login.relativeLink=on is set, return only the empty string. 
 * (see hLocalCgiBinUrl)
 * Result has to be free'd. */

boolean hHostHasPrefix(char *prefix);
/* Return TRUE if this is running on web-server with host name prefix */


boolean hIsPrivateHost(void);
/* Return TRUE if this is running on private (development) web-server.
 * This was originally genome-test as well as hgwdev, however genome-test
 * may be repurposed to direct users to the preview site instead of development site. */

boolean hIsBetaHost(void);
/* Return TRUE if this is running on beta (QA) web-server.
 * Use sparingly as behavior on beta should be as close to RR as possible. */

boolean hIsBrowserbox();
/* Return TRUE if this is the browserbox virtual machine */

boolean hIsGbic();
/* Return TRUE if this mirror has been installed by the installation script */

boolean hIsPreviewHost(void);
/* Return TRUE if this is running on preview web-server.  The preview
 * server is a mirror of the development server provided for public
 * early access. */

char *hBrowserName();
/* Return browser name based on host name */

boolean hTrackOnChrom(struct trackDb *tdb, char *chrom);
/* Return TRUE if track exists on this chromosome. */

boolean trackDataAccessible(char *database, struct trackDb *tdb);
/* Return TRUE if underlying data are accessible - meaning the track has either
 * a bigDataUrl with remote URL (http:// etc), a bigDataUrl with an existing local file,
 * or a database table with the same name.
 * Note: this returns FALSE for composite tracks; use this on subtracks or simple tracks. */

struct trackDb *trackDbPolishAfterLinkup(struct trackDb *tdbList, char *db);
/* Do various massaging that can only be done after parent/child
 * relationships are established. */

struct trackDb *hTrackDb(char *db);
/* Load tracks associated with current db.
 * Supertracks are loaded as a trackDb, but are not in the returned list,
 * but are accessible via the parent pointers of the member tracks.  Also,
 * the supertrack trackDb subtrack fields are not set here (would be
 * incompatible with the returned list)
 * Returns list sorted by priority
 *	NOTE: this result is cached, do not free it !
 */

struct trackDb *tdbForTrack(char *db, char *track,struct trackDb **tdbList);
/* Load trackDb object for a track. If track is composite, its subtracks
 * will also be loaded and inheritance will be handled; if track is a
 * subtrack then inheritance will be handled.  (Unless a subtrack has
 * "noInherit on"...) This will die if the current database does not have
 * a trackDb, but will return NULL if track is not found.
 * MAY pass in prepopulated trackDb list, or may receive the trackDb list as an inout. */
#define hTrackDbForTrack(db,track) tdbForTrack(db,track,NULL)

struct trackDb *hTrackDbForTrackAndAncestors(char *db, char *track);
/* Load trackDb object for a track. If need be grab its ancestors too.
 * This does not load children. hTrackDbForTrack will handle children, and
 * is actually faster if being called on lots of tracks.  This function
 * though is faster on one or two tracks. */
// WARNING: this works for hub and db tracks but not custom tracks.

struct trackDb *hCompositeTrackDbForSubtrack(char *db, struct trackDb *sTdb);
/* Given a trackDb that may be for a subtrack of a composite track,
 * return the trackDb for the composite track if we can find it, else NULL.
 * Note: if the composite trackDb is found and returned, then its subtracks
 * member will contain a newly allocated tdb like sTdb (but not ==). */

struct hTableInfo *hFindTableInfoWithConn(struct sqlConnection *conn, char *chrom, char *rootName);
/* Find table information, with conn as part of input parameters.  Return NULL if no table.  */

struct hTableInfo *hFindTableInfo(char *db, char *chrom, char *rootName);
/* Find table information in specified db.  Return NULL if no table. */

int hTableInfoBedFieldCount(struct hTableInfo *hti);
/* Return number of BED fields needed to save hti. */

boolean hFindChromStartEndFields(char *db, char *table,
	char retChrom[HDB_MAX_FIELD_STRING],
	char retStart[HDB_MAX_FIELD_STRING],
	char retEnd[HDB_MAX_FIELD_STRING]);
/* Given a table return the fields for selecting chromosome, start, and end. */

boolean hIsBinned(char *db, char *table);
/* Return TRUE if a table is binned. */

int hFieldIndex(char *db, char *table, char *field)
/* Return index of field in table or -1 if it doesn't exist. */;

boolean hHasField(char *db, char *table, char *field);
/* Return TRUE if table has field */

boolean hFieldHasIndex(char *db, char *table, char *field);
/* Return TRUE if a SQL index exists for table.field. */

boolean hFindFieldsAndBin(char *db, char *table,
	char retChrom[HDB_MAX_FIELD_STRING],
	char retStart[HDB_MAX_FIELD_STRING],
	char retEnd[HDB_MAX_FIELD_STRING], boolean *retBinned);
/* Given a table return the fields for selecting chromosome, start, end,
 * and whether it's binned . */

boolean hFindSplitTable(char *db, char *chrom, char *rootName,
	char *retTableBuf, int tableBufSize, boolean *hasBin);
/* Find name of table in a given database that may or may not
 * be split across chromosomes. Return FALSE if table doesn't exist. 
 *
 * Do not ignore the return value. 
 * This function does NOT tell you whether or not the table is split. 
 * It tells you if the table exists. */

struct slName *hSplitTableNames(char *db, char *rootName);
/* Return a list of all split tables for rootName, or of just rootName if not
 * split, or NULL if no such tables exist. */

int hBinLevels(void);
/* Return number of levels to bins. */

int hBinFirstShift(void);
/* Return amount to shift a number to get to finest bin. */

int hBinNextShift(void);
/* Return amount to shift a numbe to get to next coarser bin. */

int hFindBin(int start, int end);
/* Given start,end in chromosome coordinates assign it
 * a bin.   There's a bin for each 128k segment, for each
 * 1M segment, for each 8M segment, for each 64M segment,
 * and for each chromosome (which is assumed to be less than
 * 512M.)  A range goes into the smallest bin it will fit in. */

void hAddBinToQueryGeneral(char *binField, int start, int end, struct dyString *query);
/* Add clause that will restrict to relevant bins to query. allow bin field name to be specified */

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

struct sqlResult *hExtendedRangeQuery(
	struct sqlConnection *conn,  /* Open SQL connection. */
	char *rootTable, 	     /* Table (not including any chrN_) */
	char *chrom, int start, int end,  /* Range. */
	char *extraWhere,            /* Extra things to add to where clause. */
	boolean order, 	   /* If true order by start position (can be slow). */
	char *fields,      /* If non-NULL comma separated field list. */
	int *retRowOffset); /* Returns offset past bin field. */
/* Range query with lots of options. */

struct sqlResult *hChromQuery(struct sqlConnection *conn,
	char *rootTable, char *chrom,
	char *extraWhere, int *retRowOffset);
/* Construct and make a query across whole chromosome to tables
 * that may be split and/or
 * binned. */

struct sqlResult *hExtendedChromQuery(
	struct sqlConnection *conn,  /* Open SQL connection. */
	char *rootTable, 	     /* Table (not including any chrN_) */
	char *chrom,  		     /* Chromosome. */
	char *extraWhere,            /* Extra things to add to where clause. */
	boolean order, 	   /* If true order by start position (can be slow). */
	char *fields,      /* If non-NULL comma separated field list. */
	int *retRowOffset); /* Returns offset past bin field. */
/* Chromosome query fields for tables that may be split and/or binned,
 * with lots of options. */

int hOffsetPastBin(char *db, char *chrom, char *table);
/* Return offset into a row of table that skips past bin
 * field if any. */

boolean hgParseChromRange(char *db, char *spec, char **retChromName,
	int *retWinStart, int *retWinEnd);
/* Parse something of form chrom:start-end into pieces.
 * if db != NULL then check with chromInfo for names */

boolean hgIsChromRange(char *db, char *spec);
/* Returns TRUE if spec is chrom:N-M for some human
 * chromosome chrom and some N and M. */

struct trackDb *hMaybeTrackInfo(struct sqlConnection *conn, char *trackName);
/* Load trackDb object for a track. If track is composite, its subtracks
 * will also be loaded and inheritance will be handled; if track is a
 * subtrack then inheritance will be handled.  (Unless a subtrack has
 * "noInherit on"...) Don't die if conn has no trackDb table.  Return NULL
 * if trackName is not found. */

struct trackDb *hTrackInfo(struct sqlConnection *conn, char *trackName);
/* Look up track in database, errAbort if it's not there. */

boolean hTrackCanPack(char *db, char *trackName);
/* Return TRUE if this track can be packed. */

bool hTrackIsSubtrack(char *db, char *trackName);
/* Return TRUE if this track is a subtrack. */

char *hGetTrackForTable(char *db, char *table);
/* Given a table name, get first track associated with it. */

char *hGetTableForTrack(char *db, char *track);
/* Given a track name, get table associated with it. */

char *hTrackOpenVis(char *db, char *trackName);
/* Return "pack" if track is packable, otherwise "full". */

struct dbDb *hGetIndexedDatabases(void);
/* Get list of all active databases.
 * Dispose of this with dbDbFreeList. */

struct dbDb *hGetIndexedDatabasesForClade(char *db);
/* Get list of active databases in db's clade.
 * Dispose of this with dbDbFreeList. */

struct slPair *hGetCladeOptions();
/* Return a list of slPairs, each containing clade menu value (hgcentral.clade.name, e.g. 'mammal')
 * and clade menu label (hgcentral.clade.label, e.g. 'Mammal'),
 * useful for constructing a clade menu. */

struct slPair *hGetGenomeOptionsForClade(char *clade);
/* Return a list of slPairs, each containing genome menu value and menu label,
 * useful for constructing a genome menu for the given clade. */

struct slPair *hGetDbOptionsForGenome(char *genome);
/* Return a list of slPairs, each containing db menu value and menu label,
 * useful for constructing an assembly menu for the given genome. */

struct slName *hLiftOverFromDbs(void);
/* Return a list of names of the DBs in the
 * fromDb column of the liftOverChain.*/

struct slName *hLiftOverToDbs(char *fromDb);
/* Return a list of names of the DBs in the
 * toDb column of the liftOverChain.
 * If fromDb!=NULL, return only those with that
 * fromDb. */

struct slName *hLiftOverFromOrgs(void);
/* Return a list of names of organisms that
 * have databases in the fromDb column of
 * liftOverChain.*/

struct slName *hLiftOverToOrgs(char *fromDb);
/* Return a list of names of the organisms with
 * databases in the toDb column of the liftOverChain.
 * If fromDb!=NULL, return only those with that
 * fromDb. */

struct hash *hGetDatabaseRank(void);
/* Get list of databases and make a hash of order rank
 * Dispose of this with hashFree. */

struct dbDb *hGetLiftOverFromDatabases(void);
/* Get list of databases for which there is at least one liftOver chain file
 * Dispose of this with dbDbFreeList. */

struct dbDb *hGetLiftOverToDatabases(char *fromDb);
/* Get list of databases for which there are liftOver chain files
 * to convert from the fromDb assembly.
 * Dispose of this with dbDbFreeList. */

struct dbDb *hGetBlatIndexedDatabases(void);
/* Get list of databases for which there is a BLAT index.
 * Dispose of this with dbDbFreeList. */

boolean hIsBlatIndexedDatabase(char *db);
/* Return TRUE if have a BLAT server on sequence corresponding
 * to give database. */

struct blatServerTable *hFindBlatServer(char *db, boolean isTrans);
/* return the blat server information corresponding to the database */

char *hDefaultPos(char *database);
/* param database - The database within which to look up this position.
   return - default chromosome position associated with database.
    Use freeMem on return value when done.
 */

char *hOrganism(char *database);
/* Return organism associated with database.   Use freeMem on
 * return value when done. */

int hOrganismID(char *database);
/* Get organism ID from relational organism table */
/* Return 0 if not found. */

char *hScientificName(char *database);
/* Return scientific name for organism represented by this database */
/* Return NULL if unknown database */
/* NOTE: must free returned string after use */

char *hOrgShortName(char *org);
/* Get the short name for an organism.  Returns NULL if org is NULL.
 * WARNING: static return */

char *hOrgShortForDb(char *db);
/* look up the short organism scientific name given an organism db.
 * WARNING: static return */


char *hHtmlPath(char *database);
/* Return /gbdb path name to html description for this database */
/* Return NULL if unknown database */
/* NOTE: must free returned string after use */

char *hFreezeDate(char *database);
/* Return freeze date of database. Use freeMem when done. */

char *hFreezeDateOpt(char *database);
/* Return freeze date of database or NULL if unknown database
 *  Use freeMem when done. */

int hTaxId(char *database);
/* Return taxId (NCBI Taxonomy ID) associated with database. */

char *hGenome(char *database);
/* Return genome associated with database.   Use freeMem on
 * return value when done. */

char *hPreviousAssembly(char *database);
/* Return previous assembly for the genome associated with database. */

boolean hGotClade(void);
/* Return TRUE if central db contains clade info tables. */

char *hClade(char *genome);
/* If central database has clade tables, return the clade for the
 * given genome; otherwise return NULL. */

char *hDefaultDbForGenome(char *genome);
/*
Purpose: Return the default database matching the organism.

param organism - The organism for which we are trying to get the
    default database.
return - The default database name for this organism
 */

char *hDefaultGenomeForClade(char *clade);
/* Return highest relative priority genome for clade. */

char *sqlGetField(char *db, char *tblName, char *fldName,
  	          char *condition);
/* Return a single field from the database, table name, field name, and a
   condition string */

struct hash *hChromSizeHash(char *db);
/* Get hash of chromosome sizes for database.  Just hashFree it when done. */

struct hash *hChromSizeHashFromFile(char *fileName);
/* Get hash of chromosome sizes from 2- or 3-column chrom.sizes file. hashFree when done. */

struct slName *hChromList(char *db);
/* Get the list of chrom names from the database's chromInfo table. */

struct mafAli *mafLoadInRegion(struct sqlConnection *conn, char *table,
	char *chrom, int start, int end);
/* Return list of alignments in region. */

struct mafAli *axtLoadAsMafInRegion(struct sqlConnection *conn, char *table,
	char *chrom, int start, int end,
	char *tPrefix, char *qPrefix, int tSize,  struct hash *qSizeHash);
/* Return list of alignments in region from axt external file as a maf. */

char *hgDirForOrg(char *org);
/* Make directory name from organism name - getting
 * rid of dots and spaces. */

struct hash *hgReadRa(char *genome, char *database, char *rootDir,
	char *rootName, struct hash **retHashOfHash);
/* Read in ra in root, root/org, and root/org/database.
 * Returns a list of hashes, one for each ra record.  Optionally
 * if retHashOfHash is non-null it returns there a
 * a hash of hashes keyed by the name field in each
 * ra sub-hash. */

char *addCommasToPos(char *db, char *position);
/* add commas to the numbers in a position
 * returns pointer to static */

boolean parsePosition(char *position, char **retChrom, uint *retStart, uint *retEnd);
/* If position is word:number-number (possibly with commas & whitespace),
 * set retChrom, retStart (subtracting 1) and retEnd, and return TRUE.
 * Otherwise return FALSE and leave rets unchanged. */

struct grp* hLoadGrps(char *db);
/* load the grp tables using the list configured in hg.conf, returning a list
 * sorted by priority. */

void setMinIndexLengthForTrashCleaner();
/* set the minimum index size so trash cleaner will not die
 * on custom tracks on hubs that are not currently loading.
 * However, they might be re-established in the future,
 * and we want to allow it to proceed without failure
 * in order to touch the trash database table access times,
 * preserving them from the trash cleaner.
 * Only the trash cleaner should call this:
 *  src/hg/utils/refreshNamedCustomTracks/refreshNamedCustomTracks.c 
 */

int hGetMinIndexLength(char *db);
/* get the minimum index size for the given database that won't smoosh
 * together chromNames such that any group of smooshed entries has a
 * cumulative size greater than the the largest chromosome.  Allow one
 * exception cuz we're nice
 */

int chrStrippedCmp(char *chrA, char *chrB);
/*	compare chrom names after stripping chr, Scaffold_ or ps_ prefix */

int chrNameCmp(char *str1, char *str2);
/* Compare chromosome names by number, then suffix.  str1 and str2 must
 * match the regex "chr([0-9]+|[A-Za-z0-9]+)(_[A-Za-z0-9_]+)?". */

int chrSlNameCmp(const void *el1, const void *el2);
/* Compare chromosome names by number, then suffix.  el1 and el2 must be
 * slName **s (as passed in by slSort) whose names match the regex
 * "chr([0-9]+|[A-Za-z0-9]+)(_[A-Za-z0-9_]+)?". */

int chrNameCmpWithAltRandom(char *str1, char *str2);
/* Compare chromosome or linkage group names str1 and str2
 * to achieve this order:
 * chr1 .. chr22
 * chrX
 * chrY
 * chrM
 * chr1_{alt,fix,hap*,random} .. chr22_{alt,fix,hap*,random}
 * chrUns
 */

int chrSlNameCmpWithAltRandom(const void *el1, const void *el2);
/* Compare chromosome or linkage group names str1 and str2 
 * to achieve this order:
 * chr1 .. chr22
 * chrX
 * chrY
 * chrM
 * chr1_{alt,fix,hap*,random} .. chr22_{alt,fix,hap*,random}
 * chrUn*
 */

int bedCmpExtendedChr(const void *va, const void *vb);
/* Compare to sort based on chrom,chromStart.  Use extended
 * chrom name comparison, that strip prefixes and does numeric compare */

int compareDbs(char *dbA, char *dbB);
/* Compare two org# e.g. mm6 vs. mm16 or mm6 vs. hg17
 * Return > 0 if dbA > dbB, < 0 if less than, and 0 if equal */

int getTableSize(char *db, char *table);
/* Get count of rows in a table in the primary database */

struct slName *getDomainList(struct sqlConnection *conn, char *ucscGeneId,
       char *domainDb);

struct slName *getPfamDomainList(struct sqlConnection *conn, char *ucscGeneId);

boolean isUnknownChrom(char *dataBase, char *chromName);
/* Return true if chrom is one of our "unknown" chromomsomes (e.g. chrUn). */

char *hGenbankModDate(char *acc, struct sqlConnection *conn);
/* Get string for genbank last modification date, or NULL if not found..
 * Free resulting string. */

struct trackDb *findTdbForTable(char *db,struct trackDb *parent,char *table, struct customTrack *(*ctLookupName)(char *table));
/* Find or creates the tdb for this table.  Might return NULL! (e.g. all tables)
 * If this is a custom track, pass in function ctLookupName(table) which looks up a
 * custom track by name, otherwise pass NULL
 */

char *findTypeForTable(char *db,struct trackDb *parent,char *table, struct customTrack *(*ctLookupName)(char *table));
/* Finds the TrackType for this Table */

boolean trackIsType(char *database, char *table, struct trackDb *parent, char *type, struct customTrack *(*ctLookupName)(char *table));
/* Return TRUE track is a specific type.  Type should be something like "bed" or
 * "bigBed" or "bigWig"
 * if table has no parent trackDb pass NULL for parent
 * If this is a custom track, pass in function ctLookupName(table) which looks up a
 * custom track by name, otherwise pass NULL
 */

boolean hIsBigWig(char *database, char *table, struct trackDb *parent, struct customTrack *(*ctLookupName)(char *table));
/* Return TRUE if table corresponds to a bigWig file.
 * if table has no parent trackDb pass NULL for parent
 * If this is a custom track, pass in function ctLookupName(table) which looks up a
 * custom track by name, otherwise pass NULL
 */

boolean hIsBigBed(char *database, char *table, struct trackDb *parent, struct customTrack *(*ctLookupName)(char *table));
/* Return TRUE if table corresponds to a bigBed file.
 * if table has no parent trackDb pass NULL for parent
 * If this is a custom track, pass in function ctLookupName(table) which looks up a
 * custom track by name, otherwise pass NULL
 */

char *bbiNameFromSettingOrTable(struct trackDb *tdb, struct sqlConnection *conn, char *table);
/* Return file name from bigDataUrl or little table. */

char *bbiNameFromSettingOrTableChrom(struct trackDb *tdb, struct sqlConnection *conn, char *table,
				     char *seqName);
/* Return file name from bigDataUrl or little table that might have a seqName column.
 * If table does have a seqName column, return NULL if there is no file for seqName. */

#define dbdDbTableConfVariable  "dbDbTableName"
#define defaultDbdDbTableName  "dbDb"
#define defaultDbTableConfVariable  "defaultDbTableName"
#define defaultDefaultDbTableName  "defaultDb"
#define genomeCladeTableConfVariable  "genomeCladeTableName"
#define defaultGenomeCladeTableName  "genomeClade"
#define cladeTableConfVariable  "cladeTableName"
#define defaultCladeTableName  "clade"

char *dbDbTable();
/* Return the name of the dbDb table. */

char *cladeTable();
/* Return the name of the clade table. */

char *defaultDbTable();
/* Return the name of the defaultDb table. */

char *genomeCladeTable();
/* Return the name of the genomeClade table. */

struct trackDb *hFindLatestSnpTrack(char *db, char *suffix, struct trackDb **pFullTrackList);
/* Return the 'snpNNN<suffix>' track with the highest build number, if any.
 * suffix may be NULL to get the 'All SNPs' table (as opposed to Common, Flagged, Mult). */

char *hFindLatestSnpTableConn(struct sqlConnection *conn, char *suffix);
/* Return the name of the 'snp1__<suffix>' table with the highest build number, if any.
 * suffix may be NULL to get the 'All SNPs' table (as opposed to Common, Flagged, Mult). */

char *hFindLatestGencodeTableConn(struct sqlConnection *conn, char *suffix);
/* Return the 'wgEncodeGencode<suffix>V<version>' table with the highest version number, if any.
 * If suffix is NULL, it defaults to Basic. */

boolean hDbHasNcbiRefSeq(char *db);
/* Return TRUE if db has NCBI's RefSeq alignments and annotations. */

char *hRefSeqAccForChrom(char *db, char *chrom);
/* Return the RefSeq NC_000... accession for chrom if we can find it, else just chrom.
 * db must never change. */

#endif /* HDB_H */
