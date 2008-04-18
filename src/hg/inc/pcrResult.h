/* pcrResult -- support for internal track of hgPcr results. */

#ifndef PCRRESULT_H
#define PCRRESULT_H

#include "targetDb.h"
#include "cart.h"

char *pcrResultCartVar(char *db);
/* Returns the cart variable name for PCR result track info for db. 
 * Don't free the statically allocated result. */

boolean pcrResultParseCart(struct cart *cart, char **retBedFile,
			   char **retPrimerFile,
			   struct targetDb **retTarget);
/* Parse out hgPcrResult cart variable into components and make sure
 * they are valid.  If so, set *ret's and return TRUE.  Otherwise, null out 
 * *ret's and return FALSE.  ret's are ignored if NULL. */

#endif /* PCRRESULT_H */
