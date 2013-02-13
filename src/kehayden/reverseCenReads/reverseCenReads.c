/* reverseCenReads - Reverse order of monomers in semi-parsed centromeric reads.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "reverseCenReads - Reverse order of monomers in semi-parsed centromeric reads.\n"
  "usage:\n"
  "   reverseCenReads input.txt output.txt\n"
  "Assumes first word in each line is readID, and rest of words are monomers.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void reverseCenReads(char *input, char *output)
/* reverseCenReads - Reverse order of monomers in semi-parsed centromeric reads.. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *line;
int maxWords = 16;
char *words[maxWords];
while (lineFileNext(lf, &line, NULL))
    {
    int wordCount = chopByWhite(line, words, maxWords);
    if (wordCount >= maxWords)
        errAbort("Too many words (%d) line %d of %s", wordCount, lf->lineIx, lf->fileName);
    if (wordCount < 2)
        errAbort("Need at least 2 words, got %d line %d of %s", 
	    wordCount, lf->lineIx, lf->fileName);
    fprintf(f, "%s", words[0]);	/* First word - readId - not reversed. */
    int i;
    for (i=wordCount-1; i>1; --i)
        fprintf(f, "\t%s", words[i]);
    fprintf(f, "\n");
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
reverseCenReads(argv[1], argv[2]);
return 0;
}
