#ifndef GBALIGNCOMMON_H
#define GBALIGNCOMMON_H
#include "common.h"

struct gbSelect;
struct gbProcessed;
struct gbAligned;
                                   

typedef void gbNeedAlignedCallback(struct gbSelect* select,
                                   struct gbProcessed* processed,
                                   struct gbAligned* prevAligned,
                                   void* clientData);
/* Function type that is called when traversing the list of sequences that
 * need aligned. */

struct gbSelect* gbAlignGetMigrateRel(struct gbSelect* select);
/* Determine if alignments should be migrated from a previous release. */


void gbAlignFindNeedAligned(struct gbSelect* select,
                            struct gbSelect* prevSelect,
                            gbNeedAlignedCallback* callback, void* clientData);
/* Find sequences that need to be aligned for this update.  call the specified
 * funciton on each sequence.  If prevSelect is not NULL, it is check to
 * determine if the sequence is aligned in that release.  If select->orgCat is
 * set to GB_NATIVE or GB_XENO, only matching organisms are returned. */


#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
