#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"
#include "dnautil.h"
#include "bed.h"
#include "hdb.h"
#include "binRange.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedOverlap -  Remove Overlaps from bed files - choose highest scoring bed\n"
  "usage:\n"
  "   bedOverlap infile.bed output.bed\n"
  "options:\n"
  "   -xxx \n"
  "Note: infile.bed must be sorted by chrom, chromStart"
  );
}

char **cloneRow(char **row, int size)
{
int i;
char **result;

AllocArray(result, size);
for (i = 0 ; i < size ; i++)
    {
    result[i] = cloneString(row[i]);
    }
return result;
}

void outputRow(FILE *f, char **row, int size)
{
int i;
if (row == NULL) return;
for (i = 0 ; i<size ; i++)
    fprintf(f,"%s ",row[i]);
fprintf(f,"\n");
    
}

void bedOverlap(char *aFile, FILE *f)
/* Sort a bed file (in place, overwrites old file. */
{
char *prevChrom = cloneString("chr1");
int prevStart = 0, prevEnd = 0;
bool first = TRUE;

struct lineFile *lf = lineFileOpen(aFile, TRUE);
char *row[40], **bestMatch = NULL;
int bestScore = 0, bestCount = 0;
int i, wordCount;
struct binElement *hitList = NULL, *hit;
struct binKeeper *bk;

printf("Loading Predictions from %s\n",aFile);
while ((wordCount = lineFileChop(lf, row)) != 0)
    {
    char *name = row[0];
    int start = lineFileNeedNum(lf, row, 1);
    int end = lineFileNeedNum(lf, row, 2);
    int score = lineFileNeedNum(lf, row, 4);

    if (first || (sameString(name,prevChrom) && start <= prevEnd))
        {
        if (first)
            {
            prevChrom = cloneString(name);
            prevStart = start;
            prevEnd = end;
            first = FALSE;
            }
        if (score > bestScore)
            {
            bestMatch = cloneRow(row, wordCount);
            bestScore = score;
            bestCount = wordCount;
            }
        prevEnd = max(prevEnd, end);
        }
    else
        {
        if (bestMatch != NULL)
            outputRow(f, bestMatch, bestCount);
        for (i = 0; i <bestCount ; i++)
            freeMem(bestMatch[i]);
        bestMatch = NULL;
        if (ferror(f))
            {
            perror("Writing error\n");
            errAbort(" file is truncated, sorry.");
            }
        bestScore = score;
        bestCount = wordCount;
        prevChrom = cloneString(name);
        prevStart = start;
        prevEnd = end;
        bestMatch = cloneRow(row, wordCount);
        } 
    }
    if (bestMatch != NULL)
        outputRow(f, bestMatch, bestCount);
fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
FILE *f;
if (argc != 3)
    usage();
f = mustOpen(argv[2], "w");
bedOverlap(argv[1], f);
return 0;
}
