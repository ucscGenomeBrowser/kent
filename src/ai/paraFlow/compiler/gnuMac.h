/* gnuMac - Stuff concerned especially with code generation for
 * the Gnu Mac pentium assembler . */

#ifndef GNUMAC_H
#define GNUMAC_H

#ifndef ISX_H
#include "isx.h"
#endif

#ifndef ASMCODER_H
#include "asmCoder.h"
#endif 

void gnuMacModulePreamble(FILE *f);
/* Print out various incantations needed at start of every
 * source file for working on Mac OS X on Pentiums, or at
 * least on my mini. Also print initialized vars. */

void gnuMacModulePostscript(FILE *f);
/* Print out various incantations needed at end of every
 * source file for working on Mac OS X on Pentiums, or at
 * least on my mini. Also print uninitialized vars. */

void gnuMacMainStart(FILE *f);
/* Declare main function start. */

void gnuMacMainEnd(FILE *f);
/* Declare main function end. */

void gnuMacModuleVars(struct dlList *iList, FILE *f);
/* Print out info on module level variables. */

void gnuMacFunkyThunky(FILE *f);
/* Do that call to the funky get thunk thingie
 * that Mac does to make the code more relocatable
 * at the expense of burning the ebx register and
 * adding overhead to every single d*ng subroutine
 * almost! */

struct asmCoder *gnuMacPentiumAsmCoder();
/* Get coder for pentium running on mac. */

#endif /* GNUMAC_H */
