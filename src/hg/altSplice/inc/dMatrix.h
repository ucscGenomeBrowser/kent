/* dMatrix.h - Module for readling in a matrix of real valued results
   with column and row names. */

#ifndef DMATRIX_H
#define DMATRIX_H

struct dMatrix 
/* Wrapper for a matrix of results. */
{
    struct hash *nameIndex;  /* Hash with integer index for each row name. */
    int colCount;            /* Number of columns in matrix. */
    char **colNames;         /* Column Names for matrix. */
    int rowCount;            /* Number of rows in matrix. */
    char **rowNames;         /* Row names for matrix. */
    double **matrix;         /* The actual data for the resultM. */
};

struct dMatrix *dMatrixLoad(char *fileName);
/* Read in an R style result table. */

void dMatrixFree(struct dMatrix **pDMatrix);
/* Free up the memory used by a dMatrix. */

#endif /* DMATRIX_H */
