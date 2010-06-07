/* LocalMem.h - local memory routines. 
 * 
 * These routines are meant for the sort of scenario where
 * a lot of little to medium size pieces of memory are
 * allocated, and then disposed of all at once.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

struct lm *lmInit(int blockSize);
/* Create a local memory pool. Parameters are:
 *      blockSize - how much system memory to allocate at a time.  Can
 *                  pass in zero and a reasonable default will be used.
 */

void lmCleanup(struct lm **pLm);
/* Clean up a local memory pool. */

void *lmAlloc(struct lm *lm, size_t size);
/* Allocate memory from local pool. */

char *lmCloneString(struct lm *lm, char *string);
/* Return local mem copy of string. */

char*lmCloneStringZ(struct lm *lm, char *string, int size);
/* Return local mem copy of string of given size, adding null terminator. */

char *lmCloneFirstWord(struct lm *lm, char *line);
/* Clone first word in line */

char *lmCloneSomeWord(struct lm *lm, char *line, int wordIx);
/* Return a clone of the given space-delimited word within line.  Returns NULL if
 * not that many words in line. */

struct slName *lmSlName(struct lm *lm, char *name);
/* Return slName in memory. */

void *lmCloneMem(struct lm *lm, void *pt, size_t size);
/* Return a local mem copy of memory block. */

#define lmAllocVar(lm, pt) (pt = lmAlloc(lm, sizeof(*pt)));
/* Shortcut to allocating a single variable in local mem and
 * assigning pointer to it. */

#define lmAllocArray(lm, pt, size) (pt = lmAlloc(lm, sizeof(*pt) * (size)))
/* Shortcut to allocating an array in local mem and
 * assigning pointer to it. */
