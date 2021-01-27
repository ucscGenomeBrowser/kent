/* matrixScale - Scale a matrix by a constant number.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "matrixScale - Scale a matrix by a constant number.\n"
  "usage:\n"
  "   matrixScale in.tsv scale out.tsv\n"
  "where in and out are labeled matrices and scale is a floating point number\n"
  "options:\n"
  "   -unlabeled - do scale first row or column\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"unlabeled", OPTION_BOOLEAN},
   {NULL, 0},
};

void matrixScale(char *input, char *scaleString, char *output)
/* matrixScale - Scale a matrix by a constant number.. */
{
boolean unlabeled = optionExists("unlabeled");
double scale = sqlDouble(scaleString);
struct lineFile *lf = lineFileOpen(input, TRUE);
char *line;
FILE *f = mustOpen(output, "w");
if (!unlabeled)
    {
    lineFileNeedNext(lf, &line, NULL);
    fprintf(f, "%s\n", line);
    }
while (lineFileNext(lf, &line, NULL))
    {
    boolean first = TRUE;
    char *word;
    if (!unlabeled)
	{
	word = nextWord(&line);
	fprintf(f, "%s", word);
	first = FALSE;
	}
    while ((word = nextWord(&line)) != NULL)
        {
	if (!first)
	   fprintf(f, "\t");
        first = FALSE;
	if (word[0] == '0' && word[1] == 0)  // special case zero string */
	   fprintf(f, "0");
	else
	   {
	   double val = sqlDouble(word);
	   fprintf(f, "%g", val*scale);
	   }
	}
    fprintf(f, "\n");
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
matrixScale(argv[1], argv[2], argv[3]);
return 0;
}
