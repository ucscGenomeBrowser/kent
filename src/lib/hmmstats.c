/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* hmmstats.c - Stuff for doing statistical analysis in general and 
 * hidden Markov models in particular. */

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


