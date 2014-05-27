/* pslSortAcc - Sort psl file by accession of target, and write
 * one file for each target. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "obscure.h"
#include "portable.h"
#include "linefile.h"
#include "psl.h"



void usage()
/* Explain usage and exit. */
{
errAbort(
   "pslSortAcc - sort pslSort .psl output file by accession\n"
   "Make one output .psl file per accession.\n"
   "usage:\n"
   "  pslSortAcc how outDir tempDir inFile(s)\n"
   "This will sort the inFiles by accession in two steps\n"
   "Intermediate results will be put in tempDir.  The final\n"
   "result (one .psl file per target) will be put in outDir.\n"
   "Both outDir and tempDir will be created if they do not\n"
   "already exist.  The 'how' parameter should be either\n"
   "'head' or 'nohead'");
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

void outputChunk(struct psl **pPslList, char *tempDir, int midIx)
/* Sort and write out pslList and free it. */
{
char fileName[512];
FILE *f;
struct psl *psl;

if (*pPslList == NULL)
    return; 	/* Empty. */
slSort(pPslList, pslCmpTarget);
makeMidName(tempDir, midIx, fileName);
f = mustOpen(fileName, "w");
pslWriteHead(f);
for (psl = *pPslList; psl != NULL; psl = psl->next)
    pslTabOut(psl, f);
fclose(f);
pslFreeList(pPslList);
}


void pslSortAcc(char *command, char *outDir, char *tempDir, char *inFiles[], int inFileCount)
/* Do the two step sort. */
{
int chunkSize = 250000;	/* Do this many lines at once. */
int linesLeftInChunk = chunkSize;
int i;
char *inFile;
int totalLineCount = 0;
int midFileCount = 0;
struct lineFile *lf;
struct psl *psl, *pslList = NULL;
boolean noHead = (sameWord(command, "nohead"));

mkdir(outDir, 0775);
mkdir(tempDir, 0775);

/* Read in input and scatter it into sorted
 * temporary files. */
for (i = 0; i<inFileCount; ++i)
    {
    inFile = inFiles[i];
    printf("Processing %s", inFile);
    fflush(stdout);
    lf = pslFileOpen(inFile);
    while ((psl = nextPsl(lf)) != NULL)
	{
	slAddHead(&pslList, psl);
	if (--linesLeftInChunk <= 0)
	    {
	    outputChunk(&pslList, tempDir, midFileCount++);
	    linesLeftInChunk = chunkSize;
	    }
	if ((++totalLineCount & 0xffff) == 0)
	    {
	    printf(".");
	    fflush(stdout);
	    }
	}
    printf("\n");
    lineFileClose(&lf);
    }
outputChunk(&pslList, tempDir, midFileCount++);
printf("Processed %d lines into %d temp files\n", totalLineCount, midFileCount);
pslSort2(outDir, tempDir, noHead);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 5)
    usage();
pslSortAcc(argv[1], argv[2], argv[3], &argv[4], argc-4);
return 0;
}
   
