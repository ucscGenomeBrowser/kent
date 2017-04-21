/* sqlReserved - stuff to identify SQL reserved words. */

#ifndef SQLRESERVED_H
#define SQLRESERVED_H

/* Array of all SQL reserved words according to MySQL docs in March 2017 */
extern char *sqlReservedWords[];

/* Size of sqlReservedWord array */
extern int sqlReservedWordCount;

struct hash *sqlReservedHash();
/* Make up a hash of all mySQL reserved words in upper case.  Use with
 * sqlReservedCheck().  Free with hashFree() */

boolean sqlReservedCheck(struct hash *sqlReservedHash, char *s);
/* Return TRUE if s is a reserved symbol for mySQL. */

#endif /* SQLRESERVED_H */
