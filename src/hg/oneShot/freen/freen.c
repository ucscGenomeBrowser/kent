/* freen - My Pet Freen. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "md5.h"
#include "hex.h"
#include "pipeline.h"
#include "obscure.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen output\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void freen(char *input)
/* Test something */
{
verboseTimeInit();
char *old = md5HexForFile(input);
verboseTime(1, "old: %s", old);
char *nu1 = md5HexForFile(input);
verboseTime(1, "nu1: %s", nu1);
unsigned char md5[16];
md5ForFile(input, md5);
verboseTime(1, "nu2: %s", md5ToHex(md5));
char *gulp;
size_t gulpSize = 0;
readInGulp(input, &gulp, &gulpSize);
verboseTime(1, "read gulp");
char *nu3 = md5HexForBuf(gulp, gulpSize);
verboseTime(1, "nu3: %s", nu3);
char *nu4 = md5HexForString(gulp);
verboseTime(1, "nu4: %s", nu4);
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

