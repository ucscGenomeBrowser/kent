/* hgSeqSearch - CGI-script to manage fast human genome sequence searching. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSeqSearch - CGI-script to manage fast human genome sequence searching\n"
  "usage:\n"
  "   hgSeqSearch XXX\n");
}

void hgSeqSearch(char *XXX)
/* hgSeqSearch - CGI-script to manage fast human genome sequence searching. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
hgSeqSearch(argv[1]);
return 0;
}
