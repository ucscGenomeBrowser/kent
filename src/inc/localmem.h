/* LocalMem.h - local memory routines. 
 * 
 * These routines are meant for the sort of scenario where
 * a lot of little to medium size pieces of memory are
 * allocated, and then disposed of all at once.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef LOCALMEM_H
#define LOCALMEM_H

struct lm *lmInit(int blockSize);
/* Create a local memory pool. Parameters are:
 *      blockSize - how much system memory to allocate at a time.  Can
 *                  pass in zero and a reasonable default will be used.
 */

void lmCleanup(struct lm **pLm);
/* Clean up a local memory pool. */

size_t lmAvailable(struct lm *lm);
// Returns currently available memory in pool

size_t lmSize(struct lm *lm);
// Returns current size of pool, even for memory already allocated

void *lmAlloc(struct lm *lm, size_t size);
/* Allocate memory from local pool. */

void *lmAllocMoreMem(struct lm *lm, void *pt, size_t oldSize, size_t newSize);
/* Adjust memory size on a block, possibly relocating it.  If block is grown,
 * new memory is zeroed. NOTE: in RARE cases, same pointer may be returned. */

void *lmCloneMem(struct lm *lm, void *pt, size_t size);
/* Return a local mem copy of memory block. */


char *lmCloneStringZ(struct lm *lm, char *string, int size);
/* Return local mem copy of string of given size, adding null terminator. */

char *lmCloneString(struct lm *lm, char *string);
/* Return local mem copy of string. */

char *lmCloneFirstWord(struct lm *lm, char *line);
/* Clone first word in line */

char *lmCloneSomeWord(struct lm *lm, char *line, int wordIx);
/* Return a clone of the given space-delimited word within line.  Returns NULL if
 * not that many words in line. */

struct slName *lmSlName(struct lm *lm, char *name);
/* Return slName in memory. */

#define lmAllocVar(lm, pt) (pt = lmAlloc(lm, sizeof(*pt)));
/* Shortcut to allocating a single variable in local mem and
 * assigning pointer to it. */

#define lmCloneVar(lm, pt) lmCloneMem(lm, pt, sizeof((pt)[0]))
/* Allocate copy of a structure. */

#define lmAllocArray(lm, pt, size) (pt = lmAlloc(lm, sizeof(*pt) * (size)))
/* Shortcut to allocating an array in local mem and
 * assigning pointer to it. */

#endif//ndef LOCALMEM_H
