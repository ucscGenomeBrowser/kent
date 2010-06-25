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
    "   paraFetch N URL outPath\n"
    "   where N is the number of connections to use\n"
    );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void paraFetch(int numConnections, char *url, char *outPath)
/* Fetch given URL, send to stdout. */
{
parallelFetch(url, numConnections, outPath);
}



int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
paraFetch(atoi(argv[1]), argv[2], argv[3]);
return 0;
}

