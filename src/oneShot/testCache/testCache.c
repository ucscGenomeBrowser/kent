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
#include "sqlNum.h"
#include "obscure.h"
#include "portable.h"


static char const rcsid[] = "$Id: testCache.c,v 1.1 2009/02/05 20:02:41 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testCache - Experiments with file cacher.\n"
  "usage:\n"
  "   testCache cacheDir sourceUrl offset size outFile\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

typedef int (*UdcDataCallback)(char *url, bits64 offset, int size, void *buffer);
typedef boolean (*UdcTimestampCallback)(char *url, time_t *retTime);

struct udcProtocol
/* Something to handle a communications protocol like http, https, ftp, local file i/o, etc. */
    {
    struct udcProtocol *next;			/* Next in list */
    UdcDataCallback fetchData;			/* Data fetcher */
    UdcTimestampCallback fetchTimestamp;	/* Timestamp fetcher */
    };

struct udcFile
/* A file handle for our caching system. */
    {
    struct udcFile *next;	/* Next in list. */
    char *url;			/* Name of file - includes protocol */
    char *protocol;		/* The URL up to the first colon.  http: etc. */
    struct udcProtocol *prot;	/* Protocol specific data and methods. */
    time_t lastModified;	/* Last modified timestamp. */
    bits64 offset;		/* Current offset in file. */
    char *cacheDir;		/* Directory for cached file parts. */
    FILE *f;			/* File handle for file with current block. */
    bits64 startData;		/* Start of area in file we know to have data. */
    bits64 endData;		/* End of area in file we know to have data. */
    };

static char *udcRootDir = "/Users/kent/src/oneShot/testCache/cache";
static char *bitmapName = "bitmap";
static char *sparseDataName = "sparseData";

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

boolean udcTimeViaLocal(char *url, time_t *retTime)
/* Fill in *retTime with last modified time for file specified in url.
 * Return FALSE if file does not even exist. */
{
struct stat status;
int ret = stat(url, &status);
if (ret < 0)
    return FALSE;
*retTime = status.st_mtime;
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
    prot->fetchTimestamp = udcTimeViaLocal;
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
time_t lastModified;
if (!prot->fetchTimestamp(url, &lastModified))
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
file->lastModified = lastModified;
int len = strlen(udcRootDir) + 1 + strlen(protocol) + 1 + strlen(afterProtocol) + 1;
file->cacheDir = needMem(len);
safef(file->cacheDir, len, "%s/%s/%s", udcRootDir, protocol, afterProtocol);

/* Make directory. */
uglyf("file->cacheDir=%s\n", file->cacheDir);
makeDirsOnPath(file->cacheDir);


return file;
}

void udcFileClose(struct udcFile **pFile)
/* Close down cached file. */
{
}

void udcRead(struct udcFile *file, void *buf, int size)
/* Read a block from file. */
{
}

void testCache(char *sourceUrl, bits64 offset, bits64 size, char *outFile)
/* testCache - Experiments with file cacher.. */
{
/* Open up cache and file. */
struct udcFile *file = udcFileOpen(sourceUrl);

/* Read data and write it back out */
void *buf = needMem(size);
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
