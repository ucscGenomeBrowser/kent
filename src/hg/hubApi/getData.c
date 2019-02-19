/* manage endpoint /getData/ functions */

#include "dataApi.h"

static void tableDataOutput(struct sqlConnection *conn, struct jsonWrite *jw, char *query, char *table)
/* output the table data from the specified query string */
{
int columnCount = tableColumns(conn, jw, table);
jsonWriteListStart(jw, "trackData");
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    jsonWriteListStart(jw, NULL);
    int i = 0;
    for (i = 0; i < columnCount; ++i)
	jsonWriteString(jw, NULL, row[i]);
    jsonWriteListEnd(jw);
    }
jsonWriteListEnd(jw);
}

static struct bbiFile *bigFileOpen(char *trackType, char *bigDataUrl)
{
struct bbiFile *bbi = NULL;
if (startsWith("bigBed", trackType))
    bbi = bigBedFileOpen(bigDataUrl);
else if (startsWith("bigWig", trackType))
    bbi = bigWigFileOpen(bigDataUrl);

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

struct trackDb *tdb = trackHubTracksForGenome(hubGenome->trackHub, hubGenome);

if (NULL == tdb)
    apiErrAbort("failed to find a track hub definition in genome=%s for endpoint '/getdata/track'  given hubUrl='%s'", genome, hubUrl);

struct trackDb *thisTrack = NULL;
for (thisTrack = tdb; thisTrack; thisTrack = thisTrack->next)
    {
    if (sameWord(thisTrack->track, track))
	break;
    }
if (NULL == thisTrack)
    apiErrAbort("failed to find specified track=%s in genome=%s for endpoint '/getdata/track'  given hubUrl='%s'", track, genome, hubUrl);

struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "hubUrl", hubUrl);
jsonWriteString(jw, "genome", genome);
jsonWriteString(jw, "track", track);
if (isNotEmpty(chrom))
    jsonWriteString(jw, "chrom", chrom);
if ( ! (isEmpty(start) || isEmpty(end)) )
    {
    jsonWriteString(jw, "start", start);
    jsonWriteString(jw, "end", end);
    }
char *bigDataUrl = trackDbSetting(thisTrack, "bigDataUrl");
jsonWriteString(jw, "bigDataUrl", bigDataUrl);
jsonWriteString(jw, "type", thisTrack->type);
struct bbiFile *bbi = bigFileOpen(thisTrack->type, bigDataUrl);
if (NULL == bbi)
    apiErrAbort("track type %s management not implemented yet TBD track=%s in genome=%s for endpoint '/getdata/track'  given hubUrl='%s'", track, genome, hubUrl);
if (startsWith("bigBed", thisTrack->type))
    {
    struct asObject *as = bigBedAsOrDefault(bbi);
    struct sqlFieldType *fi, *fiList = sqlFieldTypesFromAs(as);
    jsonWriteListStart(jw, "columnNames");
//    int columnCount = slCount(fiList);
    for (fi = fiList; fi; fi = fi->next)
	{
	jsonWriteObjectStart(jw, NULL);
	jsonWriteString(jw, fi->name, fi->type);
	jsonWriteObjectEnd(jw);
	}
    jsonWriteListEnd(jw);
    struct lm *bbLm = lmInit(0);
    unsigned uStart = sqlUnsigned(start);
    unsigned uEnd = sqlUnsigned(end);
    struct bigBedInterval *iv, *ivList = NULL;
    ivList = bigBedIntervalQuery(bbi,chrom, uStart, uEnd, 0, bbLm);
    char *row[bbi->fieldCount];
    jsonWriteListStart(jw, "trackData");
    for (iv = ivList; iv; iv = iv->next)
	{
	char startBuf[16], endBuf[16];
	bigBedIntervalToRow(iv, chrom, startBuf, endBuf, row, bbi->fieldCount);
	jsonWriteListStart(jw, NULL);
        int i;
        for (i = 0; i < bbi->fieldCount; ++i)
	    {
	    jsonWriteString(jw, NULL, row[i]);
	    }
	jsonWriteListEnd(jw);
	}
    jsonWriteListEnd(jw);
    lmCleanup(&bbLm);
    }
else if (startsWith("bigWig", thisTrack->type))
    {
    }
bbiFileClose(&bbi);
jsonWriteObjectEnd(jw);
fputs(jw->dy->string,stdout);
}

static void getTrackData()
/* return data from a track, optionally just one chrom data,
 *  optionally just one section of that chrom data
 */
{
char *db = cgiOptionalString("db");
char *chrom = cgiOptionalString("chrom");
char *start = cgiOptionalString("start");
char *end = cgiOptionalString("end");
char *table = cgiOptionalString("track");
/* 'track' name in trackDb refers to a SQL 'table' */

if (isEmpty(db))
    apiErrAbort("missing URL db=<ucscDb> name for endpoint '/getData/track");
if (isEmpty(table))
    apiErrAbort("missing URL track=<trackName> name for endpoint '/getData/track");
struct sqlConnection *conn = hAllocConn(db);
if (! sqlTableExists(conn, table))
    apiErrAbort("can not find specified 'track=%s' for endpoint: /getData/track?db=%s&track=%s", table, db, table);

struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "db", db);
jsonWriteString(jw, "track", table);
char *dataTime = sqlTableUpdate(conn, table);
time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
replaceChar(dataTime, ' ', 'T');	/*	ISO 8601	*/
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);

char query[4096];
/* no chrom specified, return entire table */
if (isEmpty(chrom))
    {
    sqlSafef(query, sizeof(query), "select * from %s", table);
    tableDataOutput(conn, jw, query, table);
    }
else if (isEmpty(start) || isEmpty(end))
    {
    if (! sqlColumnExists(conn, table, "chrom"))
	apiErrAbort("track '%s' is not a position track, request track without chrom specification, genome: '%s'", table, db);
    jsonWriteString(jw, "chrom", chrom);
    struct chromInfo *ci = hGetChromInfo(db, chrom);
    jsonWriteNumber(jw, "start", (long long)0);
    jsonWriteNumber(jw, "end", (long long)ci->size);
    sqlSafef(query, sizeof(query), "select * from %s where chrom='%s'", table, chrom);
    tableDataOutput(conn, jw, query, table);
    }
else
    {
    if (! sqlColumnExists(conn, table, "chrom"))
	apiErrAbort("track '%s' is not a position track, request track without chrom specification, genome: '%s'", table, db);
    jsonWriteString(jw, "chrom", chrom);
    jsonWriteNumber(jw, "start", (long long)sqlSigned(start));
    jsonWriteNumber(jw, "end", (long long)sqlSigned(end));
    sqlSafef(query, sizeof(query), "select * from %s where chrom='%s' AND chromEnd > %d AND chromStart < %d", table, chrom, sqlSigned(start), sqlSigned(end));
    tableDataOutput(conn, jw, query, table);
    }
jsonWriteObjectEnd(jw);
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
