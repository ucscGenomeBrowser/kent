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
    "   paraFetch N R URL outPath\n"
    "   where N is the number of connections to use\n"
    "         R is the number of retries\n"
    "options:\n"
    "   -newer  only download a file if it is newer than the version we already have.\n"
    );
}

static struct optionSpec options[] = {
   {"newer", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean paraFetch(int numConnections, int numRetries, char *url, char *outPath, boolean newer)
/* Fetch given URL, send to stdout. */
{
return parallelFetch(url, outPath, numConnections, numRetries, newer);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
if (!paraFetch(atoi(argv[1]), atoi(argv[2]), argv[3], argv[4], optionExists("newer")))
    exit(1);
return 0;
}

