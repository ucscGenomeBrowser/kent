#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "chromInfo.h"
#include "genomeRangeTree.h"

char *firstChrom(char *db)
{
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr = sqlGetResult(conn, "select * from chromInfo limit 1");
char **row;
char *chrom;
if ((row = sqlNextRow(sr)) == NULL || !row[0])
    errAbort("no chroms in database %s\n", db);
chrom = cloneString(row[0]);
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return chrom;
}

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
/* Create a baseMask obama representing the 'track' in database 'db'.
 * Outputs number of based on overlap of chromosome ranges */
{
struct bed *b, *bList;
struct genomeRangeTree *tree = genomeRangeTreeNew();
//struct slName *chrom, *chromList = hAllChromNamesDb(db);
struct chromInfo *ci, *ciList = createChromInfoList("all", db);
struct sqlConnection *conn = hAllocOrConnect(db);
struct rbTree *rt;
double totalSize = 0, treeSize = 0, nodes = 0;
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
	nodes += rt->n;
        bedFreeList(&bList);
	}
    if (!quiet && rt)
	treeSize += rangeTreeOverlapTotalSize(rt);
    }
if (!quiet)
    fprintf(stderr,"%1.0f bases in %1.0f ranges in %d chromosomes of %1.0f (%4.3f%%) bases in genome\n",
	treeSize, nodes, tree->hash->elCount, totalSize, 100.0*treeSize/totalSize);
if (obama)
    genomeRangeTreeWrite(tree, obama); /* write the tree out */
genomeRangeTreeFree(&tree);
slFreeList(&ciList);
sqlDisconnect(&conn);
}

