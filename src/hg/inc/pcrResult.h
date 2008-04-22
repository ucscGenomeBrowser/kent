/* pcrResult -- support for internal track of hgPcr results. */

#ifndef PCRRESULT_H
#define PCRRESULT_H

#include "targetDb.h"
#include "cart.h"

char *pcrResultCartVar(char *db);
/* Returns the cart variable name for PCR result track info for db. 
 * Don't free the statically allocated result. */

boolean pcrResultParseCart(struct cart *cart, char **retPslFile,
			   char **retPrimerFile,
			   struct targetDb **retTarget);
/* Parse out hgPcrResult cart variable into components and make sure
 * they are valid.  If so, set *ret's and return TRUE.  Otherwise, null out 
 * *ret's and return FALSE.  ret's are ignored if NULL. */

void pcrResultGetPrimers(char *fileName, char **retFPrimer, char **retRPrimer);
/* Given a file whose first line is 2 words (forward primer, reverse primer)
 * set the ret's to upper-cased primer sequences.  
 * Do not free the statically allocated ret's. */

void pcrResultGetPsl(char *fileName, struct targetDb *target, char *item,
		     char *chrom, int itemStart, int itemEnd,
		     struct psl **retItemPsl, struct psl **retOtherPsls);
/* Read in psl from file.  If a psl matches the given item position, set 
 * retItemPsl to that; otherwise add it to retOtherPsls.  Die if no psl
 * matches the given item position. */

#endif /* PCRRESULT_H */
