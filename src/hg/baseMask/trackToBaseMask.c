/* overlapSelect - select records based on overlap of chromosome ranges */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
//#include "sig.h"
#include "chromInfo.h"
#include "options.h"
#include "genomeRangeTree.h"
#include "commonBaseMask.h"

/* FIXME:
 * - would be nice to be able to specify ranges in the same manner
 *   as featureBits
 * - should keep header lines in files
 * - don't need to save if infile records if stats output
 */

static struct optionSpec optionSpecs[] = {
    {"chromDb", OPTION_STRING},
    {"quiet", OPTION_BOOLEAN},
    {NULL, 0}
};


void usage(char *msg)
/* usage message and abort */
{
static char *usageMsg =
#include "trackToBaseMaskUsage.msg"
    ;
errAbort("%s\n%s", msg, usageMsg);
}

/* entry */
int main(int argc, char** argv)
{
char *db, *track, *obama, *chromDb;
optionInit(&argc, argv, optionSpecs);
--argc;
++argv;
if (argc == 0)
    usage("");
if (argc < 2 || argc > 3)
    usage("wrong # args");
db = argv[0];
track = argv[1];
obama = (argc == 2 ? NULL : argv[2]);
chromDb = optionVal("chromDb",db);

trackToBaseMask(db, track, chromDb, obama, optionExists("quiet"));
return 0;
}
