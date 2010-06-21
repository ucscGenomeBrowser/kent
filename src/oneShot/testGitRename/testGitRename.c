/* testGitRename - Just a little something to test renaming, merging, etc.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testGitRename - Just a little something to test renaming, merging, etc.\n"
  "usage:\n"
  "   testGitRename XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void testGitRename(char *XXX)
/* testGitRename - Just a little something to test renaming, merging, etc.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
testGitRename(argv[1]);
return 0;
}
