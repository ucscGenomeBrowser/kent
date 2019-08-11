/* freen - My Pet Freen.  A pet freen is actually more dangerous than a wild one. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
#include "tokenizer.h"
#include "strex.h"
#include "hmac.h"

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

void freen(char *s, char *oldVal, char *newVal)
/* Test something */
{
char *sub = replaceChars(s, oldVal, newVal);
printf("%s\n", sub);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
freen(argv[1], argv[2], argv[3]);
return 0;
}

