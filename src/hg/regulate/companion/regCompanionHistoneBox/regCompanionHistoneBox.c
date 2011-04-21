/* regCompanionHistoneBox - Run a filter that looks for something peaky surrounded by something 
 * squishy.  Good for looking at DNAse and txn factor ChIP peaks surrounded by histone marks.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "verbose.h"
#include "basicBed.h"
#include "sqlNum.h"
#include "bits.h"
#include "bigWig.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

double threshold = 10;
int boxWidth = 1200;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regCompanionHistoneBox - Run a filter that looks for something peaky surrounded by something\n"
  "squishy.  Good for looking at DNAse and txn factor ChIP peaks surrounded by histone marks.\n"
  "usage:\n"
  "   regCompanionHistoneBox peaky.bed squishy.bigWig output.bed\n"
  "where:\n"
  "  peaky.bed Only the first four fields of bed are used, but the rest are copied to output.\n"
  "  squishy.bigWig The program spends most of it's time streaming through this file.\n"
  "  output.bed The subset of peaky.bed that passes the box filter\n"
  "options:\n"
  "   -boxWidth=N - default %d. Signal is averaged in box this big around center of peaky items\n"
  "   -threshold=N.N - default %g. Average signal must be above threshold to be output\n"
  "   -peakToSig=output.tab - Output a table that has the name field of peaky.bed with the\n"
  "                           coverage and average info from squishy for the region.\n"
  , boxWidth, threshold
  );
}

static struct optionSpec options[] = {
   {"threshold", OPTION_DOUBLE},
   {"boxWidth", OPTION_INT},
   {NULL, 0},
};

struct bed3 *bed3Load(char **row)
/* Convert a row of characters to a 3-fielded bed. */
{
struct bed3 *bed;
AllocVar(bed);
bed->chrom = cloneString(row[0]);
bed->chromStart = sqlUnsigned(row[1]);
bed->chromEnd = sqlUnsigned(row[2]);
return bed;
}

struct bed3Line
/* A line from a bed file with the first few fields parsed out. */
    {
    struct bed3Line *next;
    struct bed3 *bed;	/* Allocated here */
    char *line;		/* Allocated here */
    };

int bed3LineCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,start with chroms in classy order. */
{
const struct bed3Line *a = *((struct bed3Line **)va);
const struct bed3Line *b = *((struct bed3Line **)vb);
int dif;
dif = cmpStringsWithEmbeddedNumbers(a->bed->chrom, b->bed->chrom);
if (dif == 0)
    dif = a->bed->chromStart - b->bed->chromStart;
return dif;
}

struct bed3Line *bed3LineNextChrom(struct bed3Line *list)
/* Return first item in list that is on a different chromosome
 * than the head of the list,  or NULL at end of list. */
{
if (list == NULL)
    return NULL;
char *firstChrom = list->bed->chrom;
struct bed3Line *el;
for (el = list->next; el != NULL; el = el->next)
    if (differentString(el->bed->chrom, firstChrom))
        break;
return el;
}

struct bed3Line *bed3LineReadAll(char *fileName)
/* Read a bed 4+ file and turn it into bed3Line list. */
{
struct bed3Line *list = NULL, *el;
char *row[4];
char *line;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while (lineFileNextReal(lf, &line))
    {
    AllocVar(el);
    el->line = cloneString(line);
    int wordCount = chopLine(line, row);
    lineFileExpectAtLeast(lf, 3, wordCount);
    el->bed = bed3Load(row);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

void regCompanionHistoneBox(char *peakyFile, char *squishyFile, char *outBedFile)
/* regCompanionHistoneBox - Run a filter that looks for something peaky surrounded by 
 * something squishy.  Good for looking at DNAse and txn factor ChIP peaks surrounded by 
 * histone marks.. */
{
struct bed3Line *peaky, *peakyList = bed3LineReadAll(peakyFile);
slSort(&peakyList, bed3LineCmp);

struct bbiFile *squishy = bigWigFileOpen(squishyFile);
FILE *f = mustOpen(outBedFile, "w");
struct bigWigValsOnChrom *chromVals = bigWigValsOnChromNew();
struct bed3Line *chromStart, *chromEnd;
verbose(1, "processing chromosomes");
for (chromStart = peakyList; chromStart != NULL; chromStart = chromEnd)
    {
    chromEnd = bed3LineNextChrom(chromStart);
    char *chrom = chromStart->bed->chrom;
    if (bigWigValsOnChromFetchData(chromVals, chrom, squishy))
        {
	double *valBuf = chromVals->valBuf;
	Bits *covBuf = chromVals->covBuf;
	for (peaky = chromStart; peaky != chromEnd; peaky = peaky->next)
	    {
	    struct bed3 *bed = peaky->bed;
	    int peak = (bed->chromStart + bed->chromEnd)/2;
	    int boxStart = peak - boxWidth/2;
	    int boxEnd = boxStart + boxWidth;
	    double n = bitCountRange(covBuf, boxStart, boxWidth);
	    if (n > 0)
	        {
		double sum = 0;
		int i;
		for (i=boxStart; i<boxEnd; ++i)
		    sum += valBuf[i];
		double mean = sum/n;
		if (mean >= threshold)
		    fprintf(f, "%s\n", peaky->line);
		}
	    }
	}
    verboseDot();
    }
verbose(1, "\n");
bbiFileClose(&squishy);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
threshold = optionInt("threshold", threshold);
boxWidth = optionInt("boxWidth", boxWidth);
regCompanionHistoneBox(argv[1], argv[2], argv[3]);
return 0;
}
