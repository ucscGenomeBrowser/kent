/* utility functions for data API business */

#include "dataApi.h"

/* when measureTiming is used */
static long processingStart = 0;

void startProcessTiming()
/* for measureTiming, beginning processing */
{
processingStart = clock1000();
}

void apiFinishOutput(int errorCode, char *errorString, struct jsonWrite *jw)
/* finish json output, potential output an error code other than 200 */
{
/* this is the first time any output to stdout has taken place for
 * json output, therefore, start with the appropriate header.
 */
puts("Content-Type:application/json");
/* potentially with an error code return in the header */
if (errorCode)
    {
    char errString[2048];
    safef(errString, sizeof(errString), "Status: %d %s",errorCode,errorString);
    puts(errString);
    if (err429 == errorCode)
	puts("Retry-After: 30");
    }
else if (reachedMaxItems)
    {
    char errString[2048];
    safef(errString, sizeof(errString), "Status: %d %s",err206,err206Msg);
    puts(errString);
    }
puts("\n");

if (debug)
    {
    char sizeString[64];
    unsigned long long vmPeak = currentVmPeak();
    sprintLongWithCommas(sizeString, vmPeak);
    jsonWriteString(jw, "vmPeak", sizeString);
    }

if (measureTiming)
    {
    long nowTime = clock1000();
    long long et = nowTime - processingStart;
    jsonWriteNumber(jw, "totalTimeMs", et);
    }

if (itemsReturned)
    jsonWriteNumber(jw, "itemsReturned", itemsReturned);
if (reachedMaxItems)
    {
    jsonWriteBoolean(jw, "maxItemsLimit", TRUE);
    if (downloadUrl && downloadUrl->string)
	jsonWriteString(jw, "dataDownloadUrl", downloadUrl->string);
    }

jsonWriteObjectEnd(jw);
fputs(jw->dy->string,stdout);
}	/*	void apiFinishOutput(int errorCode, char *errorString, ... ) */

void apiErrAbort(int errorCode, char *errString, char *format, ...)
/* Issue an error message in json format, and exit(0) */
{
char errMsg[2048];
va_list args;
va_start(args, format);
vsnprintf(errMsg, sizeof(errMsg), format, args);
struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "error", errMsg);
jsonWriteNumber(jw, "statusCode", errorCode);
jsonWriteString(jw, "statusMessage", errString);
apiFinishOutput(errorCode, errString, jw);
cgiExitTime("hubApi err", enteredMainTime);
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

/* temporarily from table browser until proven works, then move to library */
/* UNFORTUNATELY, this version needs an extra argument: tdb
 *  the table browser has a global hash to satisify that requirement
 */
struct asObject *asForTable(struct sqlConnection *conn, char *table,
    struct trackDb *tdb)
/* Get autoSQL description if any associated with table. */
/* Wrap some error catching around asForTable. */
{
if (tdb != NULL)
    return asForTdb(conn,tdb);

// Some cases are for tables with no tdb!
struct asObject *asObj = NULL;
if (sqlTableExists(conn, "tableDescriptions"))
    {
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        char query[256];

        sqlSafef(query, sizeof(query),
              "select autoSqlDef from tableDescriptions where tableName='%s'", table);
        char *asText = asText = sqlQuickString(conn, query);

        // If no result try split table. (not likely)
        if (asText == NULL)
            {
            sqlSafef(query, sizeof(query),
                  "select autoSqlDef from tableDescriptions where tableName='chrN_%s'", table);
            asText = sqlQuickString(conn, query);
            }
        if (asText != NULL && asText[0] != 0)
            {
            asObj = asParseText(asText);
            }
        freez(&asText);
        }
    errCatchEnd(errCatch);
    errCatchFree(&errCatch);
    }
return asObj;
}

int tableColumns(struct sqlConnection *conn, char *table,
   char ***nameReturn, char ***typeReturn, int **jsonTypes)
/* return the column names, and their MySQL data type, for the given table
 *  return number of columns (aka 'fields')
 */
{
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
    }
*nameReturn = namesReturn;
*typeReturn = typesReturn;
*jsonTypes = jsonReturn;
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
    apiErrAbort(err404, err404Msg, "error opening hubUrl: '%s', '%s'", hubUrl,  errCatch->message->string);
    }
errCatchFree(&errCatch);
return hub;
}

static int trackDbTrackCmp(const void *va, const void *vb)
/* Compare to sort based on 'track' name; use shortLabel as secondary sort key.
 * Note: parallel code to hgTracks.c:tgCmpPriority */
{
const struct trackDb *a = *((struct trackDb **)va);
const struct trackDb *b = *((struct trackDb **)vb);
int dif = strcmp(a->track, b->track);
if (dif < 0)
   return -1;
else if (dif == 0.0)
   return strcasecmp(a->shortLabel, b->shortLabel);
else
   return 1;
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
    }
slSort(tdb, trackDbTrackCmp);
// slSort(&tdb, trackDbCmp);
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

boolean allowedBigBedType(char *type)
/* return TRUE if the big* bed-like type is to be supported
 * add to this list as the big* bed-like supported types are expanded
 */
{
if (startsWithWord("bigBarChart", type) ||
    startsWithWord("bigBed", type) ||
    startsWithWord("bigGenePred", type) ||
    startsWithWord("bigInteract", type) ||
    startsWithWord("bigLolly", type) ||
    startsWithWord("bigPsl", type)
   )
    return TRUE;
else
    return FALSE;
}

struct bbiFile *bigFileOpen(char *trackType, char *bigDataUrl)
/* open bigDataUrl for correct trackType and error catch if failure */
{
struct bbiFile *bbi = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
if (allowedBigBedType(trackType))
    bbi = bigBedFileOpen(bigDataUrl);
else if (startsWith("bigWig", trackType))
    bbi = bigWigFileOpen(bigDataUrl);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    apiErrAbort(err404, err404Msg, "error opening bigFile URL: '%s', '%s'", bigDataUrl,  errCatch->message->string);
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

struct trackHubGenome *findHubGenome(struct trackHub *hub, char *genome,
    char *endpoint, char *hubUrl)
/* given open 'hub', find the specified 'genome' called from 'endpoint' */
{
struct trackHubGenome *hubGenome = NULL;
for (hubGenome = hub->genomeList; hubGenome; hubGenome = hubGenome->next)
    {
    if (sameString(genome, hubGenome->name))
	break;
    }
if (NULL == hubGenome)
    apiErrAbort(err400, err400Msg, "failed to find specified genome=%s for endpoint '%s'  given hubUrl '%s'", genome, endpoint, hubUrl);
return hubGenome;
}

char *verifyLegalArgs(char *validArgList[])
/* validArgList is an array of strings for valid arguments
 * returning string of any other arguments not on that list found in
 * cgiVarList(), NULL when none found.
 */
{
struct hash *validHash = NULL;
char *words[32];
int wordCount = 0;
int i = 0;
for ( ; isNotEmpty(validArgList[i]); ++i )
    {
    ++wordCount;
    words[i] = validArgList[i];
    }

if (wordCount)
    {
    validHash = hashNew(0);
    int i;
    for (i = 0; i < wordCount; ++i)
	hashAddInt(validHash, words[i], 1);
    }

int extrasFound = 0;
struct dyString *extras = newDyString(128);
struct cgiVar *varList = cgiVarList();
struct cgiVar *var = varList;
for ( ; var; var = var->next)
    {
    if (sameWord("cgiSpoof", var->name))
	continue;
    if (sameWord("debug", var->name))
	continue;
    if (sameWord("measureTiming", var->name))
	continue;
    if (NULL == validHash)
	{
	dyStringPrintf(extras, ";%s=%s", var->name, var->val);
	++extrasFound;
	}
    else if (0 == hashIntValDefault(validHash, var->name, 0))
	{
	if (extrasFound)
	    dyStringPrintf(extras, ";%s=%s", var->name, var->val);
	else
	    dyStringPrintf(extras, "%s=%s", var->name, var->val);
	++extrasFound;
	}
    }
if (extrasFound)
    return dyStringCannibalize(&extras);
else
    return NULL;
}

static int dbDbCmpName(const void *va, const void *vb)
/* Compare two dbDb elements: name, ignore case. */
{
const struct dbDb *a = *((struct dbDb **)va);
const struct dbDb *b = *((struct dbDb **)vb);
return strcasecmp(a->name, b->name);
}

struct dbDb *ucscDbDb()
/* return the dbDb table as an slList */
{
char query[1024];
struct sqlConnection *conn = hConnectCentral();
char *showActive0 = cfgOptionDefault("hubApi.showActive0", "off");
if (sameWord("on", showActive0))
    sqlSafef(query, sizeof(query), "select * from dbDb");
else
    sqlSafef(query, sizeof(query), "select * from dbDb where active=1");
struct dbDb *dbList = NULL, *el = NULL;
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = dbDbLoad(row);
    slAddHead(&dbList, el);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
slSort(&dbList, dbDbCmpName);
return dbList;
}

boolean isSupportedType(char *type)
/* is given type in the supportedTypes list ? */
{
boolean ret = FALSE;
struct slName *el;
for (el = supportedTypes; el; el = el->next)
    {
    if (startsWith(el->name, type))
	{
	ret = TRUE;
	break;
	}
    }
return ret;
}

void wigColumnTypes(struct jsonWrite *jw)
/* output column headers for a wiggle data output schema */
{
jsonWriteListStart(jw, "columnTypes");

jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "name", "start");
jsonWriteString(jw, "sqlType", "int");
jsonWriteString(jw, "jsonType", "number");
jsonWriteString(jw, "description", "chromStart: 0-based chromosome start position");
jsonWriteObjectEnd(jw);

jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "name", "end");
jsonWriteString(jw, "sqlType", "int");
jsonWriteString(jw, "jsonType", "number");
jsonWriteString(jw, "description", "chromEnd: 1-based chromosome end position");
jsonWriteObjectEnd(jw);

jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "name", "value");
jsonWriteString(jw, "sqlType", "float");
jsonWriteString(jw, "jsonType", "number");
jsonWriteString(jw, "description", "numerical data value for this location:start-end");
jsonWriteObjectEnd(jw);

jsonWriteListEnd(jw);
}	/* void wigColumnTypes(struct jsonWrite jw) */

void outputSchema(struct trackDb *tdb, struct jsonWrite *jw,
    char *columnNames[], char *columnTypes[], int jsonTypes[],
	struct hTableInfo *hti, int columnCount, int asColumnCount,
	    struct asColumn *columnEl)
/* print out the SQL schema for this trackDb */
{
if (tdb && isWiggleDataTable(tdb->type))
    {
        wigColumnTypes(jw);
    }
else
    {
    jsonWriteListStart(jw, "columnTypes");
    int i = 0;
    for (i = 0; i < columnCount; ++i)
        {
        jsonWriteObjectStart(jw, NULL);
        jsonWriteString(jw, "name", columnNames[i]);
        jsonWriteString(jw, "sqlType", columnTypes[i]);
        jsonWriteString(jw, "jsonType", jsonTypeStrings[jsonTypes[i]]);
        if ((0 == i) && (hti && hti->hasBin))
            jsonWriteString(jw, "description", "Indexing field to speed chromosome range queries");
        else if (columnEl && isNotEmpty(columnEl->comment))
            jsonWriteString(jw, "description", columnEl->comment);
        else
            jsonWriteString(jw, "description", "");

        /* perhaps move the comment pointer forward */
        if (columnEl)
            {
            if (asColumnCount == columnCount)
                columnEl = columnEl->next;
            else if (! ((0 == i) && (hti && hti->hasBin)))
                columnEl = columnEl->next;
            }
        jsonWriteObjectEnd(jw);
	}
    jsonWriteListEnd(jw);
    }
}

void bigColumnTypes(struct jsonWrite *jw, struct sqlFieldType *fiList,
    struct asObject *as)
/* show the column types from a big file autoSql definitions */
{
struct asColumn *columnEl = as->columnList;
jsonWriteListStart(jw, "columnTypes");
struct sqlFieldType *fi = fiList;
for ( ; fi; fi = fi->next, columnEl = columnEl->next)
    {
    int jsonType = autoSqlToJsonType(fi->type);
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "name", fi->name);
    jsonWriteString(jw, "sqlType", fi->type);
    jsonWriteString(jw, "jsonType",jsonTypeStrings[jsonType]);
    if (columnEl && isNotEmpty(columnEl->comment))
	jsonWriteString(jw, "description", columnEl->comment);
    else
	jsonWriteString(jw, "description", "");
    jsonWriteObjectEnd(jw);
    }
jsonWriteListEnd(jw);
}

boolean trackHasData(struct trackDb *tdb)
/* check if this is actually a data track:
 *	TRUE when has data, FALSE if has no data
 * When NO trackDb, can't tell at this point, will check that later
 */
{
if (tdb)
    {
    if (tdbIsContainer(tdb) || tdbIsComposite(tdb)
	|| tdbIsCompositeView(tdb) || tdbIsSuper(tdb))
	return FALSE;
    else
	return TRUE;
    }
else
    return TRUE;	/* might be true */
}

boolean protectedTrack(struct trackDb *tdb, char *tableName)
/* determine if track is off-limits protected data */
{
boolean ret = FALSE;

/* this is a fixed list for now since there are so few and this
 * takes care of the situation where the tableName might be for a table
 * that has no trackDb entry
 */
if (sameOk(tableName, "cosmicRegions"))
  ret = TRUE;
else if (sameOk(tableName, "decipherRaw"))
  ret = TRUE;
else if (sameOk(tableName, "knownToDecipher"))
  ret = TRUE;
else if (sameOk(tableName, "knownCanonToDecipher"))
  ret = TRUE;
else if (sameOk(tableName, "decipherSnvsRaw"))
  ret = TRUE;
else if (sameOk(tableName, "lovd"))
  ret = TRUE;
else if (sameOk(tableName, "lovdShort"))
  ret = TRUE;
else if (sameOk(tableName, "lovdLong"))
  ret = TRUE;
else if (sameOk(tableName, "lovdComp"))
  ret = TRUE;
else if (sameOk(tableName, "hgmd"))
  ret = TRUE;
else
    {
    if (tdb)	/* may not have a tdb at this time */
	{
	char *tbOff = trackDbSetting(tdb, "tableBrowser");
	if (tbOff && startsWithWord("off", tbOff))
	    ret = TRUE;
	}
    }
return ret;
}

boolean isWiggleDataTable(char *type)
/* is this a wiggle data track table */
{
if (startsWith("wig", type))
    {
    if (startsWith("wigMaf", type))
	return FALSE;
    else
	return TRUE;
    }
else
     return FALSE;
}
