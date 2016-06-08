/* Copyright (C) 2016 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "aveStats.h"

static int cmpDouble(const void *va, const void *vb)
/* Compare two doubles. */
{
const double *a = va;
const double *b = vb;
double diff = *a - *b;
if (diff < 0)
   return -1;
else if (diff > 0)
   return 1;
else
   return 0;
}

static inline double calcMedian(double *array, int count)
/* Calculate the median of a list of numbers. */
{
// if the array has an odd number of elements choose the center element
if (count & 1) 
    return array[count / 2];

// return the mean of the two central elements if the number of elements is even
return (array[count/2 - 1] + array[count/2]) / 2.0;
}

struct aveStats *aveStatsCalc(double *array, unsigned count)
/* Compute statistics on an unsorted array of doubles. Use Tukey hinge method for quartiles. */
{
struct aveStats *as;

AllocVar(as);
as->count = count;

// special case for arrays of size 0 and 1
if (count == 0)
    return as;
else if (count == 1)
    {
    as->q1 = as->median = as->q3 = as->average = as->minVal = as->maxVal = as->total = array[0];
    as->var = as->stdDev = 0.0;
    return as;
    }

qsort(array, count, sizeof array[0], cmpDouble);

as->minVal = array[0];
as->maxVal = array[count-1];

double *lastValue = &array[count];
double *valuePtr;

double total = 0;
for( valuePtr = array; valuePtr < lastValue; valuePtr++)
    total += *valuePtr;
as->total = total;

double average;
as->average = average = total/count;

double oneVar, totalVar = 0;
for( valuePtr = array; valuePtr < lastValue; valuePtr++)
    {
    oneVar = (average - *valuePtr);
    totalVar += oneVar * oneVar;
    }

as->var = totalVar;
as->var /= count-1;  // use sample standard deviation
as->stdDev = sqrt(as->var);

as->median = calcMedian(array, count);

unsigned halfSize = count/2 + (count & 1);
as->q1 = calcMedian(array, halfSize);
as->q3 = calcMedian(&array[count/2], halfSize);

return as;
}
