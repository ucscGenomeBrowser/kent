/* csv - stuff to help process comma separated values.  Have to wrap quotes around
 * things with commas, and escape quotes with more quotes sometimes. */

#ifndef CSV_H
#define CSV_H

void csvEscapeAndAppend(struct dyString *dy, char *string);
/* Append escaped string to dy.  Will insert comma if dy is non-empty */

char *csvEscapeToDyString(struct dyString *dy, char *string);
/* Wrap string in quotes if it has any commas.  Put result into dy, and return it as a 
 * string.   Anything already in quotes get double-quoted */

void csvWriteVal(char *val, FILE *f);
/* Write val, which may have some quotes or commas in it, in a way to be compatable with
 * csv list representation */

char *csvParseNext(char **pos, struct dyString *scratch);
/* Return next value starting at pos, putting results into scratch and
 * returning scratch->string or NULL if no values left. Will update *pos
 * to after trailing comma if any. This will tolerate and ignore leading
 * and trailing white space.  
 *     Since an empty or all-white string will return NULL, if you
 * want empty strings to be a legitimate value then they have to be quoted
 * or followed by a comma. */

boolean csvNeedsParsing(char *s);
/* Return TRUE if s is something that needs parsing through the csv parser.  That
 * is it either starts with a quote or has a comma */

struct slName *csvParse(char *csv);
/* Return a list of parsed out csv values.  Do a slFreeList of this when done */

#endif /* CSV_H */

