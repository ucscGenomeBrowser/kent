#ifndef MATRIXMARKET_H
#define MATRIXMARKET_H

/* This handles reading simple numerical (real or integer) matrix market files.
 * See https://math.nist.gov/MatrixMarket/formats.html for more info on the format
 * It is a reasonable format for sparse (mostly 0) matrices.  This only handles a
 * small subset, but it's the one that people tend to use in single cell work. */

struct matrixMarket
/* Represent a matrixMarket file that has header read */
    {
    struct matrixMarket *next;
    struct lineFile *lf;    /* Open file */
    char *format;	    /* coordinate or array */
    char *type;		    /* real, integer, complex, pattern, */
    char *symmetries;	    /* general (for none), symmetric, skew-symmetric, or hermitian */
    int colCount;	    /* Number of columns (aka n) */
    int rowCount;	    /* Number of rows (aka m) */
    int valCount;	    /* Number of items with data in matrix */
    boolean isInt;	    /* Set to true if integer valued */
       /* Variables for number at a time access */
    int x,y;		    /* Current position */
    double val;		    /* Current values */
       /* Variables for row at a time access */
    int rowIx;		    /* The y value we are working on */
    double *row;	    /* The parsed out next row */
    boolean lastRow;	    /* Set when we get to last row */
    };

struct matrixMarket *matrixMarketOpen(char *fileName);
/* Open a matrixMarket file and read header, return a handle for use in more routines. */

void matrixMarketJustNumberCoordinates(struct matrixMarket *mm, char *fileName);
/* Make sure that the matrix is of the type we can handle */

void matrixMarketClose(struct matrixMarket **pMm);
/* Close down matrix market and free up associated resources */

boolean matrixMarketNext(struct matrixMarket *mm);
/* Fetch next value from matrix market, returning FALSE if at end of file.
 * The actual next values is stored in mm->x mm->y and mm->val */

boolean matrixMarketNextRow(struct matrixMarket *mm);
/* Fetch next row into with data into mm->row and mm->rowIx, return FALSE at eof.  */

#endif /* MATRIXMARKET_H */

