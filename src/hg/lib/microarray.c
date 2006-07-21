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
struct maExpDataMatrix
{
    int nrow;
    int ncol;
    float **data;
};

float **maExpDataArray(struct expData *exps, int *pNrow, int *pNcol)
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

struct maExpDataMatrix *makeMatrix(struct expData *exps)
/* Convert the expData list into an easier-to-manipulate matrix. */
{
struct maExpDataMatrix *ret;
AllocVar(ret);
ret->data = maExpDataArray(exps, &ret->nrow, &ret->ncol);
return ret;
}

void freeExpDataMatrix(struct maExpDataMatrix **pMat)
/* Free up an maExpDataMatrix. */
{
int i;
struct maExpDataMatrix *mat;
if (!pMat || !(*pMat))
    return;
mat = *pMat;
for (i = 0; i < mat->nrow; i++)
    freeMem(mat->data[i]);
freeMem(mat->data);
freez(pMat);
}

void transposeExpDataMatrix(struct maExpDataMatrix **pMat)
/* Transpose the matrix i.e. mat[i][j] = mat[j][i]. */
/* This may come in handy later if the job is to combine genes instead of */
/* arrays... or both. */
{
int i, j;
struct maExpDataMatrix *mat, *newMat;
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

/* 2D int array to keep track of indeces in a grouping situation. FOr example */
/* the mapArray as follows x = */
/* [[ 2 0 2 0 0 0 ]  */
/*  [ 1 4 0 0 0 0 ]  */
/*  [ 2 1 3 0 0 0 ]  */
/* x is 3 x 6, so the original array had 5 columns.  The number of rows x has */
/* is the new number of columns.  The first number in each row of x keeps */
/* track of y number of columns being grouped, and the next y numbers in that */
/* row are the column indeces of the columns being grouped.  For example, */
/* using x, then if a = [[ 2 3 4 5 6 ]], then b grouped by x is */
/* b = [[ 3 6 4 ]]. */
struct mapArray 
{
    int nrow; 
    int ncol;
    int **data;
};

double maDoubleMeanHandleNA(int count, double *array)
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
if (num > 0) 
    result = sum/num;
return result;
}

double maDoubleMedianHandleNA(int count, double *array)
/* Return median value in array, skipping the NAs. */
/* This one will sort the inputted array as a side-effect. */
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

static double *floatArrayToDoubleArray(int count, float *array)
/* Pretty stupid function but saves a line or two later. */
{
double *ret;
int i;
AllocArray(ret, count);
for (i = 0; i < count; i++)
    ret[i] = array[i];
return ret;
}

static float maCalcNewScore(float *expScores, int *mapRow, enum maCombineMethod method, int minExps)
/* Calculate a median, mean, etc for a group of scores and return it. */
{
double ret = MICROARRAY_MISSING_DATA;
double *grouping;
int k;
int realCount = 0;
if (!expScores || !mapRow)
    return MICROARRAY_MISSING_DATA;
AllocArray(grouping, mapRow[0]);
for (k = 0; k < mapRow[0]; k++)
    {
    grouping[k] = (double)expScores[mapRow[k+1]];
    if (grouping[k] > MICROARRAY_MISSING_DATA)
	realCount++;
    }
if ((realCount == 0) || ((minExps > 0) && (realCount < minExps)))
    ret = MICROARRAY_MISSING_DATA;
else if (method == useMedian)
    ret = maDoubleMedianHandleNA(mapRow[0], grouping);
else if (method == useMean)
    ret = maDoubleMeanHandleNA(mapRow[0], grouping);
freez(&grouping);
return (float)ret;
}

struct maExpDataMatrix *maCombineCols(struct maExpDataMatrix *origMat, struct mapArray *mapping, enum maCombineMethod method, int minExps)
/* Make a new matrix combining the columns of the given one however then */
/* return it. */
{
struct maExpDataMatrix *newMat;
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
	newMat->data[i][j] = maCalcNewScore(origMat->data[i], mapping->data[j], method, minExps);
    }
return newMat;
}

struct expData *maExpDataCombineCols(struct expData *exps, struct mapArray *mapping, enum maCombineMethod method, int minExps)
/* median, mean, etc. the columns of the expData according to the grouping */
/* from the mapping array. Return a new expData list. */
{
struct expData *cur, *newList = NULL;
struct maExpDataMatrix *mat, *combinedMat;
int i;
if (!exps)
    return NULL;
mat = makeMatrix(exps);
combinedMat = maCombineCols(mat, mapping, method, minExps);
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

struct maMedSpec *maMedSpecReadAll(char *fileName)
/* Read in file and parse it into maMedSpecs. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct maMedSpec *medList = NULL, *med;
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

struct mapArray *mappingFromMedSpec(struct maMedSpec *specList)
/* Make an array for mapping out of the maMedSpec.ra files */
/* used by hgMedianMicroarray for the gene sorter. */
{
struct maMedSpec *spec;
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

struct expData *maExpDataGroupByExtras(struct expData *exps, struct expRecord *erList, int extrasIndex, enum maCombineMethod method, int minExps)
/* Combine experiments using mean or median and the grouping defined by the */
/* expRecord/extrasIndex combination. */
{
struct mapArray *mapping = mappingFromExpRecords(erList, extrasIndex);
struct expData *ret = maExpDataCombineCols(exps, mapping, method, minExps);
freeMem(mapping->data);
freez(&mapping);
return ret;
}

struct expData *maExpDataMeanByExtras(struct expData *exps, struct expRecord *erList, int extrasIndex, int minExps)
/* Given the data, the expRecords, and the index for the grouping, mean the */
/* columns in the data. */
{
return maExpDataGroupByExtras(exps, erList, extrasIndex, useMean, minExps);
}

struct expData *maExpDataMedianByExtras(struct expData *exps, struct expRecord *erList, int extrasIndex, int minExps)
/* Given the data, the expRecords, and the index for the grouping, median the */
/* columns in the data. */
{
return maExpDataGroupByExtras(exps, erList, extrasIndex, useMedian, minExps);
}

struct expData *maExpDataMedianFromSpec(struct expData *exps, struct maMedSpec *specs, int minExps)
/* Given the data, the and a maMedSpec list, median the columns in the data. */
{
struct mapArray *mapping = mappingFromMedSpec(specs);
struct expData *ret = maExpDataCombineCols(exps, mapping, useMedian, minExps);
freeMem(mapping->data);
freez(&mapping);
return ret;
}

int maExpDataNumBadScoresForProbe(struct expData *exps, char *probeName)
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

int maExpDataNumBadScoresForOneArray(struct expData *exps, struct expRecord *ers, char *arrayName)
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

float maExpDataOverallMean(struct expData *exps)
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

float maExpDataOverallMedian(struct expData *exps)
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

void maExpDataClipMin(struct expData *exps, float minGood, float minVal)
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

void maExpDataDoLogRatioMeanOrMedian(struct expData *exps, boolean mean)
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
    q = (mean) ? maDoubleMeanHandleNA(exp->expCount, tmpVals) : maDoubleMedianHandleNA(exp->expCount, tmpVals);
    freez(&tmpVals);
    if (q <= 0)
	{
	for (i = 0; i < exp->expCount; i++)
	    exp->expScores[i] = MICROARRAY_MISSING_DATA;
	}
    else
	{
	invQ = 1/q;
	for (i = 0; i < exp->expCount; i++)
	    if (exp->expScores[i] > MICROARRAY_MISSING_DATA)
		exp->expScores[i] = logBase2(exp->expScores[i] * invQ);
	}
    }
}

void maExpDataDoLogRatioClumping(struct expData *exps, struct mapArray *mapping, enum maCombineMethod method)
/* Sort of an intermediary function to be called by maExpDataDoLogRatioMedianOfMedians or */
/* maExpDataDoLogRatioMeanOfMeans. */
{
struct expData *cur;
struct maExpDataMatrix *originalMat = makeMatrix(exps);
struct maExpDataMatrix *clumpedMat = maCombineCols(originalMat, mapping, method, 0);
int i, j;
if (!clumpedMat)
    return;
for (i = 0, cur = exps; (cur != NULL) && (i < clumpedMat->nrow); cur = cur->next, i++)
    {
    double *tmpArray = floatArrayToDoubleArray(clumpedMat->ncol, clumpedMat->data[i]);
    double q = (method == useMean) ? maDoubleMeanHandleNA(clumpedMat->ncol, tmpArray) :
	maDoubleMedianHandleNA(clumpedMat->ncol, tmpArray);
    double invQ;
    freeMem(tmpArray);
    if (q <= 0)
	{
	for (j = 0; j < cur->expCount; j++) 
	    cur->expScores[j] = MICROARRAY_MISSING_DATA;
	}
    else
	{
	invQ = 1/q;
	for (j = 0; j < cur->expCount; j++)
	    cur->expScores[j] = (cur->expScores[j] > 0) ? 
		logBase2(invQ * cur->expScores[j]) : cur->expScores[j];
	}
    }
freeExpDataMatrix(&originalMat);
freeExpDataMatrix(&clumpedMat);
}

void maExpDataDoLogRatioClumpExpRecord(struct expData *exps, struct expRecord *erList, int extrasIndex, enum maCombineMethod method)
/* Log ratio the expData list.  The ratio of the denominator is the median of */
/* medians or the mean of means of each group of experiments.  This grouping */
/* is defined by the expRecord/extrasIndex combination. The log is base 2. */
{
struct mapArray *mapping = mappingFromExpRecords(erList, extrasIndex);
maExpDataDoLogRatioClumping(exps, mapping, method);
freeMem(mapping->data);
freez(&mapping);
}

void maExpDataDoLogRatioGivenMedSpec(struct expData *exps, struct maMedSpec *specList, enum maCombineMethod method)
/* Same as maExpDataDoLogRatioClumpExpRecord except use the maMedSpec as the */
/* thing to base the groupings off of. */
{
struct mapArray *mapping;
if (specList == NULL) 
    {
    if (method == useMedian)
	maExpDataDoLogRatioMeanOrMedian(exps, FALSE);
    else if (method == useMean)
	maExpDataDoLogRatioMeanOrMedian(exps, TRUE);
    return;
    }
mapping = mappingFromMedSpec(specList);
maExpDataDoLogRatioClumping(exps, mapping, method);
freeMem(mapping->data);
freez(&mapping);
}
