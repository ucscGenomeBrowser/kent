/* sparseRowMatrix - library to support sparse matrices that are accessed conveniently by row. */

#ifndef SPARSEROWMATRIX_H
#define SPARSEROWMATRIX_H

struct sparseRowVal
/* Linked list of things in a row */
    {
    struct sparseRowVal *next;
    int x;
    float val;
    };

struct sparseRowMatrix
/* A sparse matrix with fast row access in memory */
    {
    struct sparseRowMatrix *next;
    int xSize, ySize;	/* Dimensions of our matrix */
    struct lm *lm;	/* Local memory pool, where row lists come from */
    struct sparseRowVal **rows;
    };

struct sparseRowMatrix *sparseRowMatrixNew(int xSize, int ySize);
/* Make up a new sparseRowMatrix structure */

void sparseRowMatrixFree(struct sparseRowMatrix **pMatrix);
/* Free up resources associated with sparse matrix  */

void sparseRowMatrixAdd(struct sparseRowMatrix *matrix, int x, int y, float val);
/* Add data to our sparse matrix */

void sparseRowMatrixUnpackRow(struct sparseRowMatrix *matrix, int y, double *row, int rowSize);
/* Unpack sparse matrix into buf that should be matrix->xSize long */

void sparseRowMatrixSaveAsTsv(struct sparseRowMatrix *matrix, 
    char **columnLabels, char **rowLabels, char *fileName);
/* Save sparseRowMatrix to tab-sep-file as expanded non-sparse */

#endif /* SPARSEROWMATRIX_H */

