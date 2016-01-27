/* chainNetDbLoad - This will load a database representation of
 * a net into a chainNet representation.  Also helps database
 * representation of chain into chain. */

/* Copyright (C) 2002 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef CHAINNETDBLOAD_H
#define CHAINNETDBLOAD_H

#ifndef JKSQL_H
#include "jksql.h"
#endif

#ifndef NETALIGN_H
#include "netAlign.h"
#endif

#ifndef CHAINNET_H
#include "chainNet.h"
#endif


struct cnFill *cnFillFromNetAlign(struct netAlign *na, struct hash *nameHash);
/* Convert netAlign to cnFill. Name hash is a place to store
 * the strings. */

struct chainNet *chainNetLoadResult(struct sqlResult *sr, int rowOffset);
/* Given a query result that returns a bunch netAligns, make up
 * a list of chainNets that has the equivalent information. 
 * Note the net->size field is not filled in. */

struct chainNet *chainNetLoadRange(char *database, char *track,
	char *chrom, int start, int end, char *extraWhere);
/* Load parts of a net track that intersect range. */

struct chainNet *chainNetLoadChrom(char *database, char *track,
	char *chrom, char *extraWhere);
/* Load net on whole chromosome. */

struct chain *chainLoadIdRangeHub(char *database, char *track, char *chrom, 
	int start, int end, int id);
/* Load parts of chain of given ID from bigChain file.  Note the chain header
 * including score, tStart, tEnd, will still reflect the whole chain,
 * not just the part in range.  However only the blocks of the chain
 * overlapping the range will be loaded. */

struct chain *chainLoadIdRange(char *database, char *track, char *chrom, 
	int start, int end, int id);
/* Load parts of chain of given ID from database.  Note the chain header
 * including score, tStart, tEnd, will still reflect the whole chain,
 * not just the part in range.  However only the blocks of the chain
 * overlapping the range will be loaded. */

struct chain *chainLoadId(char *database, char *track, char *chrom, int id);
/* Load chain of given ID from database. */

#endif /* CHAINNETDBLOAD_H */

