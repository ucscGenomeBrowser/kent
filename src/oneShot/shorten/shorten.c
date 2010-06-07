#include "common.h"

int leadingTabs(char *s)
/* Return number of leading tabs in s. */
{
int count = 0;
while (*s++ == '\t') 
    ++count;
return count;
}
	
void filter(FILE *in, FILE *out)
/* Convert from tabular format to simple cDNA/associated genes on same line */
{
char lineBuf[256];
char *words[64];
int wordCount;
int tabs;
boolean firstLine = TRUE;
boolean inGeneList = FALSE;

while (fgets(lineBuf, sizeof(lineBuf), in) != NULL)
    {
    tabs = leadingTabs(lineBuf);
    wordCount = chopString(lineBuf, whiteSpaceChopper, words, ArraySize(words));
    if (tabs == 0 && wordCount >= 4 && strcmp(words[1], "Visible") == 0)
	{
	if (!firstLine)
	    fprintf(out, "\n");
	firstLine = FALSE;
	fprintf(out, "%s %s", words[0], words[3]);
	inGeneList = TRUE;
	}
    else if (inGeneList && tabs == 2)
	{
	fprintf(out, " %s", words[0]);
	}
    else
	{
	inGeneList = FALSE;
	}
    }
fprintf(out, "\n");
}

int main(int argc, char *argv[])
{
FILE *in, *out;
char *inName, *outName;

if (argc != 3)
    {
    fprintf(stderr, "This program converts ace tabular output to something shorter\n");
    fprintf(stderr, "Usage: shortin infile outfile\n");
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
