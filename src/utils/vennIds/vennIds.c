/* vennIds - From two lists of unique IDs, calculate number shared between each list and number 
 * only in each list.. */

/* Copyright (C) 2011 The Regents of the University of California 
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
  "vennIds - From two lists of unique IDs, calculate number shared between each list and number\n"
  "only in each list.\n"
  "usage:\n"
  "   vennIds a.lst b.lst\n"
  "where a.lst and b.lst are whitespace delimited files of IDs\n"
  "options:\n"
  "   -both=both.out\n  - if set put ids that are in both here, one per line"
  "   -aOnly=aOnly.out  - if set put ids that are in a.lst only here, one per line\n"
  "   -bOnly=bOnly.out  - if set put ids that are in b.lst only here, one per line\n"
  );
}

static struct optionSpec options[] = {
   {"aOnly", OPTION_STRING},
   {"bOnly", OPTION_STRING},
   {NULL, 0},
};

struct hash *hashUniqueWords(char *fileName, char **words, int wordCount)
/* Create a new hash filled with words, checking that they are unique. */
{
struct hash *hash = hashNew(0);
int i;
for (i=0; i<wordCount; ++i)
    {
    char *word = words[i];
    if (hashLookup(hash, word) != NULL)
        errAbort("Identifier %s is duplicated in file %s", word, fileName);
    hashAdd(hash, word, NULL);
    }
return hash;
}

FILE *createFileForOption(char *option)
/* If option exists, create a file to write named by the option val. */
{
char *fileName = optionVal(option, NULL);
if (fileName == NULL)
    return NULL;
else
    return mustOpen(fileName, "w");
}

void vennIds(char *aFile, char *bFile)
/* vennIds - From two lists of unique IDs, calculate number shared between each list and number only in each list.. */
{
/* Read all words in both files into two arrays. */
int aWordCount = 0, bWordCount = 0;
char **aWords, *aBuf, **bWords, *bBuf;
readAllWords(aFile, &aWords, &aWordCount, &aBuf);
readAllWords(bFile, &bWords, &bWordCount, &bBuf);

/* Make hashes, ensuring that each word is unique. */
struct hash *aHash = hashUniqueWords(aFile, aWords, aWordCount);
struct hash *bHash = hashUniqueWords(bFile, bWords, bWordCount);

/* Open up output files if any. */
FILE *aOnlyOut = createFileForOption("aOnly");
FILE *bOnlyOut = createFileForOption("bOnly");
FILE *bothOut = createFileForOption("both");

/* Count number of words that are in both. */
int i, sharedCount = 0;
for (i=0; i<aWordCount; ++i)
    {
    char *aWord = aWords[i];
    if (hashLookup(bHash, aWord))
	{
	if (bothOut != NULL)
	    fprintf(bothOut, "%s\n", aWord);
        ++sharedCount;
	}
    else
        {
	if (aOnlyOut != NULL)
	    fprintf(aOnlyOut, "%s\n", aWord);
	}
    }

/* If we are outputting bOnly, we need to scan through bWords too. */
if (bOnlyOut)
    {
    for (i=0; i<bWordCount; ++i)
	{
	char *bWord = bWords[i];
	if (!hashLookup(aHash, bWord))
	    fprintf(bOnlyOut, "%s\n", bWord);
	}
    }

/* Calculate overlap statistics. */
double total = aWordCount + bWordCount - sharedCount;
int aOnlyCount = aWordCount - sharedCount;
int bOnlyCount = bWordCount - sharedCount;

/* Print result. */
printf("%d (%4.2f%%) only in %s\n", aOnlyCount, 100.0 * aOnlyCount / total, aFile);
printf("%d (%4.2f%%) in both\n", sharedCount, 100.0 * sharedCount/total);
printf("%d (%4.2f%%) only in %s\n", bOnlyCount, 100.0 * bOnlyCount / total, bFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
vennIds(argv[1], argv[2]);
return 0;
}
