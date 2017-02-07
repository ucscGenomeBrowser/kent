/* freen - My Pet Freen. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "portable.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void freen(char *inFile)
/* Test something */
{
short s = -1;
unsigned short us = s;
short maxShort = (us/2);
short minShort = -maxShort - 1;
printf("s = %d, us=%d\n", (int)s, (int)us);
printf("minShort %d, maxShort %d\n", (int)minShort, (int)maxShort);

long long ll = -1;
unsigned long long ull = ll;
printf("ll = %lld, ull=%lld\n", ll, ull);
long long maxLongLong = ull/2;
long long minLongLong = -maxLongLong - 1;
printf("minLongLong %lld, maxLongLong %lld\n", minLongLong, maxLongLong);
printf("minLongLong 0x%llx, maxLongLong 0x%llx\n", minLongLong, maxLongLong);
printf("sizeof(int) %d, sizeof(long) %d, sizeof(long long) %d\n", 
    (int)sizeof(int), (int)sizeof(long), (int)sizeof(long long));
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}

