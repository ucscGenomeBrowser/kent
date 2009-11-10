/* Wrappers around zlib to make interfacing to it a bit easier. */

#include "common.h"
#include <zlib.h>

static char *zlibErrorMessage(int err)
/* Convert error code to errorMessage */
{
switch (err)
    {
    case Z_STREAM_END:
        return "zlib stream end";
    case Z_NEED_DICT:
        return "zlib need dictionary";
    case Z_ERRNO:
        return "zlib errno";
    case Z_STREAM_ERROR:
        return "zlib data error";
    case Z_DATA_ERROR:
        return "zlib data error";
    case Z_MEM_ERROR:
        return "zlib mem error";
    case Z_BUF_ERROR:
        return "zlib buf error";
    case Z_VERSION_ERROR:
        return "zlib version error";
    case Z_OK:
        return NULL;
    default:
	{
	static char msg[128];
	safef(msg, sizeof(msg), "zlib error code %d", err);
        return msg;
	}
    }
}

size_t zCompress(
	void *uncompressed, 	/* Start of area to compress. */
	size_t uncompressedSize,  /* Size of area to compress. */
	void *compBuf,       /* Where to put compressed bits */
	size_t compBufSize) /* Size of compressed bits - calculate using zCompBufSize */
/* Compress data from memory to memory.  Returns size after compression. */
{
uLongf compSize = compBufSize;
int err = compress((Bytef*)compBuf, &compSize, (Bytef*)uncompressed, (uLong)uncompressedSize);
if (err != 0)
    errAbort("Couldn't zCompress %lld bytes: %s", 
    	(long long)uncompressedSize, zlibErrorMessage(err));
return compSize;
}

size_t zCompBufSize(size_t uncompressedSize)
/* Return size of buffer needed to compress something of given size uncompressed. */
{
return 1.001*uncompressedSize + 13;
}

size_t zUncompress(
        void *compressed,	/* Compressed area */
	size_t compressedSize,	/* Size after compression */
	void *uncompBuf,	/* Where to put uncompressed bits */
	size_t uncompBufSize)	/* Max size of uncompressed bits. */
/* Uncompress data from memory to memory.  Returns size after decompression. */
{
uLongf uncSize = uncompBufSize;
int err = uncompress(uncompBuf,  &uncSize, compressed, compressedSize);
if (err != 0)
    errAbort("Couldn't zUncompress %lld bytes: %s", 
    	(long long)compressedSize, zlibErrorMessage(err));
return uncSize;
}

void zSelfTest(int count)
/* Run an internal diagnostic. */
{
bits32 testData[count];
int uncSize = count*sizeof(bits32);
int i;
for (i=0; i<count; ++i)
    testData[i] = i;
int compBufSize = zCompBufSize(uncSize);
char compBuf[compBufSize];
int compSize = zCompress(testData, uncSize, compBuf, compBufSize);
char uncBuf[uncSize];
zUncompress(compBuf, compSize, uncBuf, uncSize);
if (memcmp(uncBuf, testData, uncSize) != 0)
    errAbort("zSelfTest %d failed", count);
else
    verbose(2, "zSelfTest %d passed, compression ratio %3.1f\n", count, (double)compSize/uncSize);
}
