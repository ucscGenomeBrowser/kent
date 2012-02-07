/* chromPosLookup - Lookup chromosome and position given a genome coordinate. */
/* This used the b+ tree index and binary file created by chromToPos. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "sig.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "chromPosLookup - Lookup chromosome and position given a genome coordinate.\n"
  "usage:\n"
  "   chromPosLookup chromPos.bin chromPos.ix genomePos\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct bIndexFile
/* B+ tree index file handle. */
    {
    struct bIndexFile *next;	/* Next in list of index files if any. */
    char *fileName;		/* Name of file - for error reporting. */
    FILE *f;			/* Open file pointer. */
    bits32 itemCount;		/* Number of items indexed. */
    bits16 blockSize;		/* Size of block. */
    boolean isSwapped;		/* If TRUE need to byte swap everything. */
    bits32 rootOffset;		/* Offset of root block. */
    };

struct bIndexFile *bIndexFileOpen(char *fileName)
/* Open up index file - reading header and verifying things. */
{
/* Open file and allocate structure to hold info from header etc. */
struct bIndexFile *bif = needMem(sizeof(*bif));
bif->fileName = cloneString(fileName);
FILE *f = mustOpen(fileName, "rb");
bif->f = f;

/* Read magic number at head of file and use it to see if we are proper file type, and
 * see if we are byte-swapped. */
bits32 magic;
mustReadOne(f, magic);
if (magic != chromSizeIndexSig)
    {
    magic = byteSwap32(magic);
    bif->isSwapped = TRUE;
    if (magic != chromSizeIndexSig)
       errAbort("%s is not a chromosome position index file", fileName);
    }

/* Read rest of defined bits of header, byte swapping as needed. */
bif->itemCount = readBits32(f, bif->isSwapped);
bif->blockSize = readBits16(f, bif->isSwapped);

/* Skip over reserved bits of header. */
bits16 reserved16;
bits32 reserved32;
mustReadOne(f, reserved16);
mustReadOne(f, reserved32);

/* Save position of root block of b+ tree. */
bif->rootOffset = ftell(f);

return bif;
}

void bIndexFileClose(struct bIndexFile **pBif)
/* Close down and deallocate index file. */
{
struct bIndexFile *bif = *pBif;
if (bif != NULL)
    {
    carefulClose(&bif->f);
    freeMem(bif->fileName);
    freez(&bif);
    }
}

FILE *openVerifyBin(char *fileName, boolean isSwapped)
/* Open up file, read magic number and verify it.  Return file handle. */
{
FILE *f = mustOpen(fileName, "rb");
bits32 magic = readBits32(f, isSwapped);
if (magic != chromSizeBinSig)
    errAbort("%s is not a chrom position binary file", fileName);
return f;
}

bits32 rFindOffsetBefore(struct bIndexFile *bif, bits32 blockStart, bits32 key)
/* Find file offset of chrom containing key. */
{
/* Seek to start of block. */
fseek(bif->f, blockStart, SEEK_SET);

/* Read block header. */
UBYTE isLeaf;
UBYTE reserved;
bits16 childCount;
mustReadOne(bif->f, isLeaf);
mustReadOne(bif->f, reserved);
childCount = readBits16(bif->f, bif->isSwapped);

/* Scan info for first thing. */
int i;
bits32 genomeOffset = readBits32(bif->f, bif->isSwapped);	// Discarded
bits32 fileOffset = readBits32(bif->f, bif->isSwapped);

/* Loop through remainder. */
for (i=1; i<childCount; ++i)
    {
    genomeOffset = readBits32(bif->f, bif->isSwapped);
    verbose(2, "genomeOffset %d, fileOffset %d\n", genomeOffset, fileOffset);
    if (key < genomeOffset)
	break;
    fileOffset = readBits32(bif->f, bif->isSwapped);
    }

if (isLeaf)
    return fileOffset;
else
    return rFindOffsetBefore(bif, fileOffset, key);
}

void chromPosLookup(char *binFile, char *ixFile, bits32 key)
/* chromPosLookup - Lookup chromosome and position given a genome coordinate.. */
{
struct bIndexFile *bif = bIndexFileOpen(ixFile);
FILE *f = openVerifyBin(binFile, bif->isSwapped);
bits32 fileOffset = rFindOffsetBefore(bif, bif->rootOffset, key);
fseek(f, fileOffset, SEEK_SET);
bits32 genomeOffset = readBits32(f, bif->isSwapped);
char c;
while ((c = getc(f)) != 0)
    putc(c, stdout);
printf(" %u\n", key - genomeOffset);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
chromPosLookup(argv[1], argv[2], sqlUnsigned(argv[3]));
return 0;
}
