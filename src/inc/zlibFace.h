/* Wrappers around zlib to make interfacing to it a bit easier. */

#ifndef ZLIBFACE_H
#define ZLIBFACE_H

size_t zCompress(
	void *uncompressed, 	/* Start of area to compress. */
	size_t uncompressedSize,  /* Size of area to compress. */
	void *compBuf,       /* Where to put compressed bits */
	size_t compBufSize); /* Size of compressed bits - calculate using zCompBufSize */
/* Compress data from memory to memory.  Returns size after compression. */

size_t zCompBufSize(size_t uncompressedSize);
/* Return size of buffer needed to compress something of given size uncompressed. */

size_t zUncompress(
        void *compressed,	/* Compressed area */
	size_t compressedSize,	/* Size after compression */
	void *uncompBuf,	/* Where to put uncompressed bits */
	size_t uncompBufSize);	/* Max size of uncompressed bits. */
/* Uncompress data from memory to memory.  Returns size after decompression. */

void zSelfTest(int count);
/* Run an internal diagnostic. */

#endif /* ZLIBFACE_H */
