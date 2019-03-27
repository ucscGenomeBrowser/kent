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

static void tableDataOutput(char *db, struct trackDb *tdb,
    struct sqlConnection *conn, struct jsonWrite *jw, char *table,
    char *chrom, unsigned start, unsigned end)
/* output the table data from the specified query string */
{
char query[4096];
/* no chrom specified, return entire table */
if (isEmpty(chrom))
    sqlSafef(query, sizeof(query), "select * from %s", table);
else if (0 == (start + end))	/* have chrom, no start,end == full chr */
    {
    /* need to extend the chrom column check to allow tName also */
    if (! sqlColumnExists(conn, table, "chrom"))
	apiErrAbort("track '%s' is not a position track, request track without chrom specification, genome: '%s'", table, db);
    struct chromInfo *ci = hGetChromInfo(db, chrom);
    jsonWriteNumber(jw, "start", (long long)0);
    jsonWriteNumber(jw, "end", (long long)ci->size);
    sqlSafef(query, sizeof(query), "select * from %s where chrom='%s'", table, chrom);
    }
else	/* fully specified chrom:start-end */
    {
    if (! sqlColumnExists(conn, table, "chrom"))
	apiErrAbort("track '%s' is not a position track, request track without chrom specification, genome: '%s'", table, db);
    jsonWriteNumber(jw, "start", (long long)start);
    jsonWriteNumber(jw, "end", (long long)end);
    if (startsWith("wig", tdb->type))
	{
        wigTableDataOutput(jw, db, table, chrom, start, end);
        return;	/* DONE */
	}
    else
	{
	sqlSafef(query, sizeof(query), "select * from %s where chrom='%s' AND chromEnd > %u AND chromStart < %u", table, chrom, start, end);
	}
    }

/* continuing, not a wiggle output */
char **columnNames = NULL;
char **columnTypes = NULL;
int columnCount = tableColumns(conn, jw, table, &columnNames, &columnTypes);
jsonWriteListStart(jw, table);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = NULL;
unsigned itemCount = 0;
while (itemCount < maxItemsOutput && (row = sqlNextRow(sr)) != NULL)
    {
    jsonWriteObjectStart(jw, NULL);
    int i = 0;
    for (i = 0; i < columnCount; ++i)
	jsonWriteString(jw, columnNames[i], row[i]);
    jsonWriteObjectEnd(jw);
    ++itemCount;
    }
sqlFreeResult(&sr);
jsonWriteListEnd(jw);
}

static struct bbiFile *bigFileOpen(char *trackType, char *bigDataUrl)
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

/* from hgTables.h */
struct sqlFieldType
/* List field names and types */
    {
    struct sqlFieldType *next;
    char *name;         /* Name of field. */
    char *type;         /* Type of field (MySQL notion) */
    }; 

/* from hgTables.c */
static struct sqlFieldType *sqlFieldTypeNew(char *name, char *type)
/* Create a new sqlFieldType */
{
struct sqlFieldType *ft;
AllocVar(ft);
ft->name = cloneString(name);
ft->type = cloneString(type);
return ft;
}

/* from hgTables.c */
static struct sqlFieldType *sqlFieldTypesFromAs(struct asObject *as)
/* Convert asObject to list of sqlFieldTypes */
{
struct sqlFieldType *ft, *list = NULL;
struct asColumn *col;
for (col = as->columnList; col != NULL; col = col->next)
    {
    struct dyString *type = asColumnToSqlType(col);
    ft = sqlFieldTypeNew(col->name, type->string);
    slAddHead(&list, ft);
    dyStringFree(&type);
    }
slReverse(&list);
return list;
}

static void bedDataOutput(struct jsonWrite *jw, struct bbiFile *bbi,
    char *chrom, unsigned start, unsigned end, struct sqlFieldType *fiList)
/* output bed data for one chrom in the given bbi file */
{
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
        jsonWriteString(jw, fi->name, row[i]);
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

static struct trackDb *findTrackDb(char *track, struct trackDb *tdb)
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
    apiErrAbort("missing genome=<name> for endpoint '/getdata/track'  given hubUrl='%s'", hubUrl);
if (isEmpty(track))
    apiErrAbort("missing track=<name> for endpoint '/getdata/track'  given hubUrl='%s'", hubUrl);

struct trackHub *hub = errCatchTrackHubOpen(hubUrl);
struct trackHubGenome *hubGenome = NULL;
for (hubGenome = hub->genomeList; hubGenome; hubGenome = hubGenome->next)
    {
    if (sameString(genome, hubGenome->name))
	break;
    }
if (NULL == hubGenome)
    apiErrAbort("failed to find specified genome=%s for endpoint '/getdata/track'  given hubUrl '%s'", genome, hubUrl);

struct trackDb *tdb = obtainTdb(hubGenome, NULL);

if (NULL == tdb)
    apiErrAbort("failed to find a track hub definition in genome=%s for endpoint '/getdata/track'  given hubUrl='%s'", genome, hubUrl);

struct trackDb *thisTrack = findTrackDb(track, tdb);

if (NULL == thisTrack)
    apiErrAbort("failed to find specified track=%s in genome=%s for endpoint '/getdata/track'  given hubUrl='%s'", track, genome, hubUrl);

char *bigDataUrl = trackDbSetting(thisTrack, "bigDataUrl");
struct bbiFile *bbi = bigFileOpen(thisTrack->type, bigDataUrl);
if (NULL == bbi)
    apiErrAbort("track type %s management not implemented yet TBD track=%s in genome=%s for endpoint '/getdata/track'  given hubUrl='%s'", track, genome, hubUrl);

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

if (startsWith("bigBed", thisTrack->type))
    {
    struct asObject *as = bigBedAsOrDefault(bbi);
    struct sqlFieldType *fiList = sqlFieldTypesFromAs(as);
    jsonWriteListStart(jw, track);
    if (isEmpty(chrom))
	{
	struct bbiChromInfo *bci;
	for (bci = chromList; bci; bci = bci->next)
	    {
	    bedDataOutput(jw, bbi, bci->name, 0, bci->size, fiList);
	    }
	}
    else
	bedDataOutput(jw, bbi, chrom, uStart, uEnd, fiList);
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
	apiErrAbort("failed to find bigDataUrl=%s for track=%s in database=%s for endpoint '/getdata/track'", bigDataUrl, track, db);
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

if (startsWith("bigBed", thisTrack->type))
    {
    struct asObject *as = bigBedAsOrDefault(bbi);
    struct sqlFieldType *fiList = sqlFieldTypesFromAs(as);
    jsonWriteListStart(jw, track);
    if (isEmpty(chrom))
	{
	struct bbiChromInfo *bci;
	for (bci = chromList; bci; bci = bci->next)
	    {
	    bedDataOutput(jw, bbi, bci->name, 0, bci->size, fiList);
	    }
	}
    else
	bedDataOutput(jw, bbi, chrom, uStart, uEnd, fiList);
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

static void getSequenceData()
/* return DNA sequence, given at least a db=name and chrom=chr,
   optionally start and end  */
{
char *db = cgiOptionalString("db");
char *chrom = cgiOptionalString("chrom");
char *start = cgiOptionalString("start");
char *end = cgiOptionalString("end");

if (isEmpty(db))
    apiErrAbort("missing URL db=<ucscDb> name for endpoint '/getData/sequence");
if (isEmpty(chrom))
    apiErrAbort("missing URL chrom=<name> for endpoint '/getData/sequence?db=%s", db);
if (chromSeqFileExists(db, chrom))
    {
    struct chromInfo *ci = hGetChromInfo(db, chrom);
    struct dnaSeq *seq = NULL;
    if (isEmpty(start) || isEmpty(end))
	seq = hChromSeqMixed(db, chrom, 0, 0);
    else
	seq = hChromSeqMixed(db, chrom, sqlSigned(start), sqlSigned(end));
    if (NULL == seq)
        apiErrAbort("can not find sequence for chrom=%s for endpoint '/getData/sequence?db=%s&chrom=%s", chrom, db, chrom);
    struct jsonWrite *jw = apiStartOutput();
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
}

void apiGetData(char *words[MAX_PATH_INFO])
/* 'getData' function, words[1] is the subCommand */
{
if (sameWord("track", words[1]))
    {
    char *hubUrl = cgiOptionalString("hubUrl");
    if (isNotEmpty(hubUrl))
	getHubTrackData(hubUrl);
    else
	getTrackData();
    }
else if (sameWord("sequence", words[1]))
    getSequenceData();
else
    apiErrAbort("do not recognize endpoint function: '/%s/%s'", words[0], words[1]);
}
