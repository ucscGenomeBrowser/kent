#include <stdio.h>
#include <ctype.h>

#define uglyf printf

#define MAX_LINE_LENGTH 256
unsigned char line[MAX_LINE_LENGTH];
unsigned char best[MAX_LINE_LENGTH];
unsigned char nextBest[MAX_LINE_LENGTH];
int bestValue[MAX_LINE_LENGTH];
int counts[MAX_LINE_LENGTH][256];

void main(argc, argv)
int argc;
char *argv[];
{
FILE *in;
char *fileName;
char *pt;
int len;
int lineCount = 0;
int i;
int maxLen = 0;

if (argc != 2)
    {
    fprintf(stderr, "This program figures the (alligned) consensus\n");
    fprintf(stderr, "from a file.  It prints the most common and the\n");
    fprintf(stderr, "second most common letters at each position a\n");
    fprintf(stderr, "line, and a number between 0-9 showing the strength\n");
    fprintf(stderr, "of the consensus.\n\n");
    fprintf(stderr, "usage:  %s file\n", argv[0]);
    exit(-1);
    }
fileName = argv[1];
if ((in = fopen(fileName, "r")) == NULL)
    {
    fprintf(stderr, "Couldn't open %s\n", fileName);
    exit(-1);
    }
for (;;)
    {
    pt = fgets(line, sizeof(line), in);
    if (pt == NULL)
	break;
    lineCount += 1;
    len = strlen(pt);
    if (len >= sizeof(line)-1)
	{
	fprintf(stderr, "Line too long line %d of %s\n", 
		lineCount, fileName);
	fprintf(stderr, "Can only handle lines up to %d characters\n",
		sizeof(line)-1);
	exit(-1);
	}
    if (len > maxLen)
	maxLen = len;
    for (i=0; i<len; i++)
	{
	counts[i][line[i]] += 1;
	}
    }
for (i=0; i<maxLen; ++i)
    {
    int j;
    int total = 0;
    int max = 0;
    int maxIx = 0;
    int almostMax = 0;
    int almostMaxIx = 0;
    int oneCount;

    for (j=0; j<256; ++j)
	{
	oneCount = counts[i][j];
	total += oneCount;
	if (oneCount > max)
	    {
	    max = oneCount;
	    maxIx = j;
	    }
	}
    best[i] = maxIx;

    for (j=0; j<256; ++j)
	{
	if (j != maxIx)
	    {
	    oneCount = counts[i][j];
	    if (oneCount > almostMax)
		{
		almostMax = oneCount;
		almostMaxIx = j;
		}
	    }
	}
    nextBest[i] = almostMaxIx;
    if (total > 0)
	bestValue[i] = 9*max/total;
    }
for (i=0; i<maxLen; ++i)
    {
    if (best[i] != '\n')
	fputc(best[i], stdout);
    }
fputc('\n', stdout);
for (i=0; i<maxLen; ++i)
    {
    if (nextBest[i] == 0)
	fputc(' ', stdout);
    else if (nextBest[i] != '\n')
	fputc(nextBest[i], stdout);
    }
fputc('\n', stdout);
for (i=0; i<maxLen; ++i)
    {
    if (best[i] != '\n')
	fprintf(stdout, "%d", bestValue[i]);
    }
fputc('\n', stdout);
}
