/* jsonWrite - Helper routines for writing out JSON. */

#ifndef JSONWRITE_H
#define JSONWRITE_H

struct jsonWrite
/* Object to help output JSON */
     {
     struct jsonWrite *next;
     struct dyString *dy;	/* Most of this module is building json text in here */
     bool objStack[128];	/* We need stack deep enough to handle nested objects and lists */
     int stackIx;		/* Current index in stack */
     };

struct jsonWrite *jsonWriteNew();
/* Return new empty jsonWrite struct. */

void jsonWriteFree(struct jsonWrite **pJw);
/* Free up a jsonWrite object. */

void jsonWriteTag(struct jsonWrite *jw, char *var);
/* Print out quoted tag followed by colon. Print out preceding comma if need be.  */

void jsonWriteEndLine(struct jsonWrite *jw);
/* Write comma if in middle, and then newline regardless. */

void jsonWriteString(struct jsonWrite *jw, char *var, char *string);
/* Print out "var": "val".  If var is NULL, print val only.  If string is NULL, "var": null . */

void jsonWriteDateFromUnix(struct jsonWrite *jw, char *var, long long unixTimeVal);
/* Add "var": YYYY-MM-DDT-HH:MM:SSZ given a Unix time stamp. Var may be NULL. */

void jsonWriteNumber(struct jsonWrite *jw, char *var, long long val);
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

void jsonWriteStringf(struct jsonWrite *jw, char *var, char *format, ...);
/* Write "var": "val" where val is jsonStringEscape'd formatted string. */

void jsonWriteBoolean(struct jsonWrite *jw, char *var, boolean val);
/* Write out "var": true or "var": false depending on val (no quotes around true/false). */

void jsonWriteValueLabelList(struct jsonWrite *jw, char *var, struct slPair *pairList);
/* Print out a named list of {"value": "<pair->name>", "label": "<pair->val>"} objects. */

void jsonWriteSlNameList(struct jsonWrite *jw, char *var, struct slName *slnList);
/* Print out a named list of strings from slnList. */

#endif /* JSONWRITE_H */
