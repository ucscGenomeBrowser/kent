/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: freen.c,v 1.1 2004/06/29 22:05:25 kent Exp $";

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen something");
}

int xenAlignAffine(char *query, int querySize, char *target, int targetSize, 
	FILE *f, boolean printExtraAtEnds);
/* Use dynamic programming to do protein/protein alignment. */


void freen(char *out)
/* Test some hair-brained thing. */
{
char *a = "aacagtattc";
char *b = "aacaggtattc";
xenAlignAffine(a, strlen(a), b, strlen(b), stdout, FALSE);
}


int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(1000000);
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
