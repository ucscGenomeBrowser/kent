/* faFilter - Filter out fa records that don't match expression. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: faFilter.c,v 1.1 2003/09/15 05:40:22 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faFilter - Filter out fa records that don't match expression\n"
  "usage:\n"
  "   faFilter pattern in.fa\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void faFilter(char *pattern, char *inName)
/* faFilter - Filter out fa records that don't match expression. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
boolean doWrite = FALSE;
char *line;

while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
        {
	doWrite =  (stringIn(pattern, line) != NULL);
	}
    if (doWrite)
	puts(line);
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
faFilter(argv[1],argv[2]);
return 0;
}
