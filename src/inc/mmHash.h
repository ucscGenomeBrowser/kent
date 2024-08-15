/* Memory mapped hash: both keys and values are 0-terminated strings */

/* This file is copyright 2024 UCSC Genome Browser Authors, but license is hereby
 * granted for all use - public, private or commercial. */

// Typical use:
//
// * build a regular hash, convert to mmHash format and write to file
//    struct hash *hash = hashFrom...(...);
//    hashToMmHashFile(hash, pathToFile);
//
// * read mmHash from memory mapped file path, look up values
//    struct mmHash *mmh = mmHashFromFile(pathToFile);
//    char *val = mmHashFindVal(mmh, itemName);
//    ...
//    mmHashFree(&mmh);

// Memory mapped file layout:
//
// * magic bytes    [7 bytes]  0xF0 m m h v 1 0x0F
// * powerOfTwoSize [1 byte]
// * bucketOffsets  [8 bytes * 2^powerOfTwoSize]
// * variable-length key and value storage, into which bucketOffsets point.  Series of bucket elems:
//   * elCount [2 bytes]
//   * repeating elCount times:
//     * keyString [0-terminated string]
//     * valString [0-terminated string]
// Values of bucketOffsets are either 0 (no items in bucket) or offsets into the memory mapped file.
// They are 8 bytes because 4 bytes would limit the total size of the file (buckets, keys, values,
// overhead) to 4GB which is a limit we could conceivably run up against with lots of large values.
// 8 bytes seems wasteful but at least it's aligned.

#ifndef MMHASH_H
#define MMHASH_H

#include "hash.h"
#include <stdint.h>

// The min is somewhat arbitrary, just meant to make sure caller isn't passing in garbage.
#define MMHASH_MIN_POWER_OF_2_SIZE 3
#define MMHASH_MAX_POWER_OF_2_SIZE hashMaxSize

// elCount is two bytes so this is a hard limit.  Hopefully hash bucket element counts will always
// be much less than this.
#define MMHASH_MAX_EL_COUNT ((1 << 16) - 1)

struct mmHash
{
    uint32_t powerOfTwoSize;    // power of two size between MMHASH_{MIN,MAX}_POWER_OF_2_SIZE
    uint32_t mask;              // for convenience, mask on hashed key ((1 << powerOfTwoSize) - 1)
    char *mmapFileName;         // path to memory-mapped file
    unsigned char *mmapBytes;   // pointer to start of memory-mapped file
    size_t mmapLength;          // number of memory-mapped bytes
    uint64_t *bucketOffsets;    // for convenience, pointer to start of bucket array in mmapBytes
};

void hashToMmHashFile(struct hash *hash, char *mmapFilePath);
/* Convert hash, whose values are null-terminated strings, to memory-mapped hash format and
 * write to file at mmapFilePath. */

struct mmHash *mmHashFromFile(char *mmapFilePath);
/* Return an mmHash read in from memory-mapped file at mmapFilePath. */

const char *mmHashFindVal(struct mmHash *mmh, char *key);
/* Look up key in mmh and return its string value or NULL if not found.  Do not modify return val. */

void mmHashFree(struct mmHash **pMmh);
/* Free the allocated memory for *pMmh and unmap the mapped memory range if not NULL,
 * but leave the memory-mapped file in place for other processes. */

#endif // MMHASH_H
