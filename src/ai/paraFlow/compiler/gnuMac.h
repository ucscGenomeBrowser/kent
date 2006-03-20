/* gnuMac - Stuff concerned especially with code generation for
 * the Gnu Mac pentium assembler . */

#ifndef GNUMAC_H
#define GNUMAC_H

#ifndef ISX_H
#include "isx.h"
#endif /* ISX_H */

void gnuMacPreamble(struct dlList *iList, FILE *f);
/* Print out various incantations needed at start of every
 * source file for working on Mac OS X on Pentiums, or at
 * least on my mini. Also print initialized vars. */

void gnuMacPostscript(struct dlList *iList, FILE *f);
/* Print out various incantations needed at end of every
 * source file for working on Mac OS X on Pentiums, or at
 * least on my mini. Also print uninitialized vars. */

#endif /* GNUMAC_H */
