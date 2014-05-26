/* sets - Do set operations on one-word-per-line files.. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sets - Do set operations on one-word-per-line files.\n"
  "usage:\n"
  "   sets command files\n"
  "where \"command\" is one of:\n"
  "   intersect - intersect all the files\n"
  "   union - union all the files\n"
  "   subtract - with 2 files, remove stuff in the first file matching\n"
  "       matching stuff in the second file.\n"
  "   symetricDiff - Union of the files minus the intersect of the files.\n"
  );
}

/* No options yet. Maybe some future ones for multisets/bags, and related */
/* output options. */

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *hashList(struct slName *list)
/* slName -> hash... pretty simple. */
{
struct hash *myhash = newHash(23);
struct slName *cur;
for (cur = list; cur != NULL; cur = cur->next)
    hashAdd(myhash, cur->name, cloneString(cur->name));
return myhash;
} 

struct slName *thoseInHash(struct hash *wordHash, struct slName *list, boolean inverse)
/* Given a hash and a list, which in the list are also in the hash. */
{
struct slName *newList = NULL;
struct slName *cur;
for (cur = list; cur != NULL; cur = cur->next)
    {
    if ((hashLookup(wordHash, cur->name) != NULL) && (!inverse))
	slNameAddHead(&newList, cur->name);
    else if ((hashLookup(wordHash, cur->name) == NULL) && (inverse))
	slNameAddHead(&newList, cur->name);
    }
slReverse(&newList);
return newList;
}

void addMoreToHash(struct hash *wordHash, struct slName *list)
/* Add more to a hash (for union mostly). */
{
struct slName *cur;
for (cur = list; cur != NULL; cur = cur->next)
    if (hashLookup(wordHash, cur->name) == NULL)
	hashAdd(wordHash, cur->name, cloneString(cur->name));
}

struct slName *hashToList(struct hash *wordHash)
/* convert the hash back to a list. */
{
struct hashEl *elList = hashElListHash(wordHash);
struct slName *list = NULL;
struct hashEl *cur;
for (cur = elList; cur != NULL; cur = cur->next)
    slNameAddHead(&list, cur->name);
slNameSort(&list);
hashElFreeList(&elList);
return list;
}

struct slName *intersectFiles(struct slName *fileList)
/* Successively intersect files. */
{
struct slName *isectList = slNameLoadReal(fileList->name);
struct slName *curFile = fileList->next;
struct slName *cur;
while (curFile != NULL)
    {
    struct hash *isectHash = hashList(isectList);
    struct slName *someWords = slNameLoadReal(curFile->name);
    struct slName *bothWords = thoseInHash(isectHash, someWords, FALSE);
    slNameFreeList(&someWords);
    slNameFreeList(&isectList);
    isectList = bothWords;
    freeHashAndVals(&isectHash);
    curFile = curFile->next;
    }
return isectList;
}

struct slName *unionFiles(struct slName *fileList)
/* Successively union files. */
{
struct slName *initialList = slNameLoadReal(fileList->name);
struct hash *theHash = hashList(initialList);
struct slName *nextFile = fileList->next;
struct slName *unionList = NULL;
struct slName *cur;
while (nextFile != NULL)
    {
    struct slName *someWords = slNameLoadReal(nextFile->name);
    addMoreToHash(theHash, someWords);
    slNameFreeList(&someWords);
    nextFile = nextFile->next;
    }
unionList = hashToList(theHash);
hashFree(&theHash);
slNameFreeList(&initialList);
return unionList;
}

struct slName *subtractLists(struct slName *listA, struct slName *listB)
/* Subtract two lists. */
{
struct hash *theHash = hashList(listB);
struct slName *diffList = NULL, *cur;
for (cur = listA; cur != NULL; cur = cur->next)
    {
    if (hashLookup(theHash, cur->name) == NULL)
	slNameAddHead(&diffList, cur->name);
    }
freeHashAndVals(&theHash);
slReverse(&diffList);
return diffList;
}

struct slName *subtractFiles(struct slName *fileList)
/* Subtract two files using subtractLists */
{
struct slName *initialList = slNameLoadReal(fileList->name);
struct slName *subtractMe = slNameLoadReal(fileList->next->name);
struct slName *diffList = subtractLists(initialList, subtractMe);
slNameFreeList(&initialList);
slNameFreeList(&subtractMe);
return diffList;
}

void sets(char *operation, struct slName *fileList)
/* sets - Do set operations on one-word-per-line files.. */
{
struct slName *finalList = NULL, *cur;
if (sameWord(operation, "intersect"))
    finalList = intersectFiles(fileList);
else if (sameWord(operation, "union"))
    finalList = unionFiles(fileList);
else if (sameWord(operation, "subtract") && (slCount(fileList) == 2))
    finalList = subtractFiles(fileList);
else if (sameWord(operation, "symetricDiff"))
    {
    struct slName *theUnion = unionFiles(fileList);
    struct slName *theIntersect = intersectFiles(fileList);
    finalList = subtractLists(theUnion, theIntersect);
    slNameFreeList(&theUnion);
    slNameFreeList(&theIntersect);
    }
else
    usage();
/* Output here */
for (cur = finalList; cur != NULL; cur = cur->next)
    printf("%s\n", cur->name);
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct slName *fileList = NULL;
int i;
optionInit(&argc, argv, options);
if (argc < 4)
    usage();
/* make a list out of the filenames. */
for (i = 2; i < argc; i++)
    slNameAddHead(&fileList, argv[i]);
slReverse(&fileList);
sets(argv[1], fileList);
slNameFreeList(&fileList);
return 0;
}
