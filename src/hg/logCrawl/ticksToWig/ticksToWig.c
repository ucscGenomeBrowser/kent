/* ticksToWig - Convert tab-delimited file of Unix time ticks, and possibly also numerical values to wig file(s).. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "apacheLog.h"


int tickCol = 0;
int valCol = 1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ticksToWig - Convert tab-delimited file of Unix time ticks, and possibly also numerical values to wig file(s).\n"
  "usage:\n"
  "   ticksToWig in.tab density.wig [averageVal.wig]\n"
  "options:\n"
  "   -tickCol=N - Column for ticks, defaults to 0\n"
  "   -valCol=N -  Column for value, defaults to 1\n"
  "   -startTime=day/Mon/Year:HR:MN:SC\n"
  "   -startTick=tick - start time as a unix tick.\n"
  "The startTime and startTick values set where the zero time should be.  Otherwise it defaults\n"
  "to the tick in the first line of in.tab.\n"
  );
}

static struct optionSpec options[] = {
   {"tickCol", OPTION_INT},
   {"valCol", OPTION_INT},
   {"startTime", OPTION_STRING},
   {"startTick", OPTION_INT},
   {NULL, 0},
};

void printVarStepHead(FILE *f)
/* Print variable step header. */
{
fprintf(f, "variableStep chrom=chr1 span=1\n");
}

void ticksToWig(int startTick, char *inTable, char *outDensity, char *outAverage)
/* ticksToWig - Convert tab-delimited file of Unix time ticks, and possibly also 
 * numerical values to wig file(s).. */
{
struct lineFile *lf = lineFileOpen(inTable, TRUE);
FILE *densityFile = mustOpen(outDensity, "w");
printVarStepHead(densityFile);
FILE *averageFile = NULL;
if (outAverage != NULL)
    {
    averageFile = mustOpen(outAverage, "w");
    printVarStepHead(averageFile);
    }
int colsToParse = 1 + max(tickCol, valCol);
char *row[colsToParse];

time_t curTick = 0;
int sameTickCount = 0;
double tickTotal = 0;
double val = 0;
time_t tick;
while (lineFileNextRow(lf, row, colsToParse))
    {
    tick = lineFileNeedNum(lf, row, tickCol);
    if (averageFile != NULL)
       val = lineFileNeedDouble(lf, row, valCol);
    if (curTick != tick)
        {
	if (curTick > tick)
	    errAbort("Input isn't sorted - %ld > %ld line %d of %s\n", 
	    	(long)curTick, (long)tick, lf->lineIx, lf->fileName);
	if (startTick == 0)
	    startTick = tick;
        if (sameTickCount > 0)
	    {
	    fprintf(densityFile, "%ld\t%d\n", curTick - startTick + 1, sameTickCount);
	    time_t i;
	    for (i=curTick+1; i<tick; ++i)
		{
		fprintf(densityFile, "%ld\t%d\n", i - startTick + 1, 0);
		}
	   if (averageFile != NULL)
	       {
	       fprintf(averageFile, "%ld\t%f\n", 
	       		(long)curTick - startTick + 1, tickTotal/sameTickCount);
	       tickTotal = 0;
	       }
	    sameTickCount = 0;
	    }
        curTick = tick;
	}
    tickTotal += val;
    sameTickCount += 1;
    }
if (sameTickCount > 0)
   {
   fprintf(densityFile, "%ld\t%d\n", curTick - startTick + 1, sameTickCount);
   if (averageFile != NULL)
       fprintf(averageFile, "%ld\t%f\n", 
       		(long)curTick - startTick + 1, tickTotal/sameTickCount);
   }
carefulClose(&densityFile);
carefulClose(&averageFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3 && argc != 4)
    usage();
time_t startTick = 0;
char *startTime = optionVal("startTime", NULL);
if (startTime != NULL)
    {
    startTick = apacheAccessLogTimeToTick(startTime);
    }
else if (optionExists("startTick"))
    {
    startTick = optionInt("startTick", 0);
    }
tickCol = optionInt("tickCol", tickCol);
valCol = optionInt("valCol", valCol);
ticksToWig(startTick, argv[1], argv[2], (argc == 4 ? argv[3] : NULL));
return 0;
}
