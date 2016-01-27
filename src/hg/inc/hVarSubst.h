/** Handle variable substitutions in strings from trackDb and other
 * labels. See trackDb/README for descriptions of values that 
 * can be substitute. */

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef hVarSubst_h
#define hVarSubst_h

#include "trackDb.h"
#include "cart.h"


char *hVarSubst(char *desc, struct trackDb *tdb, char *database, char *src);
/* Parse a string and substitute variable references.  Return NULL if
 * no variable references were found.  Error on missing variables (except
 * $matrix).  desc is a brief description to print on error to help with
 * debugging. tdb maybe NULL to only do substitutions based on database
 * and organism. See trackDb/README for more information.*/

void hVarSubstInVar(char *desc, struct trackDb *tdb, char *database, char **varPtr);
/* hVarSubst on a dynamically allocated string, replacing string in substitutions
 * occur, freeing the old memory if necessary.  See hVarSubst for details.
 */

void hVarSubstWithCart(char *desc, struct cart *cart, struct trackDb *tdb, char *database,
                       char **varPtr);
/* Like hVarSubstInVar, but if cart is non-NULL, $hgsid will be substituted. */

void hVarSubstTrackDb(struct trackDb *tdb, char *database);
/* Substitute variables in trackDb shortLabel, longLabel, and html fields. */

#endif
