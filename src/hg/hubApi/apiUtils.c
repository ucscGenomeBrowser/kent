/* utility functions for data API business */

#include "dataApi.h"

void apiErrAbort(char *format, ...)
/* Issue an error message in json format, and exit(0) */
{
char errMsg[2048];
va_list args;
va_start(args, format);
vsnprintf(errMsg, sizeof(errMsg), format, args);
struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "error", errMsg);
jsonWriteObjectEnd(jw);
fputs(jw->dy->string,stdout);
exit(0);
}

struct jsonWrite *apiStartOutput()
/* begin json output with standard header information for all requests */
{
time_t timeNow = time(NULL);
// struct tm tm;
// gmtime_r(&timeNow, &tm);
struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, NULL);
// not recommended: jsonWriteString(jw, "apiVersion", "v"CGI_VERSION);
jsonWriteString(jw, "source", "UCSantaCruz");
jsonWriteDateFromUnix(jw, "downloadTime", (long long) timeNow);
jsonWriteNumber(jw, "downloadTimeStamp", (long long) timeNow);
return jw;
}

int tableColumns(struct sqlConnection *conn, struct jsonWrite *jw, char *table,
   char ***nameReturn, char ***typeReturn)
/* return the column names, and their MySQL data type, for the given table
 *  return number of columns (aka 'fields')
 */
{
// not needed jsonWriteListStart(jw, "columnNames");
struct sqlFieldInfo *fi, *fiList = sqlFieldInfoGet(conn, table);
int columnCount = slCount(fiList);
char **namesReturn = NULL;
char **typesReturn = NULL;
AllocArray(namesReturn, columnCount);
AllocArray(typesReturn, columnCount);
int i = 0;
for (fi = fiList; fi; fi = fi->next)
    {
    namesReturn[i] = cloneString(fi->field);
    typesReturn[i] = cloneString(fi->type);
    i++;
// not needed     jsonWriteObjectStart(jw, NULL);
// not needed     jsonWriteString(jw, fi->field, fi->type);
// not needed     jsonWriteObjectEnd(jw);
    }
// not needed jsonWriteListEnd(jw);
*nameReturn = namesReturn;
*typeReturn = namesReturn;
return columnCount;
}

struct trackHub *errCatchTrackHubOpen(char *hubUrl)
/* use errCatch around a trackHub open in case it fails */
{
struct trackHub *hub = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    hub = trackHubOpen(hubUrl, "");
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    apiErrAbort("error opening hubUrl: '%s', '%s'", hubUrl,  errCatch->message->string);
    }
errCatchFree(&errCatch);
return hub;
}

struct trackDb *obtainTdb(struct trackHubGenome *genome, char *db)
/* return a full trackDb fiven the hub genome pointer, or ucsc database name */
{
struct trackDb *tdb = NULL;
if (db)
    tdb = hTrackDb(db);
else
    {
    tdb = trackHubTracksForGenome(genome->trackHub, genome);
    tdb = trackDbLinkUpGenerations(tdb);
    tdb = trackDbPolishAfterLinkup(tdb, genome->name);
    slSort(&tdb, trackDbCmp);
    }
return tdb;
}
