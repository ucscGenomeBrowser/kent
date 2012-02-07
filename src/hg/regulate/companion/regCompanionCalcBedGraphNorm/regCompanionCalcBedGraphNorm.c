/* regCompanionCalcBedGraphNorm - Stream though bedGraphs figuring out what to multiply them by to get signal to add to 10 billion. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "regCompanionCalcBedGraphNorm - Stream though bedGraphs figuring out what to multiply them by to get signal to add to 10 billion\n"
  "usage:\n"
  "   regCompanionCalcBedGraphNorm in1.bedGraph in2.bedGraph ...\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void regCompanionCalcBedGraphNorm(int inCount, char **inFiles)
/* regCompanionCalcBedGraphNorm - Stream though bedGraphs figuring out what to multiply them by to get signal to add to 10 billion. */
{
int i;
for (i=0; i<inCount; ++i)
    {
    char *fileName = inFiles[i];
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    char *row[4];
    double sum = 0.0;
    while (lineFileRow(lf, row))
        {
	int start = sqlUnsigned(row[1]);
	int end = sqlUnsigned(row[2]);
	double score = sqlDouble(row[3]);
	int size = end - start;
	if (size <= 0)
	    errAbort("start after end line %d of %s", lf->lineIx, lf->fileName);
	sum += size*score;
	}
    lineFileClose(&lf);
    printf("%s\t%g\n", fileName, 1e10/sum);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
regCompanionCalcBedGraphNorm(argc-1, argv+1);
return 0;
}
