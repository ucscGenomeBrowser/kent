/* cachedMask - cache and then process baseMasks - 'and' or 'or' two baseMasks together */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "common.h"
#include "hdb.h"
#include "options.h"
#include "jksql.h"
#include "genomeRangeTreeFile.h"
#include "baseMaskCommon.h"

/* FIXME:
 * - would be nice to be able to specify ranges in the same manner
 *   as featureBits
 * - should keep header lines in files
 * - don't need to save if infile records if stats output
 */

static struct optionSpec optionSpecs[] = {
    {"and", OPTION_BOOLEAN},
    {"or", OPTION_BOOLEAN},
    {"quiet", OPTION_BOOLEAN},
    {"saveMem", OPTION_BOOLEAN},
    {"orDirectToFile", OPTION_BOOLEAN},
    {"logTimes", OPTION_BOOLEAN},
    {NULL, 0}
};


void usage(char *msg)
/* usage message and abort */
{
static char *usageMsg =
#include "cachedMaskUsage.msg" 
    ;
errAbort("%s\n%s", msg, usageMsg);
}

/* entry */
int main(int argc, char** argv)
{
char *cacheDir, *chromDb, *track1, *track2, *db1, *table1, *db2, *table2, *baseMask1, *baseMask2, *obama;
struct genomeRangeTreeFile *tf1, *tf2;
struct genomeRangeTree *t1, *t2;
unsigned size = 0;
int nodes, numChroms;
optionInit(&argc, argv, optionSpecs);
boolean and = optionExists("and");
boolean or = optionExists("or");
boolean quiet = optionExists("quiet");
boolean saveMem = optionExists("saveMem");
boolean logTimes = optionExists("logTimes");
boolean orDirectToFile = optionExists("orDirectToFile");
--argc;
++argv;
if (argc == 0)
    usage("");
if (argc < 3 || argc > 5)
    usage("wrong # args\n");
if (argc == 3 && (and || or))
    usage("specify second file for options: -and or -or\n");
if (argc >= 4 && ((and && or) || (!and && !or)))
    usage("specify only one of the options: -and or -or\n");

cacheDir = argv[0];
chromDb = argv[1];
track1 = argv[2];
track2 = (argc > 3 ? argv[3] : NULL);
obama = (argc > 4 ? argv[4] : NULL);

if (!strlen(cacheDir))
    errAbort("specify cache directory\n");
if (track2 == NULL)
    {
    /* cache the track if it is not there already */
    splitDbTable(chromDb, track1, &db1, &table1);
    baseMask1 = baseMaskCacheTrack(cacheDir, chromDb, db1, table1, TRUE, logTimes);
    if (!quiet)
	{
	genomeRangeTreeFileStats(baseMask1, &numChroms, &nodes, &size);
	fprintf(stderr, "%d bases in %d ranges in %d chroms in baseMask\n", size, nodes, numChroms);
	}
    }
else
    {
    /* cache the tracks if they are not there already */
    splitDbTable(chromDb, track1, &db1, &table1);
    splitDbTable(chromDb, track2, &db2, &table2);
    baseMask1 = baseMaskCacheTrack(cacheDir, chromDb, db1, table1, TRUE, logTimes);
    baseMask2 = baseMaskCacheTrack(cacheDir, chromDb, db2, table2, TRUE, logTimes);
    tf1 = genomeRangeTreeFileReadHeader(baseMask1);
    tf2 = genomeRangeTreeFileReadHeader(baseMask2);
    if (and)
	{
	genomeRangeTreeFileIntersectionDetailed(tf1, tf2, obama, &numChroms, &nodes, 
	    (quiet ? NULL : &size), saveMem);
	if (!quiet)
	    fprintf(stderr, "%d bases in %d ranges in %d chroms in intersection\n", size, nodes, numChroms);
	}
    else if (or)
	{
	genomeRangeTreeFileUnionDetailed(tf1, tf2, obama, &numChroms, &nodes, 
	    (quiet ? NULL : &size), saveMem, orDirectToFile);
	if (!quiet)
	    fprintf(stderr, "%d bases in %d ranges in %d chroms in union\n", size, nodes, numChroms);
	}
    t1 = genomeRangeTreeFileFree(&tf1);
    genomeRangeTreeFree(&t1);
    t2 = genomeRangeTreeFileFree(&tf2);
    genomeRangeTreeFree(&t2);
    }
return 0;
}

