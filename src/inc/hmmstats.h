/* hmmstats.h - Stuff for doing statistical analysis in general and 
 * hidden Markov models in particular. 
 * 
 * This file is copyright 2000 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef HMMSTATS_H
#define HMMSTATS_H

int scaledLog(double val);
/* Return scaled log of val. */

#define logScaleFactor 1000
/* Amount we scale logs by. */

double simpleGaussean(double x);
/* Gaussean distribution with standard deviation 1 and mean 0. */

double gaussean(double x, double mean, double sd);
/* Gaussean distribution with mean and standard deviation at point x  */

double calcVarianceFromSums(double sum, double sumSquares, bits64 n);
/* Calculate variance. */

double calcStdFromSums(double sum, double sumSquares, bits64 n);
/* Calculate standard deviation. */

#endif /* HMMSTATS_H */

