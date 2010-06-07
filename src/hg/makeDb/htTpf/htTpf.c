/* htTpf - Make TPF table. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

static char const rcsid[] = "$Id: htTpf.c,v 1.2 2003/05/06 07:22:26 kate Exp $";

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
