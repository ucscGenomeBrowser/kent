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

static void bedDataOutput(struct jsonWrite *jw, struct bbiFile *bbi,
    char *chrom, unsigned start, unsigned end, unsigned maxItems)
/* output bed data for one chrom in the given bbi file */
{
struct lm *bbLm = lmInit(0);
struct bigBedInterval *iv, *ivList = NULL;
ivList = bigBedIntervalQuery(bbi,chrom, start, end, 0, bbLm);
char *row[bbi->fieldCount];
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
lmCleanup(&bbLm);
}

static void wigDataOutput(struct jsonWrite *jw, struct bbiFile *bwf,
    char *chrom, unsigned start, unsigned end, unsigned maxItems)
/* output wig data for one chrom in the given bwf file */
{
struct lm *lm = lmInit(0);
struct bbiInterval *iv, *ivList = bigWigIntervalQuery(bwf, chrom, start, end, lm);
if (NULL == ivList)
    return;

unsigned itemCount = 0;

jsonWriteObjectStart(jw, NULL);
jsonWriteListStart(jw, chrom);
for (iv = ivList; iv && itemCount < maxItems; iv = iv->next)
    {
    jsonWriteListStart(jw, NULL);
    int s = max(iv->start, start);
    int e = min(iv->end, end);
    double val = iv->val;
    jsonWriteNumber(jw, NULL, (long long)s);
    jsonWriteNumber(jw, NULL, (long long)e);
    jsonWriteDouble(jw, NULL, val);
    jsonWriteListEnd(jw);
    ++itemCount;
    }
jsonWriteListEnd(jw);
jsonWriteObjectEnd(jw);
}

static void wigData(struct jsonWrite *jw, struct bbiFile *bwf, char *chrom,
    unsigned start, unsigned end, unsigned maxItems)
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
	wigDataOutput(jw, bwf, bci->name, 0, bci->size, maxItems);
	}
    }
    else
	wigDataOutput(jw, bwf, chrom, start, end, maxItems);
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

struct trackDb *thisTrack = NULL;
for (thisTrack = tdb; thisTrack; thisTrack = thisTrack->next)
    {
    if (sameWord(thisTrack->track, track))
	break;
    }
if (NULL == thisTrack)
    apiErrAbort("failed to find specified track=%s in genome=%s for endpoint '/getdata/track'  given hubUrl='%s'", track, genome, hubUrl);

char *bigDataUrl = trackDbSetting(thisTrack, "bigDataUrl");
struct bbiFile *bbi = bigFileOpen(thisTrack->type, bigDataUrl);
if (NULL == bbi)
    apiErrAbort("track type %s management not implemented yet TBD track=%s in genome=%s for endpoint '/getdata/track'  given hubUrl='%s'", track, genome, hubUrl);

struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "hubUrl", hubUrl);
jsonWriteString(jw, "genome", genome);
jsonWriteString(jw, "track", track);
unsigned chromSize = 0;
struct bbiChromInfo *chromList = NULL;
if (isNotEmpty(chrom))
    {
    jsonWriteString(jw, "chrom", chrom);
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
jsonWriteString(jw, "type", thisTrack->type);

if (startsWith("bigBed", thisTrack->type))
    {
    struct asObject *as = bigBedAsOrDefault(bbi);
    struct sqlFieldType *fi, *fiList = sqlFieldTypesFromAs(as);
    jsonWriteListStart(jw, "columnNames");
    unsigned maxItems = 0;	/* TBD will use this later for paging */
//    int columnCount = slCount(fiList);
    for (fi = fiList; fi; fi = fi->next)
	{
	jsonWriteObjectStart(jw, NULL);
	jsonWriteString(jw, fi->name, fi->type);
	jsonWriteObjectEnd(jw);
	}
    jsonWriteListEnd(jw);
    jsonWriteListStart(jw, "trackData");
    if (isEmpty(chrom))
	{
	struct bbiChromInfo *bci;
	for (bci = chromList; bci; bci = bci->next)
	    {
	    bedDataOutput(jw, bbi, bci->name, 0, bci->size, maxItems);
	    }
	}
    else
	bedDataOutput(jw, bbi, chrom, uStart, uEnd, maxItems);
    jsonWriteListEnd(jw);
    }
else if (startsWith("bigWig", thisTrack->type))
    {
    unsigned maxItems = 1000;	/* TBD will use this later for paging */
    jsonWriteListStart(jw, "trackData");
    wigData(jw, bbi, chrom, uStart, uEnd, maxItems);
    jsonWriteListEnd(jw);
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
