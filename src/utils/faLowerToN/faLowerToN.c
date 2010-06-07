/* faLowerToN - Convert lower case bases to N.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: faLowerToN.c,v 1.1 2008/10/10 16:38:44 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faLowerToN - Convert lower case bases to N.\n"
  "usage:\n"
  "   faLowerToN input.fa output.fa\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void lowerToN(char *s)
/* Convert lower case letters to N's. */
{
char c;
for (;;)
    {
    c = *s;
    if (islower(c))
        *s = 'N';
    else if (c == 0)
        break;
    ++s;
    }
}

void faLowerToN(char *inName, char *outName)
/* faLowerToN - Convert lower case bases to N.. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] != '>')
       lowerToN(line);
    fprintf(f, "%s\n", line);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
faLowerToN(argv[1], argv[2]);
return 0;
}
