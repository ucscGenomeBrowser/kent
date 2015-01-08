/* Stuff lifted from hgTables that should be libified. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef LIBIFYME_H
#define LIBIFYME_H

#include "annoFormatVep.h"
#include "annoStreamVcf.h"
#include "annoGrator.h"

boolean lookupPosition(struct cart *cart, char *cartVar);
/* Look up position if it is not already seq:start-end.
 * Return FALSE if it has written out HTML showing multiple search results.
 * If webGotWarnings() is true after this returns FALSE, no match was found
 * and a warning box was displayed, in which case it's good to reset position
 * to cart's lastPosition before proceeding. */

void nbSpaces(int count);
/* Print some non-breaking spaces. */

#endif//ndef LIBIFYME_H
