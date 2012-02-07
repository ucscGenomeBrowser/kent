/* upper - strip numbers, spaces, and punctuation turn to upper case. */
#include "common.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "upper - strip numbers, spaces, and punctuation turn to upper case\n"
  "usage:\n"
  "   upper in out\n");
}

void upper(char *inName, char *outName)
/* upper - strip numbers, spaces, and punctuation turn to upper case. */
{
FILE *in = mustOpen(inName, "r");
FILE *out = mustOpen(outName, "w");
int c;

while ((c = fgetc(in)) != EOF)
    {
    if (isalpha(c))
        putc(toupper(c), out);
    else if (c == '\n')
        putc(c, out);
    }
fclose(in);
fclose(out);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
upper(argv[1], argv[2]);
return 0;
}
