/* sangPairs - Process Sanger Paired reads to remove low quality bases and put in one big file. */
#include "common.h"
#include "portable.h"
#include "hash.h"
#include "fa.h"
#include "qaSeq.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "sangPairs - Process Sanger Paired reads to remove low quality bases and put in one big file\n"
  "usage:\n"
  "   sangPairs sangDir outFile.fa\n");
}

int tilNextGood(UBYTE *q, int size, int minQual)
/* Return number of bases until next one that meets or exceeds minQual. */
{
int i;

for (i=0; i<size; ++i)
    {
    if (q[i] >= minQual)
	break;
    }
return i;
}

int tilNextBad(UBYTE *q, int size, int minQual)
/* Return number of bases until next one that is less than minQual. */
{
int i;

for (i=0; i<size; ++i)
    {
    if (q[i] < minQual)
	break;
    }
return i;
}

int revTilNextGood(UBYTE *q, int size, int minQual)
/* Return number of bases in reverse direction until next one that meets or exceeds minQual. */
{
int i;

q += size-1;
for (i=0; i<size; ++i)
    {
    if (q[-i] >= minQual)
	break;
    }
return i;
}

int revTilNextBad(UBYTE *q, int size, int minQual)
/* Return number of bases in reverse direction until next one that is less than minQual. */
{
int i;

q += size-1;
for (i=0; i<size; ++i)
    {
    if (q[-i] < minQual)
	break;
    }
return i;
}



boolean trimQa(struct qaSeq *qa, int minQual, int minQualRun, int *retStart, int *retSize)
/* Find part of sequence that survives trimming according to minQual
 * and qual run specs.  Return FALSE if nothing left after trimming.
 * This trims sequence on both ends until it comes to a run of sequences
 * of length minQualRun or more which meets or exceeds minQual. */
{
int qaSize = qa->size;
UBYTE *q = qa->qa;
int start, end;
int runSize;

for (start = 0; start < qaSize; )
    {
    start += tilNextGood(q+start, qaSize-start, minQual);
    runSize = tilNextBad(q+start, qaSize-start, minQual);
    if (runSize >= minQualRun)
        break;
    start += runSize;
    }
*retStart = start;
if (start >= qaSize)
    {
    *retSize = 0;
    return FALSE;
    }
for (end = qaSize; end > start;)
    {
    runSize = revTilNextGood(q+start, end - start,  minQual);
    end -= runSize;
    runSize = revTilNextBad(q+start, end - start,  minQual);
    if (runSize >= minQualRun)
        break;
    end -= runSize;
    }
*retSize = end - start;
return (end > start);
}

int filterByQual(char *faFileName, FILE *f, int minQual, int minQualRun, struct hash *uniqHash)
/* Write out parts of sequence that meet quality standards to fa file in out. 
 * Returns untrimmed size. */
{
char qaFileName[512], dir[256], name[128], ext[64];
struct qaSeq *qa;
int start, size;
int initialSize;

splitPath(faFileName, dir, name, ext);
sprintf(qaFileName, "%s%s.qual", dir, name);
qa = qaMustReadBoth(qaFileName, faFileName);
if (hashLookup(uniqHash, qa->name))
   warn("%s duplicated, ignoring all but first occurence", qa->name);
else
    {
    hashAdd(uniqHash, qa->name, NULL);
    if (trimQa(qa, minQual, minQualRun, &start, &size))
	{
	faWriteNext(f, qa->name, qa->dna + start, size);
	}
    }
initialSize = qa->size;
qaSeqFree(&qa);
return initialSize;
}

void sangPairs(char *sangDir, char *outFile)
/* sangPairs - Process Sanger Paired reads to remove low quality bases and put in one big file. */
{
struct hash *hash = newHash(20);
struct fileInfo *dirList, *dirEl;
struct fileInfo *subList, *subEl;
struct fileInfo *faList, *faEl;
FILE *f = mustOpen(outFile, "w");
unsigned long totalSize = 0;

dirList = listDirX(sangDir, "*", TRUE);
for (dirEl = dirList; dirEl != NULL; dirEl = dirEl->next)
    {
    if (dirEl->isDir)
	{
	printf("%s", dirEl->name);
	fflush(stdout);
	subList = listDirX(dirEl->name, "*", TRUE);
	for (subEl = subList; subEl != NULL; subEl = subEl->next)
	    {
	    if (subEl->isDir)
	        {
		printf(".");
		fflush(stdout);
		faList = listDirX(subEl->name, "*.fasta", TRUE);
		for (faEl = faList; faEl != NULL; faEl = faEl->next)
		    {
		    totalSize += filterByQual(faEl->name, f, 19, 15, hash);
		    }
		slFreeList(&faList);
		}
	    }
	printf("\n");
	slFreeList(&subList);
	}
    }
printf("Total size %lu bytes\n", totalSize);
fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
dnaUtilOpen();
sangPairs(argv[1], argv[2]);
return 0;
}
