/* ooCloneOverlaps - Find overlaps between clones in FPC contig based on self.psl. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "psl.h"
#include "obscure.h"
#include "hCommon.h"
#include "bits.h"


struct clone
/* Information on one clone. */
    {
    struct clone *next;	/* Next in list. */
    char *name;		/* Clone name (accession, no version). Allocated in hash */
    char *version;	/* Version part of name. */
    int size;           /* Clone size. */
    int phase;		/* HTG phase.  3 - finished, 2 - ordered, 1 - draft, 0 - survey. */
    char *firstFrag;	/* Name of first fragment in fa file. */
    char *lastFrag;	/* Name of last fragment in fa file. */
    char *faFileName;   /* File with clone sequence. */
    char *startFrag;	/* Known start fragment name or NULL if none. */
    char startSide;	/* L or R depending which side of frag this is on. */
    char *endFrag;	/* Known end frag name or NULL if none. */
    char endSide;	/* L or R depending which side of frag this is on. */
    };

struct ocp
/* Overlapping clone pair. */
    {
    struct ocp *next;		/* Next in list. */
    char *name;			/* Name (allocated in hash).  Form is a^b.  */
    struct clone *a, *b;        /* Two clones in pair - in alphabetical order. */
    struct psl *pslList;        /* List of alignments that support overlap. */
    int overlap;		/* Amount of overlap between clones. */
    int sloppyOverlap;		/* Amount of reduces stringency overlap between clones. */
    int obliviousOverlap;	/* Amount of overlap oblivious to tails. */
    char aHitS;			/* Values Y, N, and ? for corresponding end hitting, not hitting and unknown. */
    char aHitT;			/* Values Y, N, and ? for corresponding end hitting, not hitting and unknown. */
    char bHitS;			/* Values Y, N, and ? for corresponding end hitting, not hitting and unknown. */
    char bHitT;			/* Values Y, N, and ? for corresponding end hitting, not hitting and unknown. */
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ooCloneOverlaps - Find overlaps between clones in FPC contig based on self.psl\n"
  "usage:\n"
  "   ooCloneOverlaps contigDir\n"
  "This needs the files cloneInfo, self.psl, and cloneEnds to exist in contigDir.\n"
  "This will create the file cloneEnds in contigDir\n");
}

struct hash *makeCloneSizeHash(char *fileName)
/* Read in clone sizes from file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
char *row[2];
int size;

while (lineFileRow(lf, row))
    {
    size = atoi(row[1]);
    if (size == 0)
        errAbort("Expecting clone size line %d of %s", lf->lineIx, lf->fileName);
    hashAddUnique(hash, row[0], intToPt(size));
    }
lineFileClose(&lf);
return hash;
}

int findCloneSize(char *cloneName, struct hash *sizeHash)
/* Find size of clone given name. */
{
char name[128];
char *pt;

strcpy(name, cloneName);
chopSuffix(name);
pt = hashMustFindVal(sizeHash, name);
return ptToInt(pt);
}

boolean loadCloneInfo(char *fileName,
	struct clone **retList, struct hash **retHash)
/* Load list of clones from fileName.  Return FALSE if can't
 * because file doesn't exist or is zero length. */
{
struct lineFile *lf = lineFileMayOpen(fileName, TRUE);
struct clone *cloneList = NULL, *clone;
struct hash *hash = NULL;
char *row[7];

if (lf == NULL)
    return FALSE;
hash = newHash(10);
while (lineFileRow(lf, row))
    {
    AllocVar(clone);
    slAddHead(&cloneList, clone);
    hashAddSaveName(hash, row[0], clone, &clone->name);
    clone->version = cloneString(row[1]);
    clone->size = lineFileNeedNum(lf, row, 2);
    clone->phase = lineFileNeedNum(lf, row, 3);
    clone->firstFrag = cloneString(row[4]);
    clone->lastFrag = cloneString(row[5]);
    clone->faFileName = cloneString(row[6]);
    }
lineFileClose(&lf);
slReverse(&cloneList);
*retHash = hash;
*retList = cloneList;
return TRUE;
}

char getLR(struct lineFile *lf, char *words[], int wordIx)
/* Make sure that words[wordIx] is L or R, and return it. */
{
char c = words[wordIx][0];
if (c == 'R' || c == 'L' || c == '?')
    return c;
else
    {
    errAbort("Expecting L or R in field %d line %d of %s",
       wordIx+1, lf->lineIx, lf->fileName);
    return '?';
    }
}

void loadCloneEndInfo(char *fileName, struct hash *cloneHash)
/* Load up cloneEnds file into list/hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct clone *clone;
char *words[16], *line;
int wordCount;
char fragName[128];
int singles = 0, doubles = 0;


while ((wordCount = lineFileChop(lf, words)) > 0)
    {
    if (wordCount != 7 && wordCount != 9)
        errAbort("Expecting 7 or 9 words line %d of %s", lf->lineIx, lf->fileName);
    clone = hashMustFindVal(cloneHash, words[0]);
    if (!sameString(clone->version, words[1]))
        errAbort("Version mismatch %s.%s and %s.s", clone->name, clone->version, clone->name, words[1]);
    sprintf(fragName, "%s%s_%s", clone->name, clone->version, words[5]);
    clone->startFrag = cloneString(fragName);
    clone->startSide = getLR(lf, words, 6);
    if (wordCount == 9)
        {
	sprintf(fragName, "%s%s_%s", clone->name, clone->version, words[7]);
	clone->endFrag = cloneString(fragName);
	clone->endSide = getLR(lf, words, 8);
	++doubles;
	}
    else
        {
	++singles;
	}
    }
lineFileClose(&lf);
printf("%d clones with 2 ends, %d clones with 1 end in %s\n",
	doubles, singles, fileName);
}

void fillInOrdered(struct clone *cloneList)
/* Fill in end info on finished and ordered clones if not already
 * present. */
{
struct clone *clone;
int filledCount = 0;

for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    if (clone->phase >= 2)
        {
	if (clone->startFrag == NULL)
	    {
	    ++filledCount;
	    clone->startFrag = cloneString(clone->firstFrag);
	    clone->startSide = 'L';
	    clone->endFrag = cloneString(clone->lastFrag);
	    clone->endSide = 'R';
	    }
	}
    }
printf("Filled in %d ends for phase 2 and 3 clones\n", filledCount);
}

char *ocpHashName(char *a, char *b)
/* Return name of ocp associated with a and b */
{
static char buf[256];
if (strcmp(a,b) < 0)
    sprintf(buf, "%s^%s", a, b);
else
    sprintf(buf, "%s^%s", b, a);
return buf;
}

struct ocp *ocpNew(char *aName, char *bName, struct hash *cloneHash, struct hash *ocpHash)
/* Return new ocp based on clones with aName and bName. */
{
struct clone *a, *b;
struct ocp *ocp;
char *ocpName;

if (strcmp(aName, bName) > 0)
    {
    char *temp = aName;
    aName = bName;
    bName = temp;
    }
ocpName = ocpHashName(aName, bName);
AllocVar(ocp);
hashAddSaveName(ocpHash, ocpName, ocp, &ocp->name);
ocp->a = hashMustFindVal(cloneHash, aName);
ocp->b = hashMustFindVal(cloneHash, bName);
return ocp;
}

struct ocp *ocpFind(char *aName, char *bName, struct hash *ocpHash)
/* Find OCP in hash if it exists. */
{
return hashFindVal(ocpHash, ocpHashName(aName, bName));
}

struct ocp *ocpMustFind(char *aName, char *bName, struct hash *ocpHash)
/* Find OCP in hash, squawk and die if it's not there . */
{
return hashMustFindVal(ocpHash, ocpHashName(aName, bName));
}

boolean filterPsl(struct psl *psl, int maxTail, int maxBad)
/* Returns TRUE if psl looks good. */
{
int startTail, endTail;
int milliBad;
int minSize;
int halfMatch;

pslTailSizes(psl, &startTail, &endTail);
halfMatch = (psl->match + psl->repMatch)/2;
if (startTail > maxTail || endTail > maxTail)
    return FALSE;
milliBad = pslCalcMilliBad(psl, FALSE);
if (milliBad > maxBad)
    return FALSE;
if (psl->match < 50)
    {
    if (startTail > 50 || endTail > 50)
        return FALSE;
    if (milliBad > 10)
        return FALSE;
    }
return TRUE;
}

void assignPsls(char *fileName, struct hash *cloneHash, 
	struct ocp **retOcpList, struct hash **retOcpHash)
/* Read in .psl file and make up ocp structures based on good looking
 * non-forking alignments. */
{
struct lineFile *lf = pslFileOpen(fileName);
struct hash *ocpHash = newHash(0);
struct ocp *ocpList = NULL, *ocp;
struct psl *psl;

while ((psl = pslNext(lf)) != NULL)
    {
    if (filterPsl(psl, 1000000, 50))
        {
	char qName[128], tName[128];
	fragToCloneName(psl->qName, qName);
	fragToCloneName(psl->tName, tName);
	if (!sameString(qName, tName))
	    {
	    if ((ocp = ocpFind(qName, tName, ocpHash)) == NULL)
		{
		ocp = ocpNew(qName, tName, cloneHash, ocpHash);
		slAddHead(&ocpList, ocp);
		}
	    slAddHead(&ocp->pslList, psl);
	    }
	else
	    {
	    pslFree(&psl);
	    }
	}
    else
        {
	pslFree(&psl);
	}
    }
lineFileClose(&lf);
slReverse(&ocpList);
*retOcpList = ocpList;
*retOcpHash = ocpHash;
}

struct frag
/* A clone fragment.  (A temporary structure here, just used
 * while calculating overlap). */
    {
    struct frag *next;		/* Next in list. */
    char *name;			/* Fragment name. Allocated in hash. */
    int size;			/* Size in bases. */
    Bits *overlaps;		/* Array with one element for each base. 
                                 * set to true if base part of overlap. */
    };

int calcCloneOverlap(struct clone *a, struct clone *b, struct psl *pslList, 
	int maxTail, int minScore)
/* Calculate overlap between clones a and b based on pslList, making
 * sure not to count overlapping bases in a more than once. */
{
struct hash *fragHash = newHash(8);
struct frag *fragList = NULL, *frag;
char cloneName[128], *fragName;
struct psl *psl;
int start, end, size, i;
int overlap = 0;
bool *o;

for (psl = pslList; psl != NULL; psl = psl->next)
    {
    if (!filterPsl(psl, maxTail, minScore))
        continue;
    fragToCloneName(psl->qName, cloneName);
    if (sameString(cloneName, a->name))
        {
	size = psl->qSize;
	start = psl->qStart;
	end = psl->qEnd;
	fragName = psl->qName;
	}
    else 
        {
	assert(sameString(cloneName, b->name));
	size = psl->tSize;
	start = psl->tStart;
	end = psl->tEnd;
	fragName = psl->tName;
	}
    if ((frag = hashFindVal(fragHash, fragName)) == NULL)
        {
	AllocVar(frag);
	slAddHead(&fragList, frag);
	hashAddSaveName(fragHash, fragName, frag, &frag->name);
	frag->size = size;
	frag->overlaps = bitAlloc(size);
	}
    assert(end > start);
    bitSetRange(frag->overlaps, start, end-start);
    }
for (frag = fragList; frag != NULL; frag = frag->next)
    {
    overlap += bitCountRange(frag->overlaps, 0, frag->size);
    bitFree(&frag->overlaps);
    }
slFreeList(&fragList);
freeHash(&fragHash);
return overlap;
}

char checkEndOverlap(char *fragName, char side, struct psl *pslList, 
    struct clone *a, struct clone *b, int overlap)
/* Return ?, Y or N depending if fragName is defined, and 
 * side of frag overlaps according to pslList. */
{
struct psl *psl;
int start, end, size;
char qName[128], tName[128];
int endSize = 1500;
Bits *endBits;
char answer = 'N';
int s, e, w, beginOfEnd;
int uniqHits = 0, repHits = 0;
int bitCount;
boolean goodOverlap = FALSE, okOverlap = FALSE;
boolean oneOk = FALSE, oneGood = FALSE;
boolean bothOnePiece = (sameString(a->firstFrag, a->lastFrag) && sameString(b->firstFrag, b->lastFrag));


if (fragName == NULL)
    return '?';
if (side == '?')
    return '?';
endBits = bitAlloc(endSize);
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    oneOk = filterPsl(psl, 50000, 30);
    if (!oneOk)
        continue;
    oneGood = filterPsl(psl, 450, 20);
    size = 0;
    if (sameString(fragName, psl->qName))
        {
	start = psl->qStart;
	end = psl->qEnd;
	size = psl->qSize;
	}
    else if (sameString(fragName, psl->tName))
        {
	start = psl->tStart;
	end = psl->tEnd;
	size = psl->tSize;
	}
    if (size > 0)
        {
	//uglyf(" psl: %s %s, start %d, end %d, size %d\n", psl->qName, psl->tName, start, end, size);
	if (endSize > (size>>1))
	    endSize = (size>>1);
	if (side == 'L')
	    {
	    s = start;
	    e = min(end, endSize);
	    w = e-s;
	    }
	else 
	    {
	    assert(side == 'R');
	    beginOfEnd = size - endSize;
	    s = max(start, beginOfEnd);
	    e = min(end, size);
	    w = e-s;
	    s -= beginOfEnd;
	    }
	if (w > 0)
	    {
	    bitSetRange(endBits, s, w);
	    uniqHits += psl->match;
	    repHits += psl->repMatch;
	    goodOverlap |= oneGood;
	    okOverlap |= oneOk;
	    }
	}
    }
bitCount = bitCountRange(endBits, 0, endSize);
if (bothOnePiece && (side == 'L' || side == 'R'))
    {
    /* Here's some special case code mostly to make small (~3k)
     * finished clones fit in. */
    int offEnd = 500;
    int s, e;

    if (a->phase == 3 && b->phase == 3)
        offEnd = 200;
    if (side == 'L')
	{
	s = 0;
	e = min(offEnd, endSize);
	}
    else
        {
	s = max(0, endSize-offEnd);
	e = endSize;
	}
    bitCount = bitCountRange(endBits, s, e-s);
    bitFree(&endBits);
    if (!goodOverlap)
	{
        return 'N';
	}
    if (bitCount >= 90 || (bitCount >= 32 && uniqHits >= 20))
	{
        return 'Y';
	}
    else if (bitCount <= 0)
	{
        return 'N';
	}
    else
	{
        return '?';
	}
    }
bitFree(&endBits);

//uglyf(" bitCount %d, uniqHits %d, repHits %d\n", bitCount, uniqHits, repHits);
if (bitCount >= 35)
    {
    if (bitCount >= 90 && (uniqHits >= 100 || uniqHits + repHits > 1000))	/* Each hit appears twice in psl list. */
	answer = 'Y';
    else
	{
	psl = pslList;
	if (a->phase == 3 && b->phase == 3 && (bitCount >= 90 || uniqHits >= 20))
	    answer = 'Y';
	else
	    {
	    answer = '?';
	    }
	}
    }
return answer;
}

void calcOverlaps(struct ocp *ocpList, struct hash *cloneHash)
/* Calculate extent of overlaps between clones based on ocp->pslList. */
{
struct ocp *ocp;
int abOverlap, baOverlap;
struct clone *a, *b;

for (ocp = ocpList; ocp != NULL; ocp = ocp->next)
    {
    //uglyf("Calculating overlap size and ends for %s\n", ocp->name);
    a = ocp->a;
    b = ocp->b;
    abOverlap = calcCloneOverlap(a, b, ocp->pslList, 450, 20);
    baOverlap = calcCloneOverlap(b, a, ocp->pslList, 450, 20);
    ocp->overlap = min(abOverlap, baOverlap);

    abOverlap = calcCloneOverlap(a, b, ocp->pslList, 10000, 30);
    baOverlap = calcCloneOverlap(b, a, ocp->pslList, 10000, 30);
    ocp->sloppyOverlap = min(abOverlap, baOverlap);

    abOverlap = calcCloneOverlap(a, b, ocp->pslList, 1000000, 20);
    baOverlap = calcCloneOverlap(b, a, ocp->pslList, 1000000, 20);
    ocp->obliviousOverlap = min(abOverlap, baOverlap);

    ocp->aHitS = checkEndOverlap(a->startFrag, a->startSide, ocp->pslList, a, b, ocp->overlap);
    ocp->aHitT = checkEndOverlap(a->endFrag, a->endSide, ocp->pslList, a, b, ocp->overlap);
    ocp->bHitS = checkEndOverlap(b->startFrag, b->startSide, ocp->pslList, b, a, ocp->overlap);
    ocp->bHitT = checkEndOverlap(b->endFrag, b->endSide, ocp->pslList, b, a, ocp->overlap);
    }
}

void writeOverlaps(char *fileName, struct ocp *ocpList)
/* Write out overlaps in list to file. */
{
FILE *f = mustOpen(fileName, "w");
struct ocp *ocp;

fprintf(f,
    "#aName   \taSize\taHitS\taHitT\tbName     \tbSize\tbHitS\tbHitT\tStrict\tLoose\tObliv.\tExtra\n");
for (ocp = ocpList; ocp != NULL; ocp = ocp->next)
    {
    if (ocp->a->phase == 0 || ocp->b->phase == 0)
        continue;
    fprintf(f, "%s\t%d\t%c\t%c\t%s\t%d\t%c\t%c\t\%d\t%d\t%d\t0\n", 
    	ocp->a->name, ocp->a->size, ocp->aHitS, ocp->aHitT,
	ocp->b->name, ocp->b->size, ocp->bHitS, ocp->bHitT, ocp->overlap,
	ocp->sloppyOverlap, ocp->obliviousOverlap);
    }
fclose(f);
}

void ooCloneOverlaps(char *contigDir)
/* ooCloneOverlaps - Find overlaps between clones in FPC contig based on self.psl. */
{
struct clone *cloneList = NULL;
struct hash *cloneHash = NULL;
struct ocp *ocpList = NULL;
struct hash *ocpHash = NULL;
char fileName[512];

sprintf(fileName, "%s/cloneInfo", contigDir);
if (!loadCloneInfo(fileName, &cloneList, &cloneHash))
    return;
printf("Processing %d clones in %s\n", slCount(cloneList), fileName);

sprintf(fileName, "%s/cloneEnds", contigDir);
loadCloneEndInfo(fileName, cloneHash);
fillInOrdered(cloneList);

sprintf(fileName, "%s/self.psl", contigDir);
assignPsls(fileName, cloneHash, &ocpList, &ocpHash);
printf("Found %d overlapping clone pairs from %s\n", slCount(ocpList), fileName);
calcOverlaps(ocpList, cloneHash);

sprintf(fileName, "%s/cloneOverlap", contigDir);
writeOverlaps(fileName, ocpList);
printf("Wrote overlaps to %s\n", fileName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
ooCloneOverlaps(argv[1]);

#ifdef SOON
ooCloneOverlaps("/projects/hg/gs.5/oo.21/18/ctg12716");
#endif
return 0;
}
