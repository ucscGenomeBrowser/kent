/* chainFilter - Filter chain files. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chainBlock.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainFilter - Filter chain files.  Output goes to standard out.\n"
  "usage:\n"
  "   chainFilter file(s)\n"
  "options:\n"
  "   -q=chr1,chr2 - restrict query side sequence to those named\n"
  "   -notQ=chr1,chr2 - restrict query side sequence to those not named\n"
  "   -t=chr1,chr2 - restrict target side sequence to those named\n"
  "   -notT=chr1,chr2 - restrict target side sequence to those not named\n"
  "   -id=N - only get one with ID number matching N\n"
  "   -minScore=N - restrict to those scoring at least N\n"
  "   -maxScore=N - restrict to those scoring less than N\n"
  "   -qStartMin=N - restrict to those with qStart at least N\n"
  "   -qStartMax=N - restrict to those with qStart less than N\n"
  "   -qEndMin=N - restrict to those with qEnd at least N\n"
  "   -qEndMax=N - restrict to those with qEnd less than N\n"
  "   -tStartMin=N - restrict to those with tStart at least N\n"
  "   -tStartMax=N - restrict to those with tStart less than N\n"
  "   -tEndMin=N - restrict to those with tEnd at least N\n"
  "   -tEndMax=N - restrict to those with tEnd less than N\n"
  "   -qOverlapStart=N - restrict to those where the query overlaps a region starting here\n"
  "   -qOverlapEnd=N - restrict to those where the query overlaps a region ending here\n"
  "   -tOverlapStart=N - restrict to those where the target overlaps a region starting here\n"
  "   -tOverlapEnd=N - restrict to those where the target overlaps a region ending here\n"
  "   -strand=?    -restrict strand (to + or -)\n"
  "   -long        -output in long format\n"
  "   -zeroGap     -get rid of gaps of length zero\n"
  "   -minGapless=N - pass those with minimum gapless block of at least N\n"
  "   -qMinGap=N     - pass those with minimum gap size of at least N\n"
  "   -tMinGap=N     - pass those with minimum gap size of at least N\n"
  "   -qMaxGap=N     - pass those with maximum gap size no larger than N\n"
  "   -tMaxGap=N     - pass those with maximum gap size no larger than N\n"
  "   -qMinSize=N    - minimum size of spanned query region\n"
  "   -qMaxSize=N    - maximum size of spanned query region\n"
  "   -tMinSize=N    - minimum size of spanned target region\n"
  "   -tMaxSize=N    - maximum size of spanned target region\n"
  "   -noRandom      - suppress chains involving '_random' chromosomes\n"
  "   -noHap         - suppress chains involving '_hap|_alt' chromosomes\n"
  );
}

struct optionSpec options[] = {
   {"q", OPTION_STRING},
   {"notQ", OPTION_STRING},
   {"t", OPTION_STRING},
   {"notT", OPTION_STRING},
   {"id", OPTION_INT},
   {"minScore", OPTION_FLOAT},
   {"maxScore", OPTION_FLOAT},
   {"qStartMin", OPTION_INT},
   {"qStartMax", OPTION_INT},
   {"qEndMin", OPTION_INT},
   {"qEndMax", OPTION_INT},
   {"tStartMin", OPTION_INT},
   {"tStartMax", OPTION_INT},
   {"tEndMin", OPTION_INT},
   {"tEndMax", OPTION_INT},
   {"qOverlapStart", OPTION_INT},
   {"qOverlapEnd", OPTION_INT},
   {"tOverlapStart", OPTION_INT},
   {"tOverlapEnd", OPTION_INT},
   {"strand", OPTION_STRING},
   {"long", OPTION_BOOLEAN},
   {"zeroGap", OPTION_BOOLEAN},
   {"minGapless", OPTION_INT},
   {"qMinGap", OPTION_INT},
   {"qMaxGap", OPTION_INT},
   {"tMaxGap", OPTION_INT},
   {"qMinSize", OPTION_INT},
   {"qMaxSize", OPTION_INT},
   {"tMinSize", OPTION_INT},
   {"tMaxSize", OPTION_INT},
   {"noRandom", OPTION_BOOLEAN},
   {"noHap", OPTION_BOOLEAN},
   {NULL, 0},
};

struct hash *hashCommaString(char *s)
/* Make hash out of comma separated string. */
{
char *e;
struct hash *hash = newHash(8);
while (s != NULL && s[0] != 0)
    {
    e = strchr(s, ',');
    if (e != NULL)
        *e = 0;
    hashAdd(hash, s, NULL);
    if (e != NULL)
	e += 1;
    s = e;
    }
return hash;
}

struct hash *hashCommaOption(char *opt)
/* Make hash out of optional value. */
{
char *s = optionVal(opt, NULL);
if (s == NULL)
    return NULL;
return hashCommaString(s);
}

int mergeCount = 0;

void mergeAdjacentBlocks(struct chain *chain)
/* Get rid of zero length gaps. */
{
struct cBlock *b, *lastB = NULL, *nextB, *bList = NULL;      /* List of blocks. */

for (b = chain->blockList; b != NULL; b = nextB)
    {
    nextB = b->next;
    if (lastB == NULL || lastB->qEnd != b->qStart || lastB->tEnd != b->tStart)
        {
	slAddHead(&bList, b);
	}
    else
        {
	lastB->qEnd = b->qEnd;
	lastB->tEnd = b->tEnd;
	++mergeCount;
	}
    lastB = b;
    }
slReverse(&bList);
chain->blockList = bList;
}

boolean calcMaxGapless(struct chain *chain)
/* Calculate largest block. */
{
struct cBlock *b;
int size, maxSize = 0;
for (b = chain->blockList; b != NULL; b = b->next)
    {
    size = b->tEnd - b->tStart;
    if (maxSize < size)
        maxSize = size;
    }
return maxSize;
}

boolean qCalcMaxGap(struct chain *chain)
/* Calculate largest q gap. */
{
struct cBlock *b, *next;
int size, maxSize = 0;
for (b = chain->blockList; ; b = next)
    {
    if ((next = b->next) == NULL)
        break; 
    size = next->qStart - b->qEnd;
    if (maxSize < size)
        maxSize = size;
    }
return maxSize;
}

boolean tCalcMaxGap(struct chain *chain)
/* Calculate largest t gap. */
{
struct cBlock *b, *next;
int size, maxSize = 0;
for (b = chain->blockList; ; b = next)
    {
    if ((next = b->next) == NULL)
        break; 
    size = next->tStart - b->tEnd;
    if (maxSize < size)
        maxSize = size;
    }
return maxSize;
}

void chainFilter(int inCount, char *inNames[])
/* chainFilter - Filter chain files. */
{
struct hash *qHash = hashCommaOption("q");
struct hash *tHash = hashCommaOption("t");
struct hash *notQHash = hashCommaOption("notQ");
struct hash *notTHash = hashCommaOption("notT");
double minScore = optionFloat("minScore", -BIGNUM);
double maxScore = optionFloat("maxScore", 1.0e20);
int qStartMin = optionInt("qStartMin", -BIGNUM);
int qStartMax = optionInt("qStartMax", BIGNUM);
int qEndMin = optionInt("qEndMin", -BIGNUM);
int qEndMax = optionInt("qEndMax", BIGNUM);
int tStartMin = optionInt("tStartMin", -BIGNUM);
int tStartMax = optionInt("tStartMax", BIGNUM);
int tEndMin = optionInt("tEndMin", -BIGNUM);
int tEndMax = optionInt("tEndMax", BIGNUM);
int qOverlapStart = optionInt("qOverlapStart", -BIGNUM);
int qOverlapEnd = optionInt("qOverlapEnd", BIGNUM);
int tOverlapStart = optionInt("tOverlapStart", -BIGNUM);
int tOverlapEnd = optionInt("tOverlapEnd", BIGNUM);
int minGapless = optionInt("minGapless", 0);
int qMinGap = optionInt("qMinGap", 0);
int tMinGap = optionInt("tMinGap", 0);
int qMaxGap = optionInt("qMaxGap", 0);
int tMaxGap = optionInt("tMaxGap", 0);
int qMinSize = optionInt("qMinSize", 0);
int qMaxSize = optionInt("qMaxSize", BIGNUM);
int tMinSize = optionInt("tMinSize", 0);
int tMaxSize = optionInt("tMaxSize", BIGNUM);
char *strand = optionVal("strand", NULL);
boolean zeroGap = optionExists("zeroGap");
int id = optionInt("id", -1);
boolean doLong = optionExists("long");
boolean noRandom = optionExists("noRandom");
boolean noHap = optionExists("noHap");
int i;

for (i=0; i<inCount; ++i)
    {
    char *fileName = inNames[i];
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    struct chain *chain;
    while ((chain = chainRead(lf)) != NULL)
        {
	boolean writeIt = TRUE;
	if (zeroGap)
	    mergeAdjacentBlocks(chain);
	if (qHash != NULL && !hashLookup(qHash, chain->qName))
	    writeIt = FALSE;
	if (notQHash != NULL && hashLookup(notQHash, chain->qName))
	    writeIt = FALSE;
	if (tHash != NULL && !hashLookup(tHash, chain->tName))
	    writeIt = FALSE;
	if (notTHash != NULL && hashLookup(notTHash, chain->tName))
	    writeIt = FALSE;
	if (chain->score < minScore || chain->score >= maxScore)
	    writeIt = FALSE;
	if (chain->qStart < qStartMin || chain->qStart >= qStartMax)
	    writeIt = FALSE;
	if (chain->qEnd < qEndMin || chain->qEnd >= qEndMax)
	    writeIt = FALSE;
	if (chain->tStart < tStartMin || chain->tStart >= tStartMax)
	    writeIt = FALSE;
	if (chain->tEnd < tEndMin || chain->tEnd >= tEndMax)
	    writeIt = FALSE;
        if (chain->qEnd < qOverlapStart || chain->qStart >= qOverlapEnd)
            writeIt = FALSE;
        if (chain->tEnd < tOverlapStart || chain->tStart >= tOverlapEnd)
            writeIt = FALSE;
	if (chain->qEnd-chain->qStart < qMinSize || chain->tEnd-chain->tStart < tMinSize)
	    writeIt = FALSE;
	if (chain->qEnd-chain->qStart > qMaxSize || chain->tEnd-chain->tStart > tMaxSize)
	    writeIt = FALSE;
	if (strand != NULL && strand[0] != chain->qStrand)
	    writeIt = FALSE;
	if (id >= 0 && id != chain->id)
	    writeIt = FALSE;
	if (minGapless != 0)
	    {
	    if (!(calcMaxGapless(chain) >= minGapless))
	        writeIt = FALSE;
	    }
	if (qMinGap != 0)
	    {
	    if (!(qCalcMaxGap(chain) >= qMinGap))
	        writeIt = FALSE;
	    }
	if (tMinGap != 0)
	    {
	    if (!(tCalcMaxGap(chain) >= tMinGap))
	        writeIt = FALSE;
	    }
	if (qMaxGap != 0)
	    {
	    if (qCalcMaxGap(chain) > qMaxGap)
	        writeIt = FALSE;
	    }
	if (tMaxGap != 0)
	    {
	    if (tCalcMaxGap(chain) > tMaxGap)
	        writeIt = FALSE;
	    }
	if (noRandom)
	    {
	    if (endsWith(chain->tName, "_random") 
	    	|| endsWith(chain->qName, "_random"))
	        writeIt = FALSE;
	    }
	if (noHap)
	    {
	    if (haplotype(chain->tName) || haplotype(chain->qName))
	        writeIt = FALSE;
	    }
	if (writeIt)
	    {
	    if (doLong)
		chainWriteLong(chain, stdout);
	    else
		chainWrite(chain, stdout);
	    }
	chainFree(&chain);
	}
    lineFileClose(&lf);
    }
if (zeroGap)
   fprintf(stderr, "%d zero length gaps eliminated\n", mergeCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
chainFilter(argc-1, argv+1);
return 0;
}
