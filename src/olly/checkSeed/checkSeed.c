/* checkSeed - Check seeds are good. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bits.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkSeed - Check seeds are good\n"
  "usage:\n"
  "   checkSeed seed oligoSize maxMismatch\n"
  "options:\n"
  "   -find=bits\n"
  );
}

int seedSize;
int ollySize;
bool *seed;
bool *mask;

boolean coexist(bool *olly, bool *seed, int size)
/* Return TRUE if can fit 1's in olly into 0's in seed. */
{
int i;
for (i=0; i<size; ++i)
    {
    if (olly[i] && seed[i])
	return FALSE;
    }
return TRUE;
}

boolean checkOlly(boolean report)
/* Make sure that mask and seed coexist at some position. */
{
int diffSize = ollySize - seedSize;
int i;
for (i=0; i<=diffSize; ++i)
    {
    if (coexist(mask+i, seed, seedSize))
	return TRUE;
    }
if (report)
    {
    printf("No possible coexistence\n");
    printf("seed: ");
    for (i=0; i<seedSize; ++i)
	printf("%c", (seed[i] ? '1' : '0'));
    printf("\n");
    printf("mask: ");
    for (i=0; i<ollySize; ++i)
	printf("%c", (mask[i] ? '1' : '0'));
    printf("\n");
    }
return FALSE;
}

boolean rCheck(int maxMiss, boolean report)
/* Add all varients of olly that differ from olly by at most maxMiss.  Don't bother
 * varying at mask position. */
{
if (!checkOlly(report))
    return FALSE;
if (maxMiss > 0)
    {
    int i,j;
    for (i=0; i<ollySize; ++i)
	{
	if (!mask[i])
	    {
	    mask[i] = TRUE;
	    if (!rCheck(maxMiss-1, report))
		return FALSE;
	    mask[i] = FALSE;
	    }
	}
    }
return TRUE;
}

void findSeed(int bitTarget, int maxMismatch)
/* Loop through all seeds of size seedSize with bitTarget bits
 * that are non-zero, trying to find a good one. */
{
int maxI = (1<<seedSize);
union bic {int i;unsigned char b[4];} bic;
int i, bitCount;
bitsInByteInit();
for (i=0; i<maxI; ++i)
    {
    bic.i = i;
    bitCount = bitsInByte[bic.b[0]] + bitsInByte[bic.b[1]] +
	bitsInByte[bic.b[2]] + bitsInByte[bic.b[3]];
    if (bitCount == bitTarget)
	{
	int j;
	for (j=seedSize-1; j>=0; --j)
	    {
	    int bit = (1<<j);
	    seed[j] = (bit & i ? TRUE : FALSE);
	    }
	memset(mask, 0, ollySize);
	if (rCheck(maxMismatch, FALSE))
	    {
	    for (j=0; j<seedSize; ++j)
		printf("%d", seed[j]);
	    printf("\n");
	    }
	}
    }
}

void checkSeed(char *asciiSeed, int oligoSize, int maxMismatch)
/* checkSeed - Check seeds are good. */
{
int i;

/* Convert seed from ascii 1/0 to boolean 1/0 */
seedSize = strlen(asciiSeed);
AllocArray(seed, seedSize);
for (i=0; i<seedSize; ++i)
    {
    if (asciiSeed[i] != '0')
	 seed[i] = TRUE;
    }

if (seedSize > oligoSize)
    errAbort("Seed is bigger than oligo, sorry");

/* Allocate mask for oligo generator. */
ollySize = oligoSize;
AllocArray(mask, oligoSize);
if (optionExists("find"))
   findSeed(optionInt("find", 0), maxMismatch);
else
    rCheck(maxMismatch, TRUE);
freez(&mask);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
checkSeed(argv[1], atoi(argv[2]), atoi(argv[3]));
return 0;
}
