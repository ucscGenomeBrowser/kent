/* hgSoftberryHom - Make table storing Softberry protein homology information. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSoftberryHom - Make table storing Softberry protein homology information\n"
  "usage:\n"
  "   hgSoftberryHom XXX\n");
}

void hgSoftberryHom(char *XXX)
/* hgSoftberryHom - Make table storing Softberry protein homology information. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
hgSoftberryHom(argv[1]);
return 0;
}
