/* wabToSt - Convert WABA output to something Intronerator understands better. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "wabToSt - Convert WABA output to something Intronerator understands better\n"
  "usage:\n"
  "   wabToSt XXX\n");
}

void wabToSt(char *XXX)
/* wabToSt - Convert WABA output to something Intronerator understands better. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
wabToSt(argv[1]);
return 0;
}
