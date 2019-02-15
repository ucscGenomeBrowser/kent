/* manage endpoint /getData/ functions */

#include "dataApi.h"

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
replaceChar(dataTime, ' ', 'T');
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);

/* no chrom specified, return entire table */
if (isEmpty(chrom))
    {
    tableColumns(conn, jw, table);
    }
else if (isEmpty(start) || isEmpty(end))
    {
    if (! sqlColumnExists(conn, table, "chrom"))
	apiErrAbort("track '%s' is not a position track, request table without chrom specification, genome: '%s'", table, db);
    jsonWriteString(jw, "chrom", chrom);
    struct chromInfo *ci = hGetChromInfo(db, chrom);
    jsonWriteNumber(jw, "start", (long long)0);
    jsonWriteNumber(jw, "end", (long long)ci->size);
    tableColumns(conn, jw, table);
    }
else
    {
    if (! sqlColumnExists(conn, table, "chrom"))
	apiErrAbort("track '%s' is not a position track, request table without chrom specification, genome: '%s'", table, db);
    jsonWriteString(jw, "chrom", chrom);
    jsonWriteNumber(jw, "start", (long long)sqlSigned(start));
    jsonWriteNumber(jw, "end", (long long)sqlSigned(end));
    tableColumns(conn, jw, table);
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
    getTrackData();
else if (sameWord("sequence", words[1]))
    getSequenceData();
else
    apiErrAbort("do not recognize endpoint function: '/%s/%s'", words[0], words[1]);
}
