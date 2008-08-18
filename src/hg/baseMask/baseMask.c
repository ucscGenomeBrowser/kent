/* baseMask - process baseMasks - 'and' or 'or' two baseMasks together */

#include "common.h"
#include "genomeRangeTree.h"
#include "options.h"

/* FIXME:
 * - would be nice to be able to specify ranges in the same manner
 *   as featureBits
 * - should keep header lines in files
 * - don't need to save if infile records if stats output
 */

static struct optionSpec optionSpecs[] = {
    {"and", OPTION_BOOLEAN},
    {"or", OPTION_BOOLEAN},
    {NULL, 0}
};


void usage(char *msg)
/* usage message and abort */
{
static char *usageMsg =
#include "baseMaskUsage.msg"
    ;
errAbort("%s:  %s", msg, usageMsg);
}

/* entry */
int main(int argc, char** argv)
{
char *baseMask1, *baseMask2, *obama;
struct genomeRangeTreeFile *tf1, *tf2;
struct genomeRangeTree *t1, *t2;
optionInit(&argc, argv, optionSpecs);
boolean and = optionExists("and");
boolean or = optionExists("or");
if (argc != 4)
    usage("wrong # args");
if ((and && or) || (!and && !or))
    usage("specify one of the options: -and or -or\n");

baseMask1 = argv[1];
baseMask2 = argv[2];
obama = argv[3];

tf1 = genomeRangeTreeFileReadHeader(baseMask1);
tf2 = genomeRangeTreeFileReadHeader(baseMask2);
if (and)
    {
    errAbort("Not implemented\n");
    //genomeRangeTreeFileAnd(tf1, tf2, obama);
    }
else if (or)
    {
    genomeRangeTreeFileOr(tf1, tf2, obama);
    }
t1 = genomeRangeTreeFileFree(&tf1);
genomeRangeTreeFree(&t1);
t2 = genomeRangeTreeFileFree(&tf2);
genomeRangeTreeFree(&t2);
return 0;
}
