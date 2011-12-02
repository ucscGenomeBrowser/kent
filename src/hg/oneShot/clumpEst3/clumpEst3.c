/* clumpEst3 - Clump together 3' ESTs. */
#include "common.h"
#include "linefile.h"
#include "est3.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "clumpEst3 - Clump together 3' ESTs\n"
  "usage:\n"
  "   clumpEst3 in out\n");
}

void writeEst3(FILE *f, struct est3 *e3)
/* Write out non-numm est3's. */
{
if (e3 != NULL)
    est3TabOut(e3, f);
}

void clumpEst3(char *inName, char *outName)
/* clumpEst3 - Clump together 3' ESTs. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
struct est3 *cum = NULL, *cur;
FILE *f = mustOpen(outName, "w");
char *words[8];
int wordCount;

while ((wordCount = lineFileChop(lf, words)) > 0)
    {
    cur = est3Load(words);
    if (cum == NULL || cur->strand[0] != cum->strand[0] 
       || cur->chromStart - cum->chromEnd > 2000
       || !sameString(cum->chrom, cur->chrom) )
       {
       writeEst3(f, cum);
       est3Free(&cum);
       cum = cur;
       }
    else
       {
       if (cur->chromStart - cum->chromEnd < 100 || cur->estCount > 1)
	   {
	   cum->chromEnd = cur->chromEnd;
	   cum->estCount += cur->estCount;
	   }
       est3Free(&cur);
       }
    }
writeEst3(f, cum);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
clumpEst3(argv[1], argv[2]);
return 0;
}
