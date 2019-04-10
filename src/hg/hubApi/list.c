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
#ifdef NOT
jsonWriteListStart(jw, NULL);
jsonWriteString(jw, NULL, el->hubUrl);
jsonWriteString(jw, NULL, el->shortLabel);
jsonWriteString(jw, NULL, el->longLabel);
jsonWriteString(jw, NULL, el->registrationTime);
jsonWriteNumber(jw, NULL, (long long)el->dbCount);
jsonWriteString(jw, NULL, el->dbList);
jsonWriteString(jw, NULL, el->descriptionUrl);
jsonWriteListEnd(jw);
#endif
}

static void jsonPublicHubs()
/* output the hubPublic SQL table */
{
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
int columnCount = tableColumns(conn, jw, hubPublicTableName(), &columnNames, &columnTypes, &jsonTypes);
jsonWriteListStart(jw, "publicHubs");
for ( ; el != NULL; el = el->next )
    {
    hubPublicJsonData(jw, el, columnCount, columnNames);
    }
jsonWriteListEnd(jw);
apiFinishOutput(0, NULL, jw);
hDisconnectCentral(&conn);
}

static void dbDbJsonData(struct jsonWrite *jw, struct dbDb *el, int columnCount,
  char **columnNames)
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
#ifdef NOT
jsonWriteListStart(jw, NULL);
jsonWriteString(jw, NULL, el->name);
jsonWriteString(jw, NULL, el->description);
jsonWriteString(jw, NULL, el->nibPath);
jsonWriteString(jw, NULL, el->organism);
jsonWriteString(jw, NULL, el->defaultPos);
jsonWriteNumber(jw, NULL, (long long)el->active);
jsonWriteNumber(jw, NULL, (long long)el->orderKey);
jsonWriteString(jw, NULL, el->genome);
jsonWriteString(jw, NULL, el->scientificName);
jsonWriteString(jw, NULL, el->htmlPath);
jsonWriteNumber(jw, NULL, (long long)el->hgNearOk);
jsonWriteNumber(jw, NULL, (long long)el->hgPbOk);
jsonWriteString(jw, NULL, el->sourceName);
jsonWriteNumber(jw, NULL, (long long)el->taxId);
jsonWriteListEnd(jw);
#endif
}

static void jsonDbDb()
/* output the dbDb SQL table */
{
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
// not needed: jsonWriteString(jw, "tableName", "dbDb");
char **columnNames = NULL;
char **columnTypes = NULL;
int *jsonTypes = NULL;
int columnCount = tableColumns(conn, jw, "dbDb", &columnNames, &columnTypes, &jsonTypes);
// jsonWriteListStart(jw, "ucscGenomes");
jsonWriteObjectStart(jw, "ucscGenomes");
for ( el=dbList; el != NULL; el = el->next )
    {
    dbDbJsonData(jw, el, columnCount, columnNames);
    }
// jsonWriteListEnd(jw);
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
    apiErrAbort(400, "Bad Request", "must specify a 'genome=name' with hubUrl for endpoint: /list/chromosomes?hubUrl=%s&genome=<empty>", hubUrl);

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
    apiErrAbort(400, "Bad Request", "can not find specified 'genome=%s' for endpoint: /list/chromosomes?hubUrl=%s&genome=%s", genome, hubUrl, genome);

struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "hubUrl", hubUrl);
jsonWriteString(jw, "genome", genome);
if (isNotEmpty(track))
    {
    jsonWriteString(jw, "track", track);
    struct trackDb *tdb = obtainTdb(foundGenome, NULL);
    if (NULL == tdb)
	apiErrAbort(400, "Bad Request", "failed to find a track hub definition in genome=%s for endpoint '/list/chromosomes' give hubUrl=%s'", genome, hubUrl);

    struct trackDb *thisTrack = findTrackDb(track, tdb);
    if (NULL == thisTrack)
	apiErrAbort(400, "Bad Request", "failed to find specified track=%s in genome=%s for endpoint '/getdata/track'  given hubUrl='%s'", track, genome, hubUrl);

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
    struct chromInfo *ci = trackHubAllChromInfo(foundGenome->name);
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

#ifdef NOT
char *table = cgiOptionalString("track");
struct sqlConnection *conn = hAllocConn(db);
/* in trackDb language: track == table */
if (table)
    {
    if (! sqlTableExists(conn, table))
	apiErrAbort(400, "Bad Request", "can not find specified 'track=%s' for endpoint: /list/chromosomes?db=%s&track=%s", table, db, table);
    if (sqlColumnExists(conn, table, "chrom"))
	{
	char *dataTime = sqlTableUpdate(conn, table);
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
        sqlSafef(query, sizeof(query), "select distinct chrom from %s", table);
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
	apiErrAbort(400, "Bad Request", "track '%s' is not a position track, request table without chrom specification, genome: '%s'", table, db);
    }
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
#endif
}

static void chromInfoJsonOutput(FILE *f, char *db)
/* for given db, if there is a track, list the chromosomes in that track,
 * for no track, simply list the chromosomes in the sequence
 */
{
char *table = cgiOptionalString("track");
struct sqlConnection *conn = hAllocConn(db);
/* in trackDb language: track == table */
if (table)
    {
    if (! sqlTableExists(conn, table))
	apiErrAbort(400, "Bad Request", "can not find specified 'track=%s' for endpoint: /list/chromosomes?db=%s&track=%s", table, db, table);
    if (sqlColumnExists(conn, table, "chrom"))
	{
	char *dataTime = sqlTableUpdate(conn, table);
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
        sqlSafef(query, sizeof(query), "select distinct chrom from %s", table);
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
	apiErrAbort(400, "Bad Request", "track '%s' is not a position track, request table without chrom specification, genome: '%s'", table, db);
    }
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
    jsonWriteObjectEnd(jw);
    }

if (tdb->subtracks)
    {
    struct trackDb *el = NULL;
    for (el = tdb->subtracks; el != NULL; el = el->next )
	{
	recursiveTrackList(jw, el);
	}
    }
}	/*	static void recursiveTrackList()	*/

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

static void trackDbJsonOutput(char *db, FILE *f)
/* return track list from specified UCSC database name */
{
struct sqlConnection *conn = hAllocConn(db);
char *dataTime = sqlTableUpdate(conn, "trackDb");
time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
replaceChar(dataTime, ' ', 'T');	/* ISO 8601 */
hFreeConn(&conn);
struct trackDb *tdbList = obtainTdb(NULL, db);
struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "db", db);
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
    char *hubUrl = cgiOptionalString("hubUrl");
    if (isEmpty(hubUrl))
	apiErrAbort(400, "Bad Request", "must supply hubUrl='http:...' some URL to a hub for /list/hubGenomes");

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
    char *hubUrl = cgiOptionalString("hubUrl");
    char *genome = cgiOptionalString("genome");
    char *db = cgiOptionalString("db");
    if (isEmpty(hubUrl) && isEmpty(db))
      apiErrAbort(400, "Bad Request", "ERROR: must supply hubUrl or db name to return track list");
    if (isEmpty(hubUrl))	// missing hubUrl implies UCSC database
	{
        trackDbJsonOutput(db, stdout);	// only need db for this function
	return;
	}
    if (isEmpty(genome) || isEmpty(hubUrl))
	{
        if (isEmpty(genome))
	    warn("# must supply genome='someName' the name of a genome in a hub for /list/tracks\n");
	if (isEmpty(hubUrl))
            apiErrAbort(400, "Bad Request", "ERROR: must supply hubUrl='http:...' some URL to a hub for /list/genomes");
	}
    struct trackHub *hub = errCatchTrackHubOpen(hubUrl);
    if (hub->genomeList)
	{
	struct trackDb *tdbList = obtainTdb(hub->genomeList, NULL);
	slSort(tdbList, trackDbTrackCmp);
        struct jsonWrite *jw = apiStartOutput();
	jsonWriteString(jw, "hubUrl", hubUrl);
	jsonWriteObjectStart(jw, genome);
	struct trackDb *el = NULL;
	for (el = tdbList; el != NULL; el = el->next )
	    {
	    recursiveTrackList(jw, el);
	    }
	jsonWriteObjectEnd(jw);
	apiFinishOutput(0, NULL, jw);
	}
    }
else if (sameWord("chromosomes", words[1]))
    {
    char *hubUrl = cgiOptionalString("hubUrl");
    char *genome = cgiOptionalString("genome");
    char *db = cgiOptionalString("db");
    if (isEmpty(hubUrl) && isEmpty(db))
        apiErrAbort(400, "Bad Request", "ERROR: must '%s' '%s' supply hubUrl or db name to return chromosome list", hubUrl, db);

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
    apiErrAbort(400, "Bad Request", "do not recognize endpoint function: '/%s/%s'", words[0], words[1]);
}	/*	void apiList(char *words[MAX_PATH_INFO])        */
