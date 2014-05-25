/* hubCheck - Check a track data hub for integrity.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "options.h"
#include "dystring.h"
#include "trackHub.h"
#include "udc.h"
#include "htmlPage.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hubCheck - Check a track data hub for integrity.\n"
  "usage:\n"
  "   hubCheck http://yourHost/yourDir/hub.txt\n"
  "options:\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs.\n"
  "                           Will create this directory if not existing\n"
  "   -verbose=2            - output verbosely\n"
  "   -clear=browserMachine - clear hub status, no checking\n"
  "   -searchFile=trixInput - output search terms into trixInput file\n"
  "   -noTracks             - don't check each track, just trackDb\n"
  );
}

static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {"clear", OPTION_STRING},
   {"searchFile", OPTION_STRING},
   {"noTracks", OPTION_BOOLEAN},
   {NULL, 0},
};

static int clearHub(char *hubUrl, char *browserMachine)
/* clear hub status */
{
char buffer[4096];

safef(buffer, sizeof buffer, 
    "http://%s/cgi-bin/hgHubConnect?hgHub_do_clear=on&hubUrl=%s\n",
    browserMachine, hubUrl);

struct htmlPage *page = htmlPageGet(buffer);

if (page == NULL)  // libraries will have put out error message 
    return 1;

if (page->status->status != 200)
    {
    printf("can not reach %s\n", browserMachine);
    return 1;
    }

// now we want to put out the string that hgHubConnect will
// output if there was an error.  
char *error = strstr(page->htmlText, "<!-- HGERROR-START -->");
if (error != NULL)
    {
    char *end = strstr(page->htmlText, "<!-- HGERROR-END -->");
    
    if (end == NULL)
	errAbort("found start error but not end error");

    *end = 0;

    char *start = strchr(error, '\n');
    if (start == NULL)
	errAbort("found HGERROR, but no following newline");
    start++;

    printf("%s\n", start);

    return 1;
    }

return 0;
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();

udcSetCacheTimeout(1);
char *browserMachine = NULL;
browserMachine = optionVal("clear", browserMachine) ;
if (browserMachine != NULL)
    return clearHub(argv[1], browserMachine);

boolean checkTracks = !optionExists("noTracks");

udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
struct dyString *errors = newDyString(1024);

FILE *searchFp = NULL;
char *searchFile = NULL;
searchFile = optionVal("searchFile", searchFile) ;
if (searchFile != NULL)
    {
    if ((searchFp = fopen(searchFile, "a")) == NULL)
	errAbort("cannot open search file %s\n", searchFile);
    }

if ( trackHubCheck(argv[1], errors, checkTracks, searchFp))
    {
    printf("Errors with hub at '%s'\n", argv[1]);
    printf("%s\n",errors->string);
    return 1;
    }
return 0;
}
