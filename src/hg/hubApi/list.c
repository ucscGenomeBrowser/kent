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
struct hubPublic *el = hubPublicLoadAll();
struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
freeMem(dataTime);
// redundant: jsonWriteString(jw, "tableName", hubPublicTableName());
char **columnNames = NULL;
char **columnTypes = NULL;
int columnCount = tableColumns(conn, jw, hubPublicTableName(), &columnNames, &columnTypes);
jsonWriteListStart(jw, "publicHubs");
for ( ; el != NULL; el = el->next )
    {
    hubPublicJsonData(jw, el, columnCount, columnNames);
    }
jsonWriteListEnd(jw);
jsonWriteObjectEnd(jw);
fputs(jw->dy->string,stdout);
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
int columnCount = tableColumns(conn, jw, "dbDb", &columnNames, &columnTypes);
if (columnCount)
    {
    }
// jsonWriteListStart(jw, "ucscGenomes");
jsonWriteObjectStart(jw, "ucscGenomes");
for ( el=dbList; el != NULL; el = el->next )
    {
    dbDbJsonData(jw, el, columnCount, columnNames);
    }
// jsonWriteListEnd(jw);
jsonWriteObjectEnd(jw);
jsonWriteObjectEnd(jw);
fputs(jw->dy->string,stdout);
hDisconnectCentral(&conn);
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
	apiErrAbort("can not find specified 'track=%s' for endpoint: /list/chromosomes?db=%s&track=%s", table, db, table);
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
        jsonWriteObjectEnd(jw);	/* top level */
        fputs(jw->dy->string,stdout);
	}
    else
	apiErrAbort("track '%s' is not a position track, request table without chrom specification, genome: '%s'", table, db);
    }
else
    {
    char *dataTime = sqlTableUpdate(conn, "chromInfo");
    time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
    replaceChar(dataTime, ' ', 'T');	/* ISO 8601 */
    struct chromInfo *ciList = createChromInfoList(NULL, db);
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
    jsonWriteObjectEnd(jw);	/* top level */
    fputs(jw->dy->string,stdout);
    }
hFreeConn(&conn);
}

static void recursiveTrackList(struct jsonWrite *jw, struct trackDb *tdb, char *type)
/* output trackDb tags, recursive when composite track */
{
jsonWriteListStart(jw, type);
struct trackDb *el = NULL;
for (el = tdb; el != NULL; el = el->next )
    {
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "track", el->track);
    jsonWriteString(jw, "shortLabel", el->shortLabel);
    jsonWriteString(jw, "type", el->type);
    jsonWriteString(jw, "longLabel", el->longLabel);
    if (el->parent)
        jsonWriteString(jw, "parent", el->parent->track);
    if (el->subtracks)
	recursiveTrackList(jw, el->subtracks, "subtracks");
    jsonWriteObjectEnd(jw);
    }
jsonWriteListEnd(jw);
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
recursiveTrackList(jw, tdbList, "tracks");
jsonWriteObjectEnd(jw);
fputs(jw->dy->string,stdout);
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
	apiErrAbort("must supply hubUrl='http:...' some URL to a hub for /list/hubGenomes");

    struct trackHub *hub = errCatchTrackHubOpen(hubUrl);
    if (hub->genomeList)
	{
        struct jsonWrite *jw = apiStartOutput();
	jsonWriteString(jw, "hubUrl", hubUrl);
        jsonWriteListStart(jw, "genomes");
	struct slName *theList = genomeList(hub, NULL, NULL);
	slNameSort(&theList);
	struct slName *el = theList;
	for ( ; el ; el = el->next )
	    {
	    jsonWriteString(jw, NULL, el->name);
	    }
	jsonWriteListEnd(jw);
	jsonWriteObjectEnd(jw);
        fputs(jw->dy->string,stdout);
	}
    }
else if (sameWord("tracks", words[1]))
    {
    char *hubUrl = cgiOptionalString("hubUrl");
    char *genome = cgiOptionalString("genome");
    char *db = cgiOptionalString("db");
    if (isEmpty(hubUrl) && isEmpty(db))
      apiErrAbort("ERROR: must supply hubUrl or db name to return track list");
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
            apiErrAbort("ERROR: must supply hubUrl='http:...' some URL to a hub for /list/genomes");
	}
    struct trackHub *hub = errCatchTrackHubOpen(hubUrl);
    if (hub->genomeList)
	{
	struct trackDb *tdbList = NULL;
	(void) genomeList(hub, &tdbList, genome);
	slSort(tdbList, trackDbTrackCmp);
        struct jsonWrite *jw = apiStartOutput();
	jsonWriteString(jw, "hubUrl", hubUrl);
	jsonWriteString(jw, "genome", genome);
        recursiveTrackList(jw, tdbList, "tracks");
	jsonWriteObjectEnd(jw);
        fputs(jw->dy->string,stdout);
	}
    }
else if (sameWord("chromosomes", words[1]))
    {
    char *hubUrl = cgiOptionalString("hubUrl");
//    char *genome = cgiOptionalString("genome");
    char *db = cgiOptionalString("db");
    if (isEmpty(hubUrl) && isEmpty(db))
        apiErrAbort("ERROR: must supply hubUrl or db name to return chromosome list");

    if (isEmpty(hubUrl))	// missing hubUrl implies UCSC database
	{
        chromInfoJsonOutput(stdout, db);
	return;
	}
    }
else
    apiErrAbort("do not recognize endpoint function: '/%s/%s'", words[0], words[1]);
}	/*	void apiList(char *words[MAX_PATH_INFO])        */
