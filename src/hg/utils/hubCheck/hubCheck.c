/* hubCheck - Check a track data hub for integrity.. */

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
  );
}

static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {"clear", OPTION_STRING},
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

char *browserMachine = NULL;
browserMachine = optionVal("clear", browserMachine) ;
if (browserMachine != NULL)
    return clearHub(argv[1], browserMachine);

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
