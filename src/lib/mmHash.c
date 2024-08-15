/* Memory mapped hash of strings */

/* This file is copyright 2024 UCSC Genome Browser Authors, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include <sys/mman.h>
#include <unistd.h>
#include "mmHash.h"

unsigned char mmhMagicBytes[] = {0xF0, 'm', 'm', 'h', 'v', '1', 0x0F};
size_t mmhMagicLen = sizeof mmhMagicBytes;

void hashToMmHashFile(struct hash *hash, char *mmapFilePath)
/* Convert hash, whose values are null-terminated strings, to memory-mapped hash format and
 * write to file at mmapFilePath. */
{
if (hash->powerOfTwoSize < MMHASH_MIN_POWER_OF_2_SIZE ||
    hash->powerOfTwoSize > MMHASH_MAX_POWER_OF_2_SIZE)
    errAbort("hashToMmHashFile: power of two size must be between %d and %d but is %d",
             MMHASH_MIN_POWER_OF_2_SIZE, MMHASH_MAX_POWER_OF_2_SIZE, hash->powerOfTwoSize);
verbose(2, "hashToMmHashFile: power of two size %d\n", hash->powerOfTwoSize);
unsigned char powerOfTwoSize = hash->powerOfTwoSize;
// First write the file: magic bytes, byte with powerOfTwoSize, bucket offsets and key/val storage
FILE *f = mustOpen(mmapFilePath, "w");
mustWrite(f, mmhMagicBytes, mmhMagicLen);
mustWrite(f, &powerOfTwoSize, 1);
// Allocate the bucket array in memory and seek past it in the file; we will fill it in with
// offsets as we go, and when done, seek back and write it to file.
size_t bucketCount = (1 << powerOfTwoSize);
size_t bucketArraySize = bucketCount * 8;
uint64_t *bucketOffsets = needLargeZeroedMem(bucketArraySize);
off_t bucketStartOffset = mmhMagicLen + 1;
off_t offset = bucketStartOffset + bucketArraySize;
mustSeek(f, offset, SEEK_SET);
int occupiedBuckets = 0;
int maxElCount = 0;
// For each hash bucket, if it has elements, then write its element count, key(s) and string value(s)
// to file and store its offset in bucketOffsets.
off_t i;
for (i = 0;  i < bucketCount;  i++)
    {
    struct hashEl *helList = hash->table[i];
    if (helList != NULL)
        {
        bucketOffsets[i] = offset;
        int helCount = slCount(helList);
        if (helCount > MMHASH_MAX_EL_COUNT)
            errAbort("hashToMmHashFile: bucket %lu has %d elements but mmHash can have up to %d; "
                     "can't convert this hash to mmHash, sorry.",
                     i, helCount, MMHASH_MAX_EL_COUNT);
        if (helCount > maxElCount)
            maxElCount = helCount;
        if (helCount > 100 && verboseLevel() >= 2)
            {
            verbose(2, "hashToMmHashFile: bucket %lu has %d elements, here are some example keys",
                    i, helCount);
            struct hashEl *hel;
            int n;
            for (hel = helList, n = 0;  hel != NULL && n < 20;  hel = hel->next, n++)
                verbose(2, ",  '%s'", hel->name);
            verbose(2, "\n");
            }
        bits16 elCount = helCount;
        mustWrite(f, &elCount, sizeof elCount);
        offset += sizeof elCount;
        struct hashEl *hel;
        for (hel = helList;  hel != NULL; hel = hel->next)
            {
            int len = strlen(hel->name) + 1;
            mustWrite(f, hel->name, len);
            offset += len;
            len = strlen((char *)hel->val) + 1;
            mustWrite(f, hel->val, len);
            offset += len;
            }
        occupiedBuckets++;
        }
    }
// Now seek back to the start of bucketOffsets and write it.
mustSeek(f, bucketStartOffset, SEEK_SET);
mustWrite(f, bucketOffsets, bucketArraySize);
carefulClose(&f);
verbose(2, "hashToMmHashFile: %d / %lu buckets occupied, max element count is %d\n",
        occupiedBuckets, bucketCount, maxElCount);
}

struct mmHash *mmHashFromFile(char *mmapFilePath)
/* Return an mmHash read in from memory-mapped file at mmapFilePath. */
{
struct mmHash *mmh;
AllocVar(mmh);
mmh->mmapFileName = cloneString(mmapFilePath);
mmh->mmapLength = fileSize(mmapFilePath);
FILE *f = mustOpen(mmapFilePath, "r");
mmh->mmapBytes = mmap(NULL, mmh->mmapLength, PROT_READ, MAP_PRIVATE, fileno(f), 0);
if (mmh->mmapBytes == MAP_FAILED)
    errnoAbort("mmHashFromFile: mmap of file failed: %s", mmapFilePath);
if (madvise(mmh->mmapBytes, mmh->mmapLength, MADV_RANDOM | MADV_WILLNEED) < 0)
    errnoAbort("mmHashFromFile: madvise of file failed: %s", mmapFilePath);
// Check first 7 magic bytes
if (strncmp((char *)mmh->mmapBytes, (char *)mmhMagicBytes, mmhMagicLen))
    errAbort("mmHashFromFile: magic bytes not found at start of file %s", mmapFilePath);
unsigned char powerOfTwoSize = mmh->mmapBytes[mmhMagicLen];
if (powerOfTwoSize < MMHASH_MIN_POWER_OF_2_SIZE || powerOfTwoSize > MMHASH_MAX_POWER_OF_2_SIZE)
    errAbort("mmHashFromFile: power of two size must be between %d and %d but is %d",
             MMHASH_MIN_POWER_OF_2_SIZE, MMHASH_MAX_POWER_OF_2_SIZE, powerOfTwoSize);
mmh->powerOfTwoSize = powerOfTwoSize;
mmh->mask = ((1 << powerOfTwoSize) - 1);
mmh->bucketOffsets = (uint64_t *)(mmh->mmapBytes + mmhMagicLen + 1);
carefulClose(&f);
return mmh;
}

const char *mmHashFindVal(struct mmHash *mmh, char *key)
/* Look up key in mmh and return its string value or NULL if not found.  Do not modify return val. */
{
bits32 keyHash = hashString(key) & mmh->mask;
size_t bucketOffset = mmh->bucketOffsets[keyHash];
// Bucket offsets are always greater than 0 because of the header stuff at the beginning of the file.
// If bucketOffset is 0, that is a special code for 'nothing in the bucket for this hash'.
if (bucketOffset == 0)
    return NULL;
unsigned char *p = mmh->mmapBytes + bucketOffset;
// First two bytes of bucket is element count
bits16 elCount = *((bits16 *)p);
p += sizeof elCount;
int i;
for (i = 0;  i < elCount;  i++)
    {
    char *keyStr = (char *)p;
    p += strlen(keyStr) + 1;
    const char *valStr = (const char *)p;
    p += strlen(valStr) + 1;
    if (!strcmp(key, keyStr))
        return valStr;
    }
return NULL;
}

void mmHashFree(struct mmHash **pMmh)
/* Free the allocated memory for *pMmh and unmap the mapped memory range if not NULL,
 * but leave the memory-mapped file in place for other processes. */
{
if (pMmh != NULL && *pMmh != NULL)
    {
    struct mmHash *mmh = *pMmh;
    if (munmap(mmh->mmapBytes, mmh->mmapLength))
        errnoAbort("mmHashFree: munmap failed");
    freez(&(mmh->mmapFileName));
    freez(pMmh);
    }
}
