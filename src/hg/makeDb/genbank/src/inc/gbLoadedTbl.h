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
    struct gbLoaded* next;  /* used for commit list */
    struct gbLoaded* relNext;  /* used for release list */
    unsigned short srcDb;   /* GB_GENBANK or GB_REFSEQ */
    char *loadRelease;      /* release version */
    char *loadUpdate;       /* update (date or full) */
    unsigned type;          /* GB_MRNA or GB_EST */
    char *accPrefix;        /* Accession prefix or null */
    boolean extFileUpdated; /* the gbExtFile tables has been updated for this
                             * partation of the release */
    /* this are not in the database */
    boolean isNew;          /* entry has is not in database */
    boolean isDirty;        /* entry has been modified; it not new,
                             * only the extFileUpdated flag can be set */
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
    struct sqlConnection *conn;    /* connection to use to access the table */
};

struct gbLoadedTbl* gbLoadedTblNew(struct sqlConnection *conn);
/* create a new object for the loaded tables.  Create the table if it doesn't
 * exist. */

boolean gbLoadedTblHaveRelease(struct gbLoadedTbl* loadedTbl,
                               char *relName);
/* check if the specified release is in the table. */

void gbLoadedTblUseRelease(struct gbLoadedTbl* loadedTbl,
                           struct gbRelease *release);
/* If the specified release has not been loaded from the database, load it.
 * Must be called before using a release. */

boolean gbLoadedTblIsLoaded(struct gbLoadedTbl* loadedTbl,
                            struct gbSelect *select);
/* Check if the type and accPrefix has been loaded for this update */

struct gbLoaded *gbLoadedTblGetEntry(struct gbLoadedTbl* loadedTbl,
                                     struct gbSelect *select);
/* get the specified entry, or NULL.  It may not have been commited yet */

boolean gbLoadedTblHasEntry(struct gbLoadedTbl* loadedTbl,
                            struct gbSelect *select);
/* Check if the the specified entry exists. It may not have been commited
 * yet */

void gbLoadedTblAdd(struct gbLoadedTbl* loadedTbl,
                    struct gbSelect *select);
/* add an entry to the table */

boolean gbLoadedTblExtFileUpdated(struct gbLoadedTbl* loadedTbl,
                                  struct gbSelect *select);
/* Check if the type and accPrefix has had their extFile entries update
 * for this release. */

void gbLoadedTblSetExtFileUpdated(struct gbLoadedTbl* loadedTbl,
                                  struct gbSelect *select);
/* Flag that type and accPrefix has had their extFile entries update
 * for this release. */

void gbLoadedTblCommit(struct gbLoadedTbl* loadedTbl);
/* commit pending changes */

void gbLoadedTblFree(struct gbLoadedTbl** loadedTblPtr);
/* Free the object */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
