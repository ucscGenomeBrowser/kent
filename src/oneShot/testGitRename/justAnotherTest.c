/* testGitRename - Just a little something to test renaming, merging, etc.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "upperFile.h"


void upperFile(char *input, char *output)
/* upperFile - convert file to upper case. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    touppers(line);
    fprintf(f, "%s\n", line);
    }
carefulClose(&f);
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testGitRename - Just a little something to test renaming, merging, etc.\n"
  "usage:\n"
  "   testGitRename input output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
upperFile(argv[1], argv[2]);
return 0;
}
