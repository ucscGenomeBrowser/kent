/* bigBedNamedItems - Extract item(s) of given name(s) from bigBed. */
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

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigBedNamedItems - Extract item of given name from bigBed\n"
  "usage:\n"
  "   bigBedNamedItems file.bb name output.bed\n"
  "options:\n"
  "   -nameFile - if set, treat name parameter as file full of space delimited names\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"nameFile", OPTION_BOOLEAN},
   {NULL, 0},
};

void bigBedNamedItems(char *bigBedFile, char *name, char *outFile)
/* bigBedNamedItems - Extract item(s) of given name(s) from bigBed. */
{
struct bbiFile *bbi = bigBedFileOpen(bigBedFile);
FILE *f = mustOpen(outFile, "w");
struct lm *lm = lmInit(0);
struct bigBedInterval *intervalList;
if (optionExists("nameFile"))
    {
    int wordCount = 0;
    char **words;
    char *buf;
    readAllWords(name, &words, &wordCount, &buf);
    intervalList = bigBedMultiNameQuery(bbi, words, wordCount, lm);
    }
else
    {
    intervalList = bigBedNameQuery(bbi, name, lm);
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
bigBedNamedItems(argv[1], argv[2], argv[3]);
return 0;
}
