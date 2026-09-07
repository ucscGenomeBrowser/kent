/** Handle variable substitutions in strings from trackDb and other
 * labels. See trackDb/README for descriptions of values that 
 * can be substitute. */

/* Copyright (C) 2009 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
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

void hVarSubstTrackDbHtml(struct cart *cart, struct trackDb *tdb, char *database);
/* Substitute variables in the description page of a hub track.  Native trackDb needs no
 * such call: hgTrackDb already substituted the html when it loaded trackDb.  A hub's html
 * comes straight off the hub's web server and has never been through substitution, so it
 * is done here, at render time, where $db, $hgsid and $parentTrack resolve to the hub_<id>_
 * names the CGIs actually use.  Only a short list of variables is recognized and nothing
 * is an error, so a dollar sign in a description page that was not written with this in
 * mind stays a dollar sign. */

#endif
