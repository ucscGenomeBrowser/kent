/*
 * Table that keeps track of which partition (type, accPrefix) of the data
 * have been loaded for a release and update.  This saves a huge amount of
 * time over checking for individual sequences.
 */
#ifndef GBLOADEDTBL_H
#define GBLOADEDTBL_H

#include "common.h"
struct sqlConnection;
struct gbRelease;
struct gbSelect;

struct gbLoaded
/* Entry in the table. */
{
    struct gbLoaded* next;
    unsigned short srcDb;   /* GB_GENBANK or GB_REFSEQ */
    char *loadRelease;      /* release version */
    char *loadUpdate;       /* update (date or full) */
    unsigned type;          /* GB_MRNA or GB_EST */
    char *accPrefix;        /* Accession prefix or null */
};

struct gbLoadedTbl
/* Object to managing in-memory data of gbLoaded table */
{
    struct gbLoadedTbl* next;
    struct hash* releaseHash; /* hash of releases that have been loaded */
    struct hash *entryHash;  /* Hash on key constructed from
                              * srcDb+release+update+type+accPrefix.
                              * Hash localmem is used for struct allocations
                              * as well */
    struct gbLoaded* uncommitted;  /* need to be committed to table */
};

struct gbLoadedTbl* gbLoadedTblNew(struct sqlConnection *conn);
/* create a new object for the loaded tables.  Create the table if it doesn't
 * exist. */

void gbLoadedTblUseRelease(struct gbLoadedTbl* loadedTbl,
                           struct sqlConnection *conn,
                           struct gbRelease *release);
/* If the specified release has not been loaded from the database, load it.
 * Must be called before using a release. */

boolean gbLoadedTblIsLoaded(struct gbLoadedTbl* loadedTbl,
                            struct gbSelect *select);
/* Check if the type and accession has been loaded for this update */

void gbLoadedTblAdd(struct gbLoadedTbl* loadedTbl,
                    struct gbSelect *select);
/* add an entry to the table */

void gbLoadedTblCommit(struct gbLoadedTbl* loadedTbl,
                       struct sqlConnection *conn);
/* commit pending changes */

void gbLoadedTblFree(struct gbLoadedTbl** loadedTblPtr);
/* Free the object */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
