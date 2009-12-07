/* udcTest -- test the URL data cache */

// TODO:
// add seed as a cmd line option

// suggestions from Mark: 1. try setvbuf, to make FILE * unbuffered -- does that help?
//                        2. *if* need to do own buffering, consider mmap()
//                           (kernel handles buffering)

#include "common.h"
#include "options.h"
#include "portable.h"
#include "udc.h"

static char const rcsid[] = "$Id: udcTest.c,v 1.1 2009/12/07 19:44:24 angie Exp $";

static struct optionSpec options[] = {
    {NULL, 0},
};

// Local copy (reference file) and URL for testing:
#define THOUSAND_HIVE "/hive/data/outside/1000genomes/ncbi/ftp-trace.ncbi.nih.gov/1000genomes/"
#define THOUSAND_FTP "ftp://ftp-trace.ncbi.nih.gov/1000genomes/ftp/pilot_data/data/"
#define CHR3_SLX_BAM "NA12878/alignment/NA12878.chrom3.SLX.maq.SRP000032.2009_07.bam"

// Use typical size range of bgzip-compressed data blocks:
#define MIN_BLK_SIZE 20000
#define MAX_BLK_SIZE 30000

// Read at most this many consecutive blocks:
#define MAX_BLOCKS 100

int mustOpen2(char *filename, int options)
/* Like mustOpen, but uses the "man 2 open" instead of fopen. */
{
int fd = open(filename, options);
if (fd < 0)
    errnoAbort("failed to open(%s, 0x%02x)", filename, options);
return fd;
}

void mustLseek(int fd, bits64 offset, int options, char *filename)
/* lseek() or die. */
{
bits64 checkOffset = lseek(fd, offset, options);
if (checkOffset != offset)
    errnoAbort("Unable to lseek to %lld in %s (lseek ret: %lld)", offset, filename, checkOffset);
}

bits64 mustRead2(int fd, char *buf, bits64 len, char *filename, bits64 offset)
/* Like mustRead, but uses the "man 2 read" instead of fread. */
{
bits64 bytesRead = read(fd, buf, len);
if (bytesRead < 0)
    errnoAbort("read of %lld bytes from %s @%lld failed", len, filename, offset);
return bytesRead;
}

void openSeekRead(char *filename, bits64 offset, bits64 len, char *buf)
/* Read len bits starting at offset from filename into buf or die. */
{
int fd = mustOpen2(filename, O_RDONLY);
mustLseek(fd, offset, SEEK_SET, filename);
bits64 bytesRead = mustRead2(fd, buf, len, filename, offset);
close(fd);
if (bytesRead != len)
    errAbort("Expected to read %lld bytes from %s @%lld, but got %lld",
	     len, filename, offset, bytesRead);
}

boolean compareBytes(char *bufTest, char *bufRef, bits64 len, char *testName, char *refName,
		     char *testDesc, bits64 offset)
/* Report any differences between bufTest and bufRef (don't errAbort). */
{
boolean gotError = FALSE;
bits64 i, difCount = 0;
for (i=0;  i < len;  i++)
    {
    if (bufTest[i] != bufRef[i])
	{
	if (difCount == 0)
	    warn("%s and %s first differ at offset %lld + %lld = %lld [0x%02x != 0x%02x]",
		 testName, refName, offset, i, offset+i, (bits8)bufTest[i], (bits8)bufRef[i]);
	difCount++;
	}
    }
if (difCount == 0)
    verbose(3, "Success: %s = ref, %lld bytes @%lld\n", testDesc, len, offset);
else
    {
    warn("-- %lld different bytes total in block of %lld bytes starting at %lld",
	 difCount, len, offset);
    gotError = TRUE;
    }
return gotError;
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

// Get data from udcf's sparse data file and compare to reference:
struct slName *sl, *cacheFiles = udcFileCacheFiles(url, udcDefaultDir());
char *sparseFileName = NULL;
for (sl = cacheFiles; sl != NULL; sl = sl->next)
    if (endsWith(sl->name, "sparseData"))
	sparseFileName = sl->name;
if (sparseFileName == NULL)
    errAbort("readAndTest: can't file sparseData file in udcFileCacheFiles results");
openSeekRead(sparseFileName, offset, len, bufTest);
gotError |= compareBytes(bufTest, bufRef, len, sparseFileName, localCopy, "sparse", offset);
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

boolean testInterleaved(char *url, char *localCopy)
/* Open two udcFile handles to the same file, read probably-different random locations,
 * read from probably-overlapping random locations, and check for errors. */
{
boolean gotError = FALSE;
bits64 size = fileSize(localCopy);

// First, read some bytes from udcFile udcf1.
struct udcFile *udcf1 = udcFileOpen(url, NULL);
int blksRead1 = 0;
bits64 offset1 = randomStartOffset(size);
gotError |= readAndTestBlocks(udcf1, &offset1, 2, &blksRead1, localCopy, url);
// While keeping udcf1 open, create udcf2 on the same URL, and read from a 
// (probably) different location:
struct udcFile *udcf2 = udcFileOpen(url, NULL);
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
return gotError;
}


int main(int argc, char *argv[])
{
boolean gotError = FALSE;
optionInit(&argc, argv, options);
char *host = getenv("HOST");
if (host == NULL || !startsWith("hgwdev", host))
    {
    // So that we don't break "make test" on other machines, use stdout and exit 0:
    puts("Sorry, this must be run on hgwdev (with HOST=hgwdev)");
    exit(0);
    }
// [hgwdev:~] grep udc.cacheDir /usr/local/apache/cgi-bin/hg.conf
// udc.cacheDir=../trash/udcCache
udcSetDefaultDir("/usr/local/apache/trash/udcCache");
long now = clock1();
printf("Seeding random with unix time %ld\n", now);
srand(now);

char *url = THOUSAND_FTP CHR3_SLX_BAM;
char *localCopy = THOUSAND_HIVE CHR3_SLX_BAM;
gotError |= testInterleaved(url, localCopy);
return gotError;
}
