/* somePsls - Get some psls from database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "somePsls - Get some psls from database\n"
  "usage:\n"
  "   somePsls XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void somePsls(char *XXX)
/* somePsls - Get some psls from database. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
somePsls(argv[1]);
return 0;
}
