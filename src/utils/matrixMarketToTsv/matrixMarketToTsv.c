/* matrixMarketToTsv - Convert matrix file from Matrix Market sparse matrix format to tab-separated-values.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "matrixMarket.h"
#include "obscure.h"

int dotMod = 100;  // Put out an I'm alive dot every so often

void usage()
/* Explain usage and exit. */
{
errAbort(
  "matrixMarketToTsv - Convert matrix file from Matrix Market sparse matrix format to tab-separated-values.\n"
  "usage:\n"
  "   matrixMarketToTsv in.mtx sampleLabels.lst geneLabels.lst out.tsv\n"
  "where in.mtx is a matrix market format matrix.  SampleLabels is a text file\n"
  "with one label per line.  It will end in the first row of the output.\n"
  "GeneLabels.lst is a text file with one gene name per line.  It will end up\n"
  "in the first column of the output\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void matrixMarketToTsv(char *inMatrix, char *inSamples, char *inGenes, char *outMatrix)
/* matrixMarketToTsv - Convert matrix file from Matrix Market sparse matrix format to tab-separated-values.. */
{
struct matrixMarket *mm = matrixMarketOpen(inMatrix);
verbose(1, "%s has %d rows and %d columns\n", inMatrix, mm->rowCount, mm->colCount);
struct slName *sampleList = readAllLines(inSamples);
int sampleCount = slCount(sampleList);
verbose(1, "Read %d samples from %s\n", sampleCount, inSamples);
if (sampleCount != mm->rowCount)
    errAbort("Mismatch between row count in matrix and sample count");
struct slName *geneList = readAllLines(inGenes);
int geneCount = slCount(geneList);
verbose(1, "Read %d genes from %s\n", geneCount, inGenes);
if (geneCount != mm->colCount)
    errAbort("Mismatch between column count in matrix and gene count");

/* Create matrix in memory the right size */
double **rows;
AllocArray(rows, geneCount);
int i;
for (i=0; i<geneCount; ++i)
    rows[i] = needMem(sampleCount * sizeof(double) );

/* Fill in matrix from mart */
verbose(1, "Reading matrix\n");
while (matrixMarketNext(mm))
    {
    rows[mm->x][mm->y] = mm->val;
    }
matrixMarketClose(&mm);

/* Open output */
verbose(1, "Writing output matrix\n");
FILE *f = mustOpen(outMatrix, "w");

/* Write first line */
struct slName *name = sampleList;
for (i=0; i<sampleCount; ++i)
    {
    fprintf(f, "\t%s", name->name);
    name = name->next;
    }
fprintf(f, "\n");

/* Write out rest of lines */
if (geneCount >= dotMod)
    verbose(1, "Showing one dot for every dotMod genes output\n");
dotForUserInit(dotMod);
name = geneList;
for (i=0; i<geneCount; ++i)
    {
    fprintf(f, "%s", name->name);
    name = name->next;
    int j;
    for (j=0; j<sampleCount; ++j)
        {
	fprintf(f, "\t%g", rows[i][j]);
	}
    fprintf(f, "\n");
    dotForUser();
    }
if (geneCount >= dotMod)
    fprintf(stderr, "\n");  /* To avoid ragged end of dots */
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
matrixMarketToTsv(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
