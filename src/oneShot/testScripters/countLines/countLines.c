/* countLines - Count lines in files. */
#include "common.h"
#include "linefile.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "countLines - Count lines in files\n"
  "usage:\n"
  "   countLines file[s]\n"
  );
}

void countFile(char *file)
/* Count lines in one file. */
{
struct lineFile *lf = lineFileOpen(file, TRUE);
char *line;
long count = 0;
while (lineFileNext(lf, &line, NULL))
    ++count;
lineFileClose(&lf);
printf("total: %ld\n", count);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
countFile(argv[1]);
return 0;
}
