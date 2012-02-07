/* regBedStats - Go through bed files and calculate a bunch of statistics on them. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hmmstats.h"
#include "obscure.h"
#include "sqlNum.h"


int chromColIx = 0, startColIx=1, endColIx=2, scoreColIx=4;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regBedStats - Go through bed files and calculate a bunch of statistics on them\n"
  "usage:\n"
  "   regBedStats fileList output\n"
  "options:\n"
  "   -chromColIx=N (default %d)\n"
  "   -startColIx=N (default %d)\n"
  "   -endColIx=N (default %d)\n"
  "   -scoreColIx=N (default %d)\n"
  , chromColIx, startColIx, endColIx, scoreColIx
  );
}

static struct optionSpec options[] = {
   {"chromColIx", OPTION_INT},
   {"startColIx", OPTION_INT},
   {"endColIx", OPTION_INT},
   {"scoreColIx", OPTION_INT},
   {NULL, 0},
};

void printStats(FILE *f, struct slDouble *list)
/* Print out stats on list: ave +-std min 1/4 median 3/4 max */
{
int count = 0;
struct slDouble *el;
double sum=0, sumSquared=0;
for (el = list; el != NULL; el = el->next)
    {
    sum += el->val;
    sumSquared += el->val * el->val;
    count += 1;
    }
double ave = sum/count;
double std = calcStdFromSums(sum, sumSquared, count);
double minVal, q1, median, q3, maxVal;
slDoubleBoxWhiskerCalc(list, &minVal, &q1, &median, &q3, &maxVal);
fprintf(f, "\t%g+-%g [%g %g %g %g %g]", ave, std, minVal, q1, median, q3, maxVal);
}

void bedFileStats(char *bedFile, int colCount, FILE *f)
/* Collect stats on sizes of things in a bed file, and scores too. */
{
struct lineFile *lf = lineFileOpen(bedFile, TRUE);
struct slDouble *sizeList=NULL, *scoreList=NULL, *el;
char *row[colCount];
while (lineFileNextRow(lf, row, colCount))
    {
    int size = sqlUnsigned(row[endColIx]) - sqlUnsigned(row[startColIx]);
    el = slDoubleNew(size);
    slAddHead(&sizeList, el);
    double score = sqlDouble(row[scoreColIx]);
    el = slDoubleNew(score);
    slAddHead(&scoreList, el);
    }
fprintf(f, "%s\t%d\tsize:", bedFile, slCount(scoreList));
printStats(f, sizeList);
fprintf(f, "\tscore:");
printStats(f, scoreList);
fprintf(f, "\n");
lineFileClose(&lf);
}

void regBedStats(char *fileOfFiles, char *output)
/* regBedStats - Go through bed files and calculate a bunch of statistics on them. */
{
struct slName *in, *inList = readAllLines(fileOfFiles);
FILE *f = mustOpen(output, "w");
int colCount = max(chromColIx, startColIx);
colCount = max(colCount, endColIx);
colCount = max(colCount, scoreColIx);
colCount += 1;
for (in = inList; in != NULL; in = in->next)
    {
    bedFileStats(in->name, colCount, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
chromColIx = optionInt("chromColIx", chromColIx);
startColIx = optionInt("startColIx", startColIx);
endColIx = optionInt("endColIx", endColIx);
scoreColIx = optionInt("scoreColIx", scoreColIx);

regBedStats(argv[1], argv[2]);
return 0;
}
