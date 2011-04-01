/* paraFetch - fetch URL with multiple parallel connections. */
#include "common.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "net.h"

void usage()
/* Explain usage and exit */
{
errAbort(
    "paraFetch - try to fetch url with multiple connections\n"
    "usage:\n"
    "   paraFetch N R URL {outPath}\n"
    "   where N is the number of connections to use\n"
    "         R is the number of retries\n"
    "   outPath is optional. If not specified, it will attempt to parse URL to discover output filename.\n"
    "options:\n"
    "   -newer  only download a file if it is newer than the version we already have.\n"
    "   -progress  Show progress of download.\n"
    );
}

static struct optionSpec options[] = {
   {"newer", OPTION_BOOLEAN},
   {"progress", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean paraFetch(int numConnections, int numRetries, char *url, char *outPath, boolean newer, boolean progress)
/* Fetch given URL, send to stdout. */
{
return parallelFetch(url, outPath, numConnections, numRetries, newer, progress);
}

char *parseUrlForName(char *url)
/* parse the name from the full url.  Free after using */
{
char *s = url;
char *e = url+strlen(url);
char *temp = strrchr(url, '/');
if (temp)
    s = temp+1;
temp = strchr(s, '?');
if (temp)
    e = temp;
return cloneStringZ(s, e-s);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *outName = NULL;
optionInit(&argc, argv, options);
if (argc != 5 && argc != 4)
    usage();
if (argc == 4)
    outName = parseUrlForName(argv[3]);	
else
    outName = argv[4];
if (!paraFetch(atoi(argv[1]), atoi(argv[2]), argv[3], outName, optionExists("newer"), optionExists("progress")))
    exit(1);
return 0;
}

