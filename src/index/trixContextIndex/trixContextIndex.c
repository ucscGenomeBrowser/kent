/* trixContextIndex - Index in.txt file used with ixIxx to produce a two column file with symbol name 
 * and file offset for that line.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trixContextIndex - Index in.txt file used with ixIxx to produce a two column file with symbol\n"
  "name and file offset for that line.\n"
  "usage:\n"
  "   trixContextIndex in.txt out.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void trixContextIndex(char *input, char *output)
/* trixContextIndex - Index in.txt file used with ixIxx to produce a two column file with symbol name 
 * and file offset for that line.. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *line;
while (lineFileNextReal(lf, &line))
    {
    long long pos = lineFileTell(lf);
    char *word = nextWord(&line);
    if (word == NULL)
        errAbort("Short line %d of %s", lf->lineIx, input);
     fprintf(f, "%s\t%lld\n", word, pos);
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
trixContextIndex(argv[1], argv[2]);
return 0;
}
