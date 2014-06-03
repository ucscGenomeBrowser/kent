/* nibSize - print out nib sizes */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "nib.h"
#include "options.h"


static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};
void usage()
/* Explain usage and exit. */
{
errAbort(
  "nibSize - print size of nibs\n"
  "usage:\n"
  "   nibSize nib1 [...]\n"
  );
}

void printNibSize(char* nibFile)
/* print the size of a nib */
{
FILE* fh;
int size;
char name[128];

nibOpenVerify(nibFile, &fh, &size);
splitPath(nibFile, NULL, name, NULL);
printf("%s\t%s\t%d\n", nibFile, name, size);

carefulClose(&fh);
}

void nibSize(int numNibs, char** nibFiles)
/* nibSize - print nib sizes */
{
int i;
for (i = 0; i < numNibs; i++)
    printNibSize(nibFiles[i]);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 2)
    usage();
nibSize(argc-1, argv+1);
return 0;
}
