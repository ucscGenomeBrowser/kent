/* manage endpoint /list/ functions */

#include "dataApi.h"

static boolean tableColumns(struct jsonWrite *jw, char *table)
/* output the column names for the given table
 * return: TRUE on error, FALSE on success
 */
{
jsonWriteListStart(jw, "columnNames");
char query[1024];
struct sqlConnection *conn = hConnectCentral();
sqlSafef(query, sizeof(query), "desc %s", table);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
row = sqlNextRow(sr);
if (NULL == row)
    {
    jsonErrAbort("ERROR: can not 'desc' table '%s'", table);
    return TRUE;
    }
while ((row = sqlNextRow(sr)) != NULL)
    jsonWriteString(jw, NULL, row[0]);
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
jsonWriteListEnd(jw);
return FALSE;
}

static void hubPublicJsonData(struct jsonWrite *jw, struct hubPublic *el)
/* Print array data for one row from hubPublic table, order here
 * must be same as was stated in the columnName header element
 *  TODO: need to figure out how to use the order of the columns as
 *        they are in the 'desc' request
 */
{
jsonWriteListStart(jw, NULL);
jsonWriteString(jw, NULL, el->hubUrl);
jsonWriteString(jw, NULL, el->shortLabel);
jsonWriteString(jw, NULL, el->longLabel);
jsonWriteString(jw, NULL, el->registrationTime);
jsonWriteNumber(jw, NULL, (long long)el->dbCount);
jsonWriteString(jw, NULL, el->dbList);
jsonWriteString(jw, NULL, el->descriptionUrl);
jsonWriteListEnd(jw);
}

static void jsonPublicHubs()
/* output the hubPublic SQL table */
{
struct sqlConnection *conn = hConnectCentral();
char *dataTime = sqlTableUpdate(conn, hubPublicTableName());
hDisconnectCentral(&conn);
time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
replaceChar(dataTime, ' ', 'T');
struct hubPublic *el = hubPublicLoadAll();
struct jsonWrite *jw = jsonStartOutput();
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
freeMem(dataTime);
jsonWriteString(jw, "tableName", hubPublicTableName());
tableColumns(jw, hubPublicTableName());
jsonWriteListStart(jw, "publicHubData");
for ( ; el != NULL; el = el->next )
    {
    hubPublicJsonData(jw, el);
    }
jsonWriteListEnd(jw);
jsonWriteObjectEnd(jw);
fputs(jw->dy->string,stdout);
}

static void dbDbJsonData(struct jsonWrite *jw, struct dbDb *el)
/* Print out dbDb table element in JSON format.
 * must be same as was stated in the columnName header element
 *  TODO: need to figure out how to use the order of the columns as
 *        they are in the 'desc' request
 */
{
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
}

static void jsonDbDb()
/* output the dbDb SQL table */
{
struct sqlConnection *conn = hConnectCentral();
char *dataTime = sqlTableUpdate(conn, "dbDb");
hDisconnectCentral(&conn);
time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
replaceChar(dataTime, ' ', 'T');
struct dbDb *dbList = ucscDbDb();
struct dbDb *el;
struct jsonWrite *jw = jsonStartOutput();
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
freeMem(dataTime);
jsonWriteString(jw, "tableName", "dbDb");
tableColumns(jw, "dbDb");
jsonWriteListStart(jw, "ucscGenomes");
for ( el=dbList; el != NULL; el = el->next )
    {
    dbDbJsonData(jw, el);
    }
jsonWriteListEnd(jw);
jsonWriteObjectEnd(jw);
fputs(jw->dy->string,stdout);
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
	jsonErrAbort("ERROR: endpoint: /list/chromosomes?db=%&table=%s ERROR table does not exist", db, table);
    if (sqlColumnExists(conn, table, "chrom"))
	{
	char *dataTime = sqlTableUpdate(conn, table);
	time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
	replaceChar(dataTime, ' ', 'T');
        struct jsonWrite *jw = jsonStartOutput();
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
	{
	jsonErrAbort("ERROR: table '%s' is not a position table, no chromosomes for genome: '%s'", table, db);
	}
    }
else
    {
    char *dataTime = sqlTableUpdate(conn, "chromInfo");
    time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
    replaceChar(dataTime, ' ', 'T');
    struct chromInfo *ciList = createChromInfoList(NULL, db);
    struct chromInfo *el = ciList;
    struct jsonWrite *jw = jsonStartOutput();
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
struct trackDb *el;
for (el = tdb; el != NULL; el = el->next )
    {
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "track", el->track);
    jsonWriteString(jw, "shortLabel", el->shortLabel);
    jsonWriteString(jw, "type", el->type);
    jsonWriteString(jw, "longLabel", el->longLabel);
    if (tdbIsComposite(el))
	{
	recursiveTrackList(jw, el->subtracks, "subtracks");
	}
    if (tdb->parent && tdbIsSuperTrackChild(el))
	jsonWriteString(jw, "superTrack", "TRUE");
    jsonWriteObjectEnd(jw);
    }
jsonWriteListEnd(jw);
}
int trackDbTrackCmp(const void *va, const void *vb)
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
replaceChar(dataTime, ' ', 'T');
hFreeConn(&conn);
struct trackDb *tdbList = hTrackDb(db);
struct jsonWrite *jw = jsonStartOutput();
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
	jsonErrAbort("must supply hubUrl='http:...' some URL to a hub for /list/hubGenomes");

    struct trackHub *hub = NULL;
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
	{
	hub = trackHubOpen(hubUrl, "");
        }
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	{
	jsonErrAbort("error opening hubUrl: '%s', '%s'", hubUrl,  errCatch->message->string);
	}
    errCatchFree(&errCatch);
    if (hub->genomeList)
	{
        struct jsonWrite *jw = jsonStartOutput();
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
      jsonErrAbort("ERROR: must supply hubUrl or db name to return track list");
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
            jsonErrAbort("ERROR: must supply hubUrl='http:...' some URL to a hub for /list/genomes");
	}
    struct trackHub *hub = trackHubOpen(hubUrl, "");
    if (hub->genomeList)
	{
	struct trackDb *dbTrackList = NULL;
	(void) genomeList(hub, &dbTrackList, genome);
	slSort(dbTrackList, trackDbTrackCmp);
        struct jsonWrite *jw = jsonStartOutput();
	jsonWriteString(jw, "hubUrl", hubUrl);
	jsonWriteString(jw, "genome", genome);
        recursiveTrackList(jw, dbTrackList, "tracks");
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
        jsonErrAbort("ERROR: must supply hubUrl or db name to return chromosome list");

    if (isEmpty(hubUrl))	// missing hubUrl implies UCSC database
	{
        chromInfoJsonOutput(stdout, db);
	return;
	}
    }
else
    jsonErrAbort("do not recognize endpoint function: '/%s/%s'", words[0], words[1]);
}	/*	void apiList(char *words[MAX_PATH_INFO])        */
