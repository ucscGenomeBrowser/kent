/* spacedSeed - stuff to help with spaced seeds for alignments. */

#ifndef SPACEDSEED_H
#define SPACEDSEED_H

extern char *spacedSeeds[];
/* Array of spaced seeds in format with '1' for cares, '0' for don't care. */

int spacedSeedMaxWeight();
/* Return max weight of spaced seed. */

int *spacedSeedOffsets(int weight);
/* Return array with offsets for seed of given weight. */

int spacedSeedSpan(int weight);
/* Return span of seed of given weight */

#endif /*SPACEDSEED_H */

