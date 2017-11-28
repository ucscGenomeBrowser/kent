/* udcCacheSizesCheck -- test the udc cache files bitmap and sparseData for size consistency. 
    Processes given directory recursively, to easily handle udc cache directories with many files.
    Optionally repairs corrupt cache files by deleting them.
    Experimentally, it checks the sparse structure allocated by the OS and compares it to the udc bitmap. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include <sys/wait.h>
#include <sys/ioctl.h>
#include "common.h"
#include "errAbort.h"
#include "options.h"
#include "portable.h"
#include "udc.h"

#define BIGPATH 16384

long totalErrors = 0;
long totalOk = 0;
long totalFixed = 0;

void usage()
/* usage message */
{
    printf("Specify cache path on command-line. e.g. /tmp/udcCache/http/somepath.bb\n");
    printf("Operates recursively on any subdirectories.\n");
    printf("-fix causes it to repair invalid cache by deleting the files.\n");
    printf("  \n");
    printf("-fiemap causes it to check the actual sparse file structure via fiemap.\n");
    printf("  This check is experimental. It does not work on all filesystems or OSes.\n");
    printf("  This option makes the check run somewhat slower. It has some false-positives where it attributes\n");
    printf("  an extent that runs longer than the bitmap says due to lost updates (which can happen too) -- \n");
    printf("  but a common cause seems to be that the extent-allocation in the OS sometimes pre-allocates\n");
    printf("  a longer extent than was written to by the application.  The same problem of too-long extents\n");
    printf("  appears to also frustrate the copying process. If you try to make a copy of a file with its extents exactly the same,\n");
    printf("  whether using cp --sparse=always, or rsync --sparse, or even just manually copying the file using seeks\n");
    printf("  based on the source fiemap, it is plagued by too-long extents in the copy.\n");
    exit(1);
}

static struct optionSpec options[] = {
    {"fix",     OPTION_BOOLEAN},
    {"fiemap",  OPTION_BOOLEAN},
    {NULL, 0},
};

boolean fix = FALSE;
boolean fiemap = FALSE;

struct fiemap_extent {
        bits64 fe_logical;  /* logical offset in bytes for the start of
                            * the extent from the beginning of the file */
        bits64 fe_physical; /* physical offset in bytes for the start
                            * of the extent from the beginning of the disk */
        bits64 fe_length;   /* length in bytes for this extent */
        bits64 fe_reserved64[2];
        bits32 fe_flags;    /* FIEMAP_EXTENT_* flags for this extent */
        bits32 fe_reserved[3];
};

struct fiemap {
        bits64 fm_start;         /* logical offset (inclusive) at
                                 * which to start mapping (in) */
        bits64 fm_length;        /* logical length of mapping which
                                 * userspace wants (in) */
        bits32 fm_flags;         /* FIEMAP_FLAG_* flags for request (in/out) */
        bits32 fm_mapped_extents;/* number of extents that were mapped (out) */
        bits32 fm_extent_count;  /* size of fm_extents array (in) */
        bits32 fm_reserved;
        struct fiemap_extent fm_extents[0]; /* array of mapped extents (out) */
};

struct fie {
    struct fie *next;
    bits64 start;
    bits64 end;
};

#define FIEMAP_FLAG_SYNC        0x00000001 /* sync file data before map */
#define FIEMAP_EXTENT_LAST              0x00000001 /* Last extent in file. */
#define FS_IOC_FIEMAP			_IOWR('f', 11, struct fiemap)

void fetch_extents(struct fiemap *fiemap, int start_extent, int *is_last, struct fie **pList)
/* fetch each extent, accumulating in fie list. */
{
struct fie *e;
struct fie *list = *pList;
unsigned int i;

for (i = 0; i < fiemap->fm_mapped_extents; i++) 
    {
    AllocVar(e);
    e->start = fiemap->fm_extents[i].fe_logical;
    e->end   = e->start + fiemap->fm_extents[i].fe_length;
    slAddHead(&list, e);

    if (fiemap->fm_extents[i].fe_flags & FIEMAP_EXTENT_LAST) 
	{
	*is_last = 1;
	break;
	}
    }

if (fiemap->fm_mapped_extents < fiemap->fm_extent_count)
	*is_last = 1;
*pList = list;
}



struct fie *fetchFiemap(char *fname)
/* fetch fiemap */
{
uint count = 32;
int blocksize = 4096;
bits64 lstart = 0;
bits64 llength = ~0ULL;
bits32 flags = 0;
int rc;
int fd;
uint start_ext = 0;
int last = 0;	
int maxioctls = 512; /* max to try */
char *fiebuf;
struct fie *list = NULL;

fiebuf = malloc(sizeof (struct fiemap) + (count * sizeof(struct fiemap_extent)));
if (!fiebuf) 
    {
    perror("Could not allocate fiemap buffers");
    exit(1);
    }

struct fiemap *fiemap = (struct fiemap *)fiebuf;

flags |= FIEMAP_FLAG_SYNC;  // force delayed writes to sync to disk, updating map.

fiemap->fm_start = lstart;
fiemap->fm_length = llength;
fiemap->fm_flags = flags;
fiemap->fm_extent_count = count;
fiemap->fm_mapped_extents = 0;

fd = open(fname, O_RDONLY);
if (fd < 0) 
    {
    errAbort("Can't open %s", fname);
    }

//if (ioctl(fd, FIGETBSZ, &blocksize) < 0) 
//    {
//    errAbort("Can't get block size");
//    }

maxioctls = fileSize(fname) / blocksize; 

do 
    {
    rc = ioctl(fd, FS_IOC_FIEMAP, (unsigned long)fiemap);
    if (rc < 0) 
	{
	close(fd);
	errnoAbort("FIEMAP ioctl failed");
	}

    fetch_extents(fiemap, start_ext, &last, &list);

    if (!last) 
	{
	int last_map = fiemap->fm_mapped_extents - 1;
	bits64 foo;

	foo = fiemap->fm_extents[last_map].fe_logical +
		fiemap->fm_extents[last_map].fe_length;
	fiemap->fm_start = foo;
	fiemap->fm_length = lstart + llength - foo;
	fiemap->fm_extent_count = count;
	}

    start_ext += fiemap->fm_mapped_extents;

    /* stop */
    maxioctls--;

    } while (!last && maxioctls > 0);

close(fd);
slReverse(&list);
return list;
}

boolean checkFiemap(char *bitmapName, unsigned long bitmapFileSize, char *sparseDataName, unsigned long sparseDataSize)
/* check sparseData fiemap structure for compatibility with bitmap */
{
struct fie *list = fetchFiemap(sparseDataName);
struct fie *f;
if (verboseLevel()==5)
    {
    for(f=list;f;f=f->next)
	{
	printf("Extent: start %lu  end %lu  length %lu\n", (unsigned long)f->start, (unsigned long)f->end, (unsigned long)(f->end - f->start));
	}
    }

// note list could be null if sparseData size is really 0.
FILE *fp = fopen(bitmapName, "r");
if (!fp)
    {
    printf("Not able to open bitmapName : %s\n", bitmapName);
    return FALSE;
    }

// Set the pointer past the header
int headerSize = 64;  // bitmapheader size
int blockSize = 8 * 1024;  // 8k blocks
if (bitmapFileSize < headerSize)
    {
    printf("bitmap file size %ld should be >= headerSize %d\n", bitmapFileSize, headerSize);
    return FALSE;
    }
fseek(fp, headerSize, SEEK_SET);
unsigned long bytesRemaining = bitmapFileSize - headerSize;
unsigned long bitsRemaining = bytesRemaining * 8;
// TODO add a test for list==null when bitsRemaining = 0?

int c = 0;
unsigned long sparseStart = 0;
unsigned long sparseEnd = 0;
int sparseDataBlocksMissing = 0;  // serious error
int sparseDataBlocksLostUpdate = 0;  // inefficiency
f = list;
int bit = -1;
while (bitsRemaining)
    {
    if (bit == -1)
	{
    	c = fgetc(fp);
	if (verboseLevel()==5)
	    printf("c=%2x\n", c);
	bit = 7;
	}
    boolean bitOn = FALSE;
    if (c & 128)  // test bit 7
	bitOn = TRUE;

    sparseEnd = sparseStart + blockSize;

    boolean overlap = FALSE;
    // compare to fiemap list
    while (TRUE)
	{
	if (!f) break;
	overlap = (f->end > sparseStart) && (sparseEnd > f->start);
	if (overlap)
	    break;
	if (sparseStart < f->start) // fie is ahead and non overlapping.
	    break;
	f = f->next; // advance if fie is too small.
	}

    if (overlap)
	{
	if (bitOn)
	    {
	    // as expected. the block has been written in sparseData and marked as 1 in bitmap.
	    }
	else
	    {
	    // not as expected. the block has been written in sparseData but marked as 0 in bitmap.
	    // lost update inefficiency?
	    ++sparseDataBlocksLostUpdate;
	    if (verboseLevel()==4)
		printf("lost 8k block in sparseData at offset %lu\n", sparseStart);
	    }
	}
    else  // no overlap. fie is ahead or end of input
	{
	if (bitOn)
	    {
	    // not as expected. the bitmap says 1 but the block has not been written to sparseData. LOST DATA!
	    ++sparseDataBlocksMissing;
	    if (verboseLevel()==4)
		printf("missing 8k block in sparseData at offset %lu\n", sparseStart);
	    }
	else
	    {
	    // as expected. the block has been not been written in sparseData and is marked as 0 in bitmap.
	    }
	}

    if (verboseLevel()==6)
    printf("bitOn=%d overlap=%d sparseStart=%lu sparseEnd=%lu f->start=%lu f->end=%lu\n", 
	bitOn, overlap, sparseStart, sparseEnd, f ? (unsigned long)f->start:0, f ? (unsigned long)f->end:0);

    
    // advance to the next bit.
    c <<= 1;  // because we are little-endian
    --bitsRemaining;
    --bit;
    sparseStart += blockSize;
    }
fclose(fp);

// check for excess fie in f at end to report blocks lost from bitmap (inefficiency)

if (verboseLevel()>=4)
    {
    if (sparseDataBlocksLostUpdate > 0)
	{
	printf("%d sparse 8k data blocks are lost from bitmap due to inefficiencies.\n", sparseDataBlocksLostUpdate);
	}
    }

// serious error - data listed in bitmap does not exist in sparesData at all.
if (sparseDataBlocksMissing > 0)
    {
    printf("%d sparse 8k data blocks are missing.\n", sparseDataBlocksMissing);
    return FALSE;
    }

return TRUE;
}

int lowestBitSet(int c)
/* find the lowest bit set by shifting */
{
if (c == 0) 
    errAbort("unexpected error: called lowestBitSet(0), parameter cannot be 0."); 
int result = 0;
while(TRUE) 
    {
    if (c & 1)
	return result;
    c >>= 1;
    ++result;
    }
}

boolean unlinkFile(char *path)
/* unlink if possible */
{
boolean result = TRUE;
if (fileExists(path))
    {
    int e = unlink(path);
    if (e)
	{
	errnoWarn("unable to unlink %s", path);
	result = FALSE;
	}
    }
return result;
}


void fixUdcPath(char *mainPath)
/* remove mainPath/bitmap and mainPath/sparseData to cleanup damaged files */
{
boolean result = TRUE;
if (!fileExists(mainPath))
    {
    printf("directory %s does not exist.\n", mainPath);
    }

char bitmapName[BIGPATH];
safef(bitmapName, sizeof bitmapName, "%s/bitmap", mainPath);

char sparseDataName[BIGPATH];
safef(sparseDataName, sizeof sparseDataName, "%s/sparseData", mainPath);

char redirName[BIGPATH];
safef(redirName, sizeof redirName, "%s/redir", mainPath);

if (!unlinkFile(bitmapName))
    result = FALSE;

if (!unlinkFile(sparseDataName))
    result = FALSE;

if (!unlinkFile(redirName))
    result = FALSE;

if (result)
    ++totalFixed;

}



boolean checkUdcPath(char *mainPath)
/* Check mainPath/bitmap and mainPath/sparseData for completeness and consistency */
{
if (!fileExists(mainPath))
    {
    printf("directory %s does not exist.\n", mainPath);
    return FALSE;
    }

char bitmapName[BIGPATH];
safef(bitmapName, sizeof bitmapName, "%s/bitmap", mainPath);

char sparseDataName[BIGPATH];
safef(sparseDataName, sizeof sparseDataName, "%s/sparseData", mainPath);

boolean bitmapNameExists = fileExists(bitmapName);
boolean sparseDataNameExists = fileExists(sparseDataName);

if (!sparseDataNameExists && !bitmapNameExists)
    {
    // Nothing to do
    return TRUE;    
    }

printf("---------\n"); 

printf("mainPath=%s\n", mainPath);

if (verboseLevel() >= 2)
    printf("bitmapName=%s\n", bitmapName);
if (!bitmapNameExists)
    {
    printf("bitmapName %s does not exist.\n", bitmapName);
    return FALSE;
    }
long bitmapFileSize = fileSize(bitmapName);
if (verboseLevel() >= 2)
    printf("bitmapFileSize=%ld\n", bitmapFileSize);

if (verboseLevel() >= 2)
    printf("sparseDataName=%s\n", sparseDataName);
if (!sparseDataNameExists)
    {
    printf("sparseDataName %s does not exist\n", sparseDataName);
    return FALSE;
    }

long sparseDataFileSize = fileSize(sparseDataName);
if (verboseLevel() >= 2)
    printf("sparseDataFileSize=%ld\n", sparseDataFileSize);

if (fiemap)
    {
    if (!checkFiemap(bitmapName, bitmapFileSize, sparseDataName, sparseDataFileSize))
	return FALSE;
    }
    

FILE *fp = fopen(bitmapName, "r");
if (!fp)
    {
    printf("Not able to open bitmapName : %s\n", bitmapName);
    return FALSE;
    }

// Set the pointer to the end
int scanBack = 1;
int c = 0;
int headerSize = 64;  // bitmapheader size
int blockSize = 8 * 1024;  // 8k blocks
if (bitmapFileSize < headerSize)
    {
    printf("bitmap file size %ld should be >= headerSize %d\n", bitmapFileSize, headerSize);
    return FALSE;
    }

while (scanBack <= (bitmapFileSize - headerSize))
    {
    fseek(fp, -scanBack, SEEK_END);
    c = fgetc(fp);
    if (c != 0)
	{
	if (verboseLevel() >= 2)
	    printf("c=%2x scanBack=%d\n", c, scanBack);
	break;
	}
    ++scanBack;
    }
int leastBit=8;
if (c == 0)
    {
    if (sparseDataFileSize == 0)
	{
	printf("Special case zero-size file OK. bitmap has nothing set, and sparseDataFile is size 0.\n");
	scanBack = bitmapFileSize - headerSize;
	}
    else
	{
	printf("bitmap has nothing set, but sparseData fileSize > 0.\n");
	return FALSE;
	}
    }
else
    {
    leastBit=lowestBitSet(c);
    }
fclose(fp);

if (verboseLevel() >= 2)
    printf("scanBack=%d\n", scanBack);

if (verboseLevel() >= 2)
    printf("leastBit=%d\n", leastBit);

int fullBytes = bitmapFileSize - headerSize - scanBack;
if (verboseLevel() >= 2)
    printf("fullBytes=%d\n", fullBytes);

int fullBits = (fullBytes*8) + (8 - leastBit);
if (verboseLevel() >= 2)
    printf("fullBits=%d\n", fullBits);

long long int fullSize = fullBits * (long long int)blockSize;
if (verboseLevel() >= 2)
    printf("fullSize=%lld\n", fullSize);

if (fullSize >= sparseDataFileSize)
    {
    if ((fullSize - sparseDataFileSize) < blockSize)
	{
	printf("sizes match!\n");
	++totalOk;
	}
    else
	{
	// special case can probably add to udc.c to clear automatically
	if (sparseDataFileSize == 0)
	    printf("sparseData size is zero. too small.\n");  
	else
	    printf("sparseData size is too small.\n");
	return FALSE;
	}
    }
else
    {
    printf("bitmap size is too small.\n");
    return FALSE;
    }

return TRUE;
}

void recursiveCheckUdcPath()
/* Recursively scan udcCache dirs for correct cache files */
{
// char *thisPath = getCurrentDir();
char thisPath[BIGPATH];
if (getcwd( thisPath, sizeof(thisPath) ) == NULL)
    errnoAbort("getCurrentDir: can't get current directory");

boolean ok = checkUdcPath(thisPath);
if (!ok)
    {
    if (fix)
	{
	fixUdcPath(thisPath);  // just deletes the files.
	}
    ++totalErrors;
    }
struct fileInfo *file, *fileList = listDirX(".", "*", FALSE);
for (file = fileList; file != NULL; file = file->next)
    {
    if (file->isDir)
        {
	if (verboseLevel() >= 3)
	    printf("file->name=%s\n", file->name);
        setCurrentDir(file->name);
	recursiveCheckUdcPath();
        setCurrentDir("..");
        }
    }
}

int main(int argc, char *argv[])
/* Set up test params and run tests. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();

fix = optionExists("fix");
fiemap = optionExists("fiemap");

setCurrentDir(argv[1]);

recursiveCheckUdcPath();

printf("---------\n");

printf("\ntotal problems found: %ld\n", totalErrors);
printf("\ntotal OK found: %ld\n", totalOk);
if (totalFixed > 0)
    printf("\ntotal fixed: %ld\n", totalFixed);

return 0;
}
