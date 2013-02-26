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
  "bigBedNamedItems - Extract item(s) of given name(s) from bigBed\n"
  "usage:\n"
  "   bigBedNamedItems file.bb name output.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void bigBedAttachNameIndex(struct bbiFile *bbi)
/* Attach name index part of bbiFile to bbi */
{
if (bbi->nameBpt == NULL)
    {
    if (bbi->nameIndexOffset == 0)
	errAbort("%s has no name index", bbi->fileName);
    udcSeek(bbi->udc, bbi->nameIndexOffset);
    bbi->nameBpt = bptFileAttach(bbi->fileName, bbi->udc);
    }
}

struct bigBedInterval *bigBedNameQuery(struct bbiFile *bbi, char *name)
/* Return (possibly empty) list of items of given name in bigBed */
{
bigBedAttachNameIndex(bbi);
boolean isSwapped = bbi->isSwapped;
struct offsetSize {bits64 offset; bits64 size;} block;
if (bptFileFind(bbi->nameBpt, name, strlen(name), &block, sizeof(block)))
    {
    if (bbi->isSwapped)
	{
	block.offset = byteSwap64(block.offset);
	block.size = byteSwap64(block.size);
	}
    uglyf("Whoohoo, found matching block at offset %lld\n", block.offset);


    /* Read in raw data */
    udcSeek(bbi->udc, block.offset);
    char *rawData = needLargeMem(block.size);
    udcRead(bbi->udc, rawData, block.size);

    /* Optionally uncompress data, and set data pointer to uncompressed version. */
    char *uncompressedData = NULL;
    char *data = NULL;
    if (bbi->uncompressBufSize > 0)
	{
	data = uncompressedData = needLargeMem(bbi->uncompressBufSize);
	size_t uncSize = zUncompress(rawData, block.size, uncompressedData, bbi->uncompressBufSize);
	assert(uncSize <= bbi->uncompressBufSize);
	}
    else
        data = rawData;

    /* Read chromosome index, start, end */
    int chromIx = memReadBits32(&data, isSwapped);
    int firstItemStart = memReadBits32(&data, isSwapped);   // Start of first item, not useful
    int firstItemEnd = memReadBits32(&data, isSwapped);	    // End of first item, not useful

    uglyf("Got chromIx %d, start %d, end %d\n", chromIx, firstItemStart, firstItemEnd);
    char nameBuf[bbi->chromBpt->keySize+1];
    bptStringKeyAtPos(bbi->chromBpt, chromIx, nameBuf, sizeof(nameBuf));
    uglyf("Chrom is %s\n", nameBuf);

    /* Clean up temporary buffers. */
    freez(&rawData);
    freez(&uncompressedData);
    data = NULL;
    }
return NULL;
}

void bigBedNamedItems(char *bigBedFile, char *name, char *outFile)
/* bigBedNamedItems - Extract item(s) of given name(s) from bigBed. */
{
struct bbiFile *bbi = bigBedFileOpen(bigBedFile);
struct bigBedInterval *interval, *intervalList = bigBedNameQuery(bbi, name);
FILE *f = mustOpen(outFile, "w");
char *chromName = "uglyFoo";
for (interval = intervalList; interval != NULL; interval = interval->next)
    {
    fprintf(f, "%s\t%u\t%u", chromName, interval->start, interval->end);
    char *rest = interval->rest;
    if (rest != NULL)
	fprintf(f, "\t%s\n", rest);
    else
	fprintf(f, "\n");
    }
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
