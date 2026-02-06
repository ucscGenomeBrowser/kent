/* manage endpoint /getData/ functions */

#include "dataApi.h"

static unsigned wigTableDataOutput(struct jsonWrite *jw, char *database,
	char *table, char *chrom, int start, int end, unsigned itemsDone)
/* output wiggle data from the given table and specified chrom:start-end */
{
struct wiggleDataStream *wds = wiggleDataStreamNew();
wds->setMaxOutput(wds, maxItemsOutput);
wds->setChromConstraint(wds, chrom);
wds->setPositionConstraint(wds, start, end);
int operations = wigFetchAscii;
(void) wds->getData(wds, database, table, operations);
struct wigAsciiData *el;
unsigned itemCount = 0;
for (el = wds->ascii; (itemCount + itemsDone) < maxItemsOutput && el; el = el->next)
    {
    unsigned span = el->span;
    unsigned count = el->count;
    unsigned i = 0;
    struct asciiDatum *data = el->data;
    for ( ; (i < count) && ((itemCount + itemsDone) < maxItemsOutput); i++,data++)
	{
	int s = data->chromStart;
	int e = s + span;
	double val = data->value;
	if (jsonOutputArrays)
	    {
	    jsonWriteListStart(jw, NULL);
	    jsonWriteNumber(jw, NULL, (long long)s);
	    jsonWriteNumber(jw, NULL, (long long)e);
	    jsonWriteDouble(jw, NULL, val);
	    jsonWriteListEnd(jw);
	    }
	else
	    {
	    jsonWriteObjectStart(jw, NULL);
	    jsonWriteNumber(jw, "start", (long long)s);
	    jsonWriteNumber(jw, "end", (long long)e);
	    jsonWriteDouble(jw, "value", val);
	    jsonWriteObjectEnd(jw);
	    }
	++itemCount;
	}
    }
if ((itemCount + itemsDone) >= maxItemsOutput)
    reachedMaxItems = TRUE;
return itemCount;
}	/* static unsigned wigTableDataOutput(struct jsonWrite *jw, ...) */

static void jsonDatumOut(struct jsonWrite *jw, char *name, char *val,
    int jsonType)
/* output a json item, determine type, appropriate output, name can be NULL */
{
if (JSON_DOUBLE == jsonType)
    jsonWriteDouble(jw, name, sqlDouble(val));
else if (JSON_NUMBER == jsonType)
    jsonWriteNumber(jw, name, sqlLongLong(val));
else
    jsonWriteString(jw, name, val);
}

static unsigned sqlQueryJsonOutput(struct sqlConnection *conn,
    struct jsonWrite *jw, char *query, int columnCount, char **columnNames,
	int *jsonTypes, unsigned itemsDone)
/* with the SQL query set up, run through those selected items */
{
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = NULL;
unsigned itemCount = 0;
while ((itemCount+itemsDone) < maxItemsOutput && (row = sqlNextRow(sr)) != NULL)
    {
    int i = 0;
    if (jsonOutputArrays)
	{
	jsonWriteListStart(jw, NULL);
	for (i = 0; i < columnCount; ++i)
	    jsonDatumOut(jw, NULL, row[i], jsonTypes[i]);
	jsonWriteListEnd(jw);
	}
    else
	{
	jsonWriteObjectStart(jw, NULL);
	for (i = 0; i < columnCount; ++i)
	    jsonDatumOut(jw, columnNames[i], row[i], jsonTypes[i]);
	jsonWriteObjectEnd(jw);
	}
    ++itemCount;
    }
sqlFreeResult(&sr);
if ((itemCount + itemsDone) >= maxItemsOutput)
    reachedMaxItems = TRUE;
return itemCount;
}	/*	static unsigned sqlQueryJsonOutput(...) */

static void tableDataOutput(char *db, struct trackDb *tdb,
    struct sqlConnection *conn, struct jsonWrite *jw, char *track,
    char *chrom, unsigned start, unsigned end)
/* output the SQL table data for given track */
{
/* for MySQL select statements, name for 'chrom' 'start' 'end' to use
 *     for a table which has different names than that
 */
char chromName[256];
char startName[256];
char endName[256];

/* defaults, normal stuff */
safef(chromName, sizeof(chromName), "chrom");
safef(startName, sizeof(startName), "chromStart");
safef(endName, sizeof(endName), "chromEnd");

/* 'track' name in trackDb often refers to a SQL 'table' */
char *sqlTable = cloneString(track);
/* might have a specific table defined instead of the track name */
char *tableName = trackDbSetting(tdb, "table");
if (isNotEmpty(tableName))
    {
    freeMem(sqlTable);
    sqlTable = cloneString(tableName);
    jsonWriteString(jw, "sqlTable", sqlTable);
    }

/* to be determined if this name is used or changes */
char *splitSqlTable = cloneString(sqlTable);

/* this function knows how to deal with split chromosomes, the NULL
 * here for the chrom name means to use the first chrom name in chromInfo
 */
struct hTableInfo *hti = hFindTableInfoWithConn(conn, NULL, sqlTable);
if (debug && hti)
    {
    jsonWriteBoolean(jw, "isPos", hti->isPos);
    jsonWriteBoolean(jw, "isSplit", hti->isSplit);
    jsonWriteBoolean(jw, "hasBin", hti->hasBin);
    }
/* check if table name needs to be modified */
if (hti && hti->isSplit)
    {
    if (isNotEmpty(chrom))
	{
	char fullTableName[256];
	safef(fullTableName, sizeof(fullTableName), "%s_%s", chrom, hti->rootName);
	freeMem(splitSqlTable);
	splitSqlTable = cloneString(fullTableName);
	if (debug)
	    jsonWriteString(jw, "splitSqlTable", splitSqlTable);
	}
    else
	{
	char *defaultChrom = hDefaultChrom(db);
	char fullTableName[256];
	safef(fullTableName, sizeof(fullTableName), "%s_%s", defaultChrom, hti->rootName);
	freeMem(splitSqlTable);
	splitSqlTable = cloneString(fullTableName);
	if (debug)
	    jsonWriteString(jw, "splitSqlTable", splitSqlTable);
	}
    }

/* determine name for 'chrom' in table select */
if (! sqlColumnExists(conn, splitSqlTable, "chrom"))
    {
    if (sqlColumnExists(conn, splitSqlTable, "tName"))	// track type psl
	{
	safef(chromName, sizeof(chromName), "tName");
	safef(startName, sizeof(startName), "tStart");
	safef(endName, sizeof(endName), "tEnd");
	}
    else if (sqlColumnExists(conn, splitSqlTable, "genoName"))// track type rmsk
	{
	safef(chromName, sizeof(chromName), "genoName");
	safef(startName, sizeof(startName), "genoStart");
	safef(endName, sizeof(endName), "genoEnd");
	}
    }

if (sqlColumnExists(conn, splitSqlTable, "txStart"))	// track type genePred
    {
    safef(startName, sizeof(startName), "txStart");
    safef(endName, sizeof(endName), "txEnd");
    }

struct dyString *query = dyStringNew(64);

/* no chrom specified, return entire table */
if (isEmpty(chrom))
    {
    /* this setup here is for the case of non-split tables, will later
     * determine if split, and then will go through each chrom
     */
    sqlDyStringPrintf(query, "select * from %s", splitSqlTable);
    }
else if (0 == (start + end))	/* have chrom, no start,end == full chr */
    {
    if (! sqlColumnExists(conn, splitSqlTable, chromName))
	apiErrAbort(err400, err400Msg, "track '%s' is not a position track, request track without chrom specification, genome: '%s'", track, db);

    jsonWriteString(jw, "chrom", chrom);
    struct chromInfo *ci = hGetChromInfo(db, chrom);
    jsonWriteNumber(jw, "start", (long long)0);
    jsonWriteNumber(jw, "end", (long long)ci->size);
    if (tdb && isWiggleDataTable(tdb->type))
	{
	if (jsonOutputArrays || debug)
	    wigColumnTypes(jw);
	jsonWriteListStart(jw, chrom);
        itemsReturned += wigTableDataOutput(jw, db, splitSqlTable, chrom, 0, ci->size, 0);
	jsonWriteListEnd(jw);
        return;	/* DONE */
	}
    else
	{
	if (sqlColumnExists(conn, splitSqlTable, startName))
	    sqlDyStringPrintf(query, "select * from %s where %s='%s' order by %s", splitSqlTable, chromName, chrom, startName);
        else
	    sqlDyStringPrintf(query, "select * from %s where %s='%s'", splitSqlTable, chromName, chrom);
	}
    }
else	/* fully specified chrom:start-end */
    {
    if (! sqlColumnExists(conn, splitSqlTable, chromName))
	apiErrAbort(err400, err400Msg, "track '%s' is not a position track, request track without chrom and start,end specifications, genome: '%s'", track, db);

    jsonWriteString(jw, "chrom", chrom);
    if (tdb && isWiggleDataTable(tdb->type))
	{
	if (jsonOutputArrays || debug)
	    wigColumnTypes(jw);
	jsonWriteListStart(jw, chrom);
        itemsReturned += wigTableDataOutput(jw, db, splitSqlTable, chrom, start, end, 0);
	jsonWriteListEnd(jw);
        return;	/* DONE */
	}
    else
	{
	sqlDyStringPrintf(query, "select * from %s where ", splitSqlTable);
	if (sqlColumnExists(conn, splitSqlTable, startName))
	    {
	    if (hti->hasBin)
		hAddBinToQuery(start, end, query);
	    sqlDyStringPrintf(query, "%s='%s' AND %s > %u AND %s < %u ORDER BY %s", chromName, chrom, endName, start, startName, end, startName);
	    }
	else
	    apiErrAbort(err400, err400Msg, "track '%s' is not a position track, request track without start,end specification, genome: '%s'", track, db);
	}
    }

if (debug)
    jsonWriteString(jw, "select", query->string);

/* continuing, could be wiggle output with no chrom specified */
char **columnNames = NULL;
char **columnTypes = NULL;
int *jsonTypes = NULL;
struct asObject *as = asForTable(conn, splitSqlTable, tdb);
if (! as)
    apiErrAbort(err500, err500Msg, "can not find schema definition for table '%s', genome: '%s'", splitSqlTable, db);
struct asColumn *columnEl = as->columnList;
int asColumnCount = slCount(columnEl);
int columnCount = tableColumns(conn, splitSqlTable, &columnNames, &columnTypes, &jsonTypes);
if (jsonOutputArrays || debug)
    {
    outputSchema(tdb, jw, columnNames, columnTypes, jsonTypes, hti,
	columnCount, asColumnCount, columnEl);
    }

unsigned itemsDone = 0;

/* empty chrom, needs to run through all chrom names */
if (isEmpty(chrom))
    {
    jsonWriteObjectStart(jw, track);	/* begin track data output */
    char fullTableName[256];
    struct chromInfo *ciList = createChromInfoList(NULL, db);
    slSort(ciList, chromInfoCmp);
    struct chromInfo *ci = ciList;
    for ( ; ci && itemsDone < maxItemsOutput; ci = ci->next )
	{
	jsonWriteListStart(jw, ci->chrom);	/* starting a chrom output */
	dyStringFree(&query);
	query = dyStringNew(64);
	if (hti && hti->isSplit) /* when split, make up split chr name */
	    {
	    safef(fullTableName, sizeof(fullTableName), "%s_%s", ci->chrom, hti->rootName);
	    sqlDyStringPrintf(query, "select * from %s where %s='%s'", fullTableName, chromName, ci->chrom);
	    }
	else
	    sqlDyStringPrintf(query, "select * from %s where %s='%s'", splitSqlTable, chromName, ci->chrom);
	if (tdb && isWiggleDataTable(tdb->type))
            itemsDone += wigTableDataOutput(jw, db, splitSqlTable, chrom,
		start, end, itemsDone);
	else
	    itemsDone += sqlQueryJsonOutput(conn, jw, query->string,
		columnCount, columnNames, jsonTypes, itemsDone);
	jsonWriteListEnd(jw);	/* chrom data output list end */
	}
    if (itemsDone >= maxItemsOutput)
	reachedMaxItems = TRUE;
    jsonWriteObjectEnd(jw);	/* end track data output */
    itemsReturned += itemsDone;
    }
else
    {	/* a single chrom has been requested, run it */
    jsonWriteListStart(jw, track);	/* data output list starting */
    itemsDone += sqlQueryJsonOutput(conn, jw, query->string, columnCount,
	columnNames, jsonTypes, itemsDone);
    jsonWriteListEnd(jw);	/* data output list end */
    itemsReturned += itemsDone;
    }
if (reachedMaxItems)
    {
    downloadUrl = dyStringNew(128);
    dyStringPrintf(downloadUrl, "http://hgdownload.soe.ucsc.edu/goldenPath/%s/database/%s.txt.gz", db, splitSqlTable);
    }
dyStringFree(&query);
}	/*  static void tableDataOutput(char *db, struct trackDb *tdb, ... ) */

static unsigned bbiDataOutput(struct jsonWrite *jw, struct bbiFile *bbi,
    char *chrom, unsigned start, unsigned end, struct sqlFieldType *fiList,
     struct trackDb *tdb, unsigned itemsDone)
/* output bed data for one chrom in the given bbi file */
{
char *itemRgb = trackDbSetting(tdb, "itemRgb");
if (bbi->definedFieldCount > 8)
    itemRgb = "on";
int *jsonTypes = NULL;
int columnCount = slCount(fiList);
AllocArray(jsonTypes, columnCount);
int i = 0;
struct sqlFieldType *fi;
for ( fi = fiList; fi; fi = fi->next)
    {
    if (itemRgb)
	{
	if (8 == i && sameWord("on", itemRgb))
	    jsonTypes[i++] = JSON_STRING;
	else
	    jsonTypes[i++] = autoSqlToJsonType(fi->type);
	}
    else
	jsonTypes[i++] = autoSqlToJsonType(fi->type);
    }
struct lm *bbLm = lmInit(0);
struct bigBedInterval *iv, *ivList = NULL;
ivList = bigBedIntervalQuery(bbi,chrom, start, end, 0, bbLm);
char *row[bbi->fieldCount];
unsigned itemCount = 0;
for (iv = ivList; itemCount < maxItemsOutput && iv; iv = iv->next)
    {
    char startBuf[16], endBuf[16];
    bigBedIntervalToRow(iv, chrom, startBuf, endBuf, row, bbi->fieldCount);
    int i;
    struct sqlFieldType *fi = fiList;
    if (jsonOutputArrays)
        {
	jsonWriteListStart(jw, NULL);
	for (i = 0; i < bbi->fieldCount; ++i)
	    jsonDatumOut(jw, NULL, row[i], jsonTypes[i]);
	jsonWriteListEnd(jw);
        }
    else
        {
	jsonWriteObjectStart(jw, NULL);
	for (i = 0; i < bbi->fieldCount; ++i, fi = fi->next)
	    jsonDatumOut(jw, fi->name, row[i], jsonTypes[i]);
	jsonWriteObjectEnd(jw);
        }
    ++itemCount;
    }
    if (itemCount >= maxItemsOutput)
	reachedMaxItems = TRUE;
lmCleanup(&bbLm);
return itemCount;
}	/* static void bbiDataOutput(struct jsonWrite *jw, . . . ) */

static unsigned wigDataOutput(struct jsonWrite *jw, struct bbiFile *bwf,
    char *chrom, unsigned start, unsigned end)
/* output wig data for one chrom in the given bwf file, return itemCount out */
{
unsigned itemCount = 0;
struct lm *lm = lmInit(0);
struct bbiInterval *iv, *ivList = bigWigIntervalQuery(bwf, chrom, start, end, lm);
if (NULL == ivList)
    return itemCount;

jsonWriteListStart(jw, chrom);
for (iv = ivList; iv && itemCount < maxItemsOutput; iv = iv->next)
    {
    int s = max(iv->start, start);
    int e = min(iv->end, end);
    double val = iv->val;
    if (jsonOutputArrays)
	{
	jsonWriteListStart(jw, NULL);
	jsonWriteNumber(jw, NULL, (long long)s);
	jsonWriteNumber(jw, NULL, (long long)e);
	jsonWriteDouble(jw, NULL, val);
	jsonWriteListEnd(jw);
	}
    else
	{
	jsonWriteObjectStart(jw, NULL);
	jsonWriteNumber(jw, "start", (long long)s);
	jsonWriteNumber(jw, "end", (long long)e);
	jsonWriteDouble(jw, "value", val);
	jsonWriteObjectEnd(jw);
	}
    ++itemCount;
    }
jsonWriteListEnd(jw);
if (itemCount >= maxItemsOutput)
    reachedMaxItems = TRUE;
return itemCount;
}	/*	static unsigned wigDataOutput(...) */

static void bigWigData(struct jsonWrite *jw, struct bbiFile *bwf, char *chrom,
    unsigned start, unsigned end)
/* output the data for a bigWig bbi file */
{
struct bbiChromInfo *chromList = NULL;
unsigned itemsDone = 0;
if (isEmpty(chrom))
    {
    chromList = bbiChromList(bwf);
    struct bbiChromInfo *bci;
    for (bci = chromList; bci && (itemsDone < maxItemsOutput); bci = bci->next)
	{
	itemsDone += wigDataOutput(jw, bwf, bci->name, 0, bci->size);
	}
	if (itemsDone >= maxItemsOutput)
	    reachedMaxItems = TRUE;
    }
    else
	itemsDone += wigDataOutput(jw, bwf, chrom, start, end);

itemsReturned += itemsDone;
}

static void getHubTrackData(char *hubUrl)
/* return data from a hub track, optionally just one chrom data,
 *  optionally just one section of that chrom data
 */
{
char *genome = cgiOptionalString("genome");
char *trackArg = cgiOptionalString("track");
char *start = cgiOptionalString("start");
char *end = cgiOptionalString("end");

if (isEmpty(genome))
    apiErrAbort(err400, err400Msg, "missing genome=<name> for endpoint '/getData/track'  given hubUrl='%s'", hubUrl);
if (isEmpty(trackArg))
    apiErrAbort(err400, err400Msg, "missing track=<name> for endpoint '/getData/track'  given hubUrl='%s'", hubUrl);

struct trackHub *hub = errCatchTrackHubOpen(hubUrl);
struct trackHubGenome *hubGenome = findHubGenome(hub, genome, "/getData/track",
  hubUrl);

hubAliasSetup(hubGenome);

char *chrom = chrOrAlias(genome, hubUrl);

struct trackDb *tdb = obtainTdb(hubGenome, NULL);

if (NULL == tdb)
    apiErrAbort(err400, err400Msg, "failed to find a track hub definition in genome=%s for endpoint '/getData/track'  given hubUrl='%s'", genome, hubUrl);

struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "hubUrl", hubUrl);
jsonWriteString(jw, "genome", genome);

// allow optional comma sep list of tracks
char *tracks[100];
int numTracks = chopByChar(trackArg, ',', tracks, sizeof(tracks));
int i = 0;
for (i = 0; i < numTracks; i++)
    {
    char *track = cloneString(tracks[i]);
    struct trackDb *thisTrack = findTrackDb(track, tdb);
    if (NULL == thisTrack)
        apiErrAbort(err400, err400Msg, "failed to find specified track=%s in genome=%s for endpoint '/getData/track'  given hubUrl='%s'", track, genome, hubUrl);
    if (trackHasNoData(thisTrack))
        apiErrAbort(err400, err400Msg, "container track '%s' does not contain data, use the children of this container for data access", track);
    if (! isSupportedType(thisTrack->type))
        apiErrAbort(err415, err415Msg, "track type '%s' for track=%s not supported at this time", thisTrack->type, track);

    char *bigDataUrl = trackDbSetting(thisTrack, "bigDataUrl");
    struct bbiFile *bbi = bigFileOpen(thisTrack->type, bigDataUrl);
    if (NULL == bbi)
        apiErrAbort(err400, err400Msg, "track type %s management not implemented yet TBD track=%s in genome=%s for endpoint '/getData/track'  given hubUrl='%s'", thisTrack->type, track, genome, hubUrl);

    unsigned chromSize = 0;
    struct bbiChromInfo *chromList = NULL;
    if (isNotEmpty(chrom))
        {
        chromSize = bbiChromSize(bbi, chrom);
        if (0 == chromSize)
        apiErrAbort(err400, err400Msg, "can not find specified chrom=%s in bigBed file URL '%s', track=%s genome=%s for endpoint '/getData/track' given hubUrl='%s'", chrom, bigDataUrl, track, genome, hubUrl);
        jsonWriteNumber(jw, "chromSize", (long long)chromSize);
        }
    else
        {
        chromList = bbiChromList(bbi);
        jsonWriteNumber(jw, "chromCount", (long long)slCount(chromList));
        }

    unsigned uStart = 0;
    unsigned uEnd = chromSize;
    if ( ! (isEmpty(start) || isEmpty(end)) )
        {
        uStart = sqlUnsigned(start);
        uEnd = sqlUnsigned(end);
        jsonWriteNumber(jw, "start", uStart);
        jsonWriteNumber(jw, "end", uEnd);
        }

    jsonWriteString(jw, "bigDataUrl", bigDataUrl);
    jsonWriteString(jw, "trackType", thisTrack->type);

    if (allowedBigBedType(thisTrack->type))
        {
        struct asObject *as = bigBedAsOrDefault(bbi);
        if (! as)
        apiErrAbort(err500, err500Msg, "can not find schema definition for bigDataUrl '%s', track=%s genome: '%s' for endpoint '/getData/track' given hubUrl='%s'", bigDataUrl, track, genome, hubUrl);
        struct sqlFieldType *fiList = sqlFieldTypesFromAs(as);
        if (jsonOutputArrays || debug)
            bigColumnTypes(jw, fiList, as);

        jsonWriteListStart(jw, track);
        unsigned itemsDone = 0;
        if (isEmpty(chrom))
        {
        struct bbiChromInfo *bci;
        for (bci = chromList; bci && (itemsDone < maxItemsOutput); bci = bci->next)
            {
            itemsDone += bbiDataOutput(jw, bbi, bci->name, 0, bci->size,
            fiList, thisTrack, itemsDone);
            }
            if (itemsDone >= maxItemsOutput)
            reachedMaxItems = TRUE;
        }
        else
        itemsDone += bbiDataOutput(jw, bbi, chrom, uStart, uEnd, fiList,
            thisTrack, itemsDone);
        itemsReturned += itemsDone;
        jsonWriteListEnd(jw);
        }
    else if (startsWith("bigWig", thisTrack->type))
        {
        if (jsonOutputArrays || debug)
        wigColumnTypes(jw);
        jsonWriteObjectStart(jw, track);
        bigWigData(jw, bbi, chrom, uStart, uEnd);
        jsonWriteObjectEnd(jw);
        }
    bbiFileClose(&bbi);
    }
apiFinishOutput(0, NULL, jw);
}	/*	static void getHubTrackData(char *hubUrl)	*/

static void getTrackData()
/* return data from a track, optionally just one chrom data,
 *  optionally just one section of that chrom data
 */
{
char *db = cgiOptionalString("genome");
char *chrom = chrOrAlias(db, NULL);
char *start = cgiOptionalString("start");
char *end = cgiOptionalString("end");
/* 'track' name in trackDb often refers to a SQL 'table' */
char *trackArg = cgiOptionalString("track");
//char *sqlTable = cloneString(trackArg); /* might be something else */
     /* depends upon 'table' setting in track db, or split table business */

unsigned chromSize = 0;	/* maybe set later */
unsigned uStart = 0;
unsigned uEnd = chromSize;	/* maybe set later */
if ( ! (isEmpty(start) || isEmpty(end)) )
    {
    uStart = sqlUnsigned(start);
    uEnd = sqlUnsigned(end);
    if (uEnd < uStart)
	apiErrAbort(err400, err400Msg, "given start coordinate %u is greater than given end coordinate", uStart, uEnd);
    }

if (isEmpty(db))
    apiErrAbort(err400, err400Msg, "missing URL variable genome=<ucscDb> name for endpoint '/getData/track");
if (isEmpty(trackArg))
    apiErrAbort(err400, err400Msg, "missing URL variable track=<trackName> name for endpoint '/getData/track");

/* database existence has already been checked before now, might
 * have disappeared in the mean time (well, not really . . .)
 */
struct sqlConnection *conn = hAllocConnMaybe(db);
if (NULL == conn)
    apiErrAbort(err400, err400Msg, "can not find genome 'genome=%s' for endpoint '/getData/track", db);

struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "genome", db);

// load the tracks
struct trackDb *tdbList = NULL;
cartTrackDbInitForApi(NULL, db, &tdbList, NULL, TRUE);

// allow optional comma sep list of tracks
char *tracks[100];
int numTracks = chopByChar(trackArg, ',', tracks, sizeof(tracks));
int i = 0;
struct hash *trackHash = hashNew(0); // let hub tracks work
for (i = 0; i < numTracks; i++)
    {
    char *track = cloneString(tracks[i]);
    char *sqlTable = cloneString(track);

    if (cartTrackDbIsAccessDenied(db, sqlTable) ||
            (cartTrackDbIsNoGenome(db, sqlTable) && !(chrom && start && end)))
        apiErrAbort(err403, err403Msg, "this data request: 'db=%s;track=%s' is protected data, see also: https://genome.ucsc.edu/FAQ/FAQdownloads.html#download40", db, track);
    struct trackDb *thisTrack = tdbForTrack(db, track, &tdbList);
    if (NULL == thisTrack)
        {
        // maybe we have a hub track, try to look it up
        if (startsWith("hub_", track))
            {
            thisTrack = hubConnectAddHubForTrackAndFindTdb(db, track, &tdbList, trackHash);
            }
        if (!thisTrack && ! sqlTableExists(conn, track))
            apiErrAbort(err400, err400Msg, "can not find track=%s name for endpoint '/getData/track", track);
        }
    if (thisTrack && ! isSupportedType(thisTrack->type))
        apiErrAbort(err415, err415Msg, "track type '%s' for track=%s not supported at this time", thisTrack->type, track);
    if (trackHasNoData(thisTrack))
        apiErrAbort(err400, err400Msg, "container track '%s' does not contain data, use the children of this container", track);

    /* might be a big* track with no table */
    char *bigDataUrl = NULL;
    boolean tableTrack = TRUE;

    if (thisTrack)
        {
        bigDataUrl = trackDbSetting(thisTrack, "bigDataUrl");

        /* might have a specific table defined instead of the track name */
        char *tableName = trackDbSetting(thisTrack, "table");
        if (isNotEmpty(tableName))
            {
            freeMem(sqlTable);
            sqlTable = cloneString(tableName);
            }
        }
    else
        {
        freeMem(sqlTable);
        sqlTable = cloneString(track);
        }

    struct hTableInfo *hti = hFindTableInfoWithConn(conn, NULL, sqlTable);

    char *splitSqlTable = NULL;

    if (hti && hti->isSplit)
        {
        if (isNotEmpty(chrom))
            {
            char fullTableName[256];
            safef(fullTableName, sizeof(fullTableName), "%s_%s", chrom, hti->rootName);
            splitSqlTable = cloneString(fullTableName);
            }
        else
            {
            char *defaultChrom = hDefaultChrom(db);
            char fullTableName[256];
            safef(fullTableName, sizeof(fullTableName), "%s_%s", defaultChrom, hti->rootName);
            splitSqlTable = cloneString(fullTableName);
            }
        }

    if (! hTableOrSplitExists(db, sqlTable))
        {
        if (! bigDataUrl)
            apiErrAbort(err400, err400Msg, "can not find specified 'track=%s' for endpoint: /getData/track?genome=%s;track=%s", track, db, track);
        else
            tableTrack = FALSE;
        }

    if (tableTrack)
        {
        char *dataTime = NULL;
        if (hti && hti->isSplit)
            dataTime = sqlTableUpdate(conn, splitSqlTable);
        else
            dataTime = sqlTableUpdate(conn, sqlTable);
        time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
        replaceChar(dataTime, ' ', 'T');	/*	ISO 8601	*/
        jsonWriteString(jw, "dataTime", dataTime);
        jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
        if (differentStringNullOk(sqlTable,track))
            jsonWriteString(jw, "sqlTable", sqlTable);
        }
    if (thisTrack)
        jsonWriteString(jw, "trackType", thisTrack->type);

    jsonWriteString(jw, "track", track);
    if (debug)
        jsonWriteBoolean(jw, "jsonOutputArrays", jsonOutputArrays);

    char query[4096];
    struct bbiFile *bbi = NULL;
    struct bbiChromInfo *chromList = NULL;

    if (thisTrack && startsWith("big", thisTrack->type))
        {
        if (bigDataUrl)
            bbi = bigFileOpen(thisTrack->type, bigDataUrl);
        else
            {
            char quickReturn[2048];
            sqlSafef(query, sizeof(query), "select fileName from %s", sqlTable);
            if (sqlQuickQuery(conn, query, quickReturn, sizeof(quickReturn)))
                {
                bigDataUrl = cloneString(quickReturn);
                bbi = bigFileOpen(thisTrack->type, bigDataUrl);
                }
            }
        if (NULL == bbi)
            apiErrAbort(err400, err400Msg, "failed to find bigDataUrl=%s for track=%s in database=%s for endpoint '/getData/track'", bigDataUrl, track, db);
        if (isNotEmpty(chrom))
            {
            jsonWriteString(jw, "chrom", chrom);
            chromSize = bbiChromSize(bbi, chrom);
            if (0 == chromSize)
                apiErrAbort(err400, err400Msg, "can not find specified chrom=%s in bigWig file URL %s", chrom, bigDataUrl);
            if (uEnd < 1)
                uEnd = chromSize;
            jsonWriteNumber(jw, "chromSize", (long long)chromSize);
            }
    else
            {
            chromList = bbiChromList(bbi);
            jsonWriteNumber(jw, "chromCount", (long long)slCount(chromList));
            }
         jsonWriteString(jw, "bigDataUrl", bigDataUrl);
        }

    /* when start, end given, show them */
    if ( uEnd > uStart )
        {
        jsonWriteNumber(jw, "start", uStart);
        jsonWriteNumber(jw, "end", uEnd);
        }

    if (thisTrack && allowedBigBedType(thisTrack->type))
        {
        struct asObject *as = bigBedAsOrDefault(bbi);
        if (! as)
            apiErrAbort(err500, err500Msg, "can not find schema definition for bigDataUrl '%s', track=%s genome='%s' for endpoint '/getData/track'", bigDataUrl, track, db);
        struct sqlFieldType *fiList = sqlFieldTypesFromAs(as);
        if (jsonOutputArrays || debug)
            bigColumnTypes(jw, fiList, as);

        jsonWriteListStart(jw, track);
        unsigned itemsDone = 0;
        if (isEmpty(chrom))
            {
            struct bbiChromInfo *bci;
            for (bci = chromList; bci && (itemsDone < maxItemsOutput); bci = bci->next)
                {
                itemsDone += bbiDataOutput(jw, bbi, bci->name, 0, bci->size,
                    fiList, thisTrack, itemsDone);
                }
                if (itemsDone >= maxItemsOutput)
                    reachedMaxItems = TRUE;
            }
        else
            itemsDone += bbiDataOutput(jw, bbi, chrom, uStart, uEnd, fiList,
                    thisTrack, itemsDone);
        itemsReturned += itemsDone;
        jsonWriteListEnd(jw);
        }
    else if (thisTrack && startsWith("bigWig", thisTrack->type))
        {
        if (jsonOutputArrays || debug)
            wigColumnTypes(jw);

        jsonWriteObjectStart(jw, track);
        bigWigData(jw, bbi, chrom, uStart, uEnd);
        jsonWriteObjectEnd(jw);
        bbiFileClose(&bbi);
        }
    else
        tableDataOutput(db, thisTrack, conn, jw, track, chrom, uStart, uEnd);
    }

apiFinishOutput(0, NULL, jw);
hFreeConn(&conn);
}	/*	static void getTrackData()	*/

static void getSequenceData(char *db, char *hubUrl)
/* return DNA sequence, given at least a genome=name and chrom=chr,
   optionally start and end, might be a track hub for UCSC database  */
{
char *chrom = chrOrAlias(db, hubUrl);
char *start = cgiOptionalString("start");
char *end = cgiOptionalString("end");
boolean revComp = FALSE;
char *revCompStr = cgiOptionalString("revComp");
if (isNotEmpty(revCompStr))
    {
    if (SETTING_IS_ON(revCompStr))
        revComp = TRUE;
    }

long timeStart = clock1000();

if (isEmpty(chrom))
    apiErrAbort(err400, err400Msg, "missing URL chrom=<name> for endpoint '/getData/sequence?genome=%s'", db);
if (chromSeqFileExists(db, chrom))
    {
    struct chromInfo *ci = hGetChromInfo(db, chrom);
    unsigned chromSize = ci->size;
    struct dnaSeq *seq = NULL;

    if (isEmpty(start) || isEmpty(end))
	if (chromSize > MAX_DNA_LENGTH)
	    apiErrAbort(err400, err400Msg, "DNA sequence request %u (size of %s) too large, limit: %u for endpoint '/getData/sequence?genome=%s;chrom=%s'", chromSize, chrom, MAX_DNA_LENGTH, db, chrom);
	else
	    seq = hChromSeqMixed(db, chrom, 0, chromSize);
    else
	if ( (sqlSigned(end) - sqlSigned(start)) > MAX_DNA_LENGTH)
	    apiErrAbort(err400, err400Msg, "DNA sequence request %d too large, limit: %u for endpoint '/getData/sequence?genome=%s;chrom=%s;start=%s;end=%s'", sqlSigned(end) - sqlSigned(start), MAX_DNA_LENGTH, db, chrom, start, end);
	else
	    {
	    if (sqlSigned(end) > chromSize)
		apiErrAbort(err400, err400Msg, "DNA sequence request end coordinate %d past end of chromosome size %d for endpoint '/getData/sequence?genome=%s;chrom=%s;start=%s;end=%s'", sqlSigned(end), chromSize, db, chrom, start, end);
	    seq = hChromSeqMixed(db, chrom, sqlSigned(start), sqlSigned(end));
	    }

    long endTime = clock1000();
    long long et = endTime - timeStart;

    if (NULL == seq)
        apiErrAbort(err400, err400Msg, "can not find sequence for chrom=%s for endpoint '/getData/sequence?genome=%s;chrom=%s'", chrom, db, chrom);
    struct jsonWrite *jw = apiStartOutput();
    if (isNotEmpty(hubUrl))
	jsonWriteString(jw, "hubUrl", hubUrl);
    if (measureTiming)
	jsonWriteNumber(jw, "dnaFetchTimeMs", et);
    jsonWriteString(jw, "genome", db);
    jsonWriteString(jw, "chrom", chrom);
    if (isEmpty(start) || isEmpty(end))
	{
        jsonWriteNumber(jw, "start", (long long)0);
        jsonWriteNumber(jw, "end", (long long)ci->size);
	}
    else
	{
        jsonWriteNumber(jw, "start", (long long)sqlSigned(start));
        jsonWriteNumber(jw, "end", (long long)sqlSigned(end));
	}
    timeStart = clock1000();
    if (revComp)
	{
	reverseComplement(seq->dna, seq->size);
	jsonWriteBoolean(jw, "revComp", revComp);
	}
    jsonWriteString(jw, "dna", seq->dna);
    endTime = clock1000();
    et = endTime - timeStart;
    if (measureTiming)
	jsonWriteNumber(jw, "dnaJsonWriteTimeMs", et);
    apiFinishOutput(0, NULL, jw);
    freeDnaSeq(&seq);
    }
else
    apiErrAbort(err400, err400Msg, "can not find specified chrom=%s in sequence for endpoint '/getData/sequence?genome=%s;chrom=%s", chrom, db, chrom);
}	/*	static void getSequenceData(char *db, char *hubUrl)	*/

static void getHubSequenceData(char *hubUrl)
/* return DNA sequence, given at least a genome=name and chrom=chr,
   optionally start and end  */
{
char *genome = cgiOptionalString("genome");
char *start = cgiOptionalString("start");
char *end = cgiOptionalString("end");
boolean revComp = FALSE;
char *revCompStr = cgiOptionalString("revComp");
if (isNotEmpty(revCompStr))
    {
    if (SETTING_IS_ON(revCompStr))
        revComp = TRUE;
    }

if (isEmpty(genome))
    apiErrAbort(err400, err400Msg, "missing genome=<name> for endpoint '/getData/sequence'  given hubUrl='%s'", hubUrl);

struct trackHub *hub = errCatchTrackHubOpen(hubUrl);
struct trackHubGenome *hubGenome = NULL;
for (hubGenome = hub->genomeList; hubGenome; hubGenome = hubGenome->next)
    {
    if (sameString(genome, trackHubSkipHubName(hubGenome->name)))
	break;
    }
if (NULL == hubGenome)
    apiErrAbort(err400, err400Msg, "failed to find specified genome=%s for endpoint '/getData/sequence'  given hubUrl '%s'", genome, hubUrl);

hubAliasSetup(hubGenome);

char *chrom = chrOrAlias(genome, hubUrl);
if (isEmpty(chrom))
    apiErrAbort(err400, err400Msg, "missing chrom=<name> for endpoint '/getData/sequence?genome=%s' given hubUrl='%s'", genome, hubUrl);

/* might be a UCSC database track hub, where hubGenome=name is the database */
if (isEmpty(hubGenome->twoBitPath))
    {
    getSequenceData(hubGenome->name, hubUrl);
    return;
    }

/* this MaybeChromInfo will open the twoBit file, if not already done */
struct chromInfo *ci = trackHubMaybeChromInfo(hubGenome->name, chrom);
unsigned chromSize = ci->size;

if (NULL == ci)
    apiErrAbort(err400, err400Msg, "can not find sequence for chrom=%s for endpoint '/getData/sequence?genome=%s;chrom=%s' given hubUrl='%s'", chrom, genome, chrom, hubUrl);

struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "hubUrl", hubUrl);
jsonWriteString(jw, "genome", genome);
jsonWriteString(jw, "chrom", chrom);
int fragStart = 0;
int fragEnd = 0;
if (isNotEmpty(start) && isNotEmpty(end))
    {
    fragStart = sqlSigned(start);
    fragEnd = sqlSigned(end);
    if ((fragEnd - fragStart) > MAX_DNA_LENGTH)
	apiErrAbort(err400, err400Msg, "DNA sequence request %d too large, limit: %u for endpoint '/getData/sequence?genome=%s;chrom=%s;start=%d;end=%d' given hubUrl='%s'", fragEnd-fragEnd, MAX_DNA_LENGTH, genome, chrom, fragStart, fragEnd, hubUrl);
    if (fragEnd > chromSize)
	apiErrAbort(err400, err400Msg, "DNA sequence request end coordinate %d past end of chromosome size %d for endpoint '/getData/sequence?genome=%s;chrom=%s;start=%d;end=%d' given hubUrl='%s'", fragEnd, chromSize, genome, chrom, fragStart, fragEnd, hubUrl);
    jsonWriteNumber(jw, "start", (long long)fragStart);
    jsonWriteNumber(jw, "end", (long long)fragEnd);
    }
else
    {
    if (ci->size > MAX_DNA_LENGTH)
	apiErrAbort(err400, err400Msg, "DNA sequence request %d too large, limit: %u for endpoint '/getData/sequence?genome=%s;chrom=%s' given hubUrl='%s'", ci->size, MAX_DNA_LENGTH, genome, chrom, hubUrl);
    jsonWriteNumber(jw, "start", (long long)0);
    jsonWriteNumber(jw, "end", (long long)ci->size);
    }
struct dnaSeq *seq = twoBitReadSeqFrag(hubGenome->tbf, chrom, fragStart, fragEnd);
if (NULL == seq)
    {
    if (fragEnd > fragStart)
	apiErrAbort(err400, err400Msg, "can not find sequence for chrom=%s;start=%s;end=%s for endpoint '/getData/sequence?genome=%s;chrom=%s;start=%s;end=%s' give hubUrl='%s'", chrom, start, end, genome, chrom, start, end, hubUrl);
    else
	apiErrAbort(err400, err400Msg, "can not find sequence for chrom=%s for endpoint '/getData/sequence?genome=%s;chrom=%s' give hubUrl='%s'", chrom, genome, chrom, hubUrl);
    }
if (revComp)
    {
    reverseComplement(seq->dna, seq->size);
    jsonWriteBoolean(jw, "revComp", revComp);
    }
jsonWriteString(jw, "dna", seq->dna);
apiFinishOutput(0, NULL, jw);
}

void apiGetData(char *words[MAX_PATH_INFO])
/* 'getData' function, words[1] is the subCommand */
{
char *hubUrl = cgiOptionalString("hubUrl");
char *genome = cgiOptionalString("genome");
/* allow a GCx genome specified without hubUrl for GenArk genomes */
if (isEmpty(hubUrl) && isNotEmpty(genome))
    hubUrl = genarkUrl(genome);

if (sameWord("track", words[1]))
    {
    char *extraArgs = verifyLegalArgs(argGetDataTrack);
    if (extraArgs)
	apiErrAbort(err400, err400Msg, "extraneous arguments found for function /getData/track '%s'", extraArgs);

    if (isNotEmpty(hubUrl))
	getHubTrackData(hubUrl);
    else
	getTrackData();
    }
else if (sameWord("sequence", words[1]))
    {
    char *extraArgs = verifyLegalArgs(argGetDataSequence);
    if (extraArgs)
	apiErrAbort(err400, err400Msg, "extraneous arguments found for function /getData/sequence '%s'", extraArgs);

    if (isNotEmpty(hubUrl))
	getHubSequenceData(hubUrl);
    else
	{
	char *db = cgiOptionalString("genome");
	if (isEmpty(db))
	    apiErrAbort(err400, err400Msg, "missing URL genome=<ucscDb> name for endpoint '/getData/sequence");
	/* existence of db has already been proven before getting here */
	getSequenceData(db, NULL);
	}
    }
else
    apiErrAbort(err400, err400Msg, "do not recognize endpoint function: '/%s/%s'", words[0], words[1]);
}
