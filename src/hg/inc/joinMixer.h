/* joinMixer - plan multiple joins by sql joins and/or hashJoins, depending on which
 * should work best for each joined table. */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef JOINMIXERH
#define JOINMIXERH

#include "hashJoin.h"

struct joinMixer
// Scheme for joining by sql and/or hashJoin(s).  Caller constructs sql query, provides
// big row for use by hashJoins, and invokes hashJoins to get final output.
{
    struct joinerPair *sqlRouteList;	// Route list of tables to be joined in mainTable sql query
    struct joinerDtf *sqlFieldList;	// List of fields to select in sql query
    struct hashJoin *hashJoins;		// List of hash-based table joiners init'd with big row ix
    uint *outIxs;			// Index of each output field in big row used by hashJoins
    uint bigRowSize;			// Size of big row (may include mysql output fields,
					// hashJoin keys from mysql, hashJoin output fields,
					// and hashJoin keys for other hashJoins)
};


struct joinMixer *joinMixerNew(struct joiner *joiner, char *db, char *mainTable,
                               struct joinerDtf *outputFieldList,
                               uint mainTableRowCount, boolean naForMissing);
/* If outputFieldList contains fields from more than one table, use joiner to figure
 * out the route of table joins to relate all fields; for each table, predict whether
 * it would be more efficient to join by sql or by hashJoin, taking into account
 * anticipated row counts.  Return info sufficient for building a sql query, a list
 * of hashJoins, bigRow size, and indexes in bigRow for each output.
 * If naForMissing is TRUE then the hashJoiner result columns will contain "n/a" when
 * there is no match in the hash. */

void joinMixerFree(struct joinMixer **pJm);
/* Free joinMixer's holdings unless already NULL. */

#endif//ndef JOINMIXERH
