/* hubCheck - Check a track data hub for integrity.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "options.h"
#include "dystring.h"
#include "hui.h"
#include "trackHub.h"
#include "udc.h"
#include "htmlPage.h"

int cacheTime = 1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hubCheck - Check a track data hub for integrity.\n"
  "usage:\n"
  "   hubCheck http://yourHost/yourDir/hub.txt\n"
  "options:\n"
  "   -noTracks             - don't check remote files for tracks, just trackDb (faster)\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs.\n"
  "                           Will create this directory if not existing\n"
  "   -cacheTime=N - set cache refresh time in seconds, default %d\n"
  "   -verbose=2            - output verbosely\n"
  , cacheTime
  );
}

static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {"noTracks", OPTION_BOOLEAN},
   {"cacheTime", OPTION_INT},
   {NULL, 0},
};

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();

cacheTime = optionInt("cacheTime", cacheTime);
udcSetCacheTimeout(cacheTime);

// UDC cache dir: first check for hg.conf setting, then override with command line option if given.
setUdcCacheDir();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));

struct dyString *errors = newDyString(1024);

struct trackHubCheckOptions *checkOptions = NULL;
AllocVar(checkOptions);
checkOptions->checkFiles = !optionExists("noTracks");
if (trackHubCheck(argv[1], checkOptions, errors))
    {
    printf("Errors with hub at '%s'\n", argv[1]);
    printf("%s\n", errors->string);
    return 1;
    }
return 0;
}
