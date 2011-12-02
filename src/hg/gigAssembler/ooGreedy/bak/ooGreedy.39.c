/* ooGreedy - A greedy method for doing ordering and orienting
 * of contigs. */

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


int version = 39;       /* Current version number. */
int maxTailSize = 2000;	/* Maximum non-aligning end. */
int minScore = -500;    /* Don't bother gluing past this. */
int minBacScore = -520; /* Bac ends can be a little worse... */

int bridgedBargeGapSize = 50000;	/* # of N's between connected barges. */
int unbridgedBargeGapSize = 100000;     /* # of N's between disconnected barges. */
int fragGapSize = 100;                  /* # of N's between frags within a barge. */

FILE *logFile;	/* File to write decision steps to. */
FILE *raftPsl;  /* File to write consistent .psl to. */
char ocpFileName[512];  /* Where to write overlapping clone pairs. */

void usage()
/* Explain usage and exit. */
{
errAbort(
   "ooGreedy - ordering and orienting of unfinished contigs using\n"
   "a relatively simple, but hopefully effective greedy method\n"
   "usage:\n"
   "      ooGreedy directory\n"
   "The program expects the files self.psl, est.psl, mrna.psl, bacEnds.psl\n"
   "bacEndsPairs to live in the directory.  It will create the files\n"
   "gl.N raft.N graph.N raft.psl ooGreedy.N.gl ooGreedy.log barge.N gold.N \n"
   "where N is the version number of ooGreedy (currently %d).\n"
   "These files describe:\n"
   "  gl.N - Where each fragment is in 'golden path' coordinates\n"
   "         along with connectivity info on fragments.\n"
   "  raft.N - Fragments connected by direct sequence overlap\n"
   "  graph.N - connections of rafts by mRNA and BAC end data\n"
   "  raft.psl - Alignments from self.psl involved in building rafts\n"
   "  ooGreedy.N.gl - same as gl.N but lacking connectivity info\n"
   "  ooGreedy.log - record of decisions program made.\n"
   "  barge.N - where clones in the contig belong.\n"
   "  gold.N - what parts of what fragment to use to build final\n"
   "           'golden path' merged sequence for contig."
   , version);
}

enum htgPhase
    {
    htgSurvey = 0,
    htgDraft = 1,
    htgOrdered = 2,
    htgFinished = 3,
    };

struct oogClone
/* Describes a clone (BAC) */
    {
    struct oogClone *next;     /* Next in clone list. */
    char *name;                /* Name of clone. */
    int mapPos;                /* Relative position in contig according to map. */
    int defaultPos;            /* Relative position in barge or contig in bases. */
    int size;                  /* Size in bases. */
    struct barge *barge;       /* Barge this is part of. */
    struct bargeEl *bel;       /* Point back to barge el to get position... */
    struct dgNode *startNode;  /* Start node in raft graph. */
    struct dgNode *endNode;    /* End node in raft graph. */
    enum htgPhase phase;       /* HTG phase. */
    bool hitFlag;              /* Used by raftsShareClones. */
    };

struct oogFrag
/* A fragment of a clone. */
    {
    struct oogFrag *next;	/* Next in frag list. */
    char *name;			/* Name of frag. */
    int size;                   /* Size in bases. */
    struct oogClone *clone;     /* Clone it's part of. */
    struct dnaSeq *seq;         /* Sequence. */
    struct raft *raft;		/* Raft fragment is in. */
    int clonePos;               /* Position within clone. (Assuming frags back to back.) */
    int defaultPos;             /* Default position in contig. */
    short defaultOrientation;   /* Default orientation. */
    };

struct oogEdge
/* An edge connecting two fragments in a raft. */
    {
    struct oogEdge *next;      /* Next in master edge list. */
    boolean isHard;            /* True if hard - nothing can come between. */
    int match;		       /* Number of non-repeating bases that match. */
    int repMatch;              /* Number of repeating bases that match. */
    int milliBad;              /* Parts per thousand of mismatch/insert. */
    int tailA, tailB;          /* End tails. */
    int score;                 /* Overall score. Big is good. */
    struct oogFrag *aSide;     /* One frag. */
    struct oogFrag *bSide;     /* Other frag. */
    struct psl *psl;          /* Alignment for of join. */
    short orientation;        /* Relative orientation of of sides. */
    bool aCovers;              /* True if a covers b completely. */
    };

struct edgeRef 
/* A reference to an oogEdge. */
    {
    struct edgeRef *next;
    struct oogEdge *edge;
    };

struct raft
/* A raft - a set of welded together fragments. */
    {
    struct raft *next;          /* Next in raft list. */
    int id;			/* Id of this raft. */
    struct raftFrag *fragList;   /* List of frags part of this raft. */
    int start, end;              /* Start and end in raft coordinates. */
    struct dgNode *node;         /* Node in mrna graph. */
    struct barge *barge;         /* Barge raft is part of. */
    struct connectedComponent *cc;   /* Other rafts connected by mrna data. */
    short orientation;           /* Relative orientation of graph.  1, -1 or 0 for unknown. */
    int defaultPos;		 /* Default Middle position of raft in contig coordinates. */
    struct edgeRef *refList;     /* Edges used to make this raft. */
    struct raft *parent;         /* Raft this got merged into. */
    struct raft *children;       /* Rafts this one has absorbed. */
    };

struct raftFrag
/* An oog frag with a little extra info - puts a fragment on a raft. */
    {
    struct raftFrag *next;      /* Next in list. */
    int raftOffset;             /* Offset from frag to raft coordinates. */
    short orientation;		/* Orientation of frag in raft. */
    struct oogFrag *frag;   /* Frag itself. */
    };

struct overlappingClonePair
/* Info on a pair of clones which overlaps. */
    {
    struct overlappingClonePair *next;  /* Next in list. */
    struct oogClone *a, *b;             /* Pair of clones. */
    int overlap;                        /* Size of overlap. */
    };

struct barge
/* A barge - a set of welded together clones. */
    {
    struct barge *next;	    /* Next in list. */
    int id;                 /* Barge id. */
    struct bargeEl *cloneList;  /* List of clones. */
    struct dgNode *node;        /* Node in barge graph. */
    int offset;                /* Offset within contig. */
    int mapPos;                /* Position within map. */
    int size;                  /* Total size. */
    };

struct bargeEl
/* A clone that's part of a barge. */
    {
    struct bargeEl *next;   /* Next in list. */
    struct oogClone *clone; /* Clone. */
    struct dlNode *dlNode;  /* List node ref. */
    int offset;             /* Offset within barge. */
    struct bargeEl *enclosed;  /* Clone enclosed by another?. */
    };

struct mrnaConnection
/* An mRNA induced link between two rafts. */
    {
    struct mrnaConnection *next; /* Next in list. */
    struct raft *raft;		/* Raft linked. */
    short orientation;          /* Relative orientation of raft to mRNA. */
    int score;                  /* Score of this connection. */
    // int milliBad;               /* Bad percentage of alignment. */
    bool bestForRaft;           /* Set to TRUE if best for raft. */
    bool raftTested;            /* Set to TRUE if raft tested for best. */
    };

struct mrnaLine
/* A linear graph with a score describing connections between
 * rafts induced by a single mRNA. */
    {
    struct mrnaLine *next;            /* Next in list. */
    char *name;                       /* Name of mRNA that induces this line. */
    struct mrnaConnection *mcList;    /* Raft connections. */
    int score;                        /* Overall score. */
    };

struct mrnaConnectionRef
/* Reference to an mrnaConnection. */
    {
    struct mrnaConnectionRef *next;     /* next in list. */
    struct mrnaConnection *mc;	        /* mrnaConnection */
    };

struct connectedComponent
/* A list of connected components. This is a temporary structure
 * that just lives while an mrnaLine is being added to an mrna graph. */
    {
    struct connectedComponent *next;	/* Next connected component. */
    struct dgNodeRef *subGraph;         /* List of graph nodes in component. */
    struct mrnaConnectionRef *subLine;  /* List of mrna nodes in component. */
    short orientation;                  /* +1 or -1 */
    };

struct edgeExtension 
/* Info on an edge that contains an extension. */
    {
    struct edgeExtension *next;  /* Next in list. */
    struct oogEdge  *edge;    /* edge reference */
    struct oogFrag *otherFrag; /* Other fragment. */
    int extSize;              /* Number of bases extended. */
    int tailSize;             /* Number of bases in tail. */
    int seqQual;              /* Quality of sequence in fragment. */
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

/* Make uglyf debugging statements go to log file too. */
#undef uglyf
#define uglyf status

int cmpEdgeExtension(const void *va, const void *vb)
/* Compare two edgeExtension to sort by seqQual. */
{
const struct edgeExtension *a = *((struct edgeExtension **)va);
const struct edgeExtension *b = *((struct edgeExtension **)vb);
int diff;
diff = b->seqQual - a->seqQual;
if (diff == 0)
    diff = b->edge->score - a->edge->score;
return diff;
}

int cmpFragDefaultPos(const void *va, const void *vb)
/* Compare two oogFrags to sort by default position . */
{
const struct oogFrag *a = *((struct oogFrag **)va);
const struct oogFrag *b = *((struct oogFrag **)vb);
return a->defaultPos - b->defaultPos;
}

int nextRaftId()
/* Return next available id for raft */
{
static int id = 0;
return ++id;
}

int raftFragCmpOffset(const void *va, const void *vb)
/* Compare two raftFrags to sort by offset . */
{
const struct raftFrag *a = *((struct raftFrag **)va);
const struct raftFrag *b = *((struct raftFrag **)vb);
return a->raftOffset - b->raftOffset;
}

int fragSeqQual(struct oogFrag *frag)
/* Return a number that corresponds to sequence quality
 * as estimated by size and phase. */
{
int sizeFactor = frag->size;
if (sizeFactor > 200000) sizeFactor = 200000;
return frag->clone->phase*50000 + sizeFactor;
}

int raftFragCmpSeqQuality(const void *va, const void *vb)
/* Compare two raftFrags to sort by sequence quality . */
{
const struct raftFrag *a = *((struct raftFrag **)va);
const struct raftFrag *b = *((struct raftFrag **)vb);
return fragSeqQual(b->frag) - fragSeqQual(a->frag);
}

char orientChar(int orient)
/* Return character representing orientation. */
{
if (orient > 0) return '+';
else if (orient < 0) return '-';
else return '.';
}

void dumpRaft(struct raft *raft, FILE *out)
/* Print out info about raft. */
{
struct raftFrag *rf;

fprintf(out, "%d raft %d bases %d frags %d default pos\n", 
	raft->id, raft->end - raft->start, slCount(raft->fragList),
	raft->defaultPos);
for (rf = raft->fragList; rf != NULL; rf = rf->next)
    {

    fprintf(out, "%d %c %s %d\n", rf->raftOffset, 
	orientChar(rf->orientation),
	rf->frag->name, rf->frag->size);
    }
}

boolean fragInRaft(struct raft *raftList, 
	struct oogFrag *frag, struct raft **retRaft, struct raftFrag **retRaftFrag)
/* Return TRUE if fragment is already in a raft.  Return which raft it's 
 * and which raft frag if applicable. */
{
struct raft *raft;
struct raftFrag *rf;

for (raft = raftList; raft != NULL; raft = raft->next)
    {
    for (rf = raft->fragList; rf != NULL; rf = rf->next)
	{
	if (rf->frag == frag)
	    {
	    *retRaft = raft;
	    *retRaftFrag = rf;
	    return TRUE;
	    }
	}
    }
*retRaft = NULL;
*retRaftFrag = NULL;
return FALSE;
}

int oogEdgeCmp(const void *va, const void *vb)
/* Compare two oogEdges for least bad first. */
{
const struct oogEdge *a = *((struct oogEdge **)va);
const struct oogEdge *b = *((struct oogEdge **)vb);
return b->score - a->score;
}

void dumpEdge(struct oogEdge *edge, FILE *out)
/* Print out an edge. */
{
fprintf(out, "edge: %11s %c> %11s %s bad %3d tailA %3d tailB %3d match %5d repMatch %5d score %4d\n", edge->aSide->name,
	((edge->orientation > 0) ? '-' : '~'),
	edge->bSide->name, (edge->aCovers ? "covers " : "extends" ),
	edge->milliBad, edge->tailA, edge->tailB, edge->match, edge->repMatch, edge->score);
}


void dumpMrnaLine(struct mrnaLine *ml, FILE *f)
/* Print out mrnaLine. */
{
struct mrnaConnection *mc;
fprintf(f, "mrnaLine: %s score %d connections %d\n", 
	ml->name, ml->score, slCount(ml->mcList));
for (mc = ml->mcList; mc != NULL; mc = mc->next)
    {
    fprintf(f, "  raft %d %c score %d\n", mc->raft->id, 
	(mc->orientation > 0 ? '+' : '-'),
	mc->score);
    }
}

int mrnaLineCmp(const void *va, const void *vb)
/* Compare to sort based on score - best score first. */
{
const struct mrnaLine *a = *((struct mrnaLine **)va);
const struct mrnaLine *b = *((struct mrnaLine **)vb);
return b->score - a->score;
}


int markBestConnections(struct mrnaConnection *list)
/* Set the best flags for connections that are best for a given raft. 
 * Returns very best score. */
{
struct mrnaConnection *startEl, *el;
int veryBestScore = -0x3fffffff;

startEl = list;
for (;;)
    {
    int bestScore = -0x3fffffff;
    struct raft *curRaft;
    struct mrnaConnection *bestEl;

    while (startEl != NULL && startEl->raftTested)
	startEl = startEl->next;
    if (startEl == NULL)
	break;
    curRaft = startEl->raft;
    for (el = startEl; el != NULL; el = el->next)
	{
	if (el->raft == curRaft)
	    {
	    if (bestScore < el->score)
		{
		bestScore = el->score;
		bestEl = el;
		if (veryBestScore < bestScore)
		    veryBestScore = bestScore;
		}
	    el->raftTested = TRUE;
	    }
	}
    bestEl->bestForRaft = TRUE;
    }
return veryBestScore;
}


struct mrnaConnection *cleanMrnaConnectionList(struct mrnaConnection *oldList)
/* Clean up old list, removing crufty connections.  */
{
struct mrnaConnection *mc, *next;
struct mrnaConnection *newList = NULL;
int veryBestScore;
int threshold;
int score;

veryBestScore = markBestConnections(oldList);
if (veryBestScore < -100)
    return NULL;
threshold = veryBestScore - 500;

for (mc = oldList; mc != NULL; mc = next)
    {
    next = mc->next;
    if (mc->bestForRaft && mc->score >= threshold)
	{
	slAddHead(&newList, mc);
	}
    }
slReverse(&newList);
return newList;
}

struct raftFrag *findRaftFrag(struct raft *raft, struct oogFrag *frag)
/* Find frag in raft or die trying. */
{
struct raftFrag *rf;
for (rf = raft->fragList; rf != NULL; rf = rf->next)
    {
    if (rf->frag == frag)
	return rf;
    }
errAbort("Couldn't find %s in raft", frag->name);
}

int scoreMrnaPsl(struct psl *psl)
/* Return score for one mRNA oriented psl. */
{
int milliBad;
int score;

milliBad = pslCalcMilliBad(psl, TRUE);
score = 25*log(1+psl->match) + log(1+psl->repMatch) - 10*milliBad;
if (psl->match <= 10)
    score -= (10-psl->match)*25;
return score;
}

int pslOrientation(struct psl *psl)
/* Translate psl strand + or - to orientation +1 or -1 */
{
if (psl->strand[0] == '-')
    return -1;
else
    return 1;
}


void findPslTails(struct psl *psl, int *retStartTail, int *retEndTail)
/* Find the length of "tails" (rather than extensions) implied by psl. */
{
int orientation = pslOrientation(psl);
int qFloppyStart, qFloppyEnd;
int tFloppyStart, tFloppyEnd;

if (orientation > 0)
    {
    qFloppyStart = psl->qStart;
    qFloppyEnd = psl->qSize - psl->qEnd;
    }
else
    {
    qFloppyStart = psl->qSize - psl->qEnd;
    qFloppyEnd = psl->qStart;
    }
tFloppyStart = psl->tStart;
tFloppyEnd = psl->tSize - psl->tEnd;
*retStartTail = min(qFloppyStart, tFloppyStart);
*retEndTail = min(qFloppyEnd, tFloppyEnd);
}

#ifdef OLD
int scoreBacEndPsl(struct psl *psl)
/* Return score for one BAC end oriented psl. */
{
int milliBad;
int score;
int startTail, endTail;

milliBad = pslCalcMilliBad(psl, FALSE);
findPslTails(psl, &startTail, &endTail);
score = 25*log(1+psl->match) + log(1+psl->repMatch) - 20*milliBad - 3*(startTail+endTail);
if (psl->match <= 10)
    score -= (10-psl->match)*25;
return score;
}
#endif

int scoreBacEndPsl(struct psl *psl)
/* Return score for one BAC end oriented psl. */
{
int milliBad;
int score;
int startTail, endTail;

milliBad = pslCalcMilliBad(psl, FALSE);
findPslTails(psl, &startTail, &endTail);
score = 25*log(1+psl->match) + log(1+psl->repMatch) - 20*milliBad - 3*(startTail+endTail);
if (psl->match <= 20)
    score -= (20-psl->match)*25;
score -= 50; /* No respect for BAC ends... */
return score;
}


struct raft *raftOfOne(struct oogFrag *frag)
/* Return a new raft containing just one fragment. */
{
struct raft *raft;
struct raftFrag *rf;

AllocVar(rf);
rf->orientation = 1;
rf->raftOffset = 0;
rf->frag = frag;

AllocVar(raft);
raft->id = nextRaftId();
raft->start = 0;
raft->end = frag->size;
slAddTail(&raft->fragList, rf);

frag->raft = raft;
return raft;
}

struct mrnaLine *oneRnaLine(struct psl *pslList, struct hash *fragHash)
/* Return an ordered list of rafts connected by this mRNA. */
{
struct mrnaLine *line;
struct mrnaConnection *mc, *mcList = NULL;
struct psl *psl;
struct hashEl *hel;
struct oogFrag *frag;
struct raft *raft;
struct raftFrag *rf;
int oneScore;
int milliScore;
double conflictTotal;
int pslOrient;
int minScore = 0x3fffffff;
int totalScore = 0;
int averageScore;
int cleanSize = 0;
int compositeScore;
char *mrnaName = pslList->qName;

fprintf(logFile, "oneRnaLine (preCleaned):\n");
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    pslOrient = pslOrientation(psl);
    AllocVar(mc);
    hel = hashLookup(fragHash, psl->tName);
    if (hel == NULL)
	{
	errAbort("Conflict between self.psl and mrna.psl or est.psl, "
	         "not run on same data.  self.psl lacks %s", psl->tName);
	}
    frag = hel->val;
    raft = frag->raft;
    rf = findRaftFrag(raft, frag);
    mc->raft = raft;
    mc->orientation = pslOrient * rf->orientation;
    mc->score = scoreMrnaPsl(psl);
    fprintf(logFile, " %d %c %d\n", raft->id,
    	(mc->orientation > 0 ? '+' : '-'),
	mc->score);
    slAddHead(&mcList, mc);
    }
slReverse(&mcList);
fprintf(logFile, "%s got %d initial connections\n", mrnaName, slCount(mcList));
mcList = cleanMrnaConnectionList(mcList);
if (mcList == NULL)
    {
    fprintf(logFile, "Rejected %s\n", mrnaName);
    return NULL;
    }
if (mcList->next == NULL)
    {
    fprintf(logFile, "%s only hits one raft\n", mrnaName);
    return NULL;
    }
AllocVar(line);
line->name = cloneString(mrnaName);

/* Figure out composite score - average of average and min scores. */
for (mc = mcList; mc != NULL; mc = mc->next)
    {
    oneScore = mc->score;
    totalScore += oneScore;
    if (oneScore < minScore)
	minScore = oneScore;
    ++cleanSize;
    }
averageScore = (totalScore+cleanSize/2)/cleanSize;
compositeScore = (averageScore + minScore)/2;
fprintf(logFile, "   %d final connections score %d\n", 
    slCount(mcList), compositeScore);
line->score = compositeScore;
line->mcList = mcList;
return line;
}

struct mrnaPsl
/* psl's associated with one piece of mrna. */
    {
    struct mrnaPsl *next;	/* Next in list. */
    char *mrnaName;             /* Name of mrna. */
    struct psl *pslList;        /* List of .psls. */
    };


struct mrnaLine *makeMrnaLines(char *fileName, struct hash *fragHash)
/* Return list of lines based on an mRNA psl file. */
{
struct hash *rnaHash = newHash(0);
struct mrnaLine *lineList = NULL, *line;
struct psl *pslList = NULL, *psl;
struct lineFile *lf = pslFileOpen(fileName);
struct hashEl *hel;
struct mrnaPsl *mpList = NULL, *mp;

/* Bundle together psl's that refer to same mRNA. */
while ((psl = pslNext(lf)) != NULL)
    {
    if ((hel = hashLookup(rnaHash, psl->qName)) == NULL)
	{
	AllocVar(mp);
	hel = hashAdd(rnaHash, psl->qName, mp);
	mp->mrnaName = hel->name;
	slAddHead(&mpList, mp);
	}
    else
	mp = hel->val;
    slAddHead(&mp->pslList, psl);
    }
slReverse(&mpList);

for (mp = mpList; mp != NULL; mp = mp->next)
    {
    slSort(&mp->pslList, pslCmpQuery);
    line = oneRnaLine(mp->pslList, fragHash);
    if (line != NULL)
	{
	slAddHead(&lineList, line);
	}
    }
slReverse(&lineList);
slFreeList(&mpList);
freeHash(&rnaHash);
return lineList;
}

struct pair
/* Keep track of a pair. */
    {
    struct pair *next;  /* next in list */
    char *a;		/* First in pair. Allocated in hash. */
    char *b;            /* Second in pair. Allocated in hash. */
    struct psl *aPslList; /* List of all psl's referencing a.  Owned here. */
    struct psl *bPslList; /* List of all psl's referencing b.  Owned here. */
    };

void pairFree(struct pair **pPair)
/* Free a pair. */
{
struct pair *pair = *pPair;
pslFreeList(&pair->aPslList);
pslFreeList(&pair->bPslList);
freez(pPair);
}

void pairFreeList(struct pair **pList)
/* Free a list of pairs. */
{
struct pair *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    pairFree(&el);
    }
*pList = NULL;
}

void makePairHash(char *pairName, struct hash **retHash, struct pair **retList)
/* Make up a hash out of the pair file. */
{
struct hash *hash = newHash(20);
struct hashEl *helA, *helB;
struct pair *list = NULL, *pair;
struct lineFile *lf = lineFileOpen(pairName, TRUE);
int lineSize;
char *line;
int wordCount;
char *words[3];
int pairCount = 0;

while (lineFileNext(lf, &line, &lineSize))
    {
    if ((wordCount = chopLine(line, words)) != 2)
	errAbort("Expecting 2 words line %d of %s\n", lf->lineIx, lf->fileName);
    AllocVar(pair);
    helA = hashAdd(hash, words[0], pair);
    helB = hashAdd(hash, words[1], pair);
    pair->a = helA->name;
    pair->b = helB->name;
    slAddHead(&list, pair);
    ++pairCount;
    }
printf("Got %d pairs in %s\n", pairCount, pairName);
lineFileClose(&lf);
slReverse(&list);
*retHash = hash;
*retList = list;
}

struct psl *bestBacPsl(struct psl *pslList, int *retScore)
/* Return best scoring psl in list according to bac scoring function,
 * or NULL if none of them are good enough. */
{
struct psl *psl;
int bestScore = minBacScore;
int scoreOne;
struct psl *bestPsl = NULL;

for (psl = pslList; psl != NULL; psl = psl->next)
    {
    scoreOne = scoreBacEndPsl(psl);
    if (scoreOne > bestScore)
	{
	bestScore = scoreOne;
	bestPsl = psl;
	}
    }
*retScore = bestScore;
return bestPsl;
}

struct raft *raftFromBacPslList(struct psl *pslList, struct hash *fragHash, 
	int *retScore, struct psl **retPsl, struct raftFrag **retRf)
/* Lookup raft from best scoring BAC psl in list. */
{
struct psl *psl;
struct oogFrag *frag;
struct hashEl *hel;
struct raft *raft;

if ((psl = bestBacPsl(pslList, retScore)) == NULL)
    return NULL;
if ((hel = hashLookup(fragHash, psl->tName)) == NULL)
    {
    errAbort("%s in bacEnds.psl but not in geno.lst files.  Need to regenerate .psl files?\n",
	psl->tName);
    }
*retPsl = psl;
frag = hel->val;
raft = frag->raft;
*retRf = findRaftFrag(raft, frag);
return raft;
}

struct mrnaLine *mlFromPair(struct pair *pair, struct hash *fragHash)
/* Create an mrnaLine describing connection between rafts implied by pair. */
{
struct raft *aRaft, *bRaft;
struct psl *aPsl, *bPsl;
struct raftFrag *aRf, *bRf;
int aScore, bScore;
int aOrient, bOrient;
struct hashEl *hel;
struct mrnaLine *ml;
struct mrnaConnection *mc;
char buf[256];

if ((aRaft = raftFromBacPslList(pair->aPslList, fragHash, &aScore, &aPsl, &aRf)) == NULL)
    return NULL;
if ((bRaft = raftFromBacPslList(pair->bPslList, fragHash, &bScore, &bPsl, &bRf)) == NULL)
    return NULL;
if (aRaft == bRaft)
    return NULL;

AllocVar(ml);
sprintf(buf, "%s_%s", pair->a, pair->b);
ml->name = cloneString(buf);
ml->score = min(aScore, bScore);

AllocVar(mc);
mc->raft = aRaft;
mc->orientation = aOrient = pslOrientation(aPsl) * aRf->orientation;
mc->score = aScore;
slAddTail(&ml->mcList, mc);

AllocVar(mc);
mc->raft = bRaft;
mc->orientation = bOrient = -pslOrientation(bPsl) * bRf->orientation;
mc->score = bScore;
slAddTail(&ml->mcList, mc);

return ml;
}


struct mrnaLine *makeBacEndLines(char *bacPairName, char *bacPslName, struct hash *fragHash)
/* Read in pair file and psl file to create list of bac end connections
 * in mrnaLine format. */
{
struct mrnaLine *mlList = NULL, *ml;
struct hash *pairHash;
struct pair *pairList, *pair;
struct psl *psl;
struct lineFile *lf;
struct hashEl *hel;
char *bacName;
int totalPsl = 0;
int pairedPsl = 0;

makePairHash(bacPairName, &pairHash, &pairList);
lf = pslFileOpen(bacPslName);
while ((psl = pslNext(lf)) != NULL)
    {
    ++totalPsl;
    bacName = psl->qName;
    if ((hel = hashLookup(pairHash, bacName)) == NULL)
	{
	pslFree(&psl);
	continue;
	}
    ++pairedPsl;
    pair = hel->val;
    if (sameString(bacName, pair->a))
	{
	slAddHead(&pair->aPslList, psl);
	}
    else
	{
	slAddHead(&pair->bPslList, psl);
	}
    }
fprintf(stdout, "%d paired bacEnd psls.  %d total bacEnd psls\n", pairedPsl, totalPsl);
fprintf(logFile, "%d paired bacEnd psls.  %d total bacEnd psls\n", pairedPsl, totalPsl);

for (pair = pairList; pair != NULL; pair = pair->next)
    {
    ml = mlFromPair(pair, fragHash);
    if (ml != NULL)
	slAddHead(&mlList, ml);
    }
pairFreeList(&pairList);
freeHash(&pairHash);
slReverse(&mlList);
return mlList;
}

struct oogClone *cloneFromName(char *fragName, struct oogClone **pList, struct hash *hash)
/* Return clone from hashtable if it exists already, otherwise make up new clone
 * store it on list and in hash, and return it. */
{
struct hashEl *hel;
char name[128];

fragToCloneName(fragName, name);
if ((hel = hashLookup(hash, name)) == NULL)
    errAbort("%s in self.psl but not geno.lst", name);
return hel->val;
}

struct oogFrag *fragFromName(char *name, struct oogFrag **pList, struct hash *hash,
	struct oogClone *clone, int size)
/* Return frag from hash table if it exists, otherwise make up a new one,
 * put it on list and hash table, and return it. */
{
struct hashEl *hel;

if ((hel = hashLookup(hash, name)) == NULL)
    {
    errAbort("%s is in self.psl, but not in any of the clones in %s\n"
	     "You probably need to regenerate self.psl");
    }
return hel->val;
}

struct oogEdge *edgeFromSelfPsl(struct psl *psl,  struct oogFrag *qFrag, struct oogFrag *tFrag)
/* Return a self edge if this looks feasable, else NULL. */
{
bool qForward = (psl->strand[0] == '+');
int qFloppyStart, qFloppyEnd;
int tFloppyStart, tFloppyEnd;
struct oogEdge *edge;
int startTail, endTail;
int sizeFactor;

#ifdef VERBOSE_LOG
findPslTails(psl, &startTail, &endTail); 
fprintf(logFile,
   "edgeFromSelf:\n"
   "  (%s %d %d of %d) %s (%s %d %d of %d) \n"
   "  startTail %d endTail %d\n",
    psl->qName, psl->qStart, psl->qEnd, psl->qSize, psl->strand,
    psl->tName, psl->tStart, psl->tEnd, psl->tSize,
    startTail, endTail);
#endif /* VERBOSE_LOG */

if (qForward)
    {
    qFloppyStart = psl->qStart;
    qFloppyEnd = psl->qSize - psl->qEnd;
    tFloppyStart = psl->tStart;
    tFloppyEnd = psl->tSize - psl->tEnd;
    if ((startTail = min(qFloppyStart,tFloppyStart)) > maxTailSize)
	return NULL;
    if ((endTail = min(qFloppyEnd, tFloppyEnd)) > maxTailSize)
	return NULL;
    AllocVar(edge);
    edge->orientation = 1;
    if (qFloppyStart >= tFloppyStart)
	{
	/*   >qqqqqq>        or   >qqqqqqqqqq>
	        >tttttt>             >tttt>
	 */
	edge->aSide = qFrag;
	edge->bSide = tFrag;
	edge->aCovers = (qFloppyEnd >= tFloppyEnd);
	}
    else
	{
	/*   >tttttt>        or   >tttttttttt>
	        >qqqqqq>             >qqqq>
	 */
	edge->aSide = tFrag;
	edge->bSide = qFrag;
	edge->aCovers = (tFloppyEnd >= qFloppyEnd);
	}
    }
else
    {
    qFloppyStart = psl->qSize - psl->qEnd;
    qFloppyEnd = psl->qStart;
    tFloppyStart = psl->tStart;
    tFloppyEnd = psl->tSize - psl->tEnd;
    if ((startTail = min(qFloppyStart,tFloppyStart)) > maxTailSize)
	return NULL;
    if ((endTail = min(qFloppyEnd, tFloppyEnd)) > maxTailSize)
	return NULL;
    AllocVar(edge);
    edge->orientation = -1;
    if (qFloppyStart >= tFloppyStart)
	{
	/*    <qqqqqqq<      or   <qqqqqqqqqqqqq<
	         >ttttttt>           >ttttt>
	 */
	edge->aSide = qFrag;
	edge->bSide = tFrag;
	edge->aCovers = (qFloppyEnd >= tFloppyEnd);
	}
    else
	{
	/*    >ttttttt>      or   >ttttttttttttt>
	         <qqqqqqq<           <qqqqq<
	 */
	edge->aSide = tFrag;
	edge->bSide = qFrag;
	edge->aCovers = (tFloppyEnd >= qFloppyEnd);
	}
    }
edge->isHard = TRUE;
edge->match = psl->match;
edge->repMatch = psl->repMatch;
edge->milliBad = pslCalcMilliBad(psl, FALSE);
edge->tailA = startTail;
edge->tailB = endTail;
edge->score = -30*edge->milliBad - (startTail+endTail)/2 + 
	round(25*log(edge->match+1) + log(edge->repMatch+1)); 
/* Fold in penalties for not having any unique matches
 * or for being a small fragment. */
if (edge->match <= 20)
    edge->score -= (20-edge->match)*25;
if (psl->qSize < 5000)
    edge->score -= (5000-psl->qSize)/40;
if (psl->tSize < 5000)
    edge->score -= (5000-psl->tSize)/40;

edge->psl = psl;
#ifdef VERBOSE_LOG
fprintf(logFile, "  score %d milliBad %d minScore %d\n", edge->score,
	edge->milliBad, minScore);
#endif /* VERBOSE_LOG */
return edge;
}

void processSelf(char *fileName, struct oogClone **pCloneList,
	struct oogFrag **pFragList, struct oogEdge **pEdgeList,
	struct psl **pSelfPslList,
	struct hash *cloneHash, struct hash *fragHash)
/* Process self.psl.  Make up clone, fragment, and self edge list. */
{
struct lineFile *lf = pslFileOpen(fileName);
struct psl *pslList = NULL, *psl;
struct oogEdge *edge;
struct oogClone *qClone, *tClone;
struct oogFrag *qFrag, *tFrag;

printf("processing %s\n", fileName);
while ((psl = pslNext(lf)) != NULL)
    {
    qClone = cloneFromName(psl->qName, pCloneList, cloneHash);
    tClone = cloneFromName(psl->tName, pCloneList, cloneHash);
    qFrag = fragFromName(psl->qName, pFragList, fragHash, qClone, psl->qSize);
    tFrag = fragFromName(psl->tName, pFragList, fragHash, tClone, psl->tSize);
    if (!sameString(psl->qName, psl->tName))
	{
	edge = edgeFromSelfPsl(psl, qFrag, tFrag);
	if (edge != NULL && edge->score >= minScore)
	    {
	    slAddHead(pEdgeList, edge);
	    }
	}
    slAddHead(&pslList, psl);
    }
lineFileClose(&lf);
slReverse(&pslList);
slReverse(pEdgeList);
*pSelfPslList = pslList;
}


void fragRangeToRaftRange(int s, int e, int sz, short orientation, int raftOffset,
	int *retStart, int *retEnd)
/* Convert range from s to e inside fragment of size sz to raft coordinates. */
{
if (orientation > 0)
    {
    *retStart = raftOffset + s;
    *retEnd = raftOffset + e;
    }
else
    {
    *retStart = sz - e + raftOffset;
    *retEnd = sz - s + raftOffset;
    }
}

void fragAliRange(struct oogFrag *frag, struct psl *psl, int *retStart, int *retEnd)
/* Return range of aligning parts of frag. */
{
if (sameString(frag->name, psl->qName))
    {
    *retStart = psl->qStart;
    *retEnd = psl->qEnd;
    }
else
    {
    *retStart = psl->tStart;
    *retEnd = psl->tEnd;
    }
}

void pslFragToRaftAliRange(
	struct oogFrag *frag, short orientation, int raftOffset,
	struct psl *psl,
	int *retAliStart, int *retAliEnd)
/* Convert alignment range associated with psl and frag from frag
 * coordinates to raft coordinates, taking care of reversing if
 * on minus strand relative to raft. */
{
int s,e, sz;
fragAliRange(frag, psl, &s, &e);
sz = frag->size;
fragRangeToRaftRange(s, e, sz, orientation, raftOffset, retAliStart, retAliEnd);
}

void raftFragToRaftAliRange(struct raftFrag *rf, 
	struct psl *psl,
	int *retAliStart, int *retAliEnd)
/* Figure out start and end of alignment involving rf as defined by psl in
 * raft coordinates. */
{
pslFragToRaftAliRange(rf->frag, rf->orientation, rf->raftOffset,
	psl, retAliStart, retAliEnd);
}


struct raft *raftOfTwo(struct oogEdge *edge)
/* Return a new raft containing just two fragments joined by edge. */
{
struct raft *raft;
struct raftFrag *rf;
struct psl *psl = edge->psl;
int aliSize;
int maxDiff;
int bOffset;
int orientation = edge->orientation;
int aAliStart, aAliEnd;
int bAliStart, bAliEnd;

AllocVar(raft);
raft->start = 0;
raft->id = nextRaftId();
fragAliRange(edge->aSide, psl, &aAliStart, &aAliEnd);
fragAliRange(edge->bSide, psl, &bAliStart, &bAliEnd);
if (orientation > 0)
    bOffset = aAliStart - bAliStart;
else
    {
    bOffset = aAliStart - (edge->bSide->size - bAliEnd);
    }
raft->end = max(edge->bSide->size + bOffset, edge->aSide->size);

/* Position first frag. */
AllocVar(rf);
rf->raftOffset = 0;
rf->orientation = 1;
rf->frag = edge->aSide;
edge->aSide->raft = raft;
slAddHead(&raft->fragList, rf);

/* Position second frag. */
AllocVar(rf);
rf->raftOffset = bOffset;
rf->orientation = orientation;
rf->frag = edge->bSide;
edge->bSide->raft = raft;
slAddHead(&raft->fragList, rf);

fprintf(logFile, "raftOfTwo: %s(%d) %c %s(%d) @ %d   .... psl (%d %d) (%d %d) start %d end %d\n", 
        edge->aSide->name,  edge->aSide->size,
	(edge->orientation > 0 ? '+' : '-'),
	edge->bSide->name, edge->bSide->size, bOffset, 
	psl->qStart, psl->qEnd, psl->tStart, psl->tEnd,
	raft->start, raft->end);
return raft;
}


boolean fragMostlyAligns(struct oogFrag *frag, int raftOffset, short raftOrientation, int fragStart, int fragEnd,
    struct raft *raft, struct oogEdge *selfEdgeList)
/* Return TRUE if frag aligns with something already in raft between start and stop. */
{
static int usedArrayAlloc = 0;
static bool *usedArray = NULL;
struct oogEdge *edge;
struct raftFrag *rf;
int raftStart;
int raftEnd;
int checkSize;
int misMatchCount = 0;
int i;

if (raftOrientation > 0)
    {
    raftStart = fragStart + raftOffset;
    raftEnd = fragEnd + raftOffset;
    }
else
    {
    int fSize = frag->size;
    raftStart = (fSize - fragEnd) + raftOffset;
    raftEnd = (fSize - fragStart) + raftOffset;
    }
if (raftStart < raft->start + maxTailSize)
    raftStart = raft->start + maxTailSize;
if (raftEnd > raft->end - maxTailSize)
    raftEnd = raft->end - maxTailSize;
checkSize = raftEnd - raftStart;

if (checkSize <= 0)
    return TRUE;

/* Allocate array that will track which parts have and alignment. */
if (checkSize > usedArrayAlloc)
    {
    usedArrayAlloc = checkSize + 10000;
    usedArray = needMoreMem(usedArray, 0, usedArrayAlloc);
    }
zeroBytes(usedArray, checkSize);

for (rf = raft->fragList; rf != NULL; rf = rf->next)
    {
    int offset = rf->raftOffset;
    char *rfName = rf->frag->name;
    bool relevant = FALSE;
    int s,e, sz;

    for (edge = selfEdgeList; edge != NULL; edge = edge->next)
	{
	struct psl *psl = edge->psl;
	int rStart, rEnd, rSize;

	if (raftOrientation*rf->orientation == edge->orientation)
	    {
	    if (sameString(psl->qName, frag->name) && sameString(psl->tName, rfName))
		{
		s = psl->tStart;
		e = psl->tEnd;
		sz = psl->tSize;
		relevant = TRUE;
		}
	    else if (sameString(psl->tName, frag->name) && sameString(psl->qName, rfName))
		{
		s = psl->qStart;
		e = psl->qEnd;
		sz = psl->qSize;
		relevant = TRUE;
		}
	    }
	if (relevant)
	    {
	    if (rf->orientation > 0)
		{
		rStart = s + offset;
		rEnd = e + offset;
		}
	    else
		{
		rStart = sz - e + offset;
		rEnd = sz - s + offset;
		}
	    if (rStart < raftStart) rStart = raftStart;
	    if (rEnd > raftEnd) rEnd = raftEnd;
	    rSize = rEnd - rStart;
	    if (rSize >= checkSize)
		return TRUE;   /* We're totally covered. */
	    if (rSize > 0)
		memset(usedArray + rStart - raftStart, 1, rSize);
	    }
	}
    }
for (i=0; i<checkSize; ++i)
    {
    if (!usedArray[i]) ++misMatchCount;
    }
return ( misMatchCount < 10);
}

void addEdgeRefToList(struct oogEdge *edge, struct edgeRef **pList)
/* Add reference to edge to raft. */
{
struct edgeRef *er;
AllocVar(er);
er->edge = edge;
slAddHead(pList, er);
}

void addEdgeRefToRaft(struct raft *raft, struct oogEdge *edge)
/* Add reference to edge to raft. */
{
addEdgeRefToList(edge, &raft->refList);
}

boolean canMerge(struct oogFrag *frag, struct oogEdge *staple, struct raftFrag *socket, 
	struct raft *raft, struct oogEdge *selfEdgeList, int *retOffset, short *retOrientation)
/* Return TRUE if frag can be merged into raft  Staple is the edge wanting
 * to bring frag into raft. Socket is the raftFrag corresponding to the
 * other frag in staple, which is already part of raft. */
{
int offset;
short orientation;
int raftAliStart, raftAliEnd;
int fragAliStart, fragAliEnd;
struct psl *psl = staple->psl;
int ok;

*retOrientation = orientation = socket->orientation * staple->orientation;
raftFragToRaftAliRange(socket, psl, &raftAliStart, &raftAliEnd);
fragAliRange(frag, psl, &fragAliStart, &fragAliEnd);
if (orientation > 0)
    *retOffset = offset = raftAliStart - fragAliStart;
else
    *retOffset = offset = raftAliStart - (frag->size - fragAliEnd);
ok = fragMostlyAligns(frag, offset, orientation, maxTailSize, frag->size-maxTailSize, raft, selfEdgeList);
return ok;
}

void noteRedundant(struct oogEdge *edge)
/* Note that given edge is redundant. */
{
fprintf(logFile, "redundant edge from %s to %s\n", edge->aSide->name, edge->bSide->name);
}

void noteConflicting(struct oogEdge *edge, char *why)
/* Note that given edge is redundant. */
{
fprintf(logFile, "conflicting edge from %s to %s (%s).\n", edge->aSide->name, edge->bSide->name, why);
}

void normalizeRaft(struct raft *raft)
/* Rearrange all offsets so that raft goes from zero to length of raft.  Fix up
 * graph begin and end.*/
{
int sMin = 0x3ffffff;
int eMax = -sMin;
struct raftFrag *rf;

for (rf = raft->fragList; rf != NULL; rf = rf->next)
    {
    int s = rf->raftOffset;
    int e = s + rf->frag->size;
    if (s < sMin) sMin = s;
    if (e > eMax) eMax = e;
    }
raft->start = 0;
raft->end = eMax - sMin;
for (rf = raft->fragList; rf != NULL; rf = rf->next)
    rf->raftOffset -= sMin;
}


boolean extendRaft(struct raft *raft, struct raftFrag *socket, 
	struct oogEdge *edge, struct oogFrag *newFrag, struct oogEdge *selfEdgeList)
/* Try to extend raft to include other fragment in edge. */
{
int offset;
short orientation;

fprintf(logFile, "extend raft\n");
if (canMerge(newFrag, edge, socket, raft, selfEdgeList, &offset, &orientation))
    {
    struct raftFrag *rf;
    int rfragStart, rfragEnd;
    AllocVar(rf);
    rf->raftOffset = offset;
    rf->frag = newFrag;
    rf->orientation = orientation;
    slAddHead(&raft->fragList, rf);
    newFrag->raft = raft;
    rfragStart = offset;
    rfragEnd = offset + newFrag->size;
    if (rfragStart < raft->start)
	raft->start = rfragStart;
    if (rfragEnd > raft->end)
	raft->end = rfragEnd;
    fprintf(logFile, "extends ok @%d %c to %d %d\n", offset, (orientation > 0 ? '+' : '-'),
    	raft->start, raft->end);
    normalizeRaft(raft);
    return TRUE;
    }
else
    {
    noteConflicting(edge, "Can't extend raft");
    return FALSE;
    }
}

void flipRaftFrags(struct raft *raft)
/* Flip orientation of each frag within raft. */
{
int end = raft->end;
struct raftFrag *rf;

fprintf(logFile, "Flipping raft %d\n", raft->id);
for (rf = raft->fragList; rf != NULL; rf = rf->next)
    {
    rf->orientation = -rf->orientation;
    rf->raftOffset = end - (rf->raftOffset + rf->frag->size);
    }
raft->end = end - raft->start;
raft->start = 0;
}

void swallowRaftB(struct raft *aRaft, struct raft *bRaft, 
	int aAliStart, int bAliStart, int aAliEnd, int bAliEnd, int orientation)
/* Make raft A swallow raft B. */
{
struct raftFrag *rf;
int bStart, bEnd;
int offset;

if (orientation < 0)
    {
    /* First flip around raft B. */
    int oldAliStart;
    int bEnd = bRaft->end;

    flipRaftFrags(bRaft);

    /* Make corresponding flip in b alignment coordinates. */
    oldAliStart = bAliStart;
    bAliStart = bEnd - bAliEnd;
    bAliEnd = bEnd - oldAliStart;
    }

offset = aAliStart - bAliStart;
for (rf = bRaft->fragList; rf != NULL; rf = rf->next)
    {
    rf->raftOffset += offset;
    rf->frag->raft = aRaft;
    }
bStart = bRaft->start + offset;
bEnd = bRaft->end + offset;
if (bStart < aRaft->start)
    aRaft->start = bStart;
if (bEnd > aRaft->end)
    aRaft->end = bEnd;

aRaft->fragList = slCat(aRaft->fragList, bRaft->fragList);
bRaft->fragList = NULL;
aRaft->refList = slCat(aRaft->refList, bRaft->refList);
bRaft->refList = NULL;

fprintf(logFile, "Merged rafts at offset %d orient %d start %d end %d\n", offset, orientation,
    aRaft->start, aRaft->end);
normalizeRaft(aRaft);
}


boolean mergeRafts(struct raft **pRaftList, struct oogEdge *selfEdgeList, 
	struct raft *aRaft, struct raft *bRaft, 
	struct raftFrag *aRf, struct raftFrag *bRf, struct oogEdge *edge)
/* Try and merge two rafts joined by edge. */
{
int aRaftAliStart, aRaftAliEnd, bRaftAliStart, bRaftAliEnd;
int startTail, endTail;
struct psl *psl = edge->psl;
int relativeRaftOrientation = edge->orientation * aRf->orientation * bRf->orientation;

fprintf(logFile, "mergeRafts %d %d\n", aRaft->id, bRaft->id);

raftFragToRaftAliRange(aRf, psl, &aRaftAliStart, &aRaftAliEnd);
raftFragToRaftAliRange(bRf, psl, &bRaftAliStart, &bRaftAliEnd);

fprintf(logFile, "  a %s pos %d orient %d size %d raft %d raftStart %d raftEnd %d\n",
    aRf->frag->name, aRf->raftOffset, aRf->orientation, aRf->frag->size,
    aRaft->id, aRaft->start, aRaft->end);
fprintf(logFile, "  b %s pos %d orient %d size %d raft %d raftStart %d raftEnd %d\n",
    bRf->frag->name, bRf->raftOffset, bRf->orientation, bRf->frag->size,
    bRaft->id, bRaft->start, bRaft->end);


if (relativeRaftOrientation > 0)
    {
    startTail = min(aRaftAliStart - aRaft->start, bRaftAliStart - bRaft->start);
    endTail = min(aRaft->end - aRaftAliEnd, bRaft->end - bRaftAliEnd);
    }
else
    {
    startTail = min(aRaftAliStart - aRaft->start, bRaft->end - bRaftAliEnd);
    endTail = min(aRaft->end - aRaftAliEnd, bRaftAliStart - bRaft->start);
    }

if (startTail > maxTailSize)
    {
    noteConflicting(edge, "Merge long start tail");
    fprintf(logFile, " startTail %d\n", startTail);
    return FALSE;
    }
if (endTail > maxTailSize)
    {
    noteConflicting(edge, "Merge long end tail");
    fprintf(logFile, " endTail %d\n", endTail);
    return FALSE;
    }
swallowRaftB(aRaft, bRaft, aRaftAliStart, bRaftAliStart, aRaftAliEnd, bRaftAliEnd, relativeRaftOrientation);
slRemoveEl(pRaftList, bRaft);
bRaft->parent = aRaft;
slAddHead(&aRaft->children, bRaft);
return TRUE;
}

void maybeLogRaft(struct raft *raft)
/* Dump raft to log file if verbose. */
{
#ifdef VERBOSE_LOG
dumpRaft(raft, logFile);
#endif
}

struct mergeFailure
/* Keep track of a failure to merge raft.  Later on the
 * rafts might have been merged for other reasons, in which
 * case we want to add the failed edge back to the raft to
 * give the golden path maker more to work with. */
    {
    struct mergeFailure *next;	/* Next in list. */
    struct oogEdge *edge;       /* Edge that failed to merge. */
    struct raft *aRaft, *bRaft; /* Two rafts to merge. */
    struct raftFrag *aRf, *bRf; /* Two frags to mergs. */
    };

struct raft *raftAncientAncestor(struct raft *raft)
/* Return eldest ancestor in raft tree. */
{
while (raft->parent != NULL)
    raft = raft->parent;
return raft;
}

void reprocessMergeFailures(struct mergeFailure *mergeFailureList)
/* Try adding back edged in mergeFailure list to rafts. */
{
struct mergeFailure *mf;
struct raft *parent;

for (mf = mergeFailureList; mf != NULL; mf = mf->next)
    {
    parent = raftAncientAncestor(mf->aRaft);
    if (parent == raftAncientAncestor(mf->bRaft))
	{
	fprintf(logFile, "Upon further consideration the following edge merges ok:\n");
	dumpEdge(mf->edge, logFile);
	addEdgeRefToRaft(parent, mf->edge);
	}
    }
}



struct raft *makeRafts(struct oogFrag *fragList, struct oogEdge *selfEdgeList)
/* Make connected rafts of fragments. Returns doubly linked list of rafts. */
{
struct raft *raftList = NULL;
struct oogEdge *edge;
struct raft *aRaft, *bRaft, *raft;
struct raftFrag *aRf, *bRf, *rf;
boolean aIn, bIn;
int max = 10;
struct mergeFailure *mergeFailureList = NULL;

for (edge = selfEdgeList; edge != NULL; edge = edge->next)
    {
    struct psl *psl = edge->psl;
    boolean edgeOk = TRUE;

    dumpEdge(edge, logFile);
    fprintf(logFile, "psl: (%s %d %d of %d) %s (%s %d %d of %d)\n", 
	   psl->qName, psl->qStart, psl->qEnd, psl->qSize, 
	   psl->strand,
	   psl->tName, psl->tStart, psl->tEnd, psl->tSize);
    aIn = fragInRaft(raftList, edge->aSide, &aRaft, &aRf);
    bIn = fragInRaft(raftList, edge->bSide, &bRaft, &bRf);
    if (!aIn && !bIn)
	{
	raft = raftOfTwo(edge);
	addEdgeRefToRaft(raft, edge);
	maybeLogRaft(raft);
	slAddHead(&raftList, raft);
	}
    else if (aIn && bIn)
	{
	if (aRaft != bRaft)
	    {
	    edgeOk = mergeRafts(&raftList, selfEdgeList, aRaft, bRaft, aRf, bRf, edge);
	    if (edgeOk)
		{
		addEdgeRefToRaft(aRaft, edge);
		maybeLogRaft(aRaft);
		}
	    else
		{
		/* Need to look at this edge again later... */
		struct mergeFailure *mf;
		AllocVar(mf);
		mf->edge = edge;
		mf->aRaft = aRaft;
		mf->bRaft = bRaft;
		mf->aRf = aRf;
		mf->bRf = bRf;
		slAddHead(&mergeFailureList, mf);
		}
	    }
	else
	    {
	    if (aRf->orientation*bRf->orientation == edge->orientation)
		{
		noteRedundant(edge);
		addEdgeRefToRaft(aRaft, edge);
		maybeLogRaft(aRaft);
		}
	    else
		{
		edgeOk = FALSE;
		fprintf(logFile, "bad orientation in redundant edge\n");
		}
	    }
	}
    else if (aIn)
	{
	edgeOk = extendRaft(aRaft, aRf, edge, edge->bSide, selfEdgeList);
	if (edgeOk)
	    {
	    addEdgeRefToRaft(aRaft, edge);
	    maybeLogRaft(aRaft);
	    }
	}
    else if (bIn)
	{
	edgeOk = extendRaft(bRaft, bRf, edge, edge->aSide, selfEdgeList);
	if (edgeOk)
	    {
	    addEdgeRefToRaft(bRaft, edge);
	    maybeLogRaft(bRaft);
	    }
	}
    fprintf(logFile, "\n");
    if (edgeOk)
	pslTabOut(psl, raftPsl);
    }
reprocessMergeFailures(mergeFailureList);
slFreeList(&mergeFailureList);

for (raft = raftList; raft != NULL; raft = raft->next)
    {
    slSort(&raft->fragList, raftFragCmpOffset);
    }
slReverse(&raftList);
return raftList;
}

int raftedFragCount(struct oogFrag *fragList)
/* Return number of fragments that are part of a raft. */
{
struct oogFrag *frag;
int count = 0;
for (frag = fragList; frag != NULL; frag = frag->next)
    if (frag->raft != NULL)
	++count;
return count;
}

void makeSingletonRafts(struct oogFrag *fragList, struct raft **pRaftList)
/* Make up rafts around isolated fragments. */
{
struct oogFrag *frag;
struct raft *list = NULL, *raft;

for (frag = fragList; frag != NULL; frag = frag->next)
    {
    if (frag->raft == NULL)
	{
	raft = raftOfOne(frag);
	slAddHead(&list, raft);
	}
    }
slReverse(&list);
*pRaftList = slCat(*pRaftList, list);
}

static struct oogClone *ovlpA, *ovlpB;	/* Clones being checked for overlaps. */
static bool *ovlpHits;                  /* A bool for each base in ovlpA, TRUE if overlaps. */
static int ovlpAlloc;                   /* Size allocated for ovlpHits. */

static void startCloneOverlapEstimate(struct oogClone *a, struct oogClone *b)
/* Start things up to estimate overlaps between clones a and b */
{
int aSize;
ovlpA = a;
ovlpB = b;
aSize = a->size;
if (a->size > ovlpAlloc)
    {
    ovlpAlloc = a->size+100000;
    ExpandArray(ovlpHits, 0, ovlpAlloc);
    }
memset(ovlpHits, 0, aSize);
}

static void addEdgesToCloneOverlap(struct oogClone *a, struct oogClone *b, struct edgeRef *refList)
/* Add in edges to clone overlap. */
{
struct edgeRef *ref;
struct oogEdge *edge;
struct psl *psl;
struct oogFrag *aFrag;
int fragStart;
int fragEnd;
int fragOffset;
int size;
int aStart;

assert(a == ovlpA && b == ovlpB);
for (ref = refList; ref != NULL; ref = ref->next)
    {
    edge = ref->edge;
    if (edge->aSide->clone == a && edge->bSide->clone == b)
	aFrag = edge->aSide;
    else if (edge->bSide->clone == a && edge->aSide->clone == b)
	aFrag = edge->bSide;
    else
    	continue;
    psl = edge->psl;
    if (sameString(psl->qName, aFrag->name))
	{
	fragStart = psl->qStart;
	fragEnd = psl->qEnd;
	}
    else
	{
	fragStart = psl->tStart;
	fragEnd = psl->tEnd;
	}
    fragOffset = aFrag->clonePos;
    size = fragEnd - fragStart;
    aStart = fragOffset + fragStart;
    if (aStart + size > a->size)
	{
	size = a->size - aStart;
	if (size <= 0)
	    size = 0;
	}
    memset(ovlpHits+aStart, TRUE, size);
    }
}

int tallyCloneOverlap()
/* Tally total overlap between clones. */
{
int i;
int total = 0;
bool *hits = ovlpHits;
int size = ovlpA->size;

for (i=0; i<size; ++i)
    total += hits[i];
return total;
}

boolean cloneInRaft(struct oogClone *clone, struct raft *raft)
/* Return TRUE if clone is in raft. */
{
struct raftFrag *rf;
for (rf = raft->fragList; rf != NULL; rf = rf->next)
    if (rf->frag->clone == clone)
	return TRUE;
return FALSE;
}

int findOverlapBetweenClones(struct oogClone *aClone, 
	struct oogClone *bClone, struct raft *raftList)
/* Return approximate number of base pairs that overlap between
 * clones a and b. */
{
struct raft *raft;
struct raftFrag *frag;
bool possibleOverlap = FALSE;

startCloneOverlapEstimate(aClone, bClone);
for (raft = raftList; raft != NULL; raft = raft->next)
    {
    if (cloneInRaft(aClone, raft) && cloneInRaft(bClone, raft))
	{
	possibleOverlap = TRUE;
	addEdgesToCloneOverlap(aClone, bClone, raft->refList);
	}
    }
if (!possibleOverlap)
    return 0;
return tallyCloneOverlap();
}

char *ocpHashName(char *a, char *b)
/* Return name of ocp associated with a and b */
{
static char buf[256];
sprintf(buf, "%s^%s", a, b);
return buf;
}

struct overlappingClonePair *findOverlappingClones(struct oogClone *cloneList, struct raft *raftList, 
	struct hash *ocpHash)
/* Return list of overlapping clone pairs. */
{
struct overlappingClonePair *ocpList = NULL, *ocp;
struct oogClone *a, *b;
int overlap;
char *ocpName;

for (a = cloneList; a != NULL; a = a->next)
    {
    for (b = a->next; b != NULL; b = b->next)
	{
	overlap = findOverlapBetweenClones(a, b, raftList);
	if (overlap > 0)
	    {
	    AllocVar(ocp);
	    ocp->a = a;
	    ocp->b = b;
	    ocp->overlap = overlap;
	    slAddHead(&ocpList, ocp);
	    ocpName = ocpHashName(a->name, b->name);
	    hashAdd(ocpHash, ocpName, ocp);
	    }
	}
    }
slReverse(&ocpList);
fprintf(logFile, "Found %d overlapping pairs in %d clones\n", slCount(ocpList), slCount(cloneList));
return ocpList;
}

void saveOcp(char *fileName, struct overlappingClonePair *ocpList)
/* Save ocpList to file. */
{
FILE *f = mustOpen(fileName, "w");
struct overlappingClonePair *ocp;
for (ocp = ocpList; ocp != NULL; ocp = ocp->next)
    fprintf(f, "%-8s\t%-8s\t%d\n", ocp->a->name, ocp->b->name, ocp->overlap);
fclose(f);
}

int findHashedOverlap(char *aName, char *bName, struct hash *ocpHash)
/* Find how much a and b overlap. */
{
struct hashEl *hel;
struct overlappingClonePair *ocp;
char *ocpName;

ocpName = ocpHashName(aName, bName);
if ((hel = hashLookup(ocpHash, ocpName)) == NULL)
    {
    ocpName = ocpHashName(bName, aName);
    hel = hashLookup(ocpHash, ocpName);
    }
if (hel == NULL)
    return 0;
ocp = hel->val;
return ocp->overlap;
}

int findNodeOverlap(struct dlNode *a, struct dlNode *b, struct hash *ocpHash)
/* Find how much a and b overlap. */
{
struct bargeEl *aBel = a->val, *bBel = b->val;
char *aName = aBel->clone->name, *bName = bBel->clone->name;
return findHashedOverlap(aName, bName, ocpHash);
}

boolean placeClone(struct dlList *orderedList, struct dlNode *cNode,
	struct hash *ocpHash)
/* Place clone b that overlaps with clone a onto orderedList. A is already on list. */
{
struct dlNode *aNode, *bNode;
int abOverlap, acOverlap, bcOverlap;
struct bargeEl *a, *b, *c;


c = cNode->val;
    {
    fprintf(logFile, "trying to place %s in :\n", c->clone->name);
    for (aNode = orderedList->head; aNode->next != NULL; aNode = aNode->next)
	{
	a = aNode->val;
	fprintf(logFile, "  %s  %d\n", a->clone->name, findNodeOverlap(aNode, cNode, ocpHash));
	bNode = aNode->next;
	if (bNode->next != NULL)
	    {
	    b = aNode->val;
	    fprintf(logFile, "     abOverlap %d\n", findNodeOverlap(aNode, bNode, ocpHash));
	    }
	}
    }

for (aNode = orderedList->head; aNode->next != NULL; aNode = aNode->next)
    {
    bNode = aNode->next;
    a = aNode->val;
    b = bNode->val;
    if ((acOverlap = findNodeOverlap(aNode, cNode, ocpHash)) == 0)
	continue;
    if (bNode->next == NULL)
	{
	dlAddTail(orderedList, cNode);
	fprintf(logFile, "Add %s at tail\n", c->clone->name);
	return TRUE;
	}
    if ((bcOverlap = findNodeOverlap(bNode, cNode, ocpHash)) == 0)
	{
	struct dlNode *prevNode = aNode->prev;
	if (prevNode->prev != NULL)
	    {
	    int prevOverlap;
	    prevOverlap = findNodeOverlap(prevNode, cNode, ocpHash);
	    if (prevOverlap == 0)
		{
		fprintf(logFile, "Contradictory overlaps, not inserting %s\n", c->clone->name);
		return FALSE;
		}
	    }
	dlAddBefore(aNode, cNode);  /* Goes before aNode. */
	fprintf(logFile, "Add %s before %s ac overlap %d\n", c->clone->name, a->clone->name, 
		acOverlap);
	return TRUE;
	}
    abOverlap = findNodeOverlap(aNode, bNode, ocpHash);
    // assert(abOverlap != 0);
    if (abOverlap < acOverlap && abOverlap < bcOverlap)
	{
	dlAddAfter(aNode, cNode);   /* Goes between a and bNode */
	fprintf(logFile, "Add %s between %s and %s\n", c->clone->name, a->clone->name, b->clone->name);
	fprintf(logFile, "  abOverlap %d acOverlap %d bcOverlap %d\n", 
		abOverlap, acOverlap, bcOverlap);
	return TRUE;
	}
    }
fprintf(logFile, "Couldn't place %s in ordered list\n", c->clone->name);
return FALSE;
}


int cmpBargeElOffset(const void *va, const void *vb)
/* Compare two oogFrags to sort by default position . */
{
const struct bargeEl *a = *((struct bargeEl **)va);
const struct bargeEl *b = *((struct bargeEl **)vb);
return a->offset - b->offset;
}

boolean bargeLooksReversed(struct barge *barge)
/* Return TRUE if default coordinates suggest that barge should be
 * reversed. */
{
int totalDiff = 0;
int diff;
struct bargeEl *aBel, *bBel;
for (aBel = barge->cloneList; aBel != NULL; aBel = aBel->next)
    {
    if ((bBel = aBel->next) != NULL)
	{
	diff = bBel->clone->mapPos - aBel->clone->mapPos;
	totalDiff += diff;
	}
    }
return (totalDiff < 0);
}

void flipBarge(struct barge *barge)
/* Flip around offsets of barge. */
{
int size = barge->size;
struct bargeEl *bel;
int cloneEnd;

for (bel = barge->cloneList; bel != NULL; bel = bel->next)
    {
    cloneEnd = bel->offset + bel->clone->size;
    bel->offset = size - cloneEnd;
    }
slSort(&barge->cloneList, cmpBargeElOffset);
}

struct dlNode *findMostOverlappingNode(struct dlList *orderedList, 
	struct dlList *randomList, struct hash *ocpHash)
/* Find node on random list that overlaps most with any clone on ordered list. */
{
struct dlNode *oNode, *rNode;
int bestOverlap = 0;
struct dlNode *bestNode = NULL;
int overlap;

for (oNode = orderedList->head; oNode->next != NULL; oNode = oNode->next)
    {
    for (rNode = randomList->head; rNode->next != NULL; rNode = rNode->next)
	{
	overlap = findNodeOverlap(oNode, rNode, ocpHash);
	if (overlap > bestOverlap)
	    {
	    bestOverlap = overlap;
	    bestNode = rNode;
	    }
	}
    }
if (bestNode != NULL)
    {
    struct bargeEl *bel = bestNode->val;
    fprintf(logFile, "     mostOverlapping is  %s with %d\n", bel->clone->name, bestOverlap);
    }
return bestNode;
}


void markEnclosedClones(struct overlappingClonePair *ocpList)
/* Mark the completely encompassed clones by setting pointer to what
 * encloses them. */
{
struct overlappingClonePair *ocp;
double aCovered, bCovered;
double closeEnoughToEncompassed = 0.95;

fprintf(logFile, "Marking enclosed clones\n");
for (ocp = ocpList; ocp != NULL; ocp = ocp->next)
    {
    double overlap = ocp->overlap;
    aCovered = overlap/ocp->a->size;
    bCovered = overlap/ocp->b->size;
    if (aCovered > bCovered)
	{
    	if (aCovered > closeEnoughToEncompassed)
	    {
	    ocp->a->bel->enclosed = ocp->b->bel;
	    fprintf(logFile, "%s covers %s\n", ocp->b->name, ocp->a->name);
	    }
	}
    else 
	{
	if (bCovered > closeEnoughToEncompassed)
	    {
	    ocp->b->bel->enclosed = ocp->a->bel;
	    fprintf(logFile, "%s covers %s\n", ocp->a->name, ocp->b->name);
	    }
	}
    }
}

void dlCat(struct dlList *a, struct dlList *b)
/* Move items from b to end of a. */
{
struct dlNode *node;
while ((node = dlPopHead(b)) != NULL)
    dlAddTail(a, node);
}

boolean findOrderedSection(struct dlList *indyList, struct dlList *orderedList, 
	struct dlList *outcasts, struct hash *ocpHash)
/* Find an ordered overlapping section of clones from indyList and move them (in order) onto
 * orderedList.  Move stuff that overlaps but still doesn't fit in into outcasts. */
{
struct dlNode *aNode;
struct bargeEl *bel;

/* Put the first clone in the ordered list. */
fprintf(logFile, "findOrderedSection\n");
aNode = dlPopHead(indyList);
if (aNode == NULL)
    return FALSE;
bel = aNode->val;
dlAddHead(orderedList, aNode);

fprintf(logFile, "Root is %s\n", bel->clone->name);
fflush(logFile);

/* Extend ordered list until nothing is left of indy list. */
while (!dlEmpty(indyList))
    {
    aNode = findMostOverlappingNode(orderedList, indyList, ocpHash);
    if (aNode == NULL)
	break;
		{
		bel =  aNode->val;
		fprintf(logFile, "  next %s \n", bel->clone->name);
		}
    dlRemove(aNode);
    if (!placeClone(orderedList, aNode, ocpHash))
	{
	bel = aNode->val;
	fprintf(logFile, "Putting %s on outcast list\n", bel->clone->name);
	fprintf(stdout, "Putting %s on outcast list\n", bel->clone->name);
	dlAddTail(outcasts, aNode);
	}
    }
fprintf(logFile, "Found ordered section of %d\n", dlCount(orderedList));
fflush(logFile);
return !dlEmpty(orderedList);
}

void fillInBargeSizeAndCloneOffsets(struct barge *barge, struct hash *ocpHash)
/* Fill in barge->size and bel->offset . */
{
int offset = 0;
int overlap;
int size;
struct oogClone *clone;
struct bargeEl *bel, *nextBel;


for (bel = barge->cloneList; bel != NULL; bel = bel->next)
    {
    clone = bel->clone;
    bel->offset = offset;
    offset += clone->size;
    if ((nextBel = bel->next) != NULL)
	offset -= findHashedOverlap(clone->name, nextBel->clone->name, ocpHash);
    }
barge->size = offset;
}


struct barge *bargeFromListOfBels(struct dlList *bList, struct hash *ocpHash)
/* Return barge that has everything in belList in it.  Don't set
 * barge size or bel->offset yet. */
{
struct barge *barge;
static int id = 0;
struct dlNode *node;
struct bargeEl *bel;


/* Allocate barge and give unique id. */
AllocVar(barge);
barge->id = ++id;
for (node = bList->head; node->next != NULL; node = node->next)
    {
    bel = node->val;
    bel->clone->barge = barge;
    slAddHead(&barge->cloneList, bel);
    }
slReverse(&barge->cloneList);
fillInBargeSizeAndCloneOffsets(barge, ocpHash);
return barge;
}


void mergeEnclosed(struct dlList *enclosedList)
/* Merge enclosed clones into barges of the clone they live inside. */
{
struct barge *barge;
struct oogClone *clone, *outerClone;
static int id;
struct dlNode *node, *nextNode;
struct bargeEl *bel, *outerBel;

for (node = enclosedList->head; node->next != NULL; node = node->next)
    {
    bel = node->val;
    clone = bel->clone;
    for (outerBel = bel; outerBel->enclosed != NULL; outerBel = outerBel->enclosed)
	;
    outerClone = outerBel->clone;
    clone->barge = barge = outerClone->barge;
    slAddHead(&barge->cloneList, bel);
    bel->offset = outerBel->offset + (outerClone->size - clone->size)/2;
    }
}


struct barge *makeBargeList(struct oogClone *cloneList, struct raft *raftList, struct hash *ocpHash)
/* Create a list of barges out of connected fragments, but don't yet order
 * clones inside barges. */
{
struct dlList *orderedList = newDlList();
struct dlList *indyList = newDlList();
struct dlList *enclosedList = newDlList();
struct dlList *outcastList = newDlList();
struct overlappingClonePair *ocp, *ocpList;
struct bargeEl *bel;
struct oogClone *clone;
struct dlNode *aNode, *bNode, *nextNode;
struct barge *bargeList = NULL, *barge;

ocpList = findOverlappingClones(cloneList, raftList, ocpHash);
saveOcp(ocpFileName, ocpList);

/* Make up a barge el for each clone and initially put them all on the
 * independent list. */
for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    AllocVar(bel);
    bel->clone = clone;
    clone->bel = bel;
    bel->dlNode = dlAddValTail(indyList, bel);
    }

/* Move clones that are enclosed into enclosed list. */
markEnclosedClones(ocpList);
for (aNode = indyList->head; aNode->next != NULL; aNode = nextNode)
    {
    nextNode = aNode->next;
    bel = aNode->val;
    if (bel->enclosed)
	{
	dlRemove(aNode);
	dlAddTail(enclosedList, aNode);
	}
    }

/* Get an ordered section and build a barge on it. */
while (findOrderedSection(indyList, orderedList, outcastList, ocpHash))
    {
    fprintf(logFile, " %d indy %d ordered %d outcast\n", dlCount(indyList), 
	dlCount(orderedList), dlCount(outcastList));
    barge = bargeFromListOfBels(orderedList, ocpHash);
    slAddHead(&bargeList, barge);
    freeDlList(&orderedList);
    orderedList = newDlList();
    dlCat(indyList, outcastList);
    }
mergeEnclosed(enclosedList);
for (barge = bargeList; barge != NULL; barge = barge->next)
    {
    slSort(&barge->cloneList, cmpBargeElOffset);
    if (bargeLooksReversed(barge))
	flipBarge(barge);
    }
freeDlList(&orderedList);
freeDlList(&indyList);
freeDlList(&enclosedList);
freeDlList(&outcastList);
slReverse(&bargeList);
return bargeList;
}

int calcBargeMapPos(struct barge *barge)
/* Calculate barge map position by average map position of it's clones. */
{
struct bargeEl *bel;
int cloneCount = 0;
int totalPos = 0;

for (bel = barge->cloneList; bel != NULL; bel = bel->next)
    {
    totalPos += bel->clone->mapPos;
    ++cloneCount;
    }
return totalPos/cloneCount;
}

int cmpBargeMapPos(const void *va, const void *vb)
/* Compare two oogFrags to sort by default position . */
{
const struct barge *a = *((struct barge **)va);
const struct barge *b = *((struct barge **)vb);
return a->mapPos - b->mapPos;
}


struct barge *makeBarges(struct oogClone *cloneList, struct raft *raftList)
/* Make barges including ordering clones inside them. */
{
struct barge *bargeList, *barge;
struct hash *ocpHash = newHash(0);
struct raft *raft;
struct oogClone *clone;

bargeList = makeBargeList(cloneList, raftList, ocpHash);
fprintf(logFile, "Got %d barges\n", slCount(bargeList));
for (barge = bargeList; barge != NULL; barge = barge->next)
    {
	{
	struct bargeEl *bel;
	struct overlappingClonePair *ocp;

	fprintf(logFile, " barge %d\n", barge->id);
	for (bel = barge->cloneList; bel != NULL; bel = bel->next)
	    {
	    clone = bel->clone;
	    fprintf(logFile, "   %s map %d size %d\n", clone->name, clone->mapPos, clone->size);
	    }
	}
    barge->mapPos = calcBargeMapPos(barge);
    }
slSort(&bargeList, cmpBargeMapPos);


/* Link rafts to barges. */
for (raft = raftList; raft != NULL; raft = raft->next)
    {
    clone = raft->fragList->frag->clone;
    assert(clone->barge != NULL);
    raft->barge = clone->barge;
    }
freeHash(&ocpHash);
return bargeList;
}

void saveBarges(char *fileName, struct barge *bargeList, struct diGraph *bargeGraph, struct raft *raftList)
/* Save barge list to file. */
{
FILE *f = mustOpen(fileName, "w");
struct barge *barge, *nextBarge;
struct bargeEl *bel;
struct oogClone *clone;

fprintf(f, "Barge (Connected Clone) File ooGreedy version %d\n\n", version);
fprintf(f, "start\taccession\tsize\toverlap\n");
fprintf(f, "--------------------------------------\n");
for (barge = bargeList; barge != NULL; barge = barge->next)
    {
    for (bel = barge->cloneList; bel != NULL; bel = bel->next)
	{
	int overlap = 0;
	struct bargeEl *nextBel;
	clone = bel->clone;
	if ((nextBel = bel->next) != NULL)
	    {
	    overlap = findOverlapBetweenClones(clone, nextBel->clone, raftList);
	    }
	fprintf(f, "%6d\t%s\t%6d\t%6d\n",
	    clone->defaultPos, clone->name, clone->size, overlap);
	}
    if ((nextBarge = barge->next) != NULL)
	{
	if (dgDirectlyFollows(bargeGraph, barge->node, nextBarge->node))
	    {
	    fprintf(f, "----- bridged gap -----\n");
	    }
	else
	    {
	    fprintf(f, "----- open gap -----\n");
	    }
	}
    }
fclose(f);
}


void calcPositions(struct barge *bargeList, struct oogFrag *fragList, struct diGraph *bargeGraph)
/* Calculate positions of barges, clones, frags. */
{
int bargeOff = 0;
struct barge *barge, *nextBarge;
struct oogFrag *frag;
struct bargeEl *bel;
struct oogClone *clone;

/* Set barge positions. */
for (barge = bargeList; barge != NULL; barge = barge->next)
    {
    barge->offset = bargeOff;
    bargeOff += barge->size;
    if ((nextBarge = barge->next) != NULL)
	{
	if (dgDirectlyFollows(bargeGraph, barge->node, nextBarge->node))
	    bargeOff += bridgedBargeGapSize;
	else
	    bargeOff += unbridgedBargeGapSize;
	}
    }

/* Set clone positions. */
for (barge = bargeList; barge != NULL; barge = barge->next)
    {
    for (bel = barge->cloneList; bel != NULL; bel = bel->next)
	{
	clone = bel->clone;
	clone->defaultPos = barge->offset + bel->offset;
	clone->startNode->priority = clone->defaultPos;
	clone->endNode->priority = clone->defaultPos + clone->size;
	}
    }

/* Set frag positions. */
for (frag = fragList; frag != NULL; frag = frag->next)
    {
    frag->defaultPos = frag->clone->defaultPos + frag->clonePos;
    }
}

void dumpCcList(struct connectedComponent *ccList, FILE *f)
/* Write out info on connected component to file. */
{
struct connectedComponent *cc;
struct dgNodeRef *ref;
fprintf(f, "dumpCcList:\n");
for (cc = ccList; cc != NULL; cc = cc->next)
    {
    fprintf(f, "%d    ", cc->orientation); 
    for (ref = cc->subGraph; ref != NULL; ref = ref->next)
	{
	fprintf(f, "%s ", ref->node->name);
	}
    fprintf(f, "\n");
    }
}


boolean setCcOrientation(struct connectedComponent *cc)
/* Set cc->orientation to be consistent between subGraph and subLine
 * if possible. If not return FALSE. */
{
struct dgNodeRef *nRef;
struct raft *raft;
int initialOrientation;
struct mrnaConnectionRef *lRef;
struct mrnaConnection *mc;

/* Take care of singleton unoriented rafts. */
nRef = cc->subGraph;
raft = nRef->node->val;
if (raft->orientation == 0)
    {
    assert(nRef->next == NULL);
    assert(cc->subLine->next == NULL);
    raft->orientation = cc->subLine->mc->orientation;
    cc->orientation = 1;
    return TRUE;
    }

lRef = cc->subLine;
mc = lRef->mc;
initialOrientation = mc->orientation*mc->raft->orientation;
for (lRef = cc->subLine->next; lRef != NULL; lRef = lRef->next)
    {
    mc = lRef->mc;
    if (mc->orientation*mc->raft->orientation != initialOrientation)
	return FALSE;
    }
cc->orientation = initialOrientation;
return TRUE;
}


boolean flipConnectedRafts(struct diGraph *raftGraph, struct dgNodeRef *raftList)
/* Flip around all rafts in list.  Return FALSE if can't because would induce cycles. 
 */
{
struct dgEdgeRef *erList;
struct dgNodeRef *ref;
struct raft *raft;
boolean ok;

fprintf(logFile, "Flipping connected component of %d rafts\n", slCount(raftList));
erList = dgFindSubEdges(raftGraph, raftList);
dgSwapEdges(raftGraph, erList);
ok = !dgHasCycles(raftGraph);
if (ok)
    {
    for (ref = raftList; ref != NULL; ref = ref->next)
	{
	raft = ref->node->val;
	raft->orientation *= -1;
	}
    }
else
    {
    dgSwapEdges(raftGraph, erList);
    }
slFreeList(&erList);
return ok;
}

boolean forceForwardOrientation(struct diGraph *raftGraph, struct connectedComponent *ccList)
/* Flip around connected subgraph orientations if necessary. */
{
struct connectedComponent *cc;
struct mrnaConnectionRef *lRef;
for (cc = ccList; cc != NULL; cc = cc->next)
    {
    if (cc->orientation < 0)
	{
	if (!flipConnectedRafts(raftGraph, cc->subGraph))
	    return FALSE;
	cc->orientation = 0;
	for (lRef = cc->subLine; lRef != NULL; lRef = lRef->next)
	    {
	    lRef->mc->orientation *= -1;
	    }
	}
    }
return TRUE;
}

boolean wouldAddCycle(struct diGraph *graph, struct mrnaConnection *mcList)
/* Add bits of mcList not already in graph to graph.  See if it has
 * cycles, remove them. Report if has cycles. */
{
int mcCount = slCount(mcList);
struct mrnaConnection *mc;
static int mcAlloc = 0;
static bool *added;
int i;
boolean isCyclic;

if (mcAlloc < mcCount)
    {
    /* Make sure have room to track all. */
    mcAlloc = mcCount*2;
    ExpandArray(added, 0, mcAlloc);
    }
memset(added, 0, mcCount);
for (mc=mcList, i=0; mc->next != NULL; mc = mc->next, ++i)
    {
    if (!dgDirectlyFollows(graph, mc->raft->node, mc->next->raft->node))
	{
	added[i] = TRUE;
	dgConnect(graph, mc->raft->node, mc->next->raft->node);
	}
    }
isCyclic = dgHasCycles(graph);
for (mc=mcList, i=0; mc->next != NULL; mc = mc->next, ++i)
    {
    if (added[i])
	{
	dgDisconnect(graph, mc->raft->node, mc->next->raft->node);
	}
    }
return isCyclic;
}

boolean canAddMrnaLine(struct diGraph *graph, struct mrnaLine *ml, 
	struct connectedComponent *ccList)
/* Return TRUE if can add line to graph.  Reverse direction of mcList if need be
 * according to orientation.  Return orientation. */
{
struct mrnaConnection *mc;
short lineOrientation = 0;
short oneOrientation;
struct dgNode *a, *b;
struct connectedComponent *cc;

/* Don't bother with 0 and 1 element lists. */
if (ml->mcList == NULL || ml->mcList->next == NULL)
    return FALSE;

/* First check for conflicts in orientation. */
for (cc = ccList; cc != NULL; cc = cc->next)
    {
    if (!setCcOrientation(cc))
	{
	fprintf(logFile, "Line orientation conflict with graph and %s\n", ml->name);
	return FALSE;
	}
    }
if (!forceForwardOrientation(graph, ccList))
    return FALSE;
/* Check for cycles. */
if (wouldAddCycle(graph, ml->mcList))
    {
    fprintf(logFile, "Cycle conflict with graph and %s (%d)\n", ml->name,
	ml->score);
    return FALSE;
    }
return TRUE;
}

void addMrnaLine(struct diGraph *graph, struct mrnaLine *ml, 
	struct connectedComponent *ccList)
/* Add line (already checked not to have cycle or orientation conflicts) to graph. */
{
struct mrnaConnection *mc, *mcNext;
struct dgNode *a, *b;

for (mc = ml->mcList; (mcNext = mc->next) != NULL; mc = mcNext)
    {
    a = mc->raft->node;
    b = mcNext->raft->node;
    dgConnect(graph, a, b);
    }
}


void clearRaftCc(struct raft *raftList)
/* Clear the connected component pointers from raftList. */
{
struct raft *raft;
for (raft = raftList; raft != NULL; raft = raft->next)
    raft->cc = NULL;
}

struct connectedComponent *findConnectedComponentsThatTouchLine(struct diGraph *graph,
	struct raft *raftList, struct mrnaLine *ml)
/* Make up a connectedComponent list. */
{
struct connectedComponent *ccList = NULL, *cc;
struct mrnaConnection *mc;
struct mrnaConnectionRef *mcRef;
struct dgNodeRef *subGraph;

clearRaftCc(raftList);
dgClearConnFlags(graph);
for (mc = ml->mcList; mc != NULL; mc = mc->next)
    {
    if ((cc = mc->raft->cc) == NULL)
	{
	struct dgNodeRef *nr;
	AllocVar(cc);
	subGraph = dgFindNewConnectedWithVals(graph, mc->raft->node);
	cc->subGraph = subGraph;
	for (nr = cc->subGraph; nr != NULL; nr = nr->next)
	    {
	    struct raft *raft = nr->node->val;
	    raft->cc = cc;
	    }
	slAddHead(&ccList, cc);
	}
    AllocVar(mcRef);
    mcRef->mc = mc;
    slAddHead(&cc->subLine, mcRef);
    }
return ccList;
}




void ooDumpGraph(struct diGraph *dg, FILE *out)
/* Dump info on graph to output. */
{
struct dgNode *node;
struct dgConnection *con;
struct raft *raft;

fprintf(out, "raft\tori\tways out\n");
fprintf(out, "-------------------------\n");
for (node = dg->nodeList; node != NULL; node = node->next)
    {
    if (node->nextList == NULL && node->prevList == NULL)
	continue;
    raft = node->val;
    if (raft != NULL)
	{
	fprintf(out, "%s\t%c\t", node->name, orientChar(raft->orientation));
	}
    else
	{
	fprintf(out, "%s\t", node->name);
	}
    for (con = node->nextList; con != NULL; con = con->next)
	{
	fprintf(out, "%s,", con->node->name);
	}
    fprintf(out, "\n");
    }
}


void addToBargeGraph(struct diGraph *bargeGraph, struct mrnaLine *ml)
/* Add mrnaLine to bargeGraph.  Connect in both directions... */
{
struct barge *lastBarge = NULL, *barge;
struct mrnaConnection *mc;

for (mc = ml->mcList; mc != NULL; mc = mc->next)
    {
    barge = mc->raft->barge;
    if (lastBarge != NULL && barge != lastBarge)
	{
	dgConnect(bargeGraph, barge->node, lastBarge->node);
	dgConnect(bargeGraph, lastBarge->node, barge->node);
	}
    lastBarge = barge;
    }
}

void maybeLogGraph(struct diGraph *graph)
/* Dump graph to log if in verbose mode. */
{
#ifdef VERBOSE_LOG
ooDumpGraph(graph, logFile);
#endif
}

struct cloneEndPos
/* Info on the end of a clone. */
    {
    struct cloneEndPos *next;	/* Next in list. */
    struct oogClone *clone;	/* Clone this refers to. */
    int pos;                    /* Position (within barge) */
    boolean isEnd;              /* True if end rather than start of clone. */
    };

int cmpCloneEndPos(const void *va, const void *vb)
/* Compare two cloneEndPos to sort by pos. */
{
const struct cloneEndPos *a = *((struct cloneEndPos **)va);
const struct cloneEndPos *b = *((struct cloneEndPos **)vb);
return (a->pos - b->pos);
}

void addBargeToRaftGraph(struct diGraph *raftGraph, struct barge *barge)
/* Add a barge to the raft graph. */
{
struct bargeEl *bel;
struct oogClone *clone;
struct cloneEndPos *cepList = NULL, *cep;
struct dgNode *node, *lastNode = NULL;
char nodeName[256];

/* Build up list of clone starts and ends sorted by position in barge. */
for (bel = barge->cloneList; bel != NULL; bel = bel->next)
    {
    AllocVar(cep);
    clone = bel->clone;
    cep->clone = clone;
    clone->defaultPos = bel->offset;	/* This gets reset later. */
    cep->pos = bel->offset;
    cep->isEnd = FALSE;
    slAddHead(&cepList, cep);
    AllocVar(cep);
    cep->clone = clone;
    cep->pos = bel->offset + clone->size;
    cep->isEnd = TRUE;
    slAddHead(&cepList, cep);
    }
slSort(&cepList, cmpCloneEndPos);

for (cep = cepList; cep != NULL; cep = cep->next)
    {
    clone = cep->clone;
    if (cep->isEnd)
	{
	sprintf(nodeName, "end-%s", clone->name);
	clone->endNode = node = dgAddNode(raftGraph, nodeName, NULL);
	}
    else
	{
	sprintf(nodeName, "start-%s", clone->name);
	clone->startNode = node = dgAddNode(raftGraph, nodeName, NULL);
	}
    if (lastNode != NULL)
	dgConnect(raftGraph, lastNode, node);
    lastNode = node;
    }
slFreeList(&cepList);
}

struct oogClone *singleCloneForRaft(struct raft *raft)
/* If raft is made up of fragments from a single clone, return that
 * clone.  Otherwise return NULL. */
{
struct raftFrag *rf = raft->fragList;
struct oogClone *clone = rf->frag->clone;

for (rf = rf->next; rf != NULL; rf = rf->next)
    {
    if (rf->frag->clone != clone)
	return NULL;
    }
return clone;
}

struct barge *singleBargeForRaft(struct raft *raft)
/* Return barge for raft if all frags in raft belong to a single
 * barge, otherwise NULL. */
{
struct raftFrag *rf = raft->fragList;
struct barge *barge = rf->frag->clone->barge;

for (rf = rf->next; rf != NULL; rf = rf->next)
    {
    if (rf->frag->clone->barge != barge)
	return NULL;
    }
return barge;
}

boolean raftHitsClone(struct raft *raft, struct oogClone *clone)
/* Returns true if a fragment of raft is in clone. */
{
struct raftFrag *rf;

for (rf = raft->fragList; rf != NULL; rf = rf->next)
    {
    if (rf->frag->clone == clone)
	return TRUE;
    }
return FALSE;
}

boolean cloneBoundsInBarge(struct raft *raft, struct oogClone **retStart, 
	struct oogClone **retEnd)
/* Return clones that bound raft if raft lies in a single barge.
 * (Otherwise return FALSE.) */
{
struct barge *barge;
struct bargeEl *bel;
struct oogClone *clone, *start = NULL, *end = NULL;
int maxEndPos = -0x3ffffff;
int endPos;

if ((barge = singleBargeForRaft(raft)) == NULL)
    return FALSE;
for (bel = barge->cloneList; bel != NULL; bel = bel->next)
    {
    clone = bel->clone;
    if (raftHitsClone(raft, clone))
	{
	if (start == NULL)
	    start = clone;
	endPos = clone->defaultPos + clone->size;
	if (endPos > maxEndPos)
	    end = clone;
	}
    }
assert(start != NULL);
*retStart = start;
*retEnd = end;
return TRUE;
}

void addRaftToRaftGraph(struct diGraph *raftGraph, struct raft *raft)
/* Add a raft to raft graph - make sure it stays inside of involved
 * clones if at all possible. */
{
struct oogClone *clone, *startClone, *endClone;
struct dgNode *raftNode;

raft->node = raftNode = dgAddNumberedNode(raftGraph, raft->id, raft);
if ((clone = singleCloneForRaft(raft)) != NULL)
    {
    dgConnect(raftGraph, clone->startNode, raftNode);
    dgConnect(raftGraph, raftNode, clone->endNode);
    }
else if (cloneBoundsInBarge(raft, &startClone, &endClone))
    {
    dgConnect(raftGraph, startClone->startNode, raftNode);
    dgConnect(raftGraph, raftNode, endClone->endNode);
    }
else
    {
    warn("Raft %d lies in multiple barges", raft->id);
    }
}

void makeRaftGraphSkeleton(struct diGraph *raftGraph, struct barge *bargeList, 
	struct raft *raftList)
/* Make skeleton of raftGraph - dummy nodes for ends of each clone and 
 * real nodes for each raft. Constrain raft nodes to lie between appropriate
 * clone end nodes. */
{
struct barge *barge;
struct raft *raft;

uglyf("makeRaftGraphSkeleton\n");
for (barge = bargeList; barge != NULL; barge = barge->next)
    addBargeToRaftGraph(raftGraph, barge);
for (raft = raftList; raft != NULL; raft = raft->next)
    addRaftToRaftGraph(raftGraph, raft);
slReverse(&raftGraph->nodeList);
}

void reverseMl(struct mrnaLine *ml)
/* Reverse order of mrnaLine. */
{
struct mrnaConnection *mc;

for (mc = ml->mcList; mc != NULL; mc = mc->next)
    mc->orientation = -mc->orientation;
slReverse(&ml->mcList);
}

void makeGraphs(struct raft *raftList, struct barge *bargeList, 
	struct mrnaLine *mrnaLineList, struct diGraph **retRaftGraph, 
	struct diGraph **retBargeGraph)
/* Make a graphs out of mRNA lines. */
{
struct mrnaLine *ml;
struct raft *raft;
struct barge *barge;
struct dgNode *node;
struct diGraph *raftGraph = dgNew();
struct diGraph *bargeGraph = dgNew();
struct connectedComponent *ccList, *cc;
boolean okOne;
int okCount = 0;
int conflictCount = 0;
boolean revMrna;


fprintf(logFile, "\nmakeGraphs\n");

/* Add skeleton of clones to raftGraph. */
makeRaftGraphSkeleton(raftGraph, bargeList, raftList);
ooDumpGraph(raftGraph, logFile);
uglyf("creating barge graph\n");

/* Add all barges to barge graph. */
for (barge = bargeList; barge != NULL; barge = barge->next)
    barge->node = node = dgAddNumberedNode(bargeGraph, barge->id, barge);
slReverse(&bargeGraph->nodeList);

/* Add in non-conflicting mrna lines one at a time. */
for (ml = mrnaLineList; ml != NULL; ml = ml->next)
    {
    okOne = FALSE;
    for (revMrna = FALSE; revMrna <= TRUE; ++revMrna)
	{
	ccList = findConnectedComponentsThatTouchLine(raftGraph, raftList, ml);
	dumpMrnaLine(ml, logFile);
	dumpCcList(ccList, logFile);
	if (canAddMrnaLine(raftGraph, ml, ccList))
	    {
	    addMrnaLine(raftGraph, ml, ccList);
	    addToBargeGraph(bargeGraph, ml);
	    dumpMrnaLine(ml, logFile);
	    fprintf(logFile, "Graph after addMrnaLine:\n");
	    maybeLogGraph(raftGraph);
	    okOne = TRUE;
	    break;
	    }
	reverseMl(ml);
	}
    if (okOne)
	++okCount;
    else
	{
	fprintf(logFile, "Couldn't add mrnaLine\n");
	++conflictCount;
	}
    fprintf(logFile, "\n");
    }
fprintf(logFile, "%d mrnaLines ok, %d conflicted\n", okCount, conflictCount);
fprintf(stdout, "%d mrnaLines ok, %d conflicted\n", okCount, conflictCount);
*retRaftGraph = raftGraph;
*retBargeGraph = bargeGraph;
}

void loadClone(char *faName, struct oogClone *clone, struct hash *fragHash, struct oogFrag **pFragList)
/* Position fragments within a clone based on position in
 * .fa file. */
{
FILE *f = mustOpen(faName, "r");
DNA *dna;
int sizeOne;
int totalSize = 0;
struct hashEl *hel;
struct oogFrag *frag;
struct dnaSeq *seq;
char *commentLine;

while (faReadNext(f, NULL, TRUE, &commentLine, &seq))
    {
    if ((hel = hashLookup(fragHash, seq->name)) != NULL)
	{
	warn("Duplicate %s, ignoring all but first\n", seq->name);
	}
    else
	{
	AllocVar(frag);
	slAddHead(pFragList, frag);
	hel = hashAdd(fragHash, seq->name, frag);
	frag->name = hel->name;
	frag->seq = seq;
	frag->clone = clone;
	frag->size = seq->size;
	frag->clonePos = totalSize;
	totalSize += seq->size;
	}
    }
clone->size = totalSize;
fclose(f);
}

int loadAllClones(char *infoName, char *genoList, 
	struct hash *fragHash, struct hash *cloneHash,
	struct oogFrag **pFragList, struct oogClone **pCloneList,
	char **retCtgName)
/* Consult info and geno list and associated fa files to build
 * default positions for fragments. */
{
struct lineFile *lf;
char *line;
int lineSize;
char *words[3];
int wordCount;
char **genoFiles;
int genoCount;
char *faFile;
char *acc;
int offset;
struct hashEl *hel;
struct oogClone *clone;
struct oogFrag *frag;
char *gBuf;
int i;
char dir[256], name[128],extension[64];
int endPos, lastEnd = 0;
int officialContigSize = 0;
struct slName *miaList = NULL, *mia;

/* Process geno.lst file. */
readAllWords(genoList, &genoFiles, &genoCount, &gBuf);
if (genoCount <= 0)
    errAbort("%s is empty\n", genoList);
for (i=0; i<genoCount; ++i)
    {
    faFile = genoFiles[i];
    splitPath(faFile, dir, name, extension);
    acc = name;
    if (fileExists(faFile))
	{
	if ((hel = hashLookup(cloneHash, acc)) != NULL)
	    {
	    warn("Duplicate %s in geno.lst, ignoring all but first", acc);
	    }
	else
	    {
	    AllocVar(clone);
	    slAddHead(pCloneList, clone);
	    hel = hashAdd(cloneHash, acc, clone);
	    clone->name = hel->name;
	    loadClone(faFile, clone, fragHash, pFragList);
	    }
	}
    else
	{
	warn("No sequence for %s", acc);
	mia = newSlName(acc);
	slAddHead(&miaList, mia);
	}
    }

/* Process info file. */
lf = lineFileOpen(infoName, TRUE);
if (!lineFileNext(lf, &line, &lineSize))
    errAbort("Empty info file %s\n", infoName);
if ((wordCount = chopLine(line, words)) < 2)
    errAbort("Bad header on %s\n", lf->fileName);
*retCtgName = cloneString(words[0]);
if (wordCount >= 3)
    officialContigSize = atoi(words[2]);
while (lineFileNext(lf, &line, &lineSize))
    {
    if ((wordCount = chopLine(line, words)) < 2)
	errAbort("Bad info file format line %d of %s\n", lf->lineIx, lf->fileName);
    if (!words[1][0] == '-' && !isdigit(words[1][0]))
	errAbort("Bad info file format line %d of %s\n", lf->lineIx, lf->fileName);
    acc = words[0];
    if ((hel = hashLookup(cloneHash, acc)) == NULL)
	{
	if (!slNameInList(miaList, acc))
	    {
	    fprintf(logFile, "Clone %s is in info but not geno.lst", acc);
	    }
	}
    else
	{
	clone = hel->val;
	clone->mapPos = atoi(words[1]) * 1000;
	if (wordCount >= 3)
	    clone->phase = atoi(words[2]);
	}
    }
lineFileClose(&lf);

for (frag = *pFragList; frag != NULL; frag = frag->next)
    {
    frag->defaultPos = frag->clone->mapPos + frag->clonePos;
    endPos = frag->defaultPos + frag->size;
    if (endPos > lastEnd)
	lastEnd = endPos;
    }
freeMem(gBuf);
slFreeList(&miaList);
slReverse(pFragList);
slReverse(pCloneList);
if (officialContigSize > 0)
    return officialContigSize;
else
    return lastEnd;
}

void oneRaftDefaultPosition(struct raft *raft)
/* Set position of raft to weighted average of position of fragments. */
{
double totalBases = 0;
double weightedTotal = 0;
struct raftFrag *rf;
struct oogFrag *frag;
double sizeOne;
double posOne;

for (rf = raft->fragList; rf != NULL; rf = rf->next)
    {
    frag = rf->frag;
    sizeOne = frag->size;
    totalBases += sizeOne;
    posOne = frag->defaultPos + 0.5*sizeOne;
    weightedTotal += sizeOne*posOne;
    }
raft->node->priority = raft->defaultPos = round(weightedTotal/totalBases);
}

void setRaftDefaultPositions(struct raft *raftList)
/* Set position of all rafts to weighted average of position of fragments. */
{
struct raft *raft;
for (raft = raftList; raft != NULL; raft = raft->next)
    oneRaftDefaultPosition(raft);
}

void writeFragMap(char *fragMapName, struct oogFrag *fragList)
/* Write out default positions of frags. */
{
struct oogFrag *frag;
FILE *f = mustOpen(fragMapName, "w");
for (frag = fragList; frag != NULL; frag = frag->next)
    {
    fprintf(f, "%s\t%d\t%d\n", frag->name, frag->defaultPos, frag->size);
    }
fclose(f);
}

struct dgNodeRef *orderRafts(struct diGraph *graph)
/* Return ref list of ordered raft nodes. */
{
return dgConstrainedPriorityOrder(graph);
}

#ifdef CRUFTY
struct dgNodeRef *orderRafts(struct diGraph *graph)
/* Return ref list of ordered raft nodes. */
{
struct dlList *threadList = newDlList();
struct dgNode *graphNode, *gn, *ngn;
struct dgConnection *wayOut;
struct dlNode *threadEl;
struct dgNodeRef *orderedList = NULL, *ref;
struct raft *raft;
struct hash *uniqHash = newHash(0);  /* Bandaide! */
char raftName[16];

/* Start out thread list with all nodes with no way in. */
for (graphNode = graph->nodeList; graphNode != NULL; graphNode = graphNode->next)
    {
    if (graphNode->prevList == NULL)
	dlAddValTail(threadList, graphNode);
    }

while (!dlEmpty(threadList))
    {
    int minPos = 0x3fffffff;
    struct dlNode *minThread = NULL;

    /* Find thread with smallest default value. */
    for (threadEl = threadList->head; threadEl->next != NULL; threadEl = threadEl->next)
	{
	graphNode = threadEl->val;
	raft = graphNode->val;
	if (raft->defaultPos < minPos)
	    {
	    minPos = raft->defaultPos;
	    minThread = threadEl;
	    }
	}

    /* Put reference to it on ordered list. */
    graphNode = minThread->val;
    raft = graphNode->val;
    sprintf(raftName, "%d", raft->id);
    if (hashLookup(uniqHash, raftName) != NULL)
	{
	warn("Attempted duplicate use of %d", raft->id);
	}
    else
	{
	hashAdd(uniqHash, raftName, raft);
	AllocVar(ref);
	ref->node = graphNode;
	slAddHead(&orderedList, ref);
	}


    /* Advance or kill the thread as need be.  It starts out killed.  May be rescued later. */
    dlRemove(minThread);	
    freeMem(minThread);
    for (wayOut = graphNode->nextList; wayOut != NULL; wayOut = wayOut->next)
	{
	boolean gotAlt = FALSE;
	ngn = wayOut->node;
	if (ngn->prevList->next != NULL)
	/* If more than way into next node, see if can get to it from a living thread. */
	    {
	    for (threadEl = threadList->head; threadEl->next != NULL; threadEl = threadEl->next)
		{
		gn = threadEl->val;
		if (dgPathExists(graph, gn, ngn))
		    {
		    gotAlt = TRUE;
		    break;
		    }
		}
	    }
	if (!gotAlt)
	    {
	    /* Regenerate thread if it's the unique way to get somewhere else. */
	    dlAddValTail(threadList, ngn);
	    }
	}
    }
freeDlList(&threadList);
freeHash(&uniqHash);
slReverse(&orderedList);
return orderedList;
}
#endif

void writeOrderList(struct dgNodeRef *orderList, char *raftOrderName)
/* Write out raft id's in order to raftOrderName. */
{
struct dgNodeRef *ref;
struct raft *raft;
FILE *f = mustOpen(raftOrderName, "w");

for (ref = orderList; ref != NULL; ref = ref->next)
    {
    raft = ref->node->val;
    if (raft != NULL)
	{
	fprintf(f, "%d %c defaultPos %d\n", raft->id, 
	    (raft->orientation < 0 ? '-' : '+'),
	    raft->defaultPos);
	}
    }
fclose(f);
}

int raftFlipTendency(struct raft *raft)
/* Return how badly raft would like to fit to better
 * accomodate default coordinates. */
{
/* raft->fragList is sorted already, and needs to be for
 * this to work. */
int diffTotal = 0;
int diffOne;
struct raftFrag *rf, *rfNext;

for (rf = raft->fragList; rf != NULL; rf = rf->next)
    {
    if ((rfNext = rf->next) != NULL)
	{
	diffOne = rfNext->frag->defaultPos - rf->frag->defaultPos;
	diffTotal += diffOne;
	}
    }
return diffTotal;
}

void flipNearDefaults(struct diGraph *graph)
/* Flip around connected components of graphs so as to make them
 * closer to default values. */
{
struct dgNodeRef *ccList, *cc, *ccNext;
struct raft *raft, *nextRaft;

dgClearConnFlags(graph);
while ((ccList = dgFindNextConnectedWithVals(graph)) != NULL)
    {
    int flipTendency = 0;
    int oneFlipTendency;
    int raftDistance;

    for (cc = ccList; cc != NULL; cc = cc->next)
	{
	raft = cc->node->val;
	if (raft != NULL)
	    {
	    if (raft->orientation == 0)
		raft->orientation = 1;
	    oneFlipTendency = raftFlipTendency(raft);
	    flipTendency += oneFlipTendency;
	    if ((ccNext = cc->next) != NULL)
		{
		nextRaft = ccNext->node->val;
		if (nextRaft != NULL)
		    {
		    raftDistance = nextRaft->defaultPos - raft->defaultPos;
		    if (dgPathExists(graph, ccNext->node, cc->node))
			{
			raftDistance = -raftDistance;
			}
		    flipTendency += 4*raftDistance;
		    }
		}
	    }
	}
    if (flipTendency < 0)
	{
	fprintf(logFile, "Flipping tendency = %d\n", flipTendency);
	flipConnectedRafts(graph, ccList);
	}
    slFreeList(&ccList);
    }
}

enum gapType
/* Type of gap this is. */
    {
    gtUnknown = 0,	/* Who knows what type? */
    gtFragment = 1,     /* Gap between fragments of a clone. */
    gtClone = 2,        /* Gap between clones. */
    gtContig = 3        /* Gap between contigs. */
    };
char *gapTypeNames[] = { "?", "fragment", "clone", "contig"};

enum bridgeType
/* Type of bridge covering gap. */
    {
    btUnknown = 0,     /* Who knows... */
    btYes = 1,         /* Some sort of bridge. */
    btNo = 2,          /* No bridge. */
    btMrna = 3,        /* Bridged by mRNA. */
    btEst = 4,         /* Bridged by EST. */
    btEstPair = 5,     /* Bridged by EST pair. */
    btBacEndBair = 6   /* Bridged by BAC end pair. */
    };
char *bridgeTypeNames[] = { "?", "yes", "no", "mRNA", "EST", "EST_pair", "BAC_end_pair"};

struct goldenPath
/* The way to patch together the sequence for this contig. */
    {
    struct goldenPath *next;    /* Next in list. */
    struct oogFrag *frag;	/* fragment or NULL if "just N's" */
    int fragStart;              /* Start in fragment.  If orientation -1 this is size-end.  */
    int fragEnd;                /* End in fragment.  If orientation -1 this is size-start. */
    short orientation;          /* Orientation of fragment. */
    enum gapType gapType;       /* Gap type (just for N's) */
    enum bridgeType bridgeType; /* Bridge type (just for N's) */
    };

struct raftPath
/* A golden path through one raft */
    {
    struct raftPath *next;     /* Next in list. */
    struct goldenPath *gpList; /* Golden path through this raft. */
    struct raft *raft;	       /* Which raft this is. */
    int start;	                /* Start in contig coordinates. */
    int size;                  /* size of raft. */
    };

void dumpGp(struct goldenPath *gp, FILE *f)
/* Write out golden path element. */
{
fprintf(f, "%d\t", gp->fragEnd - gp->fragStart);
if (gp->frag != NULL)
    {
    fprintf(f, "%s\t%c\t%d\t%d\n", gp->frag->name, (gp->orientation > 0 ? '+' : '-'),
	    gp->fragStart, gp->fragEnd);
    }
else
    {
    fprintf(f, "N\n");
    }
}

enum {bigEnoughSize = 20};

int firstBigBlock(unsigned *sizes, int count)
/* Find first big block in list, or if can't find one then first block. */
{
int i;
for (i=0; i<count; ++i)
    if (sizes[i] >= bigEnoughSize)
	return i;
return 0;
}

int lastBigBlock(unsigned *sizes, int count)
/* Find last big block in list or if can't find one then last block. */
{
int i = count;
while (--i >= 0)
    if (sizes[i] >= bigEnoughSize)
	return i;
return count-1;
}

int midBigBlock(unsigned *sizes, int count)
/* Find middle big block in list of if can't then just plain middle block. */
{
if (count == 1)
    return 0;
else
    {
    int half = count/2;
    int betterHalf = (count+1)/2;
    int i;

    for (i=0; i<=betterHalf; ++i)
	{
	int ix;
	ix = half + i;
	if (ix < count && sizes[ix] >= bigEnoughSize)
	    return ix;
	ix = half - i;
	if (ix >= 0 && sizes[ix] >= bigEnoughSize)
	    return ix;
	}
    return half;
    }
}

void forwardCopyBlocks(int blockCount, unsigned *sizes, unsigned *oSizes,
	unsigned *aStarts, unsigned *oaStarts, unsigned *bStarts, unsigned *obStarts)
/* Copy blocks without any swapping. */
{
int i;
for (i=0; i<blockCount; ++i)
    {
    oaStarts[i] = aStarts[i];
    obStarts[i] = bStarts[i];
    oSizes[i] = sizes[i];
    }
}

void reverseCopyBlocks(int blockCount, int aSize, int bSize,
	unsigned *sizes, unsigned *oSizes,
	unsigned *aStarts, unsigned *oaStarts, 
	unsigned *bStarts, unsigned *obStarts)
/* Copy blocks and swap. */
{
int i;
for (i=0; i<blockCount; ++i)
    {
    int revIx = blockCount-1-i;
    int blockSize = sizes[revIx];
    oaStarts[i] = aSize - (aStarts[revIx] + blockSize);
    obStarts[i] = bSize - (bStarts[revIx] + blockSize);
    oSizes[i] = blockSize;
    }
}

enum crossAt {crossUsual, crossMiddle, crossExtreme};

void findCrossover(struct oogEdge *edge,  struct oogFrag *aFrag, struct oogFrag *bFrag,
	struct goldenPath *a, struct goldenPath *b, enum crossAt atMiddle)
/* Figure out crossover point to get from frag to ext->other.  Use this and data in 'a'
 * to fill in 'b' and update ends of 'a' */
{
int aSize = aFrag->size, bSize = bFrag->size;
struct psl *psl = edge->psl;
int cbIx;
int cbSize;
int cbPt;
int aCross, bCross;
static int blockAlloc = 0;
int blockCount = psl->blockCount;
static unsigned *oaStarts = NULL, *obStarts = NULL, *oBlockSizes = NULL;
short aOrient = a->orientation, bOrient;
int aQual, bQual;
bool moreFromA;	/* Take more sequence from A? */

fprintf(logFile, "findCrossover %s(%d) %d %s(%d)\n", 
	aFrag->name, a->orientation, edge->orientation, bFrag->name, b->orientation);
fprintf(logFile, "psl: (%s %d %d of %d) %s (%s %d %d of %d)\n", 
       psl->qName, psl->qStart, psl->qEnd, psl->qSize, 
       psl->strand,
       psl->tName, psl->tStart, psl->tEnd, psl->tSize);
/* Make sure local variables that hold block by block alignment
 * info are big enough. */
if (blockAlloc < blockCount)
    {
    blockAlloc = blockCount*2;
    ExpandArray(oaStarts, 0, blockAlloc);
    ExpandArray(obStarts, 0, blockAlloc);
    ExpandArray(oBlockSizes, 0, blockAlloc);
    }


/* Get alignment from underlying psl and flip it as necessary so
 * that it become an alignment between the strand of a passed
 * in in "a->orientation" and whatever strand of b will stick.
 *
 * "a" can either be "q" or "t" in the psl, we don't know yet.
 * Inside psl "t" is always forward strand, "q" can vary.
 *
 * A further complication of this is that when "q" is 
 * reversed the coordinates of qStart and qEnd are in
 * terms of the forward strand, but the qStarts[i] and
 * qEnds[i] are in terms of the reverse strand, which
 * is related by the expression:
 *     qRevStart = qSize - qForEnd
 *     qRevEnd   = qSize - qForStart
 */

if (sameString(aFrag->name, psl->qName))
    {
    if (aOrient > 0)
	{
	if (edge->orientation > 0)
	    {
	    /* Straight forward case. */
	    forwardCopyBlocks(blockCount, psl->blockSizes, oBlockSizes, 
	    	psl->qStarts, oaStarts, psl->tStarts, obStarts);
	    bOrient = 1;
	    fprintf(logFile, ">>passed 1<<\n");
	    }
	else
	    {
	    /* Q is flipped and corresponds to A which is forward.
	     * Here we have to flip both Q and T as we copy to A and B. */
	    reverseCopyBlocks(blockCount, aSize, bSize,
	    	psl->blockSizes, oBlockSizes, 
	    	psl->qStarts, oaStarts, psl->tStarts, obStarts);
	    bOrient = -1;
	    fprintf(logFile, ">>passed 2<<\n");
	    }
	}
    else   /* a->orientation < 0 */
	{
	if (edge->orientation > 0)
	    {
	    /* Q and T are forward, but A is flipped.
	     * Have to flip both Q and T as we copy to A and B */
	    reverseCopyBlocks(blockCount, aSize, bSize,
	    	psl->blockSizes, oBlockSizes, 
	    	psl->qStarts, oaStarts, psl->tStarts, obStarts);
	    bOrient = -1;
	    fprintf(logFile, ">>passed 3<<\n");
	    }
	else
	    {
	    /* Q is reversed, T forward
	     * A reversed     B forward */
	    forwardCopyBlocks(blockCount, psl->blockSizes, oBlockSizes, 
	    	psl->qStarts, oaStarts, psl->tStarts, obStarts);
	    bOrient = 1;
	    fprintf(logFile, ">>passed 4<<\n");
	    }
	}
    }
else  /* A == T */
    {
    if (aOrient > 0)
	{
	if (edge->orientation > 0)
	    {
	    /* T forward  Q forward
	     * A forward  B forward. */
	    forwardCopyBlocks(blockCount, psl->blockSizes, oBlockSizes, 
	    	psl->tStarts, oaStarts, psl->qStarts, obStarts);
	    bOrient = 1;
	    fprintf(logFile, ">>passed 5<<\n");
	    }
	else
	    {
	    /* T forward Q reversed
	     * A forward B reversed */
	    forwardCopyBlocks(blockCount, psl->blockSizes, oBlockSizes, 
	    	psl->tStarts, oaStarts, psl->qStarts, obStarts);
	    bOrient = -1;
	    fprintf(logFile, ">>passed 6<<\n");
	    }
	}
    else	/* aOrient < 0 */
	{
	if (edge->orientation > 0)
	    {
	    /* T forward  Q forward
	     * A reversed B reversed */
	    reverseCopyBlocks(blockCount, aSize, bSize,
	    	psl->blockSizes, oBlockSizes, 
	    	psl->tStarts, oaStarts, psl->qStarts, obStarts);
	    bOrient = -1;
	    fprintf(logFile, ">>passed 7<<\n");
	    }
	else
	    {
	    /* T forward  Q reversed
	     * A reversed B forward */
	    reverseCopyBlocks(blockCount, aSize, bSize,
	    	psl->blockSizes, oBlockSizes, 
	    	psl->tStarts, oaStarts, psl->qStarts, obStarts);
	    bOrient = 1;
	    fprintf(logFile, ">>passed 8<<\n");
	    }
	}
    }

aQual = fragSeqQual(aFrag);
bQual = fragSeqQual(bFrag);
fprintf(logFile, "aOrient %d bOrient %d aQual %d bQual %d\n",
     aOrient, bOrient, aQual, bQual);
moreFromA = (aQual > bQual);
if (atMiddle == crossExtreme) 
    moreFromA = !moreFromA;

if (atMiddle == crossMiddle)
    {
    cbIx = midBigBlock(oBlockSizes, blockCount);
    }
else
    {
    if (moreFromA)
	cbIx = lastBigBlock(oBlockSizes, blockCount);
    else
	cbIx = firstBigBlock(oBlockSizes, blockCount);
    }

cbSize = oBlockSizes[cbIx];
cbPt = cbSize/2;
if (atMiddle != crossMiddle)
    {
    if (cbPt > 250)
	cbPt = cbSize - 250;
    if (!moreFromA)
	{
	cbPt = cbSize - cbPt;
	}
    }
aCross = oaStarts[cbIx] + cbPt;
if (aOrient > 0)
    {
    a->fragEnd = aCross;
    }
else
    {
    a->fragStart = aSize - aCross;
    }
bCross = obStarts[cbIx] + cbPt;
if (bOrient > 0)
    {
    b->fragStart = bCross;
    b->fragEnd = bSize;
    }
else
    {
    b->fragStart = 0;
    b->fragEnd = bSize - bCross;
    }
b->orientation = bOrient;
b->frag = bFrag;
}

struct raftFragRef 
/* Reference to a raft frag. */
    {
    struct raftFragRef *next;  /* Next in list. */
    struct raftFrag *rf;     /* Ref to frag. */
    struct goldenPath *gp;   /* Gp node associated with this. */
    enum crossAt crossAt;    /* Where to look for crossover. */
    };

struct raftFragRef *makeRfrList(struct raftFrag **bestFrags, int raftSize)
/* Return list of raft frag refs based on bestFrags, condensing bases next
 * to each other that refer to a single rf into one ref. */
{
struct raftFragRef *rfr, *rfrList = NULL;
struct raftFrag *rf, *last = NULL;
int i;

for (i=0; i<raftSize; ++i)
    {
    rf = bestFrags[i];
    if (rf != last && rf != NULL)
	{
	AllocVar(rfr);
	rfr->rf = rf;
	slAddHead(&rfrList, rfr);
	last = rf;
	}
    }
slReverse(&rfrList);
return rfrList;
}

struct raftFragRef *rfrLastRef(struct raftFragRef *rfrList, struct raftFrag *rf)
/* Return last rfr in list that refers to rf. */
{
struct raftFragRef *rfr, *hit = NULL;

for (rfr = rfrList; rfr != NULL; rfr = rfr->next)
    {
    if (rfr->rf == rf) 
	hit = rfr;
    }
assert(hit != NULL);
return hit;
}

struct raftFragRef *cleanRfrList(struct raftFragRef *oldList)
/* If a fragment is completely enclosed by another fragment remove
 * it from the list. */
{
struct raftFragRef *rfr, *next;
struct raftFragRef *lastRef, *badRef, *badNext;
struct raftFragRef *newList = NULL;

for (rfr = oldList; rfr != NULL; rfr = next)
    {
    lastRef = rfrLastRef(rfr, rfr->rf);
    for (badRef = rfr; badRef != lastRef; badRef = badNext)
	{
	fprintf(logFile, "Cleaning out %s\n", badRef->rf->frag->name);
	badNext = badRef->next;
	freeMem(badRef);
	}
    next = lastRef->next;
    slAddHead(&newList, lastRef);
    }
slReverse(&newList);
return newList;
}

struct goldenPath *gpFromRf(struct raftFrag *rf)
/* Construct a "default" golden path element that goes entirely
 * through fragment. */
{
struct oogFrag *frag = rf->frag;
struct goldenPath *gp;
AllocVar(gp);
gp->frag = frag;
gp->fragStart = 0;
gp->fragEnd = frag->size;
gp->orientation = rf->orientation;
return gp;
}

struct oogEdge *findSimpleConnectingEdge(struct oogFrag *a, 
	struct oogFrag *b, struct raft *raft)
/* Find best connecting edge between a and b. */
{
struct oogEdge *edge;
struct edgeRef *er;
int bestScore = -0x3fffffff;
struct oogEdge *bestEdge = NULL;

fprintf(logFile, "findSimpleConnectingEdge %s %s in raft %d (%d edges)\n",
	a->name, b->name, raft->id, slCount(raft->refList));
for (er = raft->refList; er != NULL; er = er->next)
    {
    edge = er->edge;
    if ( (edge->aSide == a && edge->bSide == b) || (edge->aSide == b && edge->bSide == a))
	{
	if (edge->score > bestScore)
	    {
	    bestScore = edge->score;
	    bestEdge = edge;
	    }
	}
    }
return bestEdge;
}


struct goldenPath *pathThroughRfrList(struct raftFragRef *rfrList, 
	struct raft *raft)
/* Find a path that connects fragments on rfrList.  */
{
struct goldenPath *lastGp = NULL, *gp, *gpList = NULL;
struct raftFragRef *rfr, *nextRfr;
struct oogEdge *edge;
int startOff = 0;

rfr = rfrList;
rfr->gp = gpList = lastGp = gpFromRf(rfr->rf);
if (rfr->next == NULL)
    return gpList;	/* Easy case. */

fprintf(logFile, "pathThroughRfrList got %d frags through raft %d starting %s\n", 
	slCount(rfrList), raft->id, lastGp->frag->name);
fprintf(logFile, "  %s\n", rfr->rf->frag->name);
for (rfr = rfrList->next; rfr != NULL; rfr = rfr->next)
    {
    fprintf(logFile, "  %s\n", rfr->rf->frag->name);
    if ((edge = findSimpleConnectingEdge(lastGp->frag, rfr->rf->frag, raft)) != NULL)
	{
	rfr->gp = gp = gpFromRf(rfr->rf);
	findCrossover(edge, lastGp->frag, gp->frag, lastGp, gp, rfr->crossAt);
	fprintf(logFile, "  lastGp (%s %d %d of %d) gp (%s %d %d of %d)\n", 
		lastGp->frag->name, lastGp->fragStart, lastGp->fragEnd, lastGp->frag->size,
		gp->frag->name, gp->fragStart, gp->fragEnd, gp->frag->size);
	slAddHead(&gpList, gp);
	lastGp = gp;
	}
    else
	{
	slFreeList(&gpList);
	return NULL;
	}
    }
slReverse(&gpList);
for (gp = gpList; gp != NULL; gp = gp->next)
    dumpGp(gp, logFile);
return gpList;
}

boolean gotReversals(struct goldenPath *gpList)
/* Check golden path for reversals. */
{
struct goldenPath *gp;

for (gp = gpList; gp != NULL; gp = gp->next)
    {
    if (gp->fragEnd <= gp->fragStart)
	{
	return TRUE;
	}
    }
return FALSE;
}


struct goldenPath *fixReversals(struct raftFragRef *rfrList, 
	struct raft *raft)
/* Fix reversals on golden path. */
{
struct goldenPath *gp;
struct raftFragRef *lastRfr = NULL, *rfr, *nextRfr;
boolean extreme;
boolean fixedIt = FALSE;
enum xSide {left = 0, right = 1, both = 2} xSide;

fprintf(logFile, "fixReversals\n");
for (extreme = 0; extreme <= 1; ++extreme)
    {
    enum crossAt crossAt = (extreme ? crossMiddle : crossExtreme);
    for (xSide = left; xSide <= both; ++xSide)
	{
	for (rfr = rfrList; rfr != NULL; rfr = rfr->next)
	    rfr->crossAt = crossUsual;
	for (rfr = rfrList; rfr != NULL; rfr = nextRfr)
	    {
	    nextRfr = rfr->next;
	    gp = rfr->gp;
	    if (gp->fragEnd <= gp->fragStart)
		{
		if (xSide == left || xSide == both)
		    rfr->crossAt = crossAt;
		if (nextRfr != NULL && (xSide == right || xSide == both))
		    nextRfr->crossAt = crossAt;
		}
	    lastRfr = rfr;
	    }
	gp = pathThroughRfrList(rfrList, raft);
	if (gp != NULL && !gotReversals(gp))
	    {
	    fixedIt = TRUE;
	    break;
	    }
	}
    if (fixedIt)
	break;
    fprintf(logFile, "Taking extreme measures to prevent reversals\n");
    }
if (gp == NULL)
    errAbort("Couldn't fix reversals", raft->id);
return gp;
}

struct goldenPath *hardPathThroughRaft(struct raft *raft)
/* We can't find a raft the easy way, so we'll make up a graph
 * and hope to find *some* way through the raft. */
{
struct diGraph *dg = dgNew();
struct dgNode *startNode, *endNode, *node, *aNode, *bNode;
struct dgNodeRef *nodeRefList, *nodeRef;
struct raftFragRef *rfrList = NULL, *rfr;
int startPos = 0x3fffffff;
int endPos = -startPos;
struct raftFrag *rf;
int s, e;
struct edgeRef *er;
struct oogEdge *edge;
struct goldenPath *gp;

fprintf(logFile, "taking hardPathThroughRaft %d\n", raft->id);
for (rf = raft->fragList; rf != NULL; rf = rf->next)
    {
    s = rf->raftOffset;
    e = s + rf->frag->size;
    node = dgAddNode(dg, rf->frag->name, rf);
    if (s < startPos)
	{
	startPos = s;
	startNode = node;
	}
    if (e > endPos)
	{
	endPos = e;
	endNode = node;
	}
    }
for (er = raft->refList; er != NULL; er = er->next)
    {
    edge = er->edge;
    aNode = dgFindNode(dg, edge->aSide->name);
    bNode = dgFindNode(dg, edge->bSide->name);
    dgConnect(dg, aNode, bNode);
    dgConnect(dg, bNode, aNode);
    }
if ((nodeRefList = dgFindPath(dg, startNode, endNode)) == NULL)
    {
    errAbort("Can't find path from %s to %s in raft %d", startNode->name,
	endNode->name, raft->id);
    }
for (nodeRef = nodeRefList; nodeRef != NULL; nodeRef = nodeRef->next)
    {
    fprintf(logFile, "  %s\n", nodeRef->node->name);
    AllocVar(rfr);
    rfr->rf = nodeRef->node->val;
    slAddHead(&rfrList, rfr);
    }
slReverse(&rfrList);
gp = pathThroughRfrList(rfrList, raft);
if (gp != NULL)
    {
    if (gotReversals(gp))
	{
	gp = fixReversals(rfrList, raft);
	}
    }
else
    {
    errAbort("Curiously couldn't find hard way through raft %d", raft->id);
    }
slFreeList(&rfrList);
slFreeList(&nodeRefList);
dgFree(&dg);
return gp;
}

void removeReversals(struct goldenPath **pGpList)
/* As a last resort - simply remove the reversals. */
{
struct goldenPath *newList = NULL, *gp, *next;

for (gp = *pGpList; gp != NULL; gp = next)
    {
    next = gp->next;
    if (gp->fragEnd > gp->fragStart)
	{
	slAddHead(&newList, gp);
	}
    else
	{
	warn("Reversal *still* in golden path %d %d at %s removing....", 
	    gp->fragStart, gp->fragEnd, gp->frag->name);
	}
    }
slReverse(&newList);
*pGpList = newList;
}

void checkGp(struct goldenPath **pGpList)
/* Check golden path for reversals. */
{
struct goldenPath *gp;
bool gotReversals = FALSE;

for (gp = *pGpList; gp != NULL; gp = gp->next)
    {
    if (gp->fragEnd <= gp->fragStart)
	{
	gotReversals = TRUE;
	}
    }
removeReversals(pGpList);
}


struct goldenPath *pathThroughRaft(struct raft *raft, 
	struct hash *fragHash)
/* Return the best way through raft given orientation. */
{
int raftSize;
struct raftFrag **bestFrag, *rf;
int *bestScore;
struct raftFragRef *rfrList = NULL, *rfr;
struct goldenPath *gp;

fprintf(logFile, "pathThroughRaft %d at %d\n", raft->id, raft->defaultPos);
if (raft->orientation < 0)
    {
    flipRaftFrags(raft);
    raft->orientation = 1;
    }
if (raft->start != 0 || raft->end <= 0)
    {
    errAbort("unnormalized raft!");
    normalizeRaft(raft);
    }
raftSize = raft->end;
AllocArray(bestFrag, raftSize);
AllocArray(bestScore, raftSize);

for (rf = raft->fragList; rf != NULL; rf = rf->next)
    {
    struct oogFrag *frag = rf->frag;
    int raftStart = rf->raftOffset;
    int fragSize = frag->size;
    int raftEnd = raftStart + fragSize;
    int score = fragSeqQual(frag);
    int lScore;
    int start, end;
    int i;
    start = raftStart;
    end = raftEnd;
    assert(end <= raftSize);
    for (i=start; i<end; ++i)
	{
	if (bestScore[i] < score)
	    {
	    bestFrag[i] = rf;
	    bestScore[i] = score;
	    }
	}
    }
rfrList = makeRfrList(bestFrag, raftSize);
rfrList = cleanRfrList(rfrList);
freeMem(bestScore);
freeMem(bestFrag);
gp = pathThroughRfrList(rfrList, raft);
if (gp == NULL)
    gp = hardPathThroughRaft(raft);
else
    {
    if (gotReversals(gp))
	gp = fixReversals(rfrList, raft);
    }
checkGp(&gp);
return gp;
}

int goldenPathSize(struct goldenPath *gpList)
/* Return number of bases in golden path. */
{
int total = 0;
struct goldenPath *gp;
for (gp = gpList; gp != NULL; gp = gp->next)
    {
    total += gp->fragEnd - gp->fragStart;
    }
return total;
}


void saveGoldenPath(char *fileName, char *contigName, struct goldenPath *goldenPath)
/* Save out golden path to file. */
{
FILE *f = mustOpen(fileName, "w");
struct goldenPath *gp;
int cStart = 0;
int fragSize;
int ix = 0;
struct oogFrag *frag;

for (gp = goldenPath; gp != NULL; gp = gp->next)
    {
    fragSize = gp->fragEnd - gp->fragStart;
    fprintf(f, "%s\t", contigName);
    fprintf(f, "%d\t%d\t", cStart+1, cStart+fragSize);
    fprintf(f, "%d\t", ++ix);
    if ((frag = gp->frag) != NULL)
	{
	fprintf(f, "%s\t%s\t%d\t%d\t%c\n", "O", frag->name, 
		gp->fragStart+1, gp->fragEnd,
		(gp->orientation > 0 ? '+' : '-'));
	}
    else
	{
	fprintf(f, "N\t%d\t%s\t%s\n", gp->fragEnd,
		gapTypeNames[gp->gapType],
		bridgeTypeNames[gp->bridgeType]);
	}
    cStart += fragSize;
    }
fclose(f);
}

int faLineSize = 50;
static int faLinePos = 0;
int faTotalSize = 0;

int faCharOut(FILE *f, char c)
/* Put out character to FA file, adding CR if necessary. */
{
fputc(toupper(c), f);
++faTotalSize;
if (++faLinePos >= faLineSize)
    {
    fputc('\n', f);
    faLinePos = 0;
    }
}

void makeGoldenFa(char *faOutName, char *ctgName, struct goldenPath *goldenPath)
/* Save out fa with golden path. */
{
FILE *f;
struct goldenPath *gp;

faLinePos = faTotalSize = 0;
f = mustOpen(faOutName, "w");
fprintf(f, ">%s\n", ctgName);
for (gp = goldenPath; gp != NULL; gp = gp->next)
    {
    struct oogFrag *frag = gp->frag;
    int size = gp->fragEnd - gp->fragStart;
    int i;
    if (frag != NULL)
	{
	struct dnaSeq *seq = frag->seq;
	DNA *dna = seq->dna + gp->fragStart;
	if (gp->orientation < 0)
	    reverseComplement(dna, size);
	for (i=0; i<size; ++i)
	    faCharOut(f, dna[i]);
	if (gp->orientation < 0)
	    reverseComplement(dna, size);
	}
    else
	{
	for (i=0; i<size; ++i)
	    {
	    faCharOut(f, 'N');
	    }
	}
    }
if (faLinePos != 0)
    fputc('\n', f);
fclose(f);
printf("Wrote %d total bases to %s\n", faTotalSize, faOutName);
}

void saveGl(char *glName, struct raftPath *raftPath)
/* Save layout as a .gl file. */
{
struct raftPath *rp;
int raftOffset;
struct raft *raft;
struct raftFrag *rf;
struct oogFrag *frag;
FILE *f = mustOpen(glName, "w");
int start, end;

for (rp = raftPath; rp != NULL; rp = rp->next)
    {
    raft = rp->raft;
    raftOffset = rp->start;
    slSort(&raft->fragList, raftFragCmpOffset);
    for (rf = raft->fragList; rf != NULL; rf = rf->next)
	{
	frag = rf->frag;
	start = raftOffset + rf->raftOffset;
	end = start + frag->size;
	fprintf(f, "%s %d %d %c\n",
	    frag->name, start, end,
	    (rf->orientation >= 0 ? '+' : '-'));
	}
    }
fclose(f);
}

boolean realConnection(struct dgConnection *conList)
/* Return TRUE if there's a connection with val (not just
 * a dummy link to clone ends). */
{
struct dgConnection *con;
for (con = conList; con != NULL; con = con->next)
    {
    if (con->node->val != NULL)
	return TRUE;
    }
return FALSE;
}

boolean inGraph(struct diGraph *graph, struct raft *raft)
/* Return TRUE if raft is connected inside graph. */
{
struct dgNode *node;
node = dgFindNumberedNode(graph, raft->id);
return realConnection(node->nextList) || realConnection(node->prevList);
}

void saveLayout(char *glName, struct raftPath *raftPath, struct diGraph *graph)
/* Save layout as a gl.N file. */
{
struct raftPath *rp;
int raftOffset;
struct raft *raft;
struct raftFrag *rf;
struct oogFrag *frag;
FILE *f = mustOpen(glName, "w");
int start, end;

for (rp = raftPath; rp != NULL; rp = rp->next)
    {
    raft = rp->raft;
    raftOffset = rp->start;
    slSort(&raft->fragList, raftFragCmpOffset);
    for (rf = raft->fragList; rf != NULL; rf = rf->next)
	{
	frag = rf->frag;
	start = raftOffset + rf->raftOffset;
	end = start + frag->size;
	fprintf(f, "%s\t%d\t%d\t%c\t",
	    frag->name, start, end,
	    (rf->orientation >= 0 ? '+' : '-'));
	if (inGraph(graph, raft))
	    {
	    fprintf(f, "G%d\t", raft->node->component);
	    }
	else
	    {
	    fprintf(f, " \t");
	    }
	if (slCount(raft->fragList) > 1)
	    {
	    fprintf(f, "R%d\n", raft->id);
	    }
	else
	    {
	    fprintf(f, " \n");
	    }
	}
    }
fclose(f);
}

struct goldenPath *gapInGp(struct raftPath *a, struct raftPath *b, struct diGraph *bargeGraph)
/* Make up data structure to describe gap between a and b. */
{
struct goldenPath *gp;
struct raft *aRaft = a->raft, *bRaft = b->raft;
enum gapType gt;
enum bridgeType bt;
int nCount;
struct barge *aBarge = aRaft->barge, *bBarge = bRaft->barge;

if (aBarge == bBarge)
    {
    gt = gtFragment;
    nCount = fragGapSize;
    if (aRaft->node->component == bRaft->node->component)
	bt = btYes;
    else
	bt = btNo;
    }
else
    {
    gt = gtClone;
    if (dgDirectlyFollows(bargeGraph, aBarge->node, bBarge->node))
	{
	bt = btYes;
	nCount = bridgedBargeGapSize;
	}
    else
	{
	bt = btNo;
	nCount = unbridgedBargeGapSize;
	}
    }
AllocVar(gp);
gp->fragEnd = nCount;
gp->gapType = gt;
gp->bridgeType = bt;
return gp;
}

void ooGreedy(char *inDir)
/* Try and do layouts specified in inDir and put results in
 * outFile. */
{
static char mrnaPslName[512];
static char estPslName[512];
static char selfPslName[512];
static char raftPslName[512];
static char oogLogName[512];
static char raftOutName[512];
static char bargeOutName[512];
static char graphOutName[512];
static char infoName[512];
static char genoListName[512];
static char fragMapName[512];
static char raftOrderName[512];
static char goldName[512];
static char faOutName[512];
static char bacPairName[512];
static char bacPslName[512];
static char glName[512];
static char layoutName[512];
struct oogEdge *selfEdgeList = NULL;
struct mrnaLine *mrnaLineList = NULL, *ml;
struct mrnaLine *estLineList = NULL;
struct mrnaLine *bacEndLineList = NULL;
struct oogClone *cloneList = NULL;
struct oogFrag *fragList = NULL;
struct hash *cloneHash = newHash(8);
struct hash *fragHash = newHash(12);
struct raft *raftList, *raft;
struct barge *bargeList, *barge;
struct psl *selfPslList = NULL;
FILE *graphOut; /* File to write graph to. */
FILE *raftOut;  /* File to write raft to. */
int raftIx;
struct diGraph *raftGraph;
struct diGraph *bargeGraph;
int contigTotalSize = 0;
int raftTotalSize = 0;
int contigPos = 0;
int raftCount = 0;
struct dgNodeRef *orderList, *orderEl;
struct goldenPath *goldenPath = NULL, *gp;
struct raftPath *raftPath = NULL, *rp;
char *ctgName;

sprintf(selfPslName, "%s/self.psl", inDir);
sprintf(mrnaPslName, "%s/mrna.psl", inDir);
sprintf(estPslName, "%s/est.psl", inDir);
sprintf(raftPslName, "%s/raft.psl", inDir);
sprintf(oogLogName, "%s/ooGreedy.log", inDir, version);
sprintf(raftOutName, "%s/raft.%d", inDir, version);
sprintf(bargeOutName, "%s/barge.%d", inDir, version);
sprintf(graphOutName, "%s/graph.%d", inDir, version);
sprintf(infoName, "%s/info", inDir);
sprintf(fragMapName, "%s/fragMap.%d", inDir, version);
sprintf(raftOrderName, "%s/raftOrder.%d", inDir, version);
sprintf(genoListName, "%s/geno.lst", inDir);
sprintf(goldName, "%s/gold.%d", inDir, version);
sprintf(bacPairName, "%s/bacEndPairs", inDir);
sprintf(bacPslName, "%s/bacEnd.psl", inDir);
sprintf(glName, "%s/ooGreedy.%d.gl", inDir, version);
sprintf(layoutName, "%s/gl.%d", inDir, version);
sprintf(ocpFileName, "%s/ocp.%d", inDir, version);

/* Open log file file. */
logFile = mustOpen(oogLogName, "w");
pushWarnHandler(warnHandler);

fprintf(stdout, "ooGreedy version %d on %s\n", version, inDir);
fprintf(logFile, "ooGreedy version %d on %s\n", version, inDir);

/* Load in DNA and make clone and fragment lists. */
contigTotalSize =  loadAllClones(infoName, genoListName, fragHash, 
	cloneHash, &fragList, &cloneList, &ctgName);
sprintf(faOutName, "%s/%s.fa", inDir, ctgName);
fprintf(stdout, "Got %d clones %d frags ~%d contig size\n", 
	slCount(cloneList), slCount(fragList), contigTotalSize);
fprintf(logFile, "Got %d clones %d frags ~%d contig size\n", 
	slCount(cloneList), slCount(fragList), contigTotalSize);

/* Load up self data structures and sort edge list. */
processSelf(selfPslName, &cloneList, &fragList, &selfEdgeList, &selfPslList, cloneHash, fragHash);
fprintf(stdout, "Found %d fragment overlaps in self.psl\n", slCount(selfEdgeList));
fprintf(logFile, "Found %d fragment overlaps in self.psl\n", slCount(selfEdgeList));


/* Make rafts. */
raftOut = mustOpen(raftOutName, "w");
raftPsl = mustOpen(raftPslName, "w");
pslWriteHead(raftPsl);
slSort(&selfEdgeList, oogEdgeCmp);
raftList = makeRafts(fragList, selfEdgeList);
fprintf(stdout, "found %d rafts (%d of %d frags) in %s\n", 
	slCount(raftList), raftedFragCount(fragList),
	slCount(fragList), selfPslName);
fprintf(logFile, "found %d rafts (%d of %d frags) in %s\n", 
	slCount(raftList), raftedFragCount(fragList),
	slCount(fragList), selfPslName);
fprintf(raftOut, "ooGreedy version %d found %d rafts (%d of %d frags) in %s\n\n", 
	version, slCount(raftList), raftedFragCount(fragList),
	slCount(fragList), selfPslName);
makeSingletonRafts(fragList, &raftList);

bargeList = makeBarges(cloneList, raftList);
fprintf(stdout, " %d barges in %d clones\n", slCount(bargeList), slCount(cloneList));

/* Read in rna edge lists.  Merge est edges into rna ones. */
    {
    int mCount, eCount, bCount;
    mrnaLineList = makeMrnaLines(mrnaPslName, fragHash);
    mCount = slCount(mrnaLineList);
    estLineList = makeMrnaLines(estPslName, fragHash);
    eCount = slCount(estLineList);
    mrnaLineList = slCat(mrnaLineList, estLineList);
    estLineList = NULL;
    bacEndLineList = makeBacEndLines(bacPairName, bacPslName, fragHash);
    bCount = slCount(bacEndLineList);
    mrnaLineList = slCat(mrnaLineList, bacEndLineList);
    bacEndLineList = NULL;
    slSort(&mrnaLineList, mrnaLineCmp);
    fprintf(stdout, "Got %d mRNA, %d EST, and %d BAC end pair bridges\n",
	mCount, eCount, bCount);
    fprintf(logFile, "Got %d mRNA, %d EST, and %d BAC end pair bridges\n",
	mCount, eCount, bCount);
    }


fprintf(logFile, "\nList of mrnaLines:\n");
for (ml = mrnaLineList; ml != NULL; ml = ml->next)
    dumpMrnaLine(ml, logFile);
makeGraphs(raftList, bargeList, mrnaLineList, &raftGraph, &bargeGraph);
printf("Made bridge graphs\n");

calcPositions(bargeList, fragList, bargeGraph);
setRaftDefaultPositions(raftList);
slSort(&fragList, cmpFragDefaultPos);
writeFragMap(fragMapName, fragList);
saveBarges(bargeOutName, bargeList, bargeGraph, raftList);
for (raft = raftList; raft != NULL; raft = raft->next)
    {
    normalizeRaft(raft);
    dumpRaft(raft, raftOut);
    fprintf(raftOut, "\n");
    }
carefulClose(&raftOut);
printf("Updated default positions\n");


flipNearDefaults(raftGraph);
graphOut = mustOpen(graphOutName, "w");
fprintf(graphOut, "mRNA/BAC ends graph by ooGreedy version %d\n\n", version); 
dgConnectedComponentsWithVals(raftGraph);
ooDumpGraph(raftGraph, graphOut);
carefulClose(&graphOut);
fflush(logFile);
printf("Flipped and saved graphs.\n");

orderList = orderRafts(raftGraph);
printf("Ordered rafts\n");

	{
	int count = 0;
	fprintf(logFile, "Order of rafts:\n");
	fprintf(logFile, "\t");
	for (orderEl = orderList; orderEl != NULL; orderEl = orderEl->next)
	    {
	    raft = orderEl->node->val;
	    if (raft != NULL)
		{
		fprintf(logFile, "%d ", raft->id);
		if (++count % 10 == 0)
		    fprintf(logFile,"\n\t");
		}
	    }
	fprintf(logFile, "\n");
	}

/* Find path through each raft. */
for (orderEl = orderList; orderEl != NULL; orderEl = orderEl->next)
    {
    raft = orderEl->node->val;
    if (raft != NULL)
	{
	AllocVar(rp);
	rp->gpList = pathThroughRaft(raft, fragHash);
	rp->raft = raft;
	rp->size = goldenPathSize(rp->gpList);
	raftTotalSize += rp->size;
	++raftCount;
	slAddHead(&raftPath, rp);
	}
    }
slReverse(&raftPath);


/* Merge together raft paths into full path */
for (rp = raftPath,raftIx = 0; rp != NULL; rp = rp->next, ++raftIx)
    {
    goldenPath = slCat(goldenPath, rp->gpList);
    rp->gpList = NULL;
    rp->start = contigPos;
    contigPos += rp->size;
    if (rp->next != NULL)
	{
	gp = gapInGp(rp, rp->next, bargeGraph);
	contigPos += gp->fragEnd;
	slAddTail(&goldenPath, gp);
	}
    }
printf("Found golden path\n");

saveGoldenPath(goldName, ctgName, goldenPath);
makeGoldenFa(faOutName, ctgName, goldenPath);
saveGl(glName, raftPath);
saveLayout(layoutName, raftPath, raftGraph);
printf("\n", faOutName);
}

#ifdef TESTING 
void testGraph()
{
struct dgNode *a, *b, *c, *d, *e;
struct dgNode *A, *B, *C, *D;
struct diGraph *graph = dgNew();
struct dgNodeRef *refList, *ref;
struct dgEdgeRef *erList, *er;

a = dgAddNode(graph, "a", NULL);
b = dgAddNode(graph, "b", NULL);
c = dgAddNode(graph, "c", NULL);
d = dgAddNode(graph, "d", NULL);
e = dgAddNode(graph, "e", NULL);

dgConnect(graph, a, b);
dgConnect(graph, b, c);
dgConnect(graph, c, d);
dgConnect(graph, d, e);

A = dgAddNode(graph, "A", "A");
dgConnect(graph, a, A);
dgConnect(graph, A, b);
B = dgAddNode(graph, "B", "B");
dgConnect(graph, b, B);
dgConnect(graph, B, c);
C = dgAddNode(graph, "C", "C");
dgConnect(graph, c, C);
dgConnect(graph, C, d);
D = dgAddNode(graph, "D", "D");
dgConnect(graph, d, D);
dgConnect(graph, D, e);

dgConnect(graph, A, B);
dgConnect(graph, C, D);

slReverse(&graph->nodeList);
dgDumpGraph(graph, uglyOut, TRUE);
printf("\n");


refList = dgFindNewConnectedWithVals(graph, D);
for (ref = refList; ref != NULL; ref = ref->next)
    printf("%s ", ref->node->name);
printf("\n");
printf("\n");

erList = dgFindSubEdges(graph, refList);
dgSwapEdges(graph, erList);
dgDumpGraph(graph, uglyOut, TRUE);

dgFree(&graph);
}
#endif /* TESTING */

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
ooGreedy(argv[1]);
#ifdef SOMETIMES
ooGreedy("easy");
#endif
return 0;
}
