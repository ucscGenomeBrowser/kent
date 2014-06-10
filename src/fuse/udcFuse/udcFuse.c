/* udcFuse - FUSE (Filesystem in USErspace) filesystem for lib/udc.c (Url Data Cache). */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifdef USE_FUSE
#include "common.h"
#include "portable.h"
#include "errCatch.h"
#include "udc.h"
#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 26
#endif
#include "fuse.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
"udcFuse - FUSE (Filesystem in USErspace) filesystem for lib/udc.c (Url Data Cache)\n"
"usage:\n"
"   udcFuse [options] emptyDirMountPoint [udcCacheDir]\n"
"options:\n"
"   -d: run in debug mode\n"
  );
}


// Important bits from http://sourceforge.net/apps/mediawiki/fuse/index.php?title=FuseInvariants:
// --------------------------------------------------------------------------
// * All requests are absolute, i.e. all paths begin with / and
//   include the complete path to a file or a directory. Symlinks,
//   . and .. are already resolved.
// * For every request you can get except for getattr(), read() and
//   write(), usually for every path argument (both source and
//   destination for link and rename, but only the source for
//   symlink), you will get a getattr() request just before the
//   callback.
//   For example, suppose I store file names of files in a filesystem
//   also into a database. To keep data in sync, I would like, for
//   each filesystem operation that succeeds, to check if the file
//   exists on the database. I just do this in the getattr() call,
//   since all other calls will be preceded by a getattr.

// * The arguments for every request are already verified as much as
//   possible. This means that, for example
//    * readdir() is only called with an existing directory name
//    ...
//    * read() and write() are only called if the file has been opened
//      with the correct flags
// --------------------------------------------------------------------------

// Since this is run by a kernel module and can't just bail when there
// is a problem.  Wrap errCatch (which has been made pthread-safe)
// around any calls to kent/src code.
#define ERR_CATCH_START() \
    { \
    struct errCatch *catch = errCatchNew(); \
    if (errCatchStart(catch)) \
	{

	// code that can errAbort goes between ERR_CATCH_START and ERR_CATCH_END,
	// calling ERR_CATCH_FREE if it does its own return statement:

#define ERR_CATCH_FREE() errCatchFree(&catch)
#define ERR_CATCH_END(msg) \
	} \
    errCatchEnd(catch); \
    if (catch->gotError) \
	{ \
	fprintf(stderr, "%s errCatch: %s", (msg), catch->message->string); \
	ERR_CATCH_FREE(); \
	return -1; \
	} \
    ERR_CATCH_FREE(); \
    }

static int checkForFile(const char *path, char *udcCachePath, struct stat *stbuf, int pid)
/* When a udc cache directory has "bitmap" and "sparseData" files, it 
 * corresponds to a file URL and a udcFile object.  Modify stbuf->st_mode 
 * to reflect a file not a directory. */
{
if (stbuf->st_mode | S_IFDIR)
    {
    DIR *dirHandle = opendir(udcCachePath);
    if (dirHandle != NULL)
	{
	// should we make sure that there are not also subdirectories??
	boolean gotBitmap = FALSE, gotSparse = FALSE;
	int isEmpty = TRUE;
	struct dirent *dirInfo;
	while ((dirInfo = readdir(dirHandle)) != NULL)
	    {
	    if (sameString(dirInfo->d_name, ".") || sameString(dirInfo->d_name, ".."))
		continue;
	    isEmpty = FALSE;
	    if (sameString(dirInfo->d_name, "bitmap"))
		gotBitmap = TRUE;
	    else if (sameString(dirInfo->d_name, "sparseData"))
		gotSparse = TRUE;
	    if (gotBitmap && gotSparse)
		break;
	    }
	// fuse gets confused when a cache path is an empty dir, and then suddenly morphs
	// into a regular file, as happens when old cache files are removed by the 
	// trash cleaner but then reappear due to a new udc access.  So if empty dir,
	// just tell fuse that it doesn't exist.
	if (isEmpty)
	    return -ENOENT;
	if (gotBitmap || gotSparse)
	    {
	    if (gotBitmap ^ gotSparse)
		fprintf(stderr, "...[%d] getattr: got one cache file but not the other - stale?\n",
			pid);
	    stbuf->st_mode &= ~(S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH);
	    stbuf->st_mode |= S_IFREG;
	    // Now we need to set the actual size in stbuf, otherwise fuse will think
	    // the size is 4096 or however many bytes have been cached so far, and will
	    // prevent callers from reading past that.  
	    char buf[4096];
	    char *url = NULL;
	    long long size = -1;
	    ERR_CATCH_START();
	    url = udcPathToUrl(path, buf, sizeof(buf), NULL);
	    size = udcSizeFromCache(url, NULL);
	    ERR_CATCH_END("udcPathToUrl or udcSizeFromCache");
	    if (size < 0)
		fprintf(stderr, "...[%d] getattr: failed to get udc cache size for %s", pid, url);
	    else
		stbuf->st_size = size;
	    }
	closedir(dirHandle);
	}
    else
	{
	fprintf(stderr, "...[%d] getattr: failed to opendir(%s)!: %s\n",
		pid, udcCachePath, strerror(errno));
	return -errno;
	}
    }
return 0;
}

#define HTTP_PATH_PREFIX "/http/"
#define QENCODED_AT_SIGN "Q40"

static int fusePathToUdcPath(const char *path, char *udcPath, size_t udcPathSize)
/* The udc cache path is almost always just udcDefaultDir() + fuse path,
 * except when the fuse path includes qEncoded http auth info -- necessary for 
 * reconstructing the URL, but not included in the udc cache path. 
 * Return -1 for problem, 0 for OK. */
{
char *httpHost = NULL;
if (startsWith(HTTP_PATH_PREFIX, path))
    httpHost = (char *)path + strlen(HTTP_PATH_PREFIX);
if (httpHost)
    {
    char *atSign = strstr(httpHost, QENCODED_AT_SIGN);
    char *nextSlash = strchr(httpHost, '/');
    if (atSign != NULL &&
	(nextSlash == NULL || atSign < nextSlash))
	{
	ERR_CATCH_START();
	safef(udcPath, udcPathSize, "%s" HTTP_PATH_PREFIX "%s",
	      udcDefaultDir(), atSign+strlen(QENCODED_AT_SIGN));
	ERR_CATCH_END("safef udcPath (skipping auth)");
	return 0;
	}
    }
ERR_CATCH_START();
safef(udcPath, udcPathSize, "%s%s", udcDefaultDir(), path);
ERR_CATCH_END("safef udcPath");
return 0;
}

static int udcfs_getattr(const char *path, struct stat *stbuf)
/* According to http://sourceforge.net/apps/mediawiki/fuse/index.php?title=FuseInvariants ,
 * getattr() is called to test existence before every other command except read, write and
 * getattr itself.  Give stat of corresponding udc cache file (but make it read-only). */
{
unsigned int pid = pthread_self();
char udcCachePath[4096];
if (fusePathToUdcPath(path, udcCachePath, sizeof(udcCachePath)) < 0)
    return -1;
int res = stat(udcCachePath, stbuf);
if (res != 0)
    {
    fprintf(stderr, "...[%d] getattr: stat(%s) failed (%d): %s\n", pid, udcCachePath, res, strerror(errno));
    return -errno;
    }
// Force read-only permissions:
stbuf->st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
int ret = checkForFile(path, udcCachePath, stbuf, pid);
fprintf(stderr, "...[%d] getattr %s finish %ld\n", pid, path, clock1000());
return ret;
}

static boolean isEmptyDir(char *udcCachePath, char *subdir)
/* Return TRUE if subdir of path is a directory with no children. */
{
boolean isDir = FALSE, isEmpty = TRUE;
char fullPath[4096];
ERR_CATCH_START();
safef(fullPath, sizeof(fullPath), "%s/%s", udcCachePath, subdir);
ERR_CATCH_END("safef fullPath");
DIR *dirHandle = opendir(fullPath);
if (dirHandle != NULL)
    {
    isDir = TRUE;
    struct dirent *dirInfo;
    while ((dirInfo = readdir(dirHandle)) != NULL)
	{
	if (sameString(dirInfo->d_name, ".") || sameString(dirInfo->d_name, ".."))
	    continue;
	isEmpty = FALSE;
	break;
	}
    closedir(dirHandle);
    }
return isDir && isEmpty;
}

static int udcfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
/* Read the corresponding udc cache directory. */
{
unsigned int pid = pthread_self();
char udcCachePath[4096];
if (fusePathToUdcPath(path, udcCachePath, sizeof(udcCachePath)) < 0)
    return -1;
DIR *dirHandle = opendir(udcCachePath);
if (dirHandle == NULL)
    {
    fprintf(stderr, "...[%d] readdir: opendir(%s) failed!: %s\n",
	    pid, udcCachePath, strerror(errno));
    return -errno;
    }
struct dirent *dirInfo;
while ((dirInfo = readdir(dirHandle)) != NULL)
    // getattr denies the existence of empty udcCache directories because they
    // might get bitmap and sparseData files and then we report them as regular
    // files not directories.  Filter out components here that are empty directories:
    if (sameString(".", dirInfo->d_name) || sameString("..", dirInfo->d_name) ||
	!isEmptyDir(udcCachePath, dirInfo->d_name))
	{
	if (filler(buf, dirInfo->d_name, NULL, 0))
	    break;
	}
int ret = closedir(dirHandle);
fprintf(stderr, "...[%d] readdir %s finish %ld\n", pid, path, clock1000());
return ret;
}

static int udcfs_open(const char *path, struct fuse_file_info *fi)
/* Call udcOpen() and stash the handle in fi->fh for use by later calls. */
{
if ((fi->flags & (O_RDONLY | O_WRONLY | O_RDWR)) != O_RDONLY)
    return -EACCES;
unsigned int pid = pthread_self();
fprintf(stderr, "...[%d] open(%s) start %ld\n", pid, path, clock1000());
struct udcFile *udcf = NULL;
ERR_CATCH_START();
char buf[4096];
char *url = udcPathToUrl(path, buf, sizeof(buf), NULL);
if (url != NULL)
    {
    if (udcCacheAge(url, NULL) < udcCacheTimeout())
	fi->keep_cache = 1;
    udcf = udcFileMayOpen(url, NULL);
    fprintf(stderr, "...[%d] open -> udcFileMayOpen(%s) -> 0x%llx\n", pid, url, (long long)udcf);
    }
else
    {
    fprintf(stderr, "...[%d] open: Unable to translate path %s to URL!\n", pid, path);
    ERR_CATCH_FREE();
    return -1;
    }
ERR_CATCH_END("udcPathToUrl, udcCacheAge or udcFileMayOpen");
if (udcf == NULL)
    {
    fprintf(stderr, "...[%d] open: Unable to open udcFile for %s!\n", pid, path);
    return -1;
    }
fi->fh = (uint64_t)udcf;
fprintf(stderr, "...[%d] open fh=0x%llx finish %ld\n", pid, (long long)(fi->fh), clock1000());
return 0;
}

static int udcfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
/* udcSeek to specified offset, udcRead size bytes into buf, return #bytes read. */
{
unsigned int pid = pthread_self();
fprintf(stderr, "...[%d] read(%s, size=%lld, offset=%lld, fh=0x%llx) start %ld\n",
	pid, path, (long long)size, (long long)offset, (long long)(fi->fh), clock1000());
struct udcFile *udcf = (struct udcFile *)(fi->fh);
if (udcf == NULL)
    {
    fprintf(stderr, "...[%d] read: fuse_file_info fh is NULL -- can't read.\n", pid);
    return -1;
    }
ERR_CATCH_START();
udcSeek(udcf, (bits64)offset);
size = udcRead(udcf, buf, size);
ERR_CATCH_END("udcSeek or udcRead");
fprintf(stderr, "...[%d] read %lld bytes finish %ld\n", pid, (long long)size, clock1000());
return size;
}

static int udcfs_release(const char *path, struct fuse_file_info *fi)
// Close the udcFile stored as fi->fh.
{
unsigned int pid = pthread_self();
fprintf(stderr, "...[%d] release %s (0x%llx) %ld\n", pid, path, (long long)(fi->fh), clock1000());
ERR_CATCH_START();
udcFileClose((struct udcFile **)&(fi->fh));
ERR_CATCH_END("udcFileClose");
return 0;
}

static struct fuse_operations udcfs_oper =
{
    .getattr	= udcfs_getattr,
    .readdir	= udcfs_readdir,
    .open	= udcfs_open,
    .read	= udcfs_read,
    .release	= udcfs_release,
};

void checkUdcCacheDir()
/* Make sure udcDefaultDir() is a readable directory. */
{
DIR *udcCacheHandle = opendir(udcDefaultDir());
if (udcCacheHandle == NULL)
    {
    fprintf(stderr, "Error: Can't open udc local cache directory '%s': %s\n",
	    udcDefaultDir(), strerror(errno));
    exit(1);
    }
closedir(udcCacheHandle);
}

int main(int argc, char *argv[])
/* udcFuse - FUSE (Filesystem in USErspace) filesystem for lib/udc.c (Url Data Cache). */
{
int minArgc = 2;
int i;
for (i = 1; i < argc; i++)
    {
    if (argv[i][0] == '-')
	minArgc++;
    }
if (argc < minArgc || argc > minArgc+1)
    usage();
if (argc == minArgc+1)
    {
    udcSetDefaultDir(argv[argc-1]);
    // Fuse does not like getting an extra arg.
    argc--;
    }

// Use kernel caching, and tell udc not to ping server, if cache files are 
// less than an hour old.  (Should make this a command-line opt.)
udcSetCacheTimeout(3600);

#ifndef UDC_TEST

return fuse_main(argc, argv, &udcfs_oper, NULL);

#else
// TEST MAIN -- don't call fuse, just call methods the way we imagine 
// fuse would call them.

#define TESTFILLER_BUFSIZE 256
int testFiller(void *buf, const char *name, const struct stat *stbuf, off_t off)
// Impersonate fuse's readdir callback (type fuse_fill_dir_t)
{
printf("  -> testFiller(%s)\n", name);
return 0;
}

#define checkRet(ret) \
{ \
if (ret < 0) \
    { \
    printf("Doh!: %s\n", strerror(-ret)); \
    exit(1); \
    } \
}

#define UDC_TEST_PATH "/ftp/ftp-trace.ncbi.nih.gov/1000genomes/ftp/pilot_data/data/NA12878/alignment/NA12878.chrom22.SLX.maq.SRP000032.2009_07.bam"
#define UDC_TEST_PATH2 "/ftp/ftp-trace.ncbi.nih.gov/1000genomes/ftp/pilot_data/data/NA12878/alignment/NA12878.chrom21.SLX.maq.SRP000032.2009_07.bam"
udcfs_oper.getattr = udcfs_oper.getattr; // avoid unused-var warning.
struct fuse_file_info fi;
memset(&fi, 0, sizeof(fi));
struct stat stbuf;
char buf[TESTFILLER_BUFSIZE];
int ret;
ret = udcfs_getattr(UDC_TEST_PATH, &stbuf);
printf("Got %d from getattr; stbuf.st_mode=0%llo\n\n", ret, (long long)stbuf.st_mode);
checkRet(ret);

ret = udcfs_readdir("/", buf, testFiller, 0, &fi);
printf("Got %d from readdir\n\n", ret);
checkRet(ret);

ret = udcfs_readdir("/ftp", buf, testFiller, 0, &fi);
printf("Got %d from readdir\n\n", ret);
checkRet(ret);

ret = udcfs_open(UDC_TEST_PATH, &fi);
printf("Got %d from open -> udc handle 0x%llx\n\n", ret, (long long)(fi.fh));
checkRet(ret);

ret = udcfs_read(UDC_TEST_PATH, buf, 4, 0, &fi);
printf("Got %d bytes: 0x%x from read @0 on 0x%llx!\n\n", ret, *(unsigned int *)buf, (long long)(fi.fh));
checkRet(ret);

// Make sure we can have two open handles on the same file at the same time:
struct fuse_file_info fi2;
memset(&fi2, 0, sizeof(fi2));
ret = udcfs_open(UDC_TEST_PATH2, &fi2);
printf("Got %d from open -> second udc handle 0x%llx\n\n", ret, (long long)(fi2.fh));
checkRet(ret);

ret = udcfs_read(UDC_TEST_PATH2, buf, 4, 8, &fi2);
printf("Got %d bytes: 0x%x from read @8 on second handle 0x%llx!\n\n", ret, *(unsigned int *)buf, (long long)(fi2.fh));
checkRet(ret);

ret = udcfs_read(UDC_TEST_PATH2, buf, 4, 8, &fi);
printf("Got %d bytes: 0x%x from read @8 on first handle 0x%llx!\n\n", ret, *(unsigned int *)buf, (long long)(fi.fh));
checkRet(ret);

ret = udcfs_read(UDC_TEST_PATH, buf, 8, 9000, &fi2);
printf("Got %d bytes: 0x%llx from read @9000 on second handle 0x%llx!\n\n", ret, *(unsigned long long *)buf, (long long)(fi2.fh));
checkRet(ret);

ret = udcfs_release(UDC_TEST_PATH2, &fi2);
printf("Got %d from release of second handle; now fi2.fh is 0x%llx\n\n", ret, (long long)(fi2.fh));
checkRet(ret);

ret = udcfs_read(UDC_TEST_PATH, buf, 8, 9000, &fi);
printf("Got %d bytes: 0x%llx from read @9000 on 0x%llx!\n\n", ret, *(unsigned long long *)buf, (long long)(fi.fh));
checkRet(ret);

ret = udcfs_release(UDC_TEST_PATH, &fi);
printf("Got %d from release; now fi.fh is 0x%llx\n\n", ret, (long long)(fi.fh));
checkRet(ret);

// Now try to getattr something that has not (at the moment anyway) yet been opened in udc first:
#define UDC_TEST_PATH3 "/ftp/ftp-trace.ncbi.nih.gov/1000genomes/ftp/pilot_data/data/NA12878/alignment/NA12878.chrom9.SLX.maq.SRP000032.2009_07.bam"
memset(&stbuf, 0, sizeof(stbuf));
ret = udcfs_getattr(UDC_TEST_PATH3, &stbuf);
printf("Got %d from getattr; stbuf.st_mode=0%llo\n\n", ret, (long long)stbuf.st_mode);
checkRet(ret);

return 0;
#endif//def UDC_TEST
}

#else // no USE_FUSE
#include <stdio.h>
int main(int argc, char *argv[])
{
printf("udcFuse requires the FUSE (filesystem in userspace) library -- make sure that is installed and add USE_FUSE=1 to your enviroment.\n");
return 0;
}

#endif//def USE_FUSE
