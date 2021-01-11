#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "obscure.h"
#include "sqlNum.h"
#include "vMatrix.h"

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
/* Return new all zero memory matrix */
{
struct memMatrix *m;
AllocVar(m);
m->lm = lmInit(0);
return m;
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

double *vTsvMatrixNextRow(struct vRowMatrix *v, char **retLabel)
/* memMatrix implementation of next row */
{
struct tsvMatrix *m = v->vData;
int rowSize = m->rowSize;
char **inRow = m->stringRow;
if (!lineFileNextRowTab(m->lf,inRow, rowSize+1))
    return NULL;
double *outRow = m->row;
int i;
for (i=0; i<rowSize; ++i)
    {
    char *s = inRow[i+1];
    if (s[0] == '0' && s[1] == 0)   // "0" - so many of these
        outRow[i] = 0.0;
    else
        outRow[i] = sqlDouble(s);
    }
if (retLabel)
    {
    *retLabel = inRow[0];
    }
return m->row;
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

struct memMatrix *memMatrixFromTsv(char *fileName)
/* Read all of matrix from tsv file into memory */
{
struct vRowMatrix *v = vRowMatrixOnTsv(fileName);
struct memMatrix *m = vRowMatrixToMem(v);
vRowMatrixFree(&v);
return m;
}

