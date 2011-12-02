/* spacedSeed - stuff to help with spaced seeds for alignments. */

#include "common.h"
#include "spacedSeed.h"


/* Seeds - the weight 9 and 11 seeds are from PatternHunter paper.
 * The weights 10,12,13,14,15,16,17 and 18 are from the Choi, Zeng,
 * and Zhang paper.  The others are just guesses. */

char *spacedSeeds[] = {
    /*  0 */ "",
    /*  1 */ "1",
    /*  2 */ "11",
    /*  3 */ "1101",
    /*  4 */ "110101",
    /*  5 */ "1101011",
    /*  6 */ "111001011",
    /*  7 */ "1110010111",
    /*  8 */ "1110010100111",
    /*  9 */ "111001010011011",
    /* 10 */ "1101100011010111",
    /* 11 */ "111010010100110111",
    /* 12 */ "111010110100110111",
    /* 13 */ "11101011001100101111",
    /* 14 */ "111011100101100101111",
    /* 15 */ "11110010101011001101111",
#ifdef EVER_NEEDED_IN_64_BIT_MACHINE
    /* 16 */ "111100110101011001101111",
    /* 17 */ "111101010111001101101111",
    /* 18 */ "1111011001110101011011111",
#endif /* EVER_NEEDED_IN_64_BIT_MACHINE */
};

int spacedSeedMaxWeight()
/* Return max weight of spaced seed. */
{
return ArraySize(spacedSeeds)-1;
}

int *spacedSeedOffsets(int weight)
/* Return array with offsets for seed of given weight. */
{
char *seed;
int *output, offset, outCount = 0, seedSize;

assert(weight >= 1 && weight < ArraySize(spacedSeeds));
seed = spacedSeeds[weight];
seedSize = strlen(seed);
AllocArray(output, weight);
for (offset=0; offset<seedSize; ++offset)
    {
    if (seed[offset] == '1')
	output[outCount++] = offset;
    }
assert(outCount == weight);
return output;
}

int spacedSeedSpan(int weight)
/* Return span of seed of given weight */
{
return strlen(spacedSeeds[weight]);
}

