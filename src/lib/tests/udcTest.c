/* udcTest -- test the URL data cache */

// suggestions from Mark: 1. try setvbuf, to make FILE * unbuffered -- does that help?
//                        2. *if* need to do own buffering, consider mmap()
//                           (kernel handles buffering)

#include <sys/wait.h>
#include "common.h"
#include "errabort.h"
#include "options.h"
#include "portable.h"
#include "udc.h"

static char const rcsid[] = "$Id: udcTest.c,v 1.2 2009/12/19 01:06:27 angie Exp $";

static struct optionSpec options[] = {
    {"raBuf",    OPTION_BOOLEAN},
    {"fork",     OPTION_BOOLEAN},
    {"protocol", OPTION_STRING},
    {"seed",     OPTION_INT},
    {NULL, 0},
};

boolean raBuf = FALSE;   /* exercise the read-ahead buffer */
boolean doFork = FALSE;
char *protocol = "ftp";
unsigned int seed = 0;

// Local copy (reference file) and URL for testing:
#define THOUSAND_HIVE "/hive/data/outside/1000genomes/ncbi/ftp-trace.ncbi.nih.gov/1000genomes/"
#define THOUSAND_FTP "ftp://ftp-trace.ncbi.nih.gov/1000genomes/ftp/pilot_data/data/"
#define CHR3_SLX_BAM "NA12878/alignment/NA12878.chrom3.SLX.maq.SRP000032.2009_07.bam"
#define CHR4_SLX_BAM "NA12878/alignment/NA12878.chrom4.SLX.maq.SRP000032.2009_07.bam"

// Use typical size range of bgzip-compressed data blocks:
#define MIN_BLK_SIZE 20000
#define MAX_BLK_SIZE 30000

// Read at most this many consecutive blocks:
#define MAX_BLOCKS 100

void openSeekRead(char *filename, bits64 offset, bits64 len, char *buf)
/* Read len bits starting at offset from filename into buf or die. */
{
int fd = mustOpenFd(filename, O_RDONLY);
mustLseek(fd, offset, SEEK_SET);
mustReadFd(fd, buf, len);
mustCloseFd(&fd);
}

boolean compareBytes(char *bufTest, char *bufRef, bits64 len, char *testName, char *refName,
		     char *testDesc, bits64 offset)
/* Report any differences between bufTest and bufRef (don't errAbort). */
{
boolean gotError = FALSE;
bits64 i, difCount = 0, nonZeroDifCount = 0;
for (i=0;  i < len;  i++)
    {
    if (bufTest[i] != bufRef[i])
	{
	if (difCount == 0)
	    warn("*** %s%s and ref first differ at offset %lld + %lld = %lld [0x%02x != 0x%02x]",
		 (bufTest[i] != '\0' ? "NONZERO " : ""),
		 testDesc, offset, i, offset+i, (bits8)bufTest[i], (bits8)bufRef[i]);
	if (bufTest[i] != '\0')
	    nonZeroDifCount++;
	difCount++;
	}
    }
if (difCount == 0)
    verbose(3, "Success: %s = ref, %lld bytes @%lld\n", testDesc, len, offset);
else
    {
    warn("--> %lld different bytes (%lld %s) total in block of %lld bytes starting at %lld",
	 difCount, nonZeroDifCount, (nonZeroDifCount ? "NONZERO" : "nonzero"), len, offset);
    gotError = TRUE;
    }
return gotError;
}

char *getSparseFileName(char *url)
/* Return the path to sparseData cache file for url. */
{
struct slName *sl, *cacheFiles = udcFileCacheFiles(url, udcDefaultDir());
char *sparseFileName = NULL;
for (sl = cacheFiles; sl != NULL; sl = sl->next)
    if (endsWith(sl->name, "sparseData"))
	sparseFileName = sl->name;
if (sparseFileName == NULL)
    errAbort("can't find sparseData file in udcFileCacheFiles(%s) results", url);
return sparseFileName;
}

boolean readAndTest(struct udcFile *udcf, bits64 offset, bits64 len, char *localCopy, char *url)
/* Read len bytes starting at offset in udcf.  Compare both the bytes returned,
 * and bytes directly read from the sparseData file, with data from the same 
 * location in our local copy of the URL data. */
{
boolean gotError = FALSE;
static char *bufRef = NULL, *bufTest = NULL;
static bits64 bufSize = 0;
if (bufRef != NULL && bufSize < len)
    {
    freez(&bufRef);
    freez(&bufTest);
    }
if (bufRef == NULL)
    {
    bufSize = ((len >> 10) + 1) << 10;  // round up to next kB
    bufRef = needMem(bufSize);
    bufTest = needMem(bufSize);
    }

// Check offset + len against size of reference file:
bits64 size = fileSize(localCopy);
if (offset > size)
    errAbort("readAndTest: Size of %s is %lld, but offset given is %lld", localCopy, size, offset);
if (offset + len > size)
    {
    bits64 newSize = size - offset;
    warn("readAndTest: Size of %s is %lld, offset %lld + len %lld = %lld exceeds that; "
	 "reducing len to %lld", localCopy, size, offset, len, offset+len, newSize);
    len = newSize;
    }
verbose(2, "0x%08llx: %lldB @%lld\n", (bits64)udcf, len, offset);

// Get data from the reference file:
openSeekRead(localCopy, offset, len, bufRef);

// Get data from udcFile object and compare to reference:
udcSeek(udcf, offset);
bits64 bytesRead = udcRead(udcf, bufTest, len);

// udcRead does a mustRead, and we have checked offset+len, so this should never happen,
// but test anyway:
if (bytesRead < len)
    errAbort("Got %lld bytes instead of %lld from %s @%lld", bytesRead, len, url, offset);
gotError |= compareBytes(bufTest, bufRef, len, url, localCopy, "url", offset);

if (0) // -- Check sparseData after the dust settles.
    {
    // Get data from udcf's sparse data file and compare to reference:
    char *sparseFileName = getSparseFileName(url);
    openSeekRead(sparseFileName, offset, len, bufTest);
    gotError |= compareBytes(bufTest, bufRef, len, sparseFileName, localCopy, "sparse", offset);
    }
return gotError;
}

INLINE double myDrand()
/* Return something from [0.0,1.0). */
{
return (double)(rand() / (RAND_MAX + 1.0));
}

INLINE bits64 randomStartOffset(bits64 max)
/* Return something from [0,max-MAX_BLOCKS*MAX_BLK_SIZE) */
{
bits64 maxStart = max - (MAX_BLOCKS * MAX_BLK_SIZE);
return (bits64)(maxStart * myDrand());
}

INLINE bits64 randomBlockSize()
/* Return something from [MIN_BLK_SIZE,MAX_BLK_SIZE) */
{
return MIN_BLK_SIZE + (bits64)((MAX_BLK_SIZE - MIN_BLK_SIZE) * myDrand());
}

boolean readAndTestBlocks(struct udcFile *udcf, bits64 *retOffset, int numBlks, int *retBlksRead,
			 char *localCopy, char *url)
/* Read numBlks randomly sized blocks from udcf starting at *retOffset and compare
 * to localCopy.  *retBlksRead starts as the number of randomly sized blocks we have
 * already read, and is incremented by the number of blocks we read. 
 * If numBlks is 0, use the number of remaining blocks before *retBlksRead reaches
 * MAX_BLOCKS.  Update *retOffset to the final offset. Return TRUE if errors. */
{
boolean gotError = FALSE;
if (*retBlksRead >= MAX_BLOCKS)
    errAbort("readAndTestBlocks: exceeded MAX_BLOCKS (%d) -- increase it?", MAX_BLOCKS);
int blksLeft = MAX_BLOCKS - *retBlksRead;
if (numBlks == 0)
    numBlks = blksLeft;
if (numBlks > blksLeft)
    {
    warn("readAndTestBlocks: %d blocks requested but we have already read %d -- "
	 "increase MAX_BLOCKS (%d)?  truncating to %d",
	 numBlks, *retBlksRead, MAX_BLOCKS, blksLeft);
    numBlks = blksLeft;
    }
int i;
for (i = 0;  i < numBlks;  i++)
    {
    int size = randomBlockSize();
    gotError |= readAndTest(udcf, *retOffset, size, localCopy, url);
    *retOffset += size;
    (*retBlksRead)++;
    }
return gotError;
}

// These are defined in udc.c but not udc.h:
#define udcBlockSize (8*1024)
boolean udcCheckCacheBits(struct udcFile *file, int startBlock, int endBlock);
/* Warn and return TRUE (error) if any bit in (startBlock,endBlock] is not set. */

boolean checkCacheFiles(bits64 accessStart, bits64 accessEnd, char *url, char *localCopy)
/* Given a range of byte offsets accessed via udc, translate those into udc block
 * coords.  Check that all bytes in the sparseData offsets corresponding to the 
 * block coords are equal to the reference (very important!) and that all blocks'
 * bits are set in bitmap (not as important). */
{
boolean gotError = FALSE;
char *bufRef = needMem(udcBlockSize), *bufSparse = needMem(udcBlockSize);
int startBlock = (int)(accessStart / udcBlockSize);
int endBlock = (int)((accessEnd + udcBlockSize-1) / udcBlockSize);
bits64 startOffset = (bits64)startBlock * udcBlockSize;
verbose(1, "checking sparseData (%lld..%lld] blocks (%d..%d].\n",
	startOffset, ((bits64)endBlock*udcBlockSize), startBlock, endBlock);
int fdLocal = mustOpenFd(localCopy, O_RDONLY);
mustLseek(fdLocal, startOffset, SEEK_SET);
char *sparseFileName = getSparseFileName(url);
int fdSparse = mustOpenFd(sparseFileName, O_RDONLY);
mustLseek(fdSparse, startOffset, SEEK_SET);
int i;
for (i = startBlock;  i < endBlock;  i++)
    {
    bits64 offset = ((bits64)i * udcBlockSize);
    memset(bufRef, 0x59, udcBlockSize);
    memset(bufSparse, 0xae, udcBlockSize);
    mustReadFd(fdLocal, bufRef, udcBlockSize);
    mustReadFd(fdSparse, bufSparse, udcBlockSize);
    char testDesc[64];
    safef(testDesc, sizeof(testDesc), "SPARSE %lld blk %d", offset, i);
    gotError |= compareBytes(bufSparse, bufRef, udcBlockSize, sparseFileName, localCopy,
			     testDesc, offset);
    }
mustCloseFd(&fdLocal);
mustCloseFd(&fdSparse);
// Check bitmap bits too:
struct udcFile *udcf = udcFileOpen(url, udcDefaultDir());
verbose(1, "checking bitmap bits (%d..%d].\n", startBlock, endBlock);
udcCheckCacheBits(udcf, startBlock, endBlock);
udcFileClose(&udcf);
return gotError;
}

boolean testReadAheadBufferMode(char *url, char *localCopy, int mode)
/* Open a udcFile, read different random locations, and check for errors. */
{
boolean gotError = FALSE;
bits64 fSize = fileSize(localCopy);

struct udcFile *udcf = udcFileOpen(url, udcDefaultDir());
bits64 offset = 0;
if (mode == -1)
   offset = 0 + 8192 * myDrand();
if (mode == 0)
   offset = (bits64)(fSize * myDrand());
if (mode == 1)
   offset = fSize - 8192 * myDrand();


int delta = 0;
int i;
for(i=0; i<100; ++i)
    {

    int size = 8192 * myDrand();

    if ((offset + size) > fSize)
	size = fSize - offset;

    gotError |= readAndTest(udcf, offset, size, localCopy, url);

    delta = -6000 + (12000 * myDrand());   // -6000 to +6000

    if (delta < 0)  // do not let unsigned offset go below 0
	if (-delta > offset)
    	    delta = -offset;  

    offset += delta;

    if (offset > fSize)
	offset = fSize;

    }

udcFileClose(&udcf);
return gotError;

}
boolean testReadAheadBuffer(char *url, char *localCopy)
/* Open a udcFile, read different random locations, and check for errors. */
{
boolean gotError = FALSE;
gotError |= testReadAheadBufferMode(url, localCopy, -1);  // near beginning of file
gotError |= testReadAheadBufferMode(url, localCopy, 0);   // anywherer in file
gotError |= testReadAheadBufferMode(url, localCopy, 1);   // near end of file
return gotError;
}


boolean testInterleaved(char *url, char *localCopy)
/* Open two udcFile handles to the same file, read probably-different random locations,
 * read from probably-overlapping random locations, and check for errors. */
{
boolean gotError = FALSE;
bits64 size = fileSize(localCopy);


// First, read some bytes from udcFile udcf1.
struct udcFile *udcf1 = udcFileOpen(url, udcDefaultDir());
int blksRead1 = 0;
bits64 offset1 = randomStartOffset(size);

gotError |= readAndTestBlocks(udcf1, &offset1, 2, &blksRead1, localCopy, url);

// While keeping udcf1 open, create udcf2 on the same URL, and read from a 
// (probably) different location:
struct udcFile *udcf2 = udcFileOpen(url, udcDefaultDir());
int blksRead2 = 0;
bits64 offset2 = randomStartOffset(size);

gotError |= readAndTestBlocks(udcf2, &offset2, 2, &blksRead2, localCopy, url);
// Interleave some successive-location reads:
int i;
for (i = 0;  i < 10;  i++)
    {
    gotError |= readAndTestBlocks(udcf1, &offset1, 1, &blksRead1, localCopy, url);
    gotError |= readAndTestBlocks(udcf2, &offset2, 1, &blksRead2, localCopy, url);
    }

// Unevenly interleave reads starting from the same new random location:
bits64 sameOffset = randomStartOffset(size);
blksRead1 = 0;
offset1 = sameOffset;
blksRead2 = 0;
offset2 = sameOffset;
gotError |= readAndTestBlocks(udcf1, &offset1, 1, &blksRead1, localCopy, url);
gotError |= readAndTestBlocks(udcf2, &offset2, 3, &blksRead2, localCopy, url);
while (blksRead1 < MAX_BLOCKS || blksRead2 < MAX_BLOCKS)
    {
    if (blksRead1 < MAX_BLOCKS)
	{
	int n = 1 + (int)(5 * myDrand());
	n = min(MAX_BLOCKS - blksRead1, n);
	gotError |= readAndTestBlocks(udcf1, &offset1, n, &blksRead1, localCopy, url);
	}
    if (blksRead2 < MAX_BLOCKS)
	{
	int n = 1 + (int)(5 * myDrand());
	n = min(MAX_BLOCKS - blksRead2, n);
	gotError |= readAndTestBlocks(udcf2, &offset2, n, &blksRead2, localCopy, url);
	}
    }
udcFileClose(&udcf1);
udcFileClose(&udcf2);
verbose(1,"checkCacheFiles\n");
gotError |= checkCacheFiles(sameOffset, max(offset1, offset2), url, localCopy);
return gotError;
}

boolean testConcurrent(char *url, char *localCopy)
/* Fork; then parent and child access the same locations (hopefully) concurrently. */
{
boolean gotErrorParent = FALSE, gotErrorChild = FALSE;
bits64 size = fileSize(localCopy);
bits64 sameOffset = randomStartOffset(size);
bits64 offsetParent = sameOffset, offsetChild = sameOffset;

pid_t kidPid = fork();
if (kidPid < 0)
    errnoAbort("testConcurrent: fork failed");
else if (kidPid == 0)
    {
    // child: access url and then exit, to pass control back to parent.
    struct udcFile *udcf = udcFileOpen(url, udcDefaultDir());
    int blksRead = 0;
    gotErrorChild = readAndTestBlocks(udcf, &offsetParent, MAX_BLOCKS, &blksRead, localCopy, url);
    udcFileClose(&udcf);
    exit(0);
    }
else
    {
    // parent: access url, wait for child, do post-checking.
    struct udcFile *udcf = udcFileOpen(url, udcDefaultDir());
    int blksRead = 0;
    gotErrorParent = readAndTestBlocks(udcf, &offsetChild, MAX_BLOCKS, &blksRead, localCopy, url);
    udcFileClose(&udcf);
    // wait for child to finish:
    int childStatus;
    int retPid = waitpid(kidPid, &childStatus, 0);
    if (retPid < 0)
	errnoAbort("testConcurrent: waitpid(%d) failed", kidPid);
    if (! WIFEXITED(childStatus))
	warn("testConcurrent: child process did not exit() normally");
    if (WEXITSTATUS(childStatus))
	warn("testConcurrent: child exit status = %d)", WEXITSTATUS(childStatus));
    if (gotErrorChild)
	verbose(1, "Parent can see child got error.\n");
    gotErrorParent |= checkCacheFiles(sameOffset, max(offsetParent, offsetChild), url, localCopy);
    return (gotErrorParent || gotErrorChild);
    }
errAbort("testConcurrent: control should never reach this point.");
return TRUE;
}


int main(int argc, char *argv[])
/* Set up test params and run tests. */
{
boolean gotError = FALSE;
optionInit(&argc, argv, options);
raBuf = optionExists("raBuf");
doFork = optionExists("fork");
protocol = optionVal("protocol", protocol);
seed = optionInt("seed", seed);

char *host = getenv("HOST");
if (host == NULL || !startsWith("hgwdev", host))
    {
    // So that we don't break "make test" on other machines, use stdout and exit 0:
    puts("Sorry, this must be run on hgwdev (with HOST=hgwdev)");
    exit(0);
    }
errAbortDebugnPushPopErr();
char tmp[256];
safef(tmp, sizeof tmp, "/data/tmp/%s/udcCache", getenv("USER"));
udcSetDefaultDir(tmp);
if (seed == 0)
    {
    long now = clock1();
    printf("Seeding random with unix time %ld\n", now);
    srand(now);
    }
else
    {
    printf("Seeding random with option -seed=%d\n", seed);
    srand(seed);
    }

if (sameString(protocol, "http"))
    {
    char *httpUrl = "http://hgwdev.cse.ucsc.edu/~angie/wgEncodeCshlRnaSeqAlignmentsK562ChromatinShort.bb";
    char *httpLocalCopy = "/gbdb/hg18/bbi/wgEncodeCshlRnaSeqAlignmentsK562ChromatinShort.bb";
    if (raBuf)
	gotError |= testReadAheadBuffer(httpUrl, httpLocalCopy);
    else if (doFork)
	gotError |= testConcurrent(httpUrl, httpLocalCopy);
    else
	gotError |= testInterleaved(httpUrl, httpLocalCopy);
    }
else if (sameString(protocol, "ftp"))
    {
    char *ftpUrl = THOUSAND_FTP CHR4_SLX_BAM;
    char *ftpLocalCopy = THOUSAND_HIVE CHR4_SLX_BAM;
    if (raBuf)
	gotError |= testReadAheadBuffer(ftpUrl, ftpLocalCopy);
    else if (doFork)
	gotError |= testConcurrent(ftpUrl, ftpLocalCopy);
    else
	gotError |= testInterleaved(ftpUrl, ftpLocalCopy);
    }
else
    errAbort("Unrecognized protocol '%s'", protocol);
return gotError;
}
