/* liftAcross - convert one coordinate system to another, no overlapping items. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: liftAcross.c,v 1.2 2008/02/11 23:06:10 hiram Exp $";

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

struct liftSpec
/* define one region lift */
    {
    struct liftSpec *next;
    int start;
    int end;
    char *dstName;
    int dstStart;
    char strand;
    };

struct hash *readLift(char *liftAcross)
/* read in liftAcross file, create hash of srcName as hash key,
 *	hash elements are simple lists of coordinate relationships
 */
{
char *row[6];
struct hash *result = newHash(8);
struct lineFile *lf = lineFileOpen(liftAcross, TRUE);
while (lineFileNextRow(lf, row, ArraySize(row)))
    {
    struct hashEl *hel = hashStore(result, row[0]);
    struct liftSpec *liftSpec;
    AllocVar(liftSpec);
    liftSpec->start = sqlUnsigned(row[1]);
    liftSpec->end = sqlUnsigned(row[2]);
    liftSpec->dstName = cloneString(row[3]);
    liftSpec->dstStart = sqlUnsigned(row[4]);
    liftSpec->strand = '+';
    if ('-' == *row[5]) liftSpec->strand = '-';
    slAddHead(&(hel->val), liftSpec);
    }
return result;
}

void liftAcross(char *liftAcross, char *srcFile, char *dstOut)
/* liftAcross - convert one coordinate system to another, no overlapping items. */
{
struct hash *lftHash = readLift(liftAcross);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
liftAcross(argv[1], argv[2], argv[3]);
return 0;
}
