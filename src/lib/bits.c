/* bits - handle operations on arrays of bits. */
#include "common.h"
#include "bits.h"


static Bits oneBit[8] = { 0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1};
static Bits leftMask[8] = {0xFF, 0x7F, 0x3F, 0x1F,  0xF,  0x7,  0x3,  0x1,};
static Bits rightMask[8] = {0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF,};
static int bitsInByte[256];

static boolean inittedBitsInByte = FALSE;

static void initBitsInByte()
/* Initialize bitsInByte array. */
{
int i;

if (!inittedBitsInByte)
    {
    inittedBitsInByte = TRUE;
    for (i=0; i<256; ++i)
        {
	int count = 0;
	if (i&1)
	    count = 1;
	if (i&2)
	    ++count;
	if (i&4)
	    ++count;
	if (i&8)
	    ++count;
	if (i&0x10)
	    ++count;
	if (i&0x20)
	    ++count;
	if (i&0x40)
	    ++count;
	if (i&0x80)
	    ++count;
	bitsInByte[i] = count;
	}
    }
}

Bits *bitAlloc(int bitCount)
/* Allocate bits. */
{
int byteCount = ((bitCount+7)>>3);
return needLargeZeroedMem(byteCount);
}

void bitFree(Bits **pB)
/* Free bits. */
{
freez(pB);
}

void bitSetOne(Bits *b, int bitIx)
/* Set a single bit. */
{
b[bitIx>>3] |= oneBit[bitIx&7];
}

void bitSetRange(Bits *b, int startIx, int bitCount)
/* Set a range of bits. */
{
int endIx = (startIx + bitCount - 1);
int startByte = (startIx>>3);
int endByte = (endIx>>3);
int startBits = (startIx&7);
int endBits = (endIx&7);
int i;

if (startByte == endByte)
    {
    b[startByte] |= (leftMask[startBits] & rightMask[endBits]);
    return;
    }
b[startByte] |= leftMask[startBits];
for (i = startByte+1; i<endByte; ++i)
    b[i] = 0xff;
b[endByte] |= rightMask[endBits];
}


boolean bitReadOne(Bits *b, int bitIx)
/* Read a single bit. */
{
return (b[bitIx>>3] & oneBit[bitIx&7]) != 0;
}

int bitCountRange(Bits *b, int startIx, int bitCount)
/* Count number of bits set in range. */
{
int endIx = (startIx + bitCount - 1);
int startByte = (startIx>>3);
int endByte = (endIx>>3);
int startBits = (startIx&7);
int endBits = (endIx&7);
int i;
int count = 0;

if (!inittedBitsInByte)
    initBitsInByte();
if (startByte == endByte)
    return bitsInByte[b[startByte] & leftMask[startBits] & rightMask[endBits]];
count = bitsInByte[b[startByte] & leftMask[startBits]];
for (i = startByte+1; i<endByte; ++i)
    count += bitsInByte[b[i]];
count += bitsInByte[b[endByte] & rightMask[endBits]];
return count;
}

void bitClear(Bits *b, int bitCount)
/* Clear many bits. */
{
int byteCount = ((bitCount+7)>>3);
zeroBytes(b, byteCount);
}

void bitAnd(Bits *a, Bits *b, int bitCount)
/* And two bitmaps.  Put result in a. */
{
int byteCount = ((bitCount+7)>>3);
while (--byteCount >= 0)
    *a++ = (*a & *b++);
}

void bitOr(Bits *a, Bits *b, int bitCount)
/* Or two bitmaps.  Put result in a. */
{
int byteCount = ((bitCount+7)>>3);
while (--byteCount >= 0)
    *a++ = (*a | *b++);
}

