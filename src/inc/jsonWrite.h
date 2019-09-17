/* jsonWrite - Helper routines for writing out JSON.  The idea of this is you build up a string inside
 * of the jsonWrite object using various jsonWrite methods, and then output the string where you want.
 *
 * struct jsonWrite *jw = jsonWriteNew();
 * jsonWriteObjectStart(jw, NULL);		// Anonymous outer object 
 * jsonWriteStringf(jw, "user", "%s %s", user->firstName, user->lastName);
 * jsonWriteNumber(jw, "year", 2015);
 * jsonWriteObjectEnd(jw);
 * printf("%s\n", jw->dy->string);
 * jsonWriteFree(&jw);
 */


#ifndef JSONWRITE_H
#define JSONWRITE_H

struct jwObjInfo
/* Helps keep track of whether a comma is needed and whether we need to close an object or list */
    {
    bool isNotEmpty;		/* TRUE if an item has already been added to this object or list */
    bool isObject;		/* TRUE if item is an object (not a list). */
    };

struct jsonWrite
/* Object to help output JSON */
     {
     struct jsonWrite *next;
     struct dyString *dy;	/* Most of this module is building json text in here */
     struct jwObjInfo objStack[128]; /* Make stack deep enough to handle nested objects and lists */
     int stackIx;		/* Current index in stack */
     char sep;			/* Separator, defaults to ' ', but set to '\n' for human
                                 * readability. */
     };

struct jsonWrite *jsonWriteNew();
/* Return new empty jsonWrite struct. */

void jsonWriteFree(struct jsonWrite **pJw);
/* Free up a jsonWrite object. */

void jsonWriteTag(struct jsonWrite *jw, char *var);
/* Print out preceding comma if necessary, and if var is non-NULL, quoted tag followed by colon. */

void jsonWriteEndLine(struct jsonWrite *jw);
/* Write comma if in middle, and then newline regardless. */

void jsonWriteString(struct jsonWrite *jw, char *var, char *string);
/* Print out "var": "val" -- or rather, jsonStringEscape(val).
 * If var is NULL, print val only.  If string is NULL, "var": null . */

void jsonWriteDateFromUnix(struct jsonWrite *jw, char *var, long long unixTimeVal);
/* Add "var": YYYY-MM-DDT-HH:MM:SSZ given a Unix time stamp. Var may be NULL. */

void jsonWriteNumber(struct jsonWrite *jw, char *var, long long val);
/* print out "var": val as number. Var may be NULL. */

void jsonWriteDouble(struct jsonWrite *jw, char *var, double val);
/* print out "var": val as number. Var may be NULL. */

void jsonWriteLink(struct jsonWrite *jw, char *var, char *objRoot, char *name);
/* Print out the jsony type link to another object.  objRoot will start and end with a '/'
 * and may have additional slashes in this usage. Var may be NULL. */

void jsonWriteLinkNum(struct jsonWrite *jw, char *var, char *objRoot, long long id);
/* Print out the jsony type link to another object with a numerical id.  objRoot will start 
 * and end with a '/' and may have additional slashes in this usage. Var may be NULL */

void jsonWriteListStart(struct jsonWrite *jw, char *var);
/* Start an array in JSON. Var may be NULL */

void jsonWriteListEnd(struct jsonWrite *jw);
/* End an array in JSON */

void jsonWriteObjectStart(struct jsonWrite *jw, char *var);
/* Print start of object, preceded by tag if var is non-NULL. */

void jsonWriteObjectEnd(struct jsonWrite *jw);
/* End object in JSON */

void jsonWriteStringf(struct jsonWrite *jw, char *var, char *format, ...)
/* Write "var": "val" where val is jsonStringEscape'd formatted string. */
#if defined(__GNUC__)
__attribute__((format(printf, 3, 4)))
#endif
;


void jsonWriteBoolean(struct jsonWrite *jw, char *var, boolean val);
/* Write out "var": true or "var": false depending on val (no quotes around true/false). */

void jsonWriteValueLabelList(struct jsonWrite *jw, char *var, struct slPair *pairList);
/* Print out a named list of {"value": "<pair->name>", "label": "<pair->val>"} objects. */

void jsonWriteSlNameList(struct jsonWrite *jw, char *var, struct slName *slnList);
/* Print out a named list of strings from slnList. */

void jsonWriteAppend(struct jsonWrite *jwA, char *var, struct jsonWrite *jwB);
/* Append jwB's contents to jwA's.  If jwB is non-NULL, it must be fully closed (no unclosed
 * list or object).  If var is non-NULL, write it out as a tag before appending.
 * If both var and jwB are NULL, leave jwA unchanged. */

int jsonWritePopToLevel(struct jsonWrite *jw, uint level);
/* Close out the objects and lists that are deeper than level, so we end up at level ready to
 * add new items.  Return the level that we end up with, which may not be the same as level,
 * if level is deeper than the current stack. */

#endif /* JSONWRITE_H */
