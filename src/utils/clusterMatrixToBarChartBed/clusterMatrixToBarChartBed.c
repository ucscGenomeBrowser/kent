/* clusterMatrixToBarChartBed - Compute a barchart bed file from  a gene matrix 
 * and a gene bed file and a way to cluster samples. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "obscure.h"
#include "sqlNum.h"

boolean clSimple = FALSE;
boolean clMedian = FALSE;
char *clName2 = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "clusterMatrixToBarChartBed - Compute a barchart bed file from  a gene matrix\n"
  "and a gene bed file and a way to cluster samples.\n"
  "NOTE: consider using matrixClusterColumns and matrixToBarChartBed instead\n"
  "usage:\n"
  "   clusterMatrixToBarChartBed sampleClusters.tsv geneMatrix.tsv geneset.bed output.bed\n"
  "where:\n"
  "   sampleClusters.tsv is a two column tab separated file with sampleId and clusterId\n"
  "   geneMatrix.tsv has a row for each gene. The first row uses the same sampleId as above\n"
  "   geneset.bed has the maps the genes in the matrix (from it's first column) to the genome\n"
  "        geneset.bed needs 6 standard bed fields.  Unless name2 is set it also needs a name2\n"
  "        field as the last field\n"
  "   output.bed is the resulting bar chart, with one column per cluster\n"
  "options:\n"
  "   -simple - don't store the position of gene in geneMatrix.tsv file in output\n"
  "   -median - use median (instead of mean)\n"
  "   -name2=twoColFile.tsv - get name2 from file where first col is same ase geneset.bed's name\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"simple", OPTION_BOOLEAN},
   {"median", OPTION_BOOLEAN},
   {"name2", OPTION_STRING},
   {NULL, 0},
};

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

void clusterMatrixToBarChartBed(char *sampleClusters, char *matrixTsv, char *geneBed, char *output)
/* clusterMatrixToBarChartBed - Compute a barchart bed file from  a gene matrix 
 * and a gene bed file and a way to cluster samples. */
{
/* Figure out if we need to do medians etc */
boolean doMedian = clMedian;

/* Load up the gene set */
verbose(2, "clusterMatrixToBarChartBed(%s,%s,%s,%s)\n", sampleClusters, matrixTsv, geneBed, output);
int bedRowSize = 0;
struct hash *geneHash = hashTsvBy(geneBed, 3, &bedRowSize);
verbose(2, "%d columns about %d genes in %s\n", bedRowSize, geneHash->elCount, geneBed);

/* Deal with external gene hash */
struct hash *nameToName2 = NULL;
if (clName2 != NULL)
    {
    int colCount = 0;
    nameToName2 = hashTsvBy(clName2, 0, &colCount);
    if (colCount != 2)
        errAbort("Expecting %s to be a two column tab separated file", clName2);
    }

/* Keep track of how many fields gene bed has to have and locate name2 */
int geneBedMinSize = 7;
int name2Ix = bedRowSize - 1;	    // Last field if it is in bed
if (clName2 != NULL)
    geneBedMinSize -= 1;
if (bedRowSize < geneBedMinSize)
    {
    if (clName2 == NULL)
	errAbort("%s needs to have at least 6 standard BED fields and a name2 field\n", geneBed);
    else
	errAbort("%s needs to have at least 6 standard BED fields\n", geneBed);
    }


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

/* Figure out size of each alphabetized cluster in terms of number of samples in cluster 
 * if we are doing median */
int clusterSize[clusterCount];
double *clusterSamples[clusterCount];
if (doMedian)
    {
    for (clusterIx = 0; clusterIx < clusterCount; ++clusterIx)
	{
	clusterSize[clusterIx] = hashIntVal(clusterHash, clusterNames[clusterIx]);
	verbose(2, "clusterSize[%d] = %d\n", clusterIx, clusterSize[clusterIx]);
	}

    /* Make up array of doubles for each cluster to hold all samples in that clusters */
    for (clusterIx = 0; clusterIx < clusterCount; ++clusterIx)
	{
	double *samples;
	AllocArray(samples, clusterSize[clusterIx]);
	clusterSamples[clusterIx] = samples;
	}
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
double sumTotal = 0;
dotForUserInit(100);
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
    struct hashEl *onePos = hashLookup(geneHash, geneName);
    if (onePos == NULL)
	{
	verbose(2, "Can't find gene %s in %s", geneName, geneBed);
	++missCount;
	continue;
	}
    else
	{
	++hitCount;
	}

    /* A gene may map multiple places.  This loop takes care of that */
    for (; onePos != NULL; onePos = hashLookupNext(onePos))
        {
	char **geneBedVal = onePos->val;	// Get our bed as string array out of hash

	/* Zero out cluster histogram */
	int i;
	for (i=0; i<clusterCount; ++i)
	    {
	    clusterTotal[i] = 0.0;
	    clusterElements[i] = 0;
	    }

	/* Loop through rest of row filling in histogram */
	for (i=1; i<colCount; ++i)
	    {
	    int clusterIx = colToCluster[i];
	    char *textVal = matrixRow[i];
	    // special case so common we parse out "0" inline
	    double val = (textVal[0] == '0' && textVal[1] == 0) ? 0.0 : sqlDouble(textVal);
	    sumTotal += val;
	    int valCount = clusterElements[clusterIx];
	    clusterElements[clusterIx] = valCount+1;
	    if (doMedian)
		{
		if (valCount >= clusterSize[clusterIx])
		    internalErr();
		clusterSamples[clusterIx][valCount] = val;
		}
	    else
		clusterTotal[clusterIx] += val;
	    }

	/* Output info - first six from the bed, then name2, then our barchart */
	for (i=0; i<6; ++i)
	    fprintf(f, "%s\t",  geneBedVal[i]);

	char *name = geneBedVal[3];	// By bed definition it's fourth field
	char *name2 = NULL;
	if (nameToName2 != NULL)
	    {
	    char **namedRow = hashFindVal(nameToName2, name);
	    if (namedRow != NULL)
		name2 = namedRow[1];	    // [0] is name 
	    else
	        warn("Can't find %s in %s", name, clName2);
	    }
	else
	    name2 = geneBedVal[name2Ix];
	if (name2 == NULL)
	    name2 = name;
	fprintf(f, "%s\t", name2);

	fprintf(f, "%d\t", clusterCount);
	for (i=0; i<clusterCount; ++i)
	    {
	    if (i != 0)
	       fprintf(f, ",");
	    if (doMedian)
		fprintf(f, "%g", doubleMedian(clusterElements[i], clusterSamples[i]));
	    else
		{
		fprintf(f, "%g",  clusterTotal[i]/clusterElements[i]);
		}
	    }
	
	/* Data file offset info */
	if (!clSimple)
	    fprintf(f, "\t%lld\t%lld",  (long long)lineFileTell(lf), (long long)lineLength);

	fprintf(f, "\n");
	}
    dotForUser();
    }
verbose(1, "\n%d genes found, %d (%0.2f%%) missed\n", hitCount, missCount, 100.0*missCount/(hitCount+missCount));
if (!doMedian)
    {
    verbose(1, "matrix total %g, %d clusters, %g ave/cluster\n", 
	sumTotal, clusterCount, sumTotal/clusterCount);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
clSimple = optionExists("simple");
clMedian = optionExists("median");
clName2 = optionVal("name2", clName2);
clusterMatrixToBarChartBed(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
