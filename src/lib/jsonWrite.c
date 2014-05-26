/* jsonWrite - Helper routines for writing out JSON.  */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "sqlNum.h"
#include "jsonParse.h"
#include "jsonWrite.h"

struct jsonWrite *jsonWriteNew()
/* Return new empty jsonWrite struct. */
{
struct jsonWrite *jw;
AllocVar(jw);
jw->dy = dyStringNew(0);
return jw;
}

void jsonWriteFree(struct jsonWrite **pJw)
/* Free up a jsonWrite object. */
{
struct jsonWrite *jw = *pJw;
if (jw != NULL)
    {
    dyStringFree(&jw->dy);
    freez(pJw);
    }
}

static void jsonWritePushObjStack(struct jsonWrite *jw, bool val)
/* Push val on stack */
{
int stackIx = jw->stackIx + 1;
if (stackIx >= ArraySize(jw->objStack))
    errAbort("Stack overflow in jsonWritePush");
jw->objStack[stackIx] = val;
jw->stackIx = stackIx;
}

static void jsonWritePopObjStack(struct jsonWrite *jw)
/* pop object stack and just discard val. */
{
int stackIx = jw->stackIx - 1;
if (stackIx < 0)
    errAbort("Stack underflow in jsonWritePopObjStack");
jw->stackIx = stackIx;
}

void jsonWriteTag(struct jsonWrite *jw, char *var)
/* Print out quoted tag followed by colon */
{
struct dyString *dy = jw->dy;
if (jw->objStack[jw->stackIx] != 0)
    dyStringAppend(dy, ",\n");
else
    jw->objStack[jw->stackIx] = 1;
dyStringPrintf(jw->dy, "\"%s\": ", var);
}

#ifdef OLD
void jsonWriteEndLine(struct jsonWrite *jw)
/* Write comma if in middle, and then newline regardless. */
{
struct dyString *dy = jw->dy;
if (isMiddle)
   dyStringAppendC(dy, ',');
dyStringAppendC(dy, '\n');
}
#endif /* OLD */

void jsonWriteString(struct jsonWrite *jw, char *var, char *string)
/* Print out "var": "val" */
{
struct dyString *dy = jw->dy;
jsonWriteTag(jw, var);
dyStringPrintf(dy, "\"%s\"", string);
}

void jsonWriteDateFromUnix(struct jsonWrite *jw, char *var, long long unixTimeVal)
/* Add "var": YYYY-MM-DDT-HH:MM:SSZ given a Unix time stamp */
{
struct dyString *dy = jw->dy;
time_t timeStamp = unixTimeVal;
struct tm tm;
gmtime_r(&timeStamp, &tm);
jsonWriteTag(jw, var);
dyStringPrintf(dy, "\"%d:%02d:%02dT%02d:%02d:%02dZ\"",
    1900+tm.tm_year, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void jsonWriteNumber(struct jsonWrite *jw, char *var, long long val)
/* print out "var": val as number */
{
struct dyString *dy = jw->dy;
jsonWriteTag(jw, var);
dyStringPrintf(dy, "%lld", val);
}

void jsonWriteLink(struct jsonWrite *jw, char *var, char *objRoot, char *name)
/* Print out the jsony type link to another object.  objRoot will start and end with a '/'
 * and may have additional slashes in this usage. */
{
struct dyString *dy = jw->dy;
jsonWriteTag(jw, var);
dyStringPrintf(dy, "\"%s%s\"", objRoot, name);
}

void jsonWriteLinkNum(struct jsonWrite *jw, char *var, char *objRoot, long long id)
/* Print out the jsony type link to another object with a numerical id.  objRoot will start 
 * and end with a '/' and may have additional slashes in this usage. */
{
struct dyString *dy = jw->dy;
jsonWriteTag(jw, var);
dyStringPrintf(dy, "\"%s%lld\"", objRoot, id);
}

void jsonWriteListStart(struct jsonWrite *jw, char *var)
/* Start an array in JSON */
{
struct dyString *dy = jw->dy;
jsonWriteTag(jw, var);
dyStringAppend(dy, "[\n");
jsonWritePushObjStack(jw, FALSE);
}

void jsonWriteListEnd(struct jsonWrite *jw)
/* End an array in JSON */
{
struct dyString *dy = jw->dy;
dyStringAppend(dy, "]\n");
jsonWritePopObjStack(jw);
}

void jsonWriteObjectStart(struct jsonWrite *jw)
/* Print start of object */
{
struct dyString *dy = jw->dy;
dyStringAppend(dy, "{\n");
jsonWritePushObjStack(jw, FALSE);
}

void jsonWriteObjectEnd(struct jsonWrite *jw)
/* End object in JSON */
{
struct dyString *dy = jw->dy;
dyStringAppend(dy, "}\n");
jsonWritePopObjStack(jw);
}

