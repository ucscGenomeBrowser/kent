/* bedSort - Sort a BED file by chrom,chromStart. */
#include "common.h"
#include "hash.h"
#include "bed.h"

static char const rcsid[] = "$Id: bedSort.c,v 1.3 2003/06/10 17:22:45 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedSort - Sort a .bed file by chrom,chromStart\n"
  "usage:\n"
  "   bedSort in.bed out.bed\n"
  "in.bed and out.bed may be the same.");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
bedSortFile(argv[1], argv[2]);
return 0;
}
