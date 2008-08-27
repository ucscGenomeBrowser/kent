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

void trackToBaseMask(char *db, char *track, char *obama, boolean quiet);


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
