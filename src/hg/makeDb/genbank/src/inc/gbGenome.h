/*
 * Information about a genome assembly that genbank entries are aligned
 * against.
 */
#ifndef GBGENOME_H
#define GBGENOME_H
#include <stdio.h>
#include <stdlib.h>
#include "common.h"

struct gbGenome
/* Object associated with genome assemble that genbank seqeunces are
 * aligned against. */
{
    char* database;               /* database for assembly */
    char* organism;               /* organism */
    struct dbToSpecies* dbMap;    /* map of db ->species */
};

struct gbGenome* gbGenomeNew(char* database);
/* create a new gbGenome object */

unsigned gbGenomeOrgCat(struct gbGenome* genome, char* organism);
/* Compare a species to the one associated with this genome, returning
 * GB_NATIVE or GB_XENO, or 0 if genome is null. */

void gbGenomeFree(struct gbGenome** genomePtr);
/* free a genome object */
#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
