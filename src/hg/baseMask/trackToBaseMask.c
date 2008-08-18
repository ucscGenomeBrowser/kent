/* overlapSelect - select records based on overlap of chromosome ranges */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "sig.h"
#include "genomeRangeTree.h"
#include "options.h"

/* FIXME:
 * - would be nice to be able to specify ranges in the same manner
 *   as featureBits
 * - should keep header lines in files
 * - don't need to save if infile records if stats output
 */

static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};


void trackToBaseMask(char *db, char *track, char *obama)
/* select records based on overlap of chromosome ranges */
{
struct bed *b, *bList;
struct genomeRangeTree *tree = genomeRangeTreeNew();
struct sqlConnection *conn = hAllocOrConnect(db);
struct slName *chrom, *chromList = hAllChromNamesDb(db);
struct rbTree *rt;
/* loop over all chromosomes adding to range tree */
for (chrom = chromList ; chrom ; chrom = chrom->next)
    {
    if ( (bList = hGetBedRangeDb(db, track, chrom->name, 0, 0, NULL)) )
	{
	rt = genomeRangeTreeFindOrAddRangeTree(tree, chrom->name);
        for ( b = bList ; b ; b = b->next )
	    {
	    bedIntoRangeTree(b, rt);
	    }
        bedFreeList(&bList);
	}
    }
genomeRangeTreeWrite(tree, obama); /* write the tree out */
genomeRangeTreeFree(&tree);
slFreeList(&chromList);
sqlDisconnect(&conn);
}


void usage(char *msg)
/* usage message and abort */
{
static char *usageMsg =
#include "trackToBaseMaskUsage.msg"
    ;
errAbort("%s:  %s", msg, usageMsg);
}

/* entry */
int main(int argc, char** argv)
{
char *db, *track, *obama;
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage("wrong # args");
db = argv[1];
track = argv[2];
obama = argv[3];

trackToBaseMask(db, track, obama);
return 0;
}
