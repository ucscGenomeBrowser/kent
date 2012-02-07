/* nt4 - stuff to handle 2-bit-a-base representation of DNA.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "dnautil.h"
#include "shaRes.h"
#include "dnaseq.h"
#include "nt4.h"
#include "sig.h"


static size_t bits32PaddedSize(size_t size)
{
    return (((size+15)>>4)<<2);
}

static struct nt4Seq *allocNt4(size_t baseCount, char *name)
/* Create a new nt4Seq struct with memory for bases. */
{
size_t memSize = bits32PaddedSize(baseCount);
struct nt4Seq *seq = needMem(sizeof(*seq));
seq->baseCount = baseCount;
seq->bases = needLargeMem(memSize);
seq->name = cloneString(name);
return seq;
}


struct nt4Seq *newNt4(DNA *dna, int size, char *name)
/* Create a new DNA seq with 2 bits per base pair. */
{
bits32 *packed;
DNA *unpacked;
char last[16];
struct nt4Seq *seq = allocNt4(size, name);
packed = seq->bases;
unpacked = dna;
while (size > 16)
    {
    *packed++ = packDna16(dna);
    dna += 16;
    size -= 16;
    }
if (size > 0)
    {
    memcpy(last, dna, size);
    *packed++ = packDna16(dna);
    }
return seq;
}

void freeNt4(struct nt4Seq **pSeq)
/* Free up DNA seq with 2 bits per base pair */
{
struct nt4Seq *seq = *pSeq;
if (seq == NULL)
    return;
freeMem(seq->bases);
freeMem(seq->name);
freez(pSeq);
}

static FILE *nt4OpenVerify(char *fileName)
/* Open up an nt4 file and verify signature.
 * Abort if any problem. */
{
FILE *f = mustOpen(fileName, "rb");
bits32 signature;
mustReadOne(f, signature);
if (signature != nt4Signature)
    errAbort("%s is not a good Nt4 file", fileName);
return f;
}

struct nt4Seq *loadNt4(char *fileName, char *seqName)
/* Load up an nt4 sequence from a file. */
{
bits32 size;
struct nt4Seq *seq;
FILE *f = nt4OpenVerify(fileName);

mustReadOne(f, size);
if (seqName == NULL)
    seqName = fileName;
seq = allocNt4(size, seqName);
mustRead(f, seq->bases, bits32PaddedSize(size));
carefulClose(&f);
return seq;
}

void  saveNt4(char *fileName, DNA *dna, bits32 dnaSize)
/* Save dna in an NT4 file. */
{
FILE *f = mustOpen(fileName, "wb");
bits32 signature = nt4Signature;
bits32 bases;
char last[16];

writeOne(f, signature);
writeOne(f, dnaSize);
while (dnaSize >= 16)
    {
    bases = packDna16(dna);
    writeOne(f, bases);
    dna += 16;
    dnaSize -= 16;
    }
if (dnaSize > 0)
    {
    zeroBytes(last, sizeof(last));
    memcpy(last, dna, dnaSize);
    bases = packDna16(last);
    writeOne(f, bases);
    }
fclose(f);
}

int nt4BaseCount(char *fileName)
/* Return number of bases in NT4 file. */
{
bits32 size;
FILE *f = nt4OpenVerify(fileName);

mustReadOne(f, size);
fclose(f);
return (int)size;
}

static void unpackRightSide(bits32 tile, int baseCount, DNA *out)
/* Unpack last part of tile into DNA. */
{
int j;

for (j=baseCount; --j>=0; )
    {
    out[j] = valToNt[tile & 0x3];
    tile >>= 2;
    }
}

static void unpackLeftSide(bits32 tile, int baseCount, DNA *out)
/* Unpack first part of tile into DNA. */
{
int bitsToDump = ((16-baseCount)<<1);
tile >>= bitsToDump;
out += baseCount;
while (--baseCount >= 0)
    {
    *--out = valToNt[tile&0x3];
    tile >>= 2;
    }
}

static void unpackMidWord(bits32 tile, int startBase, int baseCount, DNA *out)
/* Unpack part of a single word. */
{
tile >>= ((16-startBase-baseCount)<<1);
out += baseCount;
while (--baseCount >= 0)
    {
    *--out = valToNt[tile&0x3];
    tile >>= 2;
    }
}

void unalignedUnpackDna(bits32 *tiles, int start, int size, DNA *unpacked)
/* Unpack into out, even though not starting/stopping on tile 
 * boundaries. */
{
DNA *pt = unpacked;
int sizeLeft = size;
int firstBases, middleBases;
bits32 *startTile, *endTile;

dnaUtilOpen();

/* See if all data is in a single word. */
startTile = tiles + (start>>4);
endTile = tiles + ((start + size - 1)>>4);
if (startTile == endTile)
    {
    unpackMidWord(*startTile, start&0xf, size, unpacked);
    return;
    }

/* Skip over initial stuff. */
tiles = startTile;

/* See if just have one tile to 
 * Unpack the right hand side of the first tile. */
firstBases = (16 - (start&0xf));
unpackRightSide(*tiles, firstBases, pt);
pt += firstBases;
sizeLeft -= firstBases;
tiles += 1;

/* Unpack all of the middle tiles. */
middleBases = (sizeLeft&0x7ffffff0);
unpackDna(tiles, middleBases>>4, pt);
pt += middleBases;
sizeLeft -= middleBases;
tiles += (middleBases>>4);

/* Unpack the left side of last tile. */
if (sizeLeft > 0)
    unpackLeftSide(*tiles, sizeLeft, pt);
pt += sizeLeft;

/* Add trailing zero to make it a DNA string. */
assert(pt == unpacked+size);
*pt = 0;
}

DNA *nt4Unpack(struct nt4Seq *n, int start, int size)
/* Create an unpacked section of nt4 sequence.  */
{
DNA *unpacked = needLargeMem(size+1);
unalignedUnpackDna(n->bases, start, size, unpacked);
return unpacked;
}


DNA *nt4LoadPart(char *fileName, int start, int size)
/* Load part of an nt4 file. */
{
bits32 basesInFile;
int tStart, tEnd, tSize;
int end;
FILE *f;
DNA *unpacked;
bits32 *tiles;

/* Open file, and make sure request is covered by file. */
f = nt4OpenVerify(fileName);
mustReadOne(f, basesInFile);

assert(start >= 0);
end = start + size;
assert(end <= (int)basesInFile);

/* Figure out tiles to load */
tStart = (start>>4);
tEnd = ((end+15)>>4);
tSize = tEnd - tStart;

/* Allocate tile array and read it from disk. */
tiles = needLargeMem(tSize * sizeof(*tiles));
fseek(f, tStart * sizeof(*tiles), SEEK_CUR);
mustRead(f, tiles, tSize * sizeof(*tiles) );

/* Allocate and unpack array. */
unpacked = needLargeMem(size+1);
unalignedUnpackDna(tiles, start - (tStart<<4), size, unpacked);

freeMem(tiles);
fclose(f);
return unpacked;
}
