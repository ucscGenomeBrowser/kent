/* genbank.h - Various functions for dealing with genbank data */
#ifndef GENBANK_H
#define GENBANK_H

boolean genbankParseCds(char *cdsStr, unsigned *cdsStart, unsigned *cdsEnd);
/* Parse a genbank CDS, returning TRUE if it can be successfuly parsed, FALSE
 * if there are problems.  If a join() is specified, the first and last
 * coordinates are used for the CDS.  Incomplete CDS specifications will still
 * return the start or end.
 */

#endif
