/* hubCheck - Check a track data hub for integrity.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "options.h"
#include "dystring.h"
#include "hui.h"
#include "trackHub.h"
#include "udc.h"
#include "net.h"

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


void help()
/* Extended help -- these are implemented options that we are hiding to allow review time */
{
errAbort(
  "hubCheck - Check a track data hub for integrity.\n"
  "usage:\n"
  "   hubCheck http://yourHost/yourDir/hub.txt\n"
  "options:\n"
  "   -settings             - just list settings with support level\n"
  "   -checkSettings        - check trackDb settings to spec \n"
  "   -version=[v?|url]     - version to validate settings against\n"
  "                                     (defaults to version in hub.txt, or %s)\n"
  "   -extra=[file|url]     - accept settings in this file (or url)\n"
  "   -core                 - reject settings not in core set for this version\n"
  "   -noTracks             - don't check remote files for tracks, just trackDb (faster)\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs.\n"
  "                           Will create this directory if not existing\n"
  "   -cacheTime=N - set cache refresh time in seconds, default %d\n"
  "   -verbose=2            - output verbosely\n"
  , trackHubVersionDefault(), cacheTime
  );
}

static struct optionSpec options[] = {
   {"version", OPTION_STRING},
   {"core", OPTION_BOOLEAN},
   {"extra", OPTION_STRING},
   {"noTracks", OPTION_BOOLEAN},
   {"settings", OPTION_BOOLEAN},
   {"checkSettings", OPTION_BOOLEAN},
   {"udcDir", OPTION_STRING},
   {"cacheTime", OPTION_INT},
   {"help", OPTION_BOOLEAN},
   {NULL, 0},
};

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (optionExists("help"))
    help();

if (argc != 2 && !optionExists("settings"))
    usage();

struct trackHubCheckOptions *checkOptions = NULL;
AllocVar(checkOptions);

checkOptions->checkFiles = !optionExists("noTracks");
checkOptions->checkSettings = optionExists("checkSettings");
checkOptions->strict = optionExists("core");

char *version = NULL;
if (optionExists("version"))
    version = optionVal("version", NULL);
checkOptions->version = version;

char *extraFile = optionVal("extra", NULL);
if (extraFile != NULL)
    {
    verbose(2, "Accepting extra settings in '%s'\n", extraFile);
    checkOptions->extraFile = extraFile;
    checkOptions->extra = hashNew(0);
    struct lineFile *lf = NULL;
    if (startsWith("http", extraFile))
        {
        struct dyString *ds = netSlurpUrl(extraFile);
        char *s = dyStringCannibalize(&ds);
        lf = lineFileOnString(extraFile, TRUE, s);
        }
    else
        {
        lf = lineFileOpen(extraFile, TRUE);
        }
    char *line;
    while (lineFileNextReal(lf, &line))
        {
        hashAdd(checkOptions->extra, line, NULL);
        }
    lineFileClose(&lf);
    verbose(3, "Found %d extra settings\n", hashNumEntries(checkOptions->extra));
    }

cacheTime = optionInt("cacheTime", cacheTime);
udcSetCacheTimeout(cacheTime);
// UDC cache dir: first check for hg.conf setting, then override with command line option if given.
setUdcCacheDir();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));

struct dyString *errors = newDyString(1024);

if (optionExists("settings"))
    {
    struct trackHubSetting *settings = trackHubSettingsForVersion(version);
    struct trackHubSetting *setting = NULL;
    for (setting = settings; setting != NULL; setting = setting->next)
        {
        printf("%s\t%s\n", setting->name, setting->level);
        }
    return 0;
    }

if (trackHubCheck(argv[1], checkOptions, errors))
    {
    // uniquify and count errors
    struct slName *errs = slNameListFromString(errors->string, '\n');
    slUniqify(&errs, slNameCmp, slNameFree);
    printf("%d errors:\n", slCount(errs));
    printf("%s\n", slNameListToString(errs, '\n'));
    return 1;
    }
return 0;
}
