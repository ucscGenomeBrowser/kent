/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "sqlList.h"

#define TEST0   "0,"
#define SUM0    0.0
#define TEST1   "0,1.3,"
#define SUM1    1.3
#define TEST2   "0,0.239,0,0,0,0,"
#define SUM2    0.239
#define TEST3    "6.886,6.083,4.729,5.91,6.371,6.007,8.768,4.202,4.455,4.64,10.097,10.619,6.108,5.037,5.018,4.808,4.543,4.495,5.576,4.57,8.275,4.707,2.55,9.091,9.885,8.17,7.392,7.735,5.353,7.124,8.617,3.426,2.375,7.669,3.826,7.094,6.365,3.263,10.723,10.507,4.843,9.193,13.25,11.635,11.771,8.641,10.448,6.522,9.313,10.304,9.987,9.067,6.12,"

int main(int argc, char *argv[])
{
if (sqlSumDoublesCommaSep(TEST0) != SUM0)
    printf("TEST0 ERROR: %s %0.2f\n", TEST0, SUM0);
else
    printf("TEST0 PASSED\n");

if (sqlSumDoublesCommaSep(TEST1) != SUM1)
    printf("TEST1 ERROR: %s %0.2f\n", TEST1, SUM1);
else
    printf("TEST1 PASSED\n");

if (sqlSumDoublesCommaSep(TEST2) != SUM2)
    printf("TEST2 ERROR: %s %0.2f\n", TEST2, SUM2);
else
    printf("TEST2 PASSED\n");

printf("TEST3: %s %0.2f\n", TEST3, sqlSumDoublesCommaSep(TEST3));
return 0;
}

