/* clusterBox - Cluster together overlapping boxes,  or technically overlapping axt's. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "clusterBox - Cluster together overlapping boxes,  or technically overlapping axt's\n"
  "usage:\n"
  "   clusterBox XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void clusterBox(char *XXX)
/* clusterBox - Cluster together overlapping boxes,  or technically overlapping axt's. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
clusterBox(argv[1]);
return 0;
}
