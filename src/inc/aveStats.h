
#ifndef AVESTATS_H
#define AVESTATS_H

#include <float.h>

struct aveStats
{
    double q1;
    double median;
    double q3;
    double average;
    double minVal;
    double maxVal;
    unsigned count;
    double total;
    double var;
    double stdDev;
};

struct aveStats *aveStatsCalc(double *array, unsigned count);
/* Compute statistics on an unsorted array of doubles. Use Tukey hinge method for quartiles. */
#endif
