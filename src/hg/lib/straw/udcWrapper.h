/* udcWrapper - a wrapper library file to make the C udc.h library available for use
 * in C++ code.  The udc library provides a caching system that keeps blocks of data
 * fetched from URLs in sparse local files for quick use the next time the data is needed. 
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

#ifndef UDCWRAPPER_H
#define UDCWRAPPER_H

/* These definitions from the kent commom.h library */
#define boolean int
#define bits64 unsigned long long
#define bits32 unsigned long
#define bits16 unsigned

extern "C" struct udcFile;
/* Handle to a cached file.  Inside of structure mysterious unless you are udc.c. */

extern "C" struct udcFile *udcFileMayOpen(char *url, char *cacheDir);
/* Open up a cached file. cacheDir may be null in which case udcDefaultDir() will be
 * used.  Return NULL if file doesn't exist. */

extern "C" struct udcFile *udcFileOpen(char *url, char *cacheDir);
/* Open up a cached file.  cacheDir may be null in which case udcDefaultDir() will be
 * used.  Abort if if file doesn't exist. */

extern "C" void udcFileClose(struct udcFile **pFile);
/* Close down cached file. */

extern "C" bits64 udcRead(struct udcFile *file, void *buf, bits64 size);
/* Read a block from file.  Return amount actually read. */

#define udcReadOne(file, var) udcRead(file, &(var), sizeof(var))
/* Read one variable from file or die. */

extern "C" void udcMustRead(struct udcFile *file, void *buf, bits64 size);
/* Read a block from file.  Abort if any problem, including EOF before size is read. */

#define udcMustReadOne(file, var) udcMustRead(file, &(var), sizeof(var))
/* Read one variable from file or die. */

extern "C" bits64 udcReadBits64(struct udcFile *file, boolean isSwapped);
/* Read and optionally byte-swap 64 bit entity. */

extern "C" bits32 udcReadBits32(struct udcFile *file, boolean isSwapped);
/* Read and optionally byte-swap 32 bit entity. */

extern "C" bits16 udcReadBits16(struct udcFile *file, boolean isSwapped);
/* Read and optionally byte-swap 16 bit entity. */

extern "C" float udcReadFloat(struct udcFile *file, boolean isSwapped);
/* Read and optionally byte-swap floating point number. */

extern "C" double udcReadDouble(struct udcFile *file, boolean isSwapped);
/* Read and optionally byte-swap double-precision floating point number. */

extern "C" int udcGetChar(struct udcFile *file);
/* Get next character from file or die trying. */

extern "C" char *udcReadLine(struct udcFile *file);
/* Fetch next line from udc cache. */

extern "C" char *udcReadStringAndZero(struct udcFile *file);
/* Read in zero terminated string from file.  Do a freeMem of result when done. */

extern "C" char *udcFileReadAll(char *url, char *cacheDir, size_t maxSize, size_t *retSize);
/* Read a complete file via UDC. The cacheDir may be null in which case udcDefaultDir()
 * will be used.  If maxSize is non-zero, check size against maxSize
 * and abort if it's bigger.  Returns file data (with an extra terminal for the
 * common case where it's treated as a C string).  If retSize is non-NULL then
 * returns size of file in *retSize. Do a freeMem or freez of the returned buffer
 * when done. */

extern "C" struct lineFile *udcWrapShortLineFile(char *url, char *cacheDir, size_t maxSize);
/* Read in entire short (up to maxSize) url into memory and wrap a line file around it.
 * The cacheDir may be null in which case udcDefaultDir() will be used.  If maxSize
 * is zero then a default value (currently 64 meg) will be used. */

extern "C" void udcSeek(struct udcFile *file, bits64 offset);
/* Seek to a particular (absolute) position in file. */

extern "C" void udcSeekCur(struct udcFile *file, bits64 offset);
/* Seek to a particular (from current) position in file. */

extern "C" bits64 udcTell(struct udcFile *file);
/* Return current file position. */

extern "C" bits64 udcCleanup(char *cacheDir, double maxDays, boolean testOnly);
/* Remove cached files older than maxDays old. If testOnly is set
 * no clean up is done, but the size of the files that would be
 * cleaned up is still. */

extern "C" void udcParseUrl(char *url, char **retProtocol, char **retAfterProtocol, char **retColon);
/* Parse the URL into components that udc treats separately. 
 * *retAfterProtocol is Q-encoded to keep special chars out of filenames.  
 * Free all *ret's except *retColon when done. */

extern "C" void udcParseUrlFull(char *url, char **retProtocol, char **retAfterProtocol, char **retColon,
		     char **retAuth);
/* Parse the URL into components that udc treats separately.
 * *retAfterProtocol is Q-encoded to keep special chars out of filenames.  
 * Free all *ret's except *retColon when done. */

extern "C" char *udcDefaultDir();
/* Get default directory for cache.  Use this for the udcFileOpen call if you
 * don't have anything better.... */

extern "C" void udcSetDefaultDir(char *path);
/* Set default directory for cache */

extern "C" void udcDisableCache();
/* Switch off caching. Re-enable with udcSetDefaultDir */

#define udcDevicePrefix "udc:"
/* Prefix used by convention to indicate a file should be accessed via udc.  This is
 * followed by the local path name or a url, so in common practice you see things like:
 *     udc:http://genome.ucsc.edu/goldenPath/hg18/tracks/someTrack.bb */

extern "C" struct slName *udcFileCacheFiles(char *url, char *cacheDir);
/* Return low-level list of files used in cache. */

extern "C" char *udcPathToUrl(const char *path, char *buf, size_t size, char *cacheDir);
/* Translate path into an URL, store in buf, return pointer to buf if successful
 * and NULL if not. */

extern "C" long long int udcSizeFromCache(char *url, char *cacheDir);
/* Look up the file size from the local cache bitmap file, or -1 if there
 * is no cache for url. */

extern "C" time_t udcTimeFromCache(char *url, char *cacheDir);
/* Look up the file datetime from the local cache bitmap file, or 0 if there
 * is no cache for url. */

extern "C" unsigned long udcCacheAge(char *url, char *cacheDir);
/* Return the age in seconds of the oldest cache file.  If a cache file is
 * missing, return the current time (seconds since the epoch). */

extern "C" int udcCacheTimeout();
/* Get cache timeout (if local cache files are newer than this many seconds,
 * we won't ping the remote server to check the file size and update time). */

extern "C" void udcSetCacheTimeout(int timeout);
/* Set cache timeout (if local cache files are newer than this many seconds,
 * we won't ping the remote server to check the file size and update time). */

extern "C" time_t udcUpdateTime(struct udcFile *udc);
/* return udc->updateTime */

extern "C" boolean udcFastReadString(struct udcFile *f, char buf[256]);
/* Read a string into buffer, which must be long enough
 * to hold it.  String is in 'writeString' format. */

extern "C" off_t udcFileSize(char *url);
/* fetch remote or local file size from given URL or path */

extern "C" boolean udcExists(char *url);
/* return true if a remote or local file exists */

extern "C" boolean udcIsLocal(char *url);
/* return true if url is not a http or ftp file, just a normal local file path */

extern "C" void udcSetLog(FILE *fp);
/* Tell UDC where to log its statistics. */

extern "C" void udcMMap(struct udcFile *file);
/* Enable access to underlying file as memory using mmap.  udcMMapFetch
 * must be called to actually access regions of the file. */

extern "C" void *udcMMapFetch(struct udcFile *file, bits64 offset, bits64 size);
/* Return pointer to a region of the file in memory, ensuring that regions is
 * cached.. udcMMap must have been called to enable access.  This must be
 * called for first access to a range of the file or erroneous (zeros) data
 * maybe returned.  Maybe called multiple times on a range or overlapping
 * returns. */


#endif /* UDCWRAPPER_H */
