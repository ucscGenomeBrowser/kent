/* Handle escaping for XML files.  Deal with things like
 * &amp; and &quot. */

#ifndef XMLESCAPE_H
#define XMLESCAPE_H

struct hash *xmlEscapeSymHash();
/* Return hash of predefined xml character symbols to lookup. */

void xmlEscapeBytesToFile(unsigned char *buffer, int len, FILE *f);
/* Write buffer of given length to file, escaping as need be. */

void xmlEscapeStringToFile(char *s, FILE *f);
/* Write escaped zero-terminated string to file. */

#endif /* XMLESCAPE_H */

