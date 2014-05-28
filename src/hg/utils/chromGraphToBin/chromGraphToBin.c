/* chromGraphToBin - Make binary version of chromGraph.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "chromGraph.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "chromGraphToBin - Make binary version of chromGraph.\n"
  "usage:\n"
  "   chromGraphToBin in.tab out.chromGraph\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void chromGraphToBinary(char *inFile, char *outFile)
/* chromGraphToBin - Make binary version of chromGraph.. */
{
struct chromGraph *list = chromGraphLoadAll(inFile);
slSort(&list, chromGraphCmp);
chromGraphToBin(list, outFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
chromGraphToBinary(argv[1], argv[2]);
return 0;
}
