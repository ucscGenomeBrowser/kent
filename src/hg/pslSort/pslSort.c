/* pslSort - merge and sort psCluster .psl output files. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "memalloc.h"
#include "localmem.h"
#include "options.h"
#include "psl.h"


boolean nohead = FALSE; /* No header for psl files?  Command line option. */

void usage()
/* Explain usage and exit. */
{
errAbort(
   "pslSort - Merge and sort psCluster .psl output files\n"
   "usage:\n"
   "      pslSort dirs[1|2] outFile tempDir inDir(s)OrFile(s)\n"
   "\n"
   "   This will sort all of the .psl input files or those in the directories\n"
   "   inDirs in two stages - first into temporary files in tempDir\n"
   "   and second into outFile.  The device on tempDir must have\n"
   "   enough space (typically 15-20 gigabytes if processing whole genome).\n"
   "\n"
   "      pslSort g2g[1|2] outFile tempDir inDir(s)\n"
   "\n"
   "   This will sort a genome-to-genome alignment, reflecting the\n"
   "   alignments across the diagonal.\n"  
   "\n"
   "   Adding 1 or 2 to the dirs or g2g option will limit the program to only\n"
   "   the first or second pass respectively of the sort.\n"
   "\n"
   "options:\n"
   "   -nohead      Do not write psl header.\n"
   "   -verbose=N   Set verbosity level, higher for more output. Default is 1.\n"
   );
}

void makeMidName(char *tempDir, int ix, char *retName)
/* Return name of temp file of given index. */
{
sprintf(retName, "%s/tmp%d.psl", tempDir, ix);
}

struct midFile
/* Info on an intermediate file - a sorted chunk. */
    {
    struct midFile *next;	/* Next in list. */
    struct lineFile *lf;        /* Associated file. */
    struct psl *psl;            /* Current record. */
    };

struct psl *nextPsl(struct lineFile *lf)
/* Read next line from file and convert it to psl.  Return
 * NULL at eof. */
{
char *line;
int lineSize;
char *words[32];
int wordCount;

if (!lineFileNext(lf, &line, &lineSize))
    {
    //warn("File %s appears to be incomplete\n", lf->fileName);
    return NULL;
    }
wordCount = chopTabs(line, words);
if (wordCount == 21)
    {
    return pslLoad(words);
    }
else if (wordCount == 23)
    {
    return pslxLoad(words);
    }
else
    {
    warn("Bad line %d of %s", lf->lineIx, lf->fileName);
    return NULL;
    }
}


struct psl *nextLmPsl(struct lineFile *lf, struct lm *lm)
/* Read next line from file and convert it to psl.  Return
 * NULL at eof. */
{
char *line;
int lineSize;
char *words[32];
int wordCount;

if (!lineFileNext(lf, &line, &lineSize))
    return NULL;
wordCount = chopTabs(line, words);
if (wordCount == 21)
    {
    return pslLoadLm(words, lm);
    }
else if (wordCount == 23)
    {
    return pslxLoadLm(words, lm);
    }
else
    {
    warn("Bad line %d of %s", lf->lineIx, lf->fileName);
    return NULL;
    }
}




void pslSort2(char *outFile, char *tempDir)
/* Do second step of sort - merge all sorted files in tempDir
 * to final. */
{
char fileName[512];
struct slName *tmpList, *tmp;
struct midFile *midList = NULL, *mid;
int aliCount = 0;
FILE *f = mustOpen(outFile, "w");


if (!nohead)
    pslWriteHead(f);
tmpList = listDir(tempDir, "tmp*.psl");
if (tmpList == NULL)
    errAbort("No tmp*.psl files in %s\n", tempDir);
for (tmp = tmpList; tmp != NULL; tmp = tmp->next)
    {
    sprintf(fileName, "%s/%s", tempDir, tmp->name);
    AllocVar(mid);
    mid->lf = pslFileOpen(fileName);
    slAddHead(&midList, mid);
    }
verbose(1, "writing %s", outFile);
fflush(stdout);
/* Write out the lowest sorting line from mid list until done. */
for (;;)
    {
    struct midFile *bestMid = NULL;
    if ( (++aliCount & 0xffff) == 0)
	{
	verboseDot();
	fflush(stdout);
	}
    for (mid = midList; mid != NULL; mid = mid->next)
	{
	if (mid->lf != NULL && mid->psl == NULL)
	    {
	    if ((mid->psl = nextPsl(mid->lf)) == NULL)
		lineFileClose(&mid->lf);
	    }
	if (mid->psl != NULL)
	    {
	    if (bestMid == NULL || pslCmpQuery(&mid->psl, &bestMid->psl) < 0)
		bestMid = mid;
	    }
	}
    if (bestMid == NULL)
	break;
    pslTabOut(bestMid->psl, f);
    pslFree(&bestMid->psl);
    }
printf("\n");
fclose(f);

/* The followint really shouldn't be necessary.... */
for (mid = midList; mid != NULL; mid = mid->next)
    lineFileClose(&mid->lf);

printf("Cleaning up temp files\n");
for (tmp = tmpList; tmp != NULL; tmp = tmp->next)
    {
    sprintf(fileName, "%s/%s", tempDir, tmp->name);
    remove(fileName);
    }
}

int calcMilliScore(struct psl *psl)
/* Figure out percentage score. */
{
int milliScore = psl->match + psl->repMatch - psl->misMatch -
	2*psl->qNumInsert - 4*psl->tNumInsert;

milliScore *= 1000;
milliScore /= (psl->qEnd - psl->qStart);
return milliScore;
}


void reverseRange(unsigned *pStart, unsigned *pEnd, unsigned size)
/* Reverse strand that range occupies. */
{
int start, end;
start = size - *pEnd;
end = size - *pStart;
*pStart = start;
*pEnd = end;
}

void reverseStartList(unsigned *oldList, unsigned *newList, 
	unsigned *sizes, int count, int newSize)
/* Reverse order and strand of start blocks. */
{
int i, revI;

for (i=0; i<count; ++i)
    {
    revI = count-1-i;
    newList[i] = newSize - (oldList[revI] + sizes[revI]);
    }
}

void reverseSizeList(unsigned *oldList, unsigned *newList, int count)
/* Reverse order of size list. */
{
int i, revI;
for (i=0; i<count; ++i)
    {
    revI = count-1-i;
    newList[i] = oldList[revI];
    }
}

struct psl *mirrorLmPsl(struct psl *psl, struct lm *lm)
/* Reflect a psl into local memory. */
{
struct psl *p = lmCloneMem(lm, psl, sizeof(*psl));
p->qNumInsert = psl->tNumInsert;
p->tNumInsert = psl->qNumInsert;
p->qBaseInsert = psl->tBaseInsert;
p->tBaseInsert = psl->qBaseInsert;
p->qName = lmCloneString(lm, psl->tName);
p->tName = lmCloneString(lm, psl->qName);
p->qSize = psl->tSize;
p->tSize = psl->qSize;
p->qStart = psl->tStart;
p->tStart = psl->qStart;
p->qEnd = psl->tEnd;
p->tEnd = psl->qEnd;
p->blockSizes = lmCloneMem(lm, psl->blockSizes, psl->blockCount*sizeof(psl->blockSizes[0]));
p->qStarts = lmCloneMem(lm, psl->tStarts, psl->blockCount*sizeof(psl->tStarts[0]));
p->tStarts = lmCloneMem(lm, psl->qStarts, psl->blockCount*sizeof(psl->tStarts[0]));
if (p->strand[0] == '-')
    {
    reverseStartList(psl->qStarts, p->tStarts, p->blockSizes, p->blockCount, p->tSize);
    reverseStartList(psl->tStarts, p->qStarts, p->blockSizes, p->blockCount, p->qSize);
    reverseSizeList(psl->blockSizes, p->blockSizes, p->blockCount);
    }
return p;
}

boolean selfFile(char *path)
/* Return TRUE if of form XX_XX.psl */
{
char dir[256], root[128], ext[128];
char *a, *b;

splitPath(path, dir, root, ext);
a = root;
b = strchr(a, '_');
if (b == NULL)
    return FALSE;
*b++ = 0;
return sameString(a, b);
}


void pslSort(char *command, char *outFile, char *tempDir, char *inDirs[], int inDirCount)
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
boolean doReflect = FALSE;
boolean suppressSelf = FALSE;
boolean firstOnly = endsWith(command, "1");
boolean secondOnly = endsWith(command, "2");

if (startsWith("dirs", command))
    ;
else if (startsWith("g2g", command))
    {
    doReflect = TRUE;
    suppressSelf = TRUE;
    }
else
    usage();


if (!secondOnly)
    {
    makeDir(tempDir);
    /* Figure out how many files to process. */
    for (i=0; i<inDirCount; ++i)
	{
	inDir = inDirs[i];
        /* if not a dir but actually a file, just add it */
        if (isRegularFile(inDir))
            {
	    name = newSlName(cloneString(inDir));
            slAddHead(&fileList, name);
            continue;
            }
	dirDir = listDir(inDir, "*.psl");
	if (slCount(dirDir) == 0)
	    dirDir = listDir(inDir, "*.psl.gz");
	if (slCount(dirDir) == 0)
	    errAbort("No psl files in %s\n", inDir);
	verbose(1, "%s with %d files\n", inDir, slCount(dirDir));
	for (dirFile = dirDir; dirFile != NULL; dirFile = dirFile->next)
	    {
	    sprintf(fileName, "%s/%s", inDir, dirFile->name);
	    name = newSlName(fileName);
	    slAddHead(&fileList, name);
	    }
	slFreeList(&dirDir);
	}
    verbose(1, "%d files in %d dirs\n", slCount(fileList), inDirCount);
    slReverse(&fileList);
    fileCount = slCount(fileList);
    filesPerMidFile = round(sqrt(fileCount));
    // if (filesPerMidFile > 20)
	// filesPerMidFile = 20;  /* bandaide! Should keep track of mem usage. */
    verbose(1, "Got %d files %d files per mid file\n", fileCount, filesPerMidFile);

    /* Read in files a group at a time, sort, and write merged, sorted
     * output of one group. */
    name = fileList;
    while (totalFilesProcessed < fileCount)
	{
	int filesInMidFile = 0;
	struct psl *pslList = NULL, *psl;
	int lfileCount = 0;
	struct lm *lm = lmInit(256*1024);

	for (filesInMidFile = 0; filesInMidFile < filesPerMidFile && name != NULL;
	    ++filesInMidFile, ++totalFilesProcessed, name = name->next)
	    {
	    boolean reflectMe = FALSE;
	    if (doReflect)
		{
		reflectMe = !selfFile(name->name);
		}
	    verbose(2, "Reading %s (%d of %d)\n", name->name, totalFilesProcessed+1, fileCount);
	    lf = pslFileOpen(name->name);
	    while ((psl = nextLmPsl(lf, lm)) != NULL)
		{
		if (psl->qStart == psl->tStart && psl->strand[0] == '+' && 
		    suppressSelf && sameString(psl->qName, psl->tName))
		    {
		    continue;
		    }
		++lfileCount;
		slAddHead(&pslList, psl);
		if (reflectMe)
		    {
		    psl = mirrorLmPsl(psl, lm);
		    slAddHead(&pslList, psl);
		    }
		}
	    lineFileClose(&lf);
	    }
	slSort(&pslList, pslCmpQuery);
	makeMidName(tempDir, midFileCount, fileName);
	verbose(1, "Writing %s\n", fileName);
	f = mustOpen(fileName, "w");
	if (!nohead)
	    pslWriteHead(f);
	for (psl = pslList; psl != NULL; psl = psl->next)
	    {
	    pslTabOut(psl, f);
	    }
	fclose(f);
	pslList = NULL;
	lmCleanup(&lm);
	verbose(2, "lfileCount %d\n", lfileCount);
	++midFileCount;
	}
    }
if (!firstOnly)
    pslSort2(outFile, tempDir);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);

nohead = optionExists("nohead");

if (argc < 5)
    usage();
pslSort(argv[1], argv[2], argv[3], &argv[4], argc-4);
return 0;
}
