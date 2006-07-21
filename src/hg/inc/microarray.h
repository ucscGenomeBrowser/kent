/* Microarray code common to hgc, hgTracks, and hgNear. */
/* Also use in loaders like hgRatioMicroarray and hgMedianMicroarray. */

#ifndef MICROARRAY_H
#define MICROARRAY_H

#ifndef EXPRECORD_H
#include "expRecord.h"
#endif

#define MICROARRAY_MISSING_DATA -10000
/* This is an important constant.  Often zero and negative numbers are valid */
/* microarray data.  -10000 is often used then. */

/* Just an enum to list the several ways of combining columns of data. */
enum maCombineMethod
    {
    useMedian = 0,
    useMean = 1,
    };

double maDoubleMeanHandleNA(int count, double *array);
/* Calculate the mean value of an array, skipping the NA vals. */

double maDoubleMedianHandleNA(int count, double *array);
/* Return median value in array, skipping the NAs. */

struct maMedSpec
/* A specification for median. */
    {
    struct maMedSpec *next;
    char *name;		/* Column name. */
    char *group;	/* Tissue/group name. */
    int count;		/* Count of experiments to median. */
    int *ids;		/* Id's (indexes) of experiments to median. */
    };

struct maMedSpec *maMedSpecReadAll(char *fileName);
/* Read in file and parse it into maMedSpecs. */

struct expData *maExpDataGroupByExtras(struct expData *exps, struct expRecord *erList, int extrasIndex, enum maCombineMethod method, int minExps);
/* Combine experiments using mean or median and the grouping defined by the */
/* expRecord/extrasIndex combination. */

struct expData *maExpDataMeanByExtras(struct expData *exps, struct expRecord *erList, int extrasIndex, int minExps);
/* Given the data, the expRecords, and the index for the grouping, mean the */
/* columns in the data. */

struct expData *maExpDataMedianByExtras(struct expData *exps, struct expRecord *erList, int extrasIndex, int minExps);
/* Given the data, the expRecords, and the index for the grouping, median the */
/* columns in the data. */

struct expData *maExpDataMedianFromSpec(struct expData *exps, struct maMedSpec *specs, int minExps);
/* Given the data, the and a maMedSpec list, median the columns in the data. */

int maExpDataNumBadScoresForProbe(struct expData *exps, char *probeName);
/* Given a probe name, find that probe's data in the list and report */
/* the # of NA (< -9999) values in the expScores. I.e. how many bad */
/* experiments are there. Return -1 if name not found. */

int maExpDataNumBadScoresForOneArray(struct expData *exps, struct expRecord *ers, char *arrayName);
/* Find # of bad datapoints for a certain column (array) in the dataset. */
/* return -1 if not found. */

float maExpDataOverallMean(struct expData *exps);
/* Return the overall mean for the expression data. */

float maExpDataOverallMedian(struct expData *exps);
/* Return the overall median for the expression data. */

void maExpDataClipMin(struct expData *exps, float minGood, float minVal);
/* If an expData->expScores datapoint is < minGood, set it to minVal. */

void maExpDataDoLogRatioMeanOrMedian(struct expData *exps, boolean mean);
/* For the M x N sized expression matrix, change each value to be the ratio */
/* of that value to the mean or median of values in that value's row (gene). */
/* This involves no clumping of experiments to calculate the median/mean. */

void maExpDataDoLogRatioClumpExpRecord(struct expData *exps, struct expRecord *erList, int extrasIndex, enum maCombineMethod method);
/* Log ratio the expData list.  The ratio of the denominator is the median of */
/* medians or the mean of means of each group of experiments.  This grouping */
/* is defined by the expRecord/extrasIndex combination. The log is base 2. */

void maExpDataDoLogRatioGivenMedSpec(struct expData *exps, struct maMedSpec *specList, enum maCombineMethod method);
/* Same as maExpDataDoLogRatioClumpExpRecord except use the maMedSpec as the */
/* thing to base the groupings off of. */

#endif
