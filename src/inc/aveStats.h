
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

struct aveStats *aveStatsCalc(double *array, int count);
#endif
