/* chainNetDbLoad - This will load a database representation of
 * a net (netAlign) into a chainNet representation. */

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

#endif /* CHAINNETDBLOAD_H */

