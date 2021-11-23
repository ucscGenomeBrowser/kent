/*
 * Information about a genome assembly that genbank entries are aligned
 * against.
 */

/* Copyright (C) 2003 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef GBGENOME_H
#define GBGENOME_H
#include <stdio.h>
#include <stdlib.h>
#include "common.h"

struct gbGenome
/* Object associated with genome assemble that genbank seqeunces are
 * aligned against. */
{
    struct gbGenome* next;
    char* database;               /* database for assembly */
    char* organism;               /* organism */
    struct dbToSpecies* dbMap;    /* map of db ->species */
};

struct gbGenome* gbGenomeNew(char* database);
/* create a new gbGenome object */

char* gbGenomePreferedOrgName(char* organism);
/* determine the prefered organism name, if this organism is known,
 * otherwise NULL.  Used for sanity checks. Names are in static table,
 * so ptrs can be compared. */

unsigned gbGenomeOrgCat(struct gbGenome* genome, char* organism);
/* Compare a species to the one associated with this genome, returning
 * GB_NATIVE or GB_XENO, or 0 if genome is null. */

void gbGenomeFree(struct gbGenome** genomePtr);
/* free a genome object */

void gbGenomeFreeList(struct gbGenome** genomeList);
/* free a list of genome objects */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
