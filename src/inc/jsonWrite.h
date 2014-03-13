/* jsonWrite - Helper routines for writing out JSON. 
 *
 * Apologies for the awkward 'isMiddle' parameter.  This is
 * from JSON not allowing a terminal comma for a comma separated
 * list.   A larger, more usable library might find a way to
 * take care of this for you. */

#ifndef JSONWRITE_H
#define JSONWRITE_H

void dyJsonTag(struct dyString *dy, char *var);
/* Print out quoted tag followed by colon */

void dyJsonEndLine(struct dyString *dy, boolean isMiddle);
/* Write comma if in middle, and then newline regardless. */

void dyJsonString(struct dyString *dy, char *var, char *string, boolean isMiddle);
/* Print out "var": "val" */

void dyJsonDateFromUnix(struct dyString *dy, char *var, long long unixTimeVal, boolean isMiddle);
/* Add "var": YYYY-MM-DDT-HH:MM:SSZ given a Unix time stamp */

void dyJsonNumber(struct dyString *dy, char *var, long long val, boolean isMiddle);
/* print out "var": val as number */

void dyJsonLink(struct dyString *dy, char *var, char *objRoot, char *name, boolean isMiddle);
/* Print out the jsony type link to another object.  objRoot will start and end with a '/'
 * and may have additional slashes in this usage. */

void dyJsonLinkNum(struct dyString *dy, char *var, char *objRoot, long long id, boolean isMiddle);
/* Print out the jsony type link to another object with a numerical id.  objRoot will start 
 * and end with a '/' and may have additional slashes in this usage. */

void dyJsonListStart(struct dyString *dy, char *var);
/* Start an array in JSON */

void dyJsonListEnd(struct dyString *dy, boolean isMiddle);
/* End an array in JSON */

void dyJsonObjectStart(struct dyString *dy);
/* Print start of object */

void dyJsonObjectEnd(struct dyString *dy, boolean isMiddle);
/* End object in JSON */


#endif /* JSONWRITE_H */
