/* hgData_common - common routines for hgData. */
#include "common.h"
#include "hgData.h"
#include "hdb.h"
#include "chromInfo.h"

static char const rcsid[] = "$Id: hgData_common.c,v 1.1.2.4 2009/02/25 20:12:16 mikep Exp $";

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
// Uses global which does not need to be freed but is overwritten after each call
{
static char etagBuf[1000];
safef(etagBuf, sizeof(etagBuf), "%x", (unsigned)modified);
return etagBuf;
}

time_t strToTime(char *time, char *format)
// Convert human time to unix time using format
{
struct tm *tmpTime;
AllocVar(tmpTime);
if (!time || strlen(time)==0)
    return 0;
if (!strptime(time, format, tmpTime))
    errAbort("Error parsing time [%s] using format [%s] in strToTime()\n", time, format);
time_t modified = mktime(tmpTime);
freez(&tmpTime);
if (modified < 0)
    errAbort("Error converting time [%s] to time_t [%d] in strToTime()\n", time, (int)modified);
return modified;
}

char *gmtimeToStr(time_t timeVal, char *format)
// Convert unix time to human time using format
{
static char timeBuf[1024];
struct tm* tmpTime;
if ((tmpTime = gmtime(&timeVal)) == NULL )
    errAbort("Error converting time %d to GMT time\n", (int)timeVal);
if (!strftime(timeBuf, sizeof(timeBuf), format, tmpTime))
    errAbort("Error formatting GMT time %d using format [%s]\n", (int)timeVal, format);
return timeBuf;
}

char *nullOrVal(char *val)
// return the value if not null, or else the string "(null)"
{
return val ? val : "(null)";
}

