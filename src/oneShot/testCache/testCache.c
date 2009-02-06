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


static char const rcsid[] = "$Id: testCache.c,v 1.5 2009/02/06 08:26:52 kent Exp $";

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
    time_t updateTime;	/* Last modified timestamp. */
    bits64 offset;		/* Current offset in file. */
    char *cacheDir;		/* Directory for cached file parts. */
    FILE *f;			/* File handle for file with current block. */
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

static struct dyString *fileNameInCacheDir(struct udcFile *file, char *fileName)
/* Return the name of a file in the cache dir, from the cache root directory on down,
 * in a dyString.   Please dyStringFree the result when done. */
{
struct dyString *dy = dyStringNew(0);
dyStringAppend(dy, file->cacheDir);
dyStringAppendC(dy, '/');
dyStringAppend(dy, fileName);
return dy;
}

static void removeBitmapFile(struct udcFile *file)
/* Remove bitmap file. */
{
struct dyString *fileName = fileNameInCacheDir(file, bitmapName);
remove(fileName->string);
dyStringFree(&fileName);
}

void udcBitmapCreate(struct udcFile *file, bits64 remoteUpdate, bits64 remoteSize)
/* Create a new bitmap file around the given remoteUpdate time. */
{
uglyf("udcBitmapCreate(%s %llu %llu)\n", file->url, remoteUpdate, remoteSize);
struct dyString *fileName = fileNameInCacheDir(file, bitmapName);
FILE *f = mustOpen(fileName->string, "wb");
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
dyStringFree(&fileName);

/* Create an empty data file. */
fileName = fileNameInCacheDir(file, sparseDataName);
f = mustOpen(fileName->string, "wb");
carefulClose(&f);
dyStringFree(&fileName);
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
uglyf("fstat err = %d\n", err);

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

uglyf("blockSize %u, remoteUpdate %llu, fileSize %llu\n", bits->blockSize, bits->remoteUpdate, bits->fileSize);
return bits;
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

uglyf("protocol is %s\n", protocol);
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
int len = strlen(udcRootDir) + 1 + strlen(protocol) + 1 + strlen(afterProtocol) + 1;
file->cacheDir = needMem(len);
safef(file->cacheDir, len, "%s/%s/%s", udcRootDir, protocol, afterProtocol);

/* Make directory. */
uglyf("file->cacheDir=%s\n", file->cacheDir);
makeDirsOnPath(file->cacheDir);

/* Open up data file handle. */
struct dyString *fileName = fileNameInCacheDir(file, sparseDataName);
if ((file->f = fopen(fileName->string, "rb+")) == NULL)
    {
    removeBitmapFile(file);
    file->f = mustOpen(fileName->string, "wb+");
    }
dyStringFree(&fileName);

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
struct dyString *dataFileName = fileNameInCacheDir(file, sparseDataName);
bits64 startPos = (bits64)startBlock * blockSize;
bits64 bufSize = (bits64)blockCount * blockSize;
void *buf  = needLargeMem(bufSize);
file->prot->fetchData(file->url, startPos, bufSize, buf);
FILE *f = mustOpen(dataFileName->string, "r+");
fseek(f, startPos, SEEK_SET);
mustWrite(f, buf, bufSize);
carefulClose(&f);
}

static void fetchMissingBits(struct udcFile *file, struct udcBitmap *bits,
	bits64 start, bits64 end)
/* Scan through relevant parts of bitmap, fetching blocks we don't already have. */
{
uglyf("missing 1 - file %p, bits %p, start %llu, end %llu\n", file, bits, start, end);
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
    fseek(bits->f, byteStart + udcBitmapHeaderSize, SEEK_SET);
    mustWrite(bits->f, b, byteSize);
    }

freeMem(b);
}

boolean udcCacheContains(struct udcFile *file, bits64 offset, int size)
/* Return TRUE if cache already contains region. */
{
boolean result = FALSE;
struct dyString *fileName = fileNameInCacheDir(file, bitmapName);
struct udcBitmap *bits = udcBitmapOpen(fileName->string);
if (bits != NULL)
    {
    bits64 endOffset = offset + size;
    int startBlock = offset / bits->blockSize;
    int endBlock = (endOffset + bits->blockSize - 1) / bits->blockSize;
    result = allBitsSetInFile(bits->f, udcBitmapHeaderSize, startBlock, endBlock);
    udcBitmapClose(&bits);
    }
dyStringFree(&fileName);
return result;
}


void udcFetchMissing(struct udcFile *file, bits64 start, bits64 end)
/* Fetch missing pieces of data from file */
{
/* Ping server for file size and last modification time. */
struct udcRemoteFileInfo info;
if (!file->prot->fetchInfo(file->url, &info))
    errAbort("Remote file %s doesn't seem to exist", file->url);

/* Get bitmap and compare timestamp.  Delete bitmap if it's out of date. */
struct dyString *fileName = fileNameInCacheDir(file, bitmapName);
struct udcBitmap *bits = udcBitmapOpen(fileName->string);
if (bits != NULL)
    {
    if (bits->remoteUpdate != info.updateTime || bits->fileSize != info.size)
	{
        udcBitmapClose(&bits);
	remove(fileName->string);
	}
    }

/* If need be create new bitmap. */
if (bits == NULL)
    {
    udcBitmapCreate(file, info.updateTime, info.size);
    bits = udcBitmapOpen(fileName->string);
    if (bits == NULL)
        internalErr();
    }

/* Call lower level routine to finish up job. */
fetchMissingBits(file, bits, start, end);

/* Clean up */
udcBitmapClose(&bits);
dyStringFree(&fileName);
}

void udcCachePreload(struct udcFile *file, bits64 offset, int size)
/* Make sure that given data is in cache - fetching it remotely if need be. */
{
if (!udcCacheContains(file, offset, size))
    {
    udcFetchMissing(file, offset, offset+size);
    }
}

void udcRead(struct udcFile *file, void *buf, int size)
/* Read a block from file. */
{
uglyf("ok 2.1 file %p, buf %p, size %d\n", file, buf, size);
if (file->offset < file->startData || file->offset + size > file->endData)
    {
    udcCachePreload(file, file->offset, size);
    fseek(file->f, file->offset, SEEK_SET);
    }
mustRead(file->f, buf, size);
uglyf("ok 2.6\n");
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
uglyf("ok 1 offset %llu, size %llu\n", offset, size);
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
