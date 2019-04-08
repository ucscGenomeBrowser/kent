/* utility functions for data API business */

#include "dataApi.h"

#ifdef NOT
static void jsonFinishOutput(int errorCode, char *errorString, struct jsonWrite *jw)
/* potential output an error code other than 200 */
{
/* this is the first time any output to stdout has taken place for
 * json output, therefore, start with the appropriate header
 */
puts("Content-Type:application/json");
/* potentially with an error code return */
if (errorCode)
    {
    char errString[2048];
    safef(errString, sizeof(errString), "Status: %d %s",errorCode,errorString);
    puts(errString);
    }
puts("\n");

jsonWriteObjectEnd(jw);
fputs(jw->dy->string,stdout);
}
#endif

void apiErrAbort(char *format, ...)
/* Issue an error message in json format, and exit(0) */
{
char errMsg[2048];
va_list args;
va_start(args, format);
vsnprintf(errMsg, sizeof(errMsg), format, args);
struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "error", errMsg);
// jsonFinishOutput(400, "Bad Request", jw);
jsonWriteObjectEnd(jw);
fputs(jw->dy->string,stdout);
exit(0);
}

struct jsonWrite *apiStartOutput()
/* begin json output with standard header information for all requests */
{
time_t timeNow = time(NULL);
// struct tm tm;
// gmtime_r(&timeNow, &tm);
struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, NULL);
// not recommended: jsonWriteString(jw, "apiVersion", "v"CGI_VERSION);
// not needed jsonWriteString(jw, "source", "UCSantaCruz");
jsonWriteDateFromUnix(jw, "downloadTime", (long long) timeNow);
jsonWriteNumber(jw, "downloadTimeStamp", (long long) timeNow);
if (debug)
    jsonWriteNumber(jw, "botDelay", (long long) botDelay);
return jw;
}

/* these json type strings beyond 'null' are types for UCSC jsonWrite()
 * functions.  The real 'official' json types are just the first six
 */
char *jsonTypeStrings[] =
    {
    "string",	/* type 0 */
    "number",	/* type 1 */
    "object",	/* type 2 */
    "array",	/* type 3 */
    "boolean",	/* type 4 */
    "null",	/* type 5 */
    "double"	/* type 6 */
    };

/* this set of SQL types was taken from a survey of all the table
 *  descriptions on all tables on hgwdev.  This is not necessarily
 *  a full range of all SQL types possible, just what we have used in
 *  UCSC databases.  The fallback default will be string.
 * bigint binary bit blob char date datetime decimal double enum float int
 * longblob longtext mediumblob mediumint mediumtext set smallint text
 * timestamp tinyblob tinyint tinytext unsigned varbinary varchar
 * Many of these are not yet encountered since they are often in
 * non-positional tables.  This system doesn't yet output non-positional
 * tables.
 */
static int sqlTypeToJsonType(char *sqlType)
/* convert SQL data type to JSON data type (index into jsonTypes[]) */
{
/* assume string, good enough for just about anything */
int typeIndex = JSON_STRING;

if (startsWith("tinyint(1)", sqlType))
    typeIndex = JSON_BOOLEAN;
else if (startsWith("bigint", sqlType) ||
     startsWith("int", sqlType) ||
     startsWith("mediumint", sqlType) ||
     startsWith("smallint", sqlType) ||
     startsWith("tinyint", sqlType) ||
     startsWith("unsigned", sqlType)
   )
    typeIndex = JSON_NUMBER;
else if (startsWith("decimal", sqlType) ||
     startsWith("double", sqlType) ||
     startsWith("float", sqlType)
   )
    typeIndex = JSON_DOUBLE;
else if (startsWith("binary", sqlType) ||
	startsWith("bit", sqlType) ||
	startsWith("blob", sqlType) ||
	startsWith("char", sqlType) ||
	startsWith("date", sqlType) ||
	startsWith("datetime", sqlType) ||
	startsWith("enum", sqlType) ||
	startsWith("longblob", sqlType) ||
	startsWith("longtext", sqlType) ||
	startsWith("mediumblob", sqlType) ||
	startsWith("set", sqlType) ||
	startsWith("text", sqlType) ||
	startsWith("time", sqlType) ||
	startsWith("timestamp", sqlType) ||
	startsWith("tinyblob", sqlType) ||
	startsWith("tinytext", sqlType) ||
	startsWith("varbinary", sqlType) ||
	startsWith("varchar", sqlType)
	)
    typeIndex = JSON_STRING;

return typeIndex;
}	/*	static int sqlTypeToJsonType(char *sqlType)	*/

int autoSqlToJsonType(char *asType)
/* convert an autoSql field type to a Json type */
{
/* assume string, good enough for just about anything */
int typeIndex = JSON_STRING;

if (startsWith("int", asType) ||
    startsWith("uint", asType) ||
    startsWith("short", asType) ||
    startsWith("ushort", asType) ||
    startsWith("byte", asType) ||
    startsWith("ubyte", asType) ||
    startsWith("bigit", asType)
   )
    typeIndex = JSON_NUMBER;
else if (startsWith("float", asType))
	typeIndex = JSON_DOUBLE;
else if (startsWith("char", asType) ||
	 startsWith("string", asType) ||
	 startsWith("lstring", asType) ||
	 startsWith("enum", asType) ||
	 startsWith("set", asType)
	)
	typeIndex = JSON_STRING;

return typeIndex;
}	/*	int asToJsonType(char *asType)	*/

int tableColumns(struct sqlConnection *conn, struct jsonWrite *jw, char *table,
   char ***nameReturn, char ***typeReturn, int **jsonType)
/* return the column names, and their MySQL data type, for the given table
 *  return number of columns (aka 'fields')
 */
{
// not needed jsonWriteListStart(jw, "columnNames");
struct sqlFieldInfo *fi, *fiList = sqlFieldInfoGet(conn, table);
int columnCount = slCount(fiList);
char **namesReturn = NULL;
char **typesReturn = NULL;
int *jsonReturn = NULL;
AllocArray(namesReturn, columnCount);
AllocArray(typesReturn, columnCount);
AllocArray(jsonReturn, columnCount);
int i = 0;
for (fi = fiList; fi; fi = fi->next)
    {
    namesReturn[i] = cloneString(fi->field);
    typesReturn[i] = cloneString(fi->type);
    jsonReturn[i] = sqlTypeToJsonType(fi->type);
    i++;
// not needed     jsonWriteObjectStart(jw, NULL);
// not needed     jsonWriteString(jw, fi->field, fi->type);
// not needed     jsonWriteObjectEnd(jw);
    }
// not needed jsonWriteListEnd(jw);
*nameReturn = namesReturn;
*typeReturn = typesReturn;
*jsonType = jsonReturn;
return columnCount;
}

struct trackHub *errCatchTrackHubOpen(char *hubUrl)
/* use errCatch around a trackHub open in case it fails */
{
struct trackHub *hub = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    hub = trackHubOpen(hubUrl, "");
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    apiErrAbort("error opening hubUrl: '%s', '%s'", hubUrl,  errCatch->message->string);
    }
errCatchFree(&errCatch);
return hub;
}

struct trackDb *obtainTdb(struct trackHubGenome *genome, char *db)
/* return a full trackDb fiven the hub genome pointer, or ucsc database name */
{
struct trackDb *tdb = NULL;
if (db)
    tdb = hTrackDb(db);
else
    {
    tdb = trackHubTracksForGenome(genome->trackHub, genome);
    tdb = trackDbLinkUpGenerations(tdb);
    tdb = trackDbPolishAfterLinkup(tdb, genome->name);
    slSort(&tdb, trackDbCmp);
    }
return tdb;
}

struct trackDb *findTrackDb(char *track, struct trackDb *tdb)
/* search tdb structure for specific track, recursion on subtracks */
{
struct trackDb *trackFound = NULL;

for (trackFound = tdb; trackFound; trackFound = trackFound->next)
    {
    if (trackFound->subtracks)
	{
        struct trackDb *subTrack = findTrackDb(track, trackFound->subtracks);
	if (subTrack)
	    {
	    if (sameOk(subTrack->track, track))
		trackFound = subTrack;
	    }
	}
    if (sameOk(trackFound->track, track))
	break;
    }
return trackFound;
}

struct bbiFile *bigFileOpen(char *trackType, char *bigDataUrl)
/* open bigDataUrl for correct trackType and error catch if failure */
{
struct bbiFile *bbi = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
if (startsWith("bigBed", trackType))
    bbi = bigBedFileOpen(bigDataUrl);
else if (startsWith("bigWig", trackType))
    bbi = bigWigFileOpen(bigDataUrl);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    apiErrAbort("error opening bigFile URL: '%s', '%s'", bigDataUrl,  errCatch->message->string);
    }
errCatchFree(&errCatch);
return bbi;
}

int chromInfoCmp(const void *va, const void *vb)
/* Compare to sort based on size */
{
const struct chromInfo *a = *((struct chromInfo **)va);
const struct chromInfo *b = *((struct chromInfo **)vb);
int dif;
dif = (long) a->size - (long) b->size;
return dif;
}
