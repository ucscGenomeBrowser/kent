/* crTreeIndexBed - Create an index for a bed file.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "crTree.h"


int blockSize = 1024;
int itemsPerSlot = 32;	/* Set in main. */
boolean noCheckSort = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "crTreeIndexBed - Create an index for a bed file.\n"
  "usage:\n"
  "   crTreeIndexBed in.bed out.cr\n"
  "options:\n"
  "   -blockSize=N - number of children per node in index tree. Default %d\n"
  "   -itemsPerSlot=N - number of items per index slot. Default is half block size\n"
  "   -noCheckSort - Don't check sorting order of in.tab\n"
  , blockSize
  );
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {"itemsPerSlot", OPTION_INT},
   {"noCheckSort", OPTION_BOOLEAN},
   {NULL, 0},
};

struct crTreeItem *scanAll(char *fileName, struct hash *chromNameHash, 
	bits64 *retFileSize) 
/* Load all crTreeItem from a whitespace-separated file.
 * Dispose of this with chromRangeFreeList(). */
{
struct crTreeItem *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];

while (lineFileRow(lf, row))
    {
    AllocVar(el);
    el->chrom = hashStoreName(chromNameHash, row[0]);
    el->start = sqlUnsigned(row[1]);
    el->end = sqlUnsigned(row[2]);
    el->fileOffset = lineFileTell(lf);
    slAddHead(&list, el);
    }
*retFileSize = lineFileTell(lf);
lineFileClose(&lf);
slReverse(&list);
return list;
}

void crTreeIndexBed(char *inBed, char *outTree)
/* crTreeIndexBed - Create an index for a bed file.. */
{
/* Load input and create chromosome hash. */
struct hash *chromHash = hashNew(0);
bits64 fileSize;
struct crTreeItem *itemList = scanAll(inBed, chromHash, &fileSize);

/* Call library function to create index file. */
if (!noCheckSort)
    crTreeFileCreateInputCheck(itemList, chromHash, blockSize, itemsPerSlot, fileSize, outTree);
crTreeFileCreate(itemList, chromHash, blockSize, itemsPerSlot, fileSize, outTree);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
blockSize = optionInt("blockSize", blockSize);
itemsPerSlot = optionInt("itemsPerSlot", (blockSize+1)/2);
noCheckSort = optionExists("noCheckSort");
if (argc != 3)
    usage();
crTreeIndexBed(argv[1], argv[2]);
return 0;
}
