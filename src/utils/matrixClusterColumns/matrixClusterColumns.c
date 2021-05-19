/* matrixClusterColumns - Group the columns of a matrix into clusters, and output a matrix 
 * the with same number of rows and generally much fewer columns.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "fieldedTable.h"
#include "sqlNum.h"

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
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"makeIndex", OPTION_STRING},
   {"median", OPTION_BOOLEAN},
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

struct vMatrix
/* Virtual matrix - little wrapper around a fielded table/lineFile combination
 * to help manage row-at-a-time access. */
    {
    struct vMatrix *next;

        /* kind of private fields */
    struct lineFile *lf;	    // Line file for tab-sep case
    struct fieldedTable *ft;	    // fielded table if a tab-sep file
    char **rowAsStrings;		    // Our row as an array of strings

	/* From below are fields that yu can read but not change */
    double *rowAsNum;		    // Our fielded table result array of numbers
    char **rowLabels;		    // If non-NULL array of labels for each row
    char **colLabels;		    // Array of labels for each column
    int colCount;		    // Number of columns in a row
    int curRow;			    // Current row we are processing
    struct dyString *rowLabel;	    // Label associated with this line
    };

struct vMatrix *vMatrixOpen(char *matrixFile, char *columnFile, char *rowFile)
/* Figure out if it's a mtx or tgz file and open it */
{
/* Read in labels if there are any */
char **columnLabels = NULL, **rowLabels = NULL;
int columnCount = 0, rowCount = 0;
if (columnFile != NULL)
    readLineArray(columnFile, &columnCount, &columnLabels);
if (rowFile != NULL)
    readLineArray(rowFile, &rowCount, &rowLabels);

struct vMatrix *v = needMem(sizeof(*v));
struct lineFile *lf = v->lf = lineFileOpen(matrixFile, TRUE);
struct fieldedTable *ft = v->ft = fieldedTableReadTabHeader(lf, NULL, 0);
v->colCount = ft->fieldCount-1;	    // Don't include row label field 
v->rowAsNum = needHugeMem(v->colCount * sizeof(v->rowAsNum[0]));
v->rowAsStrings = needHugeMem(ft->fieldCount * sizeof(v->rowAsStrings[0]));
if (columnLabels == NULL)
    columnLabels = ft->fields+1;	// +1 to skip over row label field
v->rowLabels = rowLabels;
v->colLabels = columnLabels;
v->rowLabel = dyStringNew(32);
return v;
}

void vMatrixClose(struct vMatrix **pV)
/* Close up vMatrix */
{
struct vMatrix *v = *pV;
if (v != NULL)
    {
    fieldedTableFree(&v->ft);
    freeMem(v->rowAsNum);
    freeMem(v->rowAsStrings);
    dyStringFree(&v->rowLabel);
    freez(pV);
    }
}

double *vMatrixNextRow(struct vMatrix *v)
/* Return next row of matrix or NULL if at end of file */
{
int colCount = v->colCount;
if (!lineFileNextRow(v->lf, v->rowAsStrings, colCount+1))
    return NULL;

/* Save away the row label for later. */
char *rowLabel;
if (v->rowLabels)
    rowLabel = v->rowLabels[v->curRow];
else
    rowLabel = v->rowAsStrings[0];
dyStringClear(v->rowLabel);
dyStringAppend(v->rowLabel, rowLabel);

/* Convert ascii to floating point, with little optimization for the many zeroes we usually see */
int i;
for (i=1; i<=colCount; ++i)
    {
    char *str = v->rowAsStrings[i];
    double val = ((str[0] == '0' && str[1] == 0) ? 0.0 : sqlDouble(str));
    v->rowAsNum[i-1] = val;
    }
v->curRow += 1;
return v->rowAsNum;
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

    /* Things needed by median handling */
    boolean doMedian;	/* If true we calculate median */
    double **clusterSamples; /* An array that holds an array big enough for all vals in cluster. */

    FILE *matrixFile;		    /* output */
    };


struct clustering *clusteringNew(char *clusterField, char *outMatrixFile, char *outStatsFile,
    struct fieldedTable *metaTable, struct vMatrix *v, boolean doMedian)
/* Make up a new clustering job structure */
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

struct clustering *job;
AllocVar(job);
job->clusterField = clusterField;
job->outMatrixFile = outMatrixFile;
job->outStatsFile = outStatsFile;
int clusterFieldIx = job->clusterMetaIx = fieldedTableMustFindFieldIx(metaTable, clusterField);

/* Make up hash of sample names with cluster name values 
 * and also hash of cluster names with size values */
struct hash *sampleHash = hashNew(0);	/* Keyed by sample value is cluster */
struct hash *clusterSizeHash = job->clusterSizeHash = hashNew(0);
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

/* Make up hash that maps cluster names to cluster ids */
struct hash *clusterIxHash = hashNew(0);	/* Keyed by cluster, no value */
int i;
struct slName *name;
for (name = nameList, i=0; name != NULL; name = name->next, ++i)
    hashAddInt(clusterIxHash, name->name, i);
int clusterCount = job->clusterCount = clusterIxHash->elCount;

/* Make up array that holds size of each cluster */
AllocArray(job->clusterSizes, clusterCount);
AllocArray(job->clusterNames, clusterCount);
for (i = 0, name = nameList; i < clusterCount; ++i, name = name->next)
    {
    job->clusterSizes[i] = hashIntVal(job->clusterSizeHash, name->name);
    job->clusterNames[i] = name->name;
    verbose(2, "clusterSizes[%d] = %d\n", i, job->clusterSizes[i]);
    }

if (doMedian)
    {	
    /* Allocate arrays to hold number of samples and all sample vals for each cluster */
    job->doMedian = doMedian;
    AllocArray(job->clusterSamples, clusterCount);
    int clusterIx;
    for (clusterIx = 0; clusterIx < clusterCount; ++clusterIx)
	{
	double *samples;
	AllocArray(samples, job->clusterSizes[clusterIx]);
	job->clusterSamples[clusterIx] = samples;
	}
    }


/* Make up array that has -1 where no cluster available, otherwise output index, also
 * hash up all column labels. */
int *colToCluster = job->colToCluster = needHugeMem(colCount * sizeof(colToCluster[0]));
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
job->clusterResults = needHugeMem(clusterCount * sizeof(job->clusterResults[0]));

/* Allocate a few more things */
job->clusterTotal = needMem(clusterCount*sizeof(job->clusterTotal[0]));
job->clusterGrandTotal = needMem(clusterCount*sizeof(job->clusterGrandTotal[0]));
job->clusterElements = needMem(clusterCount*sizeof(job->clusterElements[0]));

/* Open file - and write out header */
FILE *f = job->matrixFile = mustOpen(job->outMatrixFile, "w");
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



/* Clean up and return result */
hashFree(&sampleHash);
hashFree(&clusterIxHash);
return job;
}

void outputClusterStats(struct clustering *job)
/* Output statistics on each cluster in this job. */
{
FILE *f = mustOpen(job->outStatsFile, "w");
fprintf(f, "#cluster\tcount\ttotal\n");
int i;
for (i=0; i<job->clusterCount; ++i)
    {
    fprintf(f, "%s\t%d\t%g\n", job->clusterNames[i], 
	job->clusterElements[i], job->clusterGrandTotal[i]);
    }
carefulClose(&f);
}

void clusterRow(struct clustering *job, struct vMatrix *v, double *a)
/* Process a row in a, outputting in job->file */
{
/* Zero out cluster histogram */
double *clusterTotal = job->clusterTotal;
int *clusterElements = job->clusterElements;
int clusterCount = job->clusterCount;
int i;
for (i=0; i<clusterCount; ++i)
    {
    clusterTotal[i] = 0.0;
    clusterElements[i] = 0;
    }

/* Loop through rest of row filling in histogram */
int colCount = v->colCount;
int *colToCluster = job->colToCluster;
boolean doMedian = job->doMedian;
for (i=0; i<colCount; ++i)
    {
    int clusterIx = colToCluster[i];
    if (clusterIx >= 0)
	{
	double val = a[i];
	int valCount = clusterElements[clusterIx];
	clusterElements[clusterIx] = valCount+1;
	clusterTotal[clusterIx] += val;
	if (doMedian)
	    {
	    if (valCount >= job->clusterSizes[clusterIx])
		internalErr();
	    job->clusterSamples[clusterIx][valCount] = val;
	    }
	}
    }

/* Do output to file and grand totalling */
FILE *f = job->matrixFile;
fprintf(f, "%s", v->rowLabel->string);
double *grandTotal = job->clusterGrandTotal;
for (i=0; i<clusterCount; ++i)
    {
    fprintf(f, "\t");
    double total = clusterTotal[i];
    grandTotal[i] += total;
    double val;
    if (doMedian)
	val = doubleMedian(clusterElements[i], job->clusterSamples[i]);
    else
	val = total/clusterElements[i];
    fprintf(f, "%g", val);
    }
	
fprintf(f, "\n");
}




void matrixClusterColumns(char *matrixFile, char *metaFile, char *sampleField,
    int outputCount, char **clusterFields, char **outMatrixFiles, char **outStatsFiles,
    char *outputIndex, boolean doMedian)
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

/* Load up input matrix and labels */
char *columnFile = optionVal("columnLabels", NULL);
char *rowFile = optionVal("rowLabels", NULL);
struct vMatrix *v = vMatrixOpen(matrixFile, columnFile, rowFile);
verbose(1, "matrix %s has %d fields\n", matrixFile, v->colCount);

/* Create a clustering for each output and find index in metaTable for each. */
struct clustering *jobList = NULL, *job;
int i;
for (i=0; i<outputCount; ++i)
    {
    job = clusteringNew(clusterFields[i], outMatrixFiles[i], outStatsFiles[i], 
			metaTable, v, doMedian);
    slAddTail(&jobList, job);
    }


/* Chug through big matrix a row at a time clustering */
dotForUserInit(100);
for (;;)
  {
  double *a = vMatrixNextRow(v);
  if (a == NULL)
       break;
  if (fIndex)
      {
      fprintf(fIndex, "%s", v->rowLabel->string);
      struct lineFile *lf = v->lf;
      fprintf(fIndex, "\t%lld\t%lld\n",  (long long)lineFileTell(lf), (long long)lineFileTellSize(lf));
      }
  for (job = jobList; job != NULL; job = job->next)
      clusterRow(job, v, a);
  dotForUser();
  }
fputc('\n', stderr);  // Cover last dotForUser

/* Do stats and close files */
for (job = jobList; job != NULL; job = job->next)
    {
    outputClusterStats(job);
    carefulClose(&job->matrixFile);
    }

vMatrixClose(&v);
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
    outputCount, clusterFields, outMatrixFiles, outStatsFiles, makeIndex, optionExists("median"));
return 0;
}
