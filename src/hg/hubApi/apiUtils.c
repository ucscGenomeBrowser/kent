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
jsonWriteString(jw, "apiVersion", "0.1");
jsonWriteString(jw, "source", "UCSantaCruz");
jsonWriteDateFromUnix(jw, "downloadTime", (long long) timeNow);
jsonWriteNumber(jw, "downloadTimeStamp", (long long) timeNow);
return jw;
}

