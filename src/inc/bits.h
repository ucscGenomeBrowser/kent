/* bits - handle operations on arrays of bits. */

#ifndef BITS_H
#define BITS_H

typedef unsigned char Bits;

Bits *bitAlloc(int bitCount);
/* Allocate bits. */

void bitFree(Bits **pB);
/* Free bits. */

void bitSetOne(Bits *b, int bitIx);
/* Set a single bit. */

void bitSetRange(Bits *b, int startIx, int bitCount);
/* Set a range of bits. */

boolean bitReadOne(Bits *b, int bitIx);
/* Read a single bit. */

int bitCountRange(Bits *b, int startIx, int bitCount);
/* Count number of bits set in range. */

void bitClear(Bits *b, int bitCount);
/* Clear many bits. */

void bitAnd(Bits *a, Bits *b, int bitCount);
/* And two bitmaps.  Put result in a. */

void bitOr(Bits *a, Bits *b, int bitCount);
/* Or two bitmaps.  Put result in a. */

void bitNot(Bits *a, int bitCount);
/* Flip all bits in a. */

#endif /* BITS_H */

