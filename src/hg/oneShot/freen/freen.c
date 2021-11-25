/* freen - My Pet Freen.  A pet freen is actually more dangerous than a wild one. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "portable.h"
#include "obscure.h"
#include "localmem.h"
#include "csv.h"
#include "hex.h"
#include "memgfx.h"
#include "correlate.h"
#include "vMatrix.h"

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen refMatrix.tsv sampleMatrix.tsv outColors.tsv\n");
}

void freen(char *input)
/* Test something */
{
char *s;
struct dyString *scratch = dyStringNew(0);
while ((s = csvParseNext(&input, scratch)) != NULL)
    printf("'%s'\n", s);
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

