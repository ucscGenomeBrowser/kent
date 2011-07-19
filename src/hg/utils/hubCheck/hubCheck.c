/* hubCheck - Check a track data hub for integrity.. */

#include "common.h"
#include "options.h"
#include "dystring.h"
#include "trackHub.h"
#include "udc.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hubCheck - Check a track data hub for integrity.\n"
  "usage:\n"
  "   hubCheck http://yourHost/yourDir/hub.txt\n"
  "options:\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  "   -verbose=2            - output verbosely\n"
  );
}

static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {NULL, 0},
};

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
struct dyString *errors = newDyString(1024);

if ( trackHubCheck(argv[1], errors))
    {
    printf("Errors with hub at '%s'\n", argv[1]);
    printf("%s\n",errors->string);
    return 1;
    }
return 0;
}
