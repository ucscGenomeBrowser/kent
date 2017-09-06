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
#include "obscure.h"
#include "csv.h"

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

void freen(char *input)
/* Test something */
{
uglyf("input is '%s'\n", input);
if (csvNeedsParsing(input))
    {
    printf("Parses to:\n");
    char *pt = input;
    struct dyString *scratch = dyStringNew(0);
    char *s;
    while ((s = csvParseNext(&pt, scratch)) != NULL)
	printf("'%s'\n", s);
    }
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

