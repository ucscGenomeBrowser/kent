/* bits - handle operations on arrays of bits. */

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

