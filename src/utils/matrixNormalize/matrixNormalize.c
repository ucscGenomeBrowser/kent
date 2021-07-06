/* matrixNormalize - Normalize a matrix somehow - make it's columns or rows all sum to one or have vector length one.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "vMatrix.h"

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
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"target", OPTION_DOUBLE},
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
	fprintf(f, "\t%g", row[i]);
    fprintf(f, "\n");
    }
carefulClose(&f);
vRowMatrixFree(&m);
}

void memMatrixToTsv(struct memMatrix *m, char *fileName)
/* Output memory matrix to tab-sep file with labels */
{
FILE *f = mustOpen(fileName, "w");

/* Print label row */
fprintf(f, "%s", m->centerLabel);
int x, xSize = m->xSize;
for (x=0; x<xSize; ++x)
     fprintf(f, "\t%s", m->xLabels[x]);
fprintf(f, "\n");

/* Print rest */
/* Now output going through row by row */
int y, ySize = m->ySize;
for (y=0; y<ySize; ++y)
   {
   fprintf(f, "%s", m->yLabels[y]);
   double *row = m->rows[y];
   for (x=0; x<xSize; ++x)
       fprintf(f, "\t%g", row[x]);
   fprintf(f, "\n");
   }
carefulClose(&f);
}

void matrixNormalizeColumns(char *inFile, boolean isLength, double target, char *outFile)
/* Normalize matrix one row at a time. */
{
/* Open up input file */
struct memMatrix *m = memMatrixFromTsv(inFile);

/* Go through matrix by column (y-dimension) */
int xSize = m->xSize, ySize = m->ySize;
int x,y;
for (x=0; x<xSize; ++x)
    {
    /* Calculate how big we are currently */
    double scaleVal = target;
    double sum = 0.0;
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

/* Output and go home */
memMatrixToTsv(m, outFile);
}


void matrixNormalize(char *direction, char *how, char *inFile, char *outFile)
/* matrixNormalize - Normalize a matrix somehow - make it's columns or rows all sum to one or 
 * have vector length one.. */
{
boolean isLength = howIsLength(how);
double target = optionDouble("target", 1.0);
if (sameWord(direction, "row"))
    matrixNormalizeRows(inFile, isLength, target, outFile);
else if (sameWord(direction, "column"))
    matrixNormalizeColumns(inFile, isLength,  target, outFile);
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
