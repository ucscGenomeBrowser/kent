/* freen - My Pet Freen. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "genePred.h"
#include "rangeTree.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

double tenk[100000];
double zeros[100000];
long longs[100000];
float floats[100000];

double squareFloats(float *a, int count)
/* Square all numbers in an array */
{
float acc = 0;
int i;
for (i=0; i<count; ++i)
    {
    float x = a[i];
    acc += x*x;
    }
return acc;
}

double squareArray(double *a, int count)
/* Square all numbers in an array */
{
double acc = 0;
int i;
for (i=0; i<count; ++i)
    {
    double x = a[i];
    acc += x*x;
    }
return acc;
}

long squareLongArray(long *a, int count)
/* Square all numbers in an array */
{
long acc = 0;
int i;
for (i=0; i<count; ++i)
    {
    long x = a[i];
    acc += x*x;
    }
return acc;
}

void freen(char *chrom)
/* Test something */
{
int i;
for (i=0; i<ArraySize(zeros); ++i)
    {
    longs[i] = i + 123456;
    tenk[i] = i + 123456.789;
    zeros[i] = 0;
    floats[i] = 1234 + i*0.001;
    }

int outer = 1000;
uglyTime(0);
for (i=0; i<outer; ++i)
    squareArray(tenk, ArraySize(tenk));
uglyTime("Real numbers");
for (i=0; i<outer; ++i)
    squareArray(zeros, ArraySize(zeros));
uglyTime("Zeroes");
for (i=0; i<outer; ++i)
    squareLongArray(longs, ArraySize(longs));
uglyTime("Long integers (64 bit)");
for (i=0; i<outer; ++i)
    squareFloats(floats, ArraySize(floats));
uglyTime("single precision float");
}

int main(int argc, char *argv[])
/* Process command line. */
{
// optionInit(&argc, argv, options);
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}

