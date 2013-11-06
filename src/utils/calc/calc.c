/* calc - Little command line calculator. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "dystring.h"
#include "options.h"

bool humanReadable = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "calc - Little command line calculator\n"
  "usage:\n"
  "   calc this + that * theOther / (a + b)\n"
  "Options:\n"
  "  -h - output result as a human-readable integer numbers, with k/m/g/t suffix\n"
  );
}

static struct optionSpec options[] = {
   {"h", OPTION_BOOLEAN},
   {NULL, 0},
};

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

double result = doubleExp(dy->string);

if (!humanReadable)
    {
    printf("%s = %f\n", dy->string, result);
    return;
    }

// make human readable 
char* resQual = "";
int intRes = 0;
if (result>=1E12)
    {
    intRes = round(result/1E12);
    resQual = "t";
    }
else if (result>=1E9)
    {
    intRes = round(result/1E9);
    resQual = "g";
    }
else if (result>=1E6)
    {
    intRes = round(result/1E6);
    resQual = "m";
    }
else if (result>=1E3)
    {
    intRes = round(result/1E3);
    resQual = "k";
    }
else 
    {
    intRes = round(result);
    resQual = "";
    }

printf("%s = %d%s\n", dy->string, intRes, resQual);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
humanReadable = optionExists("h");
calc(argc-1, argv+1);
return 0;
}
