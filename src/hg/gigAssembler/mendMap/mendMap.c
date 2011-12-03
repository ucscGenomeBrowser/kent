/* mm - Map mender. */
#undef VERBOSE_LOG
#include "common.h"
#include "linefile.h"
#include "obscure.h"
#include "errabort.h"
#include "memalloc.h"
#include "portable.h"
#include "hash.h"
#include "fa.h"
#include "dlist.h"
#include "diGraph.h"
#include "localmem.h"
#include "hCommon.h"
#include "psl.h"
#include "qaSeq.h"


int version = 23;       /* Current version number. */
int maxMapDeviation = 700000;   /* No map deviations further than this allowed. */
boolean isPlaced;	/* TRUE if want to really follow map. */
FILE *logFile;	/* File to write decision steps to. */

char *contigName;	/* Name of contig from info file. */
char *contigType;	/* Contig type from info file. */

enum htgPhase
    {
    htgSurvey = 0,
    htgDraft = 1,
    htgOrdered = 2,
    htgFinished = 3,
    };

struct cloneEnd
/* Information about the end of a clone. */
    {
    struct cloneEnd *next;
    struct mmClone *clone;     /* Which clone they are part of. */
    char *frag;	/* Associated fragment.  NULL if unknown. */
    int orientation;            /* Orientation. */
    bool isStart;               /* True if this is start fragment. */
    int pos;			/* Approximate position. Only used when faking it....*/
    };

struct mmClone
/* Describes a clone (BAC) */
    {
    struct mmClone *next;      /* Next in clone list. */
    char *name;                /* Name of clone (accession). */
    char *bacName;	       /* Name of BAC (usually like RP11-something) */
    int mapPos;                /* Relative position in contig according to map. */
    int flipTendency;	       /* Tendency of clone to flip. */
    char *seqCenter;	       /* Sequencing center if known. */
    char *placeInfo;	       /* Placement info. */
    enum htgPhase phase;       /* HTG phase. */
    int size;                  /* Size in bases. */
    struct dlList *fragList;   /* List of frags in clone. */
    struct barge *barge;       /* Barge this is part of. */
    struct cloneEnd *sEnd;     /* One end frag . */
    struct cloneEnd *tEnd;     /* Other end frag . */
    struct bepRef *bepList;    /* Bac end pairs that hit this. */
    struct mmClone *bestFriend; /* Clone this overlaps with most. */
    int bestOverlap;            /* Overlap with best friend. */
    int mmPos;		        /* Relative position in contig according to mm. */
    struct mmClone *enclosed;   /* Clone enclosed by another?. */
    };

struct cloneRef 
/* A reference to a clone. */
    {
    struct cloneRef *next;	/* Next in list. */
    struct mmClone *clone;	/* Clone (not allocated here). */
    };

struct overlappingClonePair
/* Info on a pair of clones which overlaps. */
    {
    struct overlappingClonePair *next;  /* Next in list. */
    struct mmClone *a, *b;             /* Pair of clones. */
    int overlap;                        /* Size of strict overlap. */
    int sloppyOverlap;			/* Size of sloppy overlap. */
    int obliviousOverlap;		/* Size of truly oblivious overlap. */
    /* ------ Stuff added to ooMapMend ------ */
    char aHitS;	/* Values Y, N, and ? for corresponding end hitting, not hitting and unknown. */
    char aHitT;	/* Values Y, N, and ? for corresponding end hitting, not hitting and unknown. */
    char bHitS;	/* Values Y, N, and ? for corresponding end hitting, not hitting and unknown. */
    char bHitT;	/* Values Y, N, and ? for corresponding end hitting, not hitting and unknown. */
    int externalPriority;	/* External priority value - from hand edited parts of map. */
    int score;			/* Overall score. */
    };

struct barge
/* A barge - a set of welded together clones. */
    {
    struct barge *next;	    /* Next in list. */
    int id;                 /* Barge id. */
    int offset;                /* Offset within contig. */
    int mapPos;                /* Position within map. */
    int size;                  /* Total size. */
    /* ---- ooMapMend ---- */
    struct dlList *cloneEndList; /* Ordered list of clone ends. */
    struct dlList *cloneList;	/* List of clones on barge. Order not significant. */
    struct dlNode *dlNode;      /* Node in barge list. */
    };

struct bep
/* A BAC end pair. */
    {
    struct bep *next;	/* Next in list. */
    char *a;		/* Name of one read. */
    char *b;		/* Name of other read. */
    char *cloneName;	/* Name of clone. */
    struct psl *aList;	/* List of alignments a is in. */
    struct psl *bList;	/* List of alignments b is in. */
    struct cloneRef *cloneList;	/* List of clones this hits. */
    };

struct bepRef
/* A reference to a BAC end pair. */
    {
    struct bepRef *next;	/* Next in list. */
    struct bep *bep;		/* bep - not allocated here. */
    };

void warnHandler(char *format, va_list args)
/* Default error message handler. */
{
if (format != NULL) 
    {
    fprintf(stderr, "[warning] ");
    fprintf(logFile, "[warning] ");
    vfprintf(stderr, format, args);
    vfprintf(logFile, format, args);
    fprintf(stderr, "\n");
    fprintf(logFile, "\n");
#ifdef SOMETIMES
    fflush(logFile);
    uglyfBreak();
#endif
    }
}

void status(char *format, ...)
/* Print status message to stdout and to log file. */
{
va_list args;
va_start(args, format);
vfprintf(logFile, format, args);
vfprintf(stdout, format, args);
va_end(args);
}

void logIt(char *format, ...)
/* Print message to log file. */
{
va_list args;
va_start(args, format);
vfprintf(logFile, format, args);
va_end(args);
#ifdef SOMETIMES
fflush(logFile);
#endif /* SOMETIMES */
}

/* Make uglyf debugging statements go to log file too. */
#undef uglyf
#define uglyf status

char orientChar(int orient)
/* Return character representing orientation. */
{
if (orient > 0) return '+';
else if (orient < 0) return '-';
else return '.';
}

char otherStrandChar(char c)
/* Return orientation char for opposite strand. */
{
if (c == '-') return '+';
else return '-';
}

int maxMapDif(struct mmClone *a, struct mmClone *b)
/* Return max distance between clones in map coordinates. */
{
int aStart, aEnd, bStart, bEnd;

aStart = a->mapPos;
aEnd = a->mapPos + a->size;
bStart = b->mapPos;
bEnd = b->mapPos + b->size;
if (rangeIntersection(aStart, aEnd, bStart, bEnd) > 0)
   return 0;
if (aEnd <= bStart) 
    return bStart - aEnd;
else
    return aStart - bEnd;
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

struct overlappingClonePair *findHashedOcp(char *aName, char *bName, struct hash *ocpHash)
/* Find overlappingClonePair if any associated with a/b overlap. */
{
char *ocpName = ocpHashName(aName, bName);
return hashFindVal(ocpHash, ocpName);
}

double draftMinCoverage = 0.90;
double initialEncloseMin = 0.95;
double finalEncloseMin = 0.98;

boolean uglyerfOn = FALSE;

void uglyerfStart()
/* Set verbose debugging flag. */
{
uglyerfOn = TRUE;
uglyf("Getting uglyer\n");
}

void uglyerfEnd()
/* Clear verbose debugging flag. */
{
uglyerfOn = FALSE;
}

void uglyerf(char *format, ...)
/* Print status message to stdout and to log file if debugging flag set. */
{
if (uglyerfOn)
    {
    va_list args;
    va_start(args, format);
    vfprintf(logFile, format, args);
    vfprintf(stdout, format, args);
    va_end(args);
    }
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mm - Map mender\n"
  "usage:\n"
  "   mm contigDir\n");
}

int mmCloneCmpSize(const void *va, const void *vb)
/* Compare to sort biggest sized first. */
{
const struct mmClone *a = *((struct mmClone **)va);
const struct mmClone *b = *((struct mmClone **)vb);
return b->size - a->size;
}

int mmCloneCmpMapPos(const void *va, const void *vb)
/* Compare to sort biggest sized first. */
{
const struct mmClone *a = *((struct mmClone **)va);
const struct mmClone *b = *((struct mmClone **)vb);
return a->mapPos - b->mapPos;
}


struct cloneEnd *newCloneEnd(struct mmClone *clone,  char *frag)
/* Allocate and initialize a clone end. */
{
struct cloneEnd *ce;
AllocVar(ce);
ce->clone = clone;
ce->frag = frag;
return ce;
}

void mmLoadClones(char *fileName, struct mmClone **retCloneList, struct hash **retCloneHash)
/* Load clones from file into list/hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[7];
struct mmClone *cloneList = NULL, *clone;
struct hash *cloneHash = newHash(11);

while (lineFileRow(lf, words))
    {
    if (!sameString(words[3], "0"))
	{
	AllocVar(clone);
	slAddHead(&cloneList, clone);
	hashAddSaveName(cloneHash, words[0], clone, &clone->name);
	clone->size = lineFileNeedNum(lf, words, 2);
	clone->phase = atoi(words[3]);
	}
    }
lineFileClose(&lf);
slSort(&cloneList, mmCloneCmpMapPos);
*retCloneList = cloneList;
*retCloneHash = cloneHash;
}

void mmAddInfo(char *fileName, struct mmClone *cloneList, struct hash *cloneHash)
/* Read in .info file and add map position to it. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *header[2], *words[4];
struct mmClone *clone;
double flipMod;

/* Parse first line and save it in global variables.. */
if (!lineFileRow(lf, header))
    errAbort("Couldn't read header from %s", fileName);
contigName = cloneString(header[0]);
contigType = cloneString(header[1]);

/* Set positions to special flag value. */
for (clone = cloneList; clone != NULL; clone = clone->next)
    clone->mapPos = -BIGNUM;

/* Read in positions from file. */
while (lineFileRow(lf, words))
    {
    int phase = lineFileNeedNum(lf, words, 2);
    if (phase != 0)
        {
	if ((clone = hashFindVal(cloneHash, words[0])) == NULL)
	    errAbort("Clone %s is in info but not cloneInfo file", words[0]);
	clone->mapPos = lineFileNeedNum(lf, words, 1) * 1000;
	flipMod = sqrt(clone->size/300000.0);
	if (flipMod < 1.0) flipMod = 1.0;
	clone->flipTendency = lineFileNeedNum(lf, words, 3) * 1000 * flipMod;
	}
    }
lineFileClose(&lf);

/* Check positions all there. */
for (clone = cloneList; clone != NULL; clone = clone->next)
    if (clone->mapPos == -BIGNUM)
        errAbort("%s is in cloneInfo but not %s", clone->name, fileName);
}

void mmAddEndInfo(char *fileName, struct mmClone *cloneList, struct hash *cloneHash)
/* Add info on clone ends from file.  Fake the rest. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[10];
int wordCount;
struct mmClone *clone;
static char dummyFrag[] = "dummyFrag";

/* Read ends from file. */
while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    if (wordCount != 9 && wordCount != 7)
        errAbort("Expecting 7 or 9 words line %d of %s, got %d", 
		lf->lineIx, lf->fileName, wordCount);
    if ((clone = hashFindVal(cloneHash, words[0])) == NULL)
	errAbort("Clone %s is in %s but not cloneInfo file", words[0], fileName);
    clone->sEnd = newCloneEnd(clone, dummyFrag);
    clone->tEnd = newCloneEnd(clone, (wordCount == 9 ? dummyFrag : NULL));
    }
lineFileClose(&lf);

/* If no ends in file then make them up using phase info. */
for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    if (clone->sEnd == NULL)
        {
	if (clone->phase >= 2)	/* Ordered or finished? */
	    {
	    clone->sEnd = newCloneEnd(clone, dummyFrag);
	    clone->tEnd = newCloneEnd(clone, dummyFrag);
	    }
	else
	    {
	    clone->sEnd = newCloneEnd(clone, NULL);
	    clone->tEnd = newCloneEnd(clone, NULL);
	    }
	}
    }
}

void mmAddMap(char *fileName, struct mmClone *cloneList, struct hash *cloneHash)
/* Add info from map file about sequencing center and placement. */
{
struct lineFile *lf = lineFileMayOpen(fileName, TRUE);
struct mmClone *clone;
char *row[6];
int wordCount;

if (lf == NULL)
    return;
while ((wordCount = lineFileChop(lf, row)) != 0)
    {
    if (wordCount < 6)
        continue;
    if (sameString(row[0], "COMMITTED"))
        continue;
    if ((clone = hashFindVal(cloneHash, row[0])) != NULL)
        {
	if (startsWith("ctg", row[3]))
	    {
	    clone->bacName = cloneString(row[1]);
	    clone->seqCenter = cloneString(row[2]);
	    clone->placeInfo = cloneString(row[5]);
	    }
	}
    }
lineFileClose(&lf);
}

int ocpCmpScore(const void *va, const void *vb)
/* Compare to sort based on score. */
{
const struct overlappingClonePair *a = *((struct overlappingClonePair **)va);
const struct overlappingClonePair *b = *((struct overlappingClonePair **)vb);
return b->score - a->score;
}

void ocpDump(struct overlappingClonePair *ocp, FILE *f)
/* Print out ocp to file. */
{
fprintf(f, "%-12s %c %c     %-12s %c %c   %6d %6d %6d  %d %d\n",  
	ocp->a->name, ocp->aHitS, ocp->aHitT, 
	ocp->b->name, ocp->bHitS, ocp->bHitT, 
	ocp->overlap, ocp->sloppyOverlap, ocp->obliviousOverlap, 
	ocp->externalPriority, ocp->score);
}

boolean ocpGreatEnds(char a, char b)
/* Returns TRUE if one of A is 'Y' and the other 'N' */
{
return (a == 'Y' && b == 'N') || (a == 'N' && b == 'Y');
}

boolean ocpHalfGood(char a, char b)
/* Returns TRUE if is of form ?N ?Y Y? N? */
{
return (a == '?' && (b == 'Y' || b == 'N')) || (b == '?' && (a == 'Y' || a == 'N'));
}

boolean ocpUnknownEnds(char a, char b)
/* Returns TRUE if is of form ?? */
{
return a == '?' && b == '?';
}

boolean ocpDoubleMiss(char a, char b)
/* Returns TRUE if is of form NN */
{
return a == 'N' && b == 'N';
}

boolean ocpDoubleHit(char a, char b)
/* RETURNS true if is of form YY */
{
return a == 'Y' && b == 'Y';
}

int ocpScore(struct overlappingClonePair *ocp, boolean ignoreEnds)
/* Figure out score for ocp so that best looking overlaps have highest score. */
{
int score = 0;
int as = ocp->aHitS, at = ocp->aHitT, bs = ocp->bHitS, bt = ocp->bHitT;
struct mmClone *a = ocp->a, *b = ocp->b;
double size = max(a->size, b->size);
double dist;
struct mmClone *outerClone = NULL, *innerClone = NULL;
int overlap = (ignoreEnds ? ocp->sloppyOverlap : ocp->overlap);
boolean bothFin = (a->phase == htgFinished && b->phase == htgFinished);
boolean bothGreat = (ocpGreatEnds(as, at) && ocpGreatEnds(bs, bt));

score += round(100000.0 * overlap / size);
score += ocp->externalPriority * 10000;
dist = maxMapDif(a, b);
if (dist > maxMapDeviation)
    {
    if (isPlaced && bothGreat && bothFin && dist < maxMapDeviation*4)
        score -= 600000;
    else
	score -= 1200000;
    }
if (ignoreEnds)
    {
    score -= dist/50;
    return score;
    }
else
    score -= dist/10;

if (ocpDoubleMiss(as, at))
    {
    outerClone = a;
    innerClone = b;
    }
if (ocpDoubleMiss(bs, bt))
    {
    innerClone = b;
    outerClone = a;
    }
if (bothGreat)
    {
    score += 1000000;
    bothGreat = TRUE;
    if (bothFin)
        score += 40000;
    }
else if (ocpGreatEnds(as, at) && ocpHalfGood(bs, bt))
    score += 500000;
else if (ocpGreatEnds(bs, bt) && ocpHalfGood(as, at))
    score += 500000;
else if (ocpHalfGood(as, at) && ocpHalfGood(bs, bt))
    score += 200000;
else if (ocpGreatEnds(as, at) && ocpUnknownEnds(bs, bt))
    score += 100000;
else if (ocpGreatEnds(bs, bt) && ocpUnknownEnds(as, at))
    score += 100000;
else if (ocpHalfGood(as, at) && ocpUnknownEnds(bs, bt))
    score += 50000;
else if (ocpHalfGood(bs, bt) && ocpUnknownEnds(as, at))
    score += 50000;
else if (outerClone != NULL)
    {
    int innerSize = innerClone->size;
    if (innerClone->phase != htgFinished)
        innerSize /= draftMinCoverage;
    if (ocp->overlap < innerSize)
        score -= 1000000;
    }
return score;
}

void updateBestFriend(struct mmClone *clone, struct mmClone *aFriend, int overlap)
/* See if aFriend is better than current best friend, and if so update best friend. */
{
if (overlap > clone->bestOverlap)
    {
    clone->bestOverlap = overlap;
    clone->bestFriend = aFriend;
    }
}

void mmLoadOcp(char *fileName, struct hash *cloneHash,
	struct overlappingClonePair **retOcpList, struct hash **retOcpHash)
/* Load up overlaps from file into list/hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[12];
struct overlappingClonePair *ocpList = NULL, *ocp;
struct hash *ocpHash = newHash(12);

while (lineFileRow(lf, words))
    {
    AllocVar(ocp);
    slAddHead(&ocpList, ocp);
    hashAdd(ocpHash, ocpHashName(words[0], words[4]), ocp);
    ocp->a = hashMustFindVal(cloneHash, words[0]);
    ocp->b = hashMustFindVal(cloneHash, words[4]);
    ocp->overlap = lineFileNeedNum(lf, words, 8);
    ocp->sloppyOverlap = lineFileNeedNum(lf, words, 9);
    ocp->obliviousOverlap = lineFileNeedNum(lf, words, 10);
    ocp->aHitS = words[2][0];
    ocp->aHitT = words[3][0];
    ocp->bHitS = words[6][0];
    ocp->bHitT = words[7][0];
    ocp->externalPriority = lineFileNeedNum(lf, words, 11);
    updateBestFriend(ocp->a, ocp->b, ocp->sloppyOverlap);
    updateBestFriend(ocp->b, ocp->a, ocp->sloppyOverlap);
    }
lineFileClose(&lf);
*retOcpList = ocpList;
*retOcpHash = ocpHash;
}

void ocpScoreAndSort(struct overlappingClonePair **pOcpList, boolean ignoreEnds)
/* Fill in scores on ocp list and sort with best scoring first.
 * Optionally ignore end info in scoreing. */
{
struct overlappingClonePair *ocp;

for (ocp = *pOcpList; ocp != NULL; ocp = ocp->next)
    ocp->score = ocpScore(ocp, ignoreEnds);
slSort(pOcpList, ocpCmpScore);
}

void cloneRefAddUnique(struct cloneRef **pRefList, struct mmClone *clone)
/* Add clone reference to list if not already on list. */
{
refAddUnique((struct slRef**)pRefList, clone);
}

void cloneRefAdd(struct cloneRef **pRefList, struct mmClone *clone)
/* Add clone reference to list. */
{
refAdd((struct slRef**)pRefList, clone);
}


void bepRefAddUnique(struct bepRef **pRefList, struct bep *bep)
/* Add clone reference to list if not already on list. */
{
refAddUnique((struct slRef**)pRefList, bep);
}

void mmLoadBacEndPairs(char *fileName, struct bep **retBepList, struct hash **retBepHash)
/* Make up list of bac end pairs from file.  Create hash keyed into either read of
 * pair. */
{
struct hash *bepHash = newHash(0);
struct bep *bepList = NULL, *bep;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[4], *line;
int wordCount, lineSize;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
        continue;
    wordCount = chopTabs(line, words);
    if (wordCount == 0)
        continue;
    lineFileExpectWords(lf, 3, wordCount);
    AllocVar(bep);
    slAddHead(&bepList, bep);
    hashAddSaveName(bepHash, words[0], bep, &bep->a);
    hashAddSaveName(bepHash, words[1], bep, &bep->b);
    bep->cloneName = cloneString(words[2]);
    }
lineFileClose(&lf);
slReverse(&bepList);
*retBepList = bepList;
*retBepHash = bepHash;
}

boolean filterBacEndPsl(struct psl *psl)
/* Figure out if a BAC end psl is a keeper. */
{
int startTail, endTail;
if (psl->match < 30)
    return FALSE;
if (psl->match + psl->repMatch < 60)
    return FALSE;
pslTailSizes(psl, &startTail, &endTail);
if (startTail > 50 || endTail > 50)
    return FALSE;
if (pslCalcMilliBad(psl, FALSE) > 50)
    return FALSE;
return TRUE;	
}

void mmLoadBacEndPsl(char *fileName, struct hash *bepHash, struct hash *cloneHash)
/* Load in decent looking alignments from bacEnd.psl, and fill in info
 * linking bacEnds and clones based on this. */
{
struct lineFile *lf = pslFileOpen(fileName);
struct bep *bep;
struct mmClone *clone;
char cloneName[128];
int count = 0, goodCount = 0;
struct psl *psl;

while ((psl = pslNext(lf)) != NULL)
    {
    ++count;
    bep = hashFindVal(bepHash, psl->qName);
    if (bep == NULL || !filterBacEndPsl(psl))
	{
	pslFree(&psl);
        continue;
	}
    ++goodCount;
    fragToCloneName(psl->tName, cloneName);
    clone = hashFindVal(cloneHash, cloneName);
    if (clone == NULL)
        {
	warn("Clone %s is in %s but not cloneInfo", cloneName, fileName);
	continue;
	}
    cloneRefAddUnique(&bep->cloneList, clone);
    bepRefAddUnique(&clone->bepList, bep);
    if (sameString(psl->qName, bep->a))
	{
	slAddHead(&bep->aList, psl);
	}
    else
        {
	assert(sameString(psl->qName, bep->b));
	slAddHead(&bep->bList, psl);
	}
    }
lineFileClose(&lf);
}

boolean cloneHasKnownEnd(struct mmClone *clone)
/* Return TRUE if clone has a known end. */
{
return clone->sEnd->frag != NULL || clone->tEnd->frag != NULL;
}

struct barge *bargeOfOne(struct mmClone *clone, struct dlList *bargeList)
/* Construct a barge around a single clone. */
{
struct barge *barge;
struct cloneEnd *start, *end;

AllocVar(barge);
barge->cloneEndList = newDlList();
barge->cloneList = newDlList();
dlAddValTail(barge->cloneList, clone);
barge->dlNode = dlAddValTail(bargeList, barge);
start = CloneVar(clone->sEnd);
start->isStart = TRUE;
dlAddValTail(barge->cloneEndList, start);
end = CloneVar(clone->tEnd);
end->isStart = FALSE;
if (clone->sEnd->frag || clone->tEnd->frag)
    start->orientation = end->orientation = 1;
dlAddValTail(barge->cloneEndList, end);
clone->barge = barge;
return barge;
}

void findCloneEndsOnList(struct dlList *list, struct mmClone *clone, 
	struct dlNode **retStart, struct dlNode **retEnd)
/* Find start and end nodes associated with clone on list. */
{
struct dlNode *node;
struct cloneEnd *ce;
boolean gotStart = FALSE, gotEnd = FALSE;
for (node = list->head; !dlEnd(node); node = node->next)
     {
     ce = node->val;
     if (ce->clone == clone)
         {
	 if (!gotStart)
	     {
	     assert(ce->isStart);
	     gotStart = TRUE;
	     *retStart = node;
	     }
	 else
	     {
	     assert(!ce->isStart);
	     gotEnd = TRUE;
	     *retEnd = node;
	     break;
	     }
         }
     }
if (!gotEnd)
    errAbort("Couldn't find ends of %s in list", clone->name);
}

char ocpHitsHow(struct overlappingClonePair *ocp, struct mmClone *clone, 
	int relPos, boolean ignoreEnds)
/* Return how other clone in pair hits clone assuming other end is oriented. 
 * RelPos is whether clone is to left (+1) or right (-1) of other. */
{
char sHit, tHit;
if (ignoreEnds)
    return '?';
if (clone == ocp->a)
    {
    sHit = ocp->aHitS;
    tHit = ocp->aHitT;
    }
else
    {
    assert(clone == ocp->b);
    sHit = ocp->bHitS;
    tHit = ocp->bHitT;
    }
if (relPos > 0)
    return tHit;
else if (relPos < 0)
    return sHit;
else
    return '?';
}

struct fitAt
/* A possible position clone fits. */
    {
    struct fitAt *next;
    struct dlNode *start, *end;		/* cloneEnd list nodes where clone goes. */
    int orientation;			/* +1, -1 or 0. */
    boolean addAfter;			/* True if add clone after start/end. */
    int score;				/* Some sort of score. */
    };

void dumpFit(struct fitAt *fit, struct mmClone *newbie)
/* Print out info on fit. */
{
struct cloneEnd *se = fit->start->val, *ee = fit->end->val;
logIt(" %s fits %s %s (%s) %s (%s), score %d\n",
    newbie->name, 
    fit->addAfter ? "after" : "before",
    se->clone->name,
    se->isStart ? "start" : "end",
    ee->clone->name,
    ee->isStart ? "start" : "end",
    fit->score);
}

int fitAtCmpScore(const void *va, const void *vb)
/* Compare to sort based on score. */
{
const struct fitAt *a = *((struct fitAt **)va);
const struct fitAt *b = *((struct fitAt **)vb);
return b->score - a->score;
}

int overlapSize(struct mmClone *a, struct mmClone *b, struct hash *ocpHash, boolean sloppy)
/* Return size of overlap between a and b. */
{
struct overlappingClonePair *ocp;

if (a == b)
    return a->size;
ocp = findHashedOcp(a->name, b->name, ocpHash);
if (ocp == NULL)
    return 0;
if (sloppy)
    return ocp->sloppyOverlap;
else
    return ocp->overlap;
}

int addMissingOverlap(struct dlNode *startN, struct dlNode *endN, 
	struct mmClone *newbie, struct hash *ocpHash, boolean sloppy)
/* Add together all overlaps that should be present between
 * newbie and clones, assuming that newbie covers all of the
 * ends in between startN and endN (including startN and endN) 
 * The result will be 0 if all overlaps are present, or a negative
 * number if some are missed. 
 *    This routine assumes that there are no clones fully enclosed
 * between startN and endN. */
{
struct cloneEnd *startE = startN->val, *endE = endN->val, *ce;
struct mmClone *startC = startE->clone, *endC = endE->clone, *clone;
int missedOverlap = 0, missedOne, minOverlap, overlap;
struct dlNode *node;

for (node = startN; node != endN->next; node = node->next)
    {
    ce = node->val;
    clone = ce->clone;
    if (ce->isStart)
        {
	if (endE->isStart)
	    minOverlap = clone->size - overlapSize(clone, endC, ocpHash, sloppy);
	else
	    minOverlap = overlapSize(clone, endC, ocpHash, sloppy);
	}
    else
        {
	if (startE->isStart)
	    minOverlap =  overlapSize(clone, startC, ocpHash, sloppy);
	else
	    minOverlap = clone->size - overlapSize(clone, startC, ocpHash, sloppy);
	}
    overlap = overlapSize(clone, newbie, ocpHash, sloppy);
    missedOne = overlap - round(minOverlap*0.90);
    if (missedOne < 0)
        missedOverlap += missedOne;
    }
return missedOverlap;
}

int addPresentOverlap(struct dlNode *innerStart, struct dlNode *innerEnd, 
	struct mmClone *newbie, struct hash *ocpHash, boolean sloppy)
/* Return amount that newbie overlaps with it's two neighbors,
 * (don't count same bits twice). */
{
struct cloneEnd *ce;
struct mmClone *before = NULL, *after = NULL;
struct dlNode *node;

/* Figure out first clone before if any. */
for (node = innerStart->prev; !dlStart(node); node = node->prev)
    {
    ce = node->val;
    if (ce->isStart)
        {
	before = ce->clone;
	break;
	}
    }

/* Figure out first node after if any. */
for (node = innerEnd->next; !dlEnd(node); node = node->next)
    {
    ce = node->val;
    if (!ce->isStart)
        {
	after = ce->clone;
	break;
	}
    }

if (before == NULL)
    return overlapSize(newbie, after, ocpHash, sloppy);
else if (after == NULL)
    return overlapSize(newbie, before, ocpHash, sloppy);
else
    {
    return overlapSize(newbie, after, ocpHash, sloppy) + overlapSize(newbie, before, ocpHash, sloppy) 
                         - overlapSize(before, after, ocpHash, sloppy);
    }
}


boolean insideEndsFit(struct mmClone *newbie, 
	struct dlNode *innerStart, struct dlNode *outerEnd, 
	struct hash *ocpHash, boolean ignoreEnds)
/* Return TRUE if all known ends from innerStart to outerEnd (half open interval)
 * hit newbie, and that newbie overlaps with all clones in interval. */
{
struct dlNode *n1;
struct mmClone *clone;
struct cloneEnd *ce;
struct overlappingClonePair *ocp;
int lorient;
char how;

logIt("       insideEndsFit: newbie %s\n", newbie->name);

if (uglyerfOn)
    {
    struct dlNode *innerEnd = outerEnd->prev;
    struct cloneEnd *se = innerStart->val, *ee = innerEnd->val;
    uglyerf("        %s (%s) to %s (%s)\n",
         se->clone->name, (se->isStart ? "start" : "end"),
         ee->clone->name, (ee->isStart ? "start" : "end"));   
    }

/* See if this implies overlaps that don't exist. */
for (n1 = innerStart; n1 != outerEnd; n1 = n1->next)
    {
    ce = n1->val;
    clone = ce->clone;
    ocp = findHashedOcp(newbie->name, clone->name, ocpHash);
    if (ocp == NULL)
	{
	logIt("        no overlap between %s and %s\n", newbie->name, clone->name);
	return FALSE;
	}
    if (ocp->overlap == 0 && !ignoreEnds)
        {
	logIt("        ignoring sloppy overlap %d between %s and %s\n", ocp->sloppyOverlap, 
		newbie->name, clone->name);
	return FALSE;
	}
    /* Check end specific matches. */
    lorient = ce->orientation;
    if (ce->isStart)
	lorient = -lorient;
    how = ocpHitsHow(ocp, clone, lorient, ignoreEnds);
    if (how == 'N')
	{
	logIt("        %s (%s) doesn't hit %s\n", clone->name, 
		(ce->isStart ? "start" : "end"), newbie->name);
	return FALSE;
	}
    }
return TRUE;
}

struct fitAt *cloneFitsAt(struct mmClone *newbie, struct dlNode *startN, struct dlNode *endN,
	boolean addAfter, int newbieOrientation, 
	struct mmClone *member, struct dlNode *memStart, 
	struct dlNode *memEnd, struct hash *ocpHash, boolean ignoreEnds)
/* Return a fitAt structure if clone can fit at position . */
{
struct dlNode *n1, *n2;
struct dlNode *innerStart, *outerEnd;
struct cloneEnd *ce;
struct mmClone *clone;
struct fitAt *fit;
int score, minScore;
int missingOverlap, presentOverlap;
int overlapCloneCount = 0;


/* Figure out the first end inside of newbie, and the first end after 
 * newbie. */
if (addAfter)
    {
    innerStart = startN->next;
    outerEnd = endN->next;
    }
else
    {
    innerStart = startN;
    outerEnd = endN;
    }

if (uglyerfOn)
    {
    struct dlNode *innerEnd = outerEnd->prev;
    struct cloneEnd *se = innerStart->val, *ee = innerEnd->val;
    uglyerf("   cloneFitsAt (inside) %s (%s) to %s (%s)\n",
         se->clone->name, (se->isStart ? "start" : "end"),
         ee->clone->name, (ee->isStart ? "start" : "end"));   
    }

/* See if this would lead to new clone enclosing another. */
for (n1 = innerStart; n1 != outerEnd; n1 = n1->next)
    {
    ce = n1->val;
    clone = ce->clone;
    overlapCloneCount += 1;
    if (ce->isStart)
        {
	for (n2 = n1->next; n2 != outerEnd; n2 = n2->next)
	    {
	    ce = n2->val;
	    if (ce->clone == clone)
	        {
		return NULL;
		}
	    }
	}
    }


/* See if this would lead to new clone being enclosed. */
if (addAfter)
    {
    for (n1 = memStart; n1 != memEnd; n1 = n1->next)
	{
	boolean newbieStartsInside = FALSE, newbieEndsInside = FALSE;
	ce = n1->val;
	clone = ce->clone;
	if (ce->isStart)
	    {
	    for (n2 = n1; ;n2 = n2->next)
		{
		ce = n2->val;
		if (n2 == startN)
		    newbieStartsInside = TRUE;
		if (ce->clone == clone && !ce->isStart)
		    break;
		if (n2 == endN)
		    newbieEndsInside = TRUE;
		}
	    }
	else
	    {
	    for (n2 = n1->prev; ;n2 = n2->prev)
		{
		if (n2 == endN)
		    newbieEndsInside = TRUE;
		if (n2 == startN)
		    newbieStartsInside = TRUE;
		ce = n2->val;
		if (ce->clone == clone && ce->isStart)
		    break;
		}
	    }
	if (newbieStartsInside && newbieEndsInside)
	    return NULL;
	}
    }
else
    {
    for (n1 = memEnd; n1 != memStart; n1 = n1->prev)
	{
	boolean newbieStartsInside = FALSE, newbieEndsInside = FALSE;
	ce = n1->val;
	clone = ce->clone;
	if (!ce->isStart)
	    {
	    for (n2 = n1; ;n2 = n2->prev)
		{
		ce = n2->val;
		if (n2 == endN)
		    newbieEndsInside = TRUE;
		if (ce->clone == clone && ce->isStart)
		    break;
		if (n2 == startN)
		    newbieStartsInside = TRUE;
		}
	    }
	else
	    {
	    for (n2 = n1->next; ;n2 = n2->next)
		{
		if (n2 == startN)
		    newbieStartsInside = TRUE;
		if (n2 == endN)
		    newbieEndsInside = TRUE;
		ce = n2->val;
		if (ce->clone == clone && !ce->isStart)
		    break;
		}
	    }
	if (newbieStartsInside && newbieEndsInside)
	    return NULL;
	}
    }


if (!insideEndsFit(newbie, innerStart, outerEnd, ocpHash, ignoreEnds))
    return NULL;

/* Score fit according to how well overlap distances add up. */
missingOverlap = addMissingOverlap(innerStart, outerEnd->prev, newbie, ocpHash, ignoreEnds);
presentOverlap = addPresentOverlap(innerStart, outerEnd->prev, newbie, ocpHash, ignoreEnds);
score = (presentOverlap<<4) + (missingOverlap<<6) + overlapCloneCount;
minScore = overlapCloneCount + 1;
if (score < minScore)
    return NULL;

/* Allocate and fill in fit structure. */
AllocVar(fit);
fit->start = startN;
fit->end = endN;
fit->orientation = newbieOrientation;
fit->addAfter = addAfter;
fit->score = score;

dumpFit(fit, newbie);
return fit;
}

void addFittedClone(struct barge *barge, struct mmClone *clone, struct fitAt *fit)
/* Add clone at specific position in barge. */
{
struct dlNode *(*listAdder)(struct dlNode *anchor, void *val);
struct dlNode *firstNode;
struct cloneEnd *start,*end;

listAdder = (fit->addAfter ? dlAddValAfter : dlAddValBefore);
dlAddValTail(barge->cloneList, clone);
clone->barge = barge;
if (fit->orientation < 0)
    {
    start = clone->tEnd;
    end = clone->sEnd;
    }
else
    {
    start = clone->sEnd;
    end = clone->tEnd;
    }
start = CloneVar(start);
end = CloneVar(end);
start->isStart = TRUE;
end->isStart = FALSE;
start->orientation = end->orientation = fit->orientation;
firstNode = (*listAdder)(fit->start, start);
if (fit->end != fit->start)
    (*listAdder)(fit->end, end);
else
    dlAddValAfter(firstNode, end);
}

enum orientTypes 
/* Possible clone orientations. */
  {
  orientImpossible = -2,
  orientForward = 1,
  orientReverse = -1,
  orientUnknown = 0,
  };

enum orientTypes ocpCloneOrientation(struct overlappingClonePair *ocp, 
	struct mmClone *clone, boolean ignoreEnds)
/* Return orientation of clone relative to other clone in pair based on end matches. 
 * Returns 1 (orientForward) if it appears clone comes after other clone. */
{
char how, howFlip;

how = ocpHitsHow(ocp, clone, -1, ignoreEnds);
howFlip = ocpHitsHow(ocp, clone, 1, ignoreEnds);
if (how == 'N' && howFlip == 'N')
    return orientImpossible;
if (how == 'Y')
    return orientForward;
else if (howFlip == 'Y')
    return orientReverse;
else if (how == 'N')
    return orientReverse;
else if (howFlip == 'N')
    return orientForward;
else if (howFlip == '?' && how == '?')
    return orientUnknown;
else
    {
    warn("Case that should never happen in ocpCloneOrientation.");
    assert(FALSE);
    return orientImpossible;
    }
}

struct fitAt *bargeFitListLeft(struct barge *barge, 
	struct mmClone *newbie,  struct mmClone *member,
	struct dlNode *memStart, struct dlNode *memEnd, 
	struct overlappingClonePair *joinOcp,
	struct hash *ocpHash, boolean ignoreEnds)
/* Return scored list of all places newbie could fit overlapping member
 * to left in barge. */
{
char how, howFlip;
int newbieOrientation = 0, memberOrientation;
struct dlNode *node, *firstStart, *n1, *n2;
struct cloneEnd *ce;
struct mmClone *clone;
struct overlappingClonePair *ocp;
struct fitAt *fitList = NULL, *fit;

uglyerf("bargeFitListLeft(newbie %s, member %s)\n", newbie->name, member->name);
ce = memStart->val;
memberOrientation = ce->orientation;

how = ocpHitsHow(joinOcp, member, -memberOrientation, ignoreEnds);
uglyerf(" how = %c\n", how);
if (how == 'N')
    return FALSE;
howFlip = ocpHitsHow(joinOcp, member, memberOrientation, ignoreEnds);
uglyerf(" howFlip = %c\n", howFlip);
if (howFlip == 'Y')
    return FALSE;	/* Might eventually want to just decrease score greatly. */
if (cloneHasKnownEnd(newbie))
    {
    newbieOrientation = ocpCloneOrientation(joinOcp, newbie, ignoreEnds);
    if (newbieOrientation == orientImpossible)
        return FALSE;
    newbieOrientation = -newbieOrientation;
    }
uglyerf(" newbieOrientation = %d\n", newbieOrientation);


/* Figure out where can stop scanning for newbie start sites
 * because have a clone ending that newbie doesn't overlap. */

uglyerf(" scanning for firstStart\n");
for (node = memStart->prev; !dlStart(node); node = node->prev)
    {
    ce = node->val;
    uglyerf("    %s %s\n", ce->clone, (ce->isStart ? "start" : "end"));
    if (!ce->isStart)
        {
	clone = ce->clone;
	ocp = findHashedOcp(clone->name, newbie->name, ocpHash);
	if (ocp == NULL)
	    break;
	}
    }
firstStart = node;
    {
    if (dlStart(node))
	{
	uglyerf("First node past beginning\n", ce->clone->name);
	}
    else
	{
	ce = node->val;
	uglyerf("First node %s\n", ce->clone->name);
	}
    }

for (n1 = memEnd; n1 != memStart; n1 = n1->prev)
    {
    for (n2 = memStart; n2 != firstStart; n2 = n2->prev)
        {
	if ((fit = cloneFitsAt(newbie, n2, n1, FALSE, 
		newbieOrientation, member, memStart, memEnd, ocpHash, ignoreEnds)) != NULL)
	    {
	    slAddHead(&fitList, fit);
	    }
	}
    }
return fitList;
}

boolean bargeAddLeft(struct barge *barge, 
	struct mmClone *newbie,  struct mmClone *member,
	struct dlNode *memStart, struct dlNode *memEnd, 
	struct overlappingClonePair *joinOcp,
	struct hash *ocpHash, boolean doAdd,
	boolean ignoreEnds, int *retOrientation)
/* See if can add barge to left of member (which is defined by it's ends on
 * barge->cloneEndList) */
{
struct fitAt *fitList, *fit;
fitList = bargeFitListLeft(barge, newbie, member, memStart, memEnd, joinOcp, ocpHash, ignoreEnds);
slSort(&fitList, fitAtCmpScore);
if ((fit = fitList) != NULL)
    {
    if (doAdd)
        addFittedClone(barge, newbie, fit);
    *retOrientation = fit->orientation;
    }
slFreeList(&fitList);
return fit != NULL;
}

struct fitAt *bargeFitListRight(struct barge *barge, 
	struct mmClone *newbie,  struct mmClone *member,
	struct dlNode *memStart, struct dlNode *memEnd, 
	struct overlappingClonePair *joinOcp,
	struct hash *ocpHash, boolean ignoreEnds)
/* Return scored list of all places newbie could fit overlapping member
 * to right in barge. */
{
char how, howFlip;
int newbieOrientation = 0, memberOrientation ;
struct dlNode *node, *lastEnd, *n1, *n2;
struct cloneEnd *ce;
struct mmClone *clone;
struct overlappingClonePair *ocp;
struct fitAt *fitList = NULL, *fit;
boolean badYes = FALSE;
int score = 0;

ce = memStart->val;
memberOrientation = ce->orientation;
how = ocpHitsHow(joinOcp, member, memberOrientation, ignoreEnds);
if (how == 'N')
    return FALSE;
howFlip = ocpHitsHow(joinOcp, member, -memberOrientation, ignoreEnds);
if (howFlip == 'Y')
    badYes = TRUE;
if (cloneHasKnownEnd(newbie))
    {
    newbieOrientation = ocpCloneOrientation(joinOcp, newbie, ignoreEnds);
    if (newbieOrientation == orientImpossible)
        return FALSE;
    }


/* Figure out where can stop scanning for newbie end sites
 * because have a clone starting that newbie doesn't overlap. */
for (node = memEnd->next; !dlEnd(node); node = node->next)
    {
    ce = node->val;
    if (ce->isStart)
        {
	clone = ce->clone;
	ocp = findHashedOcp(clone->name, newbie->name, ocpHash);
	if (ocp == NULL)
	    break;
	}
    }
lastEnd = node;

for (n1 = memStart; n1 != memEnd; n1 = n1->next)
    {
    for (n2 = memEnd; n2 != lastEnd; n2 = n2->next)
        {
	if ((fit = cloneFitsAt(newbie, n1, n2, TRUE, 
		newbieOrientation, member, memStart, memEnd, ocpHash, ignoreEnds)) != NULL)
	    {
	    if (badYes)
		{
		if (score > 0)
		    score /= 4;
		score -= 100;
		}
	    slAddHead(&fitList, fit);
	    }
	}
    }
return fitList;
}

boolean bargeAddRight(struct barge *barge, 
	struct mmClone *newbie,  struct mmClone *member,
	struct dlNode *memStart, struct dlNode *memEnd, 
	struct overlappingClonePair *joinOcp,
	struct hash *ocpHash, boolean doAdd,
	boolean ignoreEnds, int *retOrientation)
/* See if can add barge to right of member (which is defined by it's ends on
 * barge->cloneEndList) */
{
struct fitAt *fitList, *fit;
fitList = bargeFitListRight(barge, newbie, member, memStart, memEnd, joinOcp, ocpHash, ignoreEnds);
slSort(&fitList, fitAtCmpScore);
if ((fit = fitList) != NULL)
    {
    if (doAdd)
        addFittedClone(barge, newbie, fit);
    *retOrientation = fit->orientation;
    }
slFreeList(&fitList);
return fit != NULL;
}

void dumpCloneEnds(struct dlList *list, FILE *f)
/* Dump out info on clone ends in list to f. */
{
struct dlNode *node;
int orientation;

for (node = list->head; !dlEnd(node); node = node->next)
    {
    struct cloneEnd *ce = node->val;
    orientation = ce->orientation;
    fprintf(f, " ");
    if (ce->isStart)
	{
	if (orientation > 0)
	    fprintf(f, "+");
	else if (orientation < 0)
	    fprintf(f, "-");
	else
	    fprintf(f, "?");
        fprintf(f, "(");
	}
    fprintf(f, "%s", ce->clone->name);
    if (!ce->isStart)
        fprintf(f, ")");
    }
fprintf(f, "\n");
}

boolean bargeAdd(struct barge *barge, struct mmClone *newbie, struct mmClone *member,
	struct hash *ocpHash, boolean ignoreEnds, int *retOrientation, boolean *retAddAfter)
/* Attempt to add a clone 'newbie' to barge via connection with 'member'. */
{
struct dlNode *memStart, *memEnd;	/* Start/end nodes of member. */
struct overlappingClonePair *ocp;
struct fitAt *leftFits, *rightFits, *fitList, *fit;

logIt("bargeAdd: newbie %s, member %s\n", newbie->name, member->name);
dumpCloneEnds(barge->cloneEndList, logFile);
ocp = findHashedOcp(newbie->name, member->name, ocpHash);
findCloneEndsOnList(barge->cloneEndList, member, &memStart, &memEnd);

/* Find places where could fit and sort by score. */
rightFits = bargeFitListRight(barge, newbie, member, memStart, memEnd, ocp, ocpHash, ignoreEnds);
leftFits = bargeFitListLeft(barge, newbie, member, memStart, memEnd, ocp, ocpHash, ignoreEnds);
fitList = slCat(leftFits, rightFits);
leftFits = rightFits = NULL;
slSort(&fitList, fitAtCmpScore);

/* Add new clone at best fit if any. */
if ((fit = fitList) == NULL)
    {
    logIt(" %s doesn't fit in barge with %s\n", newbie->name, member->name);
    return FALSE;
    }
addFittedClone(barge, newbie, fit);
dumpCloneEnds(barge->cloneEndList, logFile);

*retOrientation = fit->orientation;
*retAddAfter = fit->addAfter;
slFreeList(&fitList);
return TRUE;
}

struct barge *bargeOfTwo(struct overlappingClonePair *ocp, struct dlList *bargeList,
	struct hash *ocpHash, struct overlappingClonePair *ocpList, boolean ignoreEnds)
/* Make up a barge with just two elements. */
{
struct mmClone *cloneA = ocp->a, *cloneB = ocp->b;
struct barge *barge;
int orientation;
boolean addAfter;

logIt("Barge of two: %s %s %d %d\n", cloneA->name, cloneB->name, ocp->overlap, ocp->score);

barge = bargeOfOne(cloneA, bargeList);
if (!bargeAdd(barge, cloneB, cloneA, ocpHash, ignoreEnds, &orientation, &addAfter))
    {
    if (ocp->overlap > 0)
	errAbort("Can't add second barge in bargeOfTwo(%s %s)", cloneA->name, cloneB->name);
    }
return barge;
}

void addToBarge(struct barge *barge, struct overlappingClonePair *ocp,
	struct mmClone *member, struct mmClone *newbie, 
	struct hash *ocpHash, struct overlappingClonePair *ocpList, boolean ignoreEnds)
/* Add clone to existing barge. */
{
int orientation;
boolean addAfter;

logIt("Adding clone %s to barge with %s\n", newbie->name, member->name);
bargeAdd(barge, newbie, member, ocpHash, ignoreEnds, &orientation, &addAfter);
}

struct dlList *dlClone(struct dlList *orig)
/* Make a copy of orig list and return it. */
{
struct dlList *cc = newDlList();
struct dlNode *node;
for (node = orig->head; !dlEnd(node); node = node->next)
    dlAddValTail(cc, node->val);
return cc;
}

/* When merging barges we do it tentatively, first saving
 * enough state to restore data structures if merger doesn't
 * work out, and then merging.  */

boolean tentativelyMergeBarges(struct barge *big, struct barge *little, 
    struct overlappingClonePair *joiningOcp, struct hash *ocpHash, boolean ignoreEnds)
/* Merge little barge into big barge.  If there's a problem
 * return false.  The big barge may have the little barge
 * partly merged in at this stage.  The function "mergeBarge"
 * below will clean things up if this happens. */
{
struct mmClone *member, *newbie, *initialNewbie;
int littleOrientation, bigOrientation;
int relBargeOrientation = 0;
struct dlNode *iNewStart, *iNewEnd;
struct dlNode *memStart, *memEnd;
struct dlNode *n1;
struct cloneEnd *ce;
boolean leftOk, rightOk, addAfter;
struct overlappingClonePair *ocp;

/* Figure out which clone is already part of big barge. */
if (joiningOcp->a->barge == big)
    {
    member = joiningOcp->a;
    initialNewbie = joiningOcp->b;
    }
else
    {
    assert(joiningOcp->b->barge == big);
    member = joiningOcp->b;
    initialNewbie = joiningOcp->a;
    }

logIt("Tentatively merging barges\n");
logIt("big: ");
dumpCloneEnds(big->cloneEndList, logFile);
logIt("little: ");
dumpCloneEnds(little->cloneEndList, logFile);


findCloneEndsOnList(little->cloneEndList, initialNewbie, &iNewStart, &iNewEnd);
for (n1 = iNewStart; !dlEnd(n1); n1 = n1->next)
    {
    ce = n1->val;
    if (ce->isStart)
        {
	newbie = ce->clone;
	leftOk = rightOk = FALSE;
	littleOrientation = ce->orientation;
	findCloneEndsOnList(big->cloneEndList, member, &memStart, &memEnd);
	ocp = findHashedOcp(member->name, newbie->name, ocpHash);
	if (ocp == NULL)
	    errAbort("Buh, neighbors don't overlap in merge barge1 ?");
	if (relBargeOrientation == 0)
	    {
	    if (!bargeAdd(big, newbie, member, ocpHash, ignoreEnds, &bigOrientation, &addAfter))
		{
		if (newbie == initialNewbie)
		    logIt("Couldn't tentatively merge initial\n");
		else
		    logIt("Couldn't tentatively second unoriented\n");
	        return FALSE;
		}
	    if (relBargeOrientation == 0)
	        {
		relBargeOrientation = littleOrientation * bigOrientation;
		}
	    if (newbie != initialNewbie && relBargeOrientation == 0)
	        {
		relBargeOrientation = (addAfter ? 1 : -1);
		}
	    }
	else if (relBargeOrientation > 0)
	    {
	    if (!bargeAddRight(big, newbie, member, memStart, memEnd, ocp, 
	    	ocpHash, TRUE, ignoreEnds, &bigOrientation))
		{
		logIt("Couldn't tentatively merge right\n");
		return FALSE;
		}
	    }
	else if (relBargeOrientation < 0)
	    {
	    if (!bargeAddLeft(big, newbie, member, memStart, 
	    	memEnd, ocp, ocpHash, TRUE, ignoreEnds, &bigOrientation))
		{
		logIt("Couldn't tentatively merge left\n");
		return FALSE;
		}
	    }
	if (bigOrientation != 0 && littleOrientation != 0 && relBargeOrientation != 0)
	    {
	    if (relBargeOrientation != bigOrientation*littleOrientation)
		{
		if (!ignoreEnds)
		    warn("Inconsistent orientations in tentatively merge at %s %s",
		    	newbie->name, member->name);
		else
		    {
		    logIt("Couldn't tentatively merge left from inconsistent orientations in 2nd pass\n");
		    return FALSE;
		    }
		}
	    }
	dumpCloneEnds(big->cloneEndList, logFile);
	member = newbie;
	}
    }
member = initialNewbie;
for (n1 = iNewStart->prev; !dlStart(n1); n1 = n1->prev)
    {
    ce = n1->val;
    if (ce->isStart)
        {
	newbie = ce->clone;
	littleOrientation = ce->orientation;
	findCloneEndsOnList(big->cloneEndList, member, &memStart, &memEnd);
	ocp = findHashedOcp(member->name, newbie->name, ocpHash);
	if (ocp == NULL)
	    errAbort("Buh, neighbors don't overlap in merge barge2 ?");
	if (relBargeOrientation == 0)
	    {
	    if (!bargeAdd(big, newbie, member, ocpHash, ignoreEnds, &bigOrientation, &addAfter))
		{
		logIt("Couldn't tentatively second unoriented left\n");
	        return FALSE;
		}
	    if (relBargeOrientation == 0)
	        {
		relBargeOrientation = littleOrientation * bigOrientation;
		}
	    if (relBargeOrientation == 0)
	        {
		relBargeOrientation = (addAfter ? -1 : 1);
		}
	    }
	else if (relBargeOrientation < 0)
	    {
	    if (!bargeAddRight(big, newbie, member, memStart, memEnd, ocp, 
	    	ocpHash, TRUE, ignoreEnds, &bigOrientation))
	        {
		logIt("Couldn't tentatively merge right 2\n");
		return FALSE;
		}
	    }
	else if (relBargeOrientation > 0)
	    {
	    if (!bargeAddLeft(big, newbie, member, memStart, memEnd, ocp, 
	    	ocpHash, TRUE, ignoreEnds, &bigOrientation))
	        {
		logIt("Couldn't tentatively merge left 2\n");
		return FALSE;
		}
	    }
	if (bigOrientation != 0 && littleOrientation != 0 && relBargeOrientation != 0)
	    {
	    if (relBargeOrientation != bigOrientation*littleOrientation)
		{
		if (!ignoreEnds)
		    warn("Inconsistent orientations in tentatively merge 2 at %s %s",
		    	newbie->name, member->name);
		else
		    {
		    logIt("Couldn't tentatively merge right from inconsistent orientations in 2nd pass\n");
		    return FALSE;
		    }
		}
	    }
	dumpCloneEnds(big->cloneEndList, logFile);
	member = newbie;
	}
    }
return TRUE;
}

struct barge *backupBarge(struct barge *orig)
/* Make a lightweight copy of barge - just enough to restore
 * barge if need be. */
{
struct barge *cc;
struct cloneEnd *ce;
struct dlNode *node;

AllocVar(cc);
cc->cloneList = dlClone(orig->cloneList);
cc->cloneEndList = dlClone(orig->cloneEndList);
for (node = cc->cloneEndList->head; !dlEnd(node); node = node->next)
    {
    ce = node->val;
    node->val = CloneVar(ce);
    }
return cc;
}

void reassertBargeCloneOwnership(struct barge *barge)
/* Make sure all clones part of barge point to barge. */
{
struct dlNode *node;
struct mmClone *clone;
for (node = barge->cloneList->head; !dlEnd(node); node = node->next)
    {
    clone = node->val;
    clone->barge = barge;
    }
}

void restoreBarge(struct barge *barge, struct barge *backup)
/* Restore barge from backup. */
{
freeDlListAndVals(&barge->cloneEndList);
freeDlList(&barge->cloneList);
barge->cloneEndList = backup->cloneEndList;
backup->cloneEndList = NULL;
barge->cloneList = backup->cloneList;
backup->cloneList = NULL;
}


boolean mergeBarges(struct barge *bargeA, struct barge *bargeB, 
	struct overlappingClonePair *ocp, struct hash *ocpHash,
	struct dlList *bargeList, boolean ignoreEnds)
/* Attempt to merge together two barges.  If can't restore things
 * so that original barges unchanged. */
{
struct barge *big, *little, *backup;
int aCount = dlCount(bargeA->cloneList), bCount = dlCount(bargeB->cloneList);
boolean ok;

logIt("Merging barges based on overlap between %s and %s\n", ocp->a->name, ocp->b->name);
logIt("BargeA has %d clones, bargeB has %d clones\n", aCount, bCount);
if (aCount >= bCount)
    {
    big = bargeA;
    little = bargeB;
    }
else
    {
    big = bargeB;
    little = bargeA;
    }
backup = backupBarge(big);
ok = tentativelyMergeBarges(big, little, ocp, ocpHash, ignoreEnds);
if (!ok)
    {
    restoreBarge(big, backup);
    reassertBargeCloneOwnership(little);
    }
else
    {
    dlRemove(little->dlNode);
    freez(&little->dlNode);
    freeDlList(&little->cloneEndList);
    freeDlList(&little->cloneList);
    freez(&little);
    }
freeDlListAndVals(&backup->cloneEndList);
freeDlList(&backup->cloneList);
freez(&backup);
return ok;
}

int scoreEnclosingOverlap(struct overlappingClonePair *ocp, struct mmClone *inner,
	boolean ignoreEnds)
/* Score whether it looks like outer clone encloses inner clone.
 * Return number between -2 (no way!) to 1 (yep) */
{
int innerSize = inner->size;

if (ocp == NULL)
    return -2;
if (ocp->overlap < innerSize * 0.75)
    return -1;
if (!ignoreEnds)
    {
    if (ocp->a == inner)
	{
	if (ocp->aHitS == 'Y' && ocp->aHitT == 'Y' && (ocp->bHitS != 'Y' || ocp->bHitT != 'Y'))
	    return 1;
	if (ocp->aHitS == 'N' || ocp->aHitT == 'N')
	    return -1;
	}
    else 
	{
	if (ocp->bHitS == 'Y' && ocp->bHitT == 'Y' && (ocp->aHitS != 'Y' || ocp->aHitT != 'Y'))
	    return 1;
	if (ocp->bHitS == 'N' || ocp->bHitT == 'N')
	    return -1;
	}
    }
if (ocp->overlap < innerSize * 0.85)
    return 0;
if (!ignoreEnds)
    {
    if (ocp->a == inner)
	{
	if (ocp->aHitS == 'Y' && ocp->aHitT == 'Y')
	    return 1;
	if (ocp->bHitS == 'N' && ocp->bHitT == 'N')
	    return 1;
	}
    else 
	{
	if (ocp->bHitS == 'Y' && ocp->bHitT == 'Y')
	    return 1;
	if (ocp->aHitS == 'N' && ocp->aHitT == 'N')
	    return 1;
	}
    }
if (ocp->sloppyOverlap > innerSize * 0.98)
    return 1;
if (ocp->overlap < innerSize * 0.95)
    return 0;
return 1;
}

int scoreNonenclosingOverlap(struct overlappingClonePair *ocp, struct mmClone *a, struct mmClone *b)
/* Score whether it looks like two clones overlap, but neither is enclosed. 
 * Returns 1 (yes), -1 (no) or 0 (maybe). */
{
if (ocp == NULL)
    return -1;
logIt("    scoreNonenclosing: ");
ocpDump(ocp, logFile);
if (ocpGreatEnds(ocp->aHitS, ocp->aHitT))
    return 1;
if (ocpGreatEnds(ocp->bHitS, ocp->bHitT))
    return 1;
if (ocp->overlap < a->size * 0.75 && ocp->overlap < b->size * 0.75)
    return 1;
if ((ocp->aHitS == 'Y' && ocp->aHitT == 'Y') || (ocp->bHitS == 'Y' && ocp->bHitT == 'Y'))
    return -1;
if (ocp->overlap < a->size * 0.85 && ocp->overlap < b->size * 0.85)
    return 1;
if ((ocp->aHitS == 'N' && ocp->aHitT == 'N') || (ocp->bHitS == 'N' && ocp->bHitT == 'N'))
    return -1;
if (ocp->overlap < a->size * 0.90 && ocp->overlap < b->size * 0.90)
    return 1;
if (ocp->overlap < a->size * 0.97 && ocp->overlap < b->size * 0.97)
    return 0;
return -1;
}

void markBuriedClones(struct overlappingClonePair *ocpList, boolean ignoreEnds)
/* Set buried field in clones. */
{
struct overlappingClonePair *ocp;
int dist;

for (ocp = ocpList; ocp != NULL; ocp = ocp->next)
    {
    dist = maxMapDif(ocp->a, ocp->b);
    if (dist <= 2*maxMapDeviation)
	{
	if (ocp->a->size > ocp->b->size)
	    {
	    if (scoreEnclosingOverlap(ocp, ocp->b, ignoreEnds) > 0)
		{
		if (ocp->b->barge == NULL)
		    {
		    ocp->b->enclosed = ocp->a;
		    logIt("%s covers %s\n", ocp->a->name, ocp->b->name);
		    }
		}
	    }
	else
	    {
	    if (scoreEnclosingOverlap(ocp, ocp->a, ignoreEnds) > 0)
		{
		if (ocp->a->barge == NULL)
		    {
		    ocp->a->enclosed = ocp->b;
		    logIt("%s covers %s\n", ocp->b->name, ocp->a->name);
		    }
		}
	    }
	}
    }
logIt("\n");
}


boolean orientPair(struct overlappingClonePair *ocp, struct mmClone *clone,
	int relOrientation, boolean ignoreEnds, int *pCloneOrientation)
/* See if orientation of clone either is, or if unknown can be made
 * consistent with position implied by one clone coming after the other 
 * and ocp. Just checks clone ends, not other clone ends
 * if any in ocp. */
{
int oldOrientation = *pCloneOrientation;
int ocpOrientation = ocpCloneOrientation(ocp, clone, ignoreEnds);

if (ocpOrientation == orientImpossible)
    return FALSE;
if (ocpDoubleMiss(ocp->aHitS, ocp->aHitT))
    return FALSE;
if (ocpDoubleMiss(ocp->bHitS, ocp->bHitT))
    return FALSE;
if (ocpOrientation != orientUnknown)
    {
    ocpOrientation *= relOrientation;
    if (oldOrientation != orientUnknown && oldOrientation != ocpOrientation)
        return FALSE;   /* Contradicts earlier orientation. */
    *pCloneOrientation = ocpOrientation;
    }
return TRUE;
}

struct fitAt *fitEnclosed(struct dlNode *listStart, struct dlNode *listEnd, 
	struct mmClone *clone, struct dlNode *outerStart, struct dlNode *outerEnd,
	struct hash *ocpHash, boolean ignoreEnds)
/* Figure out if can place clone between outerStart and outerEnd. */
{
struct dlNode *innerStart = outerStart->next, *innerEnd = outerEnd->prev;
struct dlNode *testStart, *testEnd;
struct cloneEnd *ce;
struct mmClone *testClone;
boolean pastCloneStart = FALSE, pastCloneEnd = FALSE;
int fitScore = 0, oneScore;
struct overlappingClonePair *ocp;
struct fitAt *fit;
int newbieOrientation = 0;

    {
    struct cloneEnd *ce1 = outerStart->val, *ce2 = outerEnd->val;
    char name1[64], name2[64];
    if (ce1 == NULL)
          sprintf(name1, "BEGIN");
    else
          sprintf(name1, "%s(%s)", ce1->clone->name, 
	      (ce1->isStart ? "start" : "end"));
    if (ce2 == NULL)
          sprintf(name2, "END");
    else
          sprintf(name2, "%s(%s)", ce2->clone->name, 
	      (ce2->isStart ? "start" : "end"));
    logIt("   fitEnclosed( %s between %s and %s)\n", clone->name, name1, name2);
    }

if (!insideEndsFit(clone, innerStart, outerEnd, ocpHash, ignoreEnds))
    {
    logIt("      insideEnds don't fit\n");
    return NULL;
    }

for (testStart = listStart; testStart != listEnd; testStart = testStart->next)
    {
    if (testStart == innerStart) pastCloneStart = TRUE;
    if (testStart == outerEnd) pastCloneEnd = TRUE;
    ce = testStart->val;
    if (ce->isStart)
        {
	struct dlNode *gotInStart, *gotInEnd, *gotOutStart,*gotOutEnd;
	gotInStart = gotInEnd = gotOutStart = gotOutEnd = NULL;
	testClone = ce->clone;
	for (testEnd = testStart;  ;testEnd = testEnd->next)
	    {
	    if (testEnd == innerStart) gotInStart = testEnd;
	    if (testEnd == outerStart) gotOutStart = testEnd;
	    if (testEnd == innerEnd) gotInEnd = testEnd;
	    if (testEnd == outerEnd) gotOutEnd = testEnd;
	    ce = testEnd->val;
	    if (ce->clone == testClone && !ce->isStart)
		break;
	    }
	ocp = findHashedOcp(testClone->name, clone->name, ocpHash);
	if (gotOutStart != NULL && gotOutEnd!= NULL )
	     {
	     /* Case where test encloses clone. */
	     oneScore = scoreEnclosingOverlap(ocp, clone, ignoreEnds);
	     fitScore += oneScore;
	     logIt("       %s encloses %s score %d\n", testClone->name, clone->name, oneScore);
	     }
	else if (gotOutStart == NULL && gotOutEnd == NULL)
	     {
	     if (pastCloneStart && !pastCloneEnd && innerStart != outerEnd)
		 {
		 /* Case where clone encloses test. */
		 oneScore = scoreEnclosingOverlap(ocp, testClone, ignoreEnds);
		 fitScore += oneScore;
		 logIt("       %s enclosed by %s score %d\n", testClone->name, clone->name, oneScore);
		 }
	     else
	         {
		 /* Case where test is before clone, no overlap. */
		 }
	     }
	else if (gotOutStart != NULL && gotOutStart != testEnd)
	     {
	     /* Case where order is testClone, clone and the two overlap. */
	     oneScore = scoreNonenclosingOverlap(ocp, clone, testClone);
	     logIt("     ***oneScore %d\n", oneScore);
	     if (!orientPair(ocp, clone, 1, ignoreEnds, &newbieOrientation))
	         return NULL;
	     fitScore += oneScore;
	     logIt("       %s before %s score %d\n", testClone->name, clone->name, oneScore);
	     }
	else if (gotOutEnd != NULL && gotOutEnd != testStart)
	     {
	     /* Case where order is clone, testClone and the two overlap. */
	     oneScore = scoreNonenclosingOverlap(ocp, testClone, clone);
	     logIt("     ***oneScore %d\n", oneScore);
	     fitScore += oneScore;
	     if (!orientPair(ocp, clone, -1, ignoreEnds, &newbieOrientation))
	         return NULL;
	     logIt("       %s after %s score %d\n", testClone->name, clone->name, oneScore);
	     }
	}
    }
AllocVar(fit);
fit->start = outerStart;
fit->end = innerEnd;
fit->orientation = newbieOrientation;
fit->addAfter = TRUE;
fit->score = fitScore;
return fit;
}

void placeBuriedClones(struct mmClone *cloneList, struct dlList *bargeList, struct hash *ocpHash,
  boolean ignoreEnds)
/* Place buried clones in proper position in barges. */
{
struct mmClone *clone, *outerClone;
struct barge *barge;
struct dlNode *outCloneStart, *outCloneEnd;
struct dlNode *node, *n1, *n2;
struct dlList *enclosedList = newDlList();

/* Make list of enclosed clones and sort them by size. */
for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    if (clone->enclosed != NULL)
	dlAddValTail(enclosedList, clone);
    }
dlSort(enclosedList, mmCloneCmpSize);

/* Place enclosed clones, biggest first. */
for (node = enclosedList->head; !dlEnd(node); node = node->next)
    {
    struct fitAt *fitList = NULL, *fit;
    clone = node->val;
    if (clone->barge != NULL)
        continue;	/* Must have been placed previously. */
    outerClone = clone->enclosed;
    while (outerClone->enclosed != NULL)
         outerClone = outerClone->enclosed;
    if ((barge = outerClone->barge) == NULL)
	barge = bargeOfOne(outerClone, bargeList);
    logIt("Trying to place enclosed %s\n", clone->name);
    dumpCloneEnds(barge->cloneEndList, logFile);
    findCloneEndsOnList(barge->cloneEndList, outerClone, &outCloneStart, &outCloneEnd);

    /* Search one past enclosing clone, in case not really quite enclosed. */
    if (!dlStart(outCloneStart->prev))
        outCloneStart = outCloneStart->prev;
    if (!dlEnd(outCloneEnd->next))
        outCloneEnd = outCloneEnd->next;

    for (n1 = outCloneStart; n1 != outCloneEnd; n1 = n1->next)
	{
	for (n2 = n1->next; n2 != outCloneEnd->next; n2 = n2->next)
	    {
	    if ((fit = fitEnclosed(barge->cloneEndList->head, 
		    outCloneEnd, clone, n1, n2, ocpHash, ignoreEnds)) != NULL)
		{
		dumpFit(fit, clone);
		slAddHead(&fitList, fit);
		}
	    }
	}
    slSort(&fitList, fitAtCmpScore);
    if ((fit = fitList) != NULL)
	{
	addFittedClone(barge, clone, fit);
	dumpCloneEnds(barge->cloneEndList, logFile);
	}
    else
	warn("Couldn't place enclosed clone %s", clone->name);
    slFreeList(&fitList);
    logIt("\n");
    }
freeDlList(&enclosedList);
}

void makeSingletonBarges(struct mmClone *cloneList, struct dlList *bargeList)
/* Make singleton barges around clones not barged up yet. */
{
struct mmClone *clone;
for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    if (clone->barge == NULL)
        {
	bargeOfOne(clone, bargeList);
	}
    }
}

/* Place buried clones in proper position in barges. */

struct dlList *mmBargeList(struct mmClone *cloneList, struct hash *cloneHash,
	struct hash *ocpHash, struct overlappingClonePair **pOcpList)
/* Make contigs of overlapping clones (aka barges) using overlap info. 
 * ocpList should be sorted by score. */
{
struct overlappingClonePair *ocp, *ocpList;
struct dlList *bargeList = newDlList();
struct barge *barge, *bargeA, *bargeB;
int ignoreEnds;

for (ignoreEnds = 0; ignoreEnds <= 1; ++ignoreEnds)
    {
    markBuriedClones(*pOcpList, ignoreEnds);
    status("Ordering non-enclosed clones %s end info\n", 
        (ignoreEnds ? "ignoring" : "using") );
    ocpScoreAndSort(pOcpList, ignoreEnds);
    ocpList = *pOcpList;
    for (ocp = ocpList; ocp != NULL; ocp = ocp->next)
	ocpDump(ocp, logFile);
    logIt("\n");

    logIt("Scanning overlaps in order of score\n");
    for (ocp = ocpList; ocp != NULL; ocp = ocp->next)
	{
	if (ocp->score < 0)
	    break;
	ocpDump(ocp, logFile);
	if (ocp->a->enclosed)
	    {
	    logIt("skipping enclosed: %s inside %s\n\n",
		ocp->a->name, ocp->a->enclosed->name);
	    continue;
	    }
	if (ocp->b->enclosed)
	    {
	    logIt("skipping enclosed: %s inside %s\n\n",
		ocp->b->name, ocp->b->enclosed->name);
	    continue;
	    }
    #ifdef SOMETIMES
	if (startsWith("AL365225", ocp->a->name) && startsWith("AL391058", ocp->b->name))
	    {
	    uglyerfStart();
	    uglyerf("Got a live one!\n");
	    }
	else
	    {
	    uglyerfEnd();
	    }
    #endif
	bargeA = ocp->a->barge;
	bargeB = ocp->b->barge;
	if (bargeA == NULL && bargeB == NULL)
	    {
	    barge = bargeOfTwo(ocp, bargeList, ocpHash, ocpList, ignoreEnds);
	    }
	else if (bargeA == NULL)
	    {
	    addToBarge(bargeB, ocp, ocp->b, ocp->a, ocpHash, ocpList, ignoreEnds);
	    }
	else if (bargeB == NULL)
	    {
	    addToBarge(bargeA, ocp, ocp->a, ocp->b, ocpHash, ocpList, ignoreEnds);
	    }
	else if (bargeA == bargeB)
	    {
	    /* This is handled elsewhere. */
	    logIt("Clones already in same barge.\n");
	    }
	else if (bargeA != bargeB)
	    {
	    mergeBarges(bargeA, bargeB, ocp, ocpHash, bargeList, ignoreEnds);
	    }
	logIt("\n");
	}
    }
for (ignoreEnds = 0; ignoreEnds <= 1; ++ignoreEnds)
    placeBuriedClones(cloneList, bargeList, ocpHash, ignoreEnds);
makeSingletonBarges(cloneList, bargeList);
return bargeList;
}

void flipBarge(struct barge *barge)
/* Flip orientation of barge. */
{
struct dlList *flipList = newDlList();
struct dlNode *node, *next;
struct cloneEnd *ce;

/* Reverse end elements. */
for (node = barge->cloneEndList->head; !dlEnd(node); node = next)
    {
    next = node->next;
    dlRemove(node);
    dlAddHead(flipList, node);
    ce = node->val;
    ce->isStart = !ce->isStart;
    ce->orientation = -ce->orientation;
    }
freeDlList(&barge->cloneEndList);
barge->cloneEndList = flipList;
}

boolean needFlipBarge(struct barge *barge)
/* See if barge looks to need flipping according to map. */
{
int diff = 0, oneDiff;
struct dlNode *node;
struct cloneEnd *ce;
struct mmClone *clone;
boolean isFirst = TRUE;
boolean startPos, lastStartPos = 0;

for (node = barge->cloneEndList->head; !dlEnd(node); node = node->next)
    {
    ce = node->val;
    if (ce->isStart && !ce->clone->enclosed)
        {
	clone = ce->clone;
	startPos = clone->mapPos;
	diff += clone->flipTendency * ce->orientation;
	if (isFirst)
	    isFirst = FALSE;
	else
	    {
	    oneDiff = startPos - lastStartPos;
	    if (oneDiff > clone->size)
	        oneDiff = clone->size;
	    if (oneDiff < -clone->size)
	        oneDiff = -clone->size;
	    diff += oneDiff;
	    }
	lastStartPos = startPos;
	}
    }
return diff < 0;
}

int averageMapPos(struct barge *barge)
/* Return average map position of clones in barge. */
{
double total = 0;
int count = 0;
struct mmClone *clone;
struct dlNode *node;

for (node = barge->cloneList->head; !dlEnd(node); node = node->next)
    {
    clone = node->val;
    count += 1;
    total += clone->mapPos;
    }
return round(total/count);
}


struct mmClone *findNextUnenclosed(struct dlNode *node)
/* Find next clone in double-linked clone end list that is
 * not enclosed. */
{
struct cloneEnd *ce;

for (; !dlEnd(node); node = node->next)
    {
    ce = node->val;
    if (ce->isStart && ce->clone->enclosed == NULL)
	 return ce->clone;
    }
return NULL;
}

int printOverlapInfo(FILE *f, struct mmClone *a, struct mmClone *b, struct hash *ocpHash,
	int orientation)
/* Print size of overlap between a and b, and a end info on ab overlap. */
{
struct overlappingClonePair *ocp;
char as, at, temp;

if (a == NULL || b == NULL)
    {
    fprintf(f, "%6s  - -   ", "n/a");
    return 0;
    }
ocp = findHashedOcp(a->name, b->name, ocpHash);
if (ocp->a == a)
    {
    as = ocp->aHitS;
    at = ocp->aHitT;
    }
else
    {
    as = ocp->bHitS;
    at = ocp->bHitT;
    }
if (orientation < 0)
    {
    temp = as;
    as = at;
    at = temp;
    }
if (orientation == 0 && (as != '?' || at != '?'))
    fprintf(f, "%6d (%c %c)  ", ocp->overlap, as, at);
else
    fprintf(f, "%6d  %c %c   ", ocp->overlap, as, at);
return ocp->overlap;
}

struct cloneRef *getLastClones(struct barge *barge, int minSize, struct hash *ocpHash)
/* Get all clones within minSize of end of barge. */
{
struct dlNode *node;
struct mmClone *clone, *lastClone = NULL;
struct cloneEnd *ce;
struct cloneRef *list = NULL;
int curSize = 0;

for (node = barge->cloneEndList->tail; !dlStart(node); node = node->prev)
    {
    ce = node->val;
    if (!ce->isStart)
        {
	clone = ce->clone;
	cloneRefAdd(&list, clone);
	curSize += clone->size;
	if (lastClone != NULL)
	    curSize -= overlapSize(clone, lastClone, ocpHash, FALSE);
	if (curSize >= minSize)
	    break;
	lastClone = clone;
	}
    }
return list;
}

struct cloneRef *getFirstClones(struct barge *barge, int minSize, struct hash *ocpHash)
/* Get all clones within minSize of start of barge. */
{
struct dlNode *node;
struct mmClone *clone, *lastClone = NULL;
struct cloneEnd *ce;
struct cloneRef *list = NULL;
int curSize = 0;

for (node = barge->cloneEndList->head; !dlEnd(node); node = node->next)
    {
    ce = node->val;
    if (ce->isStart)
        {
	clone = ce->clone;
	cloneRefAdd(&list, clone);
	curSize += clone->size;
	if (lastClone != NULL)
	    curSize -= overlapSize(clone, lastClone, ocpHash, FALSE);
	if (curSize >= minSize)
	    break;
	lastClone = clone;
	}
    }
return list;
}

struct slRef *refIntersection(struct slRef *aList, struct slRef *bList)
/* Make a refList that is intersection of aList and bList. */
{
struct hash *aHash;
char name[32];
struct slRef *ref, *newList = NULL;

/* Handle case where either list empty quickly. */
if (aList == NULL || bList == NULL)
    return NULL;

/* Put all of aList into hash. */
aHash = newHash(0);
for (ref = aList; ref != NULL; ref = ref->next)
    {
    sprintf(name, "%p", ref->val);
    hashAdd(aHash, name, NULL);
    }

/* Scan through bList, and add appropriate elements to list. */
for (ref = bList; ref != NULL; ref = ref->next)
    {
    sprintf(name, "%p", ref->val);
    if (hashLookup(aHash, name))
        {
	refAdd(&newList, ref->val);
	}
    }
freeHash(&aHash);
return newList;
}

struct bepRef *bepsFromClones(struct cloneRef *cloneRefList)
/* Make a bepRef list consolidating all beps from all clones in input list. */
{
struct bepRef *bepRefList = NULL, *bepRef;
struct cloneRef *cloneRef;
struct mmClone *clone;

for (cloneRef = cloneRefList; cloneRef != NULL; cloneRef = cloneRef->next)
    {
    clone = cloneRef->clone;
    for (bepRef = clone->bepList; bepRef != NULL; bepRef = bepRef->next)
        bepRefAddUnique(&bepRefList, bepRef->bep);
    }
return bepRefList;
}

boolean evalGapInfo(FILE *f, struct barge *aBarge, struct barge *bBarge, 
	struct hash *ocpHash, struct hash *bepHash)
/* Evaluate and optionally print  out info about gap between aBarge and bBarge. 
 * Returns TRUE if gap is bridged.  If f is NULL it will not print,
 * just evaluate gap. */
{
struct cloneRef *aEnd = NULL, *bStart = NULL;
struct bepRef *aBep = NULL, *bBep = NULL, *joiningBep = NULL, *bepRef;
struct bep *bep;
int bigEnough = 200000;
boolean bridged = FALSE;

aEnd = getLastClones(aBarge, bigEnough, ocpHash);
bStart = getFirstClones(bBarge, bigEnough, ocpHash);
aBep = bepsFromClones(aEnd);
bBep = bepsFromClones(bStart);
joiningBep = (struct bepRef*)refIntersection((struct slRef*)aBep, (struct slRef*)bBep);

if (joiningBep == NULL)
    {
    if (f != NULL)
	fprintf(f, "----- open gap -----\n");
    }
else
    {
    if (f != NULL)
	{
	fprintf(f, "----- bridged gap:");
	for (bepRef = joiningBep; bepRef != NULL; bepRef = bepRef->next)
	    {
	    bep = bepRef->bep;
	    fprintf(f, " %s", bep->cloneName);
	    }
	fprintf(f, "\n");
	}
    bridged = TRUE;
    }

/* Clean up time. */
slFreeList(&aBep);
slFreeList(&bBep);
slFreeList(&joiningBep);
slFreeList(&aEnd);
slFreeList(&bStart);
return bridged;
}

char cloneDir(struct cloneEnd *ce)
/* Return ? + or - for direction clone goes. */
{
int o = ce->orientation;
if (o == 0)
   return '?';
else if (o < 0)
   return '-';
else
   return '+';
}

void saveBargeFile(char *fileName, char *contigName, struct dlList *bargeList, struct hash *ocpHash,
	struct hash *cloneHash, struct hash *bepHash)
/* Write out barges to file. */
{
FILE *f = mustOpen(fileName, "w");
struct dlNode *bNode, *eNode;
struct barge *barge;
struct cloneEnd *ce;
int cloneCount;
boolean lastBridged = FALSE;

fprintf(f, "Barge (Connected Clone) File mm Version %d on %s\n\n", version, contigName);
fprintf(f, "start    name   phase clone     dir size       before       after      center plc   mapPos\n");
fprintf(f, "------------------------------------------------------------------------------------------\n");

for (bNode = bargeList->head; !dlEnd(bNode); bNode = bNode->next)
    {
    struct mmClone *clone, *outer, *lastUnenclosed = NULL, *nextUnenclosed;
    struct overlappingClonePair *ocp;

    barge = bNode->val;
    cloneCount = dlCount(barge->cloneList);
    for (eNode = barge->cloneEndList->head; !dlEnd(eNode); eNode = eNode->next)
        {
	char os, ot, is, it;
	int nextOverlap;

	ce = eNode->val;
	if (ce->isStart)
	    {
	    clone = ce->clone;
	    if ((outer = clone->enclosed) != NULL)
		{
		ocp = findHashedOcp(clone->name, outer->name, ocpHash);
		if (ocp->a == outer)
		    {
		    os = ocp->aHitS;
		    ot = ocp->aHitT;
		    is = ocp->bHitS;
		    it = ocp->bHitT;
		    }
		else
		    {
		    os = ocp->bHitS;
		    ot = ocp->bHitT;
		    is = ocp->aHitS;
		    it = ocp->aHitT;
		    }
	        fprintf(f, "     (%d %-9s %d %-11s %c %6d   %c %c  %9s %6d   %c %c %6s %3s %8d)\n",
		    clone->mmPos, clone->name, clone->phase, naForNull(clone->bacName), 
		    cloneDir(ce), clone->size, os, ot, 
		    outer->name, ocp->sloppyOverlap,
		    is, it, naForNull(clone->seqCenter), 
		    naForNull(clone->placeInfo), clone->mapPos);
		}
	    else
		{
		fprintf(f, "%-8d %-9s %d %-11s %c %6d  ", 
			clone->mmPos, clone->name, clone->phase,
			naForNull(clone->bacName), cloneDir(ce), clone->size);
		nextUnenclosed = findNextUnenclosed(eNode->next);
		printOverlapInfo(f, clone, lastUnenclosed, ocpHash, ce->orientation);
		nextOverlap = printOverlapInfo(f, clone, nextUnenclosed, ocpHash, ce->orientation);
		fprintf(f, "%6s %3s %8d", naForNull(clone->seqCenter), naForNull(clone->placeInfo), 
			clone->mapPos);
		if (cloneCount == 1)
		    {
		    boolean nextBridged = FALSE;
		    if (!dlEnd(bNode->next))
		        nextBridged = evalGapInfo(NULL, bNode->val, bNode->next->val,
				ocpHash, bepHash);
		    if (lastBridged || nextBridged)
		        fprintf(f, " bridged");
		    else
		        fprintf(f, " unbridged");
		    fprintf(f, " singleton");
		    if (clone->bestFriend != NULL)
			fprintf(f, " %s %d", clone->bestFriend->name, clone->bestOverlap);
		    else
		        fprintf(f, " none");
		    }
		lastUnenclosed = clone;
		fprintf(f, "\n");
		if (nextOverlap == 0 && nextUnenclosed != NULL)
		    {
		    fprintf(f, "----- weak gap\n");
		    }
		}
	    }
	}
    if (!dlEnd(bNode->next))
	{
	lastBridged = evalGapInfo(f, bNode->val, bNode->next->val, ocpHash, bepHash);
	}
    }
fclose(f);
}

int bargeCmpMapPos(const void *va, const void *vb)
/* Compare to sort biggest sized first. */
{
const struct barge *a = *((struct barge **)va);
const struct barge *b = *((struct barge **)vb);
return a->mapPos - b->mapPos;
}

void setUnenclosedPos(struct dlList *bargeList, struct hash *ocpHash, struct hash *bepHash)
/* Set mmPos field of all clones that are not enclosed. */
{
struct dlNode *bNode, *eNode;
int pos = 0;
struct barge *barge;
struct cloneEnd *ce;

for (bNode = bargeList->head; !dlEnd(bNode); bNode = bNode->next)
    {
    struct mmClone *clone, *lastUnenclosed = NULL;

    barge = bNode->val;
    for (eNode = barge->cloneEndList->head; !dlEnd(eNode); eNode = eNode->next)
        {
	ce = eNode->val;
	clone = ce->clone;
	if (ce->isStart)
	    {
	    if (clone->enclosed == NULL)
		{
		if (lastUnenclosed != NULL)
		    pos -= overlapSize(clone, lastUnenclosed, ocpHash, FALSE);
		clone->mmPos = pos;
		ce->pos = pos;
		pos += clone->size;
		lastUnenclosed = clone;
		}
	    }
	else
	    {
	    ce->pos = clone->mmPos + clone->size;
	    }
	}

    if (!dlEnd(bNode->next))
	{
	boolean bridged;
	bridged = evalGapInfo(NULL, bNode->val, bNode->next->val, ocpHash, bepHash);
	if (bridged)
	    pos += 50000;
	else
	    pos += 100000;
	}
    }
}

struct dlNode *findNextUnenclosedEnd(struct dlNode *eNode, int *retCount)
/* Find next end that is part of a non-enclosed clone.  */
{
struct cloneEnd *ce;
int count = 0;
for (; !dlEnd(eNode); eNode = eNode->next)
    {
    ce = eNode->val;
    if (ce->clone->enclosed == NULL)
        break;
    ++count;
    }
*retCount = count;
return eNode;
}

struct cloneEnd *findEndOfClone(struct dlNode *eNode, struct mmClone *clone)
/* Find end of given clone in doubly linked end list starting search at eNode. */
{
struct cloneEnd *ce;
for (; !dlEnd(eNode); eNode = eNode->next)
    {
    ce = eNode->val;
    if (ce->clone == clone && !ce->isStart)
        return ce;
    }
errAbort("Internal error. Couldn't find end of %s", clone->name);
return NULL;
}

void setEnclosedPos(struct dlList *bargeList, struct hash *ocpHash, struct hash *bepHash)
/* Set mmPos field of all enclosed clones. */
{
struct dlNode *bNode, *eNode, *nextEnode = NULL;
int lastPos = 0;
struct barge *barge;
struct cloneEnd *ce, *nextKnownCe, *endCe;
int lastKnownPos, nextKnownPos, knownSpan;
int encCount;
int i;

for (bNode = bargeList->head; !dlEnd(bNode); bNode = bNode->next)
    {
    struct mmClone *clone;
    barge = bNode->val;

    /* First pass - fill in value for ends of enclosed ends by interpolating
     * between other ends. */
    for (eNode = barge->cloneEndList->head; !dlEnd(eNode); eNode = nextEnode)
        {
	ce = eNode->val;
	clone = ce->clone;
	if (clone->enclosed != NULL)
	    {
	    warn("Starting with enclosed clone %s, faking it...", clone->name);
	    clone->mmPos = lastPos;
	    }
	else
	    {
	    lastPos = clone->mmPos;
	    nextEnode = findNextUnenclosedEnd(eNode->next, &encCount);
	    if (nextEnode != eNode->next)
	        {
		lastKnownPos = ce->pos;
		if (dlEnd(nextEnode))
		    {
		    warn("Ending with enclosed clone buh!...");
		    nextKnownPos = lastKnownPos + clone->size;
		    }
		else
		    {
		    nextKnownCe = nextEnode->val;
		    nextKnownPos = nextKnownCe->pos;
		    }
		knownSpan = nextKnownPos - lastKnownPos;
		for (i=1; i<=encCount; ++i)
		    {
		    eNode = eNode->next;
		    ce = eNode->val;
		    ce->pos = lastKnownPos + roundingScale(knownSpan, i, encCount+1);
		    }
		}
	    }
	}

    /* Second pass - set clone position by making midpoint of clone average of two ends. */
    for (eNode = barge->cloneEndList->head; !dlEnd(eNode); eNode = eNode->next)
        {
	ce = eNode->val;
	clone = ce->clone;
	if (clone->enclosed != NULL && ce->isStart)
	    {
	    endCe = findEndOfClone(eNode->next, clone);
	    if (ce->pos == 0 || endCe->pos == 0)
		warn("Zero position of enclosed clone end %s", clone->name);
	    clone->mmPos = (ce->pos + endCe->pos)/2 - clone->size/2;
	    }
	}
    }
}

void orderAndOrientBarges(struct dlList *bargeList, struct hash *ocpHash, struct hash *bepHash)
/* Orient barges according to map and set barge->mapPos field
 * according to first clone. */
{
struct dlNode *bNode;
struct barge *barge;

for (bNode = bargeList->head; !dlEnd(bNode); bNode = bNode->next)
    {
    barge = bNode->val;
    if (needFlipBarge(barge))
	flipBarge(barge);
    barge->mapPos = averageMapPos(barge);
    }
dlSort(bargeList, bargeCmpMapPos);
setUnenclosedPos(bargeList, ocpHash, bepHash);
setEnclosedPos(bargeList, ocpHash, bepHash);
}

void saveInfo(char *fileName, char *contigName, struct dlList *bargeList)
/* Save info file. */
{
FILE *f = mustOpen(fileName, "w");
struct dlNode *bNode, *eNode;
struct cloneEnd *ce;
struct mmClone *clone;
struct barge *barge;

fprintf(f, "%s PLACED\n", contigName);
for (bNode = bargeList->head; !dlEnd(bNode); bNode = bNode->next)
    {
    barge = bNode->val;
    for (eNode = barge->cloneEndList->head; !dlEnd(eNode); eNode = eNode->next)
        {
	ce = eNode->val;
	if (ce->isStart)
	    {
	    clone = ce->clone;
	    fprintf(f, "%s %d %d %d\n", clone->name, clone->mmPos/1000, clone->phase, clone->flipTendency);
	    }
	}
    }
fclose(f);
}

void writeEndsFile(char *fileName, struct dlList *bargeList)
/* Write out end order info. */
{
struct dlNode *node;
FILE *f = mustOpen(fileName, "w");
struct barge *barge;

for (node = bargeList->head; !dlEnd(node); node = node->next)
    {
    barge = node->val;
    dumpCloneEnds(barge->cloneEndList, f);
    }
carefulClose(&f);
}

int cloneEndCmpPos(const void *va, const void *vb)
/* Compare to sort biggest sized first. */
{
const struct cloneEnd *a = *((struct cloneEnd **)va);
const struct cloneEnd *b = *((struct cloneEnd **)vb);
return a->pos - b->pos;
}



void saveLiteralEnds(char *fileName, struct mmClone *cloneList)
/* Save ends pretending they just follow map order. 
 * Clone list needs to be sorted by mapPos*/
{
struct cloneEnd *ce;
struct barge *barge = NULL;
struct dlList *bargeList = newDlList();
int lastEnd = -BIGNUM;
struct mmClone *clone;
struct dlNode *node;

/* Build up barges literally from map positions. */
for (clone = cloneList; clone != NULL; clone = clone->next)
     {
     if (clone->mapPos > lastEnd)
         {
	 AllocVar(barge);
	 barge->cloneEndList = newDlList();
	 barge->cloneList = newDlList();
	 barge->dlNode = dlAddValTail(bargeList, barge);
	 }
     dlAddValTail(barge->cloneList, clone);
     ce = newCloneEnd(clone, NULL);
     ce->isStart = TRUE;
     ce->pos = clone->mapPos;
     dlAddValTail(barge->cloneEndList, ce);
     ce = newCloneEnd(clone, NULL);
     ce->pos = clone->mapPos + clone->size;
     dlAddValTail(barge->cloneEndList, ce);
     if (ce->pos > lastEnd)
         lastEnd = ce->pos;
     }

/* Some ends may be out of order, so sort them. */
for (node = bargeList->head; !dlEnd(node); node = node->next)
    {
    barge = node->val;
    dlSort(barge->cloneEndList, cloneEndCmpPos);
    }

/* Write it out. */
writeEndsFile(fileName, bargeList);

/* Clean up time. */
for (node = bargeList->head; !dlEnd(node); node = node->next)
    {
    barge = node->val;
    freeDlListAndVals(&barge->cloneEndList);
    freeDlList(&barge->cloneList);
    }
freeDlList(&bargeList);
}

boolean enclosedByFinished(struct mmClone *inner, struct overlappingClonePair *ocpList)
/* Return TRUE if inner clone is completely enclosed by a finished clone. */
{
struct overlappingClonePair *ocp;
for (ocp = ocpList; ocp != NULL; ocp = ocp->next)
    {
    if (ocp->a == inner && ocp->b->phase == htgFinished 
    	&& (scoreEnclosingOverlap(ocp, inner, TRUE) >= 1 
		|| scoreEnclosingOverlap(ocp, inner, FALSE) >= 1)
	&& ocp->bHitS == 'N' && ocp->bHitT == 'N')
	return TRUE;
    if (ocp->b == inner && ocp->a->phase == htgFinished
    	&& (scoreEnclosingOverlap(ocp, inner, TRUE) >= 1 
		|| scoreEnclosingOverlap(ocp, inner, FALSE) >= 1)
	&& ocp->aHitS == 'N' && ocp->aHitT == 'N')
	return TRUE;
    }
return FALSE;
}

void removeCloneFromOcp(struct mmClone *clone, struct overlappingClonePair **pOcpList,
	struct hash *ocpHash)
/* Remove all overlaps involving clone. */
{
struct overlappingClonePair *ocpList = NULL, *ocp, *nextOcp;
for (ocp = *pOcpList; ocp != NULL; ocp = nextOcp)
    {
    nextOcp = ocp->next;
    if (ocp->a == clone || ocp->b == clone)
        {
	char *handle = ocpHashName(ocp->a->name, ocp->b->name);
	hashRemove(ocpHash, handle);
	}
    else
        {
	slAddHead(&ocpList, ocp);
	}
    }
slReverse(&ocpList);
*pOcpList = ocpList;
}

void removeDraftInFinished(struct mmClone **pCloneList, struct hash *cloneHash, 
	struct overlappingClonePair **pOcpList, struct hash *ocpHash)
/* Remove draft clones that are completely enclosed inside finished
 * clones/finished contigs. */
{
struct mmClone *cloneList = NULL, *clone, *nextClone;
int rmCount = 0;
for (clone = *pCloneList; clone != NULL; clone = nextClone)
    {
    nextClone = clone->next;
    if (clone->phase != htgFinished && enclosedByFinished(clone, *pOcpList))
	{
	removeCloneFromOcp(clone, pOcpList, ocpHash);
	hashRemove(cloneHash, clone->name);
	++rmCount;
	}
    else
        {
	slAddHead(&cloneList, clone);
	}
    }
slReverse(&cloneList);
*pCloneList = cloneList;
}

void mm(char *contigDir)
/* mm - Map mender. */
{
char fileName[512];
struct mmClone *cloneList = NULL, *clone;
struct hash *cloneHash = NULL;
struct overlappingClonePair *ocpList = NULL;
struct hash *ocpHash = NULL;
struct dlList *bargeList = NULL;
struct hash *bepHash = NULL;
struct bep *bepList = NULL;

/* Set up logging of error and status messages. */
sprintf(fileName, "%s/mmLog.%d", contigDir, version);
logFile = mustOpen(fileName, "w");
pushWarnHandler(warnHandler);

sprintf(fileName, "%s/cloneInfo", contigDir);
mmLoadClones(fileName, &cloneList, &cloneHash);
status("Loaded %d clones from %s\n", slCount(cloneList), fileName);

sprintf(fileName, "%s/info", contigDir);
mmAddInfo(fileName, cloneList, cloneHash);
if (sameWord(contigType, "PLACED"))
    {
    maxMapDeviation = 50000;
    isPlaced = TRUE;
    }
status("Added map info from %s\n", fileName);

sprintf(fileName, "%s/cloneEnds", contigDir);
mmAddEndInfo(fileName, cloneList, cloneHash);
status("Added map end info from %s\n", fileName);

if (sameString(contigName, "ctg_na"))
    {
    status("Special processing %s", contigName);
    for (clone = cloneList; clone != NULL; clone = clone->next)
        {
	clone->mapPos = 0;
	}
    }
else
    {
    sprintf(fileName, "%s/map", contigDir);
    mmAddMap(fileName, cloneList, cloneHash);
    status("Added map info from %s\n", fileName);
    }

sprintf(fileName, "%s/cloneOverlap", contigDir);
mmLoadOcp(fileName, cloneHash, &ocpList, &ocpHash);
status("Loaded %d overlaps from %s\n", slCount(ocpList), fileName);

sprintf(fileName, "%s/bacEndPairs", contigDir);
mmLoadBacEndPairs(fileName, &bepList, &bepHash);
status("Loaded %d bac end pairs from %s\n", slCount(bepList), fileName);

sprintf(fileName, "%s/bacEnd.psl", contigDir);
mmLoadBacEndPsl(fileName, bepHash, cloneHash);

removeDraftInFinished(&cloneList, cloneHash, &ocpList, ocpHash);
bargeList = mmBargeList(cloneList, cloneHash, ocpHash, &ocpList);
status("Merged overlaps\n");

orderAndOrientBarges(bargeList, ocpHash, bepHash);
status("ordered barges\n");

sprintf(fileName, "%s/mmEnds.%d", contigDir, version);
status("Writing barges to %s\n", fileName);
writeEndsFile(fileName, bargeList);
sprintf(fileName, "%s/mmEnds", contigDir);
writeEndsFile(fileName, bargeList);

sprintf(fileName, "%s/mmBarge.%d", contigDir, version);
saveBargeFile(fileName, contigName, bargeList, ocpHash, cloneHash, bepHash);
sprintf(fileName, "%s/mmBarge", contigDir);
saveBargeFile(fileName, contigName, bargeList, ocpHash, cloneHash, bepHash);

sprintf(fileName, "%s/info.mm", contigDir);
saveInfo(fileName, contigName, bargeList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
mm(argv[1]);

#ifdef SOMETIMES
mm("/projects/cc/hg/oo.24/6/ctg15907");
#endif

return 0;
}
