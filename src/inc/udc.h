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

#ifndef UDC_H
#define UDC_H

struct udcFile;
/* Handle to a cached file.  Inside of structure mysterious unless you are udc.c. */

struct udcFile *udcFileOpen(char *url, char *cacheDir);
/* Open up a cached file. */

void udcFileClose(struct udcFile **pFile);
/* Close down cached file. */

int udcRead(struct udcFile *file, void *buf, int size);
/* Read a block from file.  Return amount actually read. */

void udcMustRead(struct udcFile *file, void *buf, int size);
/* Read a block from file.  Abort if any problem, including EOF before size is read. */

#define udcMustReadOne(file, var) udcMustRead(file, &(var), sizeof(var))
/* Read one variable from file or die. */

bits64 udcReadBits64(struct udcFile *file, boolean isSwapped);
/* Read and optionally byte-swap 64 bit entity. */

bits32 udcReadBits32(struct udcFile *file, boolean isSwapped);
/* Read and optionally byte-swap 32 bit entity. */

bits16 udcReadBits16(struct udcFile *file, boolean isSwapped);
/* Read and optionally byte-swap 16 bit entity. */

int udcGetChar(struct udcFile *file);
/* Get next character from file or die trying. */

void udcSeek(struct udcFile *file, bits64 offset);
/* Seek to a particular (absolute) position in file. */

bits64 udcTell(struct udcFile *file);
/* Return current file position. */

bits64 udcCleanup(char *cacheDir, double maxDays);
/* Remove cached files older than maxDays old. */

char *udcDefaultDir();
/* Get default directory for cache.  Use this for the udcFileOpen call if you
 * don't have anything better.... */

void udcSetDefaultDir(char *path);
/* Set default directory for cache */

#define udcDevicePrefix "udc:"
/* Prefix used by convention to indicate a file should be accessed via udc.  This is
 * followed by the local path name or a url, so in common practice you see things like:
 *     udc:http://genome.ucsc.edu/goldenPath/hg18/tracks/someTrack.bb */

#endif /* UDC_H */
