#include "common.h"

void filter(FILE *in, FILE *out)
/* Convert from tabular format to simple cDNA/associated genes on same line */
{
char lineBuf[256];
char *words[128];
int wordCount;

while (fgets(lineBuf, sizeof(lineBuf), in) != NULL)
    {
    wordCount = chopString(lineBuf, whiteSpaceChopper, words, ArraySize(words));
    }
fprintf(out, "\n");
}

int main(int argc, char *argv[])
{
FILE *in, *out;
char *inName, *outName;

if (argc != 3)
    {
    fprintf(stderr, "This program cDna gene lists to gene Cdna lists\n");
    fprintf(stderr, "Usage: invert infile outfile\n");
    exit(-1);
    }
inName = argv[1];
outName = argv[2];
if ((in = fopen(inName, "r")) == NULL)
    {
    fprintf(stderr, "Couldn't open %s, sorry\n", inName);
    exit(-2);
    }
if ((out = fopen(outName, "w")) == NULL)
    {
    fprintf(stderr, "Couldn't create %s, sorry\n", outName);
    exit(-3);
    }
filter(in, out);
return 0;
}
