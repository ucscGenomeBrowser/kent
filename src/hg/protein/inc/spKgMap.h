/* spKgMap - map swissprot accs to known gene ids */
#ifndef SPKGMAP_H
#define SPKGMAP_H
#include "common.h"
#include "spKgMap.h"
#include "jksql.h"
#include "kgXref.h"
#include "localmem.h"
#include "hash.h"
#include "linefile.h"

struct spKgMap;
struct sqlConnection;

struct spKgMap *spKgMapNew(struct sqlConnection *conn);
/* load data from from kgXRef table to map from swissprot accs to
 * known gene ids. */

void spKgMapFree(struct spKgMap **spKgMapPtr);
/* free a spKgMap structure */

struct slName *spKgMapGet(struct spKgMap *spKgMap, char *spId);
/* Get the list of known gene ids for a swissprot acc.  If not found NULL is
 * returned and an entry is made in the table that can be used to get the
 * count of missing mappings. */

int spKgMapCountMissing(struct spKgMap *spKgMap);
/* count the number swissprot accs with no corresponding known gene */

void spKgMapListMissing(struct spKgMap *spKgMap, char *file);
/* output the swissprot accs with no corresponding known gene */

#endif
