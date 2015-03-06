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
#define JW_SEP " "

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

INLINE void jsonWriteMaybeComma(struct jsonWrite *jw)
/* If this is not the first item added to an object or list, write a comma. */
{
if (jw->objStack[jw->stackIx] != 0)
    dyStringAppend(jw->dy, ","JW_SEP);
else
    jw->objStack[jw->stackIx] = 1;
}

void jsonWriteTag(struct jsonWrite *jw, char *var)
/* Print out quoted tag followed by colon. Print out preceding comma if need be.  */
{
if (var != NULL)
    {
    jsonWriteMaybeComma(jw);
    dyStringPrintf(jw->dy, "\"%s\": ", var);
    }
}

void jsonWriteString(struct jsonWrite *jw, char *var, char *string)
/* Print out "var": "val".  If var is NULL, print val only.  If string is NULL, "var": null . */
{
if (var)
    jsonWriteTag(jw, var);
else
    jsonWriteMaybeComma(jw);
if (string)
    dyStringPrintf(jw->dy, "\"%s\"", string);
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
dyStringPrintf(dy, "%lf", val);
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
dyStringAppend(dy, "["JW_SEP);
jsonWritePushObjStack(jw, FALSE);
}

void jsonWriteListEnd(struct jsonWrite *jw)
/* End an array in JSON */
{
struct dyString *dy = jw->dy;
dyStringAppend(dy, "]"JW_SEP);
jsonWritePopObjStack(jw);
}

void jsonWriteObjectStart(struct jsonWrite *jw, char *var)
/* Print start of object, preceded by tag if var is non-NULL. */
{
if (var)
    jsonWriteTag(jw, var);
else
    jsonWriteMaybeComma(jw);
struct dyString *dy = jw->dy;
dyStringAppend(dy, "{"JW_SEP);
jsonWritePushObjStack(jw, FALSE);
}

void jsonWriteObjectEnd(struct jsonWrite *jw)
/* End object in JSON */
{
struct dyString *dy = jw->dy;
dyStringAppend(dy, "}"JW_SEP);
jsonWritePopObjStack(jw);
}

void jsonWriteStringf(struct jsonWrite *jw, char *var, char *format, ...)
/* Write "var": "val" where val is jsonStringEscape'd formatted string. */
{
// Since we're using jsonStringEscape(), we need to use a temporary dyString
// instead of jw->dy.
struct dyString *tmpDy = dyStringNew(0);
va_list args;
va_start(args, format);
dyStringVaPrintf(tmpDy, format, args);
va_end(args);
char *escaped = jsonStringEscape(tmpDy->string);
jsonWriteString(jw, var, escaped);
freeMem(escaped);
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
