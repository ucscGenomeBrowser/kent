/* hcaColorCells - Try and figure out colors for cells based on some example cells and colors.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "localmem.h"
#include "sqlNum.h"
#include "memgfx.h"
#include "correlate.h"
#include "vMatrix.h"
#include "hex.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hcaColorCells - Try and figure out colors for cells based on some example cells and colors.\n"
  "usage:\n"
  "   hcaColorCells refMatrix.tsv refRgb.tsv sampleMatrix.tsv outColors.tsv\n"
  "options:\n"
  "   -trackDb=out.ra - output colors as barChartColors trackDb line\n"
  "   -stats=sample.stats from matrixClusterColumns - will lighten low N columns\n"
  "   -cellMin=N - set the minimum number of cells to get color penalty to N\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"trackDb", OPTION_STRING},
   {"stats", OPTION_STRING},
   {"cellMin", OPTION_INT},
   {NULL, 0},
};

double columnVectorLength(struct memMatrix *m, int column)
/* Figure out length (euclidean) of column in matrix viewed as a vector */
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
 * based on the label in the qMatrix row.  If something is missing from refMatrix
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


struct hash *loadColors(char *fileName)
/* Load up a hash keyed by name in the first column with R G B 2 digit hex in the next three cols */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = hashNew(0);
char *row[4];
while (lineFileRow(lf, row))
    {
    struct rgbColor color;
    color.r = unpackHexString(row[1], lf, 2);
    color.g = unpackHexString(row[2], lf, 2);
    color.b = unpackHexString(row[3], lf, 2);
    hashAdd(hash, row[0], lmCloneMem(hash->lm, &color, sizeof(color)));
    }
lineFileClose(&lf);
return hash;
}

void stripGencodeVersion(struct memMatrix *m)
/* Strip version off of any gencode genes in y labels */
{
int y;
for (y=0; y<m->ySize; ++y)
    {
    char *label = m->yLabels[y];
    if (startsWith("ENS", label))
	chopSuffix(label);
    }
}

struct matrixClusterStats
/* Stats on a clustering of a matrix */
    {
    struct matrixClusterStats *next;  /* Next in singly linked list. */
    char *cluster;	/* The name of the cluster */
    int count;	/* The number of samples clustered together from old matrix */
    double total;	/* The sum of all samples */
    };

struct matrixClusterStats *matrixClusterStatsLoad(char **row)
/* Load a matrixClusterStats from row fetched with select * from matrixClusterStats
 * from database.  Dispose of this with matrixClusterStatsFree(). */
{
struct matrixClusterStats *ret;

AllocVar(ret);
ret->cluster = cloneString(row[0]);
ret->count = sqlSigned(row[1]);
ret->total = sqlDouble(row[2]);
return ret;
}

struct matrixClusterStats *matrixClusterStatsLoadAllByChar(char *fileName, char chopper) 
/* Load all matrixClusterStats from a chopper separated file.
 * Dispose of this with matrixClusterStatsFreeList(). */
{
struct matrixClusterStats *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];

while (lineFileNextCharRow(lf, chopper, row, ArraySize(row)))
    {
    el = matrixClusterStatsLoad(row);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}


void hcaColorCells(char *refMatrixTsv, char *colorsTsv, char *sampleMatrixTsv, char *colorsOut)
/* hcaColorCells - Try and figure out colors for cells based on some example cells and colors.. */
{
struct hash *colorHash = loadColors(colorsTsv);

struct memMatrix *refMatrix = memMatrixFromTsv(refMatrixTsv);
verbose(1, "%s is %d x %d\n", refMatrixTsv, refMatrix->xSize, refMatrix->ySize);
normalizeColumns(refMatrix);
stripGencodeVersion(refMatrix);

struct memMatrix *sampleMatrix = memMatrixFromTsv(sampleMatrixTsv);
verbose(1, "%s is %d x %d\n", sampleMatrixTsv, sampleMatrix->xSize, sampleMatrix->ySize);
normalizeColumns(sampleMatrix);
stripGencodeVersion(sampleMatrix);

struct hash *statsHash = NULL;
int cellMin = optionInt("cellMin", 12);
int cellNearMin = cellMin*6;
char *statsFile = optionVal("stats", NULL);
if (statsFile != NULL)
    {
    struct matrixClusterStats *el, *list = matrixClusterStatsLoadAllByChar(statsFile, '\t');
    statsHash = hashNew(0);
    for (el = list; el != NULL; el = el->next)
        hashAdd(statsHash, el->cluster, el);
    }

struct hash *refRowHash = hashNew(0);
int i;
for (i=0; i<refMatrix->ySize; ++i)
    hashAdd(refRowHash, refMatrix->yLabels[i], refMatrix->rows[i]);

char *trackDb = optionVal("trackDb", NULL);
FILE *trackF = NULL;
if (trackDb != NULL)
     {
     trackF = mustOpen(trackDb, "w");
     fprintf(trackF, "barChartColors");
     }

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
     double distances[refMatrix->xSize];
     for (refIx=0; refIx < refMatrix->xSize; ++refIx)
         {
	 double distance = refDistance(refMatrix, refRowHash, refIx,  sampleMatrix, sampleIx);
	 if (distance < minDistance)
	     minDistance = distance;
	 if (distance > maxDistance)
	     maxDistance = distance;
	 distances[refIx] = distance;
	 }

     double totalForce = 0;
     double forces[refMatrix->xSize];
     for (refIx=0; refIx < refMatrix->xSize; ++refIx)
         {
	 double distance = distances[refIx];
	 // fprintf(f, "\t%g", distance);
	 distance += 0.04 - minDistance*0.9; // The 0.04 avoids zero singularity
					     // The -90% minDistance amplifies color
	 double force = 1.0/(distance*distance);    // The distance squared amplifies too
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
	     struct rgbColor *color = hashFindVal(colorHash, label);
	     if (color == NULL)
	         errAbort("Can't find %s in %s", label, colorsTsv);
	     double normForce = forces[refIx]/totalForce;
	     r += color->r * normForce;
	     g += color->g * normForce;
	     b += color->b * normForce;
	     }
	 }
     else
        r = g = b = 0x70;
     if (statsHash != NULL)
	{
	double belief = 1.0;
	double modFactor = 0.666;
	struct matrixClusterStats *stats = hashFindVal(statsHash, sampleLabel);
	if (stats == NULL)
	    errAbort("%s is in matrixFile but not %s", sampleLabel, statsFile);
	if (stats->count < cellNearMin)
	    belief *= modFactor;
	if (stats->count < cellMin)
	    belief *= modFactor;
	if (stats->total < 1e6)
	    belief *= modFactor;
	if (stats->total < 2e5)
	    belief *= modFactor;
	double doubt = 1.0 - belief;
	int maxColor = 255;
	r = r * belief + maxColor*doubt;
	g = g * belief + maxColor*doubt;
	b = b * belief + maxColor*doubt;
	}
     int ir = round(r);
     int ig = round(g);
     int ib = round(b);
     fprintf(f, "\t#%02x%02x%02x\n", ir, ig, ib);
     if (trackF != NULL)
	 fprintf(trackF, " #%02x%02x%02x", ir, ig, ib);
     }
carefulClose(&f);
if (trackF != NULL)
    {
    fprintf(trackF, "\n");
    carefulClose(&trackF);
    }

}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
hcaColorCells(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

