/* Coverage - figure out length of input to ooGreedy. */
#include "common.h"
#include "portable.h"
#include "obscure.h"
#include "fa.h"
#include "hash.h"

void usage()
/* Print usage and exit. */
{
errAbort(
   "coverage - figure out length of input to ooGreedy\n"
   "usage:\n"
   "    coverage ooDir");
}

unsigned long ctgSize(char *genoLst)
/* Total size of all existing fas in geno.lst */
{
char **fileNames, *fileName, *buf;
int fileCount;
struct dnaSeq seq;
unsigned long total = 0;
int i;
FILE *f;

readAllWords(genoLst, &fileNames, &fileCount, &buf);
for (i=0; i<fileCount; ++i)
    {
    fileName = fileNames[i];
    if ((f = fopen(fileName, "r")) != NULL)
	{
	while (faFastReadNext(f, &seq.dna, &seq.size, &seq.name))
	    total += seq.size;
	fclose(f);
	}
    }
freeMem(buf);
return total;
}


void coverage(char *ooDir)
{
struct slName *chromDirs, *chromEl;
struct slName *contigDirs, *contigEl;
char subDir[512];
unsigned long total = 0;
struct hash *uniqHash = newHash(0);


chromDirs = listDir(ooDir, "*");
printf("Chromosome:");
for (chromEl = chromDirs; chromEl != NULL; chromEl = chromEl->next)
    {
    char *chromName = chromEl->name;
    int dirNameLen = strlen(chromName);
    if (dirNameLen > 0 && dirNameLen <= 2)
	{
	printf(" %s", chromName);
	fflush(stdout);
	sprintf(subDir, "%s/%s", ooDir, chromName);
	contigDirs = listDir(subDir, "ctg*");
	for (contigEl = contigDirs; contigEl != NULL; contigEl = contigEl->next)
	    {
	    unsigned long oneSize;
	    char *contigName = contigEl->name;
	    char fileName[512];
	    sprintf(fileName, "%s/%s/%s", subDir, contigName, "geno.lst");
	    if (fileExists(fileName))
		{
		oneSize = ctgSize(fileName);
		total += oneSize;
		}
	    }
	slFreeList(&contigDirs);
	}
    }
printf("\n");
printf("Total %lu bases\n", total);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
coverage(argv[1]);
return 0;
}

