/* bigBedNamedItems - Extract item(s) of given name(s) from bigBed. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "udc.h"
#include "bPlusTree.h"
#include "bigBed.h"
#include "obscure.h"
#include "zlibFace.h"

char *field = "name";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigBedNamedItems - Extract item of given name from bigBed\n"
  "usage:\n"
  "   bigBedNamedItems file.bb name output.bed\n"
  "options:\n"
  "   -nameFile - if set, treat name parameter as file full of space delimited names\n"
  "   -field=fieldName - use index on field name, default is \"name\"\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"nameFile", OPTION_BOOLEAN},
   {"field", OPTION_STRING},
   {NULL, 0},
};

void bigBedNamedItems(char *bigBedFile, char *name, char *outFile)
/* bigBedNamedItems - Extract item(s) of given name(s) from bigBed. */
{
struct bbiFile *bbi = bigBedFileOpen(bigBedFile);
FILE *f = mustOpen(outFile, "w");
struct lm *lm = lmInit(0);
struct bigBedInterval *intervalList;
int fieldIx;
struct bptFile *bpt = bigBedOpenExtraIndex(bbi, field, &fieldIx);
if (optionExists("nameFile"))
    {
    int wordCount = 0;
    char **words;
    char *buf;
    readAllWords(name, &words, &wordCount, &buf);
    intervalList = bigBedMultiNameQuery(bbi, bpt, fieldIx, words, wordCount, lm);
    }
else
    {
    intervalList = bigBedNameQuery(bbi, bpt, fieldIx, name, lm);
    }
bigBedIntervalListToBedFile(bbi, intervalList, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
field = optionVal("field", field);
bigBedNamedItems(argv[1], argv[2], argv[3]);
return 0;
}
