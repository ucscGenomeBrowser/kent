#include "common.h"
#include "obscure.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "bed.h"
#include "expData.h"
#include "expRecord.h"
#include "microarray.h"

/* Make a 2D array out of the list of expDatas' expScores. */
struct expDataMatrix
{
    int nrow;
    int ncol;
    float **data;
};

float **expDataArray(struct expData *exps, int *pNrow, int *pNcol)
/* Make a 2D array out of a list of expDatas' expScores. */
{
struct expData *cur;
float **ret;
int i = 0;
if (!exps)
    return NULL;
*pNrow = slCount(exps);
*pNcol = exps->expCount;
AllocArray(ret, *pNrow);
for (cur = exps; cur != NULL; cur = cur->next)
    {
    float *thecopy = CloneArray(cur->expScores, *pNcol);
    ret[i++] = thecopy;
    }
return ret;
}

struct expDataMatrix *makeMatrix(struct expData *exps)
/* Convert the expData list into an easier-to-manipulate matrix. */
{
struct expDataMatrix *ret;
AllocVar(ret);
ret->data = expDataArray(exps, &ret->nrow, &ret->ncol);
return ret;
}

void freeExpDataMatrix(struct expDataMatrix **pMat)
/* Free up an expDataMatrix. */
{
int i;
struct expDataMatrix *mat;
if (!pMat || !(*pMat))
    return;
mat = *pMat;
for (i = 0; i < mat->nrow; i++)
    freeMem(mat->data[i]);
freeMem(mat->data);
freez(pMat);
}

void transposeExpDataMatrix(struct expDataMatrix **pMat)
/* Transpose the matrix i.e. mat[i][j] = mat[j][i]. */
/* This may come in handy later if the job is to combine genes instead of */
/* arrays... or both. */
{
int i, j;
struct expDataMatrix *mat, *newMat;
if (!pMat || !(*pMat))
    return;
mat = *pMat;
AllocVar(newMat);
AllocArray(newMat->data, mat->ncol);
for (i = 0; i < mat->ncol; i++)
    {
    AllocArray(newMat->data[i], mat->nrow);
    for (j = 0; j < mat->nrow; j++)
	newMat->data[i][j] = mat->data[j][i];
    }
newMat->nrow = mat->ncol;
newMat->ncol = mat->nrow;
for (i = 0; i < mat->nrow; i++)
    freeMem(mat->data[i]);
*pMat = newMat;
}

/* 2D array to keep */
struct mapArray 
{
    int nrow; 
    int ncol;
    int **data;
};

double doubleMeanHandleNA(int count, double *array)
/* Calculate the mean value of an array, skipping the NA vals. */
{
int i, num = 0;
double result = MICROARRAY_MISSING_DATA, sum = 0;
for (i = 0; i < count; i++)
    if (array[i] > MICROARRAY_MISSING_DATA)
	{
	sum += array[i];
	num++;
	}
result = sum/num;
return result;
}

double doubleMedianHandleNA(int count, double *array)
/* Return median value in array, skipping the NAs. */
{
double median, *pastNA;
int countSansNA = count;
doubleSort(count, array);
pastNA = array;
while ((countSansNA > 0) && (*pastNA <= MICROARRAY_MISSING_DATA))
    {
    pastNA++;
    countSansNA--;
    }
if (countSansNA < 1)
    return MICROARRAY_MISSING_DATA;
if ((countSansNA&1) == 1)
    median = pastNA[countSansNA>>1];
else
    {
    countSansNA >>= 1;
    median = (pastNA[countSansNA] + pastNA[countSansNA-1]) * 0.5;
    }
return median;
}

double *floatArrayToDoubleArray(int count, float *array)
/* Pretty stupid function but saves a line or two later. */
{
double *ret;
int i;
AllocArray(ret, count);
for (i = 0; i < count; i++)
    ret[i] = array[i];
return ret;
}

float calcNewScore(float *expScores, int *mapRow, enum combineMethod method)
{
double ret = MICROARRAY_MISSING_DATA;
double *grouping;
int k;
if (!expScores || !mapRow)
    return MICROARRAY_MISSING_DATA;
AllocArray(grouping, mapRow[0]);
for (k = 0; k < mapRow[0]; k++)
    grouping[k] = (double)expScores[mapRow[k+1]];
if (method == useMedian)
    ret = doubleMedianHandleNA(mapRow[0], grouping);
else if (method == useMean)
    ret = doubleMeanHandleNA(mapRow[0], grouping);
freez(&grouping);
return (float)ret;
}

struct expDataMatrix *combineCols(struct expDataMatrix *origMat, struct mapArray *mapping, enum combineMethod method)
/* Make a new matrix combining the columns of the given one however then */
/* return it. */
{
struct expDataMatrix *newMat;
int i, j;
if (!origMat)
    return NULL;
AllocVar(newMat);
newMat->nrow = origMat->nrow;
newMat->ncol = mapping->nrow;
AllocArray(newMat->data, newMat->nrow);
for (i = 0; i < newMat->nrow; i++)
    {
    AllocArray(newMat->data[i], newMat->ncol);
    for (j = 0; j < newMat->ncol; j++)
	newMat->data[i][j] = calcNewScore(origMat->data[i], mapping->data[j], method);
    }
return newMat;
}

struct expData *expDataCombineCols(struct expData *exps, struct mapArray *mapping, enum combineMethod method)
/* median, mean, etc. the columns of the expData according to the grouping */
/* from the mapping array. Return a new expData list. */
{
struct expData *cur, *newList = NULL;
struct expDataMatrix *mat, *combinedMat;
int i;
if (!exps)
    return NULL;
mat = makeMatrix(exps);
combinedMat = combineCols(mat, mapping, method);
for (cur = exps, i = 0; cur != NULL; cur = cur->next, i++)
    {
    struct expData *newExp;
    AllocVar(newExp);
    newExp->name = cloneString(cur->name);
    newExp->expCount = mapping->nrow;
    newExp->expScores = combinedMat->data[i];
    slAddHead(&newList, newExp);
    }
slReverse(&newList);
freeMem(mat->data);
freez(&mat);
freez(&combinedMat);
return newList;
}

struct mapArray *mappingFromExpRecords(struct expRecord *erList, int extrasIndex)
/* Make a mapping matrix out of a list of expRecord structs using the */
/* specific grouping provided by the expRecord->extras[extrasIndex].  */
{
struct expRecord *er;
struct mapArray *ret;
struct slName *erNames = NULL;
int i;
if (!erList)
    return NULL;
if ((extrasIndex < 0) || (extrasIndex >= erList->numExtras))
    return NULL;
for (er = erList; er != NULL; er = er->next)
    slNameStore(&erNames, er->extras[extrasIndex]);
slReverse(&erNames);
AllocVar(ret);
ret->nrow = slCount(erNames);
ret->ncol = slCount(erList) + 1;
AllocArray(ret->data, ret->nrow);
for (i = 0; i < ret->nrow; i++)
    AllocArray(ret->data[i], ret->ncol);
for (er = erList, i = 0; er != NULL; er = er->next, i++)
    {
    int rowIx = slNameFindIx(erNames, er->extras[extrasIndex]);
    ret->data[rowIx][++ret->data[rowIx][0]] = i;    
    }
slNameFreeList(&erNames);
return ret;
}

struct medSpec *medSpecReadAll(char *fileName)
/* Read in file and parse it into medSpecs. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct medSpec *medList = NULL, *med;
char *row[256], *line;
int i, count;
while (lineFileNextReal(lf, &line))
    {
    char *name = nextQuotedWord(&line);
    char *group = nextQuotedWord(&line);
    if (group == NULL)
	lineFileShort(lf);
    count = chopLine(line, row);
    if (count < 1)
        lineFileShort(lf);
    if (count >= ArraySize(row))
        errAbort("Too many experiments in median line %d of %s",
		lf->lineIx, lf->fileName);
    AllocVar(med);
    med->name = cloneString(name);
    med->group = cloneString(group);
    med->count = count;
    AllocArray(med->ids, count);
    for (i=0; i<count; ++i)
        {
	char *asc = row[i];
	if (!isdigit(asc[0]))
	    errAbort("Expecting number got %s line %d of %s", asc,
	    	lf->lineIx, lf->fileName);
	med->ids[i] = atoi(asc);
	}
    slAddHead(&medList, med);
    }
lineFileClose(&lf);
slReverse(&medList);
return medList;
}

struct mapArray *mappingFromMedSpec(struct medSpec *specList)
/* Make an array for mapping out of the medSpec.ra files */
/* used by hgMedianMicroarray for the gene sorter. */
{
struct medSpec *spec;
struct mapArray *ret;
int numTotalExps = 0;
int i;
if (specList == NULL)
    return NULL;
for (spec = specList; spec != NULL; spec = spec->next)
    numTotalExps += spec->count;
AllocVar(ret);
ret->nrow = slCount(specList);
ret->ncol = numTotalExps + 1;
AllocArray(ret->data, ret->nrow);
for (spec = specList, i = 0; spec != NULL; spec = spec->next, i++)
    {
    int j;
    AllocArray(ret->data[i], ret->ncol);
    for (j = 0; j < spec->count; j++)
	{
	ret->data[i][0]++;
	ret->data[i][j+1] = spec->ids[j];
	}
    }
return ret;
}

struct expData *expDataGroupByExtras(struct expData *exps, struct expRecord *erList, int extrasIndex, enum combineMethod method)
/* Combine experiments using mean or median and the grouping defined by the */
/* expRecord/extrasIndex combination. */
{
struct mapArray *mapping = mappingFromExpRecords(erList, extrasIndex);
struct expData *ret = expDataCombineCols(exps, mapping, method);
freeMem(mapping->data);
freez(&mapping);
return ret;
}

struct expData *expDataMeanByExtras(struct expData *exps, struct expRecord *erList, int extrasIndex)
/* Given the data, the expRecords, and the index for the grouping, mean the */
/* columns in the data. */
{
return expDataGroupByExtras(exps, erList, extrasIndex, useMean);
}

struct expData *expDataMedianByExtras(struct expData *exps, struct expRecord *erList, int extrasIndex)
/* Given the data, the expRecords, and the index for the grouping, median the */
/* columns in the data. */
{
return expDataGroupByExtras(exps, erList, extrasIndex, useMedian);
}

struct expData *expDataMedianFromSpec(struct expData *exps, struct medSpec *specs)
/* Given the data, the and a medSpec list, median the columns in the data. */
{
struct mapArray *mapping = mappingFromMedSpec(specs);
struct expData *ret = expDataCombineCols(exps, mapping, useMedian);
freeMem(mapping->data);
freez(&mapping);
return ret;
}

int expDataNumBadScoresForProbe(struct expData *exps, char *probeName)
/* Given a probe name, find that probe's data in the list and report */
/* the # of NA (< -9999) values in the expScores. I.e. how many bad */
/* experiments are there. Return -1 if name not found. */
{
struct expData *exp;
int numBad = 0;
boolean foundName = FALSE;
if (probeName == NULL)
    return -1;
for (exp = exps; exp != NULL; exp = exp->next)
    if (sameString(probeName, exp->name))
	{
	int i;
	foundName = TRUE;
	for (i = 0; i < exp->expCount; i++)
	    if (exp->expScores[i] <= MICROARRAY_MISSING_DATA)
		numBad++;
	break;
	}
return (foundName) ? numBad : -1;
}

int expDataNumBadScoresForOneArray(struct expData *exps, struct expRecord *ers, char *arrayName)
/* Find # of bad datapoints for a certain column (array) in the dataset. */
/* return -1 if not found. */
{
int expId = -1, numBad = 0;
struct expRecord *findMe;
struct expData *exp;
if (arrayName == NULL)
    return -1;
for (findMe = ers; findMe != NULL; findMe = findMe->next)
    if (sameString(findMe->name, arrayName))
	{
	expId = findMe->id;
	break;
	}
if (expId < 0)
    return -1;
for (exp = exps; exp != NULL; exp = exp->next)
    {
    if (expId >= exp->expCount)
	return -1;
    if (exp->expScores[expId] <= MICROARRAY_MISSING_DATA)
	numBad++;
    }
return numBad;
}

float expDataOverallMean(struct expData *exps)
/* Return the overall mean for the expression data. */
{
double sum = 0;
long goodVals = 0;
int i;
struct expData *exp;
if (exps == NULL)
    return -1;
for (exp = exps; exp != NULL; exp = exp->next)
    {
    for (i = 0; i < exp->expCount; i++)
	{
	if (exp->expScores[i] > MICROARRAY_MISSING_DATA)
	    {
	    goodVals++;
	    sum += exp->expScores[i];
	    }
	}
    }
return (float)(sum/(double)goodVals);
}

float expDataOverallMedian(struct expData *exps)
/* Return the overall median for the expression data. */
{
double *expVals;
int goodVals = 0;
int i;
float ret = 0;
struct expData *exp;
if (exps == NULL)
    return -1;
AllocArray(expVals, slCount(exps) * exps->expCount);
for (exp = exps; exp != NULL; exp = exp->next)
    {
    for (i = 0; i < exp->expCount; i++)
	{
	if (exp->expScores[i] > MICROARRAY_MISSING_DATA)
	    expVals[goodVals++] = exp->expScores[i];
	}
    }
ret = (float)doubleMedian(goodVals, expVals);
freez(&expVals);
return ret;
}

void expDataClipMin(struct expData *exps, float minGood, float minVal)
/* If an expData->expScores datapoint is < minGood, set it to minVal. */
{
struct expData *exp;
for (exp = exps; exp != NULL; exp = exp->next)
    {
    int i;
    for (i = 0; i < exp->expCount; i++)
	if ((exp->expScores[i] > MICROARRAY_MISSING_DATA) && (exp->expScores[i] < minGood))
	    exp->expScores[i] = minVal;
    }
}

void expDataDoLogRatioMeanOrMedian(struct expData *exps, boolean mean)
/* For the M x N sized expression matrix, change each value to be the ratio */
/* of that value to the mean or median of values in that value's row (gene). */
{
struct expData *exp;
for (exp = exps; exp != NULL; exp = exp->next)
    {
    double *tmpVals;
    int i;
    double q, invQ;
    AllocArray(tmpVals, exp->expCount);
    for (i = 0; i < exp->expCount; i++)
	tmpVals[i] = exp->expScores[i];
    q = (mean) ? doubleMeanHandleNA(exp->expCount, tmpVals) : doubleMedianHandleNA(exp->expCount, tmpVals);
    freez(&tmpVals);
    if (q <= 0)
	continue;
    invQ = 1/q;
    for (i = 0; i < exp->expCount; i++)
	if (exp->expScores[i] > MICROARRAY_MISSING_DATA)
	    exp->expScores[i] = logBase2(exp->expScores[i] * invQ);
    }
}

void expDataDoLogRatioClumping(struct expData *exps, struct mapArray *mapping, enum combineMethod method)
/* Sort of an intermediary function to be called by expDataDoLogRatioMedianOfMedians or */
/* expDataDoLogRatioMeanOfMeans. */
{
struct expData *cur;
struct expDataMatrix *originalMat = makeMatrix(exps);
struct expDataMatrix *clumpedMat = combineCols(originalMat, mapping, method);
int i, j;
if (!clumpedMat)
    return;
for (i = 0, cur = exps; (cur != NULL) && (i < clumpedMat->nrow); cur = cur->next, i++)
    {
    double *tmpArray = floatArrayToDoubleArray(clumpedMat->ncol, clumpedMat->data[i]);
    double q = (method == useMean) ? doubleMeanHandleNA(clumpedMat->ncol, tmpArray) :
	doubleMedianHandleNA(clumpedMat->ncol, tmpArray);
    double invQ;
    freeMem(tmpArray);
    if (q <= 0)
	continue;
    invQ = 1/q;
    for (j = 0; j < cur->expCount; j++)
	cur->expScores[j] = (cur->expScores[j] > 0) ? logBase2(invQ * cur->expScores[j]) : cur->expScores[j];
    }
freeExpDataMatrix(&originalMat);
freeExpDataMatrix(&clumpedMat);
}

void expDataDoLogRatioClumpExpRecord(struct expData *exps, struct expRecord *erList, int extrasIndex, enum combineMethod method)
/* Log ratio the expData list.  The ratio of the denominator is the median of */
/* medians or the mean of means of each group of experiments.  This grouping */
/* is defined by the expRecord/extrasIndex combination. The log is base 2. */
{
struct mapArray *mapping = mappingFromExpRecords(erList, extrasIndex);
expDataDoLogRatioClumping(exps, mapping, method);
freeMem(mapping->data);
freez(&mapping);
}

void expDataDoLogRatioGivenMedSpec(struct expData *exps, struct medSpec *specList, enum combineMethod method)
/* Same as expDataDoLogRatioClumpExpRecord except use the medSpec as the */
/* thing to base the groupings off of. */
{
struct mapArray *mapping = mappingFromMedSpec(specList);
expDataDoLogRatioClumping(exps, mapping, method);
freeMem(mapping->data);
freez(&mapping);
}
