/* chainFilter - Filter chain files. */
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
  "   -tStartMin=N - restrict to those with tStart at least N\n"
  "   -tStartMax=N - restrict to those with tStart less than N\n"
  "   -strand=?    -restrict strand (to + or -)\n"
  "   -long        -output in long format\n"
  "   -zeroGap     -get rid of gaps of length zero\n"
  "   -minGapless=N - pass those with minimum gapless block of at least N\n"
  "   -qMinGap=N     - pass those with minimum gap size of at least N\n"
  "   -tMinGap=N     - pass those with minimum gap size of at least N\n"
  );
}

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
struct boxIn *b, *lastB = NULL, *nextB, *bList = NULL;      /* List of blocks. */

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
struct boxIn *b;
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
struct boxIn *b, *next;
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
struct boxIn *b, *next;
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
int minScore = optionInt("minScore", -BIGNUM);
int maxScore = optionInt("maxScore", BIGNUM);
int qStartMin = optionInt("qStartMin", -BIGNUM);
int qStartMax = optionInt("qStartMax", BIGNUM);
int tStartMin = optionInt("tStartMin", -BIGNUM);
int tStartMax = optionInt("tStartMax", BIGNUM);
int minGapless = optionInt("minGapless", 0);
int qMinGap = optionInt("qMinGap", 0);
int tMinGap = optionInt("tMinGap", 0);
char *strand = optionVal("strand", NULL);
boolean zeroGap = optionExists("zeroGap");
int id = optionInt("id", -1);
boolean doLong = optionExists("long");
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
	if (chain->tStart < tStartMin || chain->tStart >= tStartMax)
	    writeIt = FALSE;
	if (strand != NULL && strand[0] != chain->qStrand)
	    writeIt = FALSE;
	if (id >= 0 && id != chain->id)
	    writeIt = FALSE;
	if (minGapless != 0)
	    writeIt = (calcMaxGapless(chain) >= minGapless);
	if (qMinGap != 0)
	    writeIt = (qCalcMaxGap(chain) >= qMinGap);
	if (tMinGap != 0)
	    writeIt = (tCalcMaxGap(chain) >= tMinGap);
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
optionHash(&argc, argv);
if (argc < 2)
    usage();
chainFilter(argc-1, argv+1);
return 0;
}
