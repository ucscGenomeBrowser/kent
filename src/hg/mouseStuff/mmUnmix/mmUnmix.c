/* mmUnmix - Help identify human contamination in mouse and vice versa.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "glDbRep.h"
#include "psl.h"
#include "xAli.h"
#include "binRange.h"
#include "portable.h"
#include "bits.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mmUnmix - Help identify human contamination in mouse and vice versa.\n"
  "usage:\n"
  "   mmUnmix xAli.pslx fragDir mouseBad.out humanBad.out\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct chrom
/* A (human) chromosome's worth of info. */
   {
   struct chrom *next;	/* Next in list. */
   char *name;		/* chr1, chr2, etc. */
   int size;		/* Chromosome size. */
   struct binKeeper *ali;  /* Alignments sorted by bin. */
   struct binKeeper *frag; /* Human contigs sorted by bin. */
   };

struct chrom *chromNew(char *name, int size)
/* Allocate a new chromosome. */
{
struct chrom *chrom;
AllocVar(chrom);
chrom->name = cloneString(name);
chrom->size = size;
chrom->ali = binKeeperNew(0, size);
chrom->frag = binKeeperNew(0, size);
return chrom;
}

void readAli(char *fileName, struct hash *chromHash, struct chrom **chromList)
/* Read alignment file into bin-keepers of chromosomes. */
{
struct lineFile *lf = pslFileOpen(fileName);
char *row[23];
int count = 0;

while (lineFileRow(lf, row))
    {
    struct xAli *xa = xAliLoad(row);
    struct chrom *chrom = hashFindVal(chromHash, xa->tName);
    if (chrom == NULL)
        {
	chrom = chromNew(xa->tName, xa->tSize);
	slAddHead(chromList, chrom);
	hashAdd(chromHash, chrom->name, chrom);
	}
    binKeeperAdd(chrom->ali, xa->tStart, xa->tEnd, xa);
    ++count;
    }
slReverse(chromList);
uglyf("%d alignments in %d chromosomes\n", count, slCount(*chromList));
}

void readFragFile(char *fileName, struct binKeeper *bk)
/* Read frag file into bk. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[5];
struct gl *gl;
int count = 0;

while (lineFileRow(lf, row))
    {
    gl = glLoad(row+1);	/* Skip bin field. */
    binKeeperAdd(bk, gl->start, gl->end, gl);
    ++count;
    }
lineFileClose(&lf);
}

void readFrags(char *fragDir, struct hash *chromHash)
/* Read all clone fragment files in dir. */
{
struct fileInfo *fileList = listDirX(fragDir, "*.txt", TRUE);
struct fileInfo *fileInfo;
char root[128];
struct chrom *chrom;
char *suffix = "_gl";

for (fileInfo = fileList; fileInfo != NULL; fileInfo = fileInfo->next)
    {
    char *fileName = fileInfo->name;
    splitPath(fileName, NULL, root, NULL);
    if (endsWith(root, suffix))
        {
	char *s = stringIn(suffix, root);
	*s = 0;
	}
    chrom = hashFindVal(chromHash, root);
    if (chrom != NULL)
        {
	readFragFile(fileName, chrom->frag);
	}
    }
uglyf("Read %d frag files in %s\n", slCount(fileList), fragDir);
}

boolean paintOneSuspect(struct xAli *xa, Bits *bits)
/* Paint bits that look suspect. */
{
int tOffset = xa->tStart;
int tSize = xa->tEnd - tOffset;
int *scores;
int blockIx, i;
int winSize = 200, winMinScore = 190, winScore = 0;
boolean lastSet = FALSE, anySet = FALSE;

/* If whole alignment less than our window size don't bother. */
if (tSize < winSize)
    return FALSE;		

/* Set up a scoring array that is size of target piece
 * of alignment and has -1 everywhere. */
assert(tSize > 0 && tSize < 1000000);
AllocArray(scores, tSize);
for (i=0; i < tSize; ++i)
   scores[i] = -1;

/* Set items in score array to 1 where there is a match. */
for (blockIx=0; blockIx < xa->blockCount; ++blockIx)
    {
    int btOffset = xa->tStarts[blockIx] - tOffset;
    DNA *q = xa->qSeq[blockIx];
    DNA *t = xa->tSeq[blockIx];
    int blockSize = xa->blockSizes[blockIx];
    for (i=0; i<blockSize; ++i)
	{
        if (q[i] == t[i])
	    scores[i + btOffset] = 1;
	}
    }


/* Examine first window. */
for (i=0; i<winSize; ++i)
    winScore += scores[i];
if (winScore >= winMinScore)
    {
    bitSetRange(bits, tOffset, winSize);
    anySet = lastSet = TRUE;
    }

/* Slide window and set bits in windows that score high enough. */
for (i = winSize; i < tSize; ++i)
    {
    winScore -= scores[i-winSize];
    winScore += scores[i];
    if (winScore >= winMinScore)
        {
	if (lastSet)
	    {
	    bitSetOne(bits, tOffset+i);
	    }
	else
	    {
	    bitSetRange(bits, tOffset + i - winSize + 1, winSize);
	    }
	anySet = lastSet = TRUE;
	}
    else
        {
	lastSet = FALSE;
	}
    }
return anySet;
}

void paintSuspectBits(struct binKeeper *bk, Bits *bits, struct xAli **suspectList)
/* Set bits that correspond to suspect alignments. */
{
struct binElement *el, *elList = binKeeperFindAll(bk);
struct xAli *xa;
for (el = elList; el != NULL; el = el->next)
    {
    xa = el->val;
    if (paintOneSuspect(xa, bits))
       {
       slAddHead(suspectList, xa);
       }
    }
slFreeList(&elList);
}

struct clone
/* All the fragments in a clone. */
    {
    struct clone *next;
    char *name;		/* Allocated in hash */
    struct gl *frags;	/* All fragments for this clone. */
    int start, end;	/* Start/end in genome. */
    int totalFragSize;	/* Size of fragments (distinct from end-start) */
    };

struct clone *makeClones(struct binKeeper *bk, struct hash *hash)
/* Fill hash of clones. */
{
struct binElement *el, *elList = binKeeperFindAll(bk);
struct gl *gl;
struct clone *clone, *cloneList = NULL;
char cloneName[256];

for (el = elList; el != NULL; el = el->next)
    {
    gl = el->val;
    strcpy(cloneName, gl->frag);
    chopSuffix(cloneName);
    if ((clone = hashFindVal(hash, cloneName)) == NULL)
        {
	AllocVar(clone);
	hashAddSaveName(hash, cloneName, clone, &clone->name);
	slAddHead(&cloneList, clone);
	clone->start = gl->start;
	clone->end = gl->end;
	}
    if (clone->start > gl->start) clone->start = gl->start;
    if (clone->end < gl->end) clone->end = gl->end;
    clone->totalFragSize += gl->end - gl->start;
    slAddHead(&clone->frags, gl);
    }
slFreeList(&elList);
slReverse(&cloneList);
return cloneList;
}


void judgeClones(struct chrom *chrom, Bits *bits, struct hash *badHash, 
	struct xAli *suspectList, FILE *badMouse, FILE *badHuman)
/* Look for contamination that is restricted to clone */
{
struct hash *cloneHash = newHash(0);
struct clone *cloneList = NULL, *clone;
int fragBad, cloneBad;

cloneList = makeClones(chrom->frag, cloneHash);
for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    int cloneBad = 0, fragBad;
    struct gl *gl;
    for (gl = clone->frags; gl != NULL; gl = gl->next)
        {
	fragBad = bitCountRange(bits, gl->start, gl->end - gl->start);
	cloneBad += fragBad;
	}
    if (cloneBad >= 800)
        {
	int cloneSize =   clone->totalFragSize;
#ifdef SOMEDAY
	int rs, re, badNeighbors;
	rs = clone->start - 5000;
	re = clone->end + 5000;
	if (rs < 0) rs = 0;
	if (re > chrom->size) re = chrom->size;
	badNeighbors = bitCountRange(bits, rs, re-rs);
	if (cloneBad > 5000 || badNeighbors - cloneBad < 40)
	    {
	    if (cloneBad > cloneSize * 0.60)
		{
		fprintf(badHuman, "clone\t%s\t%4.2f\n", clone->name, 100.0 * cloneBad / cloneSize);
		}
	    else
		{
		fprintf(badMouse, "clone\t%s\t%4.2f\n", clone->name, 100.0 * cloneBad / cloneSize);
		}
	    }
#endif
        fprintf(badHuman, "%d\t%s\t%4.2f%%\n", cloneBad, clone->name, 100.0 * cloneBad/cloneSize);
	}
    }
}

void unmixChrom(struct chrom *chrom, struct hash *badHash, FILE *badMouse, FILE *badHuman)
/* Find contamination in chromosome. */
{
Bits *bits = bitAlloc(chrom->size);
struct xAli *suspectList = NULL;

paintSuspectBits(chrom->ali, bits, &suspectList);
uglyf("%d suspect bits in chromosome %s\n", bitCountRange(bits, 0, chrom->size), chrom->name);
judgeClones(chrom, bits, badHash, suspectList, badMouse, badHuman);

/* Look for contamination that is restricted to clone */
// judgeHumanFrags(chrom, badHash, suspectList, badHuman);
// judgeMouseFrags(chrom, badHash, suspectList, badMouse, badHuman);
bitFree(&bits);
}

void mmUnmix(char *xaliName, char *fragDir, char *mouseContam, char *humanContam)
/* mmUnmix - Help identify human contamination in mouse and vice versa.. */
{
struct hash *chromHash = newHash(8);
struct chrom *chromList = NULL, *chrom;
struct hash *badHash = newHash(0);
FILE *mm = mustOpen(mouseContam, "w");
FILE *hs = mustOpen(humanContam, "w");

readAli(xaliName, chromHash, &chromList);
readFrags(fragDir, chromHash);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    unmixChrom(chrom, badHash, mm, hs);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
mmUnmix(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
