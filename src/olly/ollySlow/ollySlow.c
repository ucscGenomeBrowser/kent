/* ollySlow - Slow but flexible olligo finder. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ollySlow - Slow but flexible olligo finder\n"
  "usage:\n"
  "   ollySlow XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void ollySlow(char *XXX)
/* ollySlow - Slow but flexible olligo finder. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
ollySlow(argv[1]);
return 0;
}
