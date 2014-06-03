/* bedRandom - Create bed files that contain random regions in an assembly.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"


int fixedSize = 1000;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedRandom - Create bed files that contain random regions in an assembly.\n"
  "This is just a bed 6 file (including name and strand but no blocks)\n"
  "usage:\n"
  "   bedRandom count chrom.sizes minSize maxSize out.bed\n"
  "where:\n"
  "   count is number of random items to generate\n"
  "   chrom.sizes is a file with columns <chromosome name> <chromosome size>\n"
  "   minSize is the minimum size of a bed item\n"
  "   maxSize is the maximum size of a bed item\n"
  );
}

static struct optionSpec options[] = {
   {"size", OPTION_INT},
   {NULL, 0},
};

struct chromSize
/* Chromosome name and size. */
    {
    struct chromSize *next;
    char *name;
    int size;
    };

struct chromSize *readChromSizes(char *fileName)
/* Read two column file with chromName/size as columns and return list. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct chromSize *el, *list = NULL;
char *row[2];
while (lineFileRow(lf, row))
    {
    AllocVar(el);
    el->name = cloneString(row[0]);
    el->size = sqlUnsigned(row[1]);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

void bedRandom(int count, char *chromSizes, int minSize, int maxSize, char *outFile)
/* bedRandom - Create bed files that contain random regions in an assembly.. */
{
if (minSize != maxSize)
    errAbort("Sorry, max and min sizes must be same for now.");
int itemSize = minSize;

/* Read in chrom sizes and compute size of genome. */
struct chromSize *chrom, *chromList = readChromSizes(chromSizes);
long long genomeSize = 0;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    genomeSize += chrom->size;
long long maxStart = genomeSize - itemSize;


/* Open file and generate a bunch of random bed items. */
FILE *f = mustOpen(outFile, "w");
int itemCount = 0;
for (;;)
    {
    long long start = 2LL*rand() % maxStart; /* The 2LL is to get more than 2 gig */
    for (chrom = chromList; chrom != NULL; chrom = chrom->next)
        {
	if (start >= chrom->size)
	    start -= chrom->size;
	else
	    {
	    long long end = start + itemSize;
	    if (end <= chrom->size)
	        {
		++itemCount;
		boolean negStrand = rand() % 2;
		fprintf(f, "%s\t%lld\t%lld\trand%d\t0\t%c\n",
			chrom->name, start, end, itemCount, (negStrand ? '-' : '+'));
		}
	    break;
	    }
	}
    if (itemCount >= count)
	break;
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
bedRandom(sqlUnsigned(argv[1]), argv[2], sqlUnsigned(argv[3]), sqlUnsigned(argv[4]), argv[5]);
return 0;
}
