/* matrixClusterColumns - Group the columns of a matrix into clusters, and output a matrix 
 * the with same number of rows and generally much fewer columns.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "fieldedTable.h"
#include "sqlNum.h"
#include "pthreadDoList.h"

#define clThreadCount 50
#define chunkItemsPerThread 5
#define chunkMaxSize (clThreadCount * chunkItemsPerThread)

void usage()
/* Explain usage and exit. */
{
errAbort(
  "matrixClusterColumns - Group the columns of a matrix into clusters, and output a matrix with\n"
  "the same number of rows and generally much fewer columns. Combines columns by taking mean.\n"
  "usage:\n"
  "   matrixClusterColumns inMatrix.tsv meta.tsv cluster outMatrix.tsv outStats.tsv [cluster2 outMatrix2.tsv outStats2.tsv ... ]\n"
  "where:\n"
  "   inMatrix.tsv is a file in tsv format with cell labels in first row and gene labels in first column\n"
  "   meta.tsv is a table where the first row is field labels and the first column is sample ids\n"
  "   cluster is the name of the field with the cluster names\n"
  "You can produce multiple clusterings in the same pass through the input matrix by specifying\n"
  "additional cluster/outMatrix/outStats triples in the command line.\n"
  "options:\n"
  "   -makeIndex=index.tsv - output index tsv file with <matrix-col1><input-file-pos><line-len>\n"
  "   -median if set ouput median rather than mean cluster value\n"
  "   -excludeZeros if set exclude zeros when calculating mean/median\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"makeIndex", OPTION_STRING},
   {"median", OPTION_BOOLEAN},
   {"excludeZeros", OPTION_BOOLEAN},
   {NULL, 0},
};


void readLineArray(char *fileName, int *retCount, char ***retLines)
/* Return an array of strings, one for each line of file.  Return # of lines in file too */
{
/* This is sloppy with memory but it doesn't matter since we won't free it. */
struct slName *el, *list = readAllLines(fileName);
if (list == NULL)
    errAbort("%s is empty", fileName);
int count = slCount(list);
char **lines;
AllocArray(lines, count);
int i;
for (i=0, el=list; i<count; ++i, el = el->next)
    {
    lines[i] = el->name;
    }
*retCount = count;
*retLines = lines;
}

int countNonzero(double *a, int size)
/* Return number of nonzero items in array a */
{
int count = 0;
while (--size >= 0)
    if (*a++ != 0.0)
        ++count;
return count;
}

double sumArray(double *a, int size)
/* Return sum of all items in array */
{
double sum = 0.0;
while (--size >= 0)
    sum += *a++;
return sum;
}

struct ccMatrix
/* Local matrix structure - little wrapper around a fielded table/lineFile combination */
    {
    struct ccMatrix *next;

        /* kind of private fields */
    struct lineFile *lf;	    // Line file for tab-sep case
    struct fieldedTable *ft;	    // fielded table if a tab-sep file

	/* From below are fields that yu can read but not change */
    int colCount;		    // Number of columns in a row
    char **colLabels;		    // A label for each column 
    int curRow;			    // Current row we are processing
    };

struct ccMatrix *ccMatrixOpen(char *matrixFile)
/* Local helper matrix structure.  Could be simplified */
/* Figure out if it's a mtx or tgz file and open it */
{
/* Read in labels if there are any */
struct ccMatrix *v = needMem(sizeof(*v));
struct lineFile *lf = v->lf = lineFileOpen(matrixFile, TRUE);
struct fieldedTable *ft = v->ft = fieldedTableReadTabHeader(lf, NULL, 0);
v->colCount = ft->fieldCount-1;	    // Don't include row label field 
v->colLabels = ft->fields+1;	// +1 to skip over row label field
return v;
}

void ccMatrixClose(struct ccMatrix **pV)
/* Close up ccMatrix */
{
struct ccMatrix *v = *pV;
if (v != NULL)
    {
    fieldedTableFree(&v->ft);
    freez(pV);
    }
}

struct clustering
/* Stuff we need to cluster something.  This is something we might do
 * repeatedly to same input matrix */
    {
    struct clustering *next;
    char *clusterField;	    /* Field to cluster on */
    char *outMatrixFile;    /* Where to put matrix result */
    char *outStatsFile;	    /* Where to put stats result */
    int clusterMetaIx;	    /* Index of cluster field in meta table */
    int *colToCluster;	    /* Maps input matrix column to clustered column */
    int clusterCount;   /* Number of different values in column we are clustering */
    double *clusterTotal;  /* A place to hold totals for each cluster */
    double *clusterGrandTotal;	/* Total over all rows */
    int *clusterElements;  /* A place to hold counts for each cluster */
    double *clusterResults; /* Results after clustering */
    struct hash *clusterSizeHash;   /* Keyed by cluster, int value is elements in cluster */
    char **clusterNames;	    /* Holds name of each cluster */
    int *clusterSizes;	    /* An array that holds size of each cluster */
    boolean excludeZeros; /* If true exclude zero vals from calc */

    /* Things needed by median handling */
    boolean doMedian;	/* If true we calculate median */
    double **clusterSamples; /* An array that holds an array big enough for all vals in cluster. */

    struct dyString **chunkLinesOut; /* parallel output */
    double **chunkSubtotals;	     /* Totals */
    FILE *matrixFile;		     /* serial output */
    };


struct clustering *clusteringNew(char *clusterField, char *outMatrixFile, char *outStatsFile,
    struct fieldedTable *metaTable, struct ccMatrix *v, boolean doMedian, boolean excludeZeros)
/* Make up a new clustering structure */
{
/* Check that all column names in matrix are unique */
int colCount = v->colCount;
char **colLabels = v->colLabels;
struct hash *uniqColHash = hashNew(0);
int colIx;
for (colIx=0; colIx < colCount; colIx = colIx+1)
    {
    char *label = colLabels[colIx];
    if (hashLookup(uniqColHash, label) == NULL)
	hashAdd(uniqColHash, label, NULL);
    else
        errAbort("Duplicated column label %s in input matrix", label);
    }

struct clustering *clustering;
AllocVar(clustering);
clustering->clusterField = clusterField;
clustering->outMatrixFile = outMatrixFile;
clustering->outStatsFile = outStatsFile;
int clusterFieldIx = clustering->clusterMetaIx = fieldedTableMustFindFieldIx(metaTable, clusterField);

/* Make up hash of sample names with cluster name values 
 * and also hash of cluster names with size values */
struct hash *sampleHash = hashNew(0);	/* Keyed by sample value is cluster */
struct hash *clusterSizeHash = clustering->clusterSizeHash = hashNew(0);
struct fieldedRow *fr;
for (fr = metaTable->rowList; fr != NULL; fr = fr->next)
    {
    char **row = fr->row;
    char *sample = row[0];
    if (!hashLookup(uniqColHash, sample))
        errAbort("%s is in %s but not input matrix", sample, metaTable->name);

    hashAdd(sampleHash, sample, row[clusterFieldIx]);
    hashIncInt(clusterSizeHash, row[clusterFieldIx]);
    }

/* Find all uniq cluster names */
struct slName *nameList = NULL;
struct hash *uniqHash = hashNew(0);
for (fr = metaTable->rowList; fr != NULL; fr = fr->next)
    {
    char *cluster = fr->row[clusterFieldIx];
    if (hashLookup(uniqHash, cluster) == NULL)
        {
	slNameAddHead(&nameList, cluster);
	hashAdd(uniqHash, cluster, NULL);
	}
    }
hashFree(&uniqHash);

/* Just alphabetize names for now */
slNameSort(&nameList);
slSort(&nameList, slNameCmpWordsWithEmbeddedNumbers);

/* Make up hash that maps cluster names to cluster ids */
struct hash *clusterIxHash = hashNew(0);	/* Keyed by cluster, no value */
int i;
struct slName *name;
for (name = nameList, i=0; name != NULL; name = name->next, ++i)
    hashAddInt(clusterIxHash, name->name, i);
int clusterCount = clustering->clusterCount = clusterIxHash->elCount;

/* Make up array that holds size of each cluster */
AllocArray(clustering->clusterSizes, clusterCount);
AllocArray(clustering->clusterNames, clusterCount);
for (i = 0, name = nameList; i < clusterCount; ++i, name = name->next)
    {
    clustering->clusterSizes[i] = hashIntVal(clustering->clusterSizeHash, name->name);
    clustering->clusterNames[i] = name->name;
    verbose(2, "clusterSizes[%d] = %d\n", i, clustering->clusterSizes[i]);
    }

if (doMedian)
    {	
    /* Allocate arrays to hold number of samples and all sample vals for each cluster */
    clustering->doMedian = doMedian;
    AllocArray(clustering->clusterSamples, clusterCount);
    int clusterIx;
    for (clusterIx = 0; clusterIx < clusterCount; ++clusterIx)
	{
	double *samples;
	AllocArray(samples, clustering->clusterSizes[clusterIx]);
	clustering->clusterSamples[clusterIx] = samples;
	}
    }

clustering->excludeZeros = excludeZeros;


/* Make up array that has -1 where no cluster available, otherwise output index, also
 * hash up all column labels. */
int *colToCluster = clustering->colToCluster = needHugeMem(colCount * sizeof(colToCluster[0]));
int unclusteredColumns = 0, missCount = 0;
for (colIx=0; colIx < colCount; colIx = colIx+1)
    {
    char *colName = colLabels[colIx];
    char *clusterName = hashFindVal(sampleHash, colName);
    colToCluster[colIx] = -1;
    if (clusterName != NULL)
        {
	int clusterId = hashIntValDefault(clusterIxHash, clusterName, -1);
	colToCluster[colIx] = clusterId;
	if (clusterId == -1)
	    {
	    verbose(3, "%s is in expression matrix but not in sample cluster file", clusterName);
	    ++missCount;
	    }
	}
    else
	unclusteredColumns += 1;
    }
verbose(1, "%d total columns, %d unclustered, %d misses\n", 
    colCount, unclusteredColumns, missCount);

/* Allocate space for results for clustering one line */
clustering->clusterResults = needHugeMem(clusterCount * sizeof(clustering->clusterResults[0]));

/* Allocate a few more things */
clustering->clusterTotal = needMem(clusterCount*sizeof(clustering->clusterTotal[0]));
clustering->clusterGrandTotal = needMem(clusterCount*sizeof(clustering->clusterGrandTotal[0]));
clustering->clusterElements = needMem(clusterCount*sizeof(clustering->clusterElements[0]));

/* Open file - and write out header */
FILE *f = clustering->matrixFile = mustOpen(clustering->outMatrixFile, "w");
if (v->ft->startsSharp)
    fputc('#', f);

/* First field name agrees with first column of matrix */
fputs( v->ft->fields[0],f);

/* Use our clusters for the rest of the names */
for (name = nameList; name != NULL; name = name->next) 
    {
    fputc('\t', f);
    fputs(name->name, f);
    }
fputc('\n', f);

/* Allocate parallel output buffers */
AllocArray(clustering->chunkSubtotals, chunkMaxSize);
for (i=0; i<chunkMaxSize; ++i)
    {
    AllocArray(clustering->chunkSubtotals[i], clusterCount);
    }
AllocArray(clustering->chunkLinesOut, chunkMaxSize);
for (i=0; i<chunkMaxSize; ++i)
    clustering->chunkLinesOut[i] = dyStringNew(colCount * 3);  

/* Clean up and return result */
hashFree(&sampleHash);
hashFree(&clusterIxHash);
return clustering;
}

void outputClusterStats(struct clustering *clustering)
/* Output statistics on each cluster in this clustering. */
{
FILE *f = mustOpen(clustering->outStatsFile, "w");
fprintf(f, "#cluster\tcount\ttotal\n");
int i;
for (i=0; i<clustering->clusterCount; ++i)
    {
    fprintf(f, "%s\t%d\t%g\n", clustering->clusterNames[i], 
	clustering->clusterSizes[i], clustering->clusterGrandTotal[i]);
    }
carefulClose(&f);
}

void clusterRow(struct clustering *clustering, int colCount, char *rowLabel, 
    double *a, double *totalingTemp, int *elementsTemp, struct dyString *out,
    double *subtotals)
/* Process a row in a, outputting in clustering->file */
{
/* Zero out cluster histogram */
double *clusterTotal = totalingTemp;
int *clusterElements = elementsTemp;
int clusterCount = clustering->clusterCount;
int i;
for (i=0; i<clusterCount; ++i)
    {
    clusterTotal[i] = 0.0;
    clusterElements[i] = 0;
    }

/* Loop through rest of row filling in histogram */
int *colToCluster = clustering->colToCluster;
boolean doMedian = clustering->doMedian;
for (i=0; i<colCount; ++i)
    {
    int clusterIx = colToCluster[i];
    if (clusterIx >= 0)
	{
	double val = a[i];
	int valCount = clusterElements[clusterIx];
        if (clustering->excludeZeros && val == 0.0)
            continue;
	clusterElements[clusterIx] = valCount+1;
	clusterTotal[clusterIx] += val;
	if (doMedian)
	    {
	    if (valCount >= clustering->clusterSizes[clusterIx])
		internalErr();
	    clustering->clusterSamples[clusterIx][valCount] = val;
	    }
	}
    }

/* Do output to outstrng and do grand totalling */
dyStringClear(out);
dyStringPrintf(out, "%s", rowLabel);
for (i=0; i<clusterCount; ++i)
    {
    dyStringAppendC(out, '\t');
    double total = clusterTotal[i];
    subtotals[i] += total;
    int elements = clusterElements[i];
    double val;
    if (doMedian)
	{
	val = doubleMedian(elements, clustering->clusterSamples[i]);
	dyStringPrintf(out, "%g", val);
	}
    else
	{
	if (total > 0)
	    {
	    val = total/elements;
	    dyStringPrintf(out, "%g", val);
	    }
	else
	    dyStringAppendC(out, '0');
	}
    }
dyStringAppendC(out, '\n');
}


static void addRowToIndex(FILE *fIndex, char *rowLabel, struct lineFile *lf)
/* Write out info to index file about where this row begins */
{
if (fIndex)
  {
  fprintf(fIndex, "%s", rowLabel);
  fprintf(fIndex, "\t%lld\t%lld\n",  (long long)lineFileTell(lf), (long long)lineFileTellSize(lf));
  }
}

struct lineIoItem
/* This is an item fed to a parallel worker.  It corresponds to a single line of
 * input matrix */
    {
    struct lineIoItem *next;	/* Pointer to next in list */

    /* Information about input file and where we are in it. */
    char *fileName;
    int lineIx;	    /* Index of line in input file */
    long long lineStartOffset;	/* Start offset within file */
    long long lineSize;	/* Size of line */
    long long lineEndOffset;	
    int chunkIx;  /* Index of line in chunk */

    /* Slightly parsed input. */
    struct dyString *rowLabel;	/* Just the row label of input */
    struct dyString *lineIn;     /* Unparsed rest of input line */

    struct clustering *clusteringList;  /* Instructions on how to cluster and output */
    
    /* Temporary values used for calculating output */
    double *vals;		 /* Parse out input matrix values for this line */
    double *totalingTemp;        /* Buffer for parallel computation of totals */
    int *elementsTemp;           /* buffers for parallel computation */
    };

void lineWorker(void *item, void *context)
/* A worker to execute a single column clustering  */
{
struct lineIoItem *lii = item;
struct ccMatrix *v = context;
int xSize = v->colCount;
char *s = lii->lineIn->string;
char *rowLabel = lii->rowLabel->string;;

/* Convert ascii to floating point, with little optimization for the many zeroes we usually see */
int i;
double *vals = lii->vals;
for (i=0; i<xSize; ++i)
    {
    char *str = nextTabWord(&s);
    if (str == NULL)
        errAbort("not enough fields in input matrix line %d", lii->lineIx);
    double val = ((str[0] == '0' && str[1] == 0) ? 0.0 : sqlDouble(str));
    vals[i] = val;
    }

/* Then do the clustering */
struct clustering *clustering;
for (clustering = lii->clusteringList; clustering != NULL; clustering = clustering->next)
      {
      int chunkIx = lii->chunkIx;
      clusterRow(clustering, xSize, rowLabel, 
	    lii->vals, lii->totalingTemp, lii->elementsTemp,
	    clustering->chunkLinesOut[chunkIx], clustering->chunkSubtotals[chunkIx]);
      }
}

void matrixClusterColumns(char *matrixFile, char *metaFile, char *sampleField,
    int outputCount, char **clusterFields, char **outMatrixFiles, char **outStatsFiles,
    char *outputIndex, boolean doMedian, boolean excludeZeros)
/* matrixClusterColumns - Group the columns of a matrix into clusters, and output a matrix 
 * the with same number of rows and generally much fewer columns.. */
{
FILE *fIndex = NULL;
if (outputIndex)
    fIndex = mustOpen(outputIndex, "w");

/* Load up metadata and make sure we have all of the cluster fields we need 
 * and fill out array of clusterIx corresponding to clusterFields in metaFile. */
struct fieldedTable *metaTable = fieldedTableFromTabFile(metaFile, metaFile, NULL, 0);
struct hash *metaHash = fieldedTableIndex(metaTable, sampleField);
verbose(1, "Read %d rows from %s\n", metaHash->elCount, metaFile);

/* Load up input matrix first line at least */
struct ccMatrix *v = ccMatrixOpen(matrixFile);
verbose(1, "matrix %s has %d fields\n", matrixFile, v->colCount);

/* Create a clustering for each output and find index in metaTable for each. */
struct clustering *clusteringList = NULL, *clustering;
int i;
for (i=0; i<outputCount; ++i)
    {
    clustering = clusteringNew(clusterFields[i], outMatrixFiles[i], outStatsFiles[i], 
			metaTable, v, doMedian, excludeZeros);
    slAddTail(&clusteringList, clustering);
    }

/* Set up buffers for pthread workers */
struct lineIoItem chunks[chunkMaxSize];
for (i=0; i<chunkMaxSize; ++i)
    {
    struct lineIoItem *chunk = &chunks[i];
    chunk->fileName = matrixFile;
    chunk->clusteringList = clusteringList;
    chunk->chunkIx = i;
    chunk->lineIn = dyStringNew(0);
    chunk->rowLabel = dyStringNew(0);
    AllocArray(chunk->vals, v->colCount);
    AllocArray(chunk->totalingTemp, v->colCount);
    AllocArray(chunk->elementsTemp, v->colCount);
    }

/* Chug through big matrix a row at a time clustering */
dotForUserInit(1);

boolean atEof = FALSE;
struct lineFile *lf = v->lf;
while (!atEof)
    {
    /* Read a chunk of lines of the file */
    struct lineIoItem *chunkList = NULL, *chunk;
    int chunkSize;
    for (chunkSize = 0; chunkSize < chunkMaxSize; chunkSize += 1)
	{
	char *line;
	if (!lineFileNextReal(lf, &line))
	   {
	   atEof = TRUE;
	   break;
	   }
	chunk = &chunks[chunkSize];
	chunk->lineIx = lf->lineIx;
	char *rowLabel = nextTabWord(&line);
	addRowToIndex(fIndex, rowLabel, lf);
	dyStringClear(chunk->rowLabel);
	dyStringAppend(chunk->rowLabel, rowLabel);
	dyStringClear(chunk->lineIn);
	dyStringAppend(chunk->lineIn, line);
	slAddHead(&chunkList, chunk);
	}
    slReverse(&chunkList);

    /* Calculate strings to print in parallel */
    pthreadDoList(clThreadCount, chunkList, lineWorker, v);

    /* Do the actual file writes in serial and add subtotals to grand total */
    for (chunk = chunkList; chunk != NULL; chunk = chunk->next)
	{
	struct clustering *clustering;
	for (clustering = clusteringList; clustering != NULL; clustering = clustering->next)
	    {
	    fputs(clustering->chunkLinesOut[chunk->chunkIx]->string, clustering->matrixFile);

	    /* Add subtotals calculated in parallel to grandTotal. */
	    int i;
	    double *grandTotal = clustering->clusterGrandTotal;
	    double *subtotal = clustering->chunkSubtotals[chunk->chunkIx];
	    for (i=0; i<clustering->clusterCount; ++i)
		{
	        grandTotal[i] += subtotal[i];
		subtotal[i] = 0;
		}
	    }
	}
    dotForUser();
    }


dotForUserEnd();

/* Do stats and close files */
for (clustering = clusteringList; clustering != NULL; clustering = clustering->next)
    {
    outputClusterStats(clustering);
    carefulClose(&clustering->matrixFile);
    }

ccMatrixClose(&v);
carefulClose(&fIndex);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
char *makeIndex = optionVal("makeIndex", NULL);
int minArgc = 6;
if (argc < minArgc || ((argc-minArgc)%3)!=0)  // Force minimum, even number
    usage();
int outputCount = 1 + (argc-minArgc)/3;	      // Add one since at minimum have 1
char *clusterFields[outputCount], *outMatrixFiles[outputCount], *outStatsFiles[outputCount];
int i;
char **triples = argv + minArgc - 3;
for (i=0; i<outputCount; ++i)
    {
    clusterFields[i] = triples[0];
    outMatrixFiles[i] = triples[1];
    outStatsFiles[i] = triples[2];
    triples += 3;
    }
matrixClusterColumns(argv[1], argv[2], argv[3], 
    outputCount, clusterFields, outMatrixFiles, outStatsFiles, makeIndex, optionExists("median"), optionExists("excludeZeros"));
return 0;
}
