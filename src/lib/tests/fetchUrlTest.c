/* fetchUrlTest - test some stuff in net module. */
#include "common.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "net.h"


void usage()
/* Explain usage and exit */
{
errAbort(
"fetchUrlTest - try to fetch url\n"
"usage:\n"
"   fetchUrlTest URL\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void fetchUrlTest(char *url)
/* Fetch given URL, send to stdout. */
{
struct dyString *dy = netSlurpUrl(url);
mustWrite(stdout, dy->string, dy->stringSize);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
fetchUrlTest(argv[1]);
return 0;
}

