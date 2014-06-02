/* wordsInEachLine - Count words in each line of a file.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "wordsInEachLine - Count words in each line of a file.\n"
  "usage:\n"
  "   wordsInEachLine input output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void wordsInEachLine(char *input, char *output)
/* wordsInEachLine - Count words in each line of a file.. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    fprintf(f, "%d\n", chopByWhite(line, NULL, 0));
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
wordsInEachLine(argv[1], argv[2]);
return 0;
}
