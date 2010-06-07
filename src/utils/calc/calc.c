/* calc - Little command line calculator. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "dystring.h"

static char const rcsid[] = "$Id: calc.c,v 1.4 2003/05/06 07:41:04 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "calc - Little command line calculator\n"
  "usage:\n"
  "   calc this + that * theOther / (a + b)\n"
  );
}

void calc(int wordCount, char *words[])
/* calc - Little command line calculator. */
{
struct dyString *dy = newDyString(128);
int i;

for (i=0; i<wordCount; ++i)
    {
    if (i != 0)
        dyStringAppend(dy, " ");
    dyStringAppend(dy, words[i]);
    }
printf("%s = %f\n", dy->string, doubleExp(dy->string));
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 2)
    usage();
calc(argc-1, argv+1);
return 0;
}
