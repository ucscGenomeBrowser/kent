/* findMotif - find specified motif in nib files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "findMotif - find specified motif in nib files\n"
  "usage:\n"
  "   findMotif XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void findMotif(char *XXX)
/* findMotif - find specified motif in nib files. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
findMotif(argv[1]);
return 0;
}
