/* jsonWrite - Helper routines for writing out JSON. 
 *
 * Apologies for the awkward 'isMiddle' parameter.  This is
 * from JSON not allowing a terminal comma for a comma separated
 * list.   A larger, more usable library might find a way to
 * take care of this for you. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "sqlNum.h"
#include "jsonParse.h"
#include "jsonWrite.h"

void dyJsonTag(struct dyString *dy, char *var)
/* Print out quoted tag followed by colon */
{
dyStringPrintf(dy, "\"%s\": ", var);
}

void dyJsonEndLine(struct dyString *dy, boolean isMiddle)
/* Write comma if in middle, and then newline regardless. */
{
if (isMiddle)
   dyStringAppendC(dy, ',');
dyStringAppendC(dy, '\n');
}

void dyJsonString(struct dyString *dy, char *var, char *string, boolean isMiddle)
/* Print out "var": "val" */
{
dyJsonTag(dy, var);
dyStringPrintf(dy, "\"%s\"", string);
dyJsonEndLine(dy, isMiddle);
}

void dyJsonDateFromUnix(struct dyString *dy, char *var, long long unixTimeVal, boolean isMiddle)
/* Add "var": YYYY-MM-DDT-HH:MM:SSZ given a Unix time stamp */
{
time_t timeStamp = unixTimeVal;
struct tm tm;
gmtime_r(&timeStamp, &tm);
dyJsonTag(dy, var);
dyStringPrintf(dy, "\"%d:%02d:%02dT%02d:%02d:%02dZ\"",
    1900+tm.tm_year, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
dyJsonEndLine(dy, isMiddle);
}

void dyJsonNumber(struct dyString *dy, char *var, long long val, boolean isMiddle)
/* print out "var": val as number */
{
dyJsonTag(dy, var);
dyStringPrintf(dy, "%lld", val);
dyJsonEndLine(dy, isMiddle);
}

void dyJsonLink(struct dyString *dy, char *var, char *objRoot, char *name, boolean isMiddle)
/* Print out the jsony type link to another object.  objRoot will start and end with a '/'
 * and may have additional slashes in this usage. */
{
dyJsonTag(dy, var);
dyStringPrintf(dy, "\"%s%s\"", objRoot, name);
dyJsonEndLine(dy, isMiddle);
}

void dyJsonLinkNum(struct dyString *dy, char *var, char *objRoot, long long id, boolean isMiddle)
/* Print out the jsony type link to another object with a numerical id.  objRoot will start 
 * and end with a '/' and may have additional slashes in this usage. */
{
dyJsonTag(dy, var);
dyStringPrintf(dy, "\"%s%lld\"", objRoot, id);
dyJsonEndLine(dy, isMiddle);
}

void dyJsonListStart(struct dyString *dy, char *var)
/* Start an array in JSON */
{
dyJsonTag(dy, var);
dyStringAppend(dy, "[\n");
}

void dyJsonListEnd(struct dyString *dy, boolean isMiddle)
/* End an array in JSON */
{
dyStringAppendC(dy, ']');
dyJsonEndLine(dy, isMiddle);
}

void dyJsonObjectStart(struct dyString *dy)
/* Print start of object */
{
dyStringAppend(dy, "{\n");
}

void dyJsonObjectEnd(struct dyString *dy, boolean isMiddle)
/* End object in JSON */
{
dyStringAppendC(dy, '}');
dyJsonEndLine(dy, isMiddle);
}

