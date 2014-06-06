/* reverseLineOrder - Write out a file with first line last and last line first.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "reverseLineOrder - Write out a file with first line last and last line first.\n"
  "usage:\n"
  "   reverseLineOrder input output\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void reverseLineOrder(char *input, char *output)
/* reverseLineOrder - Write out a file with first line last and last line first.. */
{
struct slName *el, *list = readAllLines(input);
slReverse(&list);
FILE *f = mustOpen(output, "w");
for (el = list; el != NULL; el = el->next)
    {
    fprintf(f, "%s\n", el->name);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
reverseLineOrder(argv[1], argv[2]);
return 0;
}
