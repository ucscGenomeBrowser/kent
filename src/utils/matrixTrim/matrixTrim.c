/* matrixTrim - Remove some or all of a matrix.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "matrixTrim - Remove some or all of a matrix.\n"
  "usage:\n"
  "   matrixTrim command inMatrix outMatrix\n"
  "different commands:\n"
  "   labels - removes first row and first column\n"
  "   zeroRows - removes all zero rows\n"
//  "   zeroCols - removes all zero columns\n"
//  "   zeroZero - removes all zero rows and all zero columns\n"
  "options:\n"
  "   -unlabeled - matrix lacks label first row and column\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"unlabeled", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean clUnlabeled = FALSE;

void matrixTrimLabels(char *in, char *out)
/* Do copy around labels */
{
struct lineFile *lf = lineFileOpen(in, TRUE);
char *line;
FILE *f = mustOpen(out, "w");
boolean labeled = !clUnlabeled;

// Skip over label row
if (labeled)
    lineFileNeedNext(lf, &line, NULL);

while (lineFileNext(lf, &line, NULL) )
    {
    if (labeled)
	{
	line = strchr(line, '\t');
	if (line == NULL)
	    errAbort("Short line %d of %s", lf->lineIx, lf->fileName);
	line += 1;
	}
    fputs(line, f);
    fputc('\n', f);
    }
carefulClose(&f);
lineFileClose(&lf);
}

void matrixTrimZeroRows(char *in, char *out)
/* Trim rows that are all zero */
{
struct lineFile *lf = lineFileOpen(in, TRUE);
char *line;
FILE *f = mustOpen(out, "w");
boolean labeled = !clUnlabeled;

// Copy over label row
if (labeled)
    {
    lineFileNeedNext(lf, &line, NULL);
    fprintf(f, "%s\n", line);
    }

while (lineFileNext(lf, &line, NULL) )
    {
    char *label = NULL;
    if (labeled)
	label = nextTabWord(&line);
    boolean allZero = TRUE;
    if (line != NULL)
        {
	char *s = line;
	while (s[0] == '0' && s[1] == '\t')
	    s += 2;
	allZero = (s[0] == '0' && s[1] == 0);
	}
    if (!allZero)
        {
	if (label != NULL)
	     fprintf(f, "%s\t", label);
	fprintf(f, "%s\n", line);
	}
    }
carefulClose(&f);
lineFileClose(&lf);
}



void matrixTrim(char *command, char *in, char *out)
/* matrixTrim - Remove some or all of a matrix.. */
{
if (sameString(command, "labels"))
    matrixTrimLabels(in, out);
else if (sameString(command, "zeroRows"))
    matrixTrimZeroRows(in, out);
else
    errAbort("Unrecognized command %s", command);
}

int main(int argc, char *argv[])
/* Process command line and call trimming engine. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
clUnlabeled = optionExists("unlabeled");
matrixTrim(argv[1], argv[2], argv[3]);

return 0;
}
