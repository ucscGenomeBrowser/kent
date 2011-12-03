/* hmmstats.c - Stuff for doing statistical analysis in general and 
 * hidden Markov models in particular. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "hmmstats.h"


int scaledLog(double val)
/* Return scaled log of val. */
{
return round(logScaleFactor * log(val));
}

double oneOverSqrtTwoPi = 0.39894228;

double simpleGaussean(double x)
/* Gaussean distribution with standard deviation 1 and mean 0. */
{
return oneOverSqrtTwoPi * exp(-0.5*x*x );
}

double gaussean(double x, double mean, double sd)
/* Gaussean distribution with mean and standard deviation at point x  */
{
x -= mean;
x /= sd;
return oneOverSqrtTwoPi * exp(-0.5*x*x) / sd;
}

double calcVarianceFromSums(double sum, double sumSquares, bits64 n)
/* Calculate variance. */
{
double var = sumSquares - sum*sum/n;
if (n > 1)
    var /= n-1;
return var;
}

double calcStdFromSums(double sum, double sumSquares, bits64 n)
/* Calculate standard deviation. */
{
return sqrt(calcVarianceFromSums(sum, sumSquares, n));
}


