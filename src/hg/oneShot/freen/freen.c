/* freen - My Pet Freen.  A pet freen is actually more dangerous than a wild one. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "portable.h"
#include "obscure.h"
#include "localmem.h"
#include "csv.h"
#include "hex.h"
#include "memgfx.h"
#include "correlate.h"
#include "vMatrix.h"

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen refMatrix.tsv sampleMatrix.tsv outColors.tsv\n");
}

double columnVectorLength(struct memMatrix *m, int column)
{
double **rows = m->rows;
int ySize = m->ySize;
int y;
double sumSquares = 0.0;
for (y=0; y<ySize; ++y)
    {
    double val = rows[y][column];
    sumSquares += val*val;
    }
return sqrt(sumSquares);
}

void normalizeColumn(struct memMatrix *m, int column)
/* Normalize a column in matrix so that as a vector it has length 1 */
{
double **rows = m->rows;
double length = columnVectorLength(m, column);
if (length > 0.0)
    {
    int ySize = m->ySize;
    int y;
    double invLength = 1/length;
    for (y=0; y<ySize; ++y)
	rows[y][column] *= invLength;
    }
verbose(2, "oldLength %g, newLength %g\n", length, columnVectorLength(m, column));
}

void normalizeColumns(struct memMatrix *m)
/* Normalize columns so that they have Euclidian length one as a vector */
{
int x;
for (x=0; x<m->xSize; ++x)
    normalizeColumn(m, x);
}

double refDistance(struct memMatrix *refMatrix, struct hash *refHash, int refCol,
    struct memMatrix *qMatrix, int qCol)
/* Return a dotProduct between a column in a reference matrix and a query matrix.
 * The reference matrix has a row hash so that we can look up the corresponding row
 * based on the label in the qMatrix row.  If somethign is missing from refMatrix
 * that's ok, it just won't contribute to the dot product. */
{
int i;
double sumSquares = 0.0;
for (i=0; i<qMatrix->ySize; ++i)
    {
    char *label = qMatrix->yLabels[i];
    double *row = hashFindVal(refHash, label);
    if (row != NULL)
	{
	double d = row[refCol] - qMatrix->rows[i][qCol];
	sumSquares += d*d;
	}
    }
return sqrt(sumSquares);
}

double refCorrelate(struct memMatrix *refMatrix, struct hash *refHash, int refCol,
    struct memMatrix *qMatrix, int qCol)
/* Return a dotProduct between a column in a reference matrix and a query matrix.
 * The reference matrix has a row hash so that we can look up the corresponding row
 * based on the label in the qMatrix row.  If somethign is missing from refMatrix
 * that's ok, it just won't contribute to the dot product. */
{
struct correlate *c = correlateNew();
int i;
for (i=0; i<qMatrix->ySize; ++i)
    {
    char *label = qMatrix->yLabels[i];
    double *row = hashFindVal(refHash, label);
    if (row != NULL)
	correlateNext(c, row[refCol], qMatrix->rows[i][qCol]);
    }
double result = correlateResult(c);
correlateFree(&c);
return result;
}

struct hash *loadColors(char *fileName)
/* Load up a hash keyed by name in the first column with R G B 2 digit hex in the next three cols */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = hashNew(0);
char *row[4];
while (lineFileRow(lf, row))
    {
    struct rgbColor color;
    color.r = hexToByte(row[1]);
    color.g = hexToByte(row[2]);
    color.b = hexToByte(row[3]);
    hashAdd(hash, row[0], lmCloneMem(hash->lm, &color, sizeof(color)));
    }
lineFileClose(&lf);
return hash;
}

void freen(char *refMatrixTsv, char *colorsTsv, char *sampleMatrixTsv, char *colorsOut)
/* Test something */
{
struct hash *colorHash = loadColors(colorsTsv);
struct memMatrix *refMatrix = memMatrixFromTsv(refMatrixTsv);
verbose(1, "%s is %d x %d\n", refMatrixTsv, refMatrix->xSize, refMatrix->ySize);
normalizeColumns(refMatrix);
struct memMatrix *sampleMatrix = memMatrixFromTsv(sampleMatrixTsv);
verbose(1, "%s is %d x %d\n", sampleMatrixTsv, sampleMatrix->xSize, sampleMatrix->ySize);
normalizeColumns(sampleMatrix);

/* Strip dots off of gene names in sample matrix */
int i;
for (i=0; i<sampleMatrix->ySize; ++i)
    chopSuffix(sampleMatrix->yLabels[i]);

struct hash *refRowHash = hashNew(0);
for (i=0; i<refMatrix->ySize; ++i)
    hashAdd(refRowHash, refMatrix->yLabels[i], refMatrix->rows[i]);

FILE *f = mustOpen(colorsOut, "w");
fprintf(f, "#sample");
for (i=0; i<refMatrix->xSize; ++i)
     fprintf(f, "\t%s", refMatrix->xLabels[i]);
fprintf(f, "\tcolor\n");
int sampleIx;
for (sampleIx=0; sampleIx<sampleMatrix->xSize; ++sampleIx)
     {
     /* Print label */
     char *sampleLabel = sampleMatrix->xLabels[sampleIx];
     fprintf(f, "%s", sampleLabel);

     /* Figure out distances from sample to each reference and save min/max distance */
     int refIx;
     double minDistance = 3.0;	// 2 is as big as it gets
     double maxDistance = 0;
     double distances [refMatrix->xSize];
     for (refIx=0; refIx < refMatrix->xSize; ++refIx)
         {
	 double distance = refDistance(refMatrix, refRowHash, refIx,  sampleMatrix, sampleIx);
	 if (distance < minDistance)
	     minDistance = distance;
	 if (distance > maxDistance)
	     maxDistance = distance;
	 distances[refIx] = distance;
	 }

     for (refIx = 0; refIx < refMatrix->xSize; ++refIx)
	 distances[refIx] += 0.02 - minDistance*0.9;

     double totalForce = 0;
     double forces[refMatrix->xSize];
     for (refIx=0; refIx < refMatrix->xSize; ++refIx)
         {
	 double distance = distances[refIx];
	 double force = 1.0/(distance*distance);
	 fprintf(f, "\t%g", force);
	 totalForce += force;
	 forces[refIx] = force;
	 }

     /* Figure out RGB values */
     double r=0, g=0, b=0;
     if (totalForce > 0)
	 {
	 for (refIx=0; refIx < refMatrix->xSize; ++refIx)
	     {
	     char *label = refMatrix->xLabels[refIx];
	     struct rgbColor *color = hashMustFindVal(colorHash, label);
	     double normForce = forces[refIx]/totalForce;
	     r += color->r * normForce;
	     g += color->g * normForce;
	     b += color->b * normForce;
	     }
	 }
     else
          r = g = b = 0x70;
     fprintf(f, "\t#%02x%02x%02x\n", round(r), round(g), round(b));
     }
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
freen(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

