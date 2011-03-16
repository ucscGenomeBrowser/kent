/*
 * Object per genbank/refseq release directory.
 */
#ifndef GBRELEASE_H
#define GBRELEASE_H

#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
struct gbSelect;

struct gbRelease
/* Object that represents a single release.  These are created in an empty
 * state for every release directory found.  Most of the object is
 * uninitialized until data is load into it.  There is a memory pool of
 * associated with entries, which can be load and released for each partition
 * to minimize the required memory.  A pool of organism name is shared by
 * entries.  The gbIgnore file is not strictly associated with a release, only
 * a source database.  However we load entries on for one release.
 */
{
    struct gbRelease* next;    /* next oldest release */
    unsigned srcDb;            /* id of srcDb (GB_GENBANK, GB_REFSEQ) */
    char* name;                /* release/version: genbank.130.0 */
    char version[9];           /* version: 130.0 */
    struct gbIndex* index;     /* index we are associated with */
    struct gbUpdate* updates;  /* list of updates, oldest first */
    struct gbGenome* genome;   /* genome this release was loaded for */
    struct gbIgnore* ignore;   /* entries to ignore */
    struct hash* orgNames;     /* hashed string pool of organism names */
    struct hash* entryTbl;     /* table of gbEntry objects, indexed by acc,
                                * local mem used for entry date */
    int numMRnas;              /* Number of mRNA entries. */
    int numEsts;               /* Number of EST entries. */
};

struct gbRelease* gbReleaseNew(struct gbIndex* index, int srcDb, char* name);
/* Construct a new gbRelease object in the empty state */

void gbReleaseUnload(struct gbRelease* release);
/* Unload entry data, freeing memory, keep release and update objects */

void gbReleaseFree(struct gbRelease** relPtr);
/* Free a gbRelease object */

#define gbReleaseAllocEntryVar(release, ptrVar) \
/* allocate and zero a struct associated with entries. */ \
    lmAllocVar(release->entryTbl->lm, ptrVar)

char* gbReleaseObtainOrgName(struct gbRelease* release, char* orgName);
/* obtain an organism name from the string pool */

struct gbUpdate* gbReleaseGetUpdate(struct gbRelease* release,
                                    char* updateName);
/* find or create an update */

struct gbUpdate* gbReleaseMustFindUpdate(struct gbRelease* release,
                                         char* updateName);
/* find a update or generate an error */

struct gbEntry* gbReleaseFindEntry(struct gbRelease* release, char* acc);
/* find an entry object or null if not found */

struct slName* gbReleaseFindIndices(struct gbRelease* release, char* section,
                                    struct gbGenome* genome, char* fileName,
                                    char* accPrefix);
/* Build list of index files matching the specified parameters, checked
 * that they are readable.  If accPrefix is not NULL, it is the accession
 * prefix used to partition files. */

struct slName* gbReleaseGetAccPrefixes(struct gbRelease* release,
                                       unsigned state, unsigned type);
/* Find all accession prefixes for processed or aligned data (based
 * on state). */

void gbReleaseLoadIgnore(struct gbRelease* release);
/* Load the gbIgnore object if not already loaded.  Also loaded by by
 * gbReleaseLoadProcessed/Aligned, this allows access before one of those
 * methods is called.
 */

void gbReleaseLoadProcessed(struct gbSelect* select);
/* load index files from an aligned directory. */

void gbReleaseLoadAligned(struct gbSelect* select);
/* load index files from an aligned directory. */

void gbReleaseClearSelectVer(struct gbRelease* release);
/* clear the selected version and clientFlags fields in all gbEntry objects in
 * the release */

void gbReleaseDump(struct gbRelease* release, FILE* out, int indent);
/* print a gbRelease object for debugging */

boolean gbReleaseHasData(struct gbRelease* release, unsigned state,
                         unsigned type, char* accPrefix);
/* determine if the release has any processed or aligned entries, based on
 * state, for the specified type */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
