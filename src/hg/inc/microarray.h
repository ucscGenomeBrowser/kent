/* Microarray code common to hgc, hgTracks, and hgNear. */
/* Also use in loaders like hgRatioMicroarray and hgMedianMicroarray. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef MICROARRAY_H
#define MICROARRAY_H

#ifndef EXPRECORD_H
#include "expRecord.h"
#endif

#ifndef CART_H
#include "cart.h"
#endif

#ifndef BED_H
#include "bed.h"
#endif

#ifndef TRACKDB_H
#include "trackDb.h"
#endif

#ifndef CUSTOMTRACK_H
#include "customTrack.h"
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

/* Enum to use list the types of ways to color the microarray */
enum expColorType 
    {
    redGreen = 0,
    redBlue = 1,
    yellowBlue = 2,
    redBlueOnWhite = 3,
    redBlueOnYellow = 4,
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

void maExpDataDoLogRatioTranspose(struct expData *exps, boolean mean);
/* For the M x N sized expression matrix, change each value to be the ratio */
/* of that value to the mean or median of values in that value's column (probe). */
/* This involves no clumping of experiments to calculate the median/mean. */

void maExpDataDoLogRatioClumpExpRecord(struct expData *exps, struct expRecord *erList, int extrasIndex, enum maCombineMethod method);
/* Log ratio the expData list.  The ratio of the denominator is the median of */
/* medians or the mean of means of each group of experiments.  This grouping */
/* is defined by the expRecord/extrasIndex combination. The log is base 2. */

void maExpDataDoLogRatioGivenMedSpec(struct expData *exps, struct maMedSpec *specList, enum maCombineMethod method);
/* Same as maExpDataDoLogRatioClumpExpRecord except use the maMedSpec as the */
/* thing to base the groupings off of. */

void maExpDataAddConstant(struct expData *exps, double c);
/* Add a constant c to all of the microarray data. Ensure that NA values */
/* aren't inadvertantly created. */

struct maGrouping
/* Store the vital information for one of the "paragraphs" in the */
/* microarrayGroups.ra file. */
    {
    struct maGrouping *next;
    char *name;
    char *type;
    char *description;
    int size;
    int numGroups;
    int *expIds;
    int *groupSizes;
    char **names;
    };

struct microarrayGroups
/* Store all the vital info for a set of groupings in a microarrayGroups.ra */
/* file. */
    {
    int numCombinations;
    int numSubsets;
    struct maGrouping *allArrays;
    struct maGrouping *combineSettings;
    struct maGrouping *subsetSettings;
    struct maGrouping *defaultCombine;
    };

void maGroupingFree(struct maGrouping **pMag);
/* free up a maGrouping */

void maGroupingFreeList(struct maGrouping **pList);
/* Free up a list of maGroupings. */

void microarrayGroupsFree(struct microarrayGroups **pGroups);
/* Free up the microarrayGroups struct. */

struct microarrayGroups *maGetTrackGroupings(char *database, struct trackDb *tdb);
/* Get the settings from the .ra files and put them in a convenient struct. */

struct maGrouping *maGetGrouping(struct microarrayGroups *groupings, char *name);
/* Return the specfic grouping (combine or subset), or NULL if not found */

struct maGrouping *maGetGroupingFromCt(struct customTrack *ct);
/* Spoof an "all" maGrouping from a customTrack. */

struct bed *ctLoadMultScoresBedDb(struct customTrack *ct, char *chrom, int start, int end);
/* If the custom track is stored in a database, load it. */

struct maGrouping *maCombineGroupingFromCart(struct microarrayGroups *groupings, 
					     struct cart *cart, char *trackName);
/* Determine which grouping to use based on the cart status or lack thereof. */

struct maGrouping *maSubsetGroupingFromCart(struct microarrayGroups *groupings, 
					     struct cart *cart, char *trackName);
/* Determine which subsetting to use based on the cart status or lack thereof. */

int maSubsetOffsetFromCart(struct maGrouping *subset, struct cart* cart, char *trackName);

void maBedClumpGivenGrouping(struct bed *bedList, struct maGrouping *grouping, 
			     struct maGrouping *subset, int subsetOffset);
/* Clump (mean/median) a bed 15 given the grouping kind. */

struct maGrouping *maHashToMaGrouping(struct hash *oneGroup);
/* This converts a single "stanza" of the microarrayGroups.ra file to a
   maGrouping struct. */

enum expColorType getExpColorType(char *colorScheme);
/* From a color type return the respective enum. */

/* Linking to UI options... */

char *expRatioCombineDLName(char *trackName);

char *expRatioSubsetRadioName(char *trackName, struct microarrayGroups *groupings);

char *expRatioSubsetDLName(char *trackName, struct maGrouping *group);

#endif
