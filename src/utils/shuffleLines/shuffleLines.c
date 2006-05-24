/* shuffleLines - Create a version of file with lines shuffled.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

static char const rcsid[] = "$Id: shuffleLines.c,v 1.1 2006/04/30 16:05:57 kent Exp $";

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
shuffleList(&list, 1);
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
