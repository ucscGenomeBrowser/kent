/* pscSort - merge and sort psCluster .psc output files. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "as_psc.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
   "pscSort - merge and sort psCluster .psc output files\n"
   "usage:\n"
   "  pscSort outFile tempDir inDir(s)\n"
   "This will sort all of the .psc files in the directories\n"
   "inDirs in two stages - first into temporary files in tempDir\n"
   "and second into outFile.  The device on tempDir needs to have\n"
   "enough space (typically 15-20 gigabytes if processing whole genome)\n"
   );
}

static int cmpPsc(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct psc *a = *((struct psc **)va);
const struct psc *b = *((struct psc **)vb);
int dif;
dif = strcmp(a->qAcc, b->qAcc);
if (dif == 0)
    dif = a->qStart - b->qStart;
return dif;
}

void makeMidName(char *tempDir, int ix, char *retName)
/* Return name of temp file of given index. */
{
sprintf(retName, "%s/tmp%d.psc", tempDir, ix);
}

struct midFile
/* Info on an intermediate file - a sorted chunk. */
    {
    struct midFile *next;	/* Next in list. */
    struct lineFile *lf;        /* Associated file. */
    struct psc *psc;            /* Current record. */
    };

struct psc *nextPsc(struct lineFile *lf)
/* Read next line from file and convert it to psc.  Return
 * NULL at eof. */
{
char *line;
int lineSize;
char *words[16];
int wordCount;
struct psc *psc;

if (!lineFileNext(lf, &line, &lineSize))
    {
    warn("File %s appears to be incomplete\n", lf->fileName);
    return NULL;
    }
wordCount = chopTabs(line, words);
if (wordCount == 13)
    {
    return pscLoad(words);
    }
else if (wordCount == 1 && sameWord(words[0], "ZZZ"))
    {
    return NULL;
    }
else
    {
    errAbort("Bad line %d of %s", lf->lineIx, lf->fileName);
    return NULL;
    }
}

void pscSort2(char *outFile, char *tempDir)
/* Do second step of sort - merge all sorted files in tempDir
 * to final. */
{
char fileName[512];
struct slName *tmpList, *tmp;
struct midFile *midList = NULL, *mid;
int aliCount = 0;
FILE *f = mustOpen(outFile, "w");

tmpList = listDir(tempDir, "tmp*.psc");
if (tmpList == NULL)
    errAbort("No tmp*.psc files in %s\n", tempDir);
for (tmp = tmpList; tmp != NULL; tmp = tmp->next)
    {
    sprintf(fileName, "%s/%s", tempDir, tmp->name);
    AllocVar(mid);
    mid->lf = lineFileOpen(fileName, TRUE);
    slAddHead(&midList, mid);
    }
printf("writing %s", outFile);
fflush(stdout);
/* Write out the lowest sorting line from mid list until done. */
for (;;)
    {
    struct midFile *bestMid = NULL;
    if ( (++aliCount & 0xffff) == 0)
	{
	printf(".");
	fflush(stdout);
	}
    for (mid = midList; mid != NULL; mid = mid->next)
	{
	if (mid->lf != NULL && mid->psc == NULL)
	    {
	    if ((mid->psc = nextPsc(mid->lf)) == NULL)
		lineFileClose(&mid->lf);
	    }
	if (mid->psc != NULL)
	    {
	    if (bestMid == NULL || cmpPsc(&mid->psc, &bestMid->psc) < 0)
		bestMid = mid;
	    }
	}
    if (bestMid == NULL)
	break;
    pscTabOut(bestMid->psc, f);
    pscFree(&bestMid->psc);
    }
printf("\n");
fprintf(f, "ZZZ\n");
fclose(f);

printf("Cleaning up temp files\n");
for (tmp = tmpList; tmp != NULL; tmp = tmp->next)
    {
    // remove(tmp->name);
    }
}

void pscSort(char *outFile, char *tempDir, char *inDirs[], int inDirCount)
/* Do the two step sort. */
{
int i;
struct slName *fileList = NULL, *name;
char *inDir;
struct slName *dirDir, *dirFile;
char fileName[512];
int fileCount;
int totalFilesProcessed = 0;
int filesPerMidFile;
int midFileCount = 0;
FILE *f;
struct lineFile *lf;

/* Figure out how many files to process. */
for (i=0; i<inDirCount; ++i)
    {
    inDir = inDirs[i];
    dirDir = listDir(inDir, "*.psc");
    for (dirFile = dirDir; dirFile != NULL; dirFile = dirFile->next)
	{
	sprintf(fileName, "%s/%s", inDir, dirFile->name);
	name = newSlName(fileName);
	slAddHead(&fileList, name);
	}
    slFreeList(&dirDir);
    }
slReverse(&fileList);
fileCount = slCount(fileList);
filesPerMidFile = round(sqrt(fileCount));
uglyf("Got %d files %d files per mid file\n", fileCount, filesPerMidFile);

/* Read in files a group at a time, sort, and write merged, sorted
 * output of one group. */
name = fileList;
while (totalFilesProcessed < fileCount)
    {
    int filesInMidFile = 0;
    struct psc *pscList = NULL, *psc;

    for (filesInMidFile = 0; filesInMidFile < filesPerMidFile && name != NULL;
    	++filesInMidFile, ++totalFilesProcessed, name = name->next)
	{
	boolean gotEnd = FALSE;
	printf("Reading %s (%d of %d)\n", name->name, totalFilesProcessed+1, fileCount);
	lf = lineFileOpen(name->name, TRUE);
	while ((psc = nextPsc(lf)) != NULL)
	    {
	    slAddHead(&pscList, psc);
	    }
	lineFileClose(&lf);
	}
    slSort(&pscList, cmpPsc);
    makeMidName(tempDir, midFileCount, fileName);
    printf("Writing %s\n", fileName);
    f = mustOpen(fileName, "w");
    for (psc = pscList; psc != NULL; psc = psc->next)
	{
	pscTabOut(psc, f);
	}
    fprintf(f, "ZZZ\n");
    fclose(f);
    pscFreeList(&pscList);
    ++midFileCount;
    }
pscSort2(outFile, tempDir);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 4)
    usage();
// pscSort(argv[1], argv[2], &argv[3], argc-3);
pscSort2(argv[1], argv[2]);
}
   
