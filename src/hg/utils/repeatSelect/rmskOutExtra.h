/* This keeps a rmskOut plus a score. Oh isn't it grand? */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef RMSKOUTEXTRA_H
#define RMSKOUTEXTRA_H

#ifndef COMMON_H
#include "common.h"
#endif

#ifndef HASH_H
#include "hash.h"
#endif

#ifndef RMSKOUT_H
#include "rmskOut.h"
#endif

struct rmskOutExtra
    {
    struct rmskOutExtra *next;
    struct rmskOut *rmsk;
    int numMasked;
    };

void rmskOutExtraFree(struct rmskOutExtra **pThing);
/* Free one */

void rmskOutExtraFreeList(struct rmskOutExtra **pList);
/* Free a list of dynamically allocated rmskOutExtra's */

void addToRmskOutExtraHash(struct hash *repHash, struct rmskOutExtra *addme);
/* Add one to the hash of lists, indexed on the rmsk->repName. */

void freeRmskOutExtraHash(struct hash **pHash);
/* Free up that crazy hash. */

#endif
