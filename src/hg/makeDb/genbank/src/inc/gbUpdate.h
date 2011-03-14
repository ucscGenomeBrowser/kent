/*
 * Object per release update (full or daily)
 */
#ifndef GBUPDATE_H
#define GBUPDATE_H
#include "common.h"
#include "gbDefs.h"
struct gbProcessed;
struct gbAligned;

struct gbUpdate
/* Object associated with a specific update (full or daily). */
{
    struct gbUpdate* next;          /* next newest */
    char* name;                     /* update name */
    char* shortName;                /* short name, date or "full" */
    boolean isFull;                 /* is this a full or daily */
    unsigned selectProc;            /* processed selected flags (not in db) */
    unsigned selectAlign;           /* aligned selected flags (not in db) */
    struct gbRelease* release;      /* release we are associated with */
    struct gbProcessed* processed;  /* list of processed entries */
    struct gbAligned* aligned;      /* list of aligned entries */
    int numNativeAligns[GB_NUM_TYPES]; /* native aligns by type */
    int numXenoAligns[GB_NUM_TYPES];   /* xeno aligns by type */
};

extern char* UPDATE_FULL; /* name of a full update */

void gbUpdateNameFromFile(char* updateName, char* fileName);
/* Get an update name from a file name in the dir */

struct gbUpdate* gbUpdateNew(struct gbRelease* release, char* updateName);
/* create a new update object */

void gbUpdateFree(struct gbUpdate **updatePtr);
/* free an update object */

int gbUpdateCmp(struct gbUpdate* update1, struct gbUpdate* update2);
/* compare two updates for sorting. */

void gbUpdateAddAligned(struct gbUpdate* update,
                        struct gbAligned* aligned);
/* add an aligned object associated with the update */

void gbUpdateCountAligns(struct gbUpdate* update, struct gbAligned* aligned,
                         unsigned numAligns);
/* increment the count of the number of alignments */

unsigned gbUpdateGetNumAligns(struct gbUpdate* update,
                              unsigned type, unsigned orgCat);
/* Get the number of alignments for a type and orgCat */

struct gbUpdate* gbUpdateGetPrev(struct gbUpdate* update);
/* get the next oldest update */

void gbUpdateClearSelectVer(struct gbUpdate* update);
/* Clear the selected version and clientFlags fields in all gbEntry objects in
 * the update.  note that since entries are shared by multiple updates, this
 * can't be used to use with multiple updates at the same time.  It's intended
 * to be a quicker way to clear selectVer by limiting it to one update.
 */
#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
