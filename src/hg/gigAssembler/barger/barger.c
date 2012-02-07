/* barger - make a clone map based on sequence overlap and sts positions. */
#include "common.h"
#include "hCommon.h"
#include "hash.h"
#include "dlist.h"
#include "linefile.h"
#include "portable.h"


FILE *logFile;	/* File to write decision steps to. */

void logg(char *format, ...)
/* Write to log file. */
{
va_list args;
va_start(args, format);
vfprintf(logFile, format, args);
va_end(args);
// fflush(logFile);	/* uglyf */
}

void status(char *format, ...)
/* Print status message to stdout and to log file. */
{
va_list args;
va_start(args, format);
if (logFile != NULL)
    vfprintf(logFile, format, args);
vfprintf(stdout, format, args);
va_end(args);
}

/* Make uglyf debugging statements go to log file too. */
#undef uglyf
#define uglyf status


struct barge
/* A collection of overlapping clones. */
    {
    struct dlList *cloneList;
    int id;	      /* Unique barge id. */
    int stsCertainty; /* Number of good STS hits - number of bad STS hits. */
    char *stsChrom;   /* Chromosome (allocated in hash) if positioned by sts. */
    float stsStart, stsEnd;       /* STS position in chromosome (only set if stsChrom non-null) */
    struct clonePair *usedPairs;	/* Pairs used in building barge. */
    struct dlNode *node;	/* Node this is in. */
    };
static int nextBargeId;	/* Next free barge id. */

struct oogClone
/* A single clone. */
    {
    struct oogClone *next;	/* Next in list. */
    char *name;		/* Name (allocated in hash) */
    int phase;		/* HGS phase - 3 finished, 2 ordered, 1 draft, 0 small frags. */
    int size;		/* Size. */
    int bargePos;		/* Position within barge. */
    char *stsChrom;   /* Chromosome (allocated in hash) if positioned by sts. */
    float stsPos;     /* STS position in chromosome (only set if stsChrom non-null) */
    struct seqSharer *sharers;  /* List of overlapping clones. */
    struct barge *barge;    /* Barge this is part of. */
    struct dlNode *bargeNode; /* Node in barge's cloneList. */
    int maxOverlapPpt;		/* Max overlap parts per thousand. */
    struct oogClone *maxOverlapClone; /* Clone with max overlap. */
    boolean isEnclosed;		/* True if enclosed at current enclosure threshold. */
    };

struct seqSharer
/* A possibly neighboring clone. */
    {
    struct seqSharer  *next;	/* Next in list. */
    struct oogClone *clone;	/* Clone. */
    struct clonePair *pair;     /* Overlap info. */
    };

struct clonePair
/* An overlapping pair of clones. */
    {
    struct clonePair *next;	/* Next in list. */
    char *name;		        /* Name of pair - allocated in hash. */
    struct oogClone *a, *b;	/* The clones. */
    int overlap;		/* The amount of overlap. */
    int aUses, bUses;		/* Number of times a,b used. */
    float score;		/* Score function for greedy algorithm. */
    };

struct oogClone *storeClone(struct hash *cloneHash, char *name, int phase, int size)
/* Allocate clone and put it in hash table. Squawk but don't die if a dupe. */
{
struct hashEl *hel;
struct oogClone *clone;

if ((hel = hashLookup(cloneHash, name)) != NULL)
    {
    warn("Duplicate clone %s, ignoring all but first", name);
    return NULL;
    }
else
    {
    AllocVar(clone);
    hel = hashAdd(cloneHash, name, clone);
    clone->name = hel->name;
    clone->phase = phase;
    clone->size = size;
    return clone;
    }
}

boolean isAccession(char *s)
/* Return TRUE if s is of form to be a Genbank accession. */
{
int len = strlen(s);
if (len < 6 || len > 9)
    return FALSE;
if (!isalpha(s[0]))
    return FALSE;
if (!isdigit(s[len-1]))
    return FALSE;
return TRUE;
}

struct oogClone *readSeqInfo(char *fileName, struct hash *cloneHash)
/* Read in sequence.inf file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int lineSize, wordCount;
char *line, *words[16];
char cloneName[128];
struct oogClone *clone, *cloneList = NULL;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
	continue;
    wordCount = chopLine(line, words);
    if (wordCount < 8)
	errAbort("Expecting at least 8 words line %d of %s", lf->lineIx, lf->fileName);
    strncpy(cloneName, words[0], sizeof(cloneName));
    chopSuffix(cloneName);
    if (!isAccession(cloneName))
	{
	warn("Expecting an accession in first field line %d of %s", lf->lineIx, lf->fileName);
	continue;
	}
    if ((clone = storeClone(cloneHash, cloneName, atoi(words[3]), atoi(words[2]))) != NULL)
	{
	slAddHead(&cloneList, clone);
	}
    }
lineFileClose(&lf);
slReverse(&cloneList);
return cloneList;
}

char *makePairName(char *a, char *b, boolean *retFirstA)
/* Return name in form a&b or b&a (alphabetically sorted) */
{
static char buf[256];
if (strcmp(a, b) <= 0)
    {
    sprintf(buf, "%s%s", a, b);
    *retFirstA = TRUE;
    }
else
    {
    sprintf(buf, "%s%s", b, a);
    *retFirstA = FALSE;
    }
return buf;
}

struct oogClone *readStsFile(char *fileName, struct hash *cloneHash, struct oogClone *cloneList, 
	struct hash *chromHash)
/* Read in STS file.  Squawk if clones in it don't already exist, but then make them up. */
{
struct oogClone *clone;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int lineSize, wordCount;
char *line, *words[16];
char *acc;
int count = 0;
boolean firstA;

while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount == 0)
	continue;
    if (words[0][0] == '#')
	continue;
    if (wordCount != 3)
	errAbort("Expecting three words line %d of %s", lf->lineIx, lf->fileName);
    acc = words[0];
    if (!isAccession(acc))
	errAbort("Expecting an accession in first field line %d of %s", lf->lineIx, lf->fileName);
    if ((clone = hashFindVal(cloneHash, acc)) == NULL)
	{
	clone = storeClone(cloneHash, acc, 0, 150000);
	warn("%s in STS but not clone list", acc);
	slAddHead(&cloneList, clone);
	}
    clone->stsChrom = hashStoreName(chromHash, words[1]);
    clone->stsPos = atof(words[2]);
    ++count;
    }
lineFileClose(&lf);
printf("%d STS positioned clones in %s\n", count, fileName);
return cloneList;
}

struct clonePair *readPairs(char *fileName, struct hash *cloneHash, struct hash *pairHash)
/* Read in pairs from file. */
{
struct oogClone *a, *b;
struct clonePair *pair, *pairList = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int lineSize, wordCount;
char *line, *words[16];
char *acc;
int count = 0;
char *pairName;
int overlap;
boolean firstA;

while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount == 0)
	continue;
    if (words[0][0] == '#')
	continue;
    if (wordCount != 3)
	errAbort("Expecting three words line %d of %s", lf->lineIx, lf->fileName);
    if (!isAccession(words[0]) || !isAccession(words[2]) || !isdigit(words[1][0]))
	errAbort("Bad line %d of %s", lf->lineIx, lf->fileName);
    overlap = atoi(words[1]);
    a = hashMustFindVal(cloneHash, words[0]);
    b = hashMustFindVal(cloneHash, words[2]);
    pairName = makePairName(a->name, b->name, &firstA);
    if ((pair = hashFindVal(pairHash, pairName)) == NULL)
	{
	struct hashEl *hel;
	AllocVar(pair);
	hel = hashAdd(pairHash, pairName, pair);
	pair->name = hel->name;
	if (firstA)
	    {
	    pair->a = a;
	    pair->b = b;
	    }
	else
	    {
	    pair->a = b;
	    pair->b = a;
	    }
	pair->overlap = overlap;
	slAddHead(&pairList, pair);
	}
    else
	{
	if (overlap > pair->overlap)
	    pair->overlap = overlap;
	}
    }
lineFileClose(&lf);
slReverse(&pairList);
return pairList;
}

void sharesFromPairs(struct clonePair *pairList)
/* Make sequence connections between clones. */
{
struct clonePair *pair;
struct seqSharer *share;
struct oogClone *a, *b;
int overlap;

for (pair = pairList; pair != NULL; pair = pair->next)
    {
    overlap = pair->overlap;
    a = pair->a;
    b = pair->b;

    AllocVar(share);
    share->clone = a;
    share->pair = pair;
    slAddHead(&b->sharers, share);

    AllocVar(share);
    share->clone = b;
    share->pair = pair;
    slAddHead(&a->sharers, share);
    }
}

int lookupOverlap(struct hash *looseHash, struct oogClone *a, struct oogClone *b)
/* Find overlap between two clones. */
{
struct clonePair *pair;
boolean firstA;

pair = hashFindVal(looseHash, makePairName(a->name, b->name, &firstA));
if (pair == NULL)
    return 0;
return pair->overlap;
}

void findMaxOverlaps(struct oogClone *cloneList, struct hash *looseHash)
/* Find maximum overlapping clones from cloneList */
{
struct oogClone *a, *b;
struct seqSharer *share;
int maxOverlap;
int overlap;

for (a = cloneList; a != NULL; a = a->next)
    {
    maxOverlap = 0;
    for (share = a->sharers; share != NULL; share = share->next)
	{
	b = share->clone;
	overlap = lookupOverlap(looseHash, a, b);
	if (overlap > maxOverlap)
	    {
	    maxOverlap = overlap;
	    a->maxOverlapClone = b;
	    }
	}
    if (a->maxOverlapClone != NULL)
	a->maxOverlapPpt = round(1000.0*maxOverlap/a->size);
    }
}

void countUses(struct clonePair *pairList)
/* Fill in aUses and bUses fields of pairs. */
{
struct clonePair *pair;
struct oogClone *a, *b;

for (pair = pairList; pair != NULL; pair = pair->next)
    {
    a = pair->a;
    b = pair->b;
    pair->aUses = slCount(a->sharers);
    pair->bUses = slCount(b->sharers);
    }
}

void fillInScores(struct clonePair *pairList)
/* Fill in scores. */
{
struct clonePair *pair;
struct oogClone *a, *b;
int q;

for (pair = pairList; pair != NULL; pair = pair->next)
    {
    if (pair->aUses == 2 && pair->bUses == 2)
	q = 1;
    else
	{
	q = max(pair->aUses, pair->bUses);
	if (q > 2)
	    q = q*q;
	}
    pair->score = (double)pair->overlap/(double)q;
    }
}

int cmpPairScore(const void *va, const void *vb)
/* Compare two clonePair. */
{
const struct clonePair *a = *((struct clonePair **)va);
const struct clonePair *b = *((struct clonePair **)vb);
float dif = b->score - a->score;

if (dif > 0)
    return 1;
else if (dif < 0)
    return -1;
else
    return 0;
}

void writePairs(char *fileName, struct clonePair *pairList)
/* Write pairs output. */
{
FILE *f = mustOpen(fileName, "w");
struct clonePair *pair;

fprintf(f, "# score    \tcloneA    \tsizeA\tusesA\tcloneB    \tsizeB\tusesB\toverlap\n");
fprintf(f, "#--------------------------------------------------------------------------------------\n");
for (pair = pairList; pair != NULL; pair = pair->next)
    {
    fprintf(f, "%1.2f\t%s\t%d\t%d\t%s\t%d\t%d\t%d\n",
    	pair->score, pair->a->name, pair->a->size, pair->aUses, 
	pair->b->name, pair->b->size, pair->bUses, pair->overlap);
    }
fclose(f);
}

float stsMaxVariance;	/* Max allowed difference between sts values. */
int stsMaxConflict;     /* How many STS values allowed to conflict in
                         * barge.  Sort of a half baked idea.  Probably
			 * best left set to zero and possibly stripped from
			 * code. */

boolean stsClose(char *chromA, float startA, float endA, 
	char *chromB, float posB, int maxConflict)
/* Return TRUE if STS looks compatible with range. */
{
if (maxConflict > 0)
    return TRUE;
if (chromA == NULL || chromB == NULL)
    return TRUE;
if (!sameString(chromA, chromB))
    {
    logg("sts chromosome mismatch %s %s\n", chromA, chromB);
    return FALSE;
    }
if (posB < startA - stsMaxVariance)
    {
    logg("sts begins too soon %f in (%f to %f)\n", posB, startA, endA);
    return FALSE;
    }
if (posB > endA + 10)
    {
    logg("sts begins too late %f in (%f to %f)\n", posB, startA, endA);
    return FALSE;
    }
return TRUE;
}

boolean tooManyConflicts(struct barge *aBarge, struct barge *bBarge, int maxConflict)
/* Return TRUE if conflict count exceeds tolerances. */
{
if (stsMaxVariance == 0)
    return FALSE;
if (aBarge->stsCertainty > maxConflict &&  bBarge->stsCertainty > maxConflict)
    return TRUE;
return FALSE;
}

boolean stsRangesClose(struct barge *aBarge, struct barge *bBarge, int maxConflict)
/* Return TRUE if STS looks compatible with range. */
{
char *chromA = aBarge->stsChrom;
char *chromB = bBarge->stsChrom;
float startA = aBarge->stsStart, endA = aBarge->stsEnd;
float startB = bBarge->stsStart, endB = bBarge->stsEnd;
float s, e;

if (chromA == NULL || chromB == NULL)
    return TRUE;
if (!sameString(chromA, chromB))
    {
    if (tooManyConflicts(aBarge, bBarge, maxConflict))
	{
	logg("sts range chromosome mismatch %s %s\n", chromA, chromB);
	return FALSE;
	}
    }
startB -= stsMaxVariance;
endB += stsMaxVariance;
s = max(startA, startB);
e = min(endA, endB);
if (s > e)
    {
    if (tooManyConflicts(aBarge, bBarge, maxConflict))
	{
	logg("sts ranges aren't close %f-%f and %f-%f\n", startA, endA, startB, endB);
	return FALSE;
	}
    }
return TRUE;
}

void dumpBarge(struct barge *b)
/* Write out barge to log file. */
{
struct dlNode *node;
struct oogClone *clone;
logg("barge %d :", b->id);
for (node = b->cloneList->head; node->next != NULL; node = node->next)
    {
    clone = node->val;
    logg(" %s", clone->name);
    }
logg("\n");
}

struct barge *emptyBarge(struct dlList *bargeList)
/* Return a new empty barge. */
{
struct barge *barge;
AllocVar(barge);
barge->id = ++nextBargeId;
barge->cloneList = newDlList();
barge->node = dlAddValTail(bargeList, barge);
return barge;
}

struct barge *bargeOfOne(struct oogClone *clone, struct dlList *bargeList)
/* Return singleton barge. */
{
struct barge *barge = emptyBarge(bargeList);
clone->bargeNode = dlAddValTail(barge->cloneList, clone);
clone->barge = barge;
if (clone->stsChrom)
    {
    barge->stsStart = barge->stsEnd = clone->stsPos;
    barge->stsChrom = clone->stsChrom;
    barge->stsCertainty = 1;
    }
return barge;
}

struct barge *bargeFromPair(struct dlList *bargeList, struct oogClone *a, struct oogClone *b)
/* Create a barge with just the pair a/b. */
{
struct barge *barge;
struct oogClone *stsClone = NULL;

logg("bargeFromPair(%s %s)\n", a->name, b->name);
if (!stsClose(a->stsChrom, a->stsPos, a->stsPos, b->stsChrom, 
	b->stsPos, stsMaxConflict))
    return NULL;
barge = emptyBarge(bargeList);
if (a->stsChrom && b->stsChrom)
    {
    if (stsClose(a->stsChrom, a->stsPos, a->stsPos, b->stsChrom, 
	    b->stsPos, 0))
	{
	barge->stsChrom = a->stsChrom;
	if (a->stsPos < b->stsPos)
	    {
	    barge->stsStart = a->stsPos;
	    barge->stsEnd = b->stsPos;
	    }
	else
	    {
	    barge->stsStart = b->stsPos;
	    barge->stsEnd = a->stsPos;
	    }
	barge->stsCertainty = 2;
	}
    }
else 
    {
    if (a->stsChrom)
	stsClone = a;
    else if (b->stsChrom)
	stsClone = b;
    if (stsClone)
	{
	barge->stsChrom = stsClone->stsChrom;
	barge->stsStart = barge->stsEnd = stsClone->stsPos;
	barge->stsCertainty = 1;
	}
    }
a->bargeNode = dlAddValTail(barge->cloneList, a);
b->bargeNode = dlAddValTail(barge->cloneList, b);
a->barge = b->barge = barge;
return barge;
}

void fixCloneBargePointers(struct barge *barge)
/* Fix up all of the clones in barge->cloneList so that they point back to
 * this barge. */
{
struct dlNode *node;
struct oogClone *clone;
for (node = barge->cloneList->head; node->next != NULL; node = node->next)
    {
    clone = node->val;
    clone->barge = barge;
    clone->bargeNode = node;
    }
}

struct barge *pdupeBarge(struct barge *barge)
/* Return a partial duplicate of barge. This is done to explore if two
 * barges can be merged. This isn't completely independent of the
 * source barge, so be careful...  In particular the barge->cloneList->barge
 * pointers of the old barge get changed.  You will need to dispose of either
 * the original or the copy, and do a fixCloneBargePointers to restore things
 * to a normal state. */
{
struct barge *dupe;
struct dlList *oldList, *newList;
struct dlNode *node;

AllocVar(dupe);
*dupe = *barge;
dupe->cloneList = newList = newDlList();
oldList = barge->cloneList;
for (node = oldList->head; node->next != NULL; node = node->next)
    {
    dlAddValTail(newList, node->val);
    }
dupe->node = NULL;
dupe->usedPairs = NULL;
fixCloneBargePointers(dupe);
return dupe;
}

void unpdupeBarge(struct barge **pBarge)
/* Free up a partially duped barge. */
{
struct barge *barge = *pBarge;
if (barge != NULL)
    freeDlList(&barge->cloneList);
freez(pBarge);
}

struct barge *extendBarge(struct barge *barge, struct oogClone *x, struct oogClone *b, 
	struct hash *looseHash)
/* Try to insert x into barge that already includes b. X must end up to one side or other of b.*/
{
struct oogClone *a = NULL, *c = NULL;
int abOverlap = 0, bcOverlap = 0;
int axOverlap = 0, bxOverlap = 0, cxOverlap = 0;
struct dlNode *node;
int nextTendency = 0, prevTendency = 0;
int xSize = x->size;
int maxErr = xSize/10;

logg("extendBarge(%s %s)\n", x->name, b->name);
dumpBarge(barge);
if (!stsClose(barge->stsChrom, barge->stsStart, barge->stsEnd, x->stsChrom, 
	x->stsPos, stsMaxConflict))
    return NULL;
bxOverlap = lookupOverlap(looseHash, b, x);
node = b->bargeNode->prev;
if (node->prev != NULL)
    a = node->val;
node = b->bargeNode->next;
if (node->next != NULL)
    c = node->val;
if (a != NULL)
    {
    axOverlap = lookupOverlap(looseHash, a, x);
    abOverlap = lookupOverlap(looseHash, a, b);
    prevTendency = axOverlap - abOverlap;
    }
if (c != NULL)
    {
    cxOverlap = lookupOverlap(looseHash, c, x);
    bcOverlap = lookupOverlap(looseHash, b, c);
    nextTendency = cxOverlap - bcOverlap;
    }
if (prevTendency < -maxErr && nextTendency < -maxErr)
    {
    logg("Inconsistent tendencies\n");
    return NULL;
    }
if (prevTendency > nextTendency)
    {
    if (a  != NULL && axOverlap <= 0)
	{
	logg("Inconsistent axOverlap\n");
	return NULL;
	}
    x->bargeNode = dlAddValBefore(b->bargeNode, x);
    }
else
    {
    if (c  != NULL && cxOverlap <= 0)
	{
	logg("Inconsistent cxOverlap\n");
	return NULL;
	}
    x->bargeNode = dlAddValAfter(b->bargeNode, x);
    }
x->barge = barge;
if (x->stsChrom)
    {
    if (barge->stsChrom == NULL)
	{
	barge->stsChrom = x->stsChrom;
	barge->stsStart = barge->stsEnd = x->stsPos;
	barge->stsCertainty = 1;
	}
    else
	{
	if (stsClose(barge->stsChrom, barge->stsStart, barge->stsEnd, x->stsChrom, 
		x->stsPos, 0))
	    {
	    if (x->stsPos < barge->stsStart)
		barge->stsStart = x->stsPos;
	    if (x->stsPos > barge->stsEnd)
		barge->stsEnd = x->stsPos;
	    barge->stsCertainty += 1;
	    }
	else
	    {
	    barge->stsCertainty -= 1;
	    }
	}
    }
logg("   ");
dumpBarge(barge);
return barge;
}

struct barge *wiggleIntoBarge(struct barge *barge, struct oogClone *x, struct oogClone *b, 
	struct hash *looseHash, int wiggleRoom)
/* Insert x into barge containing b somewhere in the near neighborhood of b. */
{
int maxOverlap = lookupOverlap(looseHash, x, b);
int prevOverlap = maxOverlap, nextOverlap = maxOverlap;
struct oogClone *maxOverlapClone = b;
int overlap, i;
struct oogClone *clone;
struct dlNode *prevNode, *nextNode;

prevNode = b->bargeNode->prev;
nextNode = b->bargeNode->next;
for (i=0; i<wiggleRoom; ++i)
    {
    if (nextNode != NULL && nextNode->next != NULL)
	{
	clone = nextNode->val;
	overlap = lookupOverlap(looseHash, x, clone);
	if (overlap < nextOverlap)
	    {
	    nextNode = NULL;
	    }
	else
	    {
	    if (overlap > maxOverlap)
		{
		maxOverlap = overlap;
		maxOverlapClone = clone;
		}
	    nextNode = nextNode->next;
	    }
	}
    if (prevNode != NULL && prevNode->prev != NULL)
	{
	clone = prevNode->val;
	overlap = lookupOverlap(looseHash, x, clone);
	if (overlap < prevOverlap)
	    {
	    prevNode = NULL;
	    }
	else
	    {
	    if (overlap > maxOverlap)
		{
		maxOverlap = overlap;
		maxOverlapClone = clone;
		}
	    prevNode = prevNode->prev;
	    }
	}
    }
return extendBarge(barge, x, maxOverlapClone, looseHash);
}

int cmpClonePos(const void *va, const void *vb)
/* Compare two clones. */
{
const struct oogClone *a = *((struct oogClone **)va);
const struct oogClone *b = *((struct oogClone **)vb);
return (a->bargePos - b->bargePos);
}

void sortAllClonesByPos(struct dlList *bargeList)
/* Sort all clones by their position in barge. */
{
struct dlNode *bNode;
struct barge *barge;

for (bNode = bargeList->head; bNode->next != NULL; bNode = bNode->next)
    {
    barge = bNode->val;
    dlSort(barge->cloneList, cmpClonePos);
    }
}


int bargeSize(struct barge *barge)
/* Figure out size of barge. */
{
struct dlNode *node;
int end, max=0;

for (node = barge->cloneList->head; node->next != NULL; node = node->next)
    {
    struct oogClone *clone = node->val;
    end = clone->bargePos + clone->size;
    if (end > max) max = end;
    }
return max;
}

void flipBarge(struct barge *barge)
/* Flip order of cloneList. */
{
struct dlList *oldList = barge->cloneList;
struct dlList *newList = newDlList();
struct dlNode *node;
int bSize = bargeSize(barge);
int cSize;
struct oogClone *clone;
int cEnd;

while ((node = dlPopHead(oldList)) != NULL)
    {
    clone = node->val;
    cEnd = clone->bargePos + clone->size;
    clone->bargePos = bSize - cEnd;
    dlAddHead(newList, node);
    }
freeDlList(&oldList);
barge->cloneList = newList;
}

boolean needsFlipping(struct barge *barge)
/* Return TRUE if STS data indicates barge needs flipping. */
{
float tendency = 0.0;
boolean first = TRUE;
float pos, lastPos = 0;
struct dlNode *cNode;
struct oogClone *clone;

for (cNode = barge->cloneList->head; cNode->next != NULL; cNode = cNode->next)
    {
    clone = cNode->val;
    if (clone->stsChrom)
	{
	if (first)
	    {
	    first = FALSE;
	    lastPos = clone->stsPos;
	    }
	else
	    {
	    pos = clone->stsPos;
	    tendency += pos - lastPos;
	    lastPos = pos;
	    }
	}
    }
return tendency < 0.0;
}

void flipBargesAsNeeded(struct dlList *bargeList)
/* Look if STS positions make barge want to flip. */
{
struct dlNode *bNode;
struct barge *barge;

for (bNode = bargeList->head; bNode->next != NULL; bNode = bNode->next)
    {
    barge = bNode->val;
    if (barge->stsChrom && needsFlipping(barge))
	flipBarge(barge);
    }
sortAllClonesByPos(bargeList);
}

boolean doMostOfMerge(struct barge *barge, struct oogClone *a, struct oogClone *b, struct hash *looseHash)
/* Merge clone a and other clones in a's barge into barge, starting at b. 
 * There is some truly scary sh*t happening with lists and pointers here so
 * that we can back out of the merge if it doesn't work out. */
{
struct dlNode *node, *prevNode = a->bargeNode->prev;
struct oogClone *clone, *overlapClone = b;
for (node = a->bargeNode; node->next != NULL; node = node->next)
    {
    clone = node->val;
    if (!wiggleIntoBarge(barge, clone, overlapClone, looseHash, 5))
	return FALSE;
    overlapClone = clone;
    }
overlapClone = a;
for (node = prevNode; node->prev != NULL; node = node->prev)
    {
    clone = node->val;
    if (!wiggleIntoBarge(barge, clone, overlapClone, looseHash, 5))
	return FALSE;
    overlapClone = clone;
    }
return TRUE;
}


boolean trialMerge(struct dlList *bargeList, struct oogClone *a, struct oogClone *b, struct hash *looseHash)
/* Merge a's barge into b's barge.  a and b overlap. Returns FALSE if can't 
 * There is some truly scary sh*t happening with list and pointers here so
 * that we can back out of the merge if it doesn't work out. */
{
struct barge *aBarge = a->barge;
struct barge *bBarge = b->barge;
struct barge *newBarge = pdupeBarge(bBarge);
struct dlNode *node;
struct oogClone *clone;

if (doMostOfMerge(newBarge, a, b, looseHash))
    {
    logg("after merge ");
    dumpBarge(newBarge);
    newBarge->usedPairs = slCat(aBarge->usedPairs, bBarge->usedPairs);
    newBarge->node = dlAddValAfter(bBarge->node, newBarge);
    fixCloneBargePointers(newBarge);

    if (aBarge->stsChrom && bBarge->stsChrom)
	{
	if (stsRangesClose(aBarge, bBarge, 0))
	    {
	    newBarge->stsStart = min(aBarge->stsStart, bBarge->stsStart);
	    newBarge->stsEnd = max(aBarge->stsEnd, bBarge->stsEnd);
	    newBarge->stsCertainty = aBarge->stsCertainty + bBarge->stsCertainty;
	    }
	else
	    {
	    if (aBarge->stsCertainty > bBarge->stsCertainty)
		{
		newBarge->stsStart = aBarge->stsStart;
		newBarge->stsEnd = aBarge->stsEnd;
		newBarge->stsChrom = aBarge->stsChrom;
		newBarge->stsCertainty = aBarge->stsCertainty - bBarge->stsCertainty;
		}
	    else
		{
		newBarge->stsCertainty = bBarge->stsCertainty - aBarge->stsCertainty;
		}
	    }
	}
    else if (aBarge->stsChrom)
	{
	newBarge->stsStart = aBarge->stsStart;
	newBarge->stsEnd = aBarge->stsEnd;
	newBarge->stsChrom = aBarge->stsChrom;
	newBarge->stsCertainty = aBarge->stsCertainty;
	}

    dlRemove(bBarge->node);
    freez(&bBarge->node);
    unpdupeBarge(&bBarge);

    dlRemove(aBarge->node);
    freez(&aBarge->node);
    unpdupeBarge(&aBarge);
    return TRUE;
    }
else
    {
    fixCloneBargePointers(aBarge);
    fixCloneBargePointers(bBarge);
    unpdupeBarge(&newBarge);
    return FALSE;
    }
}

struct barge *mergeBarges(struct dlList *bargeList, struct oogClone *a, struct oogClone *b, 
	struct hash *looseHash)
/* Merge two barges if possible.  Return merged barge if possible, NULL otherwise. */
{
struct barge *aBarge = a->barge;
struct barge *bBarge = b->barge;

logg("mergeBarges(%s %s)\n", a->name, b->name);
    {
    logg(" ");
    dumpBarge(aBarge);
    logg(" ");
    dumpBarge(bBarge);
    }

if (!stsRangesClose(aBarge, bBarge, stsMaxConflict))
    return NULL;

/* Return TRUE if STS looks compatible with range. */
if (!trialMerge(bargeList, a, b, looseHash))
    {
    logg("Trial merge between %s (%d) and %s (%d) didn't work\n", 
    	a->name, a->barge->id, b->name, b->barge->id);
    return NULL;
    }
else
    return b->barge;
}

struct barge *fitIntoBarges(struct dlList *bargeList, struct clonePair *pair, struct hash *looseHash)
/* Fit pair into barge list if possible. */
{
struct oogClone *a = pair->a, *b = pair->b;
struct barge *barge;

logg("fitIntoBarges(%f %s %s %d\n", pair->score, pair->a->name, pair->b->name, pair->overlap);
if (a->barge == NULL && b->barge == NULL)
    {
    barge = bargeFromPair(bargeList, a, b);
    }
else if (a->barge == NULL)
    {
    barge = extendBarge(b->barge, a, b, looseHash);
    }
else if (b->barge == NULL)
    {
    barge = extendBarge(a->barge, b, a, looseHash);
    }
else if (a->barge == b->barge)
    {
    logg("Redundant overlap between %s and %s in barge %d\n", a->name, b->name, a->barge->id);
    barge = a->barge;
    }
else
    {
    barge = mergeBarges(bargeList, a, b, looseHash);
    }
return barge;
}

char *nameOrNa(char *chrom)
/* Return chrom or "NA" if NULL */
{
if (chrom == NULL)
    return "NA";
return chrom;
}

int cmpBargePos(const void *va, const void *vb)
/* Compare two barges by chromosome and sts position. */
{
const struct barge *a = *((struct barge **)va);
const struct barge *b = *((struct barge **)vb);
char *aC = nameOrNa(a->stsChrom);
char *bC = nameOrNa(b->stsChrom);
int iDif = strcmp(aC, bC);
float dif;

if (iDif != 0)
    return iDif;
dif = a->stsStart - b->stsStart;
if (dif > 0)
    return 1;
else if (dif < 0)
    return -1;
else
    return 0;
}

void markEnclosed(struct oogClone *cloneList, int maxOverlap, struct hash *looseHash)
/* Set flag on clones that look to be enclosed by another. */
{
struct oogClone *a, *b;
int overlap;
int aPpt, bPpt;

for (a = cloneList; a != NULL; a = a->next)
    a->isEnclosed = FALSE;
for (a = cloneList; a != NULL; a = a->next)
    {
    aPpt = a->maxOverlapPpt;
    if (aPpt > maxOverlap)
	{
	b = a->maxOverlapClone;
	overlap = lookupOverlap(looseHash, a, b);
	bPpt = round(1000.0*overlap/b->size);
	if (aPpt > bPpt)
	    a->isEnclosed = TRUE;
	else if (aPpt == bPpt && strcmp(a->name, b->name) < 0)
	    a->isEnclosed = TRUE;
	}
    }
}


void buryClone(struct oogClone *small, struct oogClone *big, struct dlList *bargeList, struct hash *looseHash)
/* Bury small clone in middle of big clone. */
{
struct barge *barge = big->barge;

if (barge == NULL)
    barge = bargeOfOne(big, bargeList);
if (small->barge == barge)
    return;
if (small->barge == NULL)
    {
    small->barge = barge;
    small->bargeNode = dlAddValAfter(big->bargeNode, small);
    small->bargePos = big->bargePos + (big->size - small->size)/2;
    }
else
    {
    int pos = big->bargePos + (big->size - small->size)/2;
    struct barge *sb = small->barge;
    struct dlNode *bigNode = big->bargeNode;
    struct dlNode *smallNode;
    struct oogClone *smallClone;

    while ((smallNode = dlPopHead(sb->cloneList)) != NULL)
	{
	smallClone = smallNode->val;
	dlAddAfter(bigNode, smallNode);
	smallClone->barge = barge;
	smallClone->bargePos += pos;
	bigNode = smallNode;
	}
    dlRemove(sb->node);
    barge->usedPairs = slCat(sb->usedPairs, barge->usedPairs);
    }
}

void placeEnclosedClones(struct oogClone *cloneList, struct clonePair **pPairList, 
	struct dlList *bargeList, struct hash *looseHash, int minPpt)
/* Place clones that are surrounded by another. */
{
struct clonePair *pair, *next, *leftovers = NULL;
struct oogClone *a, *b;
int overlap;

for (pair = *pPairList; pair != NULL; pair = next)
    {
    next = pair->next;
    a = pair->a;
    b = pair->b;
    if (a->maxOverlapClone == b && a->isEnclosed)
	{
	buryClone(a, b, bargeList, looseHash);
	slAddHead(&a->barge->usedPairs, pair);
	}
    else if (b->maxOverlapClone == a && b->isEnclosed)
	{
	buryClone(b, a, bargeList, looseHash);
	slAddHead(&b->barge->usedPairs, pair);
	}
    else
	{
	slAddHead(&leftovers, pair);
	}
    }
sortAllClonesByPos(bargeList);
*pPairList = leftovers;
}

void positionClones(struct dlList *bargeList, struct hash *looseHash)
/* Position clones relative to start of barge. */
{
struct dlNode *bNode;
struct dlNode *cNode, *ncNode;
struct barge *barge;
struct oogClone *clone, *nextClone;
int offset;

for (bNode = bargeList->head; bNode->next != NULL; bNode = bNode->next)
    {
    barge = bNode->val;
    offset = 0;
    for (cNode = barge->cloneList->head; cNode->next != NULL; cNode = cNode->next)
	{
	clone = cNode->val;
	clone->bargePos = offset;
	offset += clone->size;
	ncNode = cNode->next;
	if (ncNode->next != NULL)
	    {
	    nextClone = ncNode->val;
	    offset -= lookupOverlap(looseHash, clone, nextClone);
	    }
	}
    }
}


void glomBarges(struct oogClone *cloneList, struct clonePair **pPairList, 
	struct dlList *bargeList, struct hash *looseHash,
	int minScore, int maxOverlap, float maxStsDistance, int maxStsConflict)
/* Glom together clones into barges. Go through list of clone pairings in order
 * and if pairing is consistent with barges built so far use it to extend
 * barge collection. */
{
struct clonePair *leftovers = NULL, *next, *pair;
struct clonePair *rejects = NULL;
struct oogClone *a, *b;
int overlap;
int aSize, bSize;
struct barge *barge;
int rejectCount = 0, usedCount = 0, deferredCount = 0;

stsMaxVariance = maxStsDistance;
stsMaxConflict = maxStsConflict;
markEnclosed(cloneList, maxOverlap, looseHash);
for (pair = *pPairList; pair != NULL; pair = next)
    {
    next = pair->next;
    logg("pair %1.2f %s %s %d\n", pair->score, pair->a->name, pair->b->name, pair->overlap);
    if (pair->score >= minScore)
	{
	a = pair->a;
	b = pair->b;
	if (!a->isEnclosed && !b->isEnclosed)
	    {
	    if ((barge = fitIntoBarges(bargeList, pair, looseHash)) != NULL)
		{
		slAddHead(&barge->usedPairs, pair);
		++usedCount;
		}
	    else
		{
		slAddHead(&rejects, pair);
		++rejectCount;
		}
	    pair = NULL;
	    }
	else
	    {
	    logg(" rejected on overlap (%d %d %d)\n", 
	    	a->maxOverlapPpt, b->maxOverlapPpt, maxOverlap);
	    }
	}
    else
	{
	logg(" rejected on score (%d %d)\n", round(pair->score), minScore);
	}
    if (pair != NULL)
	{
	++deferredCount;
	slAddHead(&leftovers, pair);
	}
    }
slReverse(&leftovers);
// slReverse(&rejects);
// leftovers = slCat(leftovers, rejects);
*pPairList = leftovers;
positionClones(bargeList, looseHash);
status("Used %d, rejected %d, deferred %d pairs\n", usedCount, rejectCount, deferredCount);
}

void makeEasyBarges(struct oogClone *cloneList, struct clonePair **pPairList, 
	struct dlList *bargeList, struct hash *looseHash)
/* Make the relatively easy barges. */
{
glomBarges(cloneList, pPairList, bargeList, looseHash, 3000, 850, 5, 0);
}

void makeMediumBarges(struct oogClone *cloneList, struct clonePair **pPairList, 
	struct dlList *bargeList, struct hash *looseHash)
/* Make the relatively easy barges. */
{
glomBarges(cloneList, pPairList, bargeList, looseHash, 1500, 900, 10, 0);
}

int hardMaxOverlap = 950;  /* Maximum overlap in parts per thousand allowed between
                            * clones in last pass. */

void makeHardBarges(struct oogClone *cloneList, struct clonePair **pPairList, 
	struct dlList *bargeList, struct hash *looseHash)
/* Make the relatively easy barges. */
{
glomBarges(cloneList, pPairList, bargeList, looseHash, 0, hardMaxOverlap, 20, 0);
}


void writeBarges(char *fileName, struct dlList *bargeList, struct hash *looseHash)
/* Write out barges. */
{
FILE *f = mustOpen(fileName, "w");
struct dlNode *bargeNode, *cloneNode, *nextNode;
struct barge *barge;
struct oogClone *clone, *nextClone, *maxClone;
char *maxName;
int overlap;

for (bargeNode = bargeList->head; bargeNode->next != NULL; bargeNode = bargeNode->next)
    {
    barge = bargeNode->val;
    fprintf(f, "barge %d clones %d chrom %s pos %1.2f to %1.2f\n",
	barge->id, dlCount(barge->cloneList), nameOrNa(barge->stsChrom),
	barge->stsStart, barge->stsEnd);
    for (cloneNode = barge->cloneList->head; cloneNode->next != NULL; cloneNode = cloneNode->next)
	{
	overlap = 0;
	clone = cloneNode->val;
	nextNode = cloneNode->next;
	if (nextNode->next != NULL)
	    {
	    nextClone = nextNode->val;
	    overlap = lookupOverlap(looseHash, clone, nextClone);
	    }
	maxName = "n/a";
	maxClone = clone->maxOverlapClone;
	if (maxClone != NULL) maxName = maxClone->name;
	fprintf(f, "%d\t%s %6d %6d  (%-8s %6d %4d)", clone->bargePos, clone->name, clone->size, overlap, 
		maxName, round(0.001*clone->maxOverlapPpt*clone->size), clone->maxOverlapPpt);
	if (clone->stsChrom != NULL)
	    fprintf(f, "\t%s %1.2f", clone->stsChrom, clone->stsPos);
	fprintf(f, "\n");
	}
    fprintf(f, "\n");
    }
fclose(f);
}

void barger(char *seqInf, char *pairFileName, char *loosePairName, char *stsFileName, char *outDir)
/* barger - make a clone map based on sequence overlap and sts positions. */
{
struct hash *chromHash = newHash(6);
struct hash *cloneHash = newHash(16);
struct hash *pairHash = newHash(17);
struct hash *looseHash = newHash(18);
struct oogClone *cloneList = NULL, *clone;
struct clonePair *tightList = NULL, *pair;
struct clonePair *looseList = NULL;
struct dlList *bargeList = newDlList();
char fileName[512];

/* Open log file. */
    {
    sprintf(fileName, "%s/barger.log", outDir);
    logFile = mustOpen(fileName, "w");
    }

/* Read input files. */
cloneList = readSeqInfo(seqInf, cloneHash);
printf("Read %d clones in %s\n", slCount(cloneList), seqInf);
cloneList = readStsFile(stsFileName, cloneHash, cloneList, chromHash);
tightList = readPairs(pairFileName, cloneHash, pairHash);
printf("Read %d pairs in %s\n", slCount(tightList), pairFileName);
looseList = readPairs(loosePairName, cloneHash, looseHash);
printf("Read %d pairs in %s\n", slCount(looseList), loosePairName);

/* Process and connect input so that we end up with a
 * prioritized list of clone pairs. */
sharesFromPairs(tightList);
countUses(tightList);
fillInScores(tightList);
findMaxOverlaps(cloneList, looseHash);
slSort(&tightList, cmpPairScore);

makeDir(outDir);

/* Output sorted pairs list. */
    {
    sprintf(fileName, "%s/sortedPairs", outDir);
    writePairs(fileName, tightList);
    }

/* Output loose pairs list. */
    {
    sprintf(fileName, "%s/looseList", outDir);
    writePairs(fileName, looseList);
    }

status("Making easy barges\n");
makeEasyBarges(cloneList, &tightList, bargeList, looseHash);
status("Made %d easy barges %d pairs left\n", dlCount(bargeList), slCount(tightList));

/* Output easy barges. */
    {
    sprintf(fileName, "%s/easyBarges", outDir);
    writeBarges(fileName, bargeList, looseHash);
    }

status("Making medium barges\n");
makeMediumBarges(cloneList, &tightList, bargeList, looseHash);
status("Made %d medium barges %d pairs left\n", dlCount(bargeList), slCount(tightList));
/* Output medium barges. */
    {
    sprintf(fileName, "%s/mediumBarges", outDir);
    writeBarges(fileName, bargeList, looseHash);
    }

status("Making hard barges\n");
makeHardBarges(cloneList, &tightList, bargeList, looseHash);
status("Made %d hard barges %d pairs left\n", dlCount(bargeList), slCount(tightList));
/* Output hard barges. */
    {
    sprintf(fileName, "%s/hardBarges", outDir);
    writeBarges(fileName, bargeList, looseHash);
    }


status("Placing enclosed barges\n");
placeEnclosedClones(cloneList, &tightList, bargeList, looseHash, hardMaxOverlap);
flipBargesAsNeeded(bargeList);
dlSort(bargeList, cmpBargePos);
/* Output hard barges. */
    {
    sprintf(fileName, "%s/overlapBarges", outDir);
    writeBarges(fileName, bargeList, looseHash);
    }

}


void usage()
/* Explain usage and exit. */
{
errAbort(
"barger - make a clone map based on sequence overlap and sts positions.\n"
"usage:\n"
"      barger sequence.inf strictPairs loosePairs inSts outDir\n"
"where inClones is a list of clones, inPairs contains a list of overlapping clones,\n"
"inSts contains sts positions and outDir is a directory where the various result\n"
"files are written. (outDir will be created if it doesn't exist).\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 6)
    usage();
barger(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}

