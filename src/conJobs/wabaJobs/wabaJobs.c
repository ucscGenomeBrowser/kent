/* wabaJobs - Create a set of condor jobs to run WABA. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "wabaJobs - Create a set of condor jobs to run WABA\n"
  "usage:\n"
  "   wabaJobs XXX\n");
}

void wabaJobs(char *XXX)
/* wabaJobs - Create a set of condor jobs to run WABA. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
wabaJobs(argv[1]);
return 0;
}
