/* Copyright (C) 2017 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef MATHWIG_H
#define MATHWIG_H

#include "cart.h"

double *mathWigGetValues(char *db, char *equation, char *chrom, unsigned start, unsigned end, boolean missingIsZero);
/* Build an array of doubles that is calculated from bigWig's listed
 * in equation in the requested region. */

#endif
