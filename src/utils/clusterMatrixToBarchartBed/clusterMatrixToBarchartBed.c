/* clusterMatrixToBarchartBed - Take a gene matrix and a gene bed file and a way to cluster 
 * samples into a barchart type bed. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "sqlNum.h"

boolean clDataOffset = FALSE;
boolean clMean = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "clusterMatrixToBarchartBed - Take a gene matrix and a gene bed file and a way to cluster\n"
  "samples into a barchart type bed\n"
  "usage:\n"
  "   clusterMatrixToBarchartBed sampleClusters.tsv geneMatrix.tsv geneset.bed output.bed\n"
  "where:\n"
  "   sampleClusters.tsv is a tab separated file with the first column being sample ids\n"
  "   geneMatrix.tsv has a row for each gene. The first row uses the same sample ids\n"
  "   geneset.bed has the maps the genes in the matrix (from it's first column) to the genome\n"
  "   output.bed is the resulting bar chart, with one column per cluster\n"
  "options:\n"
  "   -dataOffset - store the position of gene in geneMatrix.tsv file in output\n"
  "   -mean - use mean (instead of median)\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"dataOffset", OPTION_BOOLEAN},
   {"_dataOffset", OPTION_BOOLEAN},
   {"mean", OPTION_BOOLEAN},
   {NULL, 0},
};

struct hash *hashTsvBy(char *in, int keyColIx, int *retColCount)
/* Return a hash of rows keyed by the given column */
{
struct lineFile *lf = lineFileOpen(in, TRUE);
struct hash *hash = hashNew(0);
char *line = NULL, **row = NULL;
int colCount = 0, colAlloc=0;	/* Columns as counted and as allocated */
while (lineFileNextReal(lf, &line))
    {
    if (colCount == 0)
        {
	*retColCount = colCount = chopByChar(line, '\t', NULL, 0);
	verbose(2, "Got %d columns in first real line\n", colCount);
	colAlloc = colCount + 1;  // +1 so we can detect unexpected input and complain 
	lmAllocArray(hash->lm, row, colAlloc);
	}
    int count = chopByChar(line, '\t', row, colAlloc);
    if (count != colCount)
        {
	errAbort("Expecting %d words, got more than that line %d of %s", 
	    colCount, lf->lineIx, lf->fileName);
	}
    hashAddUnique(hash, row[keyColIx], lmCloneRow(hash->lm, row, colCount) );
    }
lineFileClose(&lf);
return hash;
}

void hashSamplesAndClusters(char *tsvFile, 
    struct hash **retSampleHash, struct hash **retClusterHash)
/* Read two column tsv file into a hash keyed by first column */
{
struct hash *sampleHash = hashNew(0);
struct hash *clusterHash = hashNew(0);
char *row[2];
struct lineFile *lf = lineFileOpen(tsvFile, TRUE);
while (lineFileNextRowTab(lf, row, ArraySize(row)) )
    {
    /* Find cluster in cluster hash, if it doesn't exist make it. */
    char *clusterName = row[1];
    struct hashEl *hel = hashLookup(clusterHash, clusterName);
    if (hel == NULL)
	hel = hashAddInt(clusterHash, clusterName, 1);
    else
	hel->val = ((char *)hel->val)+1;    // Increment hash pointer as per hashIncInt
    char *clusterStableName = hel->name;	// This is allocated in clusterHash
    hashAdd(sampleHash, row[0], clusterStableName);
    }
lineFileClose(&lf);
*retSampleHash = sampleHash;
*retClusterHash = clusterHash;
}

void clusterMatrixToBarchartBed(char *sampleClusters, char *matrixTsv, char *geneBed, char *output)
/* clusterMatrixToBarchartBed - Take a gene matrix and a gene bed file and a way to cluster samples 
 * into a barchart type bed. */
{
/* Load up the gene set */
verbose(1, "clusterMatrixToBarchartBed(%s,%s,%s,%s)\n", sampleClusters, matrixTsv, geneBed, output);
int bedRowSize = 0;
struct hash *geneHash = hashTsvBy(geneBed, 3, &bedRowSize);
verbose(1, "%d genes in %s\n", geneHash->elCount, geneBed);

/* Load up the sample clustering */
struct hash *sampleHash = NULL, *clusterHash = NULL;
hashSamplesAndClusters(sampleClusters, &sampleHash, &clusterHash);
int clusterCount = clusterHash->elCount;
verbose(1, "%d samples and %d clusters in %s\n", sampleHash->elCount, clusterCount,
    sampleClusters);
if (clusterCount <= 1 || clusterCount >= 10000)
    errAbort("%d is not a good number of clusters", clusterCount);
double clusterTotal[clusterCount];
int clusterElements[clusterCount];

/* Alphabetize cluster names  */
char *clusterNames[clusterCount];
struct hashEl *hel;
struct hashCookie cookie = hashFirst(clusterHash);
int clusterIx = 0;
while ((hel = hashNext(&cookie)) != NULL)
    {
    clusterNames[clusterIx] = hel->name;
    ++clusterIx;
    }
sortStrings(clusterNames, clusterCount);
verbose(2, "%s to %s\n", clusterNames[0], clusterNames[clusterCount-1]);

/* Figure out size of each alphabetized cluster in terms of number of samples in cluster */
int clusterSize[clusterCount];
for (clusterIx = 0; clusterIx < clusterCount; ++clusterIx)
    {
    clusterSize[clusterIx] = hashIntVal(clusterHash, clusterNames[clusterIx]);
    verbose(2, "clusterSize[%d] = %d\n", clusterIx, clusterSize[clusterIx]);
    }

/* Make up array of doubles for each cluster to hold all samples in that clusters */
double *clusterSamples[clusterCount];
for (clusterIx = 0; clusterIx < clusterCount; ++clusterIx)
    {
    double *samples;
    AllocArray(samples, clusterSize[clusterIx]);
    clusterSamples[clusterIx] = samples;
    }

/* Make hash that goes from cluster name to cluster index */
struct hash *clusterToClusterIdHash = hashNew(0);
for (clusterIx = 0; clusterIx<clusterCount; ++clusterIx)
    {
    hashAddInt(clusterToClusterIdHash, clusterNames[clusterIx], clusterIx);
    }


/* Open output */
FILE *f = mustOpen(output, "w");

/* Open up matrix file and read first line into sample labeling */
struct lineFile *lf = lineFileOpen(matrixTsv, TRUE);
char *line;
lineFileNeedNext(lf, &line, NULL);
if (line[0] == '#')	// Opening sharp on labels is optional, skip it if there
    line = skipLeadingSpaces(line+1);
int colCount = chopByChar(line, '\t', NULL, 0);
int colAlloc = colCount + 1;
char **sampleNames;
AllocArray(sampleNames, colAlloc);
chopByChar(line, '\t', sampleNames, colCount);

/* Make array that maps row index to clusterID */
int colToCluster[colCount];
int colIx;
for (colIx=1; colIx <colCount; colIx = colIx+1)
    {
    char *colName = sampleNames[colIx];
    char *clusterName = hashFindVal(sampleHash, colName);
    colToCluster[colIx] = -1;
    if (clusterName != NULL)
        {
	int clusterId = hashIntValDefault(clusterToClusterIdHash, clusterName, -1);
	colToCluster[colIx] = clusterId;
	if (clusterId == -1)
	    warn("%s is in expression matrix but not in sample cluster file", clusterName);
	}
    }


/* Set up row for reading one row of matrix at a time. */
char **matrixRow;
AllocArray(matrixRow, colAlloc);
int hitCount = 0, missCount = 0;
for (;;)
    {
    /* Fetch next line and remember how long it is.  Just skip over lines that are empty or
     * start with # character. */
    int lineLength = 0;
    char *line;
    if (!lineFileNext(lf, &line, &lineLength))
        break;
    char *s = skipLeadingSpaces(line);
    char c = s[0];
    if (c == 0 || c == '#')
        continue;

    /* Chop it into tabs */
    int rowSize = chopByChar(line, '\t', matrixRow, colAlloc);
    lineFileExpectWords(lf, colCount, rowSize);

    char *geneName = matrixRow[0];
    char **geneBedVal = hashFindVal(geneHash, geneName);
    if (geneBedVal == NULL)
	{
        warn("Can't find gene %s in %s", geneName, geneBed);
	++missCount;
	continue;
	}
    else
        ++hitCount;


    /* Zero out cluster histogram */
    int i;
    for (i=0; i<clusterCount; ++i)
        {
	clusterTotal[i] = 0.0;
	clusterElements[i] = 0;
	}

    zeroBytes(&clusterTotal, sizeof(clusterTotal));
    zeroBytes(&clusterElements, sizeof(clusterElements));

    /* Loop through rest of row filling in histogram */
    for (i=1; i<colCount; ++i)
        {
	int clusterIx = colToCluster[i];
	double val = sqlDouble(matrixRow[i]);
	int pos = clusterElements[clusterIx];
	if (pos >= clusterSize[clusterIx])
	    internalErr();
	clusterElements[clusterIx] = pos+1;
	clusterSamples[clusterIx][pos] = val;
	clusterTotal[clusterIx] += val;
	}

    /* Output info */
    for (i=0; i<bedRowSize; ++i)
        fprintf(f, "%s\t",  geneBedVal[i]);
    fprintf(f, "%d\t", clusterCount);
    for (i=0; i<clusterCount; ++i)
        {
	if (i != 0)
	   fprintf(f, ",");
	if (clMean)
	    fprintf(f, "%g",  clusterTotal[i]/clusterElements[i]);
	else
	    fprintf(f, "%g", doubleMedian(clusterElements[i], clusterSamples[i]));
	}
    
    /* Data file offset info */
    if (clDataOffset)
        fprintf(f, "\t%lld\t%lld",  (long long)lineFileTell(lf), (long long)lineLength);

    fprintf(f, "\n");
    }
verbose(1, "%d genes found, %d missed\n", hitCount, missCount);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
clDataOffset = (optionExists("_dataOffset") || optionExists("dataOffset"));
clMean = optionExists("mean");
clusterMatrixToBarchartBed(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
