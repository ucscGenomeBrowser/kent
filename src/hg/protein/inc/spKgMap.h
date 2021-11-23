/* spKgMap - map swissprot accs to known gene ids */

/* Copyright (C) 2005 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef SPKGMAP_H
#define SPKGMAP_H
#include "common.h"
#include "spKgMap.h"
#include "jksql.h"
#include "kgXref.h"
#include "localmem.h"
#include "hash.h"
#include "linefile.h"

struct spKgMap;
struct sqlConnection;

struct spKgMap *spKgMapNew(struct sqlConnection *conn);
/* load data from from kgXRef and kgProtAlias tables to map from swissprot
 * accs to known gene ids. */

void spKgMapFree(struct spKgMap **spKgMapPtr);
/* free a spKgMap structure */

struct slName *spKgMapGet(struct spKgMap *spKgMap, char *spAcc);
/* Get the list of known gene ids for a swissprot acc.  If not found NULL is
 * returned. */
#endif
