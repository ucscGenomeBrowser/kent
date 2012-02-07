/* bwTestRead - Test scan and search side of bigWig.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sig.h"
#include "sqlNum.h"
#include "bPlusTree.h"
#include "cirTree.h"
#include "bwgInternal.h"
#include "bigWig.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bwTestRead - Test scan and search side of bigWig.\n"
  "usage:\n"
  "   bwTestRead file.bw chrom start end\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void bptDumpCallback(void *context, void *key, int keySize, void *val, int valSize)
{
char *keyString = cloneStringZ(key, keySize);
bits32 *pVal = val;
printf("%s:%d:%u\n", keyString, valSize, *pVal);
freeMem(keyString);
}

struct bigWigFileZoomLevel
/* A zoom level in bigWig file. */
    {
    struct bigWigFileZoomLevel *next;	/* Next in list. */
    bits32 reductionLevel;		/* How many bases per item */
    bits32 reserved;			/* Zero for now. */
    bits64 dataOffset;			/* Offset of data for this level in file. */
    bits64 indexOffset;			/* Offset of index for this level in file. */
    };

struct bigWigFile 
/* An open bigWigFile */
    {
    struct bigWigFile *next;	/* Next in list. */
    char *fileName;		/* Name of file - for better error reporting. */
    FILE *f;			/* Open file handle. */
    boolean isSwapped;		/* If TRUE need to byte swap everything. */
    struct bptFile *chromBpt;	/* Index of chromosomes. */
    bits32 zoomLevels;		/* Number of zoom levels. */
    bits64 chromTreeOffset;	/* Offset to chromosome index. */
    bits64 unzoomedDataOffset;	/* Start of unzoomed data. */
    bits64 unzoomedIndexOffset;	/* Start of unzoomed index. */
    struct bigWigFileZoomLevel *levelList;	/* List of zoom levels. */
    };

struct bigWigFile *bigWigFileOpen(char *fileName)
/* Open up big wig file. */
{
struct bigWigFile *bwf;
AllocVar(bwf);
bwf->fileName = cloneString(fileName);
FILE *f = bwf->f = mustOpen(fileName, "rb");

/* Read magic number at head of file and use it to see if we are proper file type, and
 * see if we are byte-swapped. */
bits32 magic;
boolean isSwapped = bwf->isSwapped = FALSE;
mustReadOne(f, magic);
if (magic != bigWigSig)
    {
    magic = byteSwap32(magic);
    isSwapped = TRUE;
    if (magic != bigWigSig)
       errAbort("%s is not a bigWig file", fileName);
    }

/* Read rest of defined bits of header, byte swapping as needed. */
bwf->zoomLevels = readBits32(f, isSwapped);
bwf->chromTreeOffset = readBits64(f, isSwapped);
bwf->unzoomedDataOffset = readBits64(f, isSwapped);
bwf->unzoomedIndexOffset = readBits64(f, isSwapped);

/* Skip over reserved area. */
fseek(f, 32, SEEK_CUR);

/* Read zoom headers. */
int i;
struct bigWigFileZoomLevel *level, *levelList = NULL;
for (i=0; i<bwf->zoomLevels; ++i)
    {
    AllocVar(level);
    level->reductionLevel = readBits32(f, isSwapped);
    level->reserved = readBits32(f, isSwapped);
    level->dataOffset = readBits64(f, isSwapped);
    level->indexOffset = readBits64(f, isSwapped);
    slAddHead(&levelList, level);
    }
slReverse(&levelList);
bwf->levelList = levelList;

/* Attach B+ tree of chromosome names and ids. */
bwf->chromBpt =  bptFileAttach(fileName, f);

return bwf;
}

void bigWigBlockDump(struct bigWigFile *bwf, char *chrom, FILE *out)
/* Print out info on block starting at current position. */
{
boolean isSwapped = bwf->isSwapped;
FILE *f = bwf->f;
bits32 chromId = readBits32(f, isSwapped);
bits32 start = readBits32(f, isSwapped);
bits32 end = readBits32(f, isSwapped);
bits32 itemStep = readBits32(f, isSwapped);
bits32 itemSpan = readBits32(f, isSwapped);
UBYTE type = getc(f);
UBYTE reserved8 = getc(f);
bits16 itemCount = readBits16(f, isSwapped);
bits16 i;
float val;

switch (type)
    {
    case bwgTypeBedGraph:
	{
	fprintf(out, "#bedGraph section %s:%u-%u\n",  chrom, start, end);
	for (i=0; i<itemCount; ++i)
	    {
	    bits32 start = readBits32(f, isSwapped);
	    bits32 end = readBits32(f, isSwapped);
	    mustReadOne(f, val);
	    fprintf(out, "%s\t%u\t%u\t%g\n", chrom, start, end, val);
	    }
	break;
	}
    case bwgTypeVariableStep:
	{
	fprintf(out, "variableStep chrom=%s span=%u\n", chrom, itemSpan);
	for (i=0; i<itemCount; ++i)
	    {
	    bits32 start = readBits32(f, isSwapped);
	    mustReadOne(f, val);
	    fprintf(out, "%u\t%g\n", start+1, val);
	    }
	break;
	}
    case bwgTypeFixedStep:
	{
	fprintf(out, "fixedStep chrom=%s start=%u step=%u span=%u\n", 
		chrom, start, itemStep, itemSpan);
	for (i=0; i<itemCount; ++i)
	    {
	    mustReadOne(f, val);
	    fprintf(out, "%g\n", val);
	    }
	break;
	}
    default:
        internalErr();
	break;
    }
}


struct bigWigView
/* A view (particular zoom level, etc) within a bigWigFile. */
    {
    struct bigWigView *next;	/* Next in list. */
    struct bigWigFile *file;	/* Associated bigWigFile. */
    struct cirTreeFile *index;	/* Index of items in view. */
    };

struct bigWigView *bigWigViewOpen(struct bigWigFile *bwf, float zoomLevel)
/* Open up a view at given zoom level. */
{
struct bigWigView *view;
AllocVar(view);
view->file = bwf;
fseek(bwf->f, bwf->unzoomedIndexOffset, SEEK_SET);
view->index = cirTreeFileAttach(bwf->fileName, bwf->f);
return view;
}

void bigWigViewClose(struct bigWigView **pView)
/* Close down view. */
{
struct bigWigView *view = *pView;
if (view != NULL)
    {
    cirTreeFileDetach(&view->index);
    freez(pView);
    }
}

struct fileOffsetSize *bigWigOverlappingBlocks(struct bigWigView *view, 
	char *chrom, bits32 start, bits32 end)
/* Fetch list of file blocks that contain items overlapping chromosome range. */
{
struct bigWigFile *bwf = view->file;
bits32 chromIx;
if (!bptFileFind(bwf->chromBpt, chrom, strlen(chrom), &chromIx, sizeof(chromIx)))
    return NULL;
return cirTreeFindOverlappingBlocks(view->index, chromIx, start, end);
}

void bwTestRead(char *fileName, char *chrom, int start, int end)
/* bwTestRead - Test scan and search side of bigWig.. */
{
struct bigWigFile *bwf = bigWigFileOpen(fileName);
struct bigWigView *view = bigWigViewOpen(bwf, 1.0);
struct fileOffsetSize *blockList = bigWigOverlappingBlocks(view, chrom, start, end);
struct fileOffsetSize *block;
for (block=blockList; block != NULL; block = block->next)
    {
    fseek(view->file->f, block->offset, SEEK_SET);
    bigWigBlockDump(view->file, chrom, uglyOut);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
bwTestRead(argv[1], argv[2], sqlUnsigned(argv[3]), sqlUnsigned(argv[4]));
return 0;
}
