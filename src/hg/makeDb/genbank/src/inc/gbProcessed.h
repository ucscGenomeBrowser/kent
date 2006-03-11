/*
 * Information about genbank accessions that have been processed.
 */
#ifndef GBPROCESSED_H
#define GBPROCESSED_H

#include "common.h"
#include <unistd.h>
struct gbRelease;
struct gbSelect;

struct gbProcessed
/* Object associated with an accession that has been processed.
 * Organism name is kept here to allow for better error checking
 * on name changes. */
{
    struct gbProcessed* next;       /* next (older) file */
    struct gbEntry* entry;          /* entry we are associated with */
    struct gbUpdate* update;        /* update we are associated with */
    struct gbProcessed* updateLink; /* update list */
    short version;                  /* version number */
    short orgCat;                   /* organism category */
    time_t modDate;                 /* GenBank modification date */
    char* organism;                 /* organism name */
};

/* extension for processed index */
extern char* GBIDX_EXT;

struct gbProcessed* gbProcessedNew(struct gbEntry* entry,
                                   struct gbUpdate* update,
                                   int version, time_t modDate,
                                   char* organism);
/* Create a new gbProcessed object */

void gbProcessedWriteIdxRec(FILE* fh, char* acc, int version,
                            char* modDate, char* organism);
/* Write a record to the processed index */

void gbProcessedGetDir(struct gbSelect* select, char* dir);
/* Get the processed directory based on selection criteria. */

void gbProcessedGetPath(struct gbSelect* select, char* ext, char* path);
/* Get the path to a processed file base on selection criteria. */

boolean gbProcessedGetPepFa(struct gbSelect* select, char* path);
/* Get the path to a peptide fasta file base on selection criteria.
 * Return false if there is not peptide file for the select srcDb. */

boolean gbProcessedGetIndex(struct gbSelect* select, char* idxPath);
/* Get the path to an aligned index file, returning false if it
 * doesn't exist.  If path is null, just check for existance. */

void gbProcessedParseIndex(struct gbSelect* select);
/* Parse a processed index file if it exists and is readable */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
