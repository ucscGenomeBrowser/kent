/* freen - My Pet Freen. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

//static struct optionSpec options[] = {
 //  {NULL, 0},
//};

void freen(char *input)
/* Test something */
{
char *pt = cloneString(input);
char *var, *val;
while (cgiParseNext(&pt, &var, &val))
    {
    printf("%s\t%s\n", var, val); 
    }
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

