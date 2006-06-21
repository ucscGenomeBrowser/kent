/* Quoted Printable encoding and decoding.
 * by Galt Barber */

#ifndef QUOTEDP_H
#define QUOTEDP_H

char *quotedPrintableEncode(char *input);
/* Use Quoted-Printable standard to encode a string.
 */

boolean quotedPCollapse(char *line);
/* Use Quoted-Printable standard to decode a string.
 * Return true if the line does not end in '='
 * which indicate continuation. */

char *quotedPrintableDecode(char *input);
/* Use Quoted-Printable standard to decode a string.  Return decoded
 * string which will be freeMem'd. */

#endif /* QUOTEDP_H */

