/* paraTestJob - A good test job to run on Parasol.  Can be configured to take a long time or crash. */
#include "paraCommon.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

char *version = PARA_VERSION;   /* Version number. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "paraTestJob - version %s\n"
  "A good test job to run on Parasol.  Can be configured to take a long time or crash.\n"
  "usage:\n"
  "   paraTestJob count\n"
  "Run a relatively time consuming algorithm count times.\n"
  "This algorithm takes about 1/10 per second each time.\n"
  "options:\n"
  "   -crash  Try to write to NULL when done.\n"
  "   -err  Return -1 error code when done.\n"
  "   -output=file  Make some output in file as well.\n"
  "   -heavy=n  Make output heavy: n extra lumberjack lines.\n"
  "   -input=file  Make it read in a file too.\n"
  "   -sleep=n  Sleep for N seconds.\n"
  , version
  );
}

double cumErr = 0.0;

void noteResults(double a, double b)
/* Don't really do anything. */
{
double diff = a - b;
if (diff < 0) diff = -diff;
cumErr += diff;
}

void compute()
/* Do a relatively time consuming calculation.
 * This should last about 1/10 second. */
{
int i;
double res;

for (i=0; i<1060000; ++i)
    {
    res = sqrt(i);
    noteResults(i, res*res);
    }
}

int countLines(char *fileName)
/* Count up lines in file. */
{
struct lineFile *lf = lineFileOpen(fileName, FALSE);
int lineCount = 0, charCount = 0;
int sizeOne;
char *line;
while (lineFileNext(lf, &line, &sizeOne))
    {
    ++lineCount;
    charCount += sizeOne;
    }
lineFileClose(&lf);
printf("%s has %d lines (%d chars)\n", fileName, lineCount, charCount);
return lineCount;
}

void paraTestJob(char *countString)
/* paraTestJob - A good test job to run on Parasol.  Can be configured to take a long time or crash. */
{
int i, count = atoi(countString);
char *outName = optionVal("output", NULL);
int heavy = optionInt("heavy", 0);
FILE *f = NULL;
if (optionExists("input"))
    countLines(optionVal("input", NULL));
if (outName != NULL)
    f = mustOpen(outName, "w");
for (i=0; i<count; ++i)
    {
    compute();
    if (f != NULL)
        {
	int j;
	fprintf(f, "Computation number %d of %d.  Error so far is %f\n",
		i+1, count, cumErr);
	for (j=0; j<heavy; ++j)
	    {
	    fprintf(f, "  I'm a lumberjack and I'm ok.  I work all night and I sleep all day\n");
	    }
	}
    }
printf("Cumulative error %f\n", cumErr);
if (optionExists("sleep"))
    sleep(optionInt("sleep", 1));
if (optionExists("err"))
    errAbort("Aborting with error");
if (optionExists("crash"))
    {
    char *s = NULL;
    s[0] = 0;
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
paraTestJob(argv[1]);
return 0;
}
