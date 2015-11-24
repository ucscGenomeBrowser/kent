/* hashJoin - join one or more columns of a hashed database table to an externally provided
 * char **row that contains the key and empty slot(s) for the column value(s) */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef HASHJOIN_H
#define HASHJOIN_H

#include "dystring.h"
#include "hash.h"
#include "joiner.h"
#include "localmem.h"

struct hashJoin;

struct hashJoin *hashJoinNew(struct joinerDtf *keyDtf, uint extRowKeyIx,
                             struct joinerDtf *valDtfs, uint *extRowValIxs,
                             struct joinerField *jfA, struct joinerField *jfB,
                             boolean naForMissing);
/* Return a new hashJoin.  extRowKeyIx is the index in an external row of the key
 * to use in the join.  extRowValIxs[valCount] contains each hash val column's index
 * into an external row.  jfA and jfB are optional; if given, then jfA's separator,
 * chopBefore and chopAfter will be applied to each key retrieved from the external row
 * and jfB's separator, chopBefore and chopAfter will be applied to each hash key.
 * If naForMissing is TRUE then the result columns will contain "n/a" when there is
 * no match in the hash. */

struct hashJoin *hashJoinNext(struct hashJoin *el);
/* Get the next hashJoin in a list of hashJoins. */

void hashJoinOneRow(struct hashJoin *self, char **extRow);
/* Look up some column of extRow in hash and place result(s) in other columns of extRow.
 * Don't call this again until done with extRow -- column value storage is reused. */

void hashJoinFree(struct hashJoin **pSelf);
/* Free hashJoin unless already NULL. */

void hashJoinFreeList(struct hashJoin **pList);
/* Free a list of hashJoins. */

#endif//ndef HASHJOIN_H
