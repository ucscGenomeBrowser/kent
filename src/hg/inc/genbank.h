/* genbank.h - Various functions for dealing with genbank data */
#ifndef GENBANK_H
#define GENBANK_H

struct genbankCds
/* structure return information about parsed CDS */
{
    int start;                    /* start, in [0..n) coords */
    int end;                      /* end, in [0..n) coords */
    boolean startComplete;        /* is start complete? */ 
    boolean endComplete;          /* is end complete? */ 
    boolean complement;           /* is on reverse strand */
};

boolean genbankCdsParse(char *cdsStr, struct genbankCds* cds);
/* Parse a genbank CDS, returning TRUE if it can be successfuly parsed, FALSE
 * if there are problems.  If a join() is specified, the first and last
 * coordinates are used for the CDS.  Incomplete CDS specifications will still
 * return the start or end.  start/end are set to 0 on error. */

boolean genbankParseCds(char *cdsStr, unsigned *cdsStart, unsigned *cdsEnd);
/* Compatiblity function, genbankCdsParse is prefered. Parse a genbank CDS,
 * returning TRUE if it can be successfuly parsed, FALSE if there are
 * problems.  If a join() is specified, the first and last coordinates are
 * used for the CDS.  Incomplete CDS specifications will still return the
 * start or end.  cdsStart and cdsEnd are set to -1 on error.
 */

#endif
