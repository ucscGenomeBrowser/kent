/* hgTables - shared data between hgTables modules. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef HGTABLES_H
#define HGTABLES_H


#include "jksql.h"
#include "localmem.h"
#include "dystring.h"
#include "hdb.h"
#include "hCommon.h"
#include "customTrack.h"
#include "grp.h"
#include "joiner.h"
#include "hPrint.h"
#include "hui.h"

struct region
/* A part (or all) of a chromosome. */
    {
    struct region *next;
    char *chrom;		/* Chromosome. */
    int start;			/* Zero-based. */
    int end;			/* Non-inclusive. */
    boolean fullChrom;		/* If TRUE it's full chromosome. */
    char *name;			/* possibly the encode region name */
    };

/* Global variables - generally set during initialization and then read-only. */
extern struct cart *cart;	/* This holds cgi and other variables between clicks. */
extern struct hash *oldVars;	/* The cart before new cgi stuff added. */
extern char *genome;		/* Name of genome - mouse, human, etc. */
extern char *database;		/* Current database, often but not always dbDatabase. */
extern char *freezeName;	/* Date of assembly. */
extern struct trackDb *fullTrackList;	/* List of all tracks in database. */
extern struct hash *fullTableToTdbHash;        /* All tracks and subtracks keyed by ->table field. */
extern struct trackDb *curTrack;	/* Currently selected track. */
extern struct grp *fullGroupList;	/* List of all groups. */
extern struct grp *curGroup;	/* Currently selected group. */
extern struct customTrack *theCtList;	/* List of custom tracks. */
extern char *curTable;	/* Current selected table. */
extern struct joiner *allJoiner;	/* Info on how to join tables. */

#define GALAXY_URL_BASE "https://usegalaxy.org"
#define GALAXY_URL_APP GALAXY_URL_BASE "/tool_runner"

/* --------------- HTML Helpers ----------------- */

void hPrintSpaces(int count);
/* Print a number of non-breaking spaces. */

void writeHtmlCell(char *text);
/* Write out a cell in an HTML table, making text not too big,
 * and stripping html tags and breaking spaces.... */

void htmlOpen(char *format, ...)
/* Start up a page that will be in html format. */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
;

void htmlClose();
/* Close down html format page. */

void textOpen();
/* Start up page in text format. (No need to close this). */

void explainWhyNoResults(FILE *f);
/* Put up a little explanation to user of why they got nothing. */

char *curTableLabel();
/* Return label for current table - track short label if it's a track,
 * otherwise track name. */

char *getScriptName();
/* returns script name from environment or hardcoded for command line */

void printTrackHtml(struct trackDb *tdb);
/* If trackDb has html for table, print it out in a new section. */

/* ---------- Other UI stuff. ----------------------*/

boolean varOn(char *var);
/* Return TRUE if variable exists and is set. */

void printMainHelp();
/* Put up main page help info. */

void printDownloadLink(char *typeLabel, char *fileName);
/* print a link to the file, so the user can download it right here */

boolean printTypeHelpDesc(char *fileType);
/* print a little link to our help docs given a file type. Return true if file type is a big* file format. */

struct grp *showGroupField(char *groupVar, char *event, char *groupScript,
    struct sqlConnection *conn, boolean allTablesOk);
/* Show group control. Returns selected group. */

struct trackDb *showTrackField(struct grp *selGroup, char *trackVar, char *event, char *trackScript,
                               boolean disableNoGenome);
/* Show track control. Returns selected track. */

char *showTableField(struct trackDb *track, char *varName, boolean useJoiner);
/* Show table control and label. */

struct slName *getDbListForGenome();
/* Get list of selectable databases. */

char *findSelDb();
/* Find user selected database (as opposed to genome database). */

struct slName *tablesForDb(char *db);
/* Find tables associated with database. */

/* --------- Utility functions --------------------- */

void nbSpaces(int count);
/* Print some non-breaking spaces. */

boolean anyCompression();
/*  Check if any compressed file output has been requested */

void initGroupsTracksTables();
/* Get list of groups that actually have something in them. */

struct region *getRegions();
/* Consult cart to get list of regions to work on. */

boolean isRegionWholeGenome();
/* Return TRUE if the current region is the whole genome. */

struct region *getRegionsFullGenome();
/* Get a region list that covers all of each chromosome. */

char *getRegionName();
/* Get a name for selected region.  Don't free this. */

boolean fullGenomeRegion();
/* Return TRUE if region is full genome. */

boolean isNoGenomeDisabled(char *db, char *table);
/* Return TRUE if table (or a track with which it is associated) has 'tableBrowser noGenome'
 * and region is genome. */

void checkNoGenomeDisabled(char *db, char *table);
/* Before producing output, make sure that the URL hasn't been hacked to make a
 * genome-wide query on a noGenome table. */

struct sqlResult *regionQuery(struct sqlConnection *conn, char *table,
	char *fields, struct region *region, boolean isPositional,
	char *extraWhere);
/* Construct and execute query for table on region. */

void dbOverrideFromTable(char buf[256], char **pDb, char **pTable);
/* If *pTable includes database, overrider *pDb with it, using
 * buf to hold string. */

struct grp *findSelectedGroup(struct grp *groupList, char *cgiVar);
/* Find user-selected group if possible.  If not then
 * go to various levels of defaults. */

struct trackDb *findSelectedTrack(struct trackDb *trackList,
	struct grp *group, char *varName);
/* Find selected track - from CGI variable if possible, else
 * via various defaults. */

struct trackDb *findTrack(char *name, struct trackDb *trackList);
/* Find track, or return NULL if can't find it. */

struct trackDb *mustFindTrack(char *name, struct trackDb *trackList);
/* Find track or squawk and die. */

struct asObject *asForTableNoTdb(struct sqlConnection *conn, char *table);
/* Get autoSQL description if any associated with table. Don't try tdb */

struct asObject *asForTable(struct sqlConnection *conn, char *table);
/* Get autoSQL description if any associated with table. */

char *connectingTableForTrack(char *rawTable);
/* Return table name to use with all.joiner for track.
 * You can freeMem this when done. */

char *chromTable(struct sqlConnection *conn, char *table);
/* Get chr1_table if it exists, otherwise table.
 * You can freeMem this when done. */

char *getDbTable(char *db, char *table);
/* If table already contains its real database as a dot-prefix, then
 * return a clone of table; otherwise alloc and return db.table . */

void doTabOutTable(char *database, char *table, FILE *f,
	struct sqlConnection *conn, char *fields, char outSep);
/* Do tab-separated output on table. */

struct slName *fullTableFields(char *db, char *table);
/* Return list of fields in db.table.field format. */

struct bed *getFilteredBeds(struct sqlConnection *conn,
	char *table, struct region *region, struct lm *lm,
	int *retFieldCount);
/* Get list of beds on single region that pass filtering. */

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
	char *table, struct region *region, struct lm *lm,
	int *retFieldCount);
/* Get data for track in region after all processing steps (filtering
 * intersecting etc.) in BED format.  */

struct bed *cookedBedsOnRegions(struct sqlConnection *conn,
	char *table, struct region *regionList, struct lm *lm,
	int *retFieldCount);
/* Get cooked beds on all regions. */

struct hTableInfo *getHtiOnDb(char *db, char *table);
/* Return primary table info. */

struct hTableInfo *getHti(char *db, char *table, struct sqlConnection *conn);
/* Return primary table info. Conn should be open to db. */

struct hTableInfo *maybeGetHti(char *db, char *table, struct sqlConnection *conn);
/* Return primary table info, but don't abort if table not there. Conn should be open to db. */

struct hTableInfo *maybeGetHtiOnDb(char *db, char *table);
/* Return primary table info, but don't abort if table not there. */

boolean htiIsPositional(struct hTableInfo *hti);
/* Return TRUE if hti looks like it's from a positional table. */

boolean isPositional(char *db, char *table);
/* Return TRUE if it looks to be a positional table. */

boolean isSqlStringType(char *type);
/* Return TRUE if type is a stringish SQL type. */

boolean isSqlEnumType(char *type);
/* Return TRUE if type is an enum. */

boolean isSqlSetType(char *type);
/* Return TRUE if type is a set. */

boolean isSqlNumType(char *type);
/* Return TRUE if it is a numerical SQL type. */

boolean isSqlIntType(char *type);
/* Return TRUE if it is an integer SQL type. */

void sqlFieldTypeFree(struct sqlFieldType **pFt);
/* Free resources used by sqlFieldType */

void sqlFieldTypeFreeList(struct sqlFieldType **pList);
/* Free a list of dynamically allocated sqlFieldType's */

struct sqlFieldType *sqlListFieldsAndTypes(struct sqlConnection *conn, char *table);
/* Get list of fields including their names and types.  The type currently is just
 * a MySQL type string. */

/* ------------- Functions related to joining and filtering ------------*/
void sepOutSelectedFields(
	char *primaryDb,		/* The primary database. */
	char *primaryTable, 		/* The primary table. */
	FILE *f,                        /* file for output, null for stdout */
	struct slName *fieldList,	/* List of db.table.field */
    char outSep);               /* The separator for the output */
/* Do tab-separated output on selected fields, which may
 * or may not include multiple tables. */

boolean anyFilter();
/* Return TRUE if any filter set. */

struct joinerDtf *filteringTables();
/* Get list of tables we're filtering on as joinerDtf list (with
 * the field entry NULL). */

char *filterClause(char *db, char *table, char *chrom, char *extraClause);
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

boolean canIntersect(char *db, char *table);
/* Return true if table supports intersection, i.e. it exists and is positional. */

boolean anyIntersection();
/* Return TRUE if there's an intersection to do. */

char *intersectOp();
/* Get intersect op from CGI var and make sure it's ok. */

boolean intersectionIsBpWise();
/* Return TRUE if the intersection/union operation is base pair-wise. */

Bits *bitsForIntersectingTable(struct sqlConnection *conn, struct region *region,
	int chromSize, boolean isBpWise);
/* Get a bitmap that corresponds to the table we are intersecting with.
 * Consult CGI vars to figure out what table it is. */

boolean intersectOverlapFilter(char *op, double moreThresh, double lessThresh, double overlap);
/* Return TRUE if have enough (or not too much) overlap according to thresholds and op. */

/* --------- Functions related to correlation. --------------- */
boolean anyCorrelation();
/* Return TRUE if there's a correlation to do. */

/* variables managed by correlate page. */
#define hgtaCorrelateMaxLimitCount "hgta_corrMaxLimitCount"
#define hgtaCorrelateWinSize "hgta_corrWinSize"

boolean correlateTrackTableOK(struct trackDb *tdb, char *table);
/*	is this table OK to correlate with ?	*/

/* --------- Functions related to composite tracks/subtracks. ------------- */

boolean anySubtrackMerge(char *db, char *table);
/* Return TRUE if a subtrack merge has been specified on db.table. */

boolean subtrackMergeIsBpWise();
/* Return TRUE if the subtrack merge operation is base pair-wise. */

boolean isSubtrackMerged(char *tableName);
/* Return true if tableName has been selected for subtrack merge. */

char *describeSubtrackMerge(char *linePrefix);
/* Return a multi-line string that describes the specified subtrack merge,
 * with each line beginning with linePrefix. */

void doSubtrackMergePage(struct sqlConnection *conn);
/* Respond to subtrack merge create/edit button */

void doClearSubtrackMerge(struct sqlConnection *conn);
/* Respond to click on clear intersection. */

void doSubtrackMergeSubmit(struct sqlConnection *conn);
/* Respond to submit on intersect page. */

#define NO_RESULTS "\n# No results returned from query.\n\n"

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
#define hgtaDoClearPasteIdentifierText "hgta_doClearPasteIdentifierText"
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
#define hgtaDoCorrelatePage "hgta_doCorrelatePage"
#define hgtaDoClearCorrelate "hgta_doClearCorrelate"
#define hgtaDoClearContinueCorrelate "hgta_doClearContinueCorrelate"
#define hgtaDoCorrelateSubmit "hgta_doCorrelateSubmit"
#define hgtaDoCorrelateMore "hgta_doCorrelateMore"
#define hgtaDoCorrelateSubmit "hgta_doCorrelateSubmit"
#define hgtaDoSubtrackMergePage "hgta_doSubtrackMergePage"
#define hgtaDoClearSubtrackMerge "hgta_doClearSubtrackMerge"
#define hgtaDoSubtrackMergeSubmit "hgta_doSubtrackMergeSubmit"
#define hgtaDoTest "hgta_doTest"
#define hgtaDoSchemaTable "hgta_doSchemaTable"
#define hgtaDoSchemaDb "hgta_doSchemaDb"
#define hgtaDoValueHistogram "hgta_doValueHistogram"
#define hgtaDoValueRange "hgta_doValueRange"
#define hgtaDoPrintSelectedFields "hgta_doPrintSelectedFields"
#define hgtaDoGalaxySelectedFields "hgta_doGalaxySelectedFields"
#define hgtaDoSelectFieldsMore "hgta_doSelectFieldsMore"
#define hgtaDoClearAllFieldPrefix "hgta_doClearAllField."
#define hgtaDoSetAllFieldPrefix "hgta_doSetAllField."
#define hgtaDoGenePredSequence "hgta_doGenePredSequence"
#define hgtaDoGenomicDna "hgta_doGenomicDna"
#define hgtaDoGetBed "hgta_doGetBed"
#define hgtaDoGetCustomTrackGb "hgta_doGetCustomTrackGb"
#define hgtaDoGetCustomTrackTb "hgta_doGetCustomTrackTb"
#define hgtaDoGetCustomTrackFile "hgta_doGetCustomTrackFile"
#define hgtaDoRemoveCustomTrack "hgta_doRemoveCustomTrack"
#define hgtaDoGalaxyQuery "hgta_doGalaxyQuery"
#define hgtaDoGreatOutput "hgta_doGreatOutput"
#define hgtaDoGreatQuery "hgta_doGreatQuery"
#define hgtaDoGsLogin "hgta_doGsLogin"
#define hgtaGsUser "hgta_gsUser"
#define hgtaGsPassword "hgta_gsPassword"
#define hgtaDoLookupPosition "hgta_doLookupPosition"
#define hgtaDoMetaData "hgta_doMetaData"
#define hgtaDoSetUserRegions "hgta_doSetUserRegions"
#define hgtaDoClearSetUserRegionsText "hgta_doClearSetUserRegionsText"
#define hgtaUserRegionsFile "hgta_userRegionsFile"
#define hgtaUserRegionsDb "hgta_userRegionsDb"
#define hgtaEnteredUserRegions "hgta_enteredUserRegions"
#define hgtaEnteredUserRegionFile "hgta_enteredUserRegionFile"
#define hgtaDoSubmitUserRegions "hgta_doSubmitUserRegions"
#define hgtaDoClearUserRegions "hgta_doClearUserRegions"
#define hgtaRegionTypeUserRegions "userRegions"
#define hgtaRegionTypeEncode "encode"
#define hgtaRegionTypeGenome "genome"
#define hgtaRegionTypeRange "range"
#define hgtaDoPalOut "hgta_palOut"
#define hgtaDoPal "hgta_doPal"

/* Other CGI variables. */
#define hgtaGroup "hgta_group"
#define hgtaTrack "hgta_track"
#define hgtaSelDb "hgta_selDb"
#define hgtaRegionType "hgta_regionType"
#define hgtaCompressType "hgta_compressType"
#define hgtaOutSep "hgta_outSep"
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
#define hgtaIdentifierTable "hgta_identifierTable"
#define hgtaIdentifierDb "hgta_identifierDb"
#define hgtaFilterTable "hgta_filterTable"
#define hgtaFieldSelectTable "hgta_fieldSelectTable"
#define hgtaGeneSeqType "hgta_geneSeqType"
#define hgtaPrintCustomTrackHeaders "hgta_printCustomTrackHeaders"
#define hgtaCtName "hgta_ctName"
#define hgtaCtDesc "hgta_ctDesc"
#define hgtaCtVis "hgta_ctVis"
#define hgtaCtUrl "hgta_ctUrl"
#define hgtaCtWigOutType "hgta_ctWigOutType"
#define hgtaMetaStatus "hgta_metaStatus"
#define hgtaMetaVersion "hgta_metaVersion"
#define hgtaMetaDatabases "hgta_metaDatabases"
#define hgtaMetaTables "hgta_metaTables"
#define hgtaMafTable "hgta_mafTable"

   /* These intersection page vars come in pairs so we can cancel. */
#define hgtaIntersectGroup "hgta_intersectGroup"
#define hgtaNextIntersectGroup "hgta_nextIntersectGroup"
#define hgtaIntersectTrack "hgta_intersectTrack"
#define hgtaNextIntersectTrack "hgta_nextIntersectTrack"
#define hgtaIntersectTable "hgta_intersectTable"
#define hgtaNextIntersectTable "hgta_nextIntersectTable"
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

   /* These correlate page vars come in pairs so we can cancel. */
#define hgtaCorrelateGroup "hgta_correlateGroup"
#define hgtaNextCorrelateGroup "hgta_nextCorrelateGroup"
#define hgtaCorrelateTrack "hgta_correlateTrack"
#define hgtaNextCorrelateTrack "hgta_nextCorrelateTrack"
#define hgtaCorrelateOp "hgta_correlateOp"
#define hgtaNextCorrelateOp "hgta_nextCorrelateOp"
#define hgtaCorrelateTable "hgta_correlateTable"
#define hgtaNextCorrelateTable "hgta_nextCorrelateTable"
#define hgtaCorrelateTable "hgta_correlateTable"
#define hgtaNextCorrelateTable "hgta_nextCorrelateTable"
#define hgtaCorrelateWindowSize "hgta_CorrelateWindowSize"
#define hgtaNextCorrelateWindowSize "hgta_nextCorrelateWindowSize"

/* Subtrack merge vars (also in pairs so we can cancel). */
#define hgtaSubtrackMergePrefix "hgta_subtrackMerge"
#define hgtaNextSubtrackMergePrefix "hgta_nextSubtrackMerge"
#define hgtaSubtrackMergePrimary hgtaSubtrackMergePrefix "Primary"
#define hgtaNextSubtrackMergePrimary hgtaNextSubtrackMergePrefix "Primary"
#define hgtaSubtrackMergeOp hgtaSubtrackMergePrefix "Op"
#define hgtaNextSubtrackMergeOp hgtaNextSubtrackMergePrefix "Op"
#define hgtaSubtrackMergeMoreThreshold hgtaSubtrackMergePrefix "MoreThreshold"
#define hgtaNextSubtrackMergeMoreThreshold hgtaNextSubtrackMergePrefix "MoreThreshold"
#define hgtaSubtrackMergeLessThreshold hgtaSubtrackMergePrefix "LessThreshold"
#define hgtaNextSubtrackMergeLessThreshold hgtaNextSubtrackMergePrefix "LessThreshold"
#define hgtaSubtrackMergeWigOp hgtaSubtrackMergePrefix "WigOp"
#define hgtaNextSubtrackMergeWigOp hgtaNextSubtrackMergePrefix "WigOp"
#define hgtaSubtrackMergeRequireAll hgtaSubtrackMergePrefix "RequireAll"
#define hgtaNextSubtrackMergeRequireAll hgtaNextSubtrackMergePrefix "RequireAll"
#define hgtaSubtrackMergeUseMinScore hgtaSubtrackMergePrefix "UseMinScore"
#define hgtaNextSubtrackMergeUseMinScore hgtaNextSubtrackMergePrefix "UseMinScore"
#define hgtaSubtrackMergeMinScore hgtaSubtrackMergePrefix "MinScore"
#define hgtaNextSubtrackMergeMinScore hgtaNextSubtrackMergePrefix "MinScore"

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
#define outChromGraphData "chromGraphData"
#define outMicroarrayNames "microarrayNames"
#define outMicroarrayGroupings "microarrayGroupings"
#define outGalaxy "galaxyQuery"
#define outMaf "maf"
#define outPalOptions "fasta"
#define outTab "tab"
#define outCsv "csv"

/* For disabling tables in 'tableBrowser noGenome' settings, when region is genome */
#define NO_GENOME_CLASS " class=\"hgtaNoGenome\" title=\"Position range queries only\""

/* --------- Identifier list handling stuff. ------------ */

char *identifierFileName();
/* File name identifiers are in, or NULL if no such file. */

struct hash *identifierHash(char *db, char *table);
/* Return hash full of identifiers from the given table (or NULL). */

char *getIdField(char *db, struct trackDb *track, char *table,
	struct hTableInfo *hti);
/* Get ID field for table, or NULL if none.  FreeMem result when done */

char *identifierWhereClause(char *idField, struct hash *idHash);
/* If the number of pasted IDs is reasonably low, return a where-clause component for the IDs. */

/* --------- Summary and stats stuff -------------- */
long long basesInRegion(struct region *regionList, int limit);
/* Count up all bases in regions to limit number of regions, 0 == no limit */

long long gapsInRegion(struct sqlConnection *conn, struct region *regionList,
	int limit);
/* Return count of gaps in all regions to limit number of regions,
 *	limit=0 == no limit, do them all
 */

void percentStatRow(char *label, long long p, long long q);
/* Print label, p, and p/q */

void numberStatRow(char *label, long long x);
/* Print label, x in table. */

void floatStatRow(char *label, double x);
/* Print label, x in table. */

void stringStatRow(char *label, char *val);
/* Print label, string value. */

void wigFilterStatRow(struct sqlConnection *conn);
/* Put row in statistics table that says what wig filter is on. */

/* ----------- Filter stuff in filterFields.c. -------- */

void cgiToCharFilter(char *dd, char *pat, enum charFilterType *retCft,
		     char **retVals, boolean *retInv);
/* Given a "does/doesn't" and a (list of) literal chars from CGI, fill in
 * retCft, retVals and retInv to make a filter. */

void cgiToStringFilter(char *dd, char *pat, enum stringFilterType *retSft,
		       char ***retVals, boolean *retInv);
/* Given a "does/doesn't" and a (list of) regexps from CGI, fill in
 * retCft, retVals and retInv to make a filter. */

void cgiToDoubleFilter(char *cmp, char *pat, enum numericFilterType *retNft,
		    double **retVals);
/* Given a comparison operator and a (pair of) integers from CGI, fill in
 * retNft and retVals to make a filter. */

void cgiToLongFilter(char *cmp, char *pat, enum numericFilterType *retNft,
		    long long **retVals);
/* Given a comparison operator and a (pair of) integers from CGI, fill in
 * retNft and retVals to make a filter. */

void cgiToIntFilter(char *cmp, char *pat, enum numericFilterType *retNft,
		    int **retVals);
/* Given a comparison operator and a (pair of) integers from CGI, fill in
 * retNft and retVals to make a filter. */


/* ----------- ChromGraph stuff in chromGraph.c. --------- */
boolean isChromGraph(struct trackDb *track);
/* Return TRUE if it's a chromGraph track */

void doOutChromGraphDataCt(struct trackDb *track, char *table);
/* Print out data points in region for chromGraph custom track. */

void doSummaryStatsChromGraph(struct sqlConnection *conn);
/* Put up page showing summary stats for chromGraph track. */

/* ----------- hgPal stuff in pal.c ------------------------------*/

boolean isPalCompatible(struct sqlConnection *conn,
    struct trackDb *track, char *table);
/* Return TRUE if table is genePred and there is a maf. */

void doOutPalOptions( struct sqlConnection *conn);
/* Output protein alignment page */


/* ----------- Maf stuff in maf.c ------------------------------*/

boolean isMafTable(char *database, struct trackDb *track, char *table);
/* Return TRUE if table is maf. */

void doOutMaf(struct trackDb *track, char *table, struct sqlConnection *conn);
/* Output regions as MAF. */

void doHalMaf(struct trackDb *track, char *table, struct sqlConnection *conn);
/* Output HAL regions as MAF. */

/* ----------- Wiggle business in wiggle.c -------------------- */

#define	MAX_REGION_DISPLAY	1000

enum wigOutputType
/*	type of output requested from static int wigOutRegion()	*/
    {
    wigOutData, wigOutBed, wigDataNoPrint,
    };

boolean isWiggle(char *db, char *table);
/* Return TRUE if db.table is a wiggle. */

boolean isBedGraph(char *table);
/* Return TRUE if table is specified as a bedGraph in the current database's
 * trackDb. */

struct bed *getWiggleAsBed(
    char *db, char *table, 	/* Database and table. */
    struct region *region,  /* Region to get data for. */
    char *filter, 		/* Filter to add to SQL where clause if any. */
    struct hash *idHash, 	/* Restrict to id's in this hash if non-NULL. */
    struct lm *lm,		/* Where to allocate memory. */
    struct sqlConnection *conn);	/* SQL connection to work with */
/* Return a bed list of all items in the given range in table.
 * Cleanup result via lmCleanup(&lm) rather than bedFreeList.  */

struct bed *getBedGraphAsBed(struct sqlConnection *conn, char *table,
			     struct region *region);
/* Extract a bedList in region from the given bedGraph table --
 * subtrack merge, filtering and intersection are handled here. */

void wiggleMinMax(struct trackDb *tdb, double *min, double *max);
/*	obtain wiggle data limits from trackDb or cart or settings */

struct wigAsciiData *getWiggleAsData(struct sqlConnection *conn, char *table,
	struct region *region);
/*	return the wigAsciiData list	*/

struct wigAsciiData *getWiggleData(struct sqlConnection *conn, char *table,
	struct region *region, int maxOut, int spanConstraint);
/*	like getWiggleAsData above, but with specific spanConstraint and
 *	a different data limit count, return the wigAsciiData list	*/

boolean checkWigDataFilter(char *db, char *table,
	char **constraint, double *ll, double *ul);
/*	check if filter exists, return its values, call with db="ct" for
 *	custom tracks	*/

void doOutWigBed(struct trackDb *track, char *table, struct sqlConnection *conn);
/* Return wiggle data in bed format. */

void doOutWigData(struct trackDb *track, char *table, struct sqlConnection *conn);
/* Return wiggle data in variableStep format. */

void doSummaryStatsWiggle(struct sqlConnection *conn);
/* Put up page showing summary stats for wiggle track. */

void wigShowFilter(struct sqlConnection *conn);
/* print out wiggle data value filter */

/* ----------- BigWig business in bigWig.c -------------------- */

boolean isBigWig(char *database, char *table, struct trackDb *parent,
	struct customTrack *(*ctLookupName)(char *table));
/* Local test to see if something is bigWig.  Handles hub tracks unlike hIsBigWig. */

boolean isBigWigTable(char *table);
/* Return TRUE if table is bedGraph in current database's trackDb. */

char *bigFileNameFromCtOrHub(char *table, struct sqlConnection *conn);
/* If table is a custom track or hub track, return the bigDataUrl setting;
 * otherwise return NULL.  Do a freeMem on returned string when done. */

char *bigDataIndexFromCtOrHub(char *table, struct sqlConnection *conn);
/* If table is a custom track or hub track, return the bigDataIndex setting;
 * otherwise return NULL.  Do a freeMem on returned string when done. */

char *bigWigFileName(char *table, struct sqlConnection *conn);
/* Return file name associated with bigWig.  This handles differences whether it's
 * a custom or built-in track.  Do a freeMem on returned string when done. */
#define bigBedFileName(table, conn) bigWigFileName(table, conn)

struct hTableInfo *bigWigToHti(char *table);
/* Get fields of bigWig into hti structure. */

int mathWigOutRegion(struct trackDb *track, char *table, struct sqlConnection *conn,
			     struct region *region, int maxOut,
			     enum wigOutputType wigOutType);
/* Write out mathWig for region, doing intersecting and filtering as need be. */

int bigWigOutRegion(char *table, struct sqlConnection *conn,
			     struct region *region, int maxOut,
			     enum wigOutputType wigOutType);
/* Write out bigWig for region, doing intersecting and filtering as need be. */

void doSummaryStatsBigWig(struct sqlConnection *conn);
/* Put up page showing summary stats for bigWig track. */

struct bed *bigWigIntervalsToBed(struct sqlConnection *conn, char *table, struct region *region,
				 struct lm *lm);
/* Return a list of unfiltered, unintersected intervals in region as bed (for
 * secondary table in intersection). */

void showSchemaBigWigNoTable(char *db, char *table, struct trackDb *tdb);
/* Show schema on bigWig. */

/* ----------- BigBed business in bigBed.c -------------------- */
boolean isBigBed(char *database, char *table, struct trackDb *parent,
	struct customTrack *(*ctLookupName)(char *table));
/* Local test to see if something is big bed.  Handles hub tracks unlike hIsBigBed. */

char *bigBedFileName(char *table, struct sqlConnection *conn);
/* Return file name associated with bigBed.  This handles differences whether it's
 * a custom or built-in track or hub.  Do a freeMem on returned string when done. */

struct hTableInfo *bigBedToHti(char *table, struct sqlConnection *conn);
/* Get fields of bigBed into hti structure. */

struct slName *bigBedGetFields(char *table, struct sqlConnection *conn);
/* Get fields of bigBed as simple name list. */

struct sqlFieldType *bigBedListFieldsAndTypes(struct trackDb *tdb, struct sqlConnection *conn);
/* Get fields of bigBed as list of sqlFieldType. */

struct bed *bigBedGetFilteredBedsOnRegions(struct sqlConnection *conn,
	char *db, char *table, struct region *regionList, struct lm *lm,
	int *retFieldCount);
/* Get list of beds from bigBed, in all regions, that pass filtering. */

void bigBedTabOut(char *db, char *table, struct sqlConnection *conn, char *fields, FILE *f, char outSep);
/* Print out selected fields from Big Bed.  If fields is NULL, then print out all fields. */

struct slName *randomBigBedIds(char *table, struct sqlConnection *conn, int count);
/* Return some arbitrary IDs from a bigBed file. */

void showSchemaBigBed(char *table, struct trackDb *tdb);
/* Show schema on bigBed. */

/* More stuff in bigBed.c that makes use of autoSql files. */

struct sqlFieldType *sqlFieldTypesFromAs(struct asObject *as);
/* Convert asObject to list of sqlFieldTypes */

/* HAL stuff from hal.c */

boolean isHalTable(char *table);
/* Return TRUE if table corresponds to a HAL file. */

struct slName *halGetFields(char *table);
/* Get fields of hal as simple name list. */

void halTabOut(char *db, char *table, struct sqlConnection *conn, char *fields, FILE *f);
/* BAM stuff from bam.c */

boolean isLongTabixTable(char *table);
/* Return TRUE if table corresponds to a longTabix file. */

struct slName *getLongTabixFields();
/* Get fields of bam as simple name list. */

void longTabixTabOut(char *db, char *table, struct sqlConnection *conn, char *fields, FILE *f, char outSep);
/* Print out selected fields from long tabix.  If fields is NULL, then print out all fields. */

struct hTableInfo *longTabixToHti(char *table);
/* Get standard fields of BAM into hti structure. */

struct bed *longTabixGetFilteredBedsOnRegions(struct sqlConnection *conn,
	char *db, char *table, struct region *regionList, struct lm *lm,
	int *retFieldCount);
/* Get list of beds from long tabix, in all regions, that pass filtering. */

struct sqlFieldType *longTabixListFieldsAndTypes();
/* Get fields of BAM as list of sqlFieldType. */

void showSchemaLongTabix(char *table, struct trackDb *tdb);
/* Show schema on long tabix. */

boolean isBamTable(char *table);
/* Return TRUE if table corresponds to a BAM file. */

struct slName *bamGetFields();
/* Get fields of bam as simple name list. */

struct sqlFieldType *bamListFieldsAndTypes();
/* Get fields of BAM as list of sqlFieldType. */

struct hTableInfo *bamToHti(char *table);
/* Get standard fields of BAM into hti structure. */

void showSchemaBam(char *table, struct trackDb *tdb);
/* Show schema on bam. */

void bamTabOut(char *db, char *table, struct sqlConnection *conn, char *fields, FILE *f, char outSep);
/* Print out selected fields from BAM.  If fields is NULL, then print out all fields. */

struct bed *bamGetFilteredBedsOnRegions(struct sqlConnection *conn,
	char *db, char *table, struct region *regionList, struct lm *lm,
	int *retFieldCount);
/* Get list of beds from BAM, in all regions, that pass filtering. */

struct slName *randomBamIds(char *table, struct sqlConnection *conn, int count);
/* Return some semi-random qName based IDs from a BAM file. */

/* Hi-C stuff from hic.c */

struct hTableInfo *hicToHti(char *table);
/* Get standard fields of hic into hti structure. */

boolean isHicTable(char *table);
/* Return TRUE if table corresponds to a hic file. */

struct slName *hicGetFields();
/* Get fields of hic as simple name list.  We represent hic with an interact structure, so
 * this is really just an interact as object. */

struct sqlFieldType *hicListFieldsAndTypes();
/* Get fields of hic as list of sqlFieldType (again, this is really just the list of interact fields. */

void showSchemaHic(char *table, struct trackDb *tdb);
/* Show schema on hic. */

void hicTabOut(char *db, char *table, struct sqlConnection *conn, char *fields, FILE *f, char outSep);
/* Print out selected fields from hic.  If fields is NULL, then print out all fields. */

struct bed *hicGetFilteredBedsOnRegions(struct sqlConnection *conn,
        char *db, char *table, struct region *regionList, struct lm *lm,
        int *retFieldCount);
/* Get list of beds from HIC, in all regions, that pass filtering. */

void hicMainPageConfig(struct cart *cart, struct trackDb *tdb);
/* Display Hi-C-specific track configuration options (resolution, normalization) on
 * the main page. */

/* VCF (Variant Call Format) stuff from vcf.c */

extern char *vcfDataLineAutoSqlString;

boolean isVcfTable(char *table, boolean *retIsTabix);
/* Return TRUE if table corresponds to a VCF file. 
 * If retIsTabix is non-NULL, set *retIsTabix to TRUE if this is vcfTabix (not just vcf). */

struct slName *vcfGetFields();
/* Get fields of VCF as simple name list. */

struct sqlFieldType *vcfListFieldsAndTypes();
/* Get fields of VCF as list of sqlFieldType. */

struct hTableInfo *vcfToHti(char *table, boolean isTabix);
/* Get standard fields of VCF into hti structure. */

void showSchemaVcf(char *table, struct trackDb *tdb, boolean isTabix);
/* Show schema on VCF. */

void vcfTabOut(char *db, char *table, struct sqlConnection *conn, char *fields, FILE *f,
	       boolean isTabix);
/* Print out selected fields from VCF.  If fields is NULL, then print out all fields. */

struct bed *vcfGetFilteredBedsOnRegions(struct sqlConnection *conn,
	char *db, char *table, struct region *regionList, struct lm *lm,
	int *retFieldCount, boolean isTabix);
/* Get list of beds from VCF, in all regions, that pass filtering. */

struct slName *randomVcfIds(char *table, struct sqlConnection *conn, int count, boolean isTabix);
/* Return some semi-random IDs from a VCF file. */

/* ----------- Custom track stuff. -------------- */
struct customTrack *getCustomTracks();
/* Get custom track list. */

struct customTrack *ctLookupName(char *name);
/* Lookup name in custom track list */

void removeNamedCustom(struct customTrack **pList, char *name);
/* Remove named custom track from list if it's on there. */

void flushCustomTracks();
/* Flush custom track list. */

struct slName *getBedFields(int fieldCount);
/* Get list of fields for bed of given size. */

struct bedFilter *bedFilterForCustomTrack(char *ctName);
/* If the user specified constraints, then translate them to a bedFilter. */

struct bed *customTrackGetFilteredBeds(char *db, char *name, struct region *regionList,
	struct lm *lm, int *retFieldCount);
/* Get list of beds from custom track of given name that are
 * in current regions and that pass filters.  You can bedFree
 * this when done. */

struct customTrack *newCt(char *ctName, char *ctDesc, int visNum, char *ctUrl,
			  int fields);
/* Make a new custom track record for the query results. */

struct hTableInfo *ctToHti(struct customTrack *ct);
/* Create an hTableInfo from a customTrack. */

void doTabOutDb( char *db, char *dbVarName, char *table, char *tableVarName,
	FILE *f, struct sqlConnection *conn, char *fields, char outSep);
/* Do tab-separated output on fields of a single table. */

void doTabOutCustomTracks(char *db, char *table, struct sqlConnection *conn,
	char *fields, FILE *f, char outSep);
/* Print out selected fields from custom track.  If fields
 * is NULL, then print out all fields. */

/* -----------  Bed/joining stuff -------------- */

struct bed *getRegionAsBed(
	char *db, char *table, 	/* Database and table. */
	struct region *region,  /* Region to get data for. */
	char *filter, 		/* Filter to add to SQL where clause if any. */
	struct hash *idHash, 	/* Restrict to id's in this hash if non-NULL. */
	struct lm *lm,		/* Where to allocate memory. */
	int *retFieldCount);	/* Number of fields. */
/* Return a bed list of all items in the given range in table.
 * Cleanup result via lmCleanup(&lm) rather than bedFreeList.  */

struct bed *getRegionAsMergedBed(
	char *db, char *table, 	/* Database and table. */
	struct region *region,  /* Region to get data for. */
	char *filter, 		/* Filter to add to SQL where clause if any. */
	struct hash *idHash, 	/* Restrict to id's in this hash if non-NULL. */
	struct lm *lm,		/* Where to allocate memory. */
	int *retFieldCount);	/* Number of fields. */
/* Return a bed list of all items in the given range in subtrack-merged table.
 * Cleanup result via lmCleanup(&lm) rather than bedFreeList.  */

struct bed *dbGetFilteredBedsOnRegions(struct sqlConnection *conn,
	char *db, char *dbVarName, char *table, char *tableVarName,
	struct region *regionList, struct lm *lm,
	int *retFieldCount);
/* Get list of beds from database in region that pass filtering. */

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

void doClearPasteIdentifierText(struct sqlConnection *conn);
/* Respond to clear within paste identifier page. */

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

void doOutPrimaryTable(char *table, struct sqlConnection *conn);
/* Dump out primary table. */

void doOutSelectedFields(char *table, struct sqlConnection *conn);
/* Put up select fields (for tab-separated output) page. */

void doOutSequence(struct sqlConnection *conn);
/* Output sequence page. */


void doOutBed(char *table, struct sqlConnection *conn);
/* Put up form to select BED output format. */

void doOutHyperlinks(char *table, struct sqlConnection *conn);
/* Output as genome browser hyperlinks. */

void doOutGff(char *table, struct sqlConnection *conn, boolean outputGtf);
/* Save as GFF/GTF. */

void doOutCustomTrack(char *table, struct sqlConnection *conn);
/* Put up form to select Custom Track output format. */

void doOutGalaxyQuery (struct trackDb *track, char *table, unsigned int hguid);
/* print options page for background query */

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

void doCorrelatePage(struct sqlConnection *conn);
/* Respond to correlate create/edit button */

void doClearContinueCorrelate(struct sqlConnection *conn);
/* Respond to click on clear selections from correlation calculate page. */

void doClearCorrelate(struct sqlConnection *conn);
/* Respond to click on clear correlate. */

void doCorrelateMore(struct sqlConnection *conn);
/* Continue working in correlate page. */

void doCorrelateSubmit(struct sqlConnection *conn);
/* Finish working in correlate page. */

void doGenePredSequence(struct sqlConnection *conn);
/* Output genePred sequence. */

void doGenePredPal(struct sqlConnection *conn);
/* Output genePred protein alignment. */

void doGenomicDna(struct sqlConnection *conn);
/* Get genomic sequence (UI has already told us how). */

void doGetBed(struct sqlConnection *conn);
/* Get BED output (UI has already told us how). */

void doGetCustomTrackTb(struct sqlConnection *conn);
/* Get Custom Track output (UI has already told us how) in Table Browser. */

void doGetCustomTrackGb(struct sqlConnection *conn);
/* Get Custom Track output (UI has already told us how) in Genome Browser. */

void doGetCustomTrackFile(struct sqlConnection *conn);
/* Get Custom Track file output (UI has already told us how). */

void doRemoveCustomTrack(struct sqlConnection *conn);
/* Remove custom track file. */

void doSummaryStatsBed(struct sqlConnection *conn);
/* Put up page showing summary stats for track that is in database
 * or that is bed-format custom. */

void doSubmitUserRegions(struct sqlConnection *conn);
/* Process submit in set regions page. */
void doSetUserRegions(struct sqlConnection *conn);
/* Respond to set regions button. */
void doClearUserRegions(struct sqlConnection *conn);
/* Respond to clear user regions button. */
char *userRegionsFileName();
/* File name defined regions are in, or NULL if no such file. */
struct region *getUserRegions(char *fileName);
/* Get user defined regions from fileName. */
void doClearSetUserRegionsText(struct sqlConnection *conn);
/* Respond to clear within user regions enter page. */

/* --------------- Galaxy functions --------------- */
boolean doGalaxy ();
/* Has the send query results to Galaxy checkbox been selected? */

void printGalaxySubmitButtons ();
/* print submit button to send query results to Galaxy */

void startGalaxyForm ();
/* start form to send parameters to Galaxy, also send required params */

void sendParamsToGalaxy(char *doParam, char *paramVal);
/* intermediate page for formats printed directly from top form */

void verifyGalaxyFormat(const char *output);

/* print warning about not using hyperlinks output formtat with Galaxy */
/* --------------- GREAT functions --------------- */
boolean doGreat();
/* Has the send query results to GREAT checkbox been selected? */

void verifyGreatFormat(const char *output);
/* check that specified outupt format is acceptable for GREAT; errAbort if not */

void verifyGreatAssemblies();
/* check that specified assembly is acceptable for GREAT; errAbort if not */

void printGreatSubmitButtons();
/* print submit button to send query results to GREAT */

void startGreatForm();
/* start form to send parameters to GREAT, also send required params */

void doGetGreatOutput(void (*dispatch)());
/* Generate output that GREAT will read via doGetGreatResults. */

void doGreatTopLevel();
/* intermediate page for sending to GREAT directly from top form */

/* --------------- GenomeSpace functions --------------- */
boolean isGenomeSpaceEnabled();
/* genomeSpace is enabled by the presence of GS config settings. */

boolean doGenomeSpace();
/* Has the send query results to GenomeSpace checkbox been selected? */

boolean checkGsReady();
/* check that GS requirements are met */

void doGsLogin(struct sqlConnection *conn);
/* Process user password post. */

void gsSendToDM();
/* upload the generated file to DM */

void getBackgroundStatus(char *url);
/* fetch status as the latest complete html block available */

void startBackgroundWork(char *exec, char **pWorkUrl);
/* deal with forking off child for background work
 * and setting up the trash file for communicating
 * from the child to the browser */

/* --------------- wikiTrack functions --------------- */
struct hTableInfo *wikiHti();
/* Create an hTableInfo for the wikiTrack. */

void doSummaryStatsWikiTrack(struct sqlConnection *conn);
/* Put up page showing summary stats for wikiTrack. */

struct bed *wikiTrackGetFilteredBeds(char *name, struct region *regionList,
	struct lm *lm, int *retFieldCount);
/* Get list of beds from the wikiTrack * in current regions and that pass
 *	filters.  You can bedFree this when done.  */

/* --------------- microarray track functions ------------- */

boolean isMicroarray(struct trackDb *parent, char *table);
/* Return TRUE if table is specified as a microarray in the current database's
 * trackDb. */

void doOutMicroarrayNames(struct trackDb *tdb);
/* Show the microarray names from .ra file */

#define uglyw warn	/* Warn for debugging purposes. */

int bigFileMaxOutput();
/*	return maxOut value (cart variable defined on curTable)	*/

#endif /* HGTABLES_H */
