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
#include "bamFile.h"
#include "jsonParse.h"
#include "jsonWrite.h"
#include "chromInfo.h"
#include "wiggle.h"
#include "hubPublic.h"

#ifdef USE_HAL
#include "halBlockViz.h"
#endif

/* error return codes */
#define err400	400
#define err400Msg	"Bad Request"
#define err404	404
#define err404Msg	"Not Found"
#define err429	429
#define err429Msg	"Two Many Requests"

/* maximum number of words expected in PATH_INFO parsing
 *   so far only using 2
 */
#define MAX_PATH_INFO 32

/* maximum amount of DNA allowed in a get sequence request */
#define MAX_DNA_LENGTH	499999999
/* this size is directly related to the max limit in needMem used in
 * jsonWriteString
 */

/* limit amount of output to a maximum to avoid overload */
extern int maxItemsOutput;	/* can be set in URL maxItemsOutput=N */
/* for debugging purpose, current bot delay value */
extern int botDelay;
boolean debug;	/* can be set in URL debug=1, to turn off: debug=0 */

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

int tableColumns(struct sqlConnection *conn, struct jsonWrite *jw, char *table,
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

/* ######################################################################### */
/*  functions in getData.c */

void apiGetData(char *words[MAX_PATH_INFO]);
/* 'getData' function, words[1] is the subCommand */

/* ######################################################################### */
/*  functions in list.c */

void apiList(char *words[MAX_PATH_INFO]);
/* 'list' function words[1] is the subCommand */

#endif	/*	 DATAAPH_H	*/
