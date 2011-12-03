/* rt1dTest - Test out some stuff for one dimensional r trees.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "cirTree.h"
#include "crTree.h"


int blockSize = 64;
int itemsPerSlot = 32;	/* Set in main. */
boolean noCheckSort = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rt1dTest - Test out some stuff for one dimensional r trees.\n"
  "usage to create a rtree file:\n"
  "   rt1dTest create in.tab out.rtree\n"
  "to search a file\n"
  "   rt1dTest find in.tab in.rtree chrom start end\n"
  "The in.tab file is tab-separated in the format\n"
  "   <seqId> <start> <end> <name>\n"
  "The index created will associate ranges with positions in the input file\n"
  "options:\n"
  "   -blockSize=N - number of children per node in b+ tree. Default %d\n"
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

#define CHROMRANGE_NUM_COLS 3

struct chromIxRange
/* A chromosome and an interval inside it. */
    {
    struct chromIxRange *next;  /* Next in singly linked list. */
    bits32 chromIx;	/* Index into chromosome table. */
    bits32 start;	/* Start position in chromosome. */
    bits32 end;	/* One past last base in interval in chromosome. */
    bits64 fileOffset;	/* Offset of item in file we are indexing. */
    };

struct chromIxRange *chromIxRangeLoadAll(char *fileName, bits64 *retFileSize) 
/* Load all chromIxRange from a whitespace-separated file.
 * Dispose of this with chromIxRangeFreeList(). */
{
struct chromIxRange *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];

while (lineFileRow(lf, row))
    {
    AllocVar(el);
    el->chromIx = sqlUnsigned(row[0]);
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

int chromIxRangeCmp(const void *va, const void *vb)
/* Compare to sort based on chromIx,start,-end. */
{
const struct chromIxRange *a = *((struct chromIxRange **)va);
const struct chromIxRange *b = *((struct chromIxRange **)vb);
int dif;
dif = a->chromIx - b->chromIx;
if (dif == 0)
    dif = a->start - b->start;
if (dif == 0)
    dif = b->end - a->end;
return dif;
}

struct cirTreeRange chromIxRangeKey(const void *va, void *context)
/* Get key fields. */
{
const struct chromIxRange *a = *((struct chromIxRange **)va);
struct cirTreeRange ret;
ret.chromIx = a->chromIx;
ret.start = a->start;
ret.end = a->end;
return ret;
}

bits64 chromIxRangeOffset(const void *va, void *context)
{
const struct chromIxRange *a = *((struct chromIxRange **)va);
return a->fileOffset;
}

void rt1dCreateOld(char *inBed, char *outTree)
/* rt1dCreate - create a one dimensional range tree. */
{
bits64 fileSize;
struct chromIxRange **array, *el, *list = chromIxRangeLoadAll(inBed, &fileSize);
int count = slCount(list);
verbose(1, "read %d from %s\n", count, inBed);

/* Make array of pointers out of linked list. */
AllocArray(array, count);
int i;
for (i=0, el=list; i<count; ++i, el=el->next)
    array[i] = el;

cirTreeFileCreate(array, sizeof(array[0]), count, blockSize, itemsPerSlot, NULL,
	chromIxRangeKey, chromIxRangeOffset, fileSize, outTree);
}

void rt1dFindOld(char *tabFile, char *treeFile, bits32 chromIx, bits32 start, bits32 end)
/* rt1dCreate - find items in 1-D range tree. */
{
struct lineFile *lf = lineFileOpen(tabFile, TRUE);
struct cirTreeFile *crf = cirTreeFileOpen(treeFile);
struct fileOffsetSize *block, *blockList = cirTreeFindOverlappingBlocks(crf, chromIx, start, end);
uglyf("Got %d overlapping blocks\n", slCount(blockList));
for (block = blockList; block != NULL; block = block->next)
    {
    uglyf("block->offset %llu, block->size %llu\n", block->offset, block->size);
    lineFileSeek(lf, block->offset, SEEK_SET);
    bits64 sizeUsed = 0;
    while (sizeUsed < block->size)
        {
	char *line;
	int size;
	if (!lineFileNext(lf, &line, &size))
	    errAbort("Couldn't read %s\n", lf->fileName);
	char *parsedLine = cloneString(line);
	char *row[3];
	if (chopLine(parsedLine, row) != ArraySize(row))
	    errAbort("Badly formatted line of %s\n%s", lf->fileName, line);
	bits32 bedChromIx = sqlUnsigned(row[0]);
	bits32 bedStart = sqlUnsigned(row[1]);
	bits32 bedEnd = sqlUnsigned(row[2]);
	if (bedChromIx == chromIx && rangeIntersection(bedStart, bedEnd, start, end) > 0)
	    fprintf(stdout, "%s\n", line);
	freeMem(parsedLine);
	sizeUsed += size;
	}
    }
}

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

void rt1dCreate(char *inBed, char *outTree)
/* rt1dCreate - create a one dimensional range tree. */
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

void rt1dFind(char *tabFile, char *treeFile, char *chrom, bits32 start, bits32 end)
/* rt1dCreate - find items in 1-D range tree. */
{
struct lineFile *lf = lineFileOpen(tabFile, TRUE);
struct crTreeFile *crf = crTreeFileOpen(treeFile);
struct fileOffsetSize *block, *blockList = crTreeFindOverlappingBlocks(crf, chrom, start, end);
verbose(2, "Got %d overlapping blocks\n", slCount(blockList));
for (block = blockList; block != NULL; block = block->next)
    {
    verbose(2, "block->offset %llu, block->size %llu\n", block->offset, block->size);
    lineFileSeek(lf, block->offset, SEEK_SET);
    bits64 sizeUsed = 0;
    while (sizeUsed < block->size)
        {
	char *line;
	int size;
	if (!lineFileNext(lf, &line, &size))
	    errAbort("Couldn't read %s\n", lf->fileName);
	char *parsedLine = cloneString(line);
	char *row[3];
	if (chopLine(parsedLine, row) != ArraySize(row))
	    errAbort("Badly formatted line of %s\n%s", lf->fileName, line);
	char *bedChrom = row[0];
	bits32 bedStart = sqlUnsigned(row[1]);
	bits32 bedEnd = sqlUnsigned(row[2]);
	if (sameString(bedChrom, chrom) && rangeIntersection(bedStart, bedEnd, start, end) > 0)
	    fprintf(stdout, "%s\n", line);
	freeMem(parsedLine);
	sizeUsed += size;
	}
    }
crTreeFileClose(&crf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
blockSize = optionInt("blockSize", blockSize);
itemsPerSlot = optionInt("itemsPerSlot", (blockSize+1)/2);
noCheckSort = optionExists("noCheckSort");
if (argc < 2)
    usage();
char *command = argv[1];
if (sameWord(command, "create"))
    {
    if (argc != 4)
        usage();
    rt1dCreate(argv[2], argv[3]);
    }
else if (sameWord(command, "find"))
    {
    if (argc != 7)
        usage();
    rt1dFind(argv[2], argv[3], argv[4], sqlUnsigned(argv[5]), sqlUnsigned(argv[6]));
    }
else if (sameWord(command, "createOld"))
    {
    if (argc != 4)
        usage();
    rt1dCreateOld(argv[2], argv[3]);
    }
else if (sameWord(command, "findOld"))
    {
    if (argc != 7)
        usage();
    rt1dFindOld(argv[2], argv[3], sqlUnsigned(argv[4]), sqlUnsigned(argv[5]), sqlUnsigned(argv[6]));
    }
else
    usage();
return 0;
}
