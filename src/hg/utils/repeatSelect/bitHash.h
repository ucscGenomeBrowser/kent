/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef BITHASH_H
#define BITHASH_H

#ifndef COMMON_H
#include "common.h"
#endif

#ifndef HASH_H
#include "hash.h"
#endif

struct hash *newBitHash(struct hash *chromHash);
/* Make another hashtable of bitvectors based on the chroms. */

void freeBitEl(struct hashEl *el);
/* Free a value at an el. */

void freeBitHash(struct hash **pHash);
/* Free up bit hash table and all values associated with it. */

#endif
