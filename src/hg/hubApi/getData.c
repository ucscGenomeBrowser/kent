/* manage endpoint /getData/ functions */

#include "dataApi.h"

static void wigTableDataOutput(struct jsonWrite *jw, char *database, char *table, char *chrom, int start, int end)
/* output wiggle data from the given table and specified chrom:start-end */
{
struct wiggleDataStream *wds = wiggleDataStreamNew();
wds->setMaxOutput(wds, maxItemsOutput);
wds->setChromConstraint(wds, chrom);
wds->setPositionConstraint(wds, start, end);
int operations = wigFetchAscii;
long long valuesMatched = wds->getData(wds, database, table, operations);
jsonWriteNumber(jw, "valuesMatched", valuesMatched);
struct wigAsciiData *el;
jsonWriteListStart(jw, chrom);
unsigned itemCount = 0;
for (el = wds->ascii; itemCount < maxItemsOutput && el; el = el->next)
    {
    jsonWriteObjectStart(jw, NULL);
    int s = el->data->chromStart;
    int e = s + el->span;
    double val = el->data->value;
    jsonWriteNumber(jw, "start", (long long)s);
    jsonWriteNumber(jw, "end", (long long)e);
    jsonWriteDouble(jw, "value", val);
    jsonWriteObjectEnd(jw);
    ++itemCount;
    }
jsonWriteListEnd(jw);
}

static void jsonDatumOut(struct jsonWrite *jw, char *name, char *val,
    int jsonType)
/* output a json item, determine type, appropriate output */
{
if (JSON_DOUBLE == jsonType)
    jsonWriteDouble(jw, name, sqlDouble(val));
else if (JSON_NUMBER == jsonType)
    jsonWriteNumber(jw, name, sqlLongLong(val));
else
    jsonWriteString(jw, name, val);
}

static void tableDataOutput(char *db, struct trackDb *tdb,
    struct sqlConnection *conn, struct jsonWrite *jw, char *track,
    char *chrom, unsigned start, unsigned end)
/* output the SQL table data for given track */
{
char query[4096];

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

/* might have a specific table defined instead of the track name */
char *sqlTable = cloneString(track);
char *tableName = trackDbSetting(tdb, "table");
if (isNotEmpty(tableName))
    {
    freeMem(sqlTable);
    sqlTable = cloneString(tableName);
    jsonWriteString(jw, "sqlTable", sqlTable);
    }

/* determine name for 'chrom' in table select */
if (! sqlColumnExists(conn, sqlTable, "chrom"))
    {
    if (sqlColumnExists(conn, sqlTable, "tName"))	// track type psl
	{
	safef(chromName, sizeof(chromName), "tName");
	safef(startName, sizeof(startName), "tStart");
	safef(endName, sizeof(endName), "tEnd");
	}
    else if (sqlColumnExists(conn, sqlTable, "genoName"))// track type rmsk
	{
	safef(chromName, sizeof(chromName), "genoName");
	safef(startName, sizeof(startName), "genoStart");
	safef(endName, sizeof(endName), "genoEnd");
	}
    }

if (sqlColumnExists(conn, sqlTable, "txStart"))	// track type genePred
    {
    safef(startName, sizeof(startName), "txStart");
    safef(endName, sizeof(endName), "txEnd");
    }

/* no chrom specified, return entire table */
if (isEmpty(chrom))
    sqlSafef(query, sizeof(query), "select * from %s", sqlTable);
else if (0 == (start + end))	/* have chrom, no start,end == full chr */
    {
//    boolean useTname = FALSE;
    /* need to extend the chrom column check to allow tName also */
    if (! sqlColumnExists(conn, sqlTable, chromName))
	apiErrAbort("track '%s' is not a position track, request track without chrom specification, genome: '%s'", track, db);

    jsonWriteString(jw, "chrom", chrom);
    struct chromInfo *ci = hGetChromInfo(db, chrom);
    jsonWriteNumber(jw, "start", (long long)0);
    jsonWriteNumber(jw, "end", (long long)ci->size);
    sqlSafef(query, sizeof(query), "select * from %s where %s='%s'", sqlTable, chromName, chrom);
    }
else	/* fully specified chrom:start-end */
    {
    jsonWriteString(jw, "chrom", chrom);
    jsonWriteNumber(jw, "start", (long long)start);
    jsonWriteNumber(jw, "end", (long long)end);
    if (startsWith("wig", tdb->type))
	{
        wigTableDataOutput(jw, db, sqlTable, chrom, start, end);
        return;	/* DONE */
	}
    else
	{
	sqlSafef(query, sizeof(query), "select * from %s where %s='%s' AND %s > %u AND %s < %u", sqlTable, chromName, chrom, endName, start, startName, end);
	}
    }

if (debug)
    jsonWriteString(jw, "select", query);

/* continuing, not a wiggle output */
char **columnNames = NULL;
char **columnTypes = NULL;
int *jsonTypes = NULL;
int columnCount = tableColumns(conn, jw, sqlTable, &columnNames, &columnTypes, &jsonTypes);
if (debug)
    {
    jsonWriteObjectStart(jw, "columnTypes");
    int i = 0;
    for (i = 0; i < columnCount; ++i)
	{
	char bothTypes[1024];
	safef(bothTypes, sizeof(bothTypes), "%s - %s", columnTypes[i], jsonTypeStrings[jsonTypes[i]]);
	jsonWriteString(jw, columnNames[i], bothTypes);
	}
    jsonWriteObjectEnd(jw);
    }
jsonWriteListStart(jw, track);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = NULL;
unsigned itemCount = 0;
while (itemCount < maxItemsOutput && (row = sqlNextRow(sr)) != NULL)
    {
    jsonWriteObjectStart(jw, NULL);
    int i = 0;
    for (i = 0; i < columnCount; ++i)
	{
	jsonDatumOut(jw, columnNames[i], row[i], jsonTypes[i]);
	}
    jsonWriteObjectEnd(jw);
    ++itemCount;
    }
sqlFreeResult(&sr);
jsonWriteListEnd(jw);
}	/*  static void tableDataOutput(char *db, struct trackDb *tdb, ... ) */

static void bedDataOutput(struct jsonWrite *jw, struct bbiFile *bbi,
    char *chrom, unsigned start, unsigned end, struct sqlFieldType *fiList,
     struct trackDb *tdb)
/* output bed data for one chrom in the given bbi file */
{
char *itemRgb = trackDbSetting(tdb, "itemRgb");
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
    jsonWriteObjectStart(jw, NULL);
    int i;
    struct sqlFieldType *fi = fiList;
    for (i = 0; i < bbi->fieldCount; ++i)
        {
        jsonDatumOut(jw, fi->name, row[i], jsonTypes[i]);
        fi = fi->next;
        }
    jsonWriteObjectEnd(jw);
    ++itemCount;
    }
lmCleanup(&bbLm);
}

static void wigDataOutput(struct jsonWrite *jw, struct bbiFile *bwf,
    char *chrom, unsigned start, unsigned end)
/* output wig data for one chrom in the given bwf file */
{
struct lm *lm = lmInit(0);
struct bbiInterval *iv, *ivList = bigWigIntervalQuery(bwf, chrom, start, end, lm);
if (NULL == ivList)
    return;

unsigned itemCount = 0;

jsonWriteListStart(jw, chrom);
for (iv = ivList; iv && itemCount < maxItemsOutput; iv = iv->next)
    {
    jsonWriteObjectStart(jw, NULL);
    int s = max(iv->start, start);
    int e = min(iv->end, end);
    double val = iv->val;
    jsonWriteNumber(jw, "start", (long long)s);
    jsonWriteNumber(jw, "end", (long long)e);
    jsonWriteDouble(jw, "value", val);
    jsonWriteObjectEnd(jw);
    ++itemCount;
    }
jsonWriteListEnd(jw);
}

static void wigData(struct jsonWrite *jw, struct bbiFile *bwf, char *chrom,
    unsigned start, unsigned end)
/* output the data for a bigWig bbi file */
{
struct bbiChromInfo *chromList = NULL;
// struct bbiSummaryElement sum = bbiTotalSummary(bwf);
if (isEmpty(chrom))
    {
    chromList = bbiChromList(bwf);
    struct bbiChromInfo *bci;
    for (bci = chromList; bci; bci = bci->next)
	{
	wigDataOutput(jw, bwf, bci->name, 0, bci->size);
	}
    }
    else
	wigDataOutput(jw, bwf, chrom, start, end);
}

static void bigColumnTypes(struct jsonWrite *jw, struct sqlFieldType *fiList)
/* show the column types from a big file autoSql definitions */
{
jsonWriteObjectStart(jw, "columnTypes");
struct sqlFieldType *fi = fiList;
for ( ; fi; fi = fi->next)
    {
    char bothTypes[1024];
    int jsonType = autoSqlToJsonType(fi->type);
    safef(bothTypes, sizeof(bothTypes), "%s - %s", fi->type, jsonTypeStrings[jsonType]);
    jsonWriteString(jw, fi->name, bothTypes);
    }
jsonWriteObjectEnd(jw);
}

static void getHubTrackData(char *hubUrl)
/* return data from a hub track, optionally just one chrom data,
 *  optionally just one section of that chrom data
 */
{
char *genome = cgiOptionalString("genome");
char *track = cgiOptionalString("track");
char *chrom = cgiOptionalString("chrom");
char *start = cgiOptionalString("start");
char *end = cgiOptionalString("end");

if (isEmpty(genome))
    apiErrAbort("missing genome=<name> for endpoint '/getData/track'  given hubUrl='%s'", hubUrl);
if (isEmpty(track))
    apiErrAbort("missing track=<name> for endpoint '/getData/track'  given hubUrl='%s'", hubUrl);

struct trackHub *hub = errCatchTrackHubOpen(hubUrl);
struct trackHubGenome *hubGenome = NULL;
for (hubGenome = hub->genomeList; hubGenome; hubGenome = hubGenome->next)
    {
    if (sameString(genome, hubGenome->name))
	break;
    }
if (NULL == hubGenome)
    apiErrAbort("failed to find specified genome=%s for endpoint '/getData/track'  given hubUrl '%s'", genome, hubUrl);

struct trackDb *tdb = obtainTdb(hubGenome, NULL);

if (NULL == tdb)
    apiErrAbort("failed to find a track hub definition in genome=%s for endpoint '/getData/track'  given hubUrl='%s'", genome, hubUrl);

struct trackDb *thisTrack = findTrackDb(track, tdb);

if (NULL == thisTrack)
    apiErrAbort("failed to find specified track=%s in genome=%s for endpoint '/getData/track'  given hubUrl='%s'", track, genome, hubUrl);

char *bigDataUrl = trackDbSetting(thisTrack, "bigDataUrl");
struct bbiFile *bbi = bigFileOpen(thisTrack->type, bigDataUrl);
if (NULL == bbi)
    apiErrAbort("track type %s management not implemented yet TBD track=%s in genome=%s for endpoint '/getData/track'  given hubUrl='%s'", track, genome, hubUrl);

struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "hubUrl", hubUrl);
jsonWriteString(jw, "genome", genome);
// jsonWriteString(jw, "track", track);
unsigned chromSize = 0;
struct bbiChromInfo *chromList = NULL;
if (isNotEmpty(chrom))
    {
//    jsonWriteString(jw, "chrom", chrom);
    chromSize = bbiChromSize(bbi, chrom);
    if (0 == chromSize)
	apiErrAbort("can not find specified chrom=%s in bigBed file URL %s", chrom, bigDataUrl);
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
    struct sqlFieldType *fiList = sqlFieldTypesFromAs(as);
    if (debug)
        bigColumnTypes(jw, fiList);

    jsonWriteListStart(jw, track);
    if (isEmpty(chrom))
	{
	struct bbiChromInfo *bci;
	for (bci = chromList; bci; bci = bci->next)
	    {
	    bedDataOutput(jw, bbi, bci->name, 0, bci->size, fiList, thisTrack);
	    }
	}
    else
	bedDataOutput(jw, bbi, chrom, uStart, uEnd, fiList, thisTrack);
    jsonWriteListEnd(jw);
    }
else if (startsWith("bigWig", thisTrack->type))
    {
    jsonWriteObjectStart(jw, track);
    wigData(jw, bbi, chrom, uStart, uEnd);
    jsonWriteObjectEnd(jw);
    }
bbiFileClose(&bbi);
jsonWriteObjectEnd(jw);
fputs(jw->dy->string,stdout);
}	/*	static void getHubTrackData(char *hubUrl)	*/

static void getTrackData()
/* return data from a track, optionally just one chrom data,
 *  optionally just one section of that chrom data
 */
{
char *db = cgiOptionalString("db");
char *chrom = cgiOptionalString("chrom");
char *start = cgiOptionalString("start");
char *end = cgiOptionalString("end");
/* 'track' name in trackDb refers to a SQL 'table' */
char *track = cgiOptionalString("track");
char *sqlTable = cloneString(track);
unsigned chromSize = 0;	/* maybe set later */
unsigned uStart = 0;
unsigned uEnd = chromSize;	/* maybe set later */
if ( ! (isEmpty(start) || isEmpty(end)) )
    {
    uStart = sqlUnsigned(start);
    uEnd = sqlUnsigned(end);
    if (uEnd < uStart)
	apiErrAbort("given start coordinate %u is greater than given end coordinate", uStart, uEnd);
    }

if (isEmpty(db))
    apiErrAbort("missing URL variable db=<ucscDb> name for endpoint '/getData/track");
if (isEmpty(track))
    apiErrAbort("missing URL variable track=<trackName> name for endpoint '/getData/track");

struct trackDb *thisTrack = hTrackDbForTrackAndAncestors(db, track);
if (NULL == thisTrack)
    apiErrAbort("can not find track=%s name for endpoint '/getData/track", track);

/* might be a big* track with no table */
char *bigDataUrl = trackDbSetting(thisTrack, "bigDataUrl");
boolean tableTrack = TRUE;
/* might have a specific table defined instead of the track name */
char *tableName = trackDbSetting(thisTrack, "table");
if (isNotEmpty(tableName))
    {
    freeMem(sqlTable);
    sqlTable = cloneString(tableName);
    }

struct sqlConnection *conn = hAllocConn(db);
if (! sqlTableExists(conn, sqlTable))
    {
    if (! bigDataUrl)
	apiErrAbort("can not find specified 'track=%s' for endpoint: /getData/track?db=%s&track=%s", track, db, track);
    else
	tableTrack = FALSE;
    }

struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "db", db);
if (tableTrack)
    {
    char *dataTime = sqlTableUpdate(conn, sqlTable);
    time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
    replaceChar(dataTime, ' ', 'T');	/*	ISO 8601	*/
    jsonWriteString(jw, "dataTime", dataTime);
    jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
    if (differentStringNullOk(sqlTable,track))
	jsonWriteString(jw, "sqlTable", sqlTable);
    }
jsonWriteString(jw, "trackType", thisTrack->type);
jsonWriteString(jw, "track", track);

char query[4096];
struct bbiFile *bbi = NULL;
struct bbiChromInfo *chromList = NULL;

if (startsWith("big", thisTrack->type))
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
	apiErrAbort("failed to find bigDataUrl=%s for track=%s in database=%s for endpoint '/getData/track'", bigDataUrl, track, db);
    if (isNotEmpty(chrom))
	{
	jsonWriteString(jw, "chrom", chrom);
	chromSize = bbiChromSize(bbi, chrom);
	if (0 == chromSize)
	    apiErrAbort("can not find specified chrom=%s in bigWig file URL %s", chrom, bigDataUrl);
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

if (allowedBigBedType(thisTrack->type))
    {
    struct asObject *as = bigBedAsOrDefault(bbi);
    struct sqlFieldType *fiList = sqlFieldTypesFromAs(as);
    if (debug)
        bigColumnTypes(jw, fiList);

    jsonWriteListStart(jw, track);
    if (isEmpty(chrom))
	{
	struct bbiChromInfo *bci;
	for (bci = chromList; bci; bci = bci->next)
	    {
	    bedDataOutput(jw, bbi, bci->name, 0, bci->size, fiList, thisTrack);
	    }
	}
    else
	bedDataOutput(jw, bbi, chrom, uStart, uEnd, fiList, thisTrack);
    jsonWriteListEnd(jw);
    }
else if (startsWith("bigWig", thisTrack->type))
    {
    jsonWriteObjectStart(jw, track);
    wigData(jw, bbi, chrom, uStart, uEnd);
    jsonWriteObjectEnd(jw);
    bbiFileClose(&bbi);
    }
else
    tableDataOutput(db, thisTrack, conn, jw, track, chrom, uStart, uEnd);

jsonWriteObjectEnd(jw);	/* closing the overall global object */
fputs(jw->dy->string,stdout);
hFreeConn(&conn);
}

static void getSequenceData(char *db, char *hubUrl)
/* return DNA sequence, given at least a db=name and chrom=chr,
   optionally start and end, might be a track hub for UCSC database  */
{
char *chrom = cgiOptionalString("chrom");
char *start = cgiOptionalString("start");
char *end = cgiOptionalString("end");

if (isEmpty(chrom))
    apiErrAbort("missing URL chrom=<name> for endpoint '/getData/sequence?db=%s'", db);
if (chromSeqFileExists(db, chrom))
    {
    struct chromInfo *ci = hGetChromInfo(db, chrom);
    struct dnaSeq *seq = NULL;
    if (isEmpty(start) || isEmpty(end))
	seq = hChromSeqMixed(db, chrom, 0, 0);
    else
	seq = hChromSeqMixed(db, chrom, sqlSigned(start), sqlSigned(end));
    if (NULL == seq)
        apiErrAbort("can not find sequence for chrom=%s for endpoint '/getData/sequence?db=%s&chrom=%s'", chrom, db, chrom);
    struct jsonWrite *jw = apiStartOutput();
    if (isNotEmpty(hubUrl))
	jsonWriteString(jw, "hubUrl", hubUrl);
    jsonWriteString(jw, "db", db);
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
    jsonWriteString(jw, "dna", seq->dna);
    jsonWriteObjectEnd(jw);
    fputs(jw->dy->string,stdout);
    freeDnaSeq(&seq);
    }
else
    apiErrAbort("can not find specified chrom=%s in sequence for endpoint '/getData/sequence?db=%s&chrom=%s", chrom, db, chrom);
}	/*	static void getSequenceData(char *db, char *hubUrl)	*/

static void getHubSequenceData(char *hubUrl)
/* return DNA sequence, given at least a genome=name and chrom=chr,
   optionally start and end  */
{
char *genome = cgiOptionalString("genome");
char *chrom = cgiOptionalString("chrom");
char *start = cgiOptionalString("start");
char *end = cgiOptionalString("end");

if (isEmpty(genome))
    apiErrAbort("missing genome=<name> for endpoint '/getData/sequence'  given hubUrl='%s'", hubUrl);
if (isEmpty(chrom))
    apiErrAbort("missing chrom=<name> for endpoint '/getData/sequence?genome=%s' given hubUrl='%s'", genome, hubUrl);

struct trackHub *hub = errCatchTrackHubOpen(hubUrl);
struct trackHubGenome *hubGenome = NULL;
for (hubGenome = hub->genomeList; hubGenome; hubGenome = hubGenome->next)
    {
    if (sameString(genome, hubGenome->name))
	break;
    }
if (NULL == hubGenome)
    apiErrAbort("failed to find specified genome=%s for endpoint '/getData/sequence'  given hubUrl '%s'", genome, hubUrl);

/* might be a UCSC database track hub, where hubGenome=name is the database */
if (isEmpty(hubGenome->twoBitPath))
    {
    getSequenceData(hubGenome->name, hubUrl);
    return;
    }

/* this MaybeChromInfo will open the twoBit file, if not already done */
struct chromInfo *ci = trackHubMaybeChromInfo(hubGenome->name, chrom);
if (NULL == ci)
    apiErrAbort("can not find sequence for chrom=%s for endpoint '/getData/sequence?genome=%s&chrom=%s' given hubUrl='%s'", chrom, genome, chrom, hubUrl);

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
    jsonWriteNumber(jw, "start", (long long)fragStart);
    jsonWriteNumber(jw, "end", (long long)fragEnd);
    }
else
    {
    jsonWriteNumber(jw, "start", (long long)0);
    jsonWriteNumber(jw, "end", (long long)ci->size);
    }
struct dnaSeq *seq = twoBitReadSeqFrag(hubGenome->tbf, chrom, fragStart, fragEnd);
if (NULL == seq)
    {
    if (fragEnd > fragStart)
	apiErrAbort("can not find sequence for chrom=%s;start=%s;end=%s for endpoint '/getData/sequence?genome=%s&chrom=%s;start=%s;end=%s' give hubUrl='%s'", chrom, start, end, genome, chrom, start, end, hubUrl);
    else
	apiErrAbort("can not find sequence for chrom=%s for endpoint '/getData/sequence?genome=%s&chrom=%s' give hubUrl='%s'", chrom, genome, chrom, hubUrl);
    }
jsonWriteString(jw, "dna", seq->dna);
jsonWriteObjectEnd(jw);	/* closing the overall global object */
fputs(jw->dy->string,stdout);
}

void apiGetData(char *words[MAX_PATH_INFO])
/* 'getData' function, words[1] is the subCommand */
{
char *hubUrl = cgiOptionalString("hubUrl");
if (sameWord("track", words[1]))
    {
    if (isNotEmpty(hubUrl))
	getHubTrackData(hubUrl);
    else
	getTrackData();
    }
else if (sameWord("sequence", words[1]))
    {
    if (isNotEmpty(hubUrl))
	getHubSequenceData(hubUrl);
    else
	{
	char *db = cgiOptionalString("db");
	if (isEmpty(db))
	    apiErrAbort("missing URL db=<ucscDb> name for endpoint '/getData/sequence");
	getSequenceData(db, NULL);
	}
    }
else
    apiErrAbort("do not recognize endpoint function: '/%s/%s'", words[0], words[1]);
}
