/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef MATHWIG_H
#define MATHWIG_H

#include "cart.h"

double *mathWigGetValuesMissing(char *equation, char *chrom, unsigned start, unsigned end);
/* Build an array of doubles that is calculated from bigWig's listed
 * in equation in the requested region. Math with missing data results in missing data. */

double *mathWigGetValues(char *equation, char *chrom, unsigned start, unsigned end);
/* Build an array of doubles that is calculated from bigWig's listed
 * in equation in the requested region. */

#endif
