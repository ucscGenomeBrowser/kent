/* Microarray code common to hgc, hgTracks, and hgNear. */
/* Also use in loaders like hgRatioMicroarray and hgMedianMicroarray. */

#ifndef MICROARRAY_H
#define MICROARRAY_H

#define MICROARRAY_MISSING_DATA -10000
/* This is an important constant.  Often zero and negative numbers are valid */
/* microarray data.  -10000 is often used then. */

/* Just an enum to list the several ways of combining columns of data. */
enum combineMethod
    {
    useMedian = 0,
    useMean = 1,
    };

double doubleMeanHandleNA(int count, double *array);
/* Calculate the mean value of an array, skipping the NA vals. */

double doubleMedianHandleNA(int count, double *array);
/* Return median value in array, skipping the NAs. */

struct medSpec
/* A specification for median. */
    {
    struct medSpec *next;
    char *name;		/* Column name. */
    char *group;	/* Tissue/group name. */
    int count;		/* Count of experiments to median. */
    int *ids;		/* Id's (indexes) of experiments to median. */
    };

struct medSpec *medSpecReadAll(char *fileName);
/* Read in file and parse it into medSpecs. */

struct expData *expDataGroupByExtras(struct expData *exps, struct expRecord *erList, int extrasIndex, enum combineMethod method);
/* Combine experiments using mean or median and the grouping defined by the */
/* expRecord/extrasIndex combination. */

struct expData *expDataMeanByExtras(struct expData *exps, struct expRecord *erList, int extrasIndex);
/* Given the data, the expRecords, and the index for the grouping, mean the */
/* columns in the data. */

struct expData *expDataMedianByExtras(struct expData *exps, struct expRecord *erList, int extrasIndex);
/* Given the data, the expRecords, and the index for the grouping, median the */
/* columns in the data. */

struct expData *expDataMedianFromSpec(struct expData *exps, struct medSpec *specs);
/* Given the data, the and a medSpec list, median the columns in the data. */

int expDataNumBadScoresForProbe(struct expData *exps, char *probeName);
/* Given a probe name, find that probe's data in the list and report */
/* the # of NA (< -9999) values in the expScores. I.e. how many bad */
/* experiments are there. Return -1 if name not found. */

int expDataNumBadScoresForOneArray(struct expData *exps, struct expRecord *ers, char *arrayName);
/* Find # of bad datapoints for a certain column (array) in the dataset. */
/* return -1 if not found. */

float expDataOverallMean(struct expData *exps);
/* Return the overall mean for the expression data. */

float expDataOverallMedian(struct expData *exps);
/* Return the overall median for the expression data. */

void expDataClipMin(struct expData *exps, float minGood, float minVal);
/* If an expData->expScores datapoint is < minGood, set it to minVal. */

void expDataDoLogRatioMeanOrMedian(struct expData *exps, boolean mean);
/* For the M x N sized expression matrix, change each value to be the ratio */
/* of that value to the mean or median of values in that value's row (gene). */
/* This involves no clumping of experiments to calculate the median/mean. */

void expDataDoLogRatioClumpExpRecord(struct expData *exps, struct expRecord *erList, int extrasIndex, enum combineMethod method);
/* Log ratio the expData list.  The ratio of the denominator is the median of */
/* medians or the mean of means of each group of experiments.  This grouping */
/* is defined by the expRecord/extrasIndex combination. The log is base 2. */

void expDataDoLogRatioGivenMedSpec(struct expData *exps, struct medSpec *specList, enum combineMethod method);
/* Same as expDataDoLogRatioClumpExpRecord except use the medSpec as the */
/* thing to base the groupings off of. */

#endif
