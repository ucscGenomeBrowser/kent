/* freen - My Pet Freen. */
#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "portable.h"
#include "hdb.h"
#include "trackTable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "freen - My pet freen\n"
  "usage:\n"
  "   freen hgN\n");
}

void freen(char *text)
{
printf("%s = %d\n", text, intExp(text));
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2 )
    usage();
freen(argv[1]);
return 0;
}
