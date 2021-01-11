#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "obscure.h"
#include "sparseMatrix.h"


struct sparseRowMatrix *sparseRowMatrixNew(int xSize, int ySize)
/* Make up a new sparseRowMatrix structure */
{
struct sparseRowMatrix *matrix;
AllocVar(matrix);
matrix->xSize = xSize;
matrix->ySize = ySize;
matrix->lm = lmInit(0);
lmAllocArray(matrix->lm, matrix->rows, ySize);
return matrix;
}

void sparseRowMatrixFree(struct sparseRowMatrix **pMatrix)
/* Free up resources associated with sparse matrix  */
{
struct sparseRowMatrix *matrix = *pMatrix;
if (matrix != NULL)
    {
    lmCleanup(&matrix->lm);
    freez(pMatrix);
    }
}

void sparseRowMatrixAdd(struct sparseRowMatrix *matrix, int x, int y, float val)
/* Add data to our sparse matrix */
{
struct sparseRowVal *fv;
lmAllocVar(matrix->lm, fv);
fv->x = x;
fv->val = val;
slAddHead(&matrix->rows[y], fv);
}

void sparseRowMatrixUnpackRow(struct sparseRowMatrix *matrix, int y, double *row, int rowSize)
/* Unpack sparse matrix into buf that should be matrix->xSize long */
{
if (matrix->xSize != rowSize)
    errAbort("sparseRowMatrixUnpackRow size mismatch %d vs %d", matrix->xSize, rowSize);
int bufSize = matrix->xSize * sizeof(double);
zeroBytes(row, bufSize);
struct sparseRowVal *fv;
for (fv = matrix->rows[y]; fv != NULL; fv = fv->next)
    row[fv->x] = fv->val;
}

static void sparseRowMatrixTsvBody(struct sparseRowMatrix *matrix, char **rowLabels, FILE *f)
/* Write body (but not header) of matrix to tsv file */
{
int xSize = matrix->xSize; 
int ySize = matrix->ySize;
int x,y;
double row[xSize];
for (y=0; y<ySize; ++y)
    {
    sparseRowMatrixUnpackRow(matrix, y, row, xSize);
    fprintf(f, "%s", rowLabels[y]);
    for (x=0; x<xSize; ++x)
	{
	double val = row[x];
	if (val == 0.0) 
	    fprintf(f, "\t0");	    // so much zero in the world
	else
	    fprintf(f, "\t%g", row[x]);
	}
    fprintf(f, "\n");
    }
}

void sparseRowMatrixSaveAsTsv(struct sparseRowMatrix *matrix, 
    char **columnLabels, char **rowLabels, char *fileName)
/* Save sparseRowMatrix to tab-sep-file as expanded non-sparse */
{
FILE *f = mustOpen(fileName, "w");
verbose(1, "outputting %d row matrix, a dot every 100 rows\n", matrix->ySize);
fprintf(f, "gene\t");
writeTsvRow(f, matrix->xSize, columnLabels);
sparseRowMatrixTsvBody(matrix, rowLabels, f);
carefulClose(&f);
}

