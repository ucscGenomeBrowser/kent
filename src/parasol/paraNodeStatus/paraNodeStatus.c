/* paraNodeStatus - Check status of paraNode on a list of machines. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "paraNodeStatus - Check status of paraNode on a list of machines\n"
  "usage:\n"
  "   paraNodeStatus XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void paraNodeStatus(char *XXX)
/* paraNodeStatus - Check status of paraNode on a list of machines. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
paraNodeStatus(argv[1]);
return 0;
}
