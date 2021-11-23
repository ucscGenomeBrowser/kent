/*
 * Information about genbank accessions that have been aligned.
 */

/* Copyright (C) 2003 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef GBALIGNED_H
#define GBALIGNED_H
#include <stdio.h>
#include <stdlib.h>
#include "common.h"

struct gbRelease;
struct gbSelect;
struct gbGenome;

struct gbAligned
/* Object associated with an aligned sequence for an update and genome. */
{
    struct gbAligned* next;       /* next (older) file */
    struct gbEntry* entry;        /* entry we are associated with */
    struct gbUpdate* update;      /* update we are associated with */
    struct gbAligned* updateLink; /* update list */
    UBYTE alignOrgCat;            /* Organism category of alignment.  Might
                                   * not match entry if new organism aliases
                                   * were added or organism name was
                                   * changed. */
    BYTE version;                 /* version number */
    short numAligns;              /* number of alignments */
};


/* extension for aligned index */
extern char* ALIDX_EXT;

struct gbAligned* gbAlignedNew(struct gbEntry* entry, struct gbUpdate* update,
                               int version);
/* Create a gbAligned object */

void gbAlignedCount(struct gbAligned* aligned, int numAligns);
/* increment the count of alignments */

void gbAlignedGetPath(struct gbSelect* select, char* ext, char* workDir,
                      char* path);
/* Get the path to an aligned file base on selection criteria. The
 * select.orgCat field must be set. If workDir not NULL, then this is the work
 * directory used as the base for the path.  If it is NULL, then the aligned/
 * directory is used. */

boolean gbAlignedGetIndex(struct gbSelect* select, char* idxPath);
/* Get the path to an aligned index file, returning false if it
 * doesn't exist.  If path is null, just check for existance. */

void gbAlignedWriteIdxRec(FILE* fh, char* acc, int version, int count);
/* Write a record to an index file. */

void gbAlignedParseIndex(struct gbSelect* select);
/* Parse an aligned *.alidx file if it exists and is readable.  Will
 * read native, xeno, or both based on select. */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
