/* bits - handle operations on arrays of bits. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "bits.h"



static Bits oneBit[8] = { 0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1};
static Bits leftMask[8] = {0xFF, 0x7F, 0x3F, 0x1F,  0xF,  0x7,  0x3,  0x1,};
static Bits rightMask[8] = {0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF,};
int bitsInByte[256];

static boolean inittedBitsInByte = FALSE;

void bitsInByteInit()
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

Bits *bitAlloc(bits64 bitCount)
/* Allocate bits. */
{
bits64 byteCount = ((bitCount+7)>>3);
return needLargeZeroedMem(byteCount);
}

Bits *bitRealloc(Bits *b, bits64 bitCount, bits64 newBitCount)
/* Resize a bit array.  If b is null, allocate a new array */
{
bits64 byteCount = ((bitCount+7)>>3);
bits64 newByteCount = ((newBitCount+7)>>3);
return needLargeZeroedMemResize(b, byteCount, newByteCount);
}

Bits *bitClone(Bits* orig, bits64 bitCount)
/* Clone bits. */
{
bits64 byteCount = ((bitCount+7)>>3);
Bits* bits = needLargeZeroedMem(byteCount);
memcpy(bits, orig, byteCount);
return bits;
}

void bitFree(Bits **pB)
/* Free bits. */
{
freez(pB);
}

Bits *lmBitAlloc(struct lm *lm, bits64 bitCount)
// Allocate bits.  Must supply local memory.
{
assert(lm != NULL);
bits64 byteCount = ((bitCount+7)>>3);
return lmAlloc(lm,byteCount);
}

Bits *lmBitRealloc(struct lm *lm,Bits *b, bits64 bitCount, bits64 newBitCount)
// Resize a bit array.  If b is null, allocate a new array.  Must supply local memory.
{
assert(lm != NULL);
bits64 byteCount = ((bitCount+7)>>3);
bits64 newByteCount = ((newBitCount+7)>>3);
return lmAllocMoreMem(lm, b ,byteCount, newByteCount);
}

Bits *lmBitClone(struct lm *lm,Bits* orig, bits64 bitCount)
// Clone bits.  Must supply local memory.
{
assert(lm != NULL);
bits64 byteCount = ((bitCount+7)>>3);
Bits* bits = lmAlloc(lm,byteCount);
memcpy(bits, orig, byteCount);
return bits;
}

void bitSetOne(Bits *b, bits64 bitIx)
/* Set a single bit. */
{
b[bitIx>>3] |= oneBit[bitIx&7];
}

void bitClearOne(Bits *b, bits64 bitIx)
/* Clear a single bit. */
{
b[bitIx>>3] &= ~oneBit[bitIx&7];
}

void bitSetRange(Bits *b, bits64 startIx, bits64 bitCount)
/* Set a range of bits. */
{
if (bitCount <= 0)
    return;
bits64 endIx = (startIx + bitCount - 1);
bits64 startByte = (startIx>>3);
bits64 endByte = (endIx>>3);
bits64 startBits = (startIx&7);
bits64 endBits = (endIx&7);
bits64 i;

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


boolean bitReadOne(Bits *b, bits64 bitIx)
/* Read a single bit. */
{
return (b[bitIx>>3] & oneBit[bitIx&7]) != 0;
}

bits64 bitCountRange(Bits *b, bits64 startIx, bits64 bitCount)
/* Count number of bits set in range. */
{
if (bitCount <= 0)
    return 0;
bits64 endIx = (startIx + bitCount - 1);
bits64 startByte = (startIx>>3);
bits64 endByte = (endIx>>3);
bits64 startBits = (startIx&7);
bits64 endBits = (endIx&7);
bits64 i;
bits64 count = 0;

if (!inittedBitsInByte)
    bitsInByteInit();
if (startByte == endByte)
    return bitsInByte[b[startByte] & leftMask[startBits] & rightMask[endBits]];
count = bitsInByte[b[startByte] & leftMask[startBits]];
for (i = startByte+1; i<endByte; ++i)
    count += bitsInByte[b[i]];
count += bitsInByte[b[endByte] & rightMask[endBits]];
return count;
}

bits64 bitFind(Bits *b, bits64 startIx, boolean val, bits64 bitCount)
/* Find the index of the the next set bit. */
{
unsigned char notByteVal = val ? 0 : 0xff;
bits64 iBit = startIx;
bits64 endByte = ((bitCount-1)>>3);
bits64 iByte;

/* scan initial byte */
while (((iBit & 7) != 0) && (iBit < bitCount))
    {
    if (bitReadOne(b, iBit) == val)
        return iBit;
    iBit++;
    }

/* scan byte at a time, if not already in last byte */
iByte = (iBit >> 3);
if (iByte < endByte)
    {
    while ((iByte < endByte) && (b[iByte] == notByteVal))
        iByte++;
    iBit = iByte << 3;
    }

/* scan last byte */
while (iBit < bitCount)
    {
    if (bitReadOne(b, iBit) == val)
        return iBit;
    iBit++;
    }
 return bitCount;  /* not found */
}

bits64 bitFindSet(Bits *b, bits64 startIx, bits64 bitCount)
/* Find the index of the the next set bit. */
{
return bitFind(b, startIx, TRUE, bitCount);
}

bits64 bitFindClear(Bits *b, bits64 startIx, bits64 bitCount)
/* Find the index of the the next clear bit. */
{
return bitFind(b, startIx, FALSE, bitCount);
}

void bitClear(Bits *b, bits64 bitCount)
/* Clear many bits (possibly up to 7 beyond bitCount). */
{
bits64 byteCount = ((bitCount+7)>>3);
zeroBytes(b, byteCount);
}

void bitClearRange(Bits *b, bits64 startIx, bits64 bitCount)
/* Clear a range of bits. */
{
if (bitCount <= 0)
    return;
bits64 endIx = (startIx + bitCount - 1);
bits64 startByte = (startIx>>3);
bits64 endByte = (endIx>>3);
bits64 startBits = (startIx&7);
bits64 endBits = (endIx&7);
bits64 i;

if (startByte == endByte)
    {
    b[startByte] &= ~(leftMask[startBits] & rightMask[endBits]);
    return;
    }
b[startByte] &= ~leftMask[startBits];
for (i = startByte+1; i<endByte; ++i)
    b[i] = 0x00;
b[endByte] &= ~rightMask[endBits];
}

void bitAnd(Bits *a, Bits *b, bits64 bitCount)
/* And two bitmaps.  Put result in a. */
{
bits64 byteCount = ((bitCount+7)>>3);
while (--byteCount >= 0)
    {
    *a = (*a & *b++);
    a++;
    }
}

bits64 bitAndCount(Bits *a, Bits *b, bits64 bitCount)
// Without altering 2 bitmaps, count the AND bits.
{
bits64 byteCount = ((bitCount+7)>>3);
bits64 count = 0;
if (!inittedBitsInByte)
    bitsInByteInit();
while (--byteCount >= 0)
    count += bitsInByte[(*a++ & *b++)];

return count;
}

void bitOr(Bits *a, Bits *b, bits64 bitCount)
/* Or two bitmaps.  Put result in a. */
{
bits64 byteCount = ((bitCount+7)>>3);
while (--byteCount >= 0)
    {
    *a = (*a | *b++);
    a++;
    }
}

bits64 bitOrCount(Bits *a, Bits *b, bits64 bitCount)
// Without altering 2 bitmaps, count the OR'd bits.
{
bits64 byteCount = ((bitCount+7)>>3);
bits64 count = 0;
if (!inittedBitsInByte)
    bitsInByteInit();
while (--byteCount >= 0)
    count += bitsInByte[(*a++ | *b++)];

return count;
}

void bitXor(Bits *a, Bits *b, bits64 bitCount)
{
bits64 byteCount = ((bitCount+7)>>3);
while (--byteCount >= 0)
    {
    *a = (*a ^ *b++);
    a++;
    }
}

bits64 bitXorCount(Bits *a, Bits *b, bits64 bitCount)
// Without altering 2 bitmaps, count the XOR'd bits.
{
bits64 byteCount = ((bitCount+7)>>3);
bits64 count = 0;
if (!inittedBitsInByte)
    bitsInByteInit();
while (--byteCount >= 0)
    count += bitsInByte[(*a++ ^ *b++)];

return count;
}

void bitNot(Bits *a, bits64 bitCount)
/* Flip all bits in a. */
{
bits64 byteCount = ((bitCount+7)>>3);
while (--byteCount >= 0)
    {
    *a = ~*a;
    a++;
    }
}

void bitReverseRange(Bits *bits, bits64 startIx, bits64 bitCount)
// Reverses bits in range (e.g. 110010 becomes 010011)
{
if (bitCount <= 0)
    return;
bits64 ixA = startIx;
bits64 ixB = (startIx + bitCount - 1);
for ( ;ixA < ixB; ixA++, ixB--)
    {
    boolean bitA = bitReadOne(bits, ixA);
    boolean bitB = bitReadOne(bits, ixB);
    if (!bitA && bitB)
        {
        bitSetOne(  bits, ixA);
        bitClearOne(bits, ixB);
        }
    else if (bitA && !bitB)
        {
        bitClearOne(bits, ixA);
        bitSetOne(  bits, ixB);
        }
    }
}


void bitPrint(Bits *a, bits64 startIx, bits64 bitCount, FILE* out)
/* Print part or all of bit map as a string of 0s and 1s.  Mostly useful for
 * debugging */
{
bits64 i;
for (i = startIx; i < bitCount; i++)
    {
    if (bitReadOne(a, i))
        fputc('1', out);
    else
        fputc('0', out);
    }
fputc('\n', out);
}

void bitsOut(FILE* out, Bits *bits, bits64 startIx, bits64 bitCount, boolean onlyOnes)
// Print part or all of bit map as a string of 0s and 1s.
// If onlyOnes, enclose result in [] and use ' ' instead of '0'.
{
if (onlyOnes)
    fputc('[', out);

bits64 ix = startIx;
for ( ; ix < bitCount; ix++)
    {
    if (bitReadOne(bits, ix))
        fputc('1', out);
    else
        {
        if (onlyOnes)
            fputc(' ', out);
        else
            fputc('0', out);
        }
    }
if (onlyOnes)
    fputc(']', out);
}

Bits *bitsIn(struct lm *lm,char *bitString, bits64 len)
// Returns a bitmap from a string of 1s and 0s.  Any non-zero, non-blank char sets a bit.
// Returned bitmap is the size of len even if that is longer than the string.
// Optionally supply local memory.  Note does NOT handle enclosing []s printed with bitsOut().
{
if (bitString == NULL || len == 0)
    return NULL;

Bits *bits = NULL;
if (lm != NULL)
    bits = lmBitAlloc(lm,len);
else
    bits = bitAlloc(len);

bits64 ix = 0;
for ( ;ix < len && bitString[ix] != '\0'; ix++)
    {
    if (bitString[ix] != '0' && bitString[ix] != ' ')
        bitSetOne(bits, ix);
    }
return bits;
}

