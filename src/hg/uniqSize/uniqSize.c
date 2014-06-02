/* Figure out size of unique parts of genome. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "cheapcgi.h"
#include "glDbRep.h"
#include "sqlNum.h"
#include "hash.h"


boolean stretch = FALSE;

void usage()
/* Print usage and exit. */
{
errAbort(
  "Figure out size of unique parts of genome.\n"
  "usage:\n"
  "   uniqSize ooDir agpFile glFile\n"
  "options:\n"
  "   altFile=other.agp  - if agpFile doesn't exist use altFile\n"
  "   -stretch - show stretch info as well");
}

void getSizes(char *fileName, int *retU, int *retN)
/* Add together sizes in a gold  file */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int lineSize, wordCount;
char *line, *words[16];
int start,end,size;
int u = 0, n = 0;

while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount < 8)
	errAbort("Short line %d of %s\n", lf->lineIx, lf->fileName);
    start = atoi(words[1]) - 1;
    end = atoi(words[2]);
    size = end-start;
    if (words[4][0] == 'N' || words[4][0] == 'U')
	n += size;
    else
	u += size;
    }
lineFileClose(&lf);
*retU = u;
*retN = n;
}

struct intList
/* A list of integers. */
    {
    struct intList *next;
    int n;
    };

int intListCmp(const void *va, const void *vb)
/* Compare two intLists. */
{
const struct intList *a = *((struct intList **)va);
const struct intList *b = *((struct intList **)vb);
return a->n - b->n;
}

void addIntList(struct intList **pList, int n)
/* Add an integer to list. */
{
struct intList *il;
AllocVar(il);
il->n = n;
slAddHead(pList, il);
}

unsigned intListTotal(struct intList *list)
/* Return total of items in list. */
{
unsigned total = 0;
struct intList *il;
for (il = list; il != NULL; il = il->next)
    total += il->n;
return total;
}

int calcN50(struct intList **pList)
/* Calculate N50 (contig size median base is in) for *pList.
 * As a side effect this sorts pList. */
{
unsigned acc = 0, halfTot = intListTotal(*pList)/2;
struct intList *il;

slSort(pList, intListCmp);
for (il = *pList; il != NULL; il = il->next)
    {
    acc += il->n;
    if (acc >= halfTot)
        return il->n;
    }
return 0;
}

struct chromInfo
/* Info on one chromosome. */
    {
    struct chromInfo *next;	/* Next in list. */
    char *name;			/* Chromosome name. */
    int contigCount;		/* Number of contigs. */
    int openCloneGaps;		/* Number of gaps between clones without bridging. */
    int bridgedCloneGaps;	/* Number of gaps between clones with bridging. */
    int openFragGaps;		/* Number of gaps between fragments withough bridging. */
    int bridgedFragGaps;	/* Number of gaps between fragments with bridging. */
    int c50;			/* Contig size median base is in. */
    int s50;			/* Scaffold size median base is in. */
    unsigned baseCount;		/* Number of (non-N) bases. */
    int nCount;			/* Number of N bases. */
    struct intList *contigList;	/* List of contig sizes. */
    struct intList *scaffoldList;	/* List of fragment sizes. */
    int cloneCount;		/* Total clones. */
    int totalStretch;		/* Total amount of clone stretchage. */
    int stretchedClones;	/* Number of clones stretched > 30% */
    int wayStretchedClones;	/* Number of clones stretched > 100% */
    };

struct chromInfo *oneContigInfo(char *fileName)
/* Read agp or gold.N file into chromInfo. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int lineSize, wordCount;
char *line, *words[16];
int start=0,end=0,size;
struct chromInfo *ci;
int contigStart = 0, scaffoldStart = 0;
boolean inContig = FALSE, inScaffold = FALSE;

AllocVar(ci);
while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount < 8)
	errAbort("Short line %d of %s\n", lf->lineIx, lf->fileName);
    if (ci->name == NULL)
	{
        ci->name = cloneString(words[0]);
	ci->contigCount = 1;
	}
    start = atoi(words[1]) - 1;
    end = atoi(words[2]);
    size = end-start;
    if (words[4][0] == 'N' || words[4][0] == 'U')  /* It's a gap line. */
	{
	boolean isOpen = sameWord(words[7], "no");
	/* Keep track of contig and scaffold sizes. */
	if (inContig == TRUE)
	    {
	    inContig = FALSE;
	    addIntList(&ci->contigList, start - contigStart);
	    if (ci->contigList->n <= 0)
	        errAbort("Line %d of %s, contig size %d\n",  lf->lineIx, lf->fileName, start - contigStart);
	    }
	if (isOpen)
	    {
	    if (inScaffold == TRUE)
		{
		inScaffold = FALSE;
		addIntList(&ci->scaffoldList, start - scaffoldStart);
		}
	    }

	/* Keep track of other stats. */
	ci->nCount += size;
	if (sameWord(words[6], "clone"))
	    {
	    if (isOpen)
	        ci->openCloneGaps += 1;
	    else
	        ci->bridgedCloneGaps += 1;
	    }
	else
	    {
	    if (isOpen)
	        ci->openFragGaps += 1;
	    else
	        ci->bridgedFragGaps += 1;
	    }
	}
    else		/* It's not a gap line. */
	{
	/* Keep track of contig and scaffold sizes. */
	if (inContig == FALSE)
	    {
	    contigStart = start;
	    inContig = TRUE;
	    }
	if (inScaffold == FALSE)
	    {
	    scaffoldStart = start;
	    inScaffold = TRUE;
	    }

	/* Keep track of other stats. */
	ci->baseCount += size;
	}
    }
lineFileClose(&lf);
if (inContig)
    {
    addIntList(&ci->contigList, end - contigStart);
    if (ci->contigList->n <= 0)
	errAbort("End of %s, contig size %d\n",  fileName, end - contigStart);
    }
if (inScaffold)
    addIntList(&ci->scaffoldList, end - scaffoldStart);
ci->c50 = calcN50(&ci->contigList);
ci->s50 = calcN50(&ci->scaffoldList);
return ci;
}

struct chromInfo *combineChromInfo(struct chromInfo *ciList, char *name)
/* Create a new chromInfo structure that combines all in list.   Give this
 * a new name. */
{
struct chromInfo *tot, *ci;
struct intList *il;

AllocVar(tot);
tot->name = cloneString(name);
for (ci = ciList; ci != NULL; ci = ci->next)
    {
    tot->contigCount += ci->contigCount;
    tot->openCloneGaps += ci->openCloneGaps;
    tot->bridgedCloneGaps += ci->bridgedCloneGaps;
    tot->openFragGaps += ci->openFragGaps;
    tot->bridgedFragGaps += ci->bridgedFragGaps;
    tot->baseCount += ci->baseCount;
    tot->nCount += ci->nCount;
    for (il = ci->contigList; il != NULL; il = il->next)
	addIntList(&tot->contigList, il->n);
    for (il = ci->scaffoldList; il != NULL; il = il->next)
	addIntList(&tot->scaffoldList, il->n);
    tot->cloneCount += ci->cloneCount;
    tot->totalStretch += ci->totalStretch;
    tot->stretchedClones += ci->stretchedClones;
    tot->wayStretchedClones += ci->wayStretchedClones;
    }
tot->c50 = calcN50(&tot->contigList);
tot->s50 = calcN50(&tot->scaffoldList);
return tot;
}

double percent(double a, double b)
/* percentage of a/b */
{
if (b <= 0)
    return 0.0;
else
    return 100.0*a/b;
}

void printChromInfo(struct chromInfo *ci, FILE *f)
/* Print out chromosome info. */
{
fprintf(f, "%-5s %10u %8d %8d %6d %6d %6d %6d ",
   ci->name, ci->baseCount, ci->c50, ci->s50, ci->openCloneGaps, ci->bridgedCloneGaps, ci->openFragGaps, ci->bridgedFragGaps);
if (stretch)
    fprintf(f, "%10d %4.1f  %4.1f", ci->totalStretch, 
       percent(ci->stretchedClones, ci->cloneCount), percent(ci->wayStretchedClones, ci->cloneCount));
fprintf(f, "\n");
}

void printHeader(FILE *f)
{
fprintf(f, "%-5s %10s %8s %8s %6s %6s %6s %6s %10s %5s %5s\n", 
   "chrom", "base", "N50", "N50", "open", "bridge", "open", "bridge",
   "stretch",  "+30%",  "+100%");
fprintf(f, "%-5s %10s %8s %8s %6s %6s %6s %6s %10s %5s %5s\n", 
   "name", "count", "contig", "scaffold", "clone", "clone", "contig", "contig", "total", "size", "size");
fprintf(f, "-------------------------------------------------------------------------------------\n");
}

struct clone
/* Info on one clone. */
   {
   struct clone *next;
   char *name;	/* Allocated in hash. */
   int start, end;	/* Range of sequence covered. */
   int totalSize;	/* Size of sequence without gaps. */
   };

void addStretchInfo(char *fileName, struct chromInfo *ctg)
/* Add info about how much clones are stretched from gl file. */
{
struct hash *cloneHash = newHash(12);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[4];
struct gl gl;
struct clone *cloneList = NULL, *clone;

while (lineFileRow(lf, row))
    {
    glStaticLoad(row, &gl);
    chopSuffix(gl.frag);
    if ((clone = hashFindVal(cloneHash, gl.frag)) == NULL)
        {
	AllocVar(clone);
	slAddHead(&cloneList, clone);
	hashAddSaveName(cloneHash, gl.frag, clone, &clone->name);
	clone->start = gl.start;
	clone->end = gl.end;
	}
    else
        {
	if (gl.start < clone->start) clone->start = gl.start;
	if (gl.end > clone->end) clone->end = gl.end;
	}
    clone->totalSize += gl.end - gl.start;
    }
for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    int stretchSize = clone->end - clone->start;
    double stretchRatio;
    ctg->cloneCount += 1;
    ctg->totalStretch += stretchSize - clone->totalSize;
    stretchRatio = stretchSize / clone->totalSize;
    if (stretchRatio > 1.3)
        ctg->stretchedClones += 1;
    if (stretchRatio > 2.0)
        ctg->wayStretchedClones += 1;
    }
lineFileClose(&lf);
hashFree(&cloneHash);
slFreeList(&cloneList);
}

void uniqSize(char *ooDir, char *agpFile, char *glFile, char *altFile)
/* Figure out unique parts of genome from all the
 * gold.22 files in ooDir */
{
struct fileInfo *chromDirs, *chromEl;
struct fileInfo *contigDirs, *contigEl;
char subDir[512];
struct chromInfo *ciList = NULL, *ci, *ciTotal;

chromDirs = listDirX(ooDir, "*", FALSE);
for (chromEl = chromDirs; chromEl != NULL; chromEl = chromEl->next)
    {
    char *chromName = chromEl->name;
    int dirNameLen = strlen(chromName);
    if (dirNameLen > 0 && dirNameLen <= 2 && chromEl->isDir)
	{
	struct chromInfo *ctgList = NULL, *ctg;
	sprintf(subDir, "%s/%s", ooDir, chromName);
	contigDirs = listDirX(subDir, "NT*", FALSE);
	for (contigEl = contigDirs; contigEl != NULL; contigEl = contigEl->next)
	    {
	    if (contigEl->isDir)
		{
		int nSize, uSize;
		char *contigName = contigEl->name;
		char fileName[512];
		sprintf(fileName, "%s/%s/%s", subDir, contigName, agpFile);
		if (!fileExists(fileName) && altFile != NULL)
		    sprintf(fileName, "%s/%s/%s", subDir, contigName, altFile);
		if (fileExists(fileName))
		    {
		    ctg = oneContigInfo(fileName);
		    slAddHead(&ctgList, ctg);
		    getSizes(fileName, &uSize, &nSize);
		    sprintf(fileName, "%s/%s/%s", subDir, contigName, glFile);
		    if (fileExists(fileName))
			addStretchInfo(fileName, ctg);
		    }
		else
		    {
		    warn("No %s in %s/%s", agpFile, subDir, contigName);
		    }
		}
	    }
	slFreeList(&contigDirs);
	ci = combineChromInfo(ctgList, chromName);
	slAddHead(&ciList, ci);
	}
    }
slReverse(&ciList);

printHeader(stdout);
for (ci = ciList; ci != NULL; ci = ci->next)
    printChromInfo(ci, stdout);
ciTotal = combineChromInfo(ciList, "total");
printChromInfo(ciTotal, stdout);
}


int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
stretch = cgiBoolean("stretch");
if (argc != 4)
    usage();
uniqSize(argv[1], argv[2], argv[3], cgiOptionalString("altFile"));
return 0;
}
