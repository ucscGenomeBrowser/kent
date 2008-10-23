/* greedyPack - Given a given size, and a list of chromosomes (or anything really) of 
 * various sizes, figure out how to bundle together things into bundles of that size or 
 * less, using relatively few bundles, using a very simple greedy algorithm. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "sqlNum.h"
#include "options.h"

static char const rcsid[] = "$Id: greedyPack.c,v 1.1 2008/10/23 02:31:23 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "greedyPack - Given a given size, and a list of chromosomes (or anything really) of "
  "various sizes, figure out how to bundle together things into bundles of that size or "
  "less, using relatively few bundles, using a very simple greedy algorithm.\n"
  "usage:\n"
  "   greedyPack bundleSize chrom.sizes bundleList\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct item
/* A named item of a size. */
    {
    struct item *next;
    char *name;
    int size;
    };

int cmpItemSize(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct item *a = *((struct item **)va);
const struct item *b = *((struct item **)vb);
return b->size - a->size;
}

struct item *readItemFile(char *fileName)
/* Read in two column file: columns are <name> <size>. Return list of items. */
{
struct item *list = NULL, *item;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
while (lineFileRow(lf, row))
    {
    AllocVar(item);
    item->name = cloneString(row[0]);
    item->size = lineFileNeedNum(lf, row, 1);
    slAddHead(&list, item);
    }
slReverse(&list);
return list;
}

struct item *bundleItems(struct item *list, int bundleSize, FILE *f)
/* Scan list, removing items that will fit in bundle from it, and writing them to file. */
{
int sizeLeft = bundleSize;
struct item *newList = NULL, *item, *next;
boolean firstItem = TRUE;
for (item = list; item != NULL; item = next)
    {
    next = item->next;
    if (item->size <= sizeLeft)
        {
	if (!firstItem)
	    fprintf(f, "\t");
	else
	    firstItem = FALSE;
	fprintf(f, "%s", item->name);
	sizeLeft -= item->size;
	}
    else
        {
	slAddHead(&newList, item);
	}
    }
fprintf(f, "\n");
slReverse(&newList);
return newList;
}

void greedyPack(char *bundleSizeString, char *itemSizeFile,  char *outFile)
/* greedyPack - Given a given size, and a list of chromosomes (or anything really) of 
 * various sizes, figure out how to bundle together things into bundles of that size or 
 * less, using relatively few bundles, using a very simple greedy algorithm. */
{
int bundleSize = sqlUnsigned(bundleSizeString);
struct item *itemList = readItemFile(itemSizeFile);
FILE *f = mustOpen(outFile, "w");

slSort(&itemList, cmpItemSize);
while (itemList != NULL)
     itemList = bundleItems(itemList, bundleSize, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
greedyPack(argv[1], argv[2], argv[3]);
return 0;
}
