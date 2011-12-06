/* ooCloneSizes - Print out sizes of a bunch of clone .fa files. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "dnaseq.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "ooCloneSizes - Print out sizes of a bunch of clone .fa files.\n"
  "Also first and last fragment names\n"
  "usage:\n"
  "   ooCloneSizes dir(s)\n");
}

void ooCloneSizes(int dirCount, char *dirs[])
/* ooCloneSizes - Print out sizes of a bunch of clone .fa files. */
{
int i;
char *dir;
struct fileInfo *faList, *faFile;
struct dnaSeq seq;
struct lineFile *lf;
char sDir[256], sFile[128], sExt[64];
char firstFrag[128], lastFrag[128];
ZeroVar(&seq);

for (i=0; i<dirCount; ++i)
    {
    dir = dirs[i];
    faList = listDirX(dir, "*.fa", TRUE);
    for (faFile = faList; faFile != NULL; faFile = faFile->next)
        {
	int cloneSize = 0;
	boolean isFirst = TRUE;
	lf = lineFileOpen(faFile->name, TRUE);
	while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
	    {
	    if (isFirst)
		{
		isFirst = FALSE;
	        strcpy(firstFrag, seq.name);
		}
	    strcpy(lastFrag, seq.name);
	    cloneSize += seq.size;
	    }
	splitPath(faFile->name, sDir, sFile, sExt);
	printf("%s\t%d\t%s\t%s\n", sFile, cloneSize, firstFrag, lastFrag);
	lineFileClose(&lf);
	}
    slFreeList(&faList);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 2)
    usage();
ooCloneSizes(argc-1, argv+1);
return 0;
}
