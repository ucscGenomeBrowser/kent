#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "obscure.h"
#include "sqlNum.h"
#include "vMatrix.h"
#include "pthreadDoList.h"

double *vRowMatrixNextRow(struct vRowMatrix *v, char **retLabel)
/* Return next row or NULL at end.  Just mostly wraps callback */
{
double *ret = v->nextRow(v, retLabel);
if (ret != NULL)
    v->y += 1;
return ret;
}

void vRowMatrixFree(struct vRowMatrix **pv)
/* Free up vRowMatrix including doing the free callback */
{
struct vRowMatrix *v = *pv;
if (v != NULL)
    {
    v->free(v);
    freez(pv);
    }
}

struct vRowMatrix *vRowMatrixNewEmpty(int xSize, char **columnLabels, char *centerLabel)
/* Allocate vRowMatrix but without methods or data filled in */
{
struct vRowMatrix *v;
AllocVar(v);
v->xSize = xSize;
v->columnLabels = columnLabels;
v->centerLabel = centerLabel;
return v;
}

void vRowMatrixSaveAsTsv(struct vRowMatrix *v, char *fileName)
/* Save sparseRowMatrix to tab-sep-file as expanded non-sparse */
{
FILE *f = mustOpen(fileName, "w");
int xSize = v->xSize;
fprintf(f, "\t");	// Empty corner label
writeTsvRow(f, xSize, v->columnLabels);
char *label;
double *row;
while ((row = vRowMatrixNextRow(v, &label)) != NULL)
    {
    fprintf(f, "%s", label);
    int i;
    for (i=0; i<xSize; ++i)
        {
	double val = row[i];
	if (val == 0.0)
	    fprintf(f, "\t0");
	else
	    fprintf(f, "\t%g", val);
	}
    fprintf(f, "\n");
    }
carefulClose(&f);
}

/* Stuff to provide minimal memMatrix support */

struct memMatrix *memMatrixNewEmpty()
/* Return new barely initialized memMatrix */
{
struct memMatrix *m;
AllocVar(m);
m->lm = lmInit(0);
return m;
}

void memMatrixAddNewFirstRowInfo(struct memMatrix *m, char **firstRow)
{
/* Make a local memory copy of the xLabels */
struct lm *lm = m->lm;
m->centerLabel = lmCloneString(lm, firstRow[0]);
m->xLabels = lmCloneRow(lm, firstRow+1, m->xSize);
}

void memMatrixFree(struct memMatrix **pMatrix)
/* Free up memory matrix */
{
struct memMatrix *m = *pMatrix;
if (m != NULL)
    {
    lmCleanup(&m->lm);
    }
}

struct memMatrix *vRowMatrixToMem(struct vRowMatrix *v)
/* Read entire matrix into memory - in case we need real random access  */
{
/* Create memMatrix bare bones and set xSize */
struct memMatrix *m = memMatrixNewEmpty();
int xSize = m->xSize = v->xSize;

/* Make a local memory copy of the xLabels */
struct lm *lm = m->lm;
m->centerLabel = lmCloneString(lm, v->centerLabel);
m->xLabels = lmCloneRow(lm, v->columnLabels, xSize);

/* Loop through all rows, fetching row data and label 
 * into two lists */
struct slRef *dataRefList = NULL, *dataRef;
struct slName *nameList = NULL, *name;
char *label;
double *row;
while ((row = vRowMatrixNextRow(v, &label)) != NULL)
    {
    name = lmSlName(lm, label);
    slAddHead(&nameList, name);
    lmRefAdd(lm, &dataRefList, lmCloneMem(lm, row, xSize * sizeof(*row)));
    }
slReverse(&nameList);
slReverse(&dataRefList);

/* Allocate arrays for row starts and y labels */
int ySize = m->ySize = slCount(nameList);
double **rows = m->rows = lmAlloc(lm, ySize * sizeof(m->rows[0]));
char **yLabels = m->yLabels = lmAlloc(lm, ySize * sizeof(m->yLabels[0]));

/* Copy from lists into arrays */
dataRef = dataRefList;
name = nameList;
int i;
for (i=0; i<ySize; ++i)
    {
    yLabels[i] = name->name;
    rows[i] = dataRef->val;
    name = name->next;
    dataRef = dataRef->next;
    }

/* Go home.  No clean up of small amount of list crud in m->lm until m is freed */
return m;
}

/* To help make this virtual mess a little useful, implement it for tab-separated-files
 * even inside this routine.  From here on down you could use as a template for a new
 * type of vMatrix */

struct tsvMatrix
/* Helper structure for tab-sep-file matrixes */
    {
    struct tsvMatrix *next;
    int rowSize;	    /* How big is row */
    char **columnLabels;    /* Our column names */
    char **stringRow;	    /* Unparsed string row */
    double *row;	    /* string row converted to double  */
    struct lineFile *lf;    /* File we are working on. */
    char *centerLabel;	    /* First word in file */
    };


struct tsvMatrix *tsvMatrixNew(char *fileName)
/* Open up fileName and put tsvMatrix around it, reading first line */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
if (lf == NULL)
    return NULL;
char *line;
lineFileNeedNext(lf, &line, NULL);
char *centerLabel = nextTabWord(&line);

int rowSize = chopByChar(line, '\t', NULL, 0);
if (rowSize < 1)
    errAbort("%s doesn't look like a tsv matrix file", fileName);

struct tsvMatrix *m;
AllocVar(m);
m->rowSize = rowSize;
AllocArray(m->stringRow, rowSize+1);
AllocArray(m->row, rowSize);
m->centerLabel = cloneString(centerLabel);

/* ALlocate column labels and copy outside of this line's space */
AllocArray(m->columnLabels, rowSize);
chopByChar(line, '\t', m->columnLabels, rowSize);
int i;
for (i=0; i<rowSize; ++i)
    {
    m->columnLabels[i] = cloneString(m->columnLabels[i]);
    }
m->lf = lf;
return m;
}

void tsvMatrixFree(struct tsvMatrix **pm)
/* Free up resource associated with tsvMatrix */
{
struct tsvMatrix *m = *pm;
if (m != NULL)
    {
    if (m->columnLabels != NULL)
	{
	int i;
	for (i=0; i<m->rowSize; ++i)
	   freeMem(m->columnLabels[i]);
	freeMem(m->columnLabels);
	}
    freeMem(m->centerLabel);
    freeMem(m->stringRow);
    freeMem(m->row);
    lineFileClose(&m->lf);
    freez(pm);
    }
}

static void asciiToDoubleRow(int rowSize, char **inRow, double *outRow)
/* Do relatively fast conversion of an array of ascii floating point numbers to array of double. */
{
int i;
for (i=0; i<rowSize; ++i)
    {
    char *s = inRow[i];
    if (s[0] == '0' && s[1] == 0)   // "0" - so many of these
        outRow[i] = 0.0;
    else
        outRow[i] = sqlDouble(s);
    }
}


double *tsvMatrixNextRow(struct tsvMatrix *m, char **retLabel)
/* tsvMatrix implementation of next row.  The result is only good
 * until next call to tsvMatrixNextRow. */
{
int rowSize = m->rowSize;
char **inRow = m->stringRow;
if (!lineFileNextRowTab(m->lf,inRow, rowSize+1))
    return NULL;
double *outRow = m->row;
asciiToDoubleRow(rowSize, inRow+1, outRow);
*retLabel = inRow[0];
return outRow;
}

double *vTsvMatrixNextRow(struct vRowMatrix *v, char **retLabel)
/* tsvMatrix implementation of next row */
{
struct tsvMatrix *m = v->vData;
return tsvMatrixNextRow(m, retLabel);
}

void vTsvMatrixFree(struct vRowMatrix *v)
/* Free up a mem-oriented vRowMatrix */ 
{
struct tsvMatrix *m = v->vData;
tsvMatrixFree(&m);
}

struct vRowMatrix *vRowMatrixOnTsv(char *fileName)
/* Return a vRowMatrix on a tsv file with first column and row as labels */
{
struct tsvMatrix *m = tsvMatrixNew(fileName);
struct vRowMatrix *v = vRowMatrixNewEmpty(m->rowSize, m->columnLabels, m->centerLabel);
v->vData = m;
v->nextRow = vTsvMatrixNextRow;
v->free = vTsvMatrixFree;
return v;
}

static struct memMatrix *memMatrixFromTsvSerial(char *fileName)
/* Read all of matrix from tsv file into memory */
{
struct vRowMatrix *v = vRowMatrixOnTsv(fileName);
struct memMatrix *m = vRowMatrixToMem(v);
vRowMatrixFree(&v);
return m;
}

struct unpackTsvJob
/* Help unpack a tsv into a matrix in parallel */
    {
    struct unpackTsvJob *next;
    char *line;	    /* Unparsed line */
    int lineIx;	    /* Index of line in file */
    int rowSize;    /* Size of row */
    double *unpacked;  /* Unpacked into doubles */
    char *rowLabel;
    };

void rowUnpackWorker(void *item, void *context)
/* A worker to execute a single column job */
{
struct unpackTsvJob *job = item;
char *line = job->line;	    /* We can destructively parse this, whee! */
double *unpacked = job->unpacked;
job->rowLabel = nextTabWord(&line);
int i;
for (i=0; i<job->rowSize; ++i)
    {
    char *s = nextTabWord(&line);
    if (s == NULL)
       errAbort("Short line %d\n", job->lineIx);
    unpacked[i] = sqlDouble(s);
    }
}

struct memMatrix *memMatrixFromTsvPara(char *fileName, int threadCount)
/* Read all of matrix from tsv file into memory */
{
if (threadCount <= 1)
    return memMatrixFromTsvSerial(fileName);

/* This essentially reads and parses first row */
struct tsvMatrix *tMat = tsvMatrixNew(fileName);
struct lineFile *lf = tMat->lf;
int xSize = tMat->rowSize;

/* Get empty husk of output matrix */
struct memMatrix *m = memMatrixNewEmpty();
struct lm *lm = m->lm;

/* Fill what we can from what we've read */
m->xSize = xSize;
m->centerLabel = lmCloneString(lm, tMat->centerLabel);
m->xLabels = lmCloneRow(lm, tMat->columnLabels, xSize);


struct unpackTsvJob *jobList = NULL, *job;
char *line;
int ySize = 0;
while (lineFileNextReal(lf, &line))
    {
    lmAllocVar(lm, job);
    job->line = lmCloneString(lm, line);
    job->lineIx = lf->lineIx;
    job->rowSize = xSize;
    lmAllocArray(lm, job->unpacked, job->rowSize);
    ++ySize;
    slAddHead(&jobList, job);
    }
slReverse(&jobList);
assert(slCount(jobList) == ySize);

/* Fill out more of output matrix now that we know how big it is */
m->ySize = ySize;
lmAllocArray(lm, m->yLabels, ySize);
lmAllocArray(lm, m->rows, ySize);

/* Share rows with items in jobList */
int y;
for (y=0, job = jobList; job != NULL; job = job->next, ++y)
    {
    m->rows[y] = job->unpacked;
    }

pthreadDoList(threadCount, jobList, rowUnpackWorker, lm);

for (y=0, job = jobList; job != NULL; job = job->next, ++y)
    m->yLabels[y] = job->rowLabel;

tsvMatrixFree(&tMat);
return m;
}

struct memMatrix *memMatrixFromTsv(char *fileName)
/* Read all of matrix from tsv file into memory */
{
return memMatrixFromTsvPara(fileName, 50);
}

#ifdef OLDWAY
void memMatrixToTsvSerial(struct memMatrix *m, char *fileName)
/* Output memory matrix to tab-sep file with labels */
{
FILE *f = mustOpen(fileName, "w");

/* Print label row */
fprintf(f, "%s", m->centerLabel);
int x, xSize = m->xSize;
for (x=0; x<xSize; ++x)
     fprintf(f, "\t%s", m->xLabels[x]);
fprintf(f, "\n");

/* Print rest */
/* Now output going through row by row */
int y, ySize = m->ySize;
for (y=0; y<ySize; ++y)
   {
   fprintf(f, "%s", m->yLabels[y]);
   double *row = m->rows[y];
   for (x=0; x<xSize; ++x)
       {
       double val = row[x];
       if (val == 0.0)
	   fputs("\t0", f);  // Common special case in these sparse matrices
       else
	   fprintf(f, "\t%g", row[x]);
       }
   fprintf(f, "\n");
   }
carefulClose(&f);
}
#endif /* OLDWAY */

static void printRowToDy(struct dyString *dy, struct memMatrix *m, int yOff)
/* Print out yOff's row to dy */
{
dyStringClear(dy);
dyStringPrintf(dy, "%s", m->yLabels[yOff]);
double *row = m->rows[yOff];
int x, xSize = m->xSize;
for (x=0; x<xSize; ++x)
   {
   double val = row[x];
   if (val == 0.0)
       dyStringAppend(dy, "\t0");  // Common special case in these sparse matrices
   else
       dyStringPrintf(dy, "\t%g", row[x]);
   }
dyStringPrintf(dy, "\n");
}

struct writeJob
/* Multithreaded job that handles a column */
    {
    struct writeJob *next;
    int y;	/* Index of row within matrix */
    struct dyString *dy; /* Where to put output */
    };

static void writeWorker(void *item, void *context)
/* A worker to execute a single column job */
{
struct writeJob *job = item;
struct memMatrix *m = context;
printRowToDy(job->dy, m, job->y);
}

static void memMatrixToTsvParallel(struct memMatrix *m, char *fileName, int threadCount)
/* Output memory matrix to tab-sep file with labels in parallel */
{
FILE *f = mustOpen(fileName, "w");

/* Print label row */
fprintf(f, "%s", m->centerLabel);
int x, xSize = m->xSize;
for (x=0; x<xSize; ++x)
     fprintf(f, "\t%s", m->xLabels[x]);
fprintf(f, "\n");

/* Set up chunk buffers */
int maxChunkSize = 10*threadCount;  // The 10 here is pretty arbitrary.  2 might be as good
struct writeJob chunkJobs[maxChunkSize];
int i;
for (i=0; i<maxChunkSize; ++i)
    {
    struct dyString *dy = dyStringNew(0);
    chunkJobs[i].dy = dy;
    }

/* Print rest a chunk at a time. */
int y=0, ySize = m->ySize;
while (y < ySize)
   {
   int chunkSize = ySize - y;
   if (chunkSize > maxChunkSize)
       chunkSize = maxChunkSize;

    struct writeJob *jobList = NULL;
    for (i=0; i<chunkSize; ++i)
	{
	struct writeJob *job = &chunkJobs[i];
	job->y = y + i;
	slAddHead(&jobList, job);
	}
    slReverse(&jobList);

    /* Calculate strings to print in parallel */
    pthreadDoList(threadCount, jobList, writeWorker, m);

    /* Do the actual file writes in serial */
    struct writeJob *job;
    for (job = jobList; job != NULL; job = job->next)
	fputs(job->dy->string, f);

   y += chunkSize;
   }
carefulClose(&f);

/* Clean up chunk buffers */
for (i=0; i<maxChunkSize; ++i)
    {
    dyStringFree(&chunkJobs[i].dy);
    }
}

void memMatrixToTsv(struct memMatrix *m, char *fileName)
/* Output memory matrix to tab-sep file with labels */
{
memMatrixToTsvParallel(m, fileName, 50);
}
