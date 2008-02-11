/* liftAcross - convert one coordinate system to another, no overlapping items. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: liftAcross.c,v 1.1 2008/02/11 22:14:48 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "liftAcross - convert one coordinate system to another, no overlapping items\n"
  "usage:\n"
  "   liftAcross liftAcross.txt srcFile.gp dstOut.gp\n"
  "options:\n"
  "   liftAcross.txt - six column file relating src to destination\n"
  "   srcFile.gp - genePred input file to be lifted\n"
  "   dstOut.gp - genePred output file result\n"
  "The six columns in the liftAcross.txt file are:\n"
  "srcName  srcStart  srcEnd  dstName  dstStart dstStrand\n"
  " the destination regions are the same size as the source regions.\n"
  " First incarnation only works with items that are fully contained in a\n"
  " single source region.  First incarnation only lifts genePred files."
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void liftAcross(char *liftAcross, char *srcFile, char *dstOut)
/* liftAcross - convert one coordinate system to another, no overlapping items. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
liftAcross(argv[1], argv[2], argv[3]);
return 0;
}
