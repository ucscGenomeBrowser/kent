/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "chainBlock.h"
#include "chainNetDbLoad.h"
#include "hdb.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen file");
}

void freen(char *fileName)
/* Print status code. */
{
struct chain *chain = chainLoadId("hg13", "mouseChain", "chr1", 2681);
FILE *f = mustOpen(fileName, "w");
chain = chainLoadIdRange("hg13", "mouseChain", "chr1", 11000, 12000, 2681);
chainWrite(chain, f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
}
