/* Figure out size of unique parts of genome. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "cheapcgi.h"

void usage()
/* Print usage and exit. */
{
errAbort(
  "Figure out size of unique parts of genome.\n"
  "usage:\n"
  "   uniqSize ooDir agpFile\n"
  "options:\n"
  "   altFile=other.agp  - if agpFile doesn't exist use altFile");
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
    if (words[4][0] == 'N')
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
    };

struct chromInfo *oneContigInfo(char *fileName)
/* Read agp or gold.N file into chromInfo. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int lineSize, wordCount;
char *line, *words[16];
int start,end,size;
int u = 0, n = 0;
struct chromInfo *ci;
struct intList *il;
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
    if (words[4][0] == 'N')		/* It's a gap line. */
	{
	boolean isOpen = sameWord(words[7], "no");
	/* Keep track of contig and scaffold sizes. */
	if (inContig == TRUE)
	    {
	    inContig = FALSE;
	    addIntList(&ci->contigList, start - contigStart);
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
    addIntList(&ci->contigList, end - contigStart);
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
    }
tot->c50 = calcN50(&tot->contigList);
tot->s50 = calcN50(&tot->scaffoldList);
return tot;
}

void printChromInfo(struct chromInfo *ci, FILE *f)
/* Print out chromosome info. */
{
fprintf(f, "%-5s %10u %8d %8d %6d %6d %6d %6d\n", 
   ci->name, ci->baseCount, ci->c50, ci->s50, ci->openCloneGaps, ci->bridgedCloneGaps, ci->openFragGaps, ci->bridgedFragGaps);
}

void printHeader(FILE *f)
{
fprintf(f, "%-5s %10s %8s %8s %6s %6s %6s %6s\n", 
   "chrom", "base", "N50", "N50", "open", "bridge", "open", "bridge");
fprintf(f, "%-5s %10s %8s %8s %6s %6s %6s %6s\n", 
   "name", "count", "contig", "scaffold", "clone", "clone", "contig", "contig");
fprintf(f, "--------------------------------------------------------------\n");
}

void uniqSize(char *ooDir, char *agpFile, char *altFile)
/* Figure out unique parts of genome from all the
 * gold.22 files in ooDir */
{
struct fileInfo *chromDirs, *chromEl;
struct fileInfo *contigDirs, *contigEl;
char subDir[512];
unsigned long total = 0;
unsigned long totalN = 0;
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
	contigDirs = listDirX(subDir, "ctg*", FALSE);
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
if (argc != 3)
    usage();
uniqSize(argv[1], argv[2], cgiOptionalString("altFile"));
return 0;
}
