/* jsonWrite - Helper routines for writing out JSON.  */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "sqlNum.h"
#include "jsonParse.h"
#include "jsonWrite.h"

// Separator between elements; set this to "\n" to see elements on separate lines.
// Newlines are fine in Javascript, e.g. in an embedded <script>.
// However, unescaped \n is illegal in JSON and web browsers may reject it.
// Web browser plugins can pretty-print JSON nicely.

struct jsonWrite *jsonWriteNew()
/* Return new empty jsonWrite struct. */
{
struct jsonWrite *jw;
AllocVar(jw);
jw->dy = dyStringNew(0);
jw->sep = ' ';
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

static void jsonWritePushObjStack(struct jsonWrite *jw, bool isNotEmpty, bool isObject)
/* Push a new object or list on stack */
{
int stackIx = jw->stackIx + 1;
if (stackIx >= ArraySize(jw->objStack))
    errAbort("Stack overflow in jsonWritePush");
jw->objStack[stackIx].isNotEmpty = isNotEmpty;
jw->objStack[stackIx].isObject = isObject;
jw->stackIx = stackIx;
}

static void jsonWritePopObjStack(struct jsonWrite *jw, bool isObject)
/* pop object stack and just discard val. */
{
boolean topIsObject = jw->objStack[jw->stackIx].isObject;
if (topIsObject != isObject)
    errAbort("jsonWrite: expected to close %s but was told to close %s",
             topIsObject ? "object" : "list",
             isObject ? "object" : "list");
int stackIx = jw->stackIx - 1;
if (stackIx < 0)
    errAbort("Stack underflow in jsonWritePopObjStack");
jw->stackIx = stackIx;
}

INLINE void jsonWriteMaybeComma(struct jsonWrite *jw)
/* If this is not the first item added to an object or list, write a comma. */
{
if (jw->objStack[jw->stackIx].isNotEmpty)
    {
    dyStringAppendC(jw->dy, ',');
    dyStringAppendC(jw->dy, jw->sep);
    }
else
    jw->objStack[jw->stackIx].isNotEmpty = TRUE;
}

void jsonWriteTag(struct jsonWrite *jw, char *var)
/* Print out preceding comma if necessary, and if var is non-NULL, quoted tag followed by colon. */
{
jsonWriteMaybeComma(jw);
if (var != NULL)
    dyStringPrintf(jw->dy, "\"%s\": ", var);
}

void jsonWriteString(struct jsonWrite *jw, char *var, char *string)
/* Print out "var": "val" -- or rather, jsonStringEscape(val).
 * If var is NULL, print val only.  If string is NULL, "var": null . */
{
jsonWriteTag(jw, var);
if (string)
    {
    size_t encSize = jsonStringEscapeSize(string);
    char *encoded = needMem(encSize);  /* needMem limit is 500,000,000 */
    jsonStringEscapeBuf(string, encoded, encSize);
    dyStringPrintf(jw->dy, "\"%s\"", encoded);
    freeMem(encoded);
    }
else
    dyStringAppend(jw->dy, "null");
}

void jsonWriteDateFromUnix(struct jsonWrite *jw, char *var, long long unixTimeVal)
/* Add "var": YYYY-MM-DDT-HH:MM:SSZ given a Unix time stamp. Var may be NULL. */
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
/* print out "var": val as number. Var may be NULL. */
{
struct dyString *dy = jw->dy;
jsonWriteTag(jw, var);
dyStringPrintf(dy, "%lld", val);
}

void jsonWriteDouble(struct jsonWrite *jw, char *var, double val)
/* print out "var": val as number. Var may be NULL. */
{
struct dyString *dy = jw->dy;
jsonWriteTag(jw, var);
dyStringPrintf(dy, "%g", val);
}

void jsonWriteLink(struct jsonWrite *jw, char *var, char *objRoot, char *name)
/* Print out the jsony type link to another object.  objRoot will start and end with a '/'
 * and may have additional slashes in this usage. Var may be NULL. */
{
struct dyString *dy = jw->dy;
jsonWriteTag(jw, var);
dyStringPrintf(dy, "\"%s%s\"", objRoot, name);
}

void jsonWriteLinkNum(struct jsonWrite *jw, char *var, char *objRoot, long long id)
/* Print out the jsony type link to another object with a numerical id.  objRoot will start 
 * and end with a '/' and may have additional slashes in this usage. Var may be NULL */
{
struct dyString *dy = jw->dy;
jsonWriteTag(jw, var);
dyStringPrintf(dy, "\"%s%lld\"", objRoot, id);
}

void jsonWriteListStart(struct jsonWrite *jw, char *var)
/* Start an array in JSON. Var may be NULL */
{
struct dyString *dy = jw->dy;
jsonWriteTag(jw, var);
dyStringAppendC(dy, '[');
dyStringAppendC(dy, jw->sep);
jsonWritePushObjStack(jw, FALSE, FALSE);
}

void jsonWriteListEnd(struct jsonWrite *jw)
/* End an array in JSON */
{
struct dyString *dy = jw->dy;
dyStringAppendC(dy, ']');
dyStringAppendC(dy, jw->sep);
jsonWritePopObjStack(jw, FALSE);
}

void jsonWriteObjectStart(struct jsonWrite *jw, char *var)
/* Print start of object, preceded by tag if var is non-NULL. */
{
jsonWriteTag(jw, var);
struct dyString *dy = jw->dy;
dyStringAppendC(dy, '{');
dyStringAppendC(dy, jw->sep);
jsonWritePushObjStack(jw, FALSE, TRUE);
}

void jsonWriteObjectEnd(struct jsonWrite *jw)
/* End object in JSON */
{
struct dyString *dy = jw->dy;
dyStringAppendC(dy, '}');
dyStringAppendC(dy, jw->sep);
jsonWritePopObjStack(jw, TRUE);
}

void jsonWriteStringf(struct jsonWrite *jw, char *var, char *format, ...)
/* Write "var": "val" where val is jsonStringEscape'd formatted string. */
{
// In order to use jsonStringEscape(), we need to use a temporary dyString
// instead of jw->dy in the dyStringVaPrintf, and pass that to jsonWriteString.
struct dyString *tmpDy = dyStringNew(0);
va_list args;
va_start(args, format);
dyStringVaPrintf(tmpDy, format, args);
va_end(args);
jsonWriteString(jw, var, tmpDy->string);
dyStringFree(&tmpDy);
}

void jsonWriteBoolean(struct jsonWrite *jw, char *var, boolean val)
/* Write out "var": true or "var": false depending on val (no quotes around true/false). */
{
jsonWriteTag(jw, var);
dyStringAppend(jw->dy, val ? "true" : "false");
}

void jsonWriteValueLabelList(struct jsonWrite *jw, char *var, struct slPair *pairList)
/* Print out a named list of {"value": "<pair->name>", "label": "<pair->val>"} objects. */
{
jsonWriteListStart(jw, var);
struct slPair *pair;
for (pair = pairList;  pair != NULL;  pair = pair->next)
    {
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "value", pair->name);
    jsonWriteString(jw, "label", (char *)(pair->val));
    jsonWriteObjectEnd(jw);
    }
jsonWriteListEnd(jw);
}

void jsonWriteSlNameList(struct jsonWrite *jw, char *var, struct slName *slnList)
/* Print out a named list of strings from slnList. */
{
jsonWriteListStart(jw, var);
struct slName *sln;
for (sln = slnList;  sln != NULL;  sln = sln->next)
    jsonWriteString(jw, NULL, sln->name);
jsonWriteListEnd(jw);
}

void jsonWriteAppend(struct jsonWrite *jwA, char *var, struct jsonWrite *jwB)
/* Append jwB's contents to jwA's.  If jwB is non-NULL, it must be fully closed (no unclosed
 * list or object).  If var is non-NULL, write it out as a tag before appending.
 * If both var and jwB are NULL, leave jwA unchanged. */
{
if (jwB && jwB->stackIx)
    errAbort("jsonWriteAppend: second argument must be fully closed but its stackIx is %d not 0",
             jwB->stackIx);
jsonWriteTag(jwA, var);
if (jwB)
    dyStringAppendN(jwA->dy, jwB->dy->string, jwB->dy->stringSize);
else if (var)
    dyStringAppend(jwA->dy, "null");
}

int jsonWritePopToLevel(struct jsonWrite *jw, uint level)
/* Close out the objects and lists that are deeper than level, so we end up at level ready to
 * add new items.  Return the level that we end up with, which may not be the same as level,
 * if level is deeper than the current stack. */
{
int i;
for (i = jw->stackIx;  i > level;  i--)
    {
    if (jw->objStack[i].isObject)
        jsonWriteObjectEnd(jw);
    else
        jsonWriteListEnd(jw);
    }
return jw->stackIx;
}
