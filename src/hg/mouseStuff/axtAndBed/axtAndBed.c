/* axtAndBed - Intersect an axt with a bed file and output axt.. 
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "binRange.h"
#include "axt.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtAndBed - Intersect an axt with a bed file and output axt.\n"
  "usage:\n"
  "   axtAndBed in.axt in.bed out.axt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct chromInfo
/* Track info on one chromosome. */
    {
    struct chromInfo *next;
    char *name;			/* Allocated in hash. */
    int maxEnd;			/* Maximum end of any item.  */
    struct binKeeper *bk;	/* Index items here. */
    };

struct hash *readBed(char *fileName)
/* Read in bed file into hash of binKeepers keyed by
 * target. */
{
struct lineFile *lf = NULL;
struct hash *hash = newHash(0);
char *row[3];
struct chromInfo *ciList = NULL, *ci;
int count = 0, chromCount = 0;

/* Make first pass through just figuring out maximum size
 * of each chromosome info. */
lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, row))
    {
    char *chrom = row[0];
    int e = lineFileNeedNum(lf, row, 2);
    ci = hashFindVal(hash, chrom);
    if (ci == NULL)
        {
	AllocVar(ci);
	hashAddSaveName(hash, chrom, ci, &ci->name);
	slAddHead(&ciList, ci);
	++chromCount;
	}
    if (e > ci->maxEnd)
        ci->maxEnd = e;
    ++count;
    }
lineFileClose(&lf);

/* Allocate binKeeper on each chromosome. */
for (ci = ciList; ci != NULL; ci = ci->next)
    {
    ci->bk = binKeeperNew(0, ci->maxEnd);
    }

/* Make second pass filling in binKeeper */
lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, row))
    {
    char *chrom = row[0];
    int s = lineFileNeedNum(lf, row, 1);
    int e = lineFileNeedNum(lf, row, 2);
    ci = hashMustFindVal(hash, chrom);
    binKeeperAdd(ci->bk, s, e, NULL);
    }
lineFileClose(&lf);
printf("Read %d items in %d target chromosomes from %s\n", 
	count, chromCount, fileName);
return hash;
}

void axtAndBed(char *inAxt, char *inBed, char *outAxt)
/* axtAndBed - Intersect an axt with a bed file and output axt.. */
{
struct hash *tHash = readBed(inBed); /* target keyed, binKeeper value */
struct lineFile *lf = lineFileOpen(inAxt, TRUE);
struct axt *axt;
struct binElement *list = NULL, *el;
FILE *f = mustOpen(outAxt, "w");
struct axtScoreScheme *ss = axtScoreSchemeDefault();

while ((axt = axtRead(lf)) != NULL)
    {
    struct chromInfo *ci = hashFindVal(tHash, axt->tName);
    if (ci != NULL)
	{
	list = binKeeperFind(ci->bk, axt->tStart, axt->tEnd);
	if (list != NULL)
	    {
	    /* Flatten out any overlapping elements by projecting them
	     * onto a 0/1 valued character array and then looking for 
	     * runs of 1 in this array. */
	    int tStart = axt->tStart;
	    int tEnd = axt->tEnd;
	    int tSize = tEnd - tStart;
	    int i, s = 0;
	    char c, lastC = 0;
	    char *merger = NULL;
	    AllocArray(merger, tSize+1);
	    for (el = list; el != NULL; el = el->next)
		{
		int s = el->start - tStart;
		int e = el->end - tStart;
		int sz;
		if (s < 0) s = 0;
		if (e > tSize) e = tSize;
		sz = e - s;
		if (sz > 0)
		    memset(merger + s, 1, sz);
		}
	    for (i=0; i<=tSize; ++i)
		{
		c = merger[i];
		if (c && !lastC)
		    {
		    s = i;
		    lastC = c;
		    }
		else if (!c && lastC)
		    {
		    axtSubsetOnT(axt, s+tStart, i+tStart, ss, f);
		    lastC = c;
		    }
		}
	    freez(&merger);
	    slFreeList(&list);
	    }
	}
    axtFree(&axt);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
axtAndBed(argv[1],argv[2],argv[3]);
return 0;
}
