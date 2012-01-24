/* learningLibs - A program to help learn the kent libraries.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "learningLibs - A program to help learn the kent libraries.\n"
  "usage:\n"
  "   learningLibs XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void learningLibs(char *XXX)
/* learningLibs - A program to help learn the kent libraries.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
learningLibs(argv[1]);
return 0;
}
