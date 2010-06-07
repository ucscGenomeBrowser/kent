/* pslSplit - "pslSplit - split into multiple output files by qName.*/
#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "obscure.h"
#include "portable.h"
#include "linefile.h"
#include "psl.h"
#include "options.h"

static char const rcsid[] = "$Id: pslSplit.c,v 1.7 2006/07/26 01:22:18 baertsch Exp $";

int chunkSize = 120;	/* cut out this many unique qNames in each output file. */
int maxLines = 7000;	/* cut out this many unique qNames in each output file. */
bool stripVer = FALSE;  /* remove version number if true */

void usage()
/* Explain usage and exit. */
{
errAbort(
   "pslSplit - split into multiple output files by qName\n"
   "usage:\n"
   "  pslSplit how outDir inFile(s)\n"
   "Assumes psl is already sorted by qName.  The final\n"
   "result (one .psl file per target) will be put in outDir.\n"
   "outDir will be created if it does not already exist.  \n"
   "     The 'how' parameter should be either 'head' or 'nohead'\n"
   "     -chunkSize=N (default %d) number of unique qNames per output file.\n"
   "     -stripVer                 remove version number\n"
   "     -maxLines=N (default %d)  limit max number (approx.) of lines per output file.\n"
   , chunkSize, maxLines);
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
struct psl *psl;

if (!lineFileNext(lf, &line, &lineSize))
    {
    return NULL;
    }
wordCount = chopLine(line, words);
if (wordCount == 21)
    {
    return pslLoad(words);
    }
else
    {
    errAbort("Bad line %d of %s, %d words expecting %d", lf->lineIx, lf->fileName, wordCount, 21);
    return NULL;
    }
}

void getTargetAcc(char *tName, char res[256])
/* Convert target name to just the accession. */
{
char *s = strchr(tName, '.');
if (s == NULL)
    strcpy(res, tName);
else
    {
    int size = s - tName;
    memcpy(res, tName, size);
    res[size] = 0;
    }
}

void pslSort2(char *outDir, char *tempDir, boolean noHead)
/* Do second step of sort - merge all sorted files in tempDir
 * to final outdir. */
{
char fileName[512];
struct slName *tmpList, *tmp;
struct midFile *midList = NULL, *mid;
int aliCount = 0;
FILE *f = NULL;
char lastTargetAcc[256];
char targetAcc[256];


strcpy(lastTargetAcc, "");
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
printf("writing %s", outDir);
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
	if (mid->lf != NULL && mid->psl == NULL)
	    {
	    if ((mid->psl = nextPsl(mid->lf)) == NULL)
		lineFileClose(&mid->lf);
	    }
	if (mid->psl != NULL)
	    {
	    if (bestMid == NULL || pslCmpTarget(&mid->psl, &bestMid->psl) < 0)
		bestMid = mid;
	    }
	}
    if (bestMid == NULL)
	break;
    getTargetAcc(bestMid->psl->tName, targetAcc);
    if (!sameString(targetAcc, lastTargetAcc))
	{
	strcpy(lastTargetAcc, targetAcc);
	carefulClose(&f);
	sprintf(fileName, "%s/%s.psl", outDir, targetAcc);
	f = mustOpen(fileName, "w");
	if (!noHead)
	    pslWriteHead(f);
	}
    pslTabOut(bestMid->psl, f);
    pslFree(&bestMid->psl);
    }
carefulClose(&f);
printf("\n");

printf("Cleaning up temp files\n");
for (tmp = tmpList; tmp != NULL; tmp = tmp->next)
    {
    sprintf(fileName, "%s/%s", tempDir, tmp->name);
    remove(fileName);
    }
}

void outputChunk(struct psl **pPslList, char *tempDir, int midIx, boolean noHead)
/* Sort and write out pslList and free it. */
{
char fileName[512];
FILE *f;
struct psl *psl;

if (*pPslList == NULL)
    return; 	/* Empty. */
psl = *pPslList;
//slSort(pPslList, pslCmpTarget);
makeMidName(tempDir, midIx, fileName);
if (stripVer)
    {
    char *s = stringIn(".",psl->qName);
    if (s != NULL)
        *s = 0;
    }
if (chunkSize ==1)
    safef(fileName, sizeof(fileName), "%s/%s.psl",tempDir,psl->qName);
f = mustOpen(fileName, "w");
if (!noHead)
    pslWriteHead(f);
for (psl = *pPslList; psl != NULL; psl = psl->next)
    pslTabOut(psl, f);
fclose(f);
pslFreeList(pPslList);
}


void pslSplit(char *command, char *outDir,  char *inFiles[], int inFileCount)
/* pslSplit - "pslSplit - split into multiple output files by qName.*/
{
int linesLeftInChunk = chunkSize;
int i;
char *inFile;
char fileName[512];
int fileCount;
int totalLineCount = 0;
int midFileCount = 0;
FILE *f;
struct lineFile *lf;
char *line;
char *prev = cloneString("first");
int lineSize;
struct psl *psl, *pslList = NULL;
boolean noHead = (sameWord(command, "nohead"));

mkdir(outDir, 0775);

/* Read in presorted input and scatter it into sorted
 * temporary files. */
for (i = 0; i<inFileCount; ++i)
    {
    int linesLeft = maxLines;
    bool breakNext = FALSE;
    //char name[512];
    inFile = inFiles[i];
    printf("Processing %s", inFile);
    fflush(stdout);
    lf = pslFileOpen(inFile);
    psl = nextPsl(lf) ;
    prev = cloneString(psl->qName);
    slAddHead(&pslList, psl);
    while ((psl = nextPsl(lf)) != NULL)
	{
        //safef(name, sizeof(name), "%s",psl->qName);
        //chopSuffix(name);
        if (!sameString(prev, psl->qName))
            {
            prev = cloneString(psl->qName);
            if (--linesLeftInChunk <= 0 || breakNext)
                {
                outputChunk(&pslList, outDir, midFileCount++, noHead);
                linesLeftInChunk = chunkSize;
                linesLeft = maxLines;
                breakNext = FALSE;
                }
            }
        if (--linesLeft < 0)
            {
            breakNext = TRUE;
            }
	if ((++totalLineCount & 0xffff) == 0)
	    {
	    printf(".");
	    fflush(stdout);
	    }
	slAddHead(&pslList, psl);
        //freeMem(&prev);
	}
    printf("\n");
    lineFileClose(&lf);
    }
outputChunk(&pslList, outDir, midFileCount++, noHead);
printf("Processed %d lines into %d output files\n", totalLineCount, midFileCount);
//pslSort2(outDir, tempDir, noHead);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc < 4)
    usage();
chunkSize = optionInt("chunkSize", chunkSize);
maxLines = optionInt("maxLines", maxLines);
stripVer = optionExists("stripVer");
pslSplit(argv[1], argv[2], &argv[3], argc-3);
return 0;
}
