/* bits - handle operations on arrays of bits. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef BITS_H
#define BITS_H

typedef unsigned char Bits;

Bits *bitAlloc(int bitCount);
/* Allocate bits. */

Bits *bitClone(Bits* orig, int bitCount);
/* Clone bits. */

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

