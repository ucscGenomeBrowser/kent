/* vMatrix - stuff to handle virtual matrices of double-precision numbers */

#ifndef VMATRIX_H
#define VMATRIX_H

/* To use the vRowMatrix for line at a time access do:
 *       struct vRowMatrix *v = vRowMatrixOnTsv(fileName);
 *       char *label;
 *       double *row;
 *       while ((row = vRowMatrixNextRow(v, &label))
 *           {
 *           // do your processing
 *           }
 *       vRowMatrixClose(&v);
 */

struct vRowMatrix
/* A virtual matrix optimized for row-at-a-time access */
    {
    struct vRowMatrix *next;
    int xSize;	/* Dimensions */
    int y;		/* Current Y position */
    char *centerLabel;	/* Label to put in corner between row and column labels */
    char **columnLabels;	    /* xSize of these */
    double *(*nextRow)(struct vRowMatrix *matrix, char **retLabel);	    /* Fetch next row of data, NULL at end */
    void (*free)(struct vRowMatrix *matrix);	    /* Free up any associated data */
    void *vData;				    /* Type dependent pointer */
    };

struct vRowMatrix *vRowMatrixOnTsv(char *fileName);
/* Return a vRowMatrix on a tsv file with first column and row as labels */

double *vRowMatrixNextRow(struct vRowMatrix *v, char **retLabel);
/* Return next row or NULL at end */

void vRowMatrixFree(struct vRowMatrix **pv);
/* Free up vRowMatrix (and call it's free method) */

struct vRowMatrix *vRowMatrixNewEmpty(int xSize, char **columnLabels, char *centerLabel);
/* Allocate vRowMatrix but without methods or data filled in.  Typically used by a factory/wrapper method  
 * such as vRowMatrixOnTsv*/

void vMatrixSaveAsTsv(struct vRowMatrix *matrix, 
    char **columnLabels, char **rowLabels, char *fileName);
/* Save sparseRowMatrix to tab-sep-file as expanded non-sparse */


/* Big old memory unpacked in memory matrices - a useful thing sometimes. */
struct memMatrix
/* Unpacked representation in memory */
    {
    struct memMatrix *next;
    int xSize, ySize;	/* Dimensions */
    double **rows;	/* There are xSize items in each of ySize rows */
    char **xLabels, **yLabels;  /* Arrays of labels */
    char *centerLabel;	/* Label to put in corner between row and column labels */
    struct lm *lm;
    };

void memMatrixFree(struct memMatrix **pMatrix);
/* Free up memory matrix */

struct memMatrix *memMatrixFromTsv(char *fileName);
/* Return a memMatrix based on file */

void memMatrixToTsv(struct memMatrix *m, char *fileName);
/* Write out memMatrix to file. */

#endif /* VMATRIX_H */
