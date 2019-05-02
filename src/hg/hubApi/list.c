/* manage endpoint /list/ functions */

#include "dataApi.h"

static void hubPublicJsonData(struct jsonWrite *jw, struct hubPublic *el,
  int columnCount, char **columnNames)
/* Print array data for one row from hubPublic table, order here
 * must be same as was stated in the columnName header element
 * This code should be in hg/lib/hubPublic.c (which does not exist)
 */
{
int i = 0;
jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, columnNames[i++], el->hubUrl);
jsonWriteString(jw, columnNames[i++], el->shortLabel);
jsonWriteString(jw, columnNames[i++], el->longLabel);
jsonWriteString(jw, columnNames[i++], el->registrationTime);
jsonWriteNumber(jw, columnNames[i++], (long long)el->dbCount);
jsonWriteString(jw, columnNames[i++], el->dbList);
jsonWriteString(jw, columnNames[i++], el->descriptionUrl);
jsonWriteObjectEnd(jw);
}

static void jsonPublicHubs()
/* output the hubPublic SQL table */
{
char *extraArgs = verifyLegalArgs(NULL); /* no extras allowed */
if (extraArgs)
    apiErrAbort(err400, err400Msg, "extraneous arguments found for function /list/publicHubs '%s'", extraArgs);

struct sqlConnection *conn = hConnectCentral();
char *dataTime = sqlTableUpdate(conn, hubPublicTableName());
time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
replaceChar(dataTime, ' ', 'T');	/* ISO 8601 */
struct hubPublic *el = hubPublicDbLoadAll();
struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
freeMem(dataTime);
// redundant: jsonWriteString(jw, "tableName", hubPublicTableName());
char **columnNames = NULL;
char **columnTypes = NULL;
int *jsonTypes = NULL;
int columnCount = tableColumns(conn, jw, hubPublicTableName(), &columnNames,
    &columnTypes, &jsonTypes);
jsonWriteListStart(jw, "publicHubs");
for ( ; el != NULL; el = el->next )
    {
    hubPublicJsonData(jw, el, columnCount, columnNames);
    }
jsonWriteListEnd(jw);
apiFinishOutput(0, NULL, jw);
hDisconnectCentral(&conn);
}

static void dbDbJsonData(struct jsonWrite *jw, struct dbDb *el,
    int columnCount, char **columnNames)
/* Print out dbDb table element in JSON format.
 * must be same as was stated in the columnName header element
 * This code should be over in hg/lib/dbDb.c
 */
{
int i = 0;
jsonWriteObjectStart(jw, el->name);
i++;
// redundant: jsonWriteString(jw, NULL, el->name);
jsonWriteString(jw, columnNames[i++], el->description);
jsonWriteString(jw, columnNames[i++], el->nibPath);
jsonWriteString(jw, columnNames[i++], el->organism);
jsonWriteString(jw, columnNames[i++], el->defaultPos);
jsonWriteNumber(jw, columnNames[i++], (long long)el->active);
jsonWriteNumber(jw, columnNames[i++], (long long)el->orderKey);
jsonWriteString(jw, columnNames[i++], el->genome);
jsonWriteString(jw, columnNames[i++], el->scientificName);
jsonWriteString(jw, columnNames[i++], el->htmlPath);
jsonWriteNumber(jw, columnNames[i++], (long long)el->hgNearOk);
jsonWriteNumber(jw, columnNames[i++], (long long)el->hgPbOk);
jsonWriteString(jw, columnNames[i++], el->sourceName);
jsonWriteNumber(jw, columnNames[i++], (long long)el->taxId);
jsonWriteObjectEnd(jw);
}

static void jsonDbDb()
/* output the dbDb SQL table */
{
char *extraArgs = verifyLegalArgs(NULL); /* no extras allowed */
if (extraArgs)
    apiErrAbort(err400, err400Msg, "extraneous arguments found for function /list/ucscGenomes '%s'", extraArgs);

struct sqlConnection *conn = hConnectCentral();
char *dataTime = sqlTableUpdate(conn, "dbDb");
time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
replaceChar(dataTime, ' ', 'T');	/* ISO 8601 */
struct dbDb *dbList = ucscDbDb();
struct dbDb *el;
struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
freeMem(dataTime);
char **columnNames = NULL;
char **columnTypes = NULL;
int *jsonTypes = NULL;
int columnCount = tableColumns(conn, jw, "dbDb", &columnNames, &columnTypes,
    &jsonTypes);
jsonWriteObjectStart(jw, "ucscGenomes");
for ( el=dbList; el != NULL; el = el->next )
    {
    dbDbJsonData(jw, el, columnCount, columnNames);
    }
jsonWriteObjectEnd(jw);
apiFinishOutput(0, NULL, jw);
hDisconnectCentral(&conn);
}

static void hubChromInfoJsonOutput(FILE *f, char *hubUrl, char *genome)
/* for given hubUrl list the chromosomes in the sequence for specified genome
 */
{
struct trackHub *hub = errCatchTrackHubOpen(hubUrl);
struct trackHubGenome *ge = NULL;
char *track = cgiOptionalString("track");

if (isEmpty(genome))
    apiErrAbort(err400, err400Msg, "must specify a 'genome=name' with hubUrl for endpoint: /list/chromosomes?hubUrl=%s;genome=<empty>", hubUrl);

struct trackHubGenome *foundGenome = NULL;

for (ge = hub->genomeList; ge; ge = ge->next)
    {
    if (sameOk(genome, ge->name))
	{
	foundGenome = ge;
	continue;	/* found genome */
	}
    }

if (NULL == foundGenome)
    apiErrAbort(err400, err400Msg, "can not find specified 'genome=%s' for endpoint: /list/chromosomes?hubUrl=%s;genome=%s", genome, hubUrl, genome);

struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "hubUrl", hubUrl);
jsonWriteString(jw, "genome", genome);
if (isNotEmpty(track))
    {
    jsonWriteString(jw, "track", track);
    struct trackDb *tdb = obtainTdb(foundGenome, NULL);
    if (NULL == tdb)
	apiErrAbort(err400, err400Msg, "failed to find a track hub definition in genome=%s for endpoint '/list/chromosomes' given hubUrl=%s'", genome, hubUrl);

    struct trackDb *thisTrack = findTrackDb(track, tdb);
    if (NULL == thisTrack)
	apiErrAbort(err400, err400Msg, "failed to find specified track=%s in genome=%s for endpoint '/list/chromosomes'  given hubUrl='%s'", track, genome, hubUrl);

    char *bigDataUrl = trackDbSetting(thisTrack, "bigDataUrl");
    struct bbiFile *bbi = bigFileOpen(thisTrack->type, bigDataUrl);
    struct bbiChromInfo *chrList = bbiChromList(bbi);
    slSort(chrList, chromInfoCmp);
    struct bbiChromInfo *el = chrList;
    jsonWriteNumber(jw, "chromCount", (long long)slCount(chrList));
    jsonWriteObjectStart(jw, "chromosomes");
    for ( ; el; el = el->next )
	{
	jsonWriteNumber(jw, el->name, (long long)el->size);
	}
    jsonWriteObjectEnd(jw);	/* chromosomes */
    }
else
    {
    struct chromInfo *ci = NULL;
    /* might be a track hub on a UCSC database */
    if (isEmpty(foundGenome->twoBitPath))
	{
	struct sqlConnection *conn = hAllocConnMaybe(foundGenome->trackHub->defaultDb);
	if (NULL == conn)
	    apiErrAbort(err400, err400Msg, "can not find 'genome=%s' for endpoint '/list/chromosomes", foundGenome->trackHub->defaultDb);
	else
	    hFreeConn(&conn);
	ci = createChromInfoList(NULL, foundGenome->trackHub->defaultDb);
	}
    else
	{
	ci = trackHubAllChromInfo(foundGenome->name);
	}
    slSort(ci, chromInfoCmp);
    jsonWriteNumber(jw, "chromCount", (long long)slCount(ci));
    jsonWriteObjectStart(jw, "chromosomes");
    struct chromInfo *el = ci;
    for ( ; el != NULL; el = el->next )
	{
	jsonWriteNumber(jw, el->chrom, (long long)el->size);
	}
    jsonWriteObjectEnd(jw);	/* chromosomes */
    }
apiFinishOutput(0, NULL, jw);
}

static char *validChromName(struct sqlConnection *conn, char *db, char *table,
   char **splitTableName, struct hTableInfo **tableInfo)
/* determine what the 'chrom' name should be for this table (aka track)
 * this function could be used in getData() also TBD
 */
{
static char *returnChrom = NULL;
/* to be determined if this table name is used or is some other name */
char *sqlTableName = cloneString(table);

/* 'track' name in trackDb usually refers to a SQL 'table' */
struct trackDb *tdb = obtainTdb(NULL, db);
struct trackDb *thisTrack = findTrackDb(table,tdb);
/* however, the trackDb might have a specific table defined instead */
char *tableName = trackDbSetting(thisTrack, "table");
if (isNotEmpty(tableName))
    {
    freeMem(sqlTableName);
    sqlTableName = cloneString(tableName);
    }

/* this function knows how to deal with split chromosomes, the NULL
 * here for the chrom name means to use the first chrom name in chromInfo
 */
struct hTableInfo *hti = hFindTableInfoWithConn(conn, NULL, sqlTableName);
*tableInfo = hti;	/* returning to caller */
/* check if table name needs to be modified */
if (hti && hti->isSplit)
    {
    char *defaultChrom = hDefaultChrom(db);
    char fullTableName[256];
    safef(fullTableName, sizeof(fullTableName), "%s_%s", defaultChrom, hti->rootName);
    freeMem(sqlTableName);
    sqlTableName = cloneString(fullTableName);
    *splitTableName = cloneString(fullTableName);	/* return to caller */
    }
else
    {
    *splitTableName = sqlTableName;	/* return to caller */
    }

/* may need to extend this in the future for other track types */
if (sqlColumnExists(conn, sqlTableName, "chrom"))	/* standard bed tables */
    returnChrom = cloneString("chrom");
else if (sqlColumnExists(conn, sqlTableName, "tName"))	/* track type psl */
    returnChrom = cloneString("tName");
else if (sqlColumnExists(conn, sqlTableName, "genoName"))	/* track type rmsk */
    returnChrom = cloneString("genoName");

return returnChrom;
}

static void chromInfoJsonOutput(FILE *f, char *db)
/* for given db, if there is a track, list the chromosomes in that track,
 * for no track, simply list the chromosomes in the sequence
 */
{
char *splitSqlTable = NULL;
struct hTableInfo *tableInfo = NULL;
char *chromName = NULL;
char *table = cgiOptionalString("track");
struct sqlConnection *conn = hAllocConnMaybe(db);
if (NULL == conn)
    apiErrAbort(err400, err400Msg, "can not find 'genome=%s' for endpoint '/list/chromosomes", db);

if (table)
    chromName = validChromName(conn, db, table, &splitSqlTable, &tableInfo);

/* in trackDb language: track == table */
/* punting on split tables, just return chromInfo */
if (table && chromName && ! (tableInfo && tableInfo->isSplit) )
    {
    if (! sqlTableExists(conn, splitSqlTable))
	apiErrAbort(err400, err400Msg, "can not find specified 'track=%s' for endpoint: /list/chromosomes?genome=%s;track=%s", table, db, table);

    if (sqlColumnExists(conn, splitSqlTable, chromName))
	{
	char *dataTime = sqlTableUpdate(conn, splitSqlTable);
	time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
	replaceChar(dataTime, ' ', 'T');	/* ISO 8601 */
        struct jsonWrite *jw = apiStartOutput();
	jsonWriteString(jw, "genome", db);
	jsonWriteString(jw, "track", table);
	jsonWriteString(jw, "dataTime", dataTime);
	jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
	freeMem(dataTime);
        struct slPair *list = NULL;
	char query[2048];
        sqlSafef(query, sizeof(query), "select distinct %s from %s", chromName, splitSqlTable);
	struct sqlResult *sr = sqlGetResult(conn, query);
	char **row;
	while ((row = sqlNextRow(sr)) != NULL)
    	{
            int size = hChromSize(db, row[0]);
	    slAddHead(&list, slPairNew(row[0], intToPt(size)));
    	}
	sqlFreeResult(&sr);
        slPairIntSort(&list);
        slReverse(&list);
        jsonWriteNumber(jw, "chromCount", (long long)slCount(list));
	jsonWriteObjectStart(jw, "chromosomes");
        struct slPair *el = list;
        for ( ; el != NULL; el = el->next )
            jsonWriteNumber(jw, el->name, (long long)ptToInt(el->val));
	jsonWriteObjectEnd(jw);	/* chromosomes */
	apiFinishOutput(0, NULL, jw);
	}
    else
	apiErrAbort(err400, err400Msg, "track '%s' is not a position track, request table without chrom specification, genome: '%s'", table, db);
    }
else if (table && !chromName)	/* only allowing position tables at this time */
	apiErrAbort(err400, err400Msg, "track '%s' is not a position track, request table without chrom specification, genome: '%s'", table, db);
else
    {
    char *dataTime = sqlTableUpdate(conn, "chromInfo");
    time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
    replaceChar(dataTime, ' ', 'T');	/* ISO 8601 */
    struct chromInfo *ciList = createChromInfoList(NULL, db);
    slSort(ciList, chromInfoCmp);
    struct chromInfo *el = ciList;
    struct jsonWrite *jw = apiStartOutput();
    jsonWriteString(jw, "genome", db);
    jsonWriteString(jw, "dataTime", dataTime);
    if (tableInfo && tableInfo->isSplit)	/* the split table punt */
	jsonWriteString(jw, "track", table);
    jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
    freeMem(dataTime);
    jsonWriteNumber(jw, "chromCount", (long long)slCount(ciList));
    jsonWriteObjectStart(jw, "chromosomes");
    for ( ; el != NULL; el = el->next )
	{
        jsonWriteNumber(jw, el->chrom, (long long)el->size);
	}
    jsonWriteObjectEnd(jw);	/* chromosomes */
    apiFinishOutput(0, NULL, jw);
    }
hFreeConn(&conn);
}

static void recursiveTrackList(struct jsonWrite *jw, struct trackDb *tdb)
/* output trackDb tags only for real tracks, not containers,
 * recursive when subtracks exist
 */
{
boolean isContainer = tdbIsComposite(tdb) || tdbIsCompositeView(tdb);

/* do *NOT* print containers when 'trackLeavesOnly' requested */
if (! (trackLeavesOnly && isContainer) )
    {
    jsonWriteObjectStart(jw, tdb->track);
    if (tdbIsComposite(tdb))
        jsonWriteString(jw, "compositeContainer", "TRUE");
    if (tdbIsCompositeView(tdb))
        jsonWriteString(jw, "compositeViewContainer", "TRUE");
    jsonWriteString(jw, "shortLabel", tdb->shortLabel);
    jsonWriteString(jw, "type", tdb->type);
    jsonWriteString(jw, "longLabel", tdb->longLabel);
    if (tdb->parent)
        jsonWriteString(jw, "parent", tdb->parent->track);
    if (tdb->settingsHash)
        {
        struct hashEl *hel;
        struct hashCookie hc = hashFirst(tdb->settingsHash);
        while ((hel = hashNext(&hc)) != NULL)
            {
            if (sameWord("track", hel->name))
                continue;	// already output in header
            if (isEmpty((char *)hel->val))
                jsonWriteString(jw, hel->name, "empty");
            else
                jsonWriteString(jw, hel->name, (char *)hel->val);
            }
        }

    if (tdb->subtracks)
	{
	struct trackDb *el = NULL;
	for (el = tdb->subtracks; el != NULL; el = el->next )
	    recursiveTrackList(jw, el);
	}

    jsonWriteObjectEnd(jw);
    }
else if (tdb->subtracks)
    {
    struct trackDb *el = NULL;
    for (el = tdb->subtracks; el != NULL; el = el->next )
	recursiveTrackList(jw, el);
    }
}	/*	static void recursiveTrackList()	*/

static void trackDbJsonOutput(char *db, FILE *f)
/* return track list from specified UCSC database name */
{
struct sqlConnection *conn = hAllocConnMaybe(db);
if (NULL == conn)
    apiErrAbort(err400, err400Msg, "can not find 'genome=%s' for endpoint '/list/tracks", db);

char *dataTime = sqlTableUpdate(conn, "trackDb");
time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
replaceChar(dataTime, ' ', 'T');	/* ISO 8601 */
hFreeConn(&conn);
struct trackDb *tdbList = obtainTdb(NULL, db);
struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "genome", db);
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
freeMem(dataTime);
struct trackDb *el = NULL;
for (el = tdbList; el != NULL; el = el->next )
    {
    recursiveTrackList(jw, el);
    }
apiFinishOutput(0, NULL, jw);
}	/*	static void trackDbJsonOutput(char *db, FILE *f)	*/

void apiList(char *words[MAX_PATH_INFO])
/* 'list' function words[1] is the subCommand */
{
if (sameWord("publicHubs", words[1]))
    jsonPublicHubs();
else if (sameWord("ucscGenomes", words[1]))
    jsonDbDb();
else if (sameWord("hubGenomes", words[1]))
    {
    char *extraArgs = verifyLegalArgs("hubUrl"); /* only one arg allowed */
    if (extraArgs)
	apiErrAbort(err400, err400Msg, "extraneous arguments found for function /list/hubGenomes '%s'", extraArgs);

    char *hubUrl = cgiOptionalString("hubUrl");
    if (isEmpty(hubUrl))
	apiErrAbort(err400, err400Msg, "must supply hubUrl='http:...' some URL to a hub for /list/hubGenomes");

    struct trackHub *hub = errCatchTrackHubOpen(hubUrl);
    if (hub->genomeList)
	{
	slNameSort((struct slName **)&hub->genomeList);
        struct jsonWrite *jw = apiStartOutput();
	jsonWriteString(jw, "hubUrl", hubUrl);
        jsonWriteObjectStart(jw, "genomes");
	struct trackHubGenome *el;
        for ( el = hub->genomeList; el; el = el->next)
	    {
	    jsonWriteObjectStart(jw, el->name);
	    jsonWriteString(jw, "organism", el->organism);
	    jsonWriteString(jw, "description", el->description);
	    jsonWriteString(jw, "trackDbFile", el->trackDbFile);
	    jsonWriteString(jw, "twoBitPath", el->twoBitPath);
	    jsonWriteString(jw, "groups", el->groups);
	    jsonWriteString(jw, "defaultPos", el->defaultPos);
	    jsonWriteNumber(jw, "orderKey", el->orderKey);
	    jsonWriteObjectEnd(jw);
	    }
	jsonWriteObjectEnd(jw);
	apiFinishOutput(0, NULL, jw);
	}
    }
else if (sameWord("tracks", words[1]))
    {
    char *extraArgs = verifyLegalArgs("genome;hubUrl;trackLeavesOnly");
    if (extraArgs)
	apiErrAbort(err400, err400Msg, "extraneous arguments found for function /list/tracks '%s'", extraArgs);

    char *hubUrl = cgiOptionalString("hubUrl");
    char *genome = cgiOptionalString("genome");
    char *db = cgiOptionalString("genome");
    if (isEmpty(hubUrl) && isNotEmpty(db))
	{
	struct sqlConnection *conn = hAllocConnMaybe(db);
        if (NULL == conn)
	    apiErrAbort(err400, err400Msg, "can not find 'genome=%s' for endpoint '/list/tracks", db);
	else
	    hFreeConn(&conn);
	}
    if (isEmpty(hubUrl) && isEmpty(db))
      apiErrAbort(err400, err400Msg, "missing hubUrl or genome name for endpoint /list/tracks");
    if (isEmpty(hubUrl))	// missing hubUrl implies UCSC database
	{
        trackDbJsonOutput(db, stdout);	// only need db for this function
	return;
	}
    if (isEmpty(genome) || isEmpty(hubUrl))
	{
        if (isEmpty(genome))
	    apiErrAbort(err400, err400Msg, "must supply genome='someName' the name of a genome in a hub for /list/tracks\n");
	if (isEmpty(hubUrl))
            apiErrAbort(err400, err400Msg, "must supply hubUrl='http:...' some URL to a hub for /list/tracks");
	}
    struct trackHub *hub = errCatchTrackHubOpen(hubUrl);
    struct trackHubGenome *hubGenome = findHubGenome(hub, genome,
	"/list/tracks", hubUrl);
    struct trackDb *tdbList = obtainTdb(hubGenome, NULL);
    struct jsonWrite *jw = apiStartOutput();
    jsonWriteString(jw, "hubUrl", hubUrl);
    jsonWriteObjectStart(jw, hubGenome->name);
    struct trackDb *el = NULL;
    for (el = tdbList; el != NULL; el = el->next )
	    {
	    recursiveTrackList(jw, el);
	    }
    jsonWriteObjectEnd(jw);
    apiFinishOutput(0, NULL, jw);
    }
else if (sameWord("chromosomes", words[1]))
    {
    char *extraArgs = verifyLegalArgs("genome;hubUrl;track");
    if (extraArgs)
	apiErrAbort(err400, err400Msg, "extraneous arguments found for function /list/chromosomes '%s'", extraArgs);

    char *hubUrl = cgiOptionalString("hubUrl");
    char *genome = cgiOptionalString("genome");
    char *db = cgiOptionalString("genome");
    if (isEmpty(hubUrl) && isNotEmpty(db))
	{
	struct sqlConnection *conn = hAllocConnMaybe(db);
        if (NULL == conn)
	    apiErrAbort(err400, err400Msg, "can not find 'genome=%s' for endpoint '/list/chromosomes", db);
	else
	    hFreeConn(&conn);
	}
    if (isEmpty(hubUrl) && isEmpty(db))
        apiErrAbort(err400, err400Msg, "must supply hubUrl or genome name for endpoint '/list/chromosomes", hubUrl, db);

    if (isEmpty(hubUrl))	// missing hubUrl implies UCSC database
	{
        chromInfoJsonOutput(stdout, db);
	return;
	}
    else
	{
        hubChromInfoJsonOutput(stdout, hubUrl, genome);
	return;
	}
    }
else
    apiErrAbort(err400, err400Msg, "do not recognize endpoint function: '/%s/%s'", words[0], words[1]);
}	/*	void apiList(char *words[MAX_PATH_INFO])        */
