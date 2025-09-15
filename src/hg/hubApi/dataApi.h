/* dataApi.h - functions for API access to UCSC data resources */

#ifndef DATAAPH_H

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "udc.h"
#include "knetUdc.h"
#include "genbank.h"
#include "trackHub.h"
#include "hgConfig.h"
#include "hCommon.h"
#include "hPrint.h"
#include "bigWig.h"
#include "hubConnect.h"
#include "obscure.h"
#include "errCatch.h"
#include "vcf.h"
#include "bedTabix.h"
#include "jsonParse.h"
#include "jsonWrite.h"
#include "chromInfo.h"
#include "wiggle.h"
#include "hubPublic.h"
#include "cartTrackDb.h"
#include "chromAlias.h"
#include "pipeline.h"
#include "genark.h"

#ifdef USE_HAL
#include "halBlockViz.h"
#endif

/* reference for these error codes:
 * https://www.restapitutorial.com/httpstatuscodes.html
 */
/* error return codes */
#define err206	206
#define err206Msg	"Partial Content"
#define err301	301
#define err301Msg	"Moved Permanently"
#define err400	400
#define err400Msg	"Bad Request"
#define err403	403
#define err403Msg	"Forbidden"
#define err404	404
#define err404Msg	"Not Found"
#define err415	415
#define err415Msg	"Unsupported track type"
#define err429	429
#define err429Msg	"Too Many Requests"
#define err500	500
#define err500Msg	"Internal Server Error"

/* list of all potential arguments */
#define argHubUrl	"hubUrl"
#define argGenome	"genome"
#define argTrackLeavesOnly	"trackLeavesOnly"
#define argTrack	"track"
#define argChrom	"chrom"
#define argStart	"start"
#define argEnd	"end"
#define argRevComp	"revComp"
#define argMaxItemsOutput	"maxItemsOutput"
#define argFormat	"format"
#define argJsonOutputArrays	"jsonOutputArrays"
#define argCategories "categories"
#define argSearchTerm "search"
#define argSkipContext "skipContext"
/* used in findGenome, 'q' is for the query search term */
#define argQ "q"
#define argStatsOnly "statsOnly"
#define argBrowser "browser"
#define argYear "year"
#define argCategory "category"
#define argStatus "status"
#define argLevel "level"
#define argFromGenome "fromGenome"
#define argToGenome "toGenome"
/* used in liftOver 'listExisting' function to filter the result */
#define argFilter "filter"
/* used in list/files to show only certain file types */
#define argFileType "fileType"

/* valid argument listings to verify extraneous arguments
 *  initialized in hubApi.c
 */
extern char *argListPublicHubs[];
extern char *argListUcscGenomes[];
extern char *argListGenarkGenomes[];
extern char *argListHubGenomes[];
extern char *argListTracks[];
extern char *argListChromosomes[];
extern char *argListSchema[];
extern char *argListFiles[];
extern char *argGetDataTrack[];
extern char *argGetDataSequence[];
extern char *argSearch[];
extern char *argFindGenome[];
extern char *argLiftOver[];

/* maximum number of words expected in PATH_INFO parsing
 *   so far only using 2
 */
#define MAX_PATH_INFO 32

/* maximum amount of DNA allowed in a get sequence request */
#define MAX_DNA_LENGTH	499999999
/* this size is directly related to the max limit in needMem used in
 * jsonWriteString
 */

extern long enteredMainTime;	/* will become = clock1000() on entry */

/* limit amount of output to a maximum to avoid overload */
extern int maxItemsOutput;	/* can be set in URL maxItemsOutput=N */
extern long long itemsReturned;	/* for getData functions, number of items returned */
extern boolean reachedMaxItems;	/* during getData, signal to return */

/* downloadUrl for use in error exits when reachedMaxItems */
extern struct dyString *downloadUrl;

/* supportedTypes will be initialized to a known supported set */
extern struct slName *supportedTypes;

/* for debugging purpose, current bot delay value */
extern int botDelay;
extern boolean debug;	/* can be set in URL debug=1, to turn off: debug=0 */

/* default is to list all trackDb entries, composite containers too.
 * This option will limit to only the actual track entries with data
 */
extern boolean trackLeavesOnly;	/* set by CGI parameter 'trackLeavesOnly' */

/* this selects output type 'arrays', where the default type is: objects */
extern boolean jsonOutputArrays; /* set by CGI parameter 'jsonOutputArrays' */

extern boolean measureTiming;	/* set by CGI parameters */

/*  functions in hubApi.c */
struct hubPublic *hubPublicDbLoadAll();

struct dbDb *ucscDbDb();
/* return the dbDb table as an slList */

char *verifyLegalArgs(char *validArgList[]);
/* validArgList is an array of strings for valid arguments
 * returning string of any other arguments not on that list found in
 * cgiVarList(), NULL when none found.
 */

/* ######################################################################### */
/*  functions in apiUtils.c */

void startProcessTiming();
/* for measureTiming, beginning processing */

void apiFinishOutput(int errorCode, char *errorString, struct jsonWrite *jw);
/* finish json output, potential output an error code other than 200 */

void apiErrAbort(int errorCode, char *errString, char *format, ...);
/* Issue an error message in json format, and exit(0) */

struct jsonWrite *apiStartOutput();
/* begin json output with standard header information for all requests */

extern char *jsonTypeStrings[];
#define JSON_STRING	0	//    "string",	/* type 0 */
#define JSON_NUMBER	1	//    "number",	/* type 1 */
#define JSON_OBJECT	2	//    "object",	/* type 2 */
#define JSON_ARRAY	3	//    "array",	/* type 3 */
#define JSON_BOOLEAN	4	//    "boolean",	/* type 4 */
#define JSON_NULL	5	//    "null"	/* type 5 */
#define JSON_DOUBLE	6	//    UCSC json type double	/* type 6 */

int autoSqlToJsonType(char *asType);
/* convert an autoSql field type to a Json type */

int tableColumns(struct sqlConnection *conn, char *table,
   char ***nameReturn, char ***typeReturn, int **jsonTypes);
/* return the column names, the MySQL data type, and json data type
 *   for the given table return number of columns (aka 'fields')
 */

struct trackHub *errCatchTrackHubOpen(char *hubUrl);
/* use errCatch around a trackHub open in case it fails */

struct trackDb *obtainTdb(struct trackHubGenome *genome, char *db);
/* return a full trackDb fiven the hub genome pointer, or ucsc database name */

struct trackDb *findTrackDb(char *track, struct trackDb *tdb);
/* search tdb structure for specific track, recursion on subtracks */

struct bbiFile *bigFileOpen(char *trackType, char *bigDataUrl);
/* open bigDataUrl for correct trackType and error catch if failure */

int chromInfoCmp(const void *va, const void *vb);
/* Compare to sort based on size */

boolean allowedBigBedType(char *type);
/* return TRUE if the big* type is to be supported
 * add to this list as the big* supported types are expanded
 */

/* temporarily from table browser until proven works, then move to library */
struct asObject *asForTable(struct sqlConnection *conn, char *table,
    struct trackDb *tdb);
/* Get autoSQL description if any associated with table. */
/* Wrap some error catching around asForTable. */

struct trackHubGenome *findHubGenome(struct trackHub *hub, char *genome,
    char *endpoint, char *hubUrl);
/* given open 'hub', find the specified 'genome' called from 'endpoint' */

struct dbDb *ucscDbDb();
/* return the dbDb table as an slList */

long long genArkSize();
/* return the number of rows in genark table */

struct genark *genArkList(char *oneAccession);
/* return the genark table as an slList, or just the one accession when given */

boolean isSupportedType(char *type);
/* is given type in the supportedTypes list ? */

void wigColumnTypes(struct jsonWrite *jw);
/* output column headers for a wiggle data output schema */

void outputSchema(struct trackDb *tdb, struct jsonWrite *jw,
    char *columnNames[], char *columnTypes[], int jsonTypes[],
	struct hTableInfo *hti, int columnCount, int asColumnCount,
	    struct asColumn *columnEl);
/* print out the SQL schema for this trackDb */

void bigColumnTypes(struct jsonWrite *jw, struct sqlFieldType *fiList,
    struct asObject *as);
/* show the column types from a big file autoSql definitions */

boolean trackHasData(struct trackDb *tdb);
/* check if this is actually a data track:
 *	TRUE when has data, FALSE if has no data
 * When NO trackDb, can't tell at this point, will check that later
 */

#define trackHasNoData(tdb) (!trackHasData(tdb))

boolean protectedTrack(char *db, struct trackDb *tdb, char *trackName);
/* determine if track is off-limits protected data */

boolean isWiggleDataTable(char *type);
/* is this a wiggle data track table */

char *chrOrAlias(char *db, char *hubUrl);
/* get incoming chr name, may be an alias, return the native chr name */

void hubAliasSetup(struct trackHubGenome *hubGenome);
/* see if this hub has an alias file and run chromAliasSetupBb() for it */

void textLineOut(char *lineOut);
/* accumulate text lines for output in the dyString textOutput */

void textFinishOutput();
/* all done with text output, print it all out */

/* ######################################################################### */
/*  functions in getData.c */

void apiGetData(char *words[MAX_PATH_INFO]);
/* 'getData' function, words[1] is the subCommand */

/* ######################################################################### */
/*  functions in list.c */

void apiList(char *words[MAX_PATH_INFO]);
/* 'list' function words[1] is the subCommand */

/* ######################################################################### */
/*  functions in search.c */

void apiSearch(char *words[MAX_PATH_INFO]);
/* search function */

/* ######################################################################### */
/*  functions in findGenome.c */

void apiFindGenome(char *words[MAX_PATH_INFO]);
/* 'findGenome' function */

void apiLiftOver(char *words[MAX_PATH_INFO]);
/* 'liftOver' function */

#endif	/*	 DATAAPH_H	*/
