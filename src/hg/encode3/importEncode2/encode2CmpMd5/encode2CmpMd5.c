/* encode2CmpMd5 - Do comparisons of md5sums derived from two sources.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2CmpMd5 - Do comparisons of md5sums derived from two sources.\n"
  "usage:\n"
  "   encode2CmpMd5 a.md5 b.md5 aOnly.tab bOnly.tab match.tab mismatch.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct md5Item
/* A file and a sum. */
    {
    struct md5Item *next;   /* Next in list. */
    char *name;		    /* File Name */
    char *md5;		    /* Sum */
    };

void md5ItemLoadAll(char *fileName, struct md5Item **retList, struct hash **retHash)
/* Read in all items from file. */
{
struct hash *hash = hashNew(16);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
struct md5Item *item, *list = NULL;
while (lineFileRow(lf, row))
    {
    AllocVar(item);
    item->name = cloneString(row[1]);
    item->md5 = cloneString(row[0]);
    slAddHead(&list, item);
    if (hashLookup(hash, item->name))
        errAbort("%s duplicated line %d of %s", item->name, lf->lineIx, lf->fileName);
    hashAdd(hash, item->name, item);
    }
slReverse(&list);
*retList = list;
*retHash = hash;
}

int onlyInList(struct md5Item *list, struct hash *hash, char *outFile)
/* Write items in list that are not in hash to outFile.  Return # of such items. */
{
int onlyCount = 0;
FILE *f = mustOpen(outFile, "w");
struct md5Item *item;
for (item = list; item != NULL; item = item->next)
    {
    if (!hashLookup(hash, item->name))
        {
	++onlyCount;
	fprintf(f, "%s\t%s\n", item->name, item->md5);
	}
    }
carefulClose(&f);
return onlyCount;
}

void encode2CmpMd5(char *inFileA, char *inFileB, 
    char *aOnlyOut, char *bOnlyOut, char *matchOut, char *mismatchOut)
/* encode2CmpMd5 - Do comparisons of md5sums derived from two sources.. */
{
/* Load in input. */
struct md5Item *aList = NULL, *bList = NULL;
struct hash *aHash = NULL, *bHash = NULL;
md5ItemLoadAll(inFileA, &aList, &aHash);
md5ItemLoadAll(inFileB, &bList, &bHash);
verbose(1, "%d items in %s, %d in %s\n", aHash->elCount, inFileA, bHash->elCount, inFileB);

/* Take care of items in one but not other. */
int aOnlyCount = onlyInList(aList, bHash, aOnlyOut);
int bOnlyCount = onlyInList(bList, aHash, bOnlyOut);
verbose(1, "%d items only in %s\n", aOnlyCount, inFileA);
verbose(1, "%d items only in %s\n", bOnlyCount, inFileB);

/* Do match/mismatch bits. */
FILE *matchF = mustOpen(matchOut, "w");
FILE *missF = mustOpen(mismatchOut, "w");
int matchCount = 0, missCount = 0;
struct md5Item *aItem, *bItem;
for (aItem = aList; aItem != NULL; aItem = aItem->next)
    {
    bItem = hashFindVal(bHash, aItem->name);
    if (bItem != NULL)
        {
	if (sameString(aItem->md5, bItem->md5))
	    {
	    ++matchCount;
	    fprintf(matchF, "%s\t%s\n", aItem->name, aItem->md5);
	    }
	else
	    {
	    ++missCount;
	    fprintf(missF, "%s\t%s\t%s\n", aItem->name, aItem->md5, bItem->md5);
	    }
	}
    }
verbose(1, "%d sums match, %d mismatch\n", matchCount, missCount);
carefulClose(&matchF);
carefulClose(&missF);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
encode2CmpMd5(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
