#include "common.h"
#include "obscure.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "bed.h"
#include "cart.h"
#include "expData.h"
#include "expRecord.h"
#include "hdb.h"
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

struct mapArray *maMappingFromGrouping(struct maGrouping *grouping)
/* Convert a microarrayGroups.ra style grouping to the array. */
{
struct mapArray *ret;
int i, offset = 0;
AllocVar(ret);
ret->nrow = grouping->numGroups;
ret->ncol = grouping->size + 1;
AllocArray(ret->data, ret->ncol);
for (i = 0; i < ret->nrow; i++)
    {
    int j; /* Iterator for grouping->groupSizes */
    int k = 1; /* Iterator for ret->data[i][] */
    AllocArray(ret->data[i], ret->ncol);
    ret->data[i][0] = grouping->groupSizes[i];
    for (j = offset; j < offset+grouping->groupSizes[i]; j++)
	ret->data[i][k++] = grouping->expIds[j];
    offset += grouping->groupSizes[i];
    }
return ret;
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

struct expData *maExpDataTranspose(struct expData *exps)
/* Return a transposed copy of the expData. */
{
struct expData *transposed = NULL;
int probeCount, expCount, i;
if (!exps)
    errAbort("Tried to transpose empty expData");
expCount = exps->expCount;
probeCount = slCount(exps);
for (i = 0; i < expCount; i++)
    {
    struct expData *newProbe, *cur;
    char name[256];
    int j;
    AllocVar(newProbe);
    safef(name, sizeof(name), "array-%d", i);
    newProbe->name = cloneString(name);
    newProbe->expCount = probeCount;
    AllocArray(newProbe->expScores, newProbe->expCount);
    for (j = 0, cur = exps; (cur != NULL) && (j < probeCount); cur = cur->next, j++)
	{
	if (i >= cur->expCount)
	    errAbort("There needs to be a uniform expCount in given expData in order to transpose");
	newProbe->expScores[j] = cur->expScores[i];
	}
    slAddHead(&transposed, newProbe);
    }
slReverse(&transposed);
return transposed;
}

void maExpDataUntranspose(struct expData *exps, struct expData *transpose)
/* Copy values over from the transposed expData. */
{
int expCount = transpose->expCount;
int probeCount = exps->expCount;
int i; 
struct expData *exp;
for (exp = exps, i = 0; (exp != NULL) && (i < expCount); exp = exp->next, i++)
    {
    struct expData *trans;
    int j;
    for (j = 0, trans = transpose; (j < probeCount) && (trans != NULL); j++, trans = trans->next)
	exp->expScores[j] = trans->expScores[i];
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

void maExpDataDoLogRatioTranspose(struct expData *exps, boolean mean)
/* For the M x N sized expression matrix, change each value to be the ratio */
/* of that value to the mean or median of values in that value's column (probe). */
/* This involves no clumping of experiments to calculate the median/mean. */
{
struct expData *transpose = maExpDataTranspose(exps);
maExpDataDoLogRatioMeanOrMedian(transpose, mean);
maExpDataUntranspose(exps, transpose);
expDataFreeList(&transpose);
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

struct expData *maExpDataClumpGivenGrouping(struct expData *exps, struct maGrouping *grouping)
/* Clump expDatas from a grouping from a .ra file. */
{
struct mapArray *mapping;
struct expData *ret = NULL;
enum maCombineMethod combine = useMedian;
char *combType;
if (!grouping->type || sameWord(grouping->type, "all"))
    return NULL;
combType = cloneString(grouping->type);
eraseWhiteSpace(combType);
if (sameWord(combType, "combinemean"))
    combine = useMean;
mapping = maMappingFromGrouping(grouping);
ret = maExpDataCombineCols(exps, mapping, combine, 0);
freeMem(mapping->data);
freez(&mapping);
freeMem(combType);
return ret;
}

void maExpDataAddConstant(struct expData *exps, double c)
/* Add a constant c to all of the microarray data. Ensure that NA values */
/* aren't inadvertantly created. */
{
struct expData *exp;
for (exp = exps; exp != NULL; exp = exp->next)
    {
    int i;
    for (i = 0; i < exp->expCount; i++)
	{
	if (c <= MICROARRAY_MISSING_DATA)
	    errAbort("error: Constant used caused expData value to fall into the NA range");
	if (exp->expScores[i] > MICROARRAY_MISSING_DATA)
	    exp->expScores[i] = exp->expScores[i] + c;
	}
    }
}

/* Functions for getting information from the microarrayGroups.ra files */
/* that are in the hgCgiData/<org>/<database> .ra tree. */

void maGroupingFree(struct maGrouping **pMag)
/* free up a maGrouping */
{
int i, size;
struct maGrouping *mag;
if (!pMag || !*pMag)
    return;
mag = *pMag;
size = sameString(mag->type, "all") ? mag->size : mag->numGroups;
freeMem(mag->name);
freeMem(mag->description);
freeMem(mag->expIds);
freeMem(mag->groupSizes);
freeMem(mag->type);
for (i = 0; i < size; i++)
    freeMem(mag->names[i]);
freeMem(mag->names);
freez(pMag);
}

void maGroupingFreeList(struct maGrouping **pList)
/* Free up a list of maGroupings. */
{
struct maGrouping *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    maGroupingFree(&el);
    }
*pList = NULL;
}

void microarrayGroupsFree(struct microarrayGroups **pGroups)
/* Free up the microarrayGroups struct. */
{
struct microarrayGroups *groups;
if (!pGroups || ((groups = *pGroups) == NULL))
    return;
maGroupingFree(&groups->allArrays);
maGroupingFreeList(&groups->combineSettings);
maGroupingFreeList(&groups->subsetSettings);
freez(pGroups);
}

struct maGrouping *maHashToMaGrouping(struct hash *oneGroup)
/* This converts a single "stanza" of the microarrayGroups.ra file to a */
/* maGrouping struct. */
{
struct maGrouping *ret;
char *s;
struct slName *wordList = NULL, *oneWord;
int i = 0;
AllocVar(ret);
/* required settings. */
ret->name = cloneString((char *)hashMustFindVal(oneGroup, "name"));
ret->type = cloneString((char *)hashMustFindVal(oneGroup, "type"));
ret->description = cloneString((char *)hashMustFindVal(oneGroup, "description"));
s = hashMustFindVal(oneGroup, "expIds");
wordList = slNameListFromComma(s);
ret->size = slCount(wordList);
AllocArray(ret->expIds, ret->size);
for (oneWord = wordList; oneWord != NULL; oneWord = oneWord->next)
    ret->expIds[i++] = atoi(oneWord->name);
slNameFreeList(&wordList);
s = hashMustFindVal(oneGroup, "names");
wordList = slNameListFromComma(s);
i = 0;
ret->numGroups = slCount(wordList);
AllocArray(ret->names, ret->numGroups);
for (oneWord = wordList; oneWord != NULL; oneWord = oneWord->next)
    ret->names[i++] = cloneString(oneWord->name);
slNameFreeList(&wordList);
s = hashMustFindVal(oneGroup, "groupSizes");
wordList = slNameListFromComma(s);
i = 0;
AllocArray(ret->groupSizes, ret->numGroups);
if (ret->numGroups != slCount(wordList))
    errAbort("Bad format of microarrayGroups.ra in %s, groupSizes size "
	     "= %d, != to names size = %d", ret->name, slCount(wordList), ret->numGroups);
for (oneWord = wordList; oneWord != NULL; oneWord = oneWord->next)
    ret->groupSizes[i++] = atoi(oneWord->name);
slNameFreeList(&wordList);
return ret;
}

struct maGrouping *maGetGroupingsFromList(char *commaList, struct hash *allGroupings)
/* Load all the maGroupings from a comma-delimited list. */
{
struct slName *list = slNameListFromComma(commaList), *oneItem;
struct maGrouping *retList = NULL;
for (oneItem = list; oneItem != NULL; oneItem = oneItem->next)
    {
    struct hash *oneGrouping = hashMustFindVal(allGroupings, oneItem->name);
    struct maGrouping *newGroup = maHashToMaGrouping(oneGrouping);
    slAddHead(&retList, newGroup);
    }
slNameFreeList(&list);
slReverse(&retList);
return retList;
}

struct maGrouping *maGetGroupingFromCt(struct customTrack *ct)
/* Spoof an "all" maGrouping from a customTrack. */
{
struct maGrouping *ret;
char *words = trackDbRequiredSetting(ct->tdb, "expNames");
struct slName *list = slNameListFromComma(words), *oneItem;
int i;
AllocVar(ret);
/* ret->name not used in custom tracks. */
ret->type = cloneString("all");
ret->description = cloneString("Custom Track");
ret->size = slCount(list);
ret->numGroups = ret->size;
AllocArray(ret->expIds, ret->size);
AllocArray(ret->groupSizes, ret->size);
AllocArray(ret->names, ret->size);
for (oneItem = list, i = 0; (oneItem != NULL) && (i < ret->size); oneItem = oneItem->next, i++)
    {
    ret->expIds[i] = i;
    ret->groupSizes[i] = 1;
    ret->names[i] = cloneString(oneItem->name);
    }
slFreeList(&list);
return ret;
}

struct bed *ctLoadMultScoresBedDb(struct customTrack *ct, char *chrom, int start, int end)
/* If the custom track is stored in a database, load it. */
{
int rowOffset;
char **row;
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
struct sqlResult *sr;
struct bed *bed, *bedList = NULL;
sr = hRangeQuery(conn, ct->dbTableName, chrom, start, end,
		 NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, ct->fieldCount);
    slAddHead(&bedList, bed);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
slReverse(&bedList);
return bedList;
}

struct microarrayGroups *maGetTrackGroupings(char *database, struct trackDb *tdb)
/* Get the settings from the .ra files and put them in a convenient struct. */
{
struct microarrayGroups *ret;
char *groupings = trackDbSetting(tdb, "groupings");
char *s = NULL;
struct hash *allGroups;
struct hash *mainGroup;
struct hash *tmpGroup;
struct hash *hashList; 
if (groupings == NULL)
    return NULL;
hashList = 
    hgReadRa(hGenome(database), database, "hgCgiData", 
	     "microarrayGroups.ra", &allGroups);
if (allGroups == NULL)
    errAbort("Could not get group settings for track.");
AllocVar(ret);
mainGroup = hashMustFindVal(allGroups, groupings);
s = hashMustFindVal(mainGroup, "all");
tmpGroup = hashMustFindVal(allGroups, s);
ret->allArrays = maHashToMaGrouping(tmpGroup);
s = hashFindVal(mainGroup, "combine");
if (s)
    {
    ret->combineSettings = maGetGroupingsFromList(s, allGroups);
    s = hashFindVal(mainGroup, "combine.default");
    if (s)
	{
	struct maGrouping *cur;
	if (lastChar(s) == ',')
	    s[strlen(s)-1] = '\0';
	for (cur = ret->combineSettings; cur != NULL; cur = cur->next)
	    if (sameWord(s, cur->name))
		{
		ret->defaultCombine = cur;
		break;
		}
	if (ret->defaultCombine == NULL)
	    errAbort("$%s$ not a valid combine.default in %s", s, groupings);
	}
    }
s = hashFindVal(mainGroup, "subset");
if (s)
    ret->subsetSettings = maGetGroupingsFromList(s, allGroups);
ret->numCombinations = slCount(ret->combineSettings);
ret->numSubsets = slCount(ret->subsetSettings);
hashFreeList(&hashList);
return ret;
}

struct maGrouping *maCombineGroupingFromCart(struct microarrayGroups *groupings, 
				    struct cart *cart, char *trackName)
/* Determine which grouping to use based on the cart status or lack thereof. */
{
char *setting = NULL;
char cartVar[512];
struct maGrouping *ret = NULL;
/* Possibly NULL from custom trackness. */
if (!groupings)
    return NULL;
safef(cartVar, sizeof(cartVar), "%s.combine", trackName);
setting = cartUsualString(cart, cartVar, NULL);
if (setting && sameWord(groupings->allArrays->name, setting))
    return groupings->allArrays;
if (setting)
    {
    struct maGrouping *cur;
    for (cur = groupings->combineSettings; cur != NULL; cur = cur->next)
	if (sameWord(cur->name, setting))
	    return cur;
    }
if (ret == NULL)
    ret = groupings->defaultCombine;
if (ret == NULL)
    ret = groupings->allArrays;
return ret;
}

/********* Dealing with BED. ************/

struct expData *maExpDataFromExpBed(struct bed *oneBed)
/* Convert a bed record's expScores into an expData. */
{
struct expData *ret = NULL;
AllocVar(ret);
ret->name = cloneString(oneBed->name);
ret->expCount = oneBed->expCount;
if (ret->expCount > 0) 
    ret->expScores = CloneArray(oneBed->expScores, oneBed->expCount);
return ret;
}

struct expData *maExpDataListFromExpBedList(struct bed *bedList)
/* Convert list of bed 15 records to a list of expDatas. */
{
struct expData *newList = NULL;
struct bed *cur;
for (cur = bedList; cur != NULL; cur = cur->next)
    {
    struct expData *addMe = maExpDataFromExpBed(cur);
    slAddHead(&newList, addMe);
    }
slReverse(&newList);
return newList;
}

void maBedClumpGivenGrouping(struct bed *bedList, struct maGrouping *grouping)
/* Clump (mean/median) a bed 15 given the grouping kind. */
{
struct expData *exps = maExpDataListFromExpBedList(bedList);
struct expData *clumpedExps = maExpDataClumpGivenGrouping(exps, grouping);
struct bed *bed;
struct expData *exp;
expDataFreeList(&exps);
if (!clumpedExps)
    return;
/* Go through each bed and copy over the new expDatas */
/* and free up what was there. */
for (bed = bedList, exp = clumpedExps; (bed != NULL) && (exp != NULL); 
     bed = bed->next, exp = exp->next)
    {
    int i;
    bed->expCount = exp->expCount;
    if (bed->expScores)
	freeMem(bed->expScores);
    if (bed->expIds)
	freeMem(bed->expIds);
    bed->expScores = CloneArray(exp->expScores, exp->expCount);
    AllocArray(bed->expIds, bed->expCount);
    for (i = 0; i < bed->expCount; i++)
	bed->expIds[i] = i;
    }
expDataFreeList(&clumpedExps);
}
