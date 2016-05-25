#include "common.h"
#include "aveStats.h"

static int cmpDouble(const void *va, const void *vb)
/* Compare two slNames. */
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

struct aveStats *aveStatsCalc(double *array, int count)
/* Compute stats on sorted array */
{
struct aveStats *as;

AllocVar(as);
if (count == 0)
    return as;

qsort(array, count, sizeof(array[0]), cmpDouble);

as->count = count;

double val, minVal = DBL_MAX, maxVal = -DBL_MAX;
double total = 0, average;
int i;
int q1Index, q3Index;		/*	quartile positions	*/
double oneVar, totalVar = 0;

for (i=0; i<count; ++i)
    {
    val = array[i];
    if (minVal > val) minVal = val;
    if (maxVal < val) maxVal = val;
    total += val;
    }

as->average = average = total/count;
as->minVal = minVal;
as->maxVal = maxVal;
as->total = total;

q1Index = (count+1)/4;		/*	one fourth, rounded down	*/
q3Index = (3*(count+1))/4;	/*	three fourths, rounded down	*/
if (q1Index < (count-1))
    {
    double range = array[q1Index+1] - array[q1Index];
    as->q1 = array[q1Index] +
        ((((double)count+1.0)/4.0)-(double)q1Index)*range;
    }
else
    as->q1 = array[q1Index];
if (q3Index < (count-1))
    {
    double range = array[q3Index+1] - array[q3Index];
    as->q3 = array[q3Index] +
        ((3.0*((double)count+1.0)/4.0)-(double)q3Index)*range;
    }
else
    as->q3 = array[q3Index];

for (i=0; i<count; ++i)
    {
    val = array[i];
    oneVar = (average-val);
    totalVar += oneVar*oneVar;
    }

as->var = totalVar;
if (count > 1)
    as->var /= count-1;
as->stdDev = sqrt(as->var);
as->median = array[count/2];

return as;
}

