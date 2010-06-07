/* colCount - count freq of char in each column.  */
#include "common.h"
#include "linefile.h"
#define MAXCOLS 10000

static char const rcsid[] = "$Id: colCount.c,v 1.2 2008/08/27 23:02:15 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "colCount - count freq of bases in each column in a table.\n"
  "usage:\n"
  "   colCount XXX\n");
}

void colCount(char *fileName)
/* colCount - find freq of char in each column. */
{
struct lineFile *lf;
static float totalA[MAXCOLS];
static float totalC[MAXCOLS];
static float totalT[MAXCOLS];
static float totalG[MAXCOLS];
static float totalOther[MAXCOLS];
static float total[MAXCOLS];
int maxCount = 0;
int i;
int lineSize;
char *line;

if (sameString(fileName, "stdin"))
    lf = lineFileStdin(TRUE);
else
    lf = lineFileOpen(fileName, TRUE);
for (i=0; i<MAXCOLS; ++i)
    {
     totalA[i] = 0;
     totalC[i] = 0;
     totalG[i] = 0;
     totalT[i] = 0;
     totalOther[i] = 0;
     total[i] = 0;
    }
while (lineFileNext(lf, &line, &lineSize) )
    {
    toUpperN(line, lineSize);
    for (i=0; i<lineSize; ++i)
        {
        switch (line[i])
            {
            case 'A':
                 totalA[i]++;
                 total[i]++;
                 break;
            case 'C':
                 totalC[i]++;
                 total[i]++;
                 break;
            case 'G':
                 totalG[i]++;
                 total[i]++;
                 break;
            case 'T':
                 totalT[i]++;
                 total[i]++;
                 break;
            default :
                 totalOther[i]++;
                 break;
            }
        }
        maxCount++;
        assert(maxCount < MAXCOLS);
    }
printf("pos  A    C    G    T\n");
for (i=0; i<lineSize; ++i)
    {
    printf("%03d %4.2f %4.2f %4.2f %4.2f %4.2f ", i+1, totalA[i]/total[i], totalC[i]/total[i], totalG[i]/total[i], totalT[i]/total[i], totalOther[i]/total[i]);
    if (totalA[i] > 0) printf("A");
    if (totalC[i] > 0) printf("C");
    if (totalG[i] > 0) printf("G");
    if (totalT[i] > 0) printf("T");
    printf("\n");
    }

printf("\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
colCount(argv[1]);
return 0;
}
