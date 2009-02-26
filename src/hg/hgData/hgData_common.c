/* hgData_common - common routines for hgData. */
#include "common.h"
#include "hgData.h"
#include "hdb.h"
#include "chromInfo.h"

static char const rcsid[] = "$Id: hgData_common.c,v 1.1.2.7 2009/02/26 21:17:44 mikep Exp $";

struct coords navigate(int start, int end, int chromSize)
// Calculate navigation coordinates including window left, window right
// Zoom in 10x, Zoom out 10x
{
struct coords c;
c.len         = end - start;
c.mid         = start + (c.len / 2);
c.left        = max(0, start - c.len);
c.right       = min(chromSize, end + c.len);
c.leftIn10x   = c.mid - (c.len / 20);
c.rightIn10x  = c.mid + (c.len / 20);
c.leftOut10x  = max(0,c.mid - (c.len * 5));
c.rightOut10x = min(chromSize, c.mid + (c.len * 5));
return c;
}

char *etag(time_t modified)
// Convert modification time to ETag
// Returned value must be freed by caller
{
static char etagBuf[1000];
safef(etagBuf, sizeof(etagBuf), "%x", (unsigned)modified);
return cloneString(etagBuf);
}

time_t strToTime(char *time, char *format)
// Convert human time to unix time using format
{
struct tm tmpTime;
if (!time || strlen(time)==0)
    return 0;
char *t = strptime(time, format, &tmpTime);
if (t==NULL || (t - time) != strlen(time))
    errAbort("Error parsing time [%s] using format [%s] in strToTime() (processed %d chars)\n", time, format, (int)(t ? (t - time) : 0));
time_t modified = mktime(&tmpTime);
if (modified < 0)
    errAbort("Error converting time [%s] to time_t [%d] in strToTime()\n", time, (int)modified);
return modified;
}

char *gmtimeToHttpStr(time_t timeVal)
// Convert unix time to human time using HTTP format
// Returned value must be freed by caller
{
return gmtimeToStr(timeVal, "%a, %d %b %Y %H:%M:%S GMT");
}

char *gmtimeToStr(time_t timeVal, char *format)
// Convert unix time to human time using format
// Returned value must be freed by caller
{
char timeBuf[1024];
timeBuf[0] = 0;
struct tm tmpTime;
if (gmtime_r(&timeVal, &tmpTime) == NULL )
    errAbort("Error converting time %d to GMT time\n", (int)timeVal);
if (!strftime(timeBuf, sizeof(timeBuf), format, &tmpTime))
    errAbort("Error formatting GMT time %d using format [%s]\n", (int)timeVal, format);
return cloneString(timeBuf);
}

char *nullOrVal(char *val)
// return the value if not null, or else the string "(null)"
{
return val ? val : "(null)";
}

char *valOrVariable(char *val, char *variable)
// Return val if not NULL, else return variable
{
    return val ? val : variable;
}
