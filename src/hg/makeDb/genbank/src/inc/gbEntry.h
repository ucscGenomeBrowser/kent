/*
 * Object with information about an entry in genbank.  A gbEntry object
 * is associated with a single accession, and tracks the versions and
 * files associated with it.
 */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef GBENTRY_H
#define GBENTRY_H

#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "gbDefs.h"
struct gbRelease;
struct gbUpdate;

struct gbEntry
/* Object associated with a given GenBank accession.  The selectVer field is
 * used to specify the version of this entry that is selected for a given
 * task.  This is not part of the index, only kept here to avoid building
 * another hash table.
 */
{
    char* acc;                       /* accession */
    BYTE selectVer;                  /* Version selected for processing by
                                      * current task. NULL_VERSION if not
                                      * set. */
    UBYTE type;                      /* mRNA or, EST */
    UBYTE orgCat;                    /* GB_NATIVE or GB_XENO */
    UBYTE clientFlags;               /* byte of flags that can be used
                                      * by client */
    struct gbProcessed* processed;   /* entry processed/, newest first */
    struct gbAligned* aligned;       /* aligned object, newest first */
};

struct gbEntry* gbEntryNew(struct gbRelease* release, char* acc,
                           unsigned type);
/* allocate a new gbEntry object and add it to the release */

struct gbProcessed* gbEntryFindProcessed(struct gbEntry* entry,
                                         int version);
/* Find the newest processed object for a specific version, or NULL not
 * found */

struct gbProcessed* gbEntryFindUpdateProcessed(struct gbEntry* entry,
                                               struct gbUpdate* update);
/* Find the processed object for a specific update, or NULL if not in this
 * update. */

struct gbProcessed* gbEntryAddProcessed(struct gbEntry* entry,
                                        struct gbUpdate* update,
                                        int version, time_t modDate,
                                        char* organism, enum molType molType);
/* Create a new processed object in the entry and link with it's update */

struct gbAligned* gbEntryFindAligned(struct gbEntry* entry,
                                     struct gbUpdate* update);
/* Find an aligned entry for a specific update, or NULL not * found */

struct gbAligned* gbEntryFindAlignedVer(struct gbEntry* entry, int version);
/* Find an aligned entry for a specific version, or NULL not found */

struct gbAligned* gbEntryGetAligned(struct gbEntry* entry,
                                    struct gbUpdate* update, int version,
                                    boolean *created);
/* Get an aligned entry for a specific version, creating a new one if not
 * found */

boolean gbEntryMatch(struct gbEntry* entry, unsigned flags);
/* Determine if an entry matches the entry type flag (GB_MRNA or GB_EST) in
 * flags, and if organism category is in flags (GB_NATIVE or GB_XENO),
 * check this as well. */

void gbEntryDump(struct gbEntry* entry, FILE* out, int indent);
/* print a gbEntry object */
#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
