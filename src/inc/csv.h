/* csv - stuff to help process comma separated values.  Have to wrap quotes around
 * things with commas, and escape quotes with more quotes sometimes. */

#ifndef CSV_H
#define CSV_H

char *csvEscapeToDyString(struct dyString *dy, char *string);
/* Wrap string in quotes if it has any commas.  Anything already in quotes get s double-quoted 
 * Returns transformated result, which will be input string if it has no commas, otherwise
 * will be dy*/

void csvWriteVal(char *val, FILE *f);
/* Write val, which may have some quotes or commas in it, in a way to be compatable with
 * csv list representation */

#endif /* CSV_H */

