/* hgTables - shared data between hgTables modules. */

#ifndef HGTABLES_H
#define HGTABLES_H


#ifndef JKSQL_H
#include "jksql.h"
#endif

#ifndef LOCALMEM_H
#include "localmem.h"
#endif

#ifndef DYSTRING_H
#include "dystring.h"
#endif

#ifndef HDB_H
#include "hdb.h"
#endif

#ifndef HCOMMON_H
#include "hCommon.h"
#endif

#ifndef GRP_H
#include "grp.h"
#endif

#ifndef JOINER_H
#include "joiner.h"
#endif
struct region
/* A part (or all) of a chromosome. */
    {
    struct region *next;
    char *chrom;		/* Chromosome. */
    int start;			/* Zero-based. */
    int end;			/* Non-inclusive.  If zero means full chromosome. */
    };

/* Global variables - generally set during initialization and then read-only. */
extern struct cart *cart;	/* This holds cgi and other variables between clicks. */
extern struct hash *oldVars;	/* The cart before new cgi stuff added. */
extern char *genome;		/* Name of genome - mouse, human, etc. */
extern char *dbDatabase;	/* Genome database, from db variable. */
extern char *database;		/* Current database, often but not always dbDatabase. */
extern char *freezeName;	/* Date of assembly. */
extern struct trackDb *fullTrackList;	/* List of all tracks in database. */
extern struct trackDb *curTrack;	/* Currently selected track. */
extern struct customTrack *theCtList;	/* List of custom tracks. */
extern char *curTable;	/* Current selected table. */
struct joiner *allJoiner;	/* Info on how to join tables. */

/* --------------- HTML Helpers ----------------- */

void hPrintf(char *format, ...);
/* Print out some html.  Check for write error so we can
 * terminate if http connection breaks. */

void hPrintSpaces(int count);
/* Print a number of non-breaking spaces. */

void htmlOpen(char *format, ...);
/* Start up a page that will be in html format. */

void htmlClose();
/* Close down html format page. */

void textOpen();
/* Start up page in text format. (No need to close this). */

void explainWhyNoResults();
/* Put up a little explanation to user of why they got nothing. */

char *curTableLabel();
/* Return label for current table - track short label if it's a track,
 * otherwise track name. */

/* ---------- Javascript stuff. ----------------------*/

void jsCreateHiddenForm(char **vars, int varCount);
/* Create a hidden form with the given variables */

void jsWriteFunctions();
/* Write out Javascript functions. */

char *jsOnChangeEnd(struct dyString **pDy);
/* Finish up javascript onChange command. */

void jsDropDownCarryOver(struct dyString *dy, char *var);
/* Add statement to carry-over drop-down item to dy. */

void jsTextCarryOver(struct dyString *dy, char *var);
/* Add statement to carry-over text item to dy. */

void jsTrackingVar(char *jsVar, char *val);
/* Emit a little Javascript to keep track of a variable. 
 * This helps especially with radio buttons. */

void jsTrackedVarCarryOver(struct dyString *dy, char *cgiVar, char *jsVar);
/* Carry over tracked variable (radio button?) to hidden form. */

void jsMakeTrackingRadioButton(char *cgiVar, char *jsVar, 
	char *val, char *selVal);
/* Make a radio button that also sets tracking variable
 * in javascript. */

void jsMakeTrackingCheckBox(char *cgiVar, char *jsVar, boolean usualVal);
/* Make a check box filling in with existing value and
 * putting a javascript tracking variable on it. */

char *jsOnRangeChange(char *cgiVar, char *jsVar, char *val);
/* Make a little javascript to set the range radio button when
 * they type in the range text box. */

/* ---------- Other UI stuff. ----------------------*/

boolean varOn(char *var);
/* Return TRUE if variable exists and is set. */

void printMainHelp();
/* Put up main page help info. */

struct grp *showGroupField(char *groupVar, char *groupScript,
    struct sqlConnection *conn, boolean allTablesOk);
/* Show group control. Returns selected group. */

struct trackDb *showTrackField(struct grp *selGroup,
	char *trackVar, char *trackScript);
/* Show track control. Returns selected track. */

/* --------- Utility functions --------------------- */

struct region *getRegions();
/* Consult cart to get list of regions to work on. */

char *getRegionName();
/* Get a name for selected region.  Don't free this. */

struct region *getRegionsWithChromEnds();
/* Get list of regions.  End field is set to chrom size rather
 * than zero for full chromosomes. */

void regionFillInChromEnds(struct region *regionList);
/* Fill in end fields if set to zero to be whole chrom. */

boolean fullGenomeRegion();
/* Return TRUE if region is full genome. */

struct sqlResult *regionQuery(struct sqlConnection *conn, char *table,
	char *fields, struct region *region, boolean isPositional,
	char *extraWhere);
/* Construct and execute query for table on region. */

void dbOverrideFromTable(char buf[256], char **pDb, char **pTable);
/* If *pTable includes database, overrider *pDb with it, using
 * buf to hold string. */

struct grp *makeGroupList(struct sqlConnection *conn, 
	struct trackDb *trackList, boolean allTablesOk);
/* Get list of groups that actually have something in them. */

struct grp *findSelectedGroup(struct grp *groupList, char *cgiVar);
/* Find user-selected group if possible.  If not then
 * go to various levels of defaults. */

struct slName *tablesForTrack(struct trackDb *track);
/* Return list of all tables associated with track. */

struct trackDb *findSelectedTrack(struct trackDb *trackList, 
	struct grp *group, char *varName);
/* Find selected track - from CGI variable if possible, else
 * via various defaults. */

struct customTrack *getCustomTracks();
/* Get custom track list. */

struct trackDb *findTrack(char *name, struct trackDb *trackList);
/* Find track, or return NULL if can't find it. */

struct trackDb *mustFindTrack(char *name, struct trackDb *trackList);
/* Find track or squawk and die. */

struct trackDb *findTrackInGroup(char *name, struct trackDb *trackList, 
	struct grp *group);
/* Find named track that is in group (NULL for any group).
 * Return NULL if can't find it. */

struct trackDb *findSelectedTrack(struct trackDb *trackList, 
	struct grp *group, char *varName);
/* Find selected track - from CGI variable if possible, else
 * via various defaults. */

struct asObject *asForTable(struct sqlConnection *conn, char *table);
/* Get autoSQL description if any associated with table. */

struct asColumn *asColumnFind(struct asObject *asObj, char *name);
/* Return named column. */

char *connectingTableForTrack(char *rawTable);
/* Return table name to use with all.joiner for track. 
 * You can freeMem this when done. */

char *chromTable(struct sqlConnection *conn, char *table);
/* Get chr1_table if it exists, otherwise table. 
 * You can freeMem this when done. */

char *chrnTable(struct sqlConnection *conn, char *table);
/* Return chrN_table if table is split, otherwise table. 
 * You can freeMem this when done. */


void doTabOutTable(char *database, char *table, 
	struct sqlConnection *conn, char *fields);
/* Do tab-separated output on table. */

struct bed *getFilteredBeds(struct sqlConnection *conn,
	char *table, struct region *region, struct lm *lm);
/* Get list of beds on single region that pass filtering. */

struct bed *getAllFilteredBeds(struct sqlConnection *conn, 
	char *table, struct lm *lm);
/* getAllFilteredBeds - get list of beds in selected regions 
 * that pass filtering. */

void bedSqlFieldsExceptForChrom(struct hTableInfo *hti,
	int *retFieldCount, char **retFields);
/* Given tableInfo figure out what fields are needed to
 * add to a database query to have information to create
 * a bed. The chromosome is not one of these fields - we
 * assume that is already known since we're processing one
 * chromosome at a time.   Return a comma separated (no last
 * comma) list of fields that can be freeMem'd, and the count
 * of fields (this *including* the chromosome). */

struct bed *bedFromRow(
	char *chrom, 		  /* Chromosome bed is on. */
	char **row,  		  /* Row with other data for bed. */
	int fieldCount,		  /* Number of fields in final bed. */
	boolean isPsl, 		  /* True if in PSL format. */
	boolean isGenePred,	  /* True if in GenePred format. */
	boolean isBedWithBlocks,  /* True if BED with block list. */
	boolean *pslKnowIfProtein,/* Have we figured out if psl is protein? */
	boolean *pslIsProtein,    /* True if we know psl is protien. */
	struct lm *lm);		  /* Local memory pool */
/* Create bed from a database row when we already understand
 * the format pretty well.  The bed is allocated inside of
 * the local memory pool lm.  Generally use this in conjunction
 * with the results of a SQL query constructed with the aid
 * of the bedSqlFieldsExceptForChrom function. */

struct bed *getAllIntersectedBeds(struct sqlConnection *conn, 
	char *table, struct lm *lm);
/* Get list of beds in selected regions that pass intersection
 * (and filtering). Do lmCleanup (not bedFreeList) when done. */

struct bed *cookedBedList(struct sqlConnection *conn,
	char *table, struct region *region, struct lm *lm);
/* Get data for track in region after all processing steps (filtering
 * intersecting etc.) in BED format. */

struct bed *cookedBedsOnRegions(struct sqlConnection *conn, 
	char *table, struct region *regionList, struct lm *lm);
/* Get cooked beds on all regions. */

struct hTableInfo *getHti(char *db, char *table);
/* Return primary table info. */

struct hTableInfo *maybeGetHti(char *db, char *table);
/* Return primary table info, but don't abort if table not there. */

boolean htiIsPositional(struct hTableInfo *hti);
/* Return TRUE if hti looks like it's from a positional table. */

boolean isPositional(char *db, char *table);
/* Return TRUE if it looks to be a positional table. */

boolean isSqlStringType(char *type);
/* Return TRUE if it a a stringish SQL type. */

boolean isSqlNumType(char *type);
/* Return TRUE if it is a numerical SQL type. */

/* ------------- Functions related to joining and filtering ------------*/
void tabOutSelectedFields(
	char *primaryDb,		/* The primary database. */
	char *primaryTable, 		/* The primary table. */
	struct slName *fieldList);	/* List of db.table.field */
/* Do tab-separated output on selected fields, which may
 * or may not include multiple tables. */

boolean anyFilter();
/* Return TRUE if any filter set. */

char *filterClause(char *db, char *table, char *chrom);
/* Get filter clause (something to put after 'where')
 * for table */

char *filterFieldVarName(char *db, char *table, char *field, char *type);
/* Return variable name for filter page. */

/* Some types of filter vars. */
#define filterDdVar "dd"
#define filterCmpVar "cmp"
#define filterPatternVar "pat"
#define filterRawLogicVar "rawLogic"
#define filterRawQueryVar "rawQuery"
#define filterDataValueVar "dataValue"
#define filterMaxOutputVar "maxOutput"

/* --------- Functions related to intersecting. --------------- */

boolean anyIntersection();
/* Return TRUE if there's an intersection to do. */


/* --------- CGI/Cart Variables --------------------- */

/* Command type variables - control which page is up.  Get stripped from
 * cart. */
#define hgtaDo "hgta_do" /* All commands start with this and are removed
	 		  * from cart. */
#define hgtaDoMainPage "hgta_doMainPage"
#define hgtaDoTopSubmit "hgta_doTopSubmit"
#define hgtaDoSummaryStats "hgta_doSummaryStats"
#define hgtaDoSchema "hgta_doSchema"
#define hgtaDoPasteIdentifiers "hgta_doPasteIdentifiers"
#define hgtaDoPastedIdentifiers "hgta_doPastedIdentiers"
#define hgtaDoUploadIdentifiers "hgta_doUploadIdentifiers"
#define hgtaDoClearIdentifiers "hgta_doClearIdentifiers"
#define hgtaDoFilterPage "hgta_doFilterPage"
#define hgtaDoFilterSubmit "hgta_doFilterSubmit"
#define hgtaDoFilterMore "hgta_doFilterMore"
#define hgtaDoClearFilter "hgta_doClearFilter"
#define hgtaDoIntersectPage "hgta_doIntersectPage"
#define hgtaDoClearIntersect "hgta_doClearIntersect"
#define hgtaDoIntersectMore "hgta_doIntersectMore"
#define hgtaDoIntersectSubmit "hgta_doIntersectSubmit"
#define hgtaDoTest "hgta_doTest"
#define hgtaDoSchemaTable "hgta_doSchemaTable"
#define hgtaDoSchemaDb "hgta_doSchemaDb"
#define hgtaDoValueHistogram "hgta_doValueHistogram"
#define hgtaDoValueRange "hgta_doValueRange"
#define hgtaDoPrintSelectedFields "hgta_doPrintSelectedFields"
#define hgtaDoSelectFieldsMore "hgta_doSelectFieldsMore"
#define hgtaDoClearAllFieldPrefix "hgta_doClearAllField."
#define hgtaDoSetAllFieldPrefix "hgta_doSetAllField."
#define hgtaDoGenePredSequence "hgta_doGenePredSequence"
#define hgtaDoGenomicDna "hgta_doGenomicDna"
#define hgtaDoGetBed "hgta_doGetBed"
#define hgtaDoGetCustomTrack "hgta_doGetCustomTrack"
#define hgtaDoGetCustomTrackFile "hgta_doGetCustomTrackFile"
#define hgtaDoGetGalaQuery "hgta_doGetGalaQuery"
#define hgtaDoAllGalaQuery "hgta_doAllGalaQuery"
#define hgtaDoLookupPosition "hgta_doLookupPosition"

/* Other CGI variables. */
#define hgtaGroup "hgta_group"
#define hgtaTrack "hgta_track"
#define hgtaSelDb "hgta_selDb"
#define hgtaRegionType "hgta_regionType"
#define hgtaRange "position"
#define hgtaOffsetStart "hgta_offsetStart"
#define hgtaOffsetEnd "hgta_offsetEnd"
#define hgtaOffsetRelativeTo "hgta_offsetRelativeTo"
#define hgtaOutputType "hgta_outputType"
#define hgtaOutputPad "hgta_outputPad"
#define hgtaOutFileName "hgta_outFileName"
#define hgtaDatabase "hgta_database"  /* In most cases use "db" */
#define hgtaTable "hgta_table"
#define hgtaHistoTable "hgta_histoTable"
#define hgtaPastedIdentifiers "hgta_pastedIdentifiers"
#define hgtaIdentifierFile "hgta_identifierFile"
#define hgtaFilterTable "hgta_filterTable"
#define hgtaGeneSeqType "hgta_geneSeqType"
#define hgtaPrintCustomTrackHeaders "hgta_printCustomTrackHeaders"
#define hgtaCtName "hgta_ctName"
#define hgtaCtDesc "hgta_ctDesc"
#define hgtaCtVis "hgta_ctVis"
#define hgtaCtUrl "hgta_ctUrl"
#define hgtaCtWigOutType "hgta_ctWigOutType"

   /* These intersection page vars come in pairs so we can cancel. */
#define hgtaIntersectGroup "hgta_intersectGroup"
#define hgtaNextIntersectGroup "hgta_nextIntersectGroup"
#define hgtaIntersectTrack "hgta_intersectTrack"
#define hgtaNextIntersectTrack "hgta_nextIntersectTrack"
#define hgtaIntersectOp "hgta_intersectOp"
#define hgtaNextIntersectOp "hgta_nextIntersectOp"
#define hgtaMoreThreshold "hgta_moreThreshold"
#define hgtaNextMoreThreshold "hgta_nextMoreThreshold"
#define hgtaLessThreshold "hgta_lessThreshold"
#define hgtaNextLessThreshold "hgta_nextLessThreshold"
#define hgtaInvertTable "hgta_invertTable"
#define hgtaNextInvertTable "hgta_nextInvertTable"
#define hgtaInvertTable2 "hgta_invertTable2"
#define hgtaNextInvertTable2 "hgta_nextInvertTable2"

/* Prefix for variables managed by field selector. */
#define hgtaFieldSelectPrefix "hgta_fs."
/* Prefix for variables managed by filter page. */
#define hgtaFilterPrefix "hgta_fil."
/* Prefix for variables containing filters themselves. */
#define hgtaFilterVarPrefix hgtaFilterPrefix "v."
/* Prefix for temp name files created for custom tracks */
#define hgtaCtTempNamePrefix "hgtct"

/* Output types. */
#define outPrimaryTable "primaryTable"
#define outSequence "sequence"
#define outSelectedFields "selectedFields"
#define outSchema "schema"
#define outSummaryStats "stats"
#define outBed "bed"
#define outGff "gff"
#define outCustomTrack "customTrack"
#define outHyperlinks "hyperlinks"
#define outWigData "wigData"
#define outWigBed "wigBed"
#define outGala "galaQuery"
#define outMaf "maf"

/* --------- Identifier list handling stuff. ------------ */

char *identifierFileName();
/* File name identifiers are in, or NULL if no such file. */

struct hash *identifierHash();
/* Return hash full of identifiers. */

char *getIdField(char *db, struct trackDb *track, char *table, 
	struct hTableInfo *hti);
/* Get ID field for table, or NULL if none.  FreeMem result when done */

/* --------- Summary and stats stuff -------------- */
long long basesInRegion(struct region *regionList);
/* Count up all bases in regions. */

long long gapsInRegion(struct sqlConnection *conn, struct region *regionList);
/* Return count of gaps in all regions. */

void percentStatRow(char *label, long long p, long long q);
/* Print label, p, and p/q */

void numberStatRow(char *label, long long x);
/* Print label, x in table. */

void floatStatRow(char *label, double x);
/* Print label, x in table. */

void stringStatRow(char *label, char *val);
/* Print label, string value. */

/* ----------- Maf stuff in maf.c ------------------------------*/

boolean isMafTable(char *database, struct trackDb *track, char *table);
/* Return TRUE if table is maf. */

void doOutMaf(struct trackDb *track, char *table, struct sqlConnection *conn);
/* Output regions as MAF. */

/* ----------- Wiggle business in wiggle.c -------------------- */

boolean isWiggle(char *db, char *table);
/* Return TRUE if db.table is a wiggle. */

struct bed *getWiggleAsBed(
    char *db, char *table, 	/* Database and table. */
    struct region *region,  /* Region to get data for. */
    char *filter, 		/* Filter to add to SQL where clause if any. */
    struct hash *idHash, 	/* Restrict to id's in this hash if non-NULL. */
    struct lm *lm,		/* Where to allocate memory. */
    struct sqlConnection *conn);	/* SQL connection to work with */
/* Return a bed list of all items in the given range in table.
 * Cleanup result via lmCleanup(&lm) rather than bedFreeList.  */

struct wigAsciiData *getWiggleAsData(struct sqlConnection *conn, char *table,
	struct region *region, struct lm *lm);
/*	return the wigAsciiData list	*/

void doOutWigBed(struct trackDb *track, char *table, struct sqlConnection *conn);
/* Return wiggle data in bed format. */

void doOutWigData(struct trackDb *track, char *table, struct sqlConnection *conn);
/* Return wiggle data in variableStep format. */

void doSummaryStatsWiggle(struct sqlConnection *conn);
/* Put up page showing summary stats for wiggle track. */

/* ----------- Custom track stuff. -------------- */
boolean isCustomTrack(char *table);
/* Return TRUE if table is a custom track. */

struct customTrack *getCustomTracks();
/* Get custom track list. */

struct slName *getBedFields(int fieldCount);
/* Get list of fields for bed of given size. */

struct bed *customTrackGetFilteredBeds(char *name, struct region *regionList,
	struct lm *lm, boolean *retGotFilter, boolean *retGotIds);
/* Get list of beds from custom track of given name that are
 * in current regions and that pass filters.  You can bedFree
 * this when done.  
 * If you pass in a non-null retGotFilter this will let you know
 * if a filter was applied.  Similarly retGotIds lets you know
 * if an identifier list was applied*/

struct customTrack *lookupCt(char *name);
/* Find named custom track. */

struct customTrack *newCt(char *ctName, char *ctDesc, int visNum, char *ctUrl,
			  int fields);
/* Make a new custom track record for the query results. */

struct hTableInfo *ctToHti(struct customTrack *ct);
/* Create an hTableInfo from a customTrack. */

void doTabOutCustomTracks(struct trackDb *track, struct sqlConnection *conn,
	char *fields);
/* Print out selected fields from custom track.  If fields
 * is NULL, then print out all fields. */

/* ----------- Page displayers -------------- */

void doMainPage(struct sqlConnection *conn);
/* Put up the first page user sees. */

void mainPageAfterOpen(struct sqlConnection *conn);
/* Put up main page assuming htmlOpen()/htmlClose()
 * will happen in calling routine. */

void doTest();
/* Put up a page to see what happens. */

void doTableSchema(char *db, char *table, struct sqlConnection *conn);
/* Show schema around table. */

void doSchema(struct sqlConnection *conn);
/* Show schema around current track. */

void doValueHistogram(char *field);
/* Put up value histogram. */

void doValueRange(char *field);
/* Put up value histogram. */

void doPasteIdentifiers(struct sqlConnection *conn);
/* Respond to paste identifiers button. */

void doPastedIdentifiers(struct sqlConnection *conn);
/* Process submit in past identifiers page. */

void doUploadIdentifiers(struct sqlConnection *conn);
/* Respond to upload identifiers button. */

void doClearIdentifiers(struct sqlConnection *conn);
/* Respond to clear identifiers button. */

void doPrintSelectedFields();
/* Actually produce selected field output as text stream. */

void doSelectFieldsMore();
/* Continue with select fields dialog. */

void doClearAllField(char *dbTable);
/* Clear all checks by fields in db.table. */

void doSetAllField(char *dbTable);
/* Set all checks by fields in db.table. */

void doOutSelectedFields(char *table, struct sqlConnection *conn);
/* Put up select fields (for tab-separated output) page. */

void doOutSequence(struct sqlConnection *conn);
/* Output sequence page. */

void doOutBed(char *table, struct sqlConnection *conn);
/* Put up form to select BED output format. */

void doOutHyperlinks(char *table, struct sqlConnection *conn);
/* Output as genome browser hyperlinks. */

void doOutGff(char *table, struct sqlConnection *conn);
/* Save as GFF. */

void doOutCustomTrack(char *table, struct sqlConnection *conn);
/* Put up form to select Custom Track output format. */

void doOutGalaQuery(struct trackDb *track, char *table, struct sqlConnection *conn);
/* Put up form to select GALA query output format. */

void doSummaryStats(struct sqlConnection *conn);
/* Put up page showing summary stats for track. */

void doFilterPage(struct sqlConnection *conn);
/* Respond to filter create/edit button */

void doFilterMore(struct sqlConnection *conn);
/* Continue with Filter Page. */

void doFilterSubmit(struct sqlConnection *conn);
/* Respond to submit on filters page. */

void doClearFilter(struct sqlConnection *conn);
/* Respond to click on clear filter. */

void doIntersectPage(struct sqlConnection *conn);
/* Respond to intersect create/edit button */

void doClearIntersect(struct sqlConnection *conn);
/* Respond to click on clear intersection. */

void doIntersectMore(struct sqlConnection *conn);
/* Continue working in intersect page. */

void doIntersectSubmit(struct sqlConnection *conn);
/* Finish working in intersect page. */

void doGenePredSequence(struct sqlConnection *conn);
/* Output genePred sequence. */

void doGenomicDna(struct sqlConnection *conn);
/* Get genomic sequence (UI has already told us how). */

void doGetBed(struct sqlConnection *conn);
/* Get BED output (UI has already told us how). */

void doGetCustomTrack(struct sqlConnection *conn);
/* Get Custom Track output (UI has already told us how). */

void doGetCustomTrackFile(struct sqlConnection *conn);
/* Get Custom Track file output (UI has already told us how). */

/* --------------- GALA functions --------------- */
#define galaCmdBufferSize 160

void doGetGalaQuery(struct sqlConnection *conn);
/* Get GALA query output */

void doAllGalaQuery(struct sqlConnection *conn);
/* Run GALA generated query in table browser, return results to GALA */

boolean galaAvail(char *db);
/* Check to see if GALA is available for this freeze */

#endif /* HGTABLES_H */


