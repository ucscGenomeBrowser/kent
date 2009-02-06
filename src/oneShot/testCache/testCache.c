/* testCache - Experiments with file cacher.. */

/* udc - url data cache - a caching system that keeps blocks of data fetched from URLs in
 * sparse local files for quick use the next time the data is needed. 
 *
 * This cache is enormously simplified by there being no local _write_ to the cache,
 * just reads.  
 *
 * The overall strategy of the implementation is to have a root cache directory
 * with a subdir for each file being cached.  The directory for a single cached file
 * contains two files - "bitmap" and "sparseData" that contains information on which
 * parts of the URL are cached and the actual cached data respectively. The subdirectory name
 * associated with the file is constructed from the URL in a straightforward manner.
 *     http://genome.ucsc.edu/cgi-bin/hgGateway
 * gets mapped to:
 *     rootCacheDir/http/genome.ucsc.edu/cgi-bin/hgGateway/
 * The URL protocol is the first directory under the root, and the remainder of the
 * URL, with some necessary escaping, is used to define the rest of the cache directory
 * structure, with each '/' after the protocol line translating into another directory
 * level.
 *    
 * The bitmap file contains time stamp and size data as well as an array with one bit
 * for each block of the file that has been fetched.  Currently the block size is 8K. */
#include <sys/types.h>
#include <sys/stat.h>

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "sqlNum.h"
#include "obscure.h"
#include "bits.h"
#include "portable.h"
#include "sig.h"


static char const rcsid[] = "$Id: testCache.c,v 1.6 2009/02/06 18:51:47 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testCache - Experiments with file cacher.\n"
  "usage:\n"
  "   testCache sourceUrl offset size outFile\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct udcRemoteFileInfo
/* Information about a remote file. */
    {
    bits64 updateTime;	/* Last update in seconds since 1970 */
    bits64 size;	/* Remote file size */
    };

typedef int (*UdcDataCallback)(char *url, bits64 offset, int size, void *buffer);
typedef boolean (*UdcInfoCallback)(char *url, struct udcRemoteFileInfo *retInfo);

#define udcBlockSize (8*1024)
/* All fetch requests are rounded up to block size. */

#define udcBitmapHeaderSize (64)

struct udcProtocol
/* Something to handle a communications protocol like http, https, ftp, local file i/o, etc. */
    {
    struct udcProtocol *next;	/* Next in list */
    UdcDataCallback fetchData;	/* Data fetcher */
    UdcInfoCallback fetchInfo;	/* Timestamp & size fetcher */
    };

struct udcFile
/* A file handle for our caching system. */
    {
    struct udcFile *next;	/* Next in list. */
    char *url;			/* Name of file - includes protocol */
    char *protocol;		/* The URL up to the first colon.  http: etc. */
    struct udcProtocol *prot;	/* Protocol specific data and methods. */
    time_t updateTime;		/* Last modified timestamp. */
    bits64 size;		/* Size of file. */
    bits64 offset;		/* Current offset in file. */
    char *cacheDir;		/* Directory for cached file parts. */
    char *bitmapFileName;	/* Name of bitmap file. */
    char *sparseFileName;	/* Name of sparse data file. */
    FILE *fSparse;		/* File handle for sparse data file. */
    bits64 startData;		/* Start of area in file we know to have data. */
    bits64 endData;		/* End of area in file we know to have data. */
    };

struct udcBitmap
/* The control structure including the bitmap of blocks that are cached. */
    {
    struct udcBitmap *next;	/* Next in list. */
    bits32 blockSize;		/* Number of bytes per block of file. */
    bits64 remoteUpdate;	/* Remote last update time. */
    bits64 fileSize;		/* File size */
    bits64 localUpdate;		/* Time we last fetched new data into cache. */
    bits64 localAccess;		/* Time we last accessed data. */
    boolean isSwapped;		/* If true need to swap all bytes on read. */
    FILE *f;			/* File handle for file with current block. */
    };
static char *udcRootDir = "/Users/kent/src/oneShot/testCache/cache";
static char *bitmapName = "bitmap";
static char *sparseDataName = "sparseData";

char *fileNameInCacheDir(struct udcFile *file, char *fileName)
/* Return the name of a file in the cache dir, from the cache root directory on down.
 * Do a freeMem on this when done. */
{
int dirLen = strlen(file->cacheDir);
int nameLen = strlen(fileName);
char *path = needMem(dirLen + nameLen + 2);
memcpy(path, file->cacheDir, dirLen);
path[dirLen] = '/';
memcpy(path+dirLen+1, fileName, dirLen);
return path;
}

void udcNewCreateBitmapAndSparse(struct udcFile *file, bits64 remoteUpdate, bits64 remoteSize)
/* Create a new bitmap file around the given remoteUpdate time. */
{
uglyf("udcNewCreateBitmapAndSparse(%s %llu %llu)\n", file->url, remoteUpdate, remoteSize);
FILE *f = mustOpen(file->bitmapFileName, "wb");
bits32 sig = udcBitmapSig;
bits32 blockSize = udcBlockSize;
bits64 reserved64 = 0;
int blockCount = (remoteSize + udcBlockSize - 1)/udcBlockSize;
int bitmapSize = bitToByteSize(blockCount);
int i;

// #define bitToByteSize(bitSize) ((bitSize+7)/8)
// /* Convert number of bits to number of bytes needed to store bits. */

/* Write out fixed part of header. */
writeOne(f, sig);
writeOne(f, blockSize);
writeOne(f, remoteUpdate);
writeOne(f, remoteSize);
writeOne(f, reserved64);
writeOne(f, reserved64);
writeOne(f, reserved64);
writeOne(f, reserved64);
writeOne(f, reserved64);
assert(ftell(f) == udcBitmapHeaderSize);

/* Write out initial all-zero bitmap. */
for (i=0; i<bitmapSize; ++i)
    putc(0, f);

/* Clean up bitmap file and name. */
carefulClose(&f);

/* Create an empty data file. */
f = mustOpen(file->sparseFileName, "wb");
carefulClose(&f);
}

struct udcBitmap *udcBitmapOpen(char *fileName)
/* Open up a bitmap file and read and verify header.  Return NULL if file doesn't
 * exist, abort on error. */
{
uglyf("udcBitmapOpen(%s)\n", fileName);
/* Open file, returning NULL if can't. */
FILE *f = fopen(fileName, "r+b");
if (f == NULL)
    return NULL;

/* Get status info from file. */
struct stat status;
int err = fstat(fileno(f), &status);

/* Read signature and decide if byte-swapping is needed. */
bits32 magic;
boolean isSwapped = FALSE;
mustReadOne(f, magic);
if (magic != udcBitmapSig)
    {
    magic = byteSwap32(magic);
    isSwapped = TRUE;
    if (magic != udcBitmapSig)
       errAbort("%s is not a udcBitmap file", fileName);
    }

/* Allocate bitmap object, fill it in, and return it. */
struct udcBitmap *bits;
AllocVar(bits);
bits->blockSize = readBits32(f, isSwapped);
bits->remoteUpdate = readBits64(f, isSwapped);
bits->fileSize = readBits64(f, isSwapped);
bits->localUpdate = status.st_mtime;
bits->localAccess = status.st_atime;
bits->isSwapped = isSwapped;
bits->f = f;

return bits;
}

void killStaleStuff(struct udcFile *file)
/* Kill sparseData file and reset bitmap if time stamp indicates things are stale. */
{
}

void udcBitmapClose(struct udcBitmap **pBits)
/* Free up resources associated with udcBitmap. */
{
struct udcBitmap *bits = *pBits;
if (bits != NULL)
    {
    carefulClose(&bits->f);
    freez(pBits);
    }
}

int udcDataViaLocal(char *url, bits64 offset, int size, void *buffer)
/* Fetch a block of data of given size into buffer using the http: protocol.
* Returns number of bytes actually read.  Does an errAbort on
* error.  Typically will be called with size in the 8k - 64k range. */
{
uglyf("reading remote data - %d bytes at %lld - on %s\n", size, offset, url);
FILE *f = mustOpen(url, "rb");
fseek(f, offset, SEEK_SET);
int sizeRead = fread(buffer, 1, size, f);
if (ferror(f))
    {
    warn("udcDataViaLocal failed to fetch %d bytes at %lld", size, offset);
    errnoAbort("file %s", url);
    }
carefulClose(&f);
return sizeRead;
}

boolean udcInfoViaLocal(char *url, struct udcRemoteFileInfo *retInfo)
/* Fill in *retTime with last modified time for file specified in url.
 * Return FALSE if file does not even exist. */
{
uglyf("checking remote info on %s\n", url);
struct stat status;
int ret = stat(url, &status);
if (ret < 0)
    return FALSE;
retInfo->updateTime = status.st_mtime;
retInfo->size = status.st_size;
return TRUE;
}

struct udcProtocol *udcProtocolNew(char *upToColon)
/* Build up a new protocol around a string such as "http" or "local" */
{
struct udcProtocol *prot;
AllocVar(prot);
if (sameString(upToColon, "local"))
    {
    prot->fetchData = udcDataViaLocal;
    prot->fetchInfo = udcInfoViaLocal;
    }
else
    {
    errAbort("Unrecognized protocol %s in udcProtNew", upToColon);
    }
return prot;
}

void udcProtocolFree(struct udcProtocol **pProt)
/* Free up protocol resources. */
{
freez(pProt);
}

static void setInitialCachedDataBounds(struct udcFile *file)
/* Open up bitmap file and read a little bit of it to see if cache is stale,
 * and if not to see if the initial part is cached.  Sets the data members
 * startData, and endData.  If the case is stale it makes fresh empty
 * cacheDir/sparseData and cacheDir/bitmap files. */
{
/* Get existing bitmap, and if it's stale clean up. */
struct udcBitmap *bits = udcBitmapOpen(file->bitmapFileName);
if (bits != NULL)
    {
    if (bits->remoteUpdate != file->updateTime || bits->fileSize != file->size)
	{
        udcBitmapClose(&bits);
	remove(file->bitmapFileName);
	remove(file->sparseFileName);
	}
    }

/* If no bitmap, then create one, and also an empty sparse data file. */
if (bits == NULL)
    {
    udcNewCreateBitmapAndSparse(file, file->updateTime, file->size);
    bits = udcBitmapOpen(file->bitmapFileName);
    if (bits == NULL)
        internalErr();
    }

/* Read in a little bit from bitmap while we have it open to see if we have anything cached. */
if (file->size > 0)
    {
    Bits b = fgetc(bits->f);
    int endBlock = (file->size + udcBlockSize - 1)/udcBlockSize;
    if (endBlock > 8)
        endBlock = 8;
    int initialCachedBlocks = bitFindClear(&b, 0, endBlock);
    file->startData = 0;
    file->endData = endBlock * udcBlockSize;
    }

udcBitmapClose(&bits);
}


struct udcFile *udcFileOpen(char *url)
/* Open up a cached file. */
{
/* Parse out protocol.  Make it "local" if none specified. */
char *protocol, *afterProtocol;
char *colon = strchr(url, ':');
struct udcProtocol *prot;
if (colon != NULL)
    {
    int colonPos = colon - url;
    protocol = cloneStringZ(url, colonPos);
    afterProtocol = url + colonPos + 1;
    }
else
    {
    protocol = cloneString("local");
    if (url[0] != '/')
        errAbort("Local urls must start at /");
    if (stringIn("..", url) || stringIn("~", url) || stringIn("//", url) ||
    	stringIn("/./", url) || endsWith("/.", url))
	{
	errAbort("relative paths not allowed in local urls (%s)", url);
	}
    afterProtocol = url+1;
    }
prot = udcProtocolNew(protocol);

/* Figure out if anything exists. */
struct udcRemoteFileInfo info;
if (!prot->fetchInfo(url, &info))
    {
    udcProtocolFree(&prot);
    return NULL;
    }

/* Allocate file object and start filling it in. */
struct udcFile *file;
AllocVar(file);
file->url = cloneString(url);
file->protocol = protocol;
file->prot = prot;
file->updateTime = info.updateTime;
file->size = info.size;
int len = strlen(udcRootDir) + 1 + strlen(protocol) + 1 + strlen(afterProtocol) + 1;
file->cacheDir = needMem(len);
safef(file->cacheDir, len, "%s/%s/%s", udcRootDir, protocol, afterProtocol);

/* Make directory. */
uglyf("file->cacheDir=%s\n", file->cacheDir);
makeDirsOnPath(file->cacheDir);

/* Create file names for bitmap and data portions. */
file->bitmapFileName = fileNameInCacheDir(file, bitmapName);
file->sparseFileName = fileNameInCacheDir(file, sparseDataName);

/* Figure out a little bit about the extent of the good cached data if any. */
setInitialCachedDataBounds(file);
file->fSparse = mustOpen(file->sparseFileName, "rb+");
return file;
}

void udcFileClose(struct udcFile **pFile)
/* Close down cached file. */
{
freez(pFile);
}

void readBitsIntoBuf(FILE *f, int headerSize, int bitStart, int bitEnd,
	Bits **retBits, int *retPartOffset)
/* Do some bit-to-byte offset conversions and read in all the bytes that
 * have information in the bits we're interested in. */
{
int byteStart = bitStart/8;
int byteEnd = (bitEnd+7)/8;
int byteSize = byteEnd - byteStart;
uglyf("Reading bits representing blocks %d-%d, byteStart=%d, byteEnd=%d, byteSize=%d\n", bitStart, bitEnd, byteStart, byteEnd, byteSize);
Bits *bits = needLargeMem(byteSize);
fseek(f, headerSize + byteStart, SEEK_SET);
mustRead(f, bits, byteSize);
*retBits = bits;
*retPartOffset = byteStart*8;
}

boolean allBitsSetInFile(FILE *f, int headerSize, int bitStart, int bitEnd)
/* Return TRUE if all bits in file between start and end are set. */
{
int partOffset;
Bits *bits;

readBitsIntoBuf(f, headerSize, bitStart, bitEnd, &bits, &partOffset);

int partBitStart = bitStart - partOffset;
int partBitEnd = bitEnd - partOffset;
int bitSize = bitEnd - bitStart;
int nextClearBit = bitFindClear(bits, partBitStart, bitSize);
boolean allSet = (nextClearBit >= partBitEnd);

freeMem(bits);
return allSet;
}

static void fetchMissingBlocks(struct udcFile *file, int startBlock, int blockCount,
	int blockSize)
/* Fetch missing blocks from remote and put them into file. */
{
/* TODO: rework this so that it just fetches something like up to 64k at a time,
 * and locks/updates/unlocks the bitmap around each one of these fetches. */
bits64 startPos = (bits64)startBlock * blockSize;
bits64 bufSize = (bits64)blockCount * blockSize;
void *buf  = needLargeMem(bufSize);
file->prot->fetchData(file->url, startPos, bufSize, buf);
fseek(file->fSparse, startPos, SEEK_SET);
mustWrite(file->fSparse, buf, bufSize);
}

static void fetchMissingBits(struct udcFile *file, struct udcBitmap *bits,
	bits64 start, bits64 end, bits64 *retFetchedStart, bits64 *retFetchedEnd)
/* Scan through relevant parts of bitmap, fetching blocks we don't already have. */
{
/* Fetch relevant part of bitmap into memory */
int partOffset;
Bits *b;
int startBlock = start / bits->blockSize;
int endBlock = (end + bits->blockSize - 1) / bits->blockSize;
readBitsIntoBuf(bits->f, udcBitmapHeaderSize, startBlock, endBlock, &b, &partOffset);

/* Loop around first skipping set bits, then fetching clear bits. */
boolean dirty = FALSE;
int s = startBlock - partOffset;
int e = endBlock - partOffset;
for (;;)
    {
    int nextClearBit = bitFindClear(b, s, e);
    if (nextClearBit >= e)
        break;
    int nextSetBit = bitFindSet(b, nextClearBit, e);
    int clearSize =  nextSetBit - nextClearBit;
    fetchMissingBlocks(file, nextClearBit + partOffset, clearSize, bits->blockSize);
    bitSetRange(b, nextClearBit, clearSize);
    dirty = TRUE;
    if (nextSetBit >= e)
        break;
    s = nextSetBit;
    }

if (dirty)
    {
    /* Update bitmap on disk.... */
    int byteStart = startBlock/8;
    int byteEnd = (endBlock+7)/8;
    int byteSize = byteEnd - byteStart;
    uglyf("updating bitmap block %d-%d\n", startBlock, endBlock);
    fseek(bits->f, byteStart + udcBitmapHeaderSize, SEEK_SET);
    mustWrite(bits->f, b, byteSize);
    }

freeMem(b);
*retFetchedStart = startBlock * bits->blockSize;
*retFetchedEnd = endBlock * bits->blockSize;
}

boolean udcCacheContains(struct udcFile *file, struct udcBitmap *bits, bits64 offset, int size)
/* Return TRUE if cache already contains region. */
{
bits64 endOffset = offset + size;
int startBlock = offset / bits->blockSize;
int endBlock = (endOffset + bits->blockSize - 1) / bits->blockSize;
return allBitsSetInFile(bits->f, udcBitmapHeaderSize, startBlock, endBlock);
}


void udcFetchMissing(struct udcFile *file, struct udcBitmap *bits, bits64 start, bits64 end)
/* Fetch missing pieces of data from file */
{
/* Call lower level routine fetch remote data that is not already here. */
bits64 fetchedStart, fetchedEnd;
fetchMissingBits(file, bits, start, end, &fetchedStart, &fetchedEnd);

/* Update file startData/endData members to include new data (and old as well if
 * the new data overlaps the old). */
if (rangeIntersection(file->startData, file->endData, fetchedStart, fetchedEnd) >= 0)
    {
    if (fetchedStart > file->startData)
        fetchedStart = file->startData;
    if (fetchedEnd < file->endData)
        fetchedEnd = file->endData;
    }
file->startData = fetchedStart;
file->endData = fetchedEnd;
}

void udcCachePreload(struct udcFile *file, bits64 offset, int size)
/* Make sure that given data is in cache - fetching it remotely if need be. */
{
struct udcBitmap *bits = udcBitmapOpen(file->bitmapFileName);
if (!udcCacheContains(file, bits, offset, size))
    udcFetchMissing(file, bits, offset, offset+size);
udcBitmapClose(&bits);
}

void udcRead(struct udcFile *file, void *buf, int size)
/* Read a block from file. */
{
if (file->offset < file->startData || file->offset + size > file->endData)
    {
    udcCachePreload(file, file->offset, size);
    fseek(file->fSparse, file->offset, SEEK_SET);
    }
uglyf("Reading %d from localFile at pos %llu\n", size, file->offset);
mustRead(file->fSparse, buf, size);
file->offset += size;
}

void udcSeek(struct udcFile *file, bits64 offset)
/* Seek to a particular position in file. */
{
file->offset = offset;
}

void testCache(char *sourceUrl, bits64 offset, bits64 size, char *outFile)
/* testCache - Experiments with file cacher.. */
{
/* Open up cache and file. */
struct udcFile *file = udcFileOpen(sourceUrl);

/* Read data and write it back out */
void *buf = needMem(size);
udcSeek(file, offset);
udcRead(file, buf, size);
writeGulp(outFile, buf, size);

/* Clean up. */
udcFileClose(&file);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
testCache(argv[1], sqlUnsigned(argv[2]), sqlUnsigned(argv[3]), argv[4]);
return 0;
}
