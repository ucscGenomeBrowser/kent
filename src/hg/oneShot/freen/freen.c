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

//static struct optionSpec options[] = {
 //  {NULL, 0},
//};

int squareP(int a)
{
return a*a;
}

void freen(char *chrom)
/* Test something */
{
printf("%'d\n", squareP(1000));
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

