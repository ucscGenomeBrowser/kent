/* chainNetDbLoad - This will load a database representation of
 * a net into a chainNet representation.  Also helps database
 * representation of chain into chain. */

/* Copyright (C) 2002 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

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

#ifndef BIGNET_H
#include "bigNet.h"
#endif

#ifndef BIGBED_H
#include "bigBed.h"
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

struct chainNet *chainNetLoadRangeHub(char *fileName, char *chrom, int start, int end);
/* Load the parts of a bigNet file that intersect range into a chainNet.
 * Note the net->size field is not filled in. */

struct chainNet *chainNetLoadRangeQuickLift(char *quickLiftFile, char *fileName,
                                            char *chrom, int start, int end);
/* Load the part of a bigNet file that quickLifts into chrom:start-end, and build a
 * chainNet in the destination assembly's coordinates.  Only the target side of the net
 * moves; the query side describes a third assembly and is carried across untouched.
 * Note the net->size field is not filled in. */

struct bigNet *bigNetFromInterval(struct bbiFile *bbi, struct bigBedInterval *bb,
                                  char *fileName, struct bigNet *bn);
/* Fill in bn from one interval of a bigNet file.  The chrom name is the one the file
 * carries, which for a quickLifted net is in the source assembly. */

struct chainNet *chainNetLoadChrom(char *database, char *track,
	char *chrom, char *extraWhere);
/* Load net on whole chromosome. */

struct chain *chainLoadIdRangeHub(char *database, char *fileName, char *track, char *chrom, 
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

