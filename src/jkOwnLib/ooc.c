/* ooc.c - Stuff to handle overused N-mers (tiles) in genome
 * indexing schemes. */
/* Copyright 2001-2003 Jim Kent.  All rights reserved. */
#include "common.h"
#include "ooc.h"
#include "sig.h"


void oocMaskCounts(char *oocFile, bits32 *tileCounts, int tileSize, bits32 maxPat)
/* Set items of tileCounts to maxPat if they are in oocFile. 
 * Effectively masks this out of index.*/
{
if (oocFile != NULL)
    {
    bits32 sig, psz;
    FILE *f = mustOpen(oocFile, "rb");
    boolean mustSwap = FALSE;

    mustReadOne(f, sig);
    mustReadOne(f, psz);
    if (sig == oocSig)
	mustSwap = FALSE;
    else if (sig == oocSigSwapped)
	{
	mustSwap = TRUE;
	psz = byteSwap32(psz);
	}
    else
        errAbort("Bad signature on %s\n", oocFile);
    if (psz != tileSize)
        errAbort("Oligo size mismatch in %s. Expecting %d got %d\n", 
            oocFile, tileSize, psz);
    if (mustSwap)
	{
	union {bits32 whole; UBYTE bytes[4];} u,v;
	while (readOne(f, u))
	    {
	    v.bytes[0] = u.bytes[3];
	    v.bytes[1] = u.bytes[2];
	    v.bytes[2] = u.bytes[1];
	    v.bytes[3] = u.bytes[0];
	    tileCounts[v.whole] = maxPat;
	    }
	}
    else
	{
	bits32 oli;
	while (readOne(f, oli))
	    tileCounts[oli] = maxPat;
	}
    fclose(f);
    }
}

void oocMaskSimpleRepeats(bits32 *tileCounts, int seedSize, bits32 maxPat)
/* Mask out simple repeats in index . */
{
int i, j, k;
int tileMask = (1<<(seedSize+seedSize))-1;
for (i=0; i<4; ++i)
    {
    for (j=0; j<4; ++j)
        {
        bits32 repeat = 0;
        for (k=0; k<8; ++k)
            {
            repeat <<= 2;
            repeat |= i;
            repeat <<= 2;
            repeat |= j;
            }
        repeat &= tileMask;
        tileCounts[repeat] = maxPat;
        }
    }
}

