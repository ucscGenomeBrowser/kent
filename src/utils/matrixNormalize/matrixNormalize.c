/* matrixNormalize - Normalize a matrix somehow - make it's columns or rows all sum to one or have vector length one.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "vMatrix.h"
#include "pthreadDoList.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "matrixNormalize - Normalize a matrix somehow - make it's columns or rows all sum to one or have vector length one.\n"
  "usage:\n"
  "   matrixNormalize direction how inMatrix outMatrix\n"
  "where \"direction\" is one of\n"
  "   row - normalize rows to one\n"
  "   column - normalize columns to one\n"
  "and \"how\" is one of\n"
  "   sum - sum adds to one after normalization\n"
  "   length - Euclidian length as a vector adds to one\n"
  "options:\n"
  "   -target=val - use target val instead of one for normalizing\n"
  "   -serial - disable use of pthreads to speed up via parallelization\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"target", OPTION_DOUBLE},
   {"serial", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean howIsLength(char *how)
/* Return TRUE if how is "length",  FALSE if it is "sum", and abort otherwise */
{
if (sameWord(how, "length"))
    return TRUE;
else if (sameWord(how, "sum"))
    return FALSE;
errAbort("Unrecognized \"how\" %s", how);
return FALSE;
}

void matrixNormalizeRows(char *inFile, boolean isLength, double target, char *outFile)
/* Normalize matrix one row at a time. */
{
struct vRowMatrix *m = vRowMatrixOnTsv(inFile);
FILE *f = mustOpen(outFile, "w");
int size = m->xSize;
double *row;
char *label;
while ((row = vRowMatrixNextRow(m, &label)) != NULL)
    {
    double scaleVal = target;
    double sum = 0.0;
    int i;
    if (isLength)
       {
       for (i=0; i<size; ++i)
	    {
	    double val = row[i];
            sum += val*val;
	    }
       sum = sqrt(sum);
       }
    else
       {
       for (i=0; i<size; ++i)
            sum += row[i];
       }
    if (sum != 0.0)
	scaleVal /= sum;
    for (i=0; i<size; ++i)
        row[i] *= scaleVal;
    fprintf(f, "%s", label);
    for (i=0; i<size; ++i)
	{
	double val = row[i];
	if (val == 0.0)
	   fputs("\t0", f);
	else
	    fprintf(f, "\t%g", row[i]);
	}
    fprintf(f, "\n");
    }
carefulClose(&f);
vRowMatrixFree(&m);
}

static void normCol(struct memMatrix *m, int x, double target, boolean isLength)
/* Normalize column x of matrix to sum to target according to measure in isLength */
{
/* Calculate how big we are currently */
double scaleVal = target;
double sum = 0.0;
int ySize = m->ySize;
int y;
if (isLength)
    {
    for (y=0; y<ySize; ++y)
	{
	double val = m->rows[y][x];
	sum += val*val;
	}
    sum = sqrt(sum);
    }
else
    {
    for (y=0; y<ySize; ++y)
	sum += m->rows[y][x];
    }

/* Multiply ourselves by normalizing scale factor */
if (sum != 0.0)
    {
    scaleVal /= sum;
    for (y=0; y<ySize; ++y)
	m->rows[y][x] *= scaleVal;
    }
}

struct columnJob
/* Multithreaded job that handles a column */
    {
    struct columnJob *next;
    int x;	/* Index of column withing matrix */
    boolean isLength;	/* Do sqrt(sum(squares)) type normalization */
    double target;	/* Want column to end up summing to this */
    };

void columnWorker(void *item, void *context)
/* A worker to execute a single column job */
{
struct columnJob *job = item;
struct memMatrix *m = context;
normCol(m, job->x, job->target, job->isLength);
}

void matrixNormalizeColumns(char *inFile, boolean isLength, double target, char *outFile, 
    int threads)
/* Normalize matrix one row at a time. */
{
/* Open up input file */
verboseTimeInit();
struct memMatrix *m = memMatrixFromTsv(inFile);
verboseTime(1, "Loaded %d x %d matrix from %s", m->xSize, m->ySize, inFile);

/* Go through matrix by column (y-dimension) */
int xSize = m->xSize;
int x;

if (threads > 1)
    {
    struct columnJob *jobList = NULL;
    for (x=0; x<xSize; ++x)
	{
	struct columnJob *job;
	AllocVar(job);
	job->x = x;
	job->isLength = isLength;
	job->target = target;
	slAddHead(&jobList, job);
	}
    slReverse(&jobList);
    pthreadDoList(threads, jobList, columnWorker, m);
    }
else
    {
    for (x=0; x<xSize; ++x)
	{
	normCol(m, x, target, isLength);
	}
    }
verboseTime(1, "normalized matrix");
memMatrixToTsv(m, outFile);
verboseTime(1, "Wrote %s", outFile);
}


void matrixNormalize(char *direction, char *how, char *inFile, char *outFile)
/* matrixNormalize - Normalize a matrix somehow - make it's columns or rows all sum to one or 
 * have vector length one.. */
{
boolean isLength = howIsLength(how);
double target = optionDouble("target", 1.0);
boolean serial = optionExists("serial");
int threads = (serial ? 1 : 50);
if (sameWord(direction, "row"))
    matrixNormalizeRows(inFile, isLength, target, outFile);
else if (sameWord(direction, "column"))
    matrixNormalizeColumns(inFile, isLength,  target, outFile, threads);
else
    errAbort("Unrecognized direction %s", direction);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
matrixNormalize(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
