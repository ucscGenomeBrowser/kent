/* hgTables - shared data between hgTables modules. */

#ifndef HGTABLES_H
#define HGTABLES_H

#ifndef JKSQL_H
#include "jksql.h"
#endif

#ifndef DYSTRING_H
#include "dystring.h"
#endif

#ifndef GRP_H
#include "grp.h"
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
extern char *database;		/* Name of genome database - hg15, mm3, or the like. */
extern char *freezeName;	/* Date of assembly. */
extern struct trackDb *fullTrackList;	/* List of all tracks in database. */
extern struct trackDb *curTrack;	/* Currently selected track. */
extern struct customTrack *theCtList;	/* List of custom tracks. */

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

void hTableStart();
/* For some reason BORDER=1 does not work in our web.c nested table scheme.
 * So use web.c's trick of using an enclosing table to provide a border.   */

void hTableEnd();
/* Close out table started with hTableStart() */

/* ---------- Javascript stuff. ----------------------*/

void jsCreateHiddenForm(char **vars, int varCount);
/* Create a hidden form with the given variables */

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

/* ---------- Other UI stuff. ----------------------*/

boolean varOn(char *var);
/* Return TRUE if variable exists and is set. */

void printMainHelp();
/* Put up main page help info. */

struct trackDb *showGroupTrackRow(char *groupVar, char *groupScript,
    char *trackVar, char *trackScript, struct sqlConnection *conn);
/* Show group & track row of controls.  Returns selected track */


/* --------- Utility functions --------------------- */

struct region *getRegions();
/* Consult cart to get list of regions to work on. */

struct region *getRegionsWithChromEnds();
/* Get list of regions.  End field is set to chrom size rather
 * than zero for full chromosomes. */

boolean fullGenomeRegion();
/* Return TRUE if region is full genome. */

struct sqlResult *regionQuery(struct sqlConnection *conn, char *table,
	char *fields, struct region *region, boolean isPositional,
	char *extraWhere);
/* Construct and execute query for table on region. */

struct grp *makeGroupList(struct sqlConnection *conn, 
	struct trackDb *trackList);
/* Get list of groups that actually have something in them. */

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

char *connectingTableForTrack(struct trackDb *track);
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
	struct trackDb *track, struct region *region);
/* Get list of beds on single region that pass filtering. */

struct bed *getAllFilteredBeds(struct sqlConnection *conn, 
	struct trackDb *track);
/* getAllFilteredBeds - get list of beds in selected regions 
 * that pass filtering. */

struct bed *getAllIntersectedBeds(struct sqlConnection *conn, 
	struct trackDb *track);
/* get list of beds in selected regions that pass intersection
 * (and filtering). */

struct hTableInfo *getHti(char *db, char *table);
/* Return primary table info. */

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

char *filterClause(char *db, char *table);
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

/* Functions related to intersecting. */

boolean anyIntersection();
/* Return TRUE if there's an intersection to do. */

/* --------- CGI/Cart Variables --------------------- */

/* Command type variables - control which page is up.  Get stripped from
 * cart. */
#define hgtaDo "hgta_do" /* All commands start with this and are removed
	 		  * from cart. */
#define hgtaDoMainPage "hgta_doMainPage"
#define hgtaDoTopSubmit "hgta_doTopSubmit"
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
#define hgtaDoSchema "hgta_doSchema"
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

/* Other CGI variables. */
#define hgtaGroup "hgta_group"
#define hgtaTrack "hgta_track"
#define hgtaRegionType "hgta_regionType"
#define hgtaRange "hgta_range"
#define hgtaOffsetStart "hgta_offsetStart"
#define hgtaOffsetEnd "hgta_offsetEnd"
#define hgtaOffsetRelativeTo "hgta_offsetRelativeTo"
#define hgtaOutputType "hgta_outputType"
#define hgtaDatabase "hgta_database"  /* In most cases use "db" */
#define hgtaTable "hgta_table"
#define hgtaPastedIdentifiers "hgta_pastedIdentifiers"
#define hgtaIdentifierFile "hgta_identifierFile"
#define hgtaFilterOn "hgta_filterOn"
#define hgtaGeneSeqType "hgta_geneSeqType"
#define hgtaPrintCustomTrackHeaders "hgta_printCustomTrackHeaders"
#define hgtaCtName "hgta_ctName"
#define hgtaCtDesc "hgta_ctDesc"
#define hgtaCtVis "hgta_ctVis"
#define hgtaCtUrl "hgta_ctUrl"

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

/* Output types. */
#define outPrimaryTable "primaryTable"
#define outSequence "sequence"
#define outSelectedFields "selectedFields"
#define outSchema "schema"
#define outSummaryStats "stats"
#define outBed "bed"
#define outGff "gff"
#define outCustomTrack "customTrack"

/* Identifier list handling stuff. */

char *identifierFileName();
/* File name identifiers are in, or NULL if no such file. */

struct hash *identifierHash();
/* Return hash full of identifiers. */

/* ----------- Custom track stuff. -------------- */
boolean isCustomTrack(char *table);
/* Return TRUE if table is a custom track. */

struct customTrack *getCustomTracks();
/* Get custom track list. */

struct slName *getBedFields(int fieldCount);
/* Get list of fields for bed of given size. */

struct bed *customTrackGetFilteredBeds(char *name, struct region *regionList,
	boolean *retGotFilter, boolean *retGotIds);
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

void doTabOutCustomTracks(char *name, char *fields);
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

void doTrackSchema(struct trackDb *track, struct sqlConnection *conn);
/* Show schema around track. */

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

void doOutSelectedFields(struct trackDb *track, struct sqlConnection *conn);
/* Put up select fields (for tab-separated output) page. */

void doOutSequence(struct trackDb *track, struct sqlConnection *conn);
/* Output sequence page. */

void doOutBed(struct trackDb *track, struct sqlConnection *conn);
/* Put up form to select BED output format. */

void doOutGff(struct trackDb *track, struct sqlConnection *conn);
/* Save as GFF. */

void doOutCustomTrack(struct trackDb *track, struct sqlConnection *conn);
/* Put up form to select Custom Track output format. */

void doOutSummaryStats(struct trackDb *track, struct sqlConnection *conn);
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

#endif /* HGTABLES_H */


