/* stringify - Convert file to C strings. */
#include "common.h"
#include "linefile.h"

static char const rcsid[] = "$Id: stringify.c,v 1.3 2003/05/06 07:41:08 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "stringify - Convert file to C strings\n"
  "usage:\n"
  "   stringify in.txt\n"
  "A stringified version of in.txt  will be printed to standard output.\n"
  );
}

void stringify(char *fileName, FILE *f)
/* stringify - Convert file to C strings. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, c;

while (lineFileNext(lf, &line, NULL))
    {
    fputc('"', f);
    while ((c = *line++) != 0)
        {
        if (c == '"' || c == '\\')
            fputc('\\', f);
        fputc(c, f);
        }
    fputc('\\', f);
    fputc('n', f);
    fputc('"', f);
    fputc('\n', f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
stringify(argv[1], stdout);
return 0;
}

