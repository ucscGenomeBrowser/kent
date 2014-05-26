/* alignSeqSizes - Parse and store query and target sequence sizes for use
 * when converting or parsing alignments */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "alignSeqSizes.h"
#include "linefile.h"
#include "hash.h"
#include "sqlNum.h"
#include "obscure.h"

struct alignSeqSizes
/* table of sequence sizes */
{
    struct hash* qSizes;
    struct hash* tSizes;
};

static void addSize(char *name, unsigned size, struct hash *sizes)
/* add a name and size */
{
struct hashEl *hel = hashLookup(sizes, name);
if (hel != NULL)
    {
    /* size must be the same if duplicated */
    if (ptToInt(hel->val) != size)
        errAbort("sequence %s already specified as size %d, can't set to %d",
                 name, ptToInt(hel->val), size);
    }
else
    hashAdd(sizes, name, intToPt(size));

}

static void readSizesFile(char *fileName, struct hash *sizes)
/* Read tab-separated file into hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
while (lineFileChopNextTab(lf, row, ArraySize(row)))
    addSize(row[0], lineFileNeedNum(lf, row, 1), sizes);
lineFileClose(&lf);
}

static void parseSizesStr(char* sizesStr, struct hash* sizes)
/* parse string with pairs of name and value */
{
char **words;
int numWords = chopByWhite(sizesStr, NULL, 0);
int i;

if (numWords % 1)
    errAbort("sizes string must contain whitespace seprated pairs of name and size: %s",
             sizesStr);
words = needMem(numWords);
chopByWhite(sizesStr, words, numWords);
for (i = 0; i < numWords; i += 2)
    addSize(words[i], sqlUnsigned(words[i+1]), sizes);
freez(&words);
}

struct alignSeqSizes* alignSeqSizesNew(char *querySizesFile, char *querySizesStr,
                                       char *targetSizesFile, char *targetSizesStr)
/* Allocate a new object.  If *File arguments are not null, it is a tab
 * separated file of name and size.  If sizesStr is not null, it is a string
 * with whitespace-seperated pairs of name and size. */
{
struct alignSeqSizes* ass;
AllocVar(ass);
ass->qSizes = newHash(0);
ass->tSizes = newHash(0);

if (querySizesFile != NULL)
    readSizesFile(querySizesFile, ass->qSizes);
if (querySizesStr != NULL)
    parseSizesStr(querySizesStr, ass->qSizes);
if (targetSizesFile != NULL)
    readSizesFile(targetSizesFile, ass->tSizes);
if (targetSizesStr != NULL)
    parseSizesStr(targetSizesStr, ass->tSizes);

return ass;
}

void alignSeqSizesFree(struct alignSeqSizes** assPtr)
/* free an alignSeqSizes object */
{
struct alignSeqSizes* ass = *assPtr;
if (ass != NULL)
    {
    hashFree(&ass->qSizes);
    hashFree(&ass->tSizes);
    freez(assPtr);
    }
}

int alignSeqSizesMustGetQuery(struct alignSeqSizes* ass, char *name)
/* Find size of a query sequence, or error if not found */
{
return ptToInt(hashMustFindVal(ass->qSizes, name));
}

int alignSeqSizesMustGetTarget(struct alignSeqSizes* ass, char *name)
/* Find size of a query sequence, or error if not found */
{
return ptToInt(hashMustFindVal(ass->tSizes, name));
}

