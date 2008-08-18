/* overlapSelect - select records based on overlap of chromosome ranges */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
//#include "sig.h"
#include "chromInfo.h"
#include "genomeRangeTree.h"
#include "options.h"

/* FIXME:
 * - would be nice to be able to specify ranges in the same manner
 *   as featureBits
 * - should keep header lines in files
 * - don't need to save if infile records if stats output
 */

static struct optionSpec optionSpecs[] = {
    {"quiet", OPTION_BOOLEAN},
    {NULL, 0}
};

static struct chromInfo *createChromInfoList(char *name, char *database)
/* Load up all chromosome infos. 
 * Copied verbatim from featureBits.c - maybe could be moved to library ? */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr = NULL;
char **row;
int loaded=0;
struct chromInfo *ret = NULL;
unsigned totalSize = 0;

if (sameWord(name, "all"))
    sr = sqlGetResult(conn, "select * from chromInfo");
else
    {
    char select[256];
    safef(select, ArraySize(select), "select * from chromInfo where chrom='%s'",
        name);
    sr = sqlGetResult(conn, select);
    }

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct chromInfo *el;
    struct chromInfo *ci;
    AllocVar(ci);

    el = chromInfoLoad(row);
    ci->chrom = cloneString(el->chrom);
    ci->size = el->size;
    totalSize += el->size;
    slAddHead(&ret, ci);
    ++loaded;
    }
if (loaded < 1)
    errAbort("ERROR: can not find chrom name: '%s'\n", name);
slReverse(&ret);
if (sameWord(name, "all"))
    verbose(2, "#\tloaded size info for %d chroms, total size: %u\n",
        loaded, totalSize);
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return ret;
}


void trackToBaseMask(char *db, char *track, char *obama, boolean quiet)
/* select records based on overlap of chromosome ranges */
{
struct bed *b, *bList;
struct genomeRangeTree *tree = genomeRangeTreeNew();
//struct slName *chrom, *chromList = hAllChromNamesDb(db);
struct chromInfo *ci, *ciList = createChromInfoList("all", db);
struct sqlConnection *conn = hAllocOrConnect(db);
struct rbTree *rt;
double totalSize = 0, treeSize = 0;
/* loop over all chromosomes adding to range tree */
for (ci = ciList ; ci ; ci = ci->next)
    {
    totalSize += ci->size;
    rt = NULL;
    if ( (bList = hGetBedRangeDb(db, track, ci->chrom, 0, 0, NULL)) )
	{
	rt = genomeRangeTreeFindOrAddRangeTree(tree, ci->chrom);
        for ( b = bList ; b ; b = b->next )
	    {
	    bedIntoRangeTree(b, rt);
	    }
        bedFreeList(&bList);
	}
    if (!quiet && rt)
	treeSize += rangeTreeOverlapTotalSize(rt);
    }
if (!quiet)
    fprintf(stderr,"%1.0f bases of %1.0f (%4.3f%%) in intersection\n",
	treeSize, totalSize, 100.0*treeSize/totalSize);
if (obama)
    genomeRangeTreeWrite(tree, obama); /* write the tree out */
genomeRangeTreeFree(&tree);
slFreeList(&ciList);
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
if (argc < 3 || argc > 4)
    usage("wrong # args");
db = argv[1];
track = argv[2];
obama = (argc == 3 ? NULL : argv[3]);

trackToBaseMask(db, track, obama, optionExists("quiet"));
return 0;
}
