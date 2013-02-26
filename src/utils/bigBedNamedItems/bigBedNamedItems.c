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

boolean bigBedNameQuery(struct bbiFile *bbi, char *name, FILE *f)
/* Write item matching name to file.  Return TRUE if anything written.  */
{
bigBedAttachNameIndex(bbi);
boolean isSwapped = bbi->isSwapped;
struct offsetSize {bits64 offset; bits64 size;} block;
boolean didWrite = FALSE;
if (bptFileFind(bbi->nameBpt, name, strlen(name), &block, sizeof(block)))
    {
    if (bbi->isSwapped)
	{
	block.offset = byteSwap64(block.offset);
	block.size = byteSwap64(block.size);
	}

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

    /* Set up for "memRead" routines to more or less treat memory block like file */
    char *blockPt = data, *blockEnd = data + block.size;
    struct dyString *dy = dyStringNew(32); // Keep bits outside of chrom/start/end here


    /* Read next record into local variables. */
    while (blockPt < blockEnd)
	{
	bits32 chromIx = memReadBits32(&blockPt, isSwapped);
	bits32 s = memReadBits32(&blockPt, isSwapped);
	bits32 e = memReadBits32(&blockPt, isSwapped);
	int c;
	dyStringClear(dy);
	while ((c = *blockPt++) >= 0)
	    {
	    if (c == 0)
		break;
	    dyStringAppendC(dy, c);
	    }
	if (startsWithWordByDelimiter(name, '\t', dy->string))
	    {
	    char chromName[bbi->chromBpt->keySize+1];
	    bptStringKeyAtPos(bbi->chromBpt, chromIx, chromName, sizeof(chromName));
	    fprintf(f, "%s\t%u\t%u\t%s\n", chromName, s, e, dy->string);
	    didWrite = TRUE;
	    }
	}

    /* Clean up temporary buffers. */
    dyStringFree(&dy);
    freez(&uncompressedData);
    freez(&rawData);
    }
return didWrite;
}

void bigBedNamedItems(char *bigBedFile, char *name, char *outFile)
/* bigBedNamedItems - Extract item(s) of given name(s) from bigBed. */
{
struct bbiFile *bbi = bigBedFileOpen(bigBedFile);
FILE *f = mustOpen(outFile, "w");
bigBedNameQuery(bbi, name, f);
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
