/* foo2 - Testing cvs option in newProg. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "foo2 - Testing cvs option in newProg\n"
  "usage:\n"
  "   foo2 XXX\n");
}

void foo2(char *command)
/* foo2 - Testing cvs option in newProg. */
{
printf("I don't know how to %s", command);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
foo2(argv[1]);
return 0;
}
