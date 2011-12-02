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

#include <sys/file.h>
#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "bits.h"
#include "linefile.h"
#include "portable.h"
#include "sig.h"
#include "net.h"
#include "cheapcgi.h"
#include "udc.h"


#define udcBlockSize (8*1024)
/* All fetch requests are rounded up to block size. */

#define udcMaxBytesPerRemoteFetch (udcBlockSize * 32)
/* Very large remote reads are broken down into chunks this size. */

struct connInfo
/* Socket descriptor and associated info, for keeping net connections open. */
    {
    int socket;                 /* Socket descriptor for data connection (or 0). */
    bits64 offset;		/* Current file offset of socket. */
    int ctrlSocket;             /* (FTP only) Control socket descriptor or 0. */
    };

typedef int (*UdcDataCallback)(char *url, bits64 offset, int size, void *buffer,
			       struct connInfo *ci);
/* Type for callback function that fetches file data. */

struct udcRemoteFileInfo
/* Information about a remote file. */
    {
    bits64 updateTime;	/* Last update in seconds since 1970 */
    bits64 size;	/* Remote file size */
    struct connInfo ci; /* Connection info for open net connection */
    };

typedef boolean (*UdcInfoCallback)(char *url, struct udcRemoteFileInfo *retInfo);
/* Type for callback function that fetches file timestamp and size. */

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
    int fdSparse;		/* File descriptor for sparse data file. */
    boolean sparseReadAhead;    /* Read-ahead has something in the buffer */
    char *sparseReadAheadBuf;   /* Read-ahead buffer, if any */
    bits64 sparseRAOffset;      /* Read-ahead buffer offset */
    struct udcBitmap *bits;     /* udcBitMap */
    bits64 startData;		/* Start of area in file we know to have data. */
    bits64 endData;		/* End of area in file we know to have data. */
    bits32 bitmapVersion;	/* Version of associated bitmap we were opened with. */
    struct connInfo connInfo;   /* Connection info for open net connection. */
    };

struct udcBitmap
/* The control structure including the bitmap of blocks that are cached. */
    {
    struct udcBitmap *next;	/* Next in list. */
    bits32 blockSize;		/* Number of bytes per block of file. */
    bits64 remoteUpdate;	/* Remote last update time. */
    bits64 fileSize;		/* File size */
    bits32 version;		/* Version - increments each time cache is stale. */
    bits64 localUpdate;		/* Time we last fetched new data into cache. */
    bits64 localAccess;		/* Time we last accessed data. */
    boolean isSwapped;		/* If true need to swap all bytes on read. */
    int fd;			/* File descriptor for file with current block. */
    };
static char *bitmapName = "bitmap";
static char *sparseDataName = "sparseData";
#define udcBitmapHeaderSize (64)
static int cacheTimeout = 0;

#define MAX_SKIP_TO_SAVE_RECONNECT (udcMaxBytesPerRemoteFetch / 2)

static void readAndIgnore(int sd, bits64 size)
/* Read size bytes from sd and return. */
{
static char *buf = NULL;
if (buf == NULL)
    buf = needMem(udcBlockSize);
bits64 remaining = size, total = 0;
while (remaining > 0)
    {
    bits64 chunkSize = min(remaining, udcBlockSize);
    bits64 rd = read(sd, buf, chunkSize);
    if (rd < 0)
	errnoAbort("readAndIgnore: error reading socket after %lld bytes", total);
    remaining -= rd;
    total += rd;
    }
if (total < size)
    errAbort("readAndIgnore: got EOF at %lld bytes (wanted %lld)", total, size);
}

static int connInfoGetSocket(struct connInfo *ci, char *url, bits64 offset, int size)
/* If ci has an open socket and the given offset matches ci's current offset,
 * reuse ci->socket.  Otherwise close the socket, open a new one, and update ci,
 * or return -1 if there is an error opening a new one. */
{
if (ci != NULL && ci->socket > 0 && ci->offset != offset)
    {
    bits64 skipSize = (offset - ci->offset);
    if (skipSize > 0 && skipSize <= MAX_SKIP_TO_SAVE_RECONNECT)
	{
	verbose(2, "!! skipping %lld bytes @%lld to avoid reconnect\n", skipSize, ci->offset);
	readAndIgnore(ci->socket, skipSize);
	ci->offset = offset;
	}
    else
	{
	verbose(2, "Offset mismatch (ci %lld != new %lld), reopening.\n", ci->offset, offset);
	mustCloseFd(&(ci->socket));
	if (ci->ctrlSocket > 0)
	    mustCloseFd(&(ci->ctrlSocket));
	ZeroVar(ci);
	}
    }
int sd;
if (ci == NULL || ci->socket <= 0)
    {
    char rangeUrl[2048];
    if (ci == NULL)
	{
	safef(rangeUrl, sizeof(rangeUrl), "%s;byterange=%lld-%lld",
	      url, offset, (offset + size - 1));
	sd = netUrlOpen(rangeUrl);
	}
    else
	{
	safef(rangeUrl, sizeof(rangeUrl), "%s;byterange=%lld-", url, offset);
	sd = ci->socket = netUrlOpenSockets(rangeUrl, &(ci->ctrlSocket));
	ci->offset = offset;
	}
    if (sd < 0)
	return -1;
    if (startsWith("http", url))
	{
	char *newUrl = NULL;
	int newSd = 0;
	if (!netSkipHttpHeaderLinesHandlingRedirect(sd, rangeUrl, &newSd, &newUrl))
	    return -1;
	if (newUrl)
	    {
	    freeMem(newUrl); 
	    sd = newSd;
	    if (ci != NULL)
		ci->socket = newSd;
	    }
	}
    }
else
    sd = ci->socket;
return sd;
}

/********* Section for local file protocol **********/

static char *assertLocalUrl(char *url)
/* Make sure that url is local and return bits past the protocol. */
{
if (startsWith("local:", url))
    url += 6;
if (url[0] != '/')
    errAbort("Local urls must start at /");
if (stringIn("..", url) || stringIn("~", url) || stringIn("//", url) ||
    stringIn("/./", url) || endsWith("/.", url))
    {
    errAbort("relative paths not allowed in local urls (%s)", url);
    }
return url;
}

static int udcDataViaLocal(char *url, bits64 offset, int size, void *buffer, struct connInfo *ci)
/* Fetch a block of data of given size into buffer using the http: protocol.
* Returns number of bytes actually read.  Does an errAbort on
* error.  Typically will be called with size in the 8k - 64k range. */
{
/* Need to check time stamp here. */
verbose(2, "reading remote data - %d bytes at %lld - on %s\n", size, offset, url);
url = assertLocalUrl(url);
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

static boolean udcInfoViaLocal(char *url, struct udcRemoteFileInfo *retInfo)
/* Fill in *retTime with last modified time for file specified in url.
 * Return FALSE if file does not even exist. */
{
verbose(2, "checking remote info on %s\n", url);
url = assertLocalUrl(url);
struct stat status;
int ret = stat(url, &status);
if (ret < 0)
    return FALSE;
retInfo->updateTime = status.st_mtime;
retInfo->size = status.st_size;
return TRUE;
}

/********* Section for transparent file protocol **********/

static int udcDataViaTransparent(char *url, bits64 offset, int size, void *buffer,
				 struct connInfo *ci)
/* Fetch a block of data of given size into buffer using the http: protocol.
* Returns number of bytes actually read.  Does an errAbort on
* error.  Typically will be called with size in the 8k - 64k range. */
{
internalErr();	/* Should not get here. */
return size;
}

static boolean udcInfoViaTransparent(char *url, struct udcRemoteFileInfo *retInfo)
/* Fill in *retInfo with last modified time for file specified in url.
 * Return FALSE if file does not even exist. */
{
internalErr();	/* Should not get here. */
return FALSE;
}

/********* Section for slow local file protocol - simulates network... **********/

static int udcDataViaSlow(char *url, bits64 offset, int size, void *buffer, struct connInfo *ci)
/* Fetch a block of data of given size into buffer using the http: protocol.
* Returns number of bytes actually read.  Does an errAbort on
* error.  Typically will be called with size in the 8k - 64k range. */
{
verbose(2, "slow reading remote data - %d bytes at %lld - on %s\n", size, offset, url);
sleep1000(500);
char *fileName = url + 5;  /* skip over 'slow:' */
FILE *f = mustOpen(fileName, "rb");
fseek(f, offset, SEEK_SET);
char *pt = buffer;
int i, step=1024;
int sizeRead = 0;
for (i=0; i<size; i += step)
    {
    sleep1000(250);
    int readChunk = size - i;
    if (readChunk > step)
        readChunk = step;
    int oneReadSize = fread(pt, 1, readChunk, f);
    verbose(2, "slowly read %d bytes\n", oneReadSize);
    if (ferror(f))
	{
	warn("udcDataViaSlow failed to fetch %d bytes at %lld", size, offset);
	errnoAbort("file %s", fileName);
	}
    pt += step;
    sizeRead += oneReadSize;
    }
carefulClose(&f);
return sizeRead;
}

static boolean udcInfoViaSlow(char *url, struct udcRemoteFileInfo *retInfo)
/* Fill in *retTime with last modified time for file specified in url.
 * Return FALSE if file does not even exist. */
{
char *fileName = url + 5;  /* skip over 'slow:' */
verbose(2, "slow checking remote info on %s\n", url);
sleep1000(500);
struct stat status;
int ret = stat(fileName, &status);
if (ret < 0)
    return FALSE;
retInfo->updateTime = status.st_mtime;
retInfo->size = status.st_size;
return TRUE;
}

/********* Section for http protocol **********/

int udcDataViaHttpOrFtp(char *url, bits64 offset, int size, void *buffer, struct connInfo *ci)
/* Fetch a block of data of given size into buffer using url's protocol,
 * which must be http, https or ftp.  Returns number of bytes actually read.
 * Does an errAbort on error.
 * Typically will be called with size in the 8k-64k range. */
{
if (startsWith("http://",url) || startsWith("https://",url) || startsWith("ftp://",url))
    verbose(2, "reading http/https/ftp data - %d bytes at %lld - on %s\n", size, offset, url);
else
    errAbort("Invalid protocol in url [%s] in udcDataViaFtp, only http, https, or ftp supported",
	     url); 
int sd = connInfoGetSocket(ci, url, offset, size);
if (sd < 0)
    errAbort("Can't get data socket for %s", url);
int rd = 0, total = 0, remaining = size;
char *buf = (char *)buffer;
while ((remaining > 0) && ((rd = read(sd, buf, remaining)) > 0))
    {
    total += rd;
    buf += rd;
    remaining -= rd;
    }
if (rd == -1)
    errnoAbort("udcDataViaHttpOrFtp: error reading socket");
if (ci == NULL)
    mustCloseFd(&sd);
else
    ci->offset += total;
return total;
}

boolean udcInfoViaHttp(char *url, struct udcRemoteFileInfo *retInfo)
/* Gets size and last modified time of URL
 * and returns status of HEAD GET. */
{
verbose(2, "checking http remote info on %s\n", url);
struct hash *hash = newHash(0);
int status = netUrlHead(url, hash);
if (status != 200) // && status != 302 && status != 301)
    return FALSE;
char *sizeString = hashFindValUpperCase(hash, "Content-Length:");
if (sizeString == NULL)
    {
    /* try to get remote file size by an alternate method */
    retInfo->size = netUrlSizeByRangeResponse(url);
    if (retInfo->size < 0)
	{
    	hashFree(&hash);
	errAbort("No Content-Length: returned in header for %s, can't proceed, sorry", url);
	}
    }
else
    {
    retInfo->size = atoll(sizeString);
    }

char *lastModString = hashFindValUpperCase(hash, "Last-Modified:");
if (lastModString == NULL)
    {
    // Date is a poor substitute!  It will always appear that the cache is stale.
    // But at least we can read files from dropbox.com.
    lastModString = hashFindValUpperCase(hash, "Date:");
    if (lastModString == NULL)
	{
	hashFree(&hash);
	errAbort("No Last-Modified: or Date: returned in header for %s, can't proceed, sorry", url);
	}
    }
struct tm tm;
time_t t;
// Last-Modified: Wed, 15 Nov 1995 04:58:08 GMT
// This will always be GMT
if (strptime(lastModString, "%a, %d %b %Y %H:%M:%S %Z", &tm) == NULL)
    { /* Handle error */;
    hashFree(&hash);
    errAbort("unable to parse last-modified string [%s]", lastModString);
    }
t = mktimeFromUtc(&tm);
if (t == -1)
    { /* Handle error */;
    hashFree(&hash);
    errAbort("mktimeFromUtc failed while converting last-modified string [%s] from UTC time", lastModString);
    }
retInfo->updateTime = t;

hashFree(&hash);
return status;
}


/********* Section for ftp protocol **********/

// fetchData method: See udcDataViaHttpOrFtp above.

boolean udcInfoViaFtp(char *url, struct udcRemoteFileInfo *retInfo)
/* Gets size and last modified time of FTP URL */
{
verbose(2, "checking ftp remote info on %s\n", url);
long long size = 0;
time_t t, tUtc;
struct tm *tm = NULL;
// TODO: would be nice to add int *retCtrlSocket to netGetFtpInfo so we can stash 
// in retInfo->connInfo and keep socket open.
boolean ok = netGetFtpInfo(url, &size, &tUtc);
if (!ok)
    return FALSE;
// Convert UTC to localtime
tm = localtime(&tUtc);
t = mktimeFromUtc(tm);
if (t == -1)
    { /* Handle error */;
    errAbort("mktimeFromUtc failed while converting FTP UTC last-modified time %ld to local time", (long) tUtc);
    }
retInfo->size = size;
retInfo->updateTime = t;
return TRUE;
}


/********* Non-protocol-specific bits **********/


static char *fileNameInCacheDir(struct udcFile *file, char *fileName)
/* Return the name of a file in the cache dir, from the cache root directory on down.
 * Do a freeMem on this when done. */
{
int dirLen = strlen(file->cacheDir);
int nameLen = strlen(fileName);
char *path = needMem(dirLen + nameLen + 2);
memcpy(path, file->cacheDir, dirLen);
path[dirLen] = '/';
memcpy(path+dirLen+1, fileName, nameLen);
return path;
}

static void udcNewCreateBitmapAndSparse(struct udcFile *file, 
	bits64 remoteUpdate, bits64 remoteSize, bits32 version)
/* Create a new bitmap file around the given remoteUpdate time. */
{
int fd = mustOpenFd(file->bitmapFileName, O_WRONLY | O_CREAT | O_TRUNC);
bits32 sig = udcBitmapSig;
bits32 blockSize = udcBlockSize;
bits64 reserved64 = 0;
bits32 reserved32 = 0;
int blockCount = (remoteSize + udcBlockSize - 1)/udcBlockSize;
int bitmapSize = bitToByteSize(blockCount);

/* Write out fixed part of header. */
writeOneFd(fd, sig);
writeOneFd(fd, blockSize);
writeOneFd(fd, remoteUpdate);
writeOneFd(fd, remoteSize);
writeOneFd(fd, version);
writeOneFd(fd, reserved32);
writeOneFd(fd, reserved64);
writeOneFd(fd, reserved64);
writeOneFd(fd, reserved64);
writeOneFd(fd, reserved64);
long long offset = mustLseek(fd, 0, SEEK_CUR);
if (offset != udcBitmapHeaderSize)
    errAbort("offset in fd=%d, f=%s is %lld, not expected udcBitmapHeaderSize %d",
	     fd, file->bitmapFileName, offset, udcBitmapHeaderSize);

/* Write out initial all-zero bitmap, using sparse-file method: write 0 to final address. */
unsigned char zero = 0;
mustLseek(fd, bitmapSize-1, SEEK_CUR);
mustWriteFd(fd, &zero, 1);

/* Clean up bitmap file and name. */
mustCloseFd(&fd);

/* Create an empty data file. */
fd = mustOpenFd(file->sparseFileName, O_WRONLY | O_CREAT | O_TRUNC);
mustCloseFd(&fd);
}

static struct udcBitmap *udcBitmapOpen(char *fileName)
/* Open up a bitmap file and read and verify header.  Return NULL if file doesn't
 * exist, abort on error. */
{
/* Open file, returning NULL if can't. */
int fd = open(fileName, O_RDWR);
if (fd < 0)
    {
    if (errno == ENOENT)
	return NULL;
    else
	errnoAbort("Can't open(%s, O_RDWR)", fileName);
    }

/* Get status info from file. */
struct stat status;
fstat(fd, &status);

/* Read signature and decide if byte-swapping is needed. */
// TODO: maybe buffer the I/O for performance?  Don't read past header - 
// fd offset needs to point to first data block when we return.
bits32 magic;
boolean isSwapped = FALSE;
mustReadOneFd(fd, magic);
if (magic != udcBitmapSig)
    {
    magic = byteSwap32(magic);
    isSwapped = TRUE;
    if (magic != udcBitmapSig)
       errAbort("%s is not a udcBitmap file", fileName);
    }

/* Allocate bitmap object, fill it in, and return it. */
bits32 reserved32;
bits64 reserved64;
struct udcBitmap *bits;
AllocVar(bits);
bits->blockSize = fdReadBits32(fd, isSwapped);
bits->remoteUpdate = fdReadBits64(fd, isSwapped);
bits->fileSize = fdReadBits64(fd, isSwapped);
bits->version = fdReadBits32(fd, isSwapped);
reserved32 = fdReadBits32(fd, isSwapped);
reserved64 = fdReadBits64(fd, isSwapped);
reserved64 = fdReadBits64(fd, isSwapped);
reserved64 = fdReadBits64(fd, isSwapped);
reserved64 = fdReadBits64(fd, isSwapped);
bits->localUpdate = status.st_mtime;
bits->localAccess = status.st_atime;
bits->isSwapped = isSwapped;
bits->fd = fd;

return bits;
}

static void udcBitmapClose(struct udcBitmap **pBits)
/* Free up resources associated with udcBitmap. */
{
struct udcBitmap *bits = *pBits;
if (bits != NULL)
    {
    mustCloseFd(&(bits->fd));
    freez(pBits);
    }
}

static struct udcProtocol *udcProtocolNew(char *upToColon)
/* Build up a new protocol around a string such as "http" or "local" */
{
struct udcProtocol *prot;
AllocVar(prot);
if (sameString(upToColon, "local"))
    {
    prot->fetchData = udcDataViaLocal;
    prot->fetchInfo = udcInfoViaLocal;
    }
else if (sameString(upToColon, "slow"))
    {
    prot->fetchData = udcDataViaSlow;
    prot->fetchInfo = udcInfoViaSlow;
    }
else if (sameString(upToColon, "http") || sameString(upToColon, "https"))
    {
    prot->fetchData = udcDataViaHttpOrFtp;
    prot->fetchInfo = udcInfoViaHttp;
    }
else if (sameString(upToColon, "ftp"))
    {
    prot->fetchData = udcDataViaHttpOrFtp;
    prot->fetchInfo = udcInfoViaFtp;
    }
else if (sameString(upToColon, "transparent"))
    {
    prot->fetchData = udcDataViaTransparent;
    prot->fetchInfo = udcInfoViaTransparent;
    }
else
    {
    errAbort("Unrecognized protocol %s in udcProtNew", upToColon);
    }
return prot;
}

static void udcProtocolFree(struct udcProtocol **pProt)
/* Free up protocol resources. */
{
freez(pProt);
}

static void setInitialCachedDataBounds(struct udcFile *file, boolean useCacheInfo)
/* Open up bitmap file and read a little bit of it to see if cache is stale,
 * and if not to see if the initial part is cached.  Sets the data members
 * startData, and endData.  If the case is stale it makes fresh empty
 * cacheDir/sparseData and cacheDir/bitmap files. */
{
bits32 version = 0;

/* Get existing bitmap, and if it's stale clean up. */
struct udcBitmap *bits = udcBitmapOpen(file->bitmapFileName);
if (bits != NULL)
    {
    if (useCacheInfo)
	{
	file->size = bits->fileSize;
	file->updateTime = bits->remoteUpdate;
	}
    version = bits->version;
    if (bits->remoteUpdate != file->updateTime || bits->fileSize != file->size ||
	!fileExists(file->sparseFileName))
	{
	verbose(2, "removing stale version (%lld! = %lld or %lld! = %lld or %s doesn't exist), "
		"new version %d\n",
		bits->remoteUpdate, (long long)file->updateTime, bits->fileSize, file->size,
		file->sparseFileName, version);
        udcBitmapClose(&bits);
	remove(file->bitmapFileName);
	remove(file->sparseFileName);
	++version;
	}
    }
else
    verbose(2, "bitmap file %s does not already exist, creating.\n", file->bitmapFileName);

/* If no bitmap, then create one, and also an empty sparse data file. */
if (bits == NULL)
    {
    udcNewCreateBitmapAndSparse(file, file->updateTime, file->size, version);
    bits = udcBitmapOpen(file->bitmapFileName);
    if (bits == NULL)
        errAbort("Unable to open bitmap file %s", file->bitmapFileName);
    }

file->bitmapVersion = bits->version;

/* Read in a little bit from bitmap while we have it open to see if we have anything cached. */
if (file->size > 0)
    {
    Bits b;
    off_t wasAt = lseek(bits->fd, 0, SEEK_CUR);
    mustReadOneFd(bits->fd, b);
    int endBlock = (file->size + udcBlockSize - 1)/udcBlockSize;
    if (endBlock > 8)
        endBlock = 8;
    int initialCachedBlocks = bitFindClear(&b, 0, endBlock);
    file->endData = initialCachedBlocks * udcBlockSize;
    mustLseek(bits->fd, wasAt, SEEK_SET);
    } 

file->bits = bits;

}

static boolean qEscaped(char c)
/* Returns TRUE if character needs to be escaped in q-encoding. */
{
if (isalnum(c))
    return c == 'Q';
else
    return c != '_' && c != '-' && c != '/' && c != '.';
}

static char *qEncode(char *input)
/* Do a simple encoding to convert input string into "normal" characters.
 * Abnormal letters, and '!' get converted into Q followed by two hexadecimal digits. */
{
/* First go through and figure out encoded size. */
int size = 0;
char *s, *d, c;
s = input;
while ((c = *s++) != 0)
    {
    if (qEscaped(c))
	size += 3;
    else
	size += 1;
    }

/* Allocate and fill in output. */
char *output = needMem(size+1);
s = input;
d = output;
while ((c = *s++) != 0)
    {
    if (qEscaped(c))
        {
	sprintf(d, "Q%02X", (unsigned)c);
	d += 3;
	}
    else
        *d++ = c;
    }
return output;
}

void udcParseUrlFull(char *url, char **retProtocol, char **retAfterProtocol, char **retColon,
		     char **retAuth)
/* Parse the URL into components that udc treats separately.
 * *retAfterProtocol is Q-encoded to keep special chars out of filenames.  
 * Free all *ret's except *retColon when done. */
{
char *protocol, *afterProtocol;
char *colon = strchr(url, ':');
if (!colon)
    {
    *retColon = NULL;
    return;
    }
int colonPos = colon - url;
protocol = cloneStringZ(url, colonPos);
afterProtocol = url + colonPos + 1;
while (afterProtocol[0] == '/')
   afterProtocol += 1;
char *userPwd = strchr(afterProtocol, '@');
if (userPwd)
    {
    if (retAuth)
	{
	char auth[1024];
	safencpy(auth, sizeof(auth), afterProtocol, userPwd+1-afterProtocol);
	*retAuth = qEncode(auth);
	}
    char *afterHost = strchr(afterProtocol, '/');
    if (!afterHost)
	{
	afterHost = afterProtocol+strlen(afterProtocol);
	}
    if (userPwd < afterHost)
	afterProtocol = userPwd + 1;
    }
else if (retAuth)
    *retAuth = NULL;
afterProtocol = qEncode(afterProtocol);
*retProtocol = protocol;
*retAfterProtocol = afterProtocol;
*retColon = colon;
}

void udcParseUrl(char *url, char **retProtocol, char **retAfterProtocol, char **retColon)
/* Parse the URL into components that udc treats separately.
 * *retAfterProtocol is Q-encoded to keep special chars out of filenames.  
 * Free  *retProtocol and *retAfterProtocol but not *retColon when done. */
{
udcParseUrlFull(url, retProtocol, retAfterProtocol, retColon, NULL);
}

void udcPathAndFileNames(struct udcFile *file, char *cacheDir, char *protocol, char *afterProtocol)
/* Initialize udcFile path and names */
{
int len = strlen(cacheDir) + 1 + strlen(protocol) + 1 + strlen(afterProtocol) + 1;
file->cacheDir = needMem(len);
safef(file->cacheDir, len, "%s/%s/%s", cacheDir, protocol, afterProtocol);

/* Create file names for bitmap and data portions. */
file->bitmapFileName = fileNameInCacheDir(file, bitmapName);
file->sparseFileName = fileNameInCacheDir(file, sparseDataName);
}

static long long int udcSizeAndModTimeFromBitmap(char *bitmapFileName, time_t *retTime)
/* Look up the file size from the local cache bitmap file, or -1 if there
 * is no cache for url. If retTime is non-null, store the remote update time in it. */
{
long long int ret = -1;
struct udcBitmap *bits = udcBitmapOpen(bitmapFileName);
if (bits != NULL)
    {
    ret = bits->fileSize;
    if (retTime)
	*retTime = bits->remoteUpdate;
    }
else
    warn("Can't open bitmap file %s: %s\n", bitmapFileName, strerror(errno));
udcBitmapClose(&bits);
return ret;
}

struct udcFile *udcFileMayOpen(char *url, char *cacheDir)
/* Open up a cached file. cacheDir may be null in which case udcDefaultDir() will be
 * used.  Return NULL if file doesn't exist. */
{
if (cacheDir == NULL)
    cacheDir = udcDefaultDir();
verbose(2, "udcfileOpen(%s, %s)\n", url, cacheDir);
/* Parse out protocol.  Make it "transparent" if none specified. */
char *protocol = NULL, *afterProtocol = NULL, *colon;
boolean isTransparent = FALSE;
udcParseUrl(url, &protocol, &afterProtocol, &colon);
if (!colon)
    {
    freeMem(protocol);
    protocol = cloneString("transparent");
    freeMem(afterProtocol);
    afterProtocol = cloneString(url);
    isTransparent = TRUE;
    }
struct udcProtocol *prot;
prot = udcProtocolNew(protocol);

/* Figure out if anything exists. */
boolean useCacheInfo = FALSE;
struct udcRemoteFileInfo info;
ZeroVar(&info);
if (!isTransparent)
    {
    useCacheInfo = (udcCacheAge(url, cacheDir) < udcCacheTimeout());
    if (!useCacheInfo)
	{
	if (!prot->fetchInfo(url, &info))
	    {
	    udcProtocolFree(&prot);
	    freeMem(protocol);
	    freeMem(afterProtocol);
	    return NULL;
	    }
	}
    }

/* Allocate file object and start filling it in. */
struct udcFile *file;
AllocVar(file);
file->url = cloneString(url);
file->protocol = protocol;
file->prot = prot;
if (isTransparent)
    {
    /* If transparent dummy up things so that the "sparse" file pointer is actually
     * the file itself, which appears to be completely loaded in cache. */
    int fd = file->fdSparse = mustOpenFd(url, O_RDONLY);
    struct stat status;
    fstat(fd, &status);
    file->startData = 0;
    file->endData = file->size = status.st_size;
    }
else
    {
    udcPathAndFileNames(file, cacheDir, protocol, afterProtocol);
    if (!useCacheInfo)
	{
	file->updateTime = info.updateTime;
	file->size = info.size;
	memcpy(&(file->connInfo), &(info.ci), sizeof(struct connInfo));
	// update cache file mod times, so if we're caching we won't do this again
	// until the timeout has expired again:
	if (udcCacheTimeout() > 0 && fileExists(file->bitmapFileName))
	    (void)maybeTouchFile(file->bitmapFileName);
	}

    /* Make directory. */
    makeDirsOnPath(file->cacheDir);

    /* Figure out a little bit about the extent of the good cached data if any. Open bits bitmap. */
    setInitialCachedDataBounds(file, useCacheInfo);

    file->fdSparse = mustOpenFd(file->sparseFileName, O_RDWR);

    }
freeMem(afterProtocol);
return file;
}

struct udcFile *udcFileOpen(char *url, char *cacheDir)
/* Open up a cached file.  cacheDir may be null in which case udcDefaultDir() will be
 * used.  Abort if if file doesn't exist. */
{
struct udcFile *udcFile = udcFileMayOpen(url, cacheDir);
if (udcFile == NULL)
    errAbort("Couldn't open %s", url);
return udcFile;
}


struct slName *udcFileCacheFiles(char *url, char *cacheDir)
/* Return low-level list of files used in cache. */
{
char *protocol, *afterProtocol, *colon;
struct udcFile *file;
udcParseUrl(url, &protocol, &afterProtocol, &colon);
if (colon == NULL)
    return NULL;
AllocVar(file);
udcPathAndFileNames(file, cacheDir, protocol, afterProtocol);
struct slName *list = NULL;
slAddHead(&list, slNameNew(file->bitmapFileName));
slAddHead(&list, slNameNew(file->sparseFileName));
slReverse(&list);
freeMem(file->cacheDir);
freeMem(file->bitmapFileName);
freeMem(file->sparseFileName);
freeMem(file);
freeMem(protocol);
freeMem(afterProtocol);
return list;
}

void udcFileClose(struct udcFile **pFile)
/* Close down cached file. */
{
struct udcFile *file = *pFile;
if (file != NULL)
    {
    if (file->connInfo.socket != 0)
	mustCloseFd(&(file->connInfo.socket));
    if (file->connInfo.ctrlSocket != 0)
	mustCloseFd(&(file->connInfo.ctrlSocket));
    freeMem(file->url);
    freeMem(file->protocol);
    udcProtocolFree(&file->prot);
    freeMem(file->cacheDir);
    freeMem(file->bitmapFileName);
    freeMem(file->sparseFileName);
    freeMem(file->sparseReadAheadBuf);
    mustCloseFd(&(file->fdSparse));
    udcBitmapClose(&file->bits);
    }
freez(pFile);
}

static void qDecode(const char *input, char *buf, size_t size)
/* Reverse the qEncode performed on afterProcotol above into buf or abort. */
{
safecpy(buf, size, input);
char c, *r = buf, *w = buf;
while ((c = *r++) != '\0')
    {
    if (c == 'Q')
	{
	int q;
	if (sscanf(r, "%02X", &q))
	    {
	    *w++ = (char)q;
	    r += 2;
	    }
	else
	    errAbort("qDecode: input \"%s\" does not appear to be properly formatted "
		     "starting at \"%s\"", input, r);
	}
    else
	*w++ = c;
    }
*w = '\0';
}

char *udcPathToUrl(const char *path, char *buf, size_t size, char *cacheDir)
/* Translate path into an URL, store in buf, return pointer to buf if successful
 * and NULL if not. */
{
if (cacheDir == NULL)
    cacheDir = udcDefaultDir();
int offset = 0;
if (startsWith(cacheDir, (char *)path))
    offset = strlen(cacheDir);
if (path[offset] == '/')
    offset++;
char protocol[16];
strncpy(protocol, path+offset, sizeof(protocol));
protocol[ArraySize(protocol)-1] = '\0';
char *p = strchr(protocol, '/');
if (p == NULL)
    {
    errAbort("unable to parse protocol (first non-'%s' directory) out of path '%s'\n",
	     cacheDir, path);
    return NULL;
    }
*p++ = '\0';
char afterProtocol[4096];
qDecode(path+1+strlen(protocol)+1, afterProtocol, sizeof(afterProtocol));
safef(buf, size, "%s://%s", protocol, afterProtocol);
return buf;
}

long long int udcSizeFromCache(char *url, char *cacheDir)
/* Look up the file size from the local cache bitmap file, or -1 if there
 * is no cache for url. */
{
long long int ret = -1;
if (cacheDir == NULL)
    cacheDir = udcDefaultDir();
struct slName *sl, *slList = udcFileCacheFiles(url, cacheDir);
for (sl = slList;  sl != NULL;  sl = sl->next)
    if (endsWith(sl->name, bitmapName))
	{
	ret = udcSizeAndModTimeFromBitmap(sl->name, NULL);
	break;
	}
slNameFreeList(&slList);
return ret;
}

unsigned long udcCacheAge(char *url, char *cacheDir)
/* Return the age in seconds of the oldest cache file.  If a cache file is
 * missing, return the current time (seconds since the epoch). */
{
unsigned long now = clock1(), oldestTime = now;
if (cacheDir == NULL)
    cacheDir = udcDefaultDir();
struct slName *sl, *slList = udcFileCacheFiles(url, cacheDir);
if (slList == NULL)
    return now;
for (sl = slList;  sl != NULL;  sl = sl->next)
    if (endsWith(sl->name, bitmapName))
	{
	if (fileExists(sl->name))
	    oldestTime = min(fileModTime(sl->name), oldestTime);
	else
	    return now;
	}
return (now - oldestTime);
}

static void readBitsIntoBuf(int fd, int headerSize, int bitStart, int bitEnd,
	Bits **retBits, int *retPartOffset)
/* Do some bit-to-byte offset conversions and read in all the bytes that
 * have information in the bits we're interested in. */
{
int byteStart = bitStart/8;
int byteEnd = bitToByteSize(bitEnd);
int byteSize = byteEnd - byteStart;
Bits *bits = needLargeMem(byteSize);
mustLseek(fd, headerSize + byteStart, SEEK_SET);
mustReadFd(fd, bits, byteSize);
*retBits = bits;
*retPartOffset = byteStart*8;
}

static boolean allBitsSetInFile(int bitStart, int bitEnd, int partOffset, Bits *bits)
/* Return TRUE if all bits in file between start and end are set. */
{
int partBitStart = bitStart - partOffset;
int partBitEnd = bitEnd - partOffset;
int nextClearBit = bitFindClear(bits, partBitStart, partBitEnd);
boolean allSet = (nextClearBit >= partBitEnd);
return allSet;
}

// For tests/udcTest.c debugging: not declared in udc.h, but not static either:
boolean udcCheckCacheBits(struct udcFile *file, int startBlock, int endBlock)
/* Warn and return TRUE if any bit in (startBlock,endBlock] is not set. */
{
boolean gotUnset = FALSE;
struct udcBitmap *bitmap = udcBitmapOpen(file->bitmapFileName);
int partOffset;
Bits *bits;
readBitsIntoBuf(bitmap->fd, udcBitmapHeaderSize, startBlock, endBlock, &bits, &partOffset);

int partBitStart = startBlock - partOffset;
int partBitEnd = endBlock - partOffset;
int nextClearBit = bitFindClear(bits, partBitStart, partBitEnd);
while (nextClearBit < partBitEnd)
    {
    int clearBlock = nextClearBit + partOffset;
    warn("... udcFile 0x%04lx: bit for block %d (%lld..%lld] is not set",
	 (unsigned long)file, clearBlock,
	 ((long long)clearBlock * udcBlockSize), (((long long)clearBlock+1) * udcBlockSize));
    gotUnset = TRUE;
    int nextSetBit = bitFindSet(bits, nextClearBit, partBitEnd);
    nextClearBit = bitFindClear(bits, nextSetBit, partBitEnd);
    }
return gotUnset;
}

static void fetchMissingBlocks(struct udcFile *file, struct udcBitmap *bits, 
	int startBlock, int blockCount, int blockSize)
/* Fetch missing blocks from remote and put them into file.  errAbort if trouble. */
{
bits64 startPos = (bits64)startBlock * blockSize;
bits64 endPos = startPos + (bits64)blockCount * blockSize;
if (endPos > file->size)
    endPos = file->size;
if (endPos > startPos)
    {
    bits64 readSize = endPos - startPos;
    void *buf = needLargeMem(readSize);
    
    int actualSize = file->prot->fetchData(file->url, startPos, readSize, buf, &(file->connInfo));
    if (actualSize != readSize)
	errAbort("unable to fetch %lld bytes from %s @%lld (got %d bytes)",
		 readSize, file->url, startPos, actualSize);
    mustLseek(file->fdSparse, startPos, SEEK_SET);
    mustWriteFd(file->fdSparse, buf, readSize);
    freez(&buf);
    }
}

static boolean fetchMissingBits(struct udcFile *file, struct udcBitmap *bits,
	bits64 start, bits64 end, bits64 *retFetchedStart, bits64 *retFetchedEnd)
/* Scan through relevant parts of bitmap, fetching blocks we don't already have. */
{
/* Fetch relevant part of bitmap into memory */
int partOffset;
Bits *b;
int startBlock = start / bits->blockSize;
int endBlock = (end + bits->blockSize - 1) / bits->blockSize;
readBitsIntoBuf(bits->fd, udcBitmapHeaderSize, startBlock, endBlock, &b, &partOffset);
if (allBitsSetInFile(startBlock, endBlock, partOffset, b))
    {  // it is already in the cache
    freeMem(b);
    return TRUE;
    }

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

    fetchMissingBlocks(file, bits, nextClearBit + partOffset, clearSize, bits->blockSize);
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
    int byteEnd = bitToByteSize(endBlock);
    int byteSize = byteEnd - byteStart;
    mustLseek(bits->fd, byteStart + udcBitmapHeaderSize, SEEK_SET);
    mustWriteFd(bits->fd, b, byteSize);
    }

freeMem(b);
*retFetchedStart = startBlock * bits->blockSize;
*retFetchedEnd = endBlock * bits->blockSize;
return FALSE;
}

static boolean rangeIntersectOrTouch64(bits64 start1, bits64 end1, bits64 start2, bits64 end2)
/* Return true if two 64-bit ranges intersect or touch. */
{  // cannot use the version of this function that is in common.c since it only handles integers.
bits64 s = max(start1,start2);
bits64 e = min(end1,end2);
return e >= s;
}


static void udcFetchMissing(struct udcFile *file, struct udcBitmap *bits, bits64 start, bits64 end)
/* Fetch missing pieces of data from file */
{
/* Call lower level routine fetch remote data that is not already here. */
bits64 fetchedStart, fetchedEnd;
if (fetchMissingBits(file, bits, start, end, &fetchedStart, &fetchedEnd))
    return;

/* Update file startData/endData members to include new data (and old as well if
 * the new data overlaps the old). */
if (rangeIntersectOrTouch64(file->startData, file->endData, fetchedStart, fetchedEnd))
    {
    if (fetchedStart > file->startData)
        fetchedStart = file->startData;
    if (fetchedEnd < file->endData)
        fetchedEnd = file->endData;
    }
file->startData = fetchedStart;
file->endData = fetchedEnd;
}

static boolean udcCachePreload(struct udcFile *file, bits64 offset, bits64 size)
/* Make sure that given data is in cache - fetching it remotely if need be. 
 * Return TRUE on success. */
{
boolean ok = TRUE;
/* We'll break this operation into blocks of a reasonable size to allow
 * other processes to get cache access, since we have to lock the cache files. */
bits64 s,e, endPos=offset+size;
for (s = offset; s < endPos; s = e)
    {
    /* Figure out bounds of this section. */
    e = s + udcMaxBytesPerRemoteFetch;
    if (e > endPos)
	e = endPos;

    struct udcBitmap *bits = file->bits;
    if (bits->version == file->bitmapVersion)
	{
        udcFetchMissing(file, bits, s, e);
	}
    else
	{
	ok = FALSE;
	verbose(2, "udcCachePreload version check failed %d vs %d", 
		bits->version, file->bitmapVersion);
	}
    if (!ok)
        break;
    }
return ok;
}

#define READAHEADBUFSIZE 4096
bits64 udcRead(struct udcFile *file, void *buf, bits64 size)
/* Read a block from file.  Return amount actually read. */
{

/* Figure out region of file we're going to read, and clip it against file size. */
bits64 start = file->offset;
if (start > file->size)
    return 0;
bits64 end = start + size;
if (end > file->size)
    end = file->size;
size = end - start;
char *cbuf = buf;

/* use read-ahead buffer if present */
bits64 bytesRead = 0;

bits64 raStart;
bits64 raEnd;
while(TRUE)
    {
    if (file->sparseReadAhead)
	{
	raStart = file->sparseRAOffset;
	raEnd = raStart+READAHEADBUFSIZE;
	if (start >= raStart && start < raEnd)
	    {
	    // copy bytes out of rabuf
	    bits64 endInBuf = min(raEnd, end);
	    bits64 sizeInBuf = endInBuf - start;
	    memcpy(cbuf, file->sparseReadAheadBuf + (start-raStart), sizeInBuf);
	    cbuf += sizeInBuf;
	    bytesRead += sizeInBuf;
	    start = raEnd;
	    size -= sizeInBuf;
	    file->offset += sizeInBuf;
	    if (size == 0)
		break;
	    }
	file->sparseReadAhead = FALSE;
	mustLseek(file->fdSparse, start, SEEK_SET);
	}

    bits64 saveEnd = end;
    if (size < READAHEADBUFSIZE)
	{
	file->sparseReadAhead = TRUE;
	if (!file->sparseReadAheadBuf)
	    file->sparseReadAheadBuf = needMem(READAHEADBUFSIZE);
	file->sparseRAOffset = start;
	size = READAHEADBUFSIZE;
	end = start + size;
	if (end > file->size)
	    {
	    end = file->size;
	    size = end - start;
	    }
	}


    /* If we're outside of the window of file we already know is good, then have to
     * consult cache on disk, and maybe even fetch data remotely! */
    if (start < file->startData || end > file->endData)
	{

	if (!udcCachePreload(file, start, size))
	    {
	    verbose(2, "udcCachePreload failed");
	    bytesRead = 0;
	    break;
	    }

	/* Currently only need fseek here.  Would be safer, but possibly
	 * slower to move fseek so it is always executed in front of read, in
	 * case other code is moving around file pointer. */

	mustLseek(file->fdSparse, start, SEEK_SET);
	}

    if (file->sparseReadAhead)
	{
	mustReadFd(file->fdSparse, file->sparseReadAheadBuf, size);
	end = saveEnd;
	size = end - start;
	}
    else
	{
	mustReadFd(file->fdSparse, cbuf, size);
	file->offset += size;
	bytesRead += size;
	break;
	}
    }

return bytesRead;
}

void udcMustRead(struct udcFile *file, void *buf, bits64 size)
/* Read a block from file.  Abort if any problem, including EOF before size is read. */
{
bits64 sizeRead = udcRead(file, buf, size);
if (sizeRead < size)
    errAbort("udc couldn't read %llu bytes from %s, did read %llu", size, file->url, sizeRead);
}

int udcGetChar(struct udcFile *file)
/* Get next character from file or die trying. */
{
UBYTE b;
udcMustRead(file, &b, 1);
return b;
}

bits64 udcReadBits64(struct udcFile *file, boolean isSwapped)
/* Read and optionally byte-swap 64 bit entity. */
{
bits64 val;
udcMustRead(file, &val, sizeof(val));
if (isSwapped)
    val = byteSwap64(val);
return val;
}

bits32 udcReadBits32(struct udcFile *file, boolean isSwapped)
/* Read and optionally byte-swap 32 bit entity. */
{
bits32 val;
udcMustRead(file, &val, sizeof(val));
if (isSwapped)
    val = byteSwap32(val);
return val;
}

bits16 udcReadBits16(struct udcFile *file, boolean isSwapped)
/* Read and optionally byte-swap 16 bit entity. */
{
bits16 val;
udcMustRead(file, &val, sizeof(val));
if (isSwapped)
    val = byteSwap16(val);
return val;
}

float udcReadFloat(struct udcFile *file, boolean isSwapped)
/* Read and optionally byte-swap floating point number. */
{
float val;
udcMustRead(file, &val, sizeof(val));
if (isSwapped)
    val = byteSwapFloat(val);
return val;
}

double udcReadDouble(struct udcFile *file, boolean isSwapped)
/* Read and optionally byte-swap double-precision floating point number. */
{
double val;
udcMustRead(file, &val, sizeof(val));
if (isSwapped)
    val = byteSwapDouble(val);
return val;
}

char *udcReadStringAndZero(struct udcFile *file)
/* Read in zero terminated string from file.  Do a freeMem of result when done. */
{
char shortBuf[2], *longBuf = NULL, *buf = shortBuf;
int i, bufSize = sizeof(shortBuf);
for (i=0; ; ++i)
    {
    /* See if need to expand buffer, which is initially on stack, but if it gets big goes into 
     * heap. */
    if (i >= bufSize)
        {
	int newBufSize = bufSize*2;
	char *newBuf = needLargeMem(newBufSize);
	memcpy(newBuf, buf, bufSize);
	freeMem(longBuf);
	buf = longBuf = newBuf;
	bufSize = newBufSize;
	}
    char c = udcGetChar(file);
    buf[i] = c;
    if (c == 0)
        break;
    }
char *retString = cloneString(buf);
freeMem(longBuf);
return retString;
}

char *udcFileReadAll(char *url, char *cacheDir, size_t maxSize, size_t *retSize)
/* Read a complete file via UDC. The cacheDir may be null in which case udcDefaultDir()
 * will be used.  If maxSize is non-zero, check size against maxSize
 * and abort if it's bigger.  Returns file data (with an extra terminal for the
 * common case where it's treated as a C string).  If retSize is non-NULL then
 * returns size of file in *retSize. Do a freeMem or freez of the returned buffer
 * when done. */
{
struct udcFile  *file = udcFileOpen(url, cacheDir);
size_t size = file->size;
if (maxSize != 0 && size > maxSize)
    errAbort("%s is %lld bytes, but maxSize to udcFileReadAll is %lld",
    	url, (long long)size, (long long)maxSize);
char *buf = needLargeMem(size+1);
udcMustRead(file, buf, size);
buf[size] = 0;	// add trailing zero for string processing
udcFileClose(&file);
if (retSize != NULL)
    *retSize = size;
return buf;
}

struct lineFile *udcWrapShortLineFile(char *url, char *cacheDir, size_t maxSize)
/* Read in entire short (up to maxSize) url into memory and wrap a line file around it.
 * The cacheDir may be null in which case udcDefaultDir() will be used.  If maxSize
 * is zero then a default value (currently 64 meg) will be used. */
{
if (maxSize == 0) maxSize = 64 * 1024 * 1024;
char *buf = udcFileReadAll(url, cacheDir, maxSize, NULL);
return lineFileOnString(url, TRUE, buf);
}

void udcSeek(struct udcFile *file, bits64 offset)
/* Seek to a particular position in file. */
{
file->offset = offset;
mustLseek(file->fdSparse, offset, SEEK_SET);
}

bits64 udcTell(struct udcFile *file)
/* Return current file position. */
{
return file->offset;
}

static long bitRealDataSize(char *fileName)
/* Return number of real bytes indicated by bitmaps */
{
struct udcBitmap *bits = udcBitmapOpen(fileName);
int blockSize = bits->blockSize;
long byteSize = 0;
int blockCount = (bits->fileSize + blockSize - 1)/blockSize;
if (blockCount > 0)
    {
    int bitmapSize = bitToByteSize(blockCount);
    Bits *b = needLargeMem(bitmapSize);
    mustReadFd(bits->fd, b, bitmapSize);
    int bitsSet = bitCountRange(b, 0, blockCount);
    byteSize = (long)bitsSet*blockSize;
    freez(&b);
    }
udcBitmapClose(&bits);
return byteSize;
}

static bits64 rCleanup(time_t deleteTime, boolean testOnly)
/* Delete any bitmap or sparseData files last accessed before deleteTime */
{
struct fileInfo *file, *fileList = listDirX(".", "*", FALSE);
bits64 results = 0;
for (file = fileList; file != NULL; file = file->next)
    {
    if (file->isDir)
        {
	setCurrentDir(file->name);
	bits64 oneResult = rCleanup(deleteTime, testOnly);
	setCurrentDir("..");
	if (oneResult > 0)
	    {
	    if (!testOnly)
		remove(file->name);
	    results += oneResult;
	    results += file->size;
	    }
	}
    else if (sameString(file->name, bitmapName))
        {
	if (file->size > udcBitmapHeaderSize) /* prevent failure on bitmap files of size 0 or less than header size */
	    verbose(2, "%ld (%ld) %s/%s\n", bitRealDataSize(file->name), (long)file->size, getCurrentDir(), file->name);
	if (file->lastAccess < deleteTime)
	    {
	    /* Remove all files when get bitmap, so that can ensure they are deleted in 
	     * right order. */
	    results += file->size;
	    if (!testOnly)
		{
		remove(bitmapName);
		remove(sparseDataName);
		}
	    }
	}
    else if (sameString(file->name, sparseDataName))
        {
	if (results > 0)
	    results += file->size;
	}
    }
return results;
}

bits64 udcCleanup(char *cacheDir, double maxDays, boolean testOnly)
/* Remove cached files older than maxDays old. If testOnly is set
 * no clean up is done, but the size of the files that would be
 * cleaned up is still. */

{
time_t maxSeconds = maxDays * 24 * 60 * 60;
char *curPath = cloneString(getCurrentDir());
setCurrentDir(cacheDir);
time_t deleteTime = time(NULL) - maxSeconds;
bits64 result = rCleanup(deleteTime, testOnly);
setCurrentDir(curPath);
return result;
}

static char *defaultDir = "/tmp/udcCache";

char *udcDefaultDir()
/* Get default directory for cache */
{
return defaultDir;
}

void udcSetDefaultDir(char *path)
/* Set default directory for cache */
{
defaultDir = path;
}


int udcCacheTimeout()
/* Get cache timeout (if local cache files are newer than this many seconds,
 * we won't ping the remote server to check the file size and update time). */
{
return cacheTimeout;
}

void udcSetCacheTimeout(int timeout)
/* Set cache timeout (if local cache files are newer than this many seconds,
 * we won't ping the remote server to check the file size and update time). */
{
cacheTimeout = timeout;
}

time_t udcUpdateTime(struct udcFile *udc)
/* return udc->updateTime */
{
if (sameString("transparent", udc->protocol))
    {
    struct stat status;
    int ret = stat(udc->url, &status);
    if (ret < 0)
	return 0;
    else
	return  status.st_mtime;
    }
return udc->updateTime;
}

#ifdef PROGRESS_METER
off_t remoteFileSize(char *url)
/* fetch remote file size from given URL */
{
off_t answer = 0;
struct udcRemoteFileInfo info;

if (startsWith("http://",url) || startsWith("https://",url))
    {
    if (udcInfoViaHttp(url, &info))
	answer = info.size;
    }
else if (startsWith("ftp://",url))
    {
    if (udcInfoViaFtp(url, &info))
	answer = info.size;
    }

return answer;
}
#endif
