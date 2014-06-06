/* netToBed - Convert target coverage of net to a bed file.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chainNet.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "netToBed - Convert target coverage of net to a bed file.\n"
  "usage:\n"
  "   netToBed in.net out.bed\n"
  "options:\n"
  "   -maxGap=N - break up at gaps of given size or more\n"
  "   -minFill=N - only include fill of given size of above.\n"
  );
}

int minFill = 0;
int maxGap = BIGNUM;

static struct optionSpec options[] = {
   {"maxGap", OPTION_INT},
   {"minFill", OPTION_INT},
   {NULL, 0},
};

void netToBedSimple(char *inName, char *outName)
/* netToBedSimple - Convert target coverage of net to a bed file
 * not breaking at gaps or anything.*/
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
char *row[3];
char *chrom = NULL;
int start, size;

while (lineFileRow(lf, row))
    {
    if (sameWord("net", row[0]))
	{
	freeMem(chrom);
	chrom = cloneString(row[1]);
	}
    else
        {
	start = lineFileNeedNum(lf, row, 1);
	size = lineFileNeedNum(lf, row, 2);
	fprintf(f, "%s\t%d\t%d\n", chrom, start, start+size);
	}
    }
}

void rNetToBed(struct chainNet *net, struct cnFill *fillList, 
	int maxGap, int minFill, FILE *f)
/* Recursively traverse net outputting bits to bed file. */
{
struct cnFill *fill, *gap;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    if (fill->tSize >= minFill)
        {
	int start = fill->tStart;
	for (gap = fill->children; gap != NULL; gap = gap->next)
	    {
	    if (gap->tSize >= maxGap)
	        {
		fprintf(f, "%s\t%d\t%d\t%s\n", net->name, start, 
			gap->tStart, fill->qName);
		start = gap->tStart + gap->tSize;
		}
	    rNetToBed(net, gap->children, maxGap, minFill, f);
	    }
	fprintf(f, "%s\t%d\t%d\t%s\n", net->name, start, 
		fill->tStart+fill->tSize, fill->qName);
	}
    }
}

void netToBedDetailed(char *inName, char *outName, 
	int maxGap, int minFill)
/* netToBedDetailed - Convert target coverage of net to a bed 
 * breaking up things at big gaps and excluding small
 * fills. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct chainNet *net;

while ((net = chainNetRead(lf)) != NULL)
    {
    rNetToBed(net, net->fillList, maxGap, minFill, f);
    chainNetFree(&net);
    }
}

void netToBed(char *inName, char *outName)
/* netToBed - Convert target coverage of net to a bed file.. */
{
if (optionExists("maxGap") || optionExists("minFill"))
    {
    maxGap = optionInt("maxGap", maxGap);
    minFill = optionInt("minFill", minFill);
    netToBedDetailed(inName, outName, maxGap, minFill);
    }
else
    netToBedSimple(inName, outName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
netToBed(argv[1], argv[2]);
return 0;
}
