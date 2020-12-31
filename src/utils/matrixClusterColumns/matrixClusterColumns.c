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
  "   matrixClusterColumns input meta.tsv sample cluster output.tsv [clusterCol output2 ... ]\n"
  "where:\n"
  "   input is a file either in tsv format or in mtx (matrix mart sorted) format\n"
  "   meta.tsv is a table where the first row is field labels and the first column is sample ids\n"
  "   sample is the name of the field with the sample (often single cell) id's\n"
  "   cluster is the name of the field with the cluster names\n"
  "You can produce multiple clusterings in the same pass through the input matrix by specifying\n"
  "additional cluster/output pairs in the command line.\n"
  "options:\n"
  "   -columnLabels=file.txt - a file with a line for each column, required for mtx inputs\n"
  "   -rowLabels=file.txt - a file with a label for each row, also required for mtx inputs\n"
  "   -makeIndex=index.tsv - output index tsv file with <matrix-col1><input-file-pos><line-len>\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"columnLabels", OPTION_STRING},
   {"rowLabels", OPTION_STRING},
   {"makeIndex", OPTION_STRING},
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
    char *outputFile;	    /* Where to put result */
    int clusterMetaIx;	    /* Index of cluster field in meta table */
    int *colToCluster;	    /* Maps input matrix column to clustered column */
    int clusterCount;   /* Number of different values in column we are clustering */
    double *clusterTotal;  /* A place to hold totals for each cluster */
    int *clusterElements;  /* A place to hold counts for each cluster */
    double *clusterResults; /* Results after clustering */
    FILE *file;		    /* output */
    };


struct clustering *clusteringNew(char *clusterField, char *outputFile, 
    struct fieldedTable *metaTable, int sampleFieldIx, struct vMatrix *v)
/* Make up a new clustering job structure */
{
struct clustering *job;
AllocVar(job);
job->clusterField = clusterField;
job->outputFile = outputFile;
int clusterFieldIx = job->clusterMetaIx = fieldedTableMustFindFieldIx(metaTable, clusterField);

/* Make up hash of sample names with cluster values */
struct hash *sampleHash = hashNew(0);	/* Keyed by sample value is cluster */
struct fieldedRow *fr;
for (fr = metaTable->rowList; fr != NULL; fr = fr->next)
    {
    char **row = fr->row;
    hashAdd(sampleHash, row[sampleFieldIx], row[clusterFieldIx]);
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
struct hash *clusterHash = hashNew(0);	/* Keyed by cluster, no value */
struct slName *name;
int i;
for (name = nameList, i=0; name != NULL; name = name->next, ++i)
    hashAddInt(clusterHash, name->name, i);
int clusterCount = job->clusterCount = clusterHash->elCount;

/* Make up array that has -1 where no cluster available, otherwise output index */
int colCount = v->colCount;
int *colToCluster = job->colToCluster = needHugeMem(colCount * sizeof(colToCluster[0]));
int colIx;
int unclusteredColumns = 0, missCount = 0;
for (colIx=0; colIx < colCount; colIx = colIx+1)
    {
    char *colName = v->colLabels[colIx];
    char *clusterName = hashFindVal(sampleHash, colName);
    colToCluster[colIx] = -1;
    if (clusterName != NULL)
        {
	int clusterId = hashIntValDefault(clusterHash, clusterName, -1);
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
job->clusterElements = needMem(clusterCount*sizeof(job->clusterElements[0]));

/* Open file - and write out header */
FILE *f = job->file = mustOpen(job->outputFile, "w");
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
hashFree(&clusterHash);
return job;
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
for (i=0; i<colCount; ++i)
    {
    int clusterIx = colToCluster[i];
    if (clusterIx >= 0)
	{
	double val = a[i];
	int valCount = clusterElements[clusterIx];
	clusterElements[clusterIx] = valCount+1;
#ifdef SOON
	if (doMedian)
	    {
	    if (valCount >= clusterSize[clusterIx])
		internalErr();
	    clusterSamples[clusterIx][valCount] = val;
	    }
	else
#endif /* SOON */
	    clusterTotal[clusterIx] += val;
	}
    }

/* Do output to file */
FILE *f = job->file;
fprintf(f, "%s", v->rowLabel->string);
for (i=0; i<clusterCount; ++i)
    {
    fprintf(f, "\t");
    double val;
#ifdef SOON
    if (doMedian)
	val = doubleMedian(clusterElements[i], clusterSamples[i]);
    else
#endif /* SOON */
	val = clusterTotal[i]/clusterElements[i];
    fprintf(f, "%g", val);
    }
	
	/* Data file offset info */

fprintf(f, "\n");
}




void matrixClusterColumns(char *matrixFile, char *metaFile, char *sampleField,
    int outputCount, char **clusterFields, char **outputFiles, char *outputIndex)
/* matrixClusterColumns - Group the columns of a matrix into clusters, and output a matrix 
 * the with same number of rows and generally much fewer columns.. */
{
FILE *fIndex = NULL;
if (outputIndex)
    fIndex = mustOpen(outputIndex, "w");

/* Load up metadata and make sure we have all of the cluster fields we need 
 * and fill out array of clusterIx corresponding to clusterFields in metaFile. */
struct fieldedTable *metaTable = fieldedTableFromTabFile(metaFile, metaFile, NULL, 0);
int sampleFieldIx = fieldedTableMustFindFieldIx(metaTable, sampleField);
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
    job = clusteringNew(clusterFields[i], outputFiles[i], metaTable, sampleFieldIx, v);
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

/* Close files */
for (job = jobList; job != NULL; job = job->next)
    carefulClose(&job->file);

vMatrixClose(&v);
carefulClose(&fIndex);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
char *makeIndex = optionVal("makeIndex", NULL);
int minArgc = 6;
if (argc < minArgc || ((argc-minArgc)%2)!=0)  // Force minimum, even number
    usage();
int outputCount = 1 + (argc-minArgc)/2;	      // Add one since at minimum have 1
char *clusterFields[outputCount], *outputFiles[outputCount];
int i;
char **pair = argv + minArgc - 2;
for (i=0; i<outputCount; ++i)
    {
    clusterFields[i] = pair[0];
    outputFiles[i] = pair[1];
    pair += 2;
    }
matrixClusterColumns(argv[1], argv[2], argv[3], outputCount, clusterFields, outputFiles, makeIndex);
return 0;
}
