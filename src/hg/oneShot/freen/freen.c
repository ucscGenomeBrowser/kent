/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "chainNet.h"
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
int rowOffset;
struct sqlConnection *conn;
struct sqlResult *sr;
struct chainNet *net = chainNetLoadChrom("hg13", "mouseNet3", "chr10", NULL);
FILE *f = mustOpen(fileName, "w");
chainNetWrite(net, f);
}



int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
}
