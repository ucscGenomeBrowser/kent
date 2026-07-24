/* utility functions for data API business */

#include "trackHub.h"
#include "dataApi.h"
#include "net.h"
#include "wikiLink.h"

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
    char *errMessage;
    unsigned hubId = hubFindOrAddUrlInStatusTable(NULL, hubUrl, &errMessage);
    /* this hubFromIdNoAbort() call will update the hub status error message
     *    if it has recovered from previous errors.
     */
    struct hubConnectStatus *hubStatus = hubFromIdNoAbort(hubId);
    
    // if we got an error, throw error
    if (NULL == hubStatus)	/* this should not happen */
        errAbort("could not find hub %u in status table", hubId);
    if (!isEmpty(hubStatus->errorMessage))
	{
	stripChar(hubStatus->errorMessage, '\n');
	    errAbort("%s", hubStatus->errorMessage);
	}

    // use hubId in hubName
    char buffer[4096];
    safef(buffer, sizeof buffer, "hub_%d", hubId);
    hub = trackHubOpen(hubUrl, buffer);
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
/* return a full trackDb given the hub genome pointer, or ucsc database name */
{
struct trackDb *tdb = NULL;
if (db)
    tdb = hTrackDb(db);
else
    tdb = trackHubAddTracksGenome(genome);
/* remove the synthetic gcOnFly track it exists only for hgTracks display */
struct trackDb *filtered = NULL;
struct trackDb *next;
while (tdb)
    {
    next = tdb->next;
    if (sameString(GC_ON_FLY_TRACK_NAME, trackHubSkipHubName(tdb->track)))
	{
	tdb->next = NULL;
	}
    else
	{
	slAddHead(&filtered, tdb);
	}
    tdb = next;
    }
slReverse(&filtered);
slSort(filtered, trackDbTrackCmp);
return filtered;
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
	    if (sameOk(trackHubSkipHubName(subTrack->track), track))
		trackFound = subTrack;
	    }
	}
    if (sameOk(trackHubSkipHubName(trackFound->track), track))
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
    startsWithWord("bigRmsk", type) ||
    startsWithWord("bigDbSnp", type) ||
    startsWithWord("bigMaf", type) ||
    startsWithWord("bigChain", type) ||
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
    bbi = bigBedFileOpenAlias(bigDataUrl, chromAliasFindAliases);
else if (startsWith("bigWig", trackType))
    bbi = bigWigFileOpenAlias(bigDataUrl, chromAliasFindAliases);
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
    if (sameString(genome, trackHubSkipHubName(hubGenome->name)))
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
struct dyString *extras = dyStringNew(128);
struct cgiVar *varList = cgiVarList();
struct cgiVar *var = varList;
for ( ; var; var = var->next)
    {
    if (sameWord("cgiSpoof", var->name))
	continue;
    if (sameWord("debug", var->name))
	continue;
    if (sameWord("cmd", var->name))
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

static int genArkCmpName(const void *va, const void *vb)
/* Compare two genark elements: gcAccession, ignore case. */
{
const struct genark *a = *((struct genark **)va);
const struct genark *b = *((struct genark **)vb);
return strcasecmp(a->gcAccession, b->gcAccession);
}

static int dbDbCmpName(const void *va, const void *vb)
/* Compare two dbDb elements: name, ignore case. */
{
const struct dbDb *a = *((struct dbDb **)va);
const struct dbDb *b = *((struct dbDb **)vb);
return strcasecmp(a->name, b->name);
}

long long genArkSize()
/* return the number of rows in genark table */
{
char query[1024];
struct sqlConnection *conn = hConnectCentral();
sqlSafef(query, sizeof(query), "SELECT COUNT(*) FROM %s", genarkTableName());
return sqlQuickLongLong(conn, query);
}

struct genark *genArkList(char *oneAccession)
/* return the genark table as an slList, or just the one accession when given */
{
char query[1024];
struct sqlConnection *conn = hConnectCentral();
if (oneAccession)
    sqlSafef(query, sizeof(query), "SELECT * FROM %s WHERE gcAccession = '%s'", genarkTableName(), oneAccession);
else
    sqlSafef(query, sizeof(query), "SELECT * FROM %s ORDER BY gcAccession limit %d", genarkTableName(), maxItemsOutput);
struct genark *list = NULL, *el = NULL;
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = genarkLoad(row);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
slSort(&list, genArkCmpName);
return list;
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

void wigColumnTypes(struct jsonWrite *jw, char *track)
/* output column headers for a wiggle data output schema */
{
jsonWriteListStart(jw, track);

jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "name", "chrom");
jsonWriteString(jw, "sqlType", "varchar");
jsonWriteString(jw, "jsonType", "string");
jsonWriteString(jw, "description", "chromosome name");
jsonWriteObjectEnd(jw);

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
	    struct asColumn *columnEl, char *keyStr)
/* print out the SQL schema for this trackDb */
{
if (tdb && isWiggleDataTable(tdb->type))
    {
        wigColumnTypes(jw, keyStr);
    }
else
    {
    jsonWriteListStart(jw, keyStr);
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
    struct asObject *as, char *track)
/* show the column types from a big file autoSql definitions */
{
struct asColumn *columnEl = as->columnList;
jsonWriteListStart(jw, track);
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

boolean protectedTrack(char *db, struct trackDb *tdb, char *tableName)
/* determine if track is off-limits protected data */
{
// if the table is from hub, assume db should be from hub as well
if (isHubTrack(tableName))
    {
    char buffer[4096];

    safef(buffer, sizeof buffer, "hub_%d_%s",hubIdFromTrackName(tableName), db);

    db = cloneString(buffer);
    }

return cartTrackDbIsAccessDenied(db, tableName) || cartTrackDbIsNoGenome(db, tableName);
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

char *chrOrAlias(char *db, char *hubUrl)
/* get incoming chr name, may be an alias, return the native chr name
 * might be given a db, maybe not
 * might be given a hubUrl, maybe not
 */
{
char *cartChr = cgiOptionalString("chrom");
if (isEmpty(cartChr))
   return NULL;
char *chrom = cartChr;
if (isEmpty(hubUrl))
    {
    if (isEmpty(db))
       return chrom;
    chromAliasSetup(db);
    chrom = hgOfficialChromName(db, chrom);
    }
else
    {
/*
    not sure if the 'curated' hub situation has been solved yet
    if (sameString("hs1", db)) {
      chromAliasSetup("hub_25359_hs1");
    } else {
      chromAliasSetup(db);
    }
*/
    chrom = chromAliasFindNative(chrom);
    }
if (isEmpty(chrom))	// can't find it here, return the name from the cart
    chrom = cartChr;
return chrom;
}
struct trackHubGenome *hubGenome = NULL;

void hubAliasSetup(struct trackHubGenome *hubGenome)
/* see if this hub has an alias file and run chromAliasSetupBb() for it */
{
if (hubGenome->settingsHash)
    {
    char *aliasFile = hashFindVal(hubGenome->settingsHash, "chromAliasBb");
    char *absFileName = NULL;
    if (aliasFile)
       absFileName = trackHubRelativeUrl((hubGenome->trackHub)->url, aliasFile);
    if (absFileName)
        {
        chromAliasSetupBb(NULL, absFileName);
        }
    }
}

static struct dyString *textOutput = NULL;

void textLineOut(char *lineOut)
/* accumulate text lines for output in the dyString textOutput */
{
if (NULL == textOutput)
    {
    textOutput = dyStringNew(0);

    boolean doContext = (cgiOptionalInt(argSkipContext, 0)==0);
    if (doContext)
        {
        char outString[1024];
        time_t timeNow = time(NULL);
        struct tm tm;
        gmtime_r(&timeNow, &tm);
        safef(outString, sizeof(outString),
           "# downloadTime: \"%d:%02d:%02dT%02d:%02d:%02dZ\"",
            1900+tm.tm_year, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min,
             tm.tm_sec);
        dyStringPrintf(textOutput, "%s\n", outString);
        safef(outString, sizeof(outString), "# downloadTimeStamp: %lld",
            (long long) timeNow);
        dyStringPrintf(textOutput, "%s\n", outString);
        }
    }

dyStringPrintf(textOutput, "%s\n", lineOut);
}

void textFinishOutput()
/* all done with text output, print it all out */
{
puts("Content-Type:text/plain\n");
printf("%s", dyStringCannibalize(&textOutput));
}

static char *thisHostName()
/* Return this machine's own hostname via gethostname().  Unlike hHttpHost(),
 * which reflects the client-supplied HTTP_HOST/Host: header, this can't be
 * spoofed by the request and doesn't change depending on which round-robin
 * name (e.g. genome.ucsc.edu) the client used to reach this box -- using
 * hHttpHost() here made onGenomeRRMachine() misclassify RR machines reached
 * via the genome.ucsc.edu name, sending them into an infinite self-relay
 * loop. */
{
static char host[256];
static boolean init = FALSE;
if (!init)
    {
    if (gethostname(host, sizeof(host)) != 0)
        host[0] = '\0';
    init = TRUE;
    }
return host;
}

boolean inUcscEduDomain()
/* Return TRUE if this machine is configured as belonging to the ucsc.edu
 * domain, per hg.conf's central.domain setting (the same setting used for
 * the cross-host login cookie domain).  This is deployment config rather
 * than something derived from the machine's own OS hostname/DNS: a box's
 * local/cloud-assigned hostname and its UCSC-facing name (e.g.
 * genome-euro.ucsc.edu) can be unrelated DNS labels with no way to bridge
 * them via gethostname()/getaddrinfo(). */
{
char *domain = cfgOption(CFG_CENTRAL_DOMAIN);
return (domain != NULL && strstr(domain, ".ucsc.edu") != NULL);
}

boolean onGenomeRRMachine()
/* Return TRUE if running on one of the genome.ucsc.edu round-robin
 * machines (hgw0, hgw1, hgw2, ...), which carry SQL grants on
 * hgcentral.gbMembers.  Only meaningful within inUcscEduDomain(). */
{
if (hIsPrivateHost())	// stay on localhost for hgwdev and hgwbeta
    return TRUE;
char *hostName = thisHostName();
if (isEmpty(hostName))
    return FALSE;
if (startsWith("hgwbeta", hostName))
    return TRUE;
if (!startsWith("hgw", hostName))
    return FALSE;
char afterHgw = hostName[3];
return (afterHgw >= '0' && afterHgw <= '9');
}

char *submitOttoRequest(char *requestType, char *fromDb, char *toDb, char *email, char *comment)
/* Record a row in the ottoRequest table via hConnectCentral(), applying the
 * liftOver duplicate/daily-rate guards when requestType is "liftOver", or a
 * plain insert when requestType is "assembly".  Returns a status string:
 * "disabled", "duplicate", "rateLimited", "accepted", or "error".  Never
 * apiErrAbort()s -- used both for direct local calls (this host has
 * hgcentral write grants) and to answer relaySubmitOttoRequest() calls. */
{
char *ottoTable = cfgOption("ottoTable");
if (isEmpty(ottoTable))
    return "disabled";

struct sqlConnection *conn = hConnectCentral();
if (!sqlTableExists(conn, ottoTable))
    {
    hDisconnectCentral(&conn);
    return "disabled";
    }

char *status = "error";
if (sameString(requestType, "liftOver"))
    {
    struct dyString *dq = dyStringNew(0);
    sqlDyStringPrintf(dq,
        "SELECT COUNT(*) FROM %s WHERE requestType='liftOver' AND "
        "((fromDb='%s' AND toDb='%s') OR (fromDb='%s' AND toDb='%s'))",
        ottoTable, fromDb, toDb, toDb, fromDb);
    int dupCount = sqlQuickNum(conn, dyStringCannibalize(&dq));
    if (dupCount > 0)
        status = "duplicate";
    else
        {
        char *limitStr = cfgOption("liftDailyLimit");
        int dailyLimit = isNotEmpty(limitStr) ? atoi(limitStr) : 0;
        boolean rateLimited = FALSE;
        if (dailyLimit > 0)
            {
            struct dyString *q = dyStringNew(0);
            sqlDyStringPrintf(q,
                "SELECT COUNT(*) FROM %s WHERE requestType='liftOver' AND email='%s' "
                "AND DATE(requestTime) = CURDATE()",
                ottoTable, email);
            int todayCount = sqlQuickNum(conn, dyStringCannibalize(&q));
            rateLimited = (todayCount >= dailyLimit);
            }
        if (rateLimited)
            status = "rateLimited";
        else
            {
            /* Atomic insert with duplicate re-check, closing the race window
             * between the SELECT above and this INSERT. */
            struct dyString *update = dyStringNew(0);
            sqlDyStringPrintf(update,
                "INSERT INTO %s (requestType, fromDb, toDb, email, comment, requestTime, status, buildDir) "
                "SELECT 'liftOver', '%s', '%s', '%s', '%s', now(), 0, '' "
                "WHERE NOT EXISTS ("
                "  SELECT 1 FROM %s WHERE requestType='liftOver' AND "
                "  ((fromDb='%s' AND toDb='%s') OR (fromDb='%s' AND toDb='%s'))"
                ")",
                ottoTable, fromDb, toDb, email, comment,
                ottoTable, fromDb, toDb, toDb, fromDb);
            sqlUpdate(conn, dyStringCannibalize(&update));
            int rowsAffected = sqlQuickNum(conn, "SELECT ROW_COUNT()");
            status = (rowsAffected > 0) ? "accepted" : "duplicate";
            }
        }
    }
else if (sameString(requestType, "assembly"))
    {
    struct dyString *update = dyStringNew(0);
    sqlDyStringPrintf(update,
        "INSERT INTO %s (requestType, fromDb, toDb, email, comment, requestTime, status, buildDir) "
        "VALUES ('assembly', '%s', '%s', '%s', '%s', now(), 0, '')",
        ottoTable, fromDb, toDb, email, comment);
    sqlUpdate(conn, dyStringCannibalize(&update));
    status = "accepted";
    }
hDisconnectCentral(&conn);
return status;
}

char *relaySubmitOttoRequest(char *requestType, char *fromDb, char *toDb, char *email, char *comment)
/* Relay an ottoRequest submission to genome.ucsc.edu's /submitOttoRequest
 * endpoint, for hosts that lack local hgcentral write grants (see
 * inUcscEduDomain()/onGenomeRRMachine()).  Authenticates with the shared
 * hg.conf secret 'hubApi.relaySecret', which must also be configured on
 * genome.ucsc.edu.  Returns the same status vocabulary as
 * submitOttoRequest(): "disabled", "duplicate", "rateLimited", "accepted",
 * or "error" (including when the secret isn't configured locally, or the
 * relay call itself fails). */
{
char *secret = cfgOption("hubApi.relaySecret");
if (isEmpty(secret))
    return "error";

struct dyString *url = dyStringNew(0);
dyStringPrintf(url, "https://genome.ucsc.edu/cgi-bin/hubApi/submitOttoRequest"
    "?%s=%s&%s=%s&%s=%s&%s=%s&%s=%s&%s=%s",
    argRequestType, cgiEncode(requestType),
    argFromGenome, cgiEncode(fromDb),
    argToGenome, cgiEncode(toDb),
    argEmail, cgiEncode(email),
    argComment, cgiEncode(comment),
    argRelaySecret, cgiEncode(secret));

int sd = netUrlOpen(dyStringContents(url));
if (sd < 0)
    {
    dyStringFree(&url);
    return "error";
    }
char *redirectedUrl = NULL;
if (!netSkipHttpHeaderLinesWithRedirect(sd, dyStringContents(url), &redirectedUrl))
    {
    close(sd);
    dyStringFree(&url);
    return "error";
    }
dyStringFree(&url);
struct dyString *body = netSlurpFile(sd);
close(sd);
struct jsonElement *json = jsonParse(body->string);
dyStringFree(&body);
if (json == NULL)
    return "error";
char *status = jsonStringField(json, "status");
return isNotEmpty(status) ? cloneString(status) : "error";
}

void apiSubmitOttoRequest(char *words[MAX_PATH_INFO])
/* Internal server-to-server endpoint: record a row in the ottoRequest table
 * on behalf of a hubApi host that lacks its own hgcentral write grants (see
 * relaySubmitOttoRequest()).  Requires the shared hg.conf secret
 * 'hubApi.relaySecret' to match, since this performs the actual database
 * write without the bot-challenge cookie check that /liftRequest and
 * /assemblyRequest do locally -- that check already ran on the relaying
 * host before it got here.  Always answers 200 + a JSON "status" field;
 * never apiErrAbort()s for a duplicate/rateLimited outcome, so the relay
 * caller can distinguish it from a hard failure. */
{
char *secret = cfgOption("hubApi.relaySecret");
char *givenSecret = cgiOptionalString(argRelaySecret);
if (isEmpty(secret) || isEmpty(givenSecret) || !sameString(secret, givenSecret))
    apiErrAbort(err403, err403Msg, "not authorized for endpoint '/submitOttoRequest");

char *requestType = cgiOptionalString(argRequestType);
char *fromDb = cgiOptionalString(argFromGenome);
char *toDb = cgiOptionalString(argToGenome);
char *email = cgiOptionalString(argEmail);
char *comment = cgiOptionalString(argComment);
if (isEmpty(requestType) || isEmpty(fromDb) || isEmpty(toDb) || isEmpty(email) || isEmpty(comment))
    apiErrAbort(err400, err400Msg, "must have all arguments: %s, %s, %s, %s, %s for endpoint '/submitOttoRequest",
        argRequestType, argFromGenome, argToGenome, argEmail, argComment);
if (!sameString(requestType, "liftOver") && !sameString(requestType, "assembly"))
    apiErrAbort(err400, err400Msg, "unrecognized %s '%s' for endpoint '/submitOttoRequest", argRequestType, requestType);

char *status = submitOttoRequest(requestType, fromDb, toDb, email, comment);
struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "status", status);
apiFinishOutput(0, NULL, jw);
}

