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

int main(int argc, char *argv[])
/* Process command line. */
{
uglyAbort(". is %d, 45 is %c", '.', 45);
if (argc != 2 )
    usage();
return 0;
}
