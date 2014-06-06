/* shuffleLines - Create a version of file with lines shuffled.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "shuffleLines - Create a version of file with lines shuffled.\n"
  "usage:\n"
  "   shuffleLines input output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void shuffleLines(char *in, char *out)
/* shuffleLines - Create a version of file with lines shuffled.. */
{
struct lineFile *lf = lineFileOpen(in, TRUE);
FILE *f = mustOpen(out, "w");
struct slName *list = NULL, *el;
char *line;

while (lineFileNext(lf, &line, NULL))
    slNameAddHead(&list, line);
shuffleList(&list);
for (el = list; el != NULL; el = el->next)
    fprintf(f, "%s\n", el->name);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
srand(time(NULL));
shuffleLines(argv[1], argv[2]);
return 0;
}
