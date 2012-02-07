/* htTpf - Make TPF table. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "htTpf - Make TPF table\n"
  "usage:\n"
  "   htTpf XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void htTpf(char *XXX)
/* htTpf - Make TPF table. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
htTpf(argv[1]);
return 0;
}
