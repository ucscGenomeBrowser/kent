/* ooGreedy - A greedy method for merging and doing ordering and orienting
 * of draft contigs. */

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


int version = 102;       /* Current version number. */

int minFragSize = 1;            /* Minimum size of fragments we'll accept. */
int maxMapDeviation = 600000;   /* No map deviations further than this allowed. */
int freeMapDeviation = 250000;  /* Map deviations to this point free. */

int maxTailSize = 2000;	/* Maximum non-aligning end. */
int minScore = -500;    /* Don't bother gluing past this. */
int minBacScore = -720; /* Bac ends can be a little worse... */

int bridgedBargeGapSize = 50000;	/* # of N's between connected barges. */
int unbridgedBargeGapSize = 100000;     /* # of N's between disconnected barges. */
int fragGapSize = 100;                  /* # of N's between frags within a barge. */



FILE *logFile;	/* File to write decision steps to. */
FILE *raftPsl = NULL;  /* File to write consistent .psl to. */

void usage()
/* Explain usage and exit. */
{
errAbort(
   "ooGreedy - ordering and orienting of unfinished contigs using\n"
   "a relatively simple, but hopefully effective greedy method.\n"
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
    int flipTendency;	       /* Tendency of clone to flip according to map. */
    enum htgPhase phase;       /* HTG phase. */
    int size;                  /* Size in bases. */
    struct dlList *fragList;   /* List of frags in clone. */
        /* Fields up to here set by loadAllClones. Rest filled in later. */
    int defaultPos;            /* Relative position in barge or contig in bases. */
    int orientation;	       /* Orientation from ends: +1, -1 or 0 for unknown. */
    struct barge *barge;       /* Barge this is part of. */
    struct bargeEl *bel;       /* Point back to barge el to get position... */
    struct dgNode *startNode;  /* Start node in raft graph. */
    struct dgNode *endNode;    /* End node in raft graph. */
    bool hitFlag;              /* Used by ordCountStarts. */
    int maxShare;              /* Max amount of sequence shared */
    struct oogClone *maxShareClone;  /* Clone that shares max sequence. */
    };


struct oogFrag
/* A fragment of a clone. */
    {
    struct oogFrag *next;	/* Next in frag list. */
    char *name;			/* Name of frag. */
    int size;                   /* Size in bases. */
    struct oogClone *clone;     /* Clone it's part of. */
    struct qaSeq *seq;          /* Sequence. */
    int startGood, endGood;     /* Range of sequence with good quality scores. */
    struct raft *raft;		/* Raft fragment is in. */
    int clonePos;               /* Position within clone. (Assuming frags back to back.) */
    int defaultPos;             /* Default position in contig. */
    };

struct oogEdge
/* An edge connecting two fragments in a raft. */
    {
    struct oogEdge *next;      /* Next in master edge list. */
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
    bool used;                 /* True if hard - nothing can come between. */
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
    bool fixedOrientation;       /* True if raft has fixed orientation. */
    int defaultPos;		 /* Default Middle position of raft in contig coordinates. */
    int flipTendency;            /* Which orientation map says raft should have. */
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

struct cloneEnd
/* Info on a clone end. */
    {
    struct cloneEnd *next;	/* Next in list. */
    struct oogClone *clone;	/* Clone this is part of. */
    boolean isStart;		/* True if start end. */
    int orientation;		/* +1, -1 or 0 if unknown. */
    };

struct barge
/* A barge - a set of welded together clones. */
    {
    struct barge *next;	    /* Next in list. */
    int id;                 /* Barge id. */
    struct bargeEl *cloneList;  /* List of clones. */
    struct cloneEnd *endList;	/* List of clone ends. */
    struct oogEdge *selfEdges;  /* List of edges inside barge. */
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
    int abOverlap;	    /* Overlap between this and next clone. */
    int axOverlap;          /* Overlap between this and clone we're positioning (temp) */
    struct bargeEl *enclosed;  /* Clone enclosed by another?. */
    };

struct splitClone
/* Info on a split finished clone.  (A special case of a special case -
 * one of about 70 'finished' clones that seems to have a contaminant in
 * the middle, and hence got split by Greg into 2 or more fragments.) */
    {
    struct splitClone *next;	/* Next in list. */
    char *name;			/* Name of this clone. Not allocated here. */
    struct splitFrag *fragList; /* List of fragments. */
    };

struct splitFrag 
/* Info on a single frag of a split clone. */
    {
    struct splitFrag *next;	/* Next in list. */
    char *name;			/* Name of this frag. Not allocated here. */
    struct splitClone *clone;	/* Clone this is associated with. */
    int start;			/* Start position in submission coordinates. */
    int end;			/* End position in submission coordinates. */
    };

enum cableType 
/* Describes a single connection between rafts. */
   {
   ctReadPair,       /* A pair of reads, probably from a plasmid size selected insert. */
   ctMrna,           /* (full length) mRNA. */
   ctEst,            /* Single EST. */
   ctEstPair,        /* Pair of ESTs. */
   ctBacEndPair,     /* Pair of BAC ends. */
   ctCloneEnds, /* (dummy) start/end of clone. */
   ctChain,	     /* Chain in clone from sequencing center submission. */
   ctBigRaft,	     /* Big raft with orientation info from map. */
   };

char *cableTypeName(int cableType)
/* Given cable type return name. */
{
static char *names[] = 
   {"readPair", "mRNA", "EST", "estPair", "bacEndPair", "cloneEnds", 
   	"chain", "bigRaft"};
assert(cableType >= 0 && cableType < ArraySize(names));
return names[cableType];
}

struct cable
/* Part of a bridge - a single connection that joins two rafts. 
 * This is attatched to the directed graph that connects rafts. */
   {
   struct cable *next;
   int minDistance;     /* Minimum distance between. */
   int maxDistance;     /* Maximum distance between. */
   char *aName;		/* Name of first of joining pair.  Not allocated here. */
   char *bName;         /* Name of second of joining pair.  Not allocated here. */
   signed char aOrientation;   /* Orientation of aRaft. */
   signed char bOrientation;   /* Orientation of bRaft. */
   UBYTE cableType;     /* Type of connection. */
   };

struct bridge
/* A bridge between two rafts.  Part of the directed graph between rafts
 * made by paired reads,  mRNAs, BAC end pairs, and ESTs.  This bridge is 
 * held together by "cables". */
    {
    struct bridge *next;	/* Next in list. */
    int minDistance;            /* Minimum allowed distance of bridge. */
    int maxDistance;            /* Maximum allowed distance of bridge. */
    struct cable *cableList;    /* List of cables making up bridge. */
    };

struct lineConnection
/* An mRNA/EST/BAC end pair/read pair induced link between two rafts. 
 * This connection is scored and only tentative. Permanant connections
 * become cable and bridges. */
    {
    struct lineConnection *next; /* Next in list. */
    char *name;                 /* Not allocated here. */
    struct raft *raft;		/* Raft linked. */
    int position;	        /* Position of middle of underlying alignment in raft. */
    int score;                  /* Score of this connection. */
    signed char orientation;    /* Relative orientation of raft to line graph. */
    bool bestForRaft;           /* Set to TRUE if best for raft. */
    bool raftTested;            /* Set to TRUE if raft tested for best. */
    };

struct lineGraph
/* A linear graph with a score describing connections between
 * rafts induced by a single mRNA/read pair/etc.  This is scored
 * and tentative.  LineGraphs are added one at a time to the master
 * directed graph, and rejected if they cause inconsistencies in this
 * graph.  */
    {
    struct lineGraph *next;         /* Next in list. */
    struct lineConnection *mcList;  /* Raft connections. */
    int score;                      /* Overall score. */
    enum cableType cableType;       /* Type of graph. */
    int minDistance;                /* Minimum allowed distance of bridge. */
    int maxDistance;                /* Maximum allowed distance of bridge. */
    };

struct lineConnectionRef
/* Reference to an lineConnection. */
    {
    struct lineConnectionRef *next;     /* next in list. */
    struct lineConnection *mc;	        /* lineConnection */
    };

struct connectedComponent
/* A list of connected components. This is a temporary structure
 * that just lives while an lineGraph is being added to an mrna graph. */
    {
    struct connectedComponent *next;	/* Next connected component. */
    struct dgNodeRef *subGraph;         /* List of graph nodes in component. */
    struct lineConnectionRef *subLine;  /* List of mrna nodes in component. */
    short orientation;                  /* +1 or -1 */
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

void logIt(char *format, ...)
/* Print message to log file. */
{
va_list args;
va_start(args, format);
vfprintf(logFile, format, args);
va_end(args);
}

void *findHashValOrDie(struct hash *hash, char *name, char *lastWords)
/* Find name in hash and return associated value or print last words
 * and die. */
{
void *val = hashFindVal(hash, name);
if (val == NULL)
    errAbort("Couldn't find %s in hash.  %s", name, lastWords);
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

boolean inPairHash(struct hash *pairHash, char *a, char *b)
/* Return TRUE if a/b are in a pair hash. */
{
return (hashLookup(pairHash, ocpHashName(a, b)) != NULL);
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

char contaminatedName[512]; /* Where to write clones that appear to have mixed plate. */
FILE *contaminatedFile = NULL; /* File corresponding to contaminatedName. */

void recordContamination(char *name, char *type)
/* Record contamination if any. */
{
if (contaminatedFile == NULL) 
    contaminatedFile = mustOpen(contaminatedName, "w");
fprintf(contaminatedFile, "%s\t%s\n", name, type);
}

int maxMapDif(struct oogClone *a, struct oogClone *b)
/* Return max distance between clones in map coordinates. */
{
int fDif, rDif;
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

struct splitClone *readSplitFin(char *fileName, 
	struct hash *splitCloneHash, struct hash *splitFragHash)
/* Read file of split finished clones into list and hashes. */
{
struct splitClone *cloneList = NULL, *clone = NULL;
struct splitFrag *frag;
struct hashEl *hel;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[10];
int wordCount;
char *acc, *lastAcc = "";

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    lineFileExpectWords(lf, 4, wordCount);
    acc = words[1];
    if (!sameString(acc, lastAcc))
        {
	AllocVar(clone);
	hel = hashAddUnique(splitCloneHash, acc, clone);
	clone->name = hel->name;
	slAddHead(&cloneList, clone);
	lastAcc = acc;
	}
    AllocVar(frag);
    hel = hashAddUnique(splitFragHash, words[0], frag);
    frag->name = hel->name;
    frag->clone = clone;
    frag->start = atoi(words[2]);
    frag->end = atoi(words[3]);
    slAddTail(&clone->fragList, frag);
    }
lineFileClose(&lf);
slReverse(&cloneList);
return cloneList;
}


void reverseUnsignedArray(unsigned *a, long length)
/* Reverse an array of unsigned number. */
{
long halfLen = (length>>1);
unsigned *end = a+length;
unsigned c;
while (--halfLen >= 0)
    {
    c = *a;
    *a++ = *--end;
    *end = c;
    }
}


int pslCmpQueryTargetDiag(const void *va, const void *vb)
/* Compare to sort based on query, target, diagonal position. */
{
const struct psl *a = *((struct psl **)va);
const struct psl *b = *((struct psl **)vb);
int dif;
dif = strcmp(a->qName, b->qName);
if (dif == 0)
    dif = strcmp(a->strand, b->strand);
if (dif == 0)
    dif = strcmp(a->tName, b->tName);
if (dif == 0)
    {
    int aDiag;
    int bDiag;
    if (a->strand[0] == '+')
	{
	aDiag = a->qStart - a->tStart;
	bDiag = b->qStart - b->tStart;
	}
    else
        {
	aDiag = (a->qSize - a->qEnd) - a->tStart;
	bDiag = (b->qSize - b->qEnd) - b->tStart;
	}
    dif = aDiag - bDiag;
    }
return dif;
}

int pslCmpTargetQueryDiag(const void *va, const void *vb)
/* Compare to sort based on target, query, diagonal position. */
{
const struct psl *a = *((struct psl **)va);
const struct psl *b = *((struct psl **)vb);
int dif;
dif = strcmp(a->tName, b->tName);
if (dif == 0)
    dif = strcmp(a->strand, b->strand);
if (dif == 0)
    dif = strcmp(a->qName, b->qName);
if (dif == 0)
    {
    int aDiag;
    int bDiag;
    if (a->strand[0] == '+')
	{
	aDiag = a->qStart - a->tStart;
	bDiag = b->qStart - b->tStart;
	}
    else
        {
	aDiag = (a->qSize - a->qEnd) - a->tStart;
	bDiag = (b->qSize - b->qEnd) - b->tStart;
	}
    dif = aDiag - bDiag;
    }
return dif;
}


void splitPslOnPrefix(struct psl **pInList, char *prefix, boolean doQ,
     struct psl **retMatches, struct psl **retMismatches)
/* Split inList into those whose sequence name (qName or tName depending
 * on doQ) starts with prefix, and those whose doesn't.  Eats inList. */
{
struct psl *psl, *nextPsl, *matchList = NULL, *mismatchList = NULL;
char *name;
int totalCount = 0, matchCount = 0, mismatchCount = 0;

for (psl = *pInList; psl != NULL; psl = nextPsl)
    {
    ++totalCount;
    nextPsl = psl->next;
    name = (doQ ? psl->qName : psl->tName);
    if (startsWith(prefix, name))
        {
	++matchCount;
	slAddHead(&matchList, psl);
	}
    else
        {
	++mismatchCount;
	slAddHead(&mismatchList, psl);
	}
    }
slReverse(&matchList);
*retMatches = matchList;
slReverse(&mismatchList);
*retMismatches = mismatchList;
*pInList = NULL;
}


void raftFree(struct raft **pRaft)
/* Free up memory associated with a raft. */
{
struct raft *raft;
if ((raft = *pRaft) != NULL)
    {
    slFreeList(&raft->fragList);
    freez(pRaft);
    }
}

void raftFreeList(struct raft **pList)
/* Free up a list of rafts. */
{
struct raft *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    raftFree(&el);
    }
*pList = NULL;
}

void unlinkFragsFromRafts(struct oogFrag *fragList)
/* NULL out frag->raft for all in list. */
{
struct oogFrag *frag;
for (frag = fragList; frag != NULL; frag = frag->next)
    frag->raft = NULL;
}


int cmpFragDefaultPos(const void *va, const void *vb)
/* Compare two oogFrags to sort by default position . */
{
const struct oogFrag *a = *((struct oogFrag **)va);
const struct oogFrag *b = *((struct oogFrag **)vb);
return a->defaultPos - b->defaultPos;
}

enum specialRafts
/* ID's of special rafts. */
   {
   srOriented = 0  /* This dummy helps us not flip things. */
   };

int nextRaftId()
/* Return next available id for raft */
{
static int id = 0;
return ++id;
}

struct raft *makeFixedRaft()
/* Return a new raft containing just one fragment. */
{
struct raft *raft;
AllocVar(raft);
raft->id = srOriented;
raft->orientation = 1;
return raft;
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

boolean fragInSomeRaft(struct raft *raftList, 
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


void addCable(struct diGraph *graph, struct dgNode *aNode, char *aName, int aOrientation,
	struct dgNode *bNode, char *bName, int bOrientation, enum cableType type,
	int minDistance, int maxDistance)
/* Add cable to graph.  Allocate edge and bridge if necessary.  This
 * does *not* check that cable is consistent. */
{
struct dgEdge *edge = dgDirectlyFollows(graph, aNode, bNode);
struct cable *cable;
struct bridge *bridge;

AllocVar(cable);
cable->minDistance = minDistance;
cable->maxDistance = maxDistance;
cable->aName = aName;
cable->bName = bName;
cable->aOrientation = aOrientation;
cable->bOrientation = bOrientation;
cable->cableType = type;
if (edge == NULL)
    {
    AllocVar(bridge);
    bridge->minDistance = minDistance;
    bridge->maxDistance = maxDistance;
    bridge->cableList = cable;
    edge = dgConnectWithVal(graph, aNode, bNode, bridge);
    }
else
    {
    bridge = edge->val;
    if (bridge->minDistance < minDistance)
        bridge->minDistance = minDistance;
    if (bridge->maxDistance > maxDistance)
        bridge->maxDistance = maxDistance;
    assert(bridge->minDistance <= bridge->maxDistance);
    slAddHead(&bridge->cableList, cable);
    }
}

void addDummyCable(struct diGraph *graph, struct dgNode *aNode, struct dgNode *bNode)
/* Add cable that contains no info other than aNode follows bNode. */
{
addCable(graph, aNode, aNode->name, 
    1, bNode, bNode->name, 1, ctCloneEnds, 0, BIGNUM/10);
}

void dumpEdge(struct oogEdge *edge, FILE *out)
/* Print out an edge. */
{
fprintf(out, "edge: %11s %c> %11s %s bad %3d tailA %3d tailB %3d match %5d repMatch %5d score %4d\n", edge->aSide->name,
	((edge->orientation > 0) ? '-' : '~'),
	edge->bSide->name, (edge->aCovers ? "covers " : "extends" ),
	edge->milliBad, edge->tailA, edge->tailB, edge->match, edge->repMatch, edge->score);
}

void dumpLineGraph(struct lineGraph *ml, FILE *f)
/* Print out lineGraph. */
{
struct lineConnection *mc;
fprintf(f, "%s lineGraph: score %d, min %d, max %d, connections %d\n", 
	cableTypeName(ml->cableType), ml->score, 
	ml->minDistance, ml->maxDistance, slCount(ml->mcList));
for (mc = ml->mcList; mc != NULL; mc = mc->next)
    {
    fprintf(f, "  name %s, raft %d %c, pos %d, score %d\n", 
        mc->name, mc->raft->id, 
	(mc->orientation > 0 ? '+' : '-'),
	mc->position, mc->score);
    }
}

int lineGraphCmp(const void *va, const void *vb)
/* Compare to sort based on score - best score first. */
{
const struct lineGraph *a = *((struct lineGraph **)va);
const struct lineGraph *b = *((struct lineGraph **)vb);
return b->score - a->score;
}


int markBestConnections(struct lineConnection *list)
/* Set the best flags for connections that are best for a given raft. 
 * Returns very best score. */
{
struct lineConnection *startEl, *el;
int veryBestScore = -BIGNUM;

startEl = list;
for (;;)
    {
    int bestScore = -BIGNUM;
    struct raft *curRaft;
    struct lineConnection *bestEl;

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


struct lineConnection *cleanMrnaConnectionList(struct lineConnection *oldList)
/* Clean up old list, removing crufty connections.  */
{
struct lineConnection *mc, *next;
struct lineConnection *newList = NULL;
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
score -= 10;
return score;
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

int scoreBacEndPsl(struct psl *psl)
/* Return score for one BAC end oriented psl. */
{
int milliBad;
int score;
int startTail, endTail;

milliBad = pslCalcMilliBad(psl, FALSE);
findPslTails(psl, &startTail, &endTail);
score = 25*log(1+psl->match) + log(1+psl->repMatch) - 20*milliBad - 4*(startTail+endTail);
if (psl->match <= 20)
    score -= (20-psl->match)*25;
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

struct oogFrag *fragForName(struct hash *fragHash, char *name)
/* Return named fragment. */
{
return hashFindVal(fragHash, name);
}

int pslCenterInRaft(struct psl *psl, struct raftFrag *rf)
/* Find out center of alignment in raft coordinates. */
{
int tCenter = (psl->tStart + psl->tEnd)/2;
if (rf->orientation < 0)
    tCenter = rf->frag->size - tCenter;
return tCenter + rf->raftOffset;
}

struct lineGraph *oneRnaLine(struct psl *pslList, struct hash *fragHash,
	enum cableType cableType, int scoreAdjust)
/* Return an ordered list of rafts connected by this mRNA. */
{
struct lineGraph *ml;
struct lineConnection *mc, *mcList = NULL;
struct psl *psl;
struct oogFrag *frag;
struct raft *raft;
struct raftFrag *rf;
int oneScore;
int milliScore;
double conflictTotal;
int pslOrient;
int minScore = BIGNUM;
int totalScore = 0;
int averageScore;
int cleanSize = 0;
int compositeScore;
int qCenter;
char *mrnaName = pslList->qName;

fprintf(logFile, "oneRnaLine (preCleaned):\n");
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    pslOrient = pslOrientation(psl);
    frag = fragForName(fragHash, psl->tName);
    if (frag != NULL)
	{
	AllocVar(mc);
	raft = frag->raft;
	rf = findRaftFrag(raft, frag);
	mc->raft = raft;
	mc->orientation = pslOrient * rf->orientation;
	mc->position = pslCenterInRaft(psl, rf);
	mc->score = scoreMrnaPsl(psl);
	mc->name = mrnaName;
	fprintf(logFile, " %d %c %d %s\n", raft->id,
	    (mc->orientation > 0 ? '+' : '-'),
	    mc->score, psl->tName);
	slAddHead(&mcList, mc);
	}
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
AllocVar(ml);
ml->cableType = cableType;
ml->minDistance = 0;
ml->maxDistance = 500000;	/* Longest intron I've seen is about 600k.  Over 300k very rare. */

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
compositeScore = (averageScore + minScore)/2 + scoreAdjust;
fprintf(logFile, "   %d final connections score %d\n", 
    slCount(mcList), compositeScore);
ml->score = compositeScore;
ml->mcList = mcList;
return ml;
}

struct mrnaPsl
/* psl's associated with one piece of mrna. */
    {
    struct mrnaPsl *next;	/* Next in list. */
    char *mrnaName;             /* Name of mrna. */
    struct psl *pslList;        /* List of .psls. */
    };

boolean filterFrag(struct oogFrag *frag)
/* Return TRUE if passed filter. */
{
struct oogClone *clone = frag->clone;
if (clone->phase == htgFinished)
    return TRUE;
return frag->size >= minFragSize;
}

boolean pslTargetFragFilter(struct psl *psl, struct hash *fragHash)
/* See if target fragment passes filter. */
{
struct oogFrag *frag = fragForName(fragHash, psl->tName);
if (frag == NULL)
    return FALSE;
return filterFrag(frag);
}

boolean pslFragFilter(struct psl *psl, struct hash *fragHash)
/* See if both sides of psl pass filter. */
{
struct oogFrag *frag = fragForName(fragHash, psl->tName);
if (frag == NULL) return FALSE;
if (!filterFrag(frag)) return FALSE;
frag = fragForName(fragHash, psl->qName);
if (frag == NULL) return FALSE;
return filterFrag(frag);
}

struct lineGraph *makeMrnaLines(char *fileName, struct hash *fragHash,
	enum cableType cableType, int scoreAdjust, 
	struct hash *splitFinFragHash)
/* Return list of lines based on an mRNA psl file.  */
{
struct hash *rnaHash = newHash(0);
struct lineGraph *mlList = NULL, *ml;
struct psl *pslList = NULL, *psl, *nextPsl;
struct hashEl *hel;
struct mrnaPsl *mpList = NULL, *mp;

pslList = pslLoadAll(fileName);
for (psl = pslList; psl != NULL; psl = nextPsl)
    {
    nextPsl = psl->next;
    if (pslTargetFragFilter(psl, fragHash))
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
    else
        pslFree(&psl);
    }
slReverse(&mpList);

for (mp = mpList; mp != NULL; mp = mp->next)
    {
    slSort(&mp->pslList, pslCmpQuery);
    ml = oneRnaLine(mp->pslList, fragHash, cableType, scoreAdjust);
    if (ml != NULL)
	{
	slAddHead(&mlList, ml);
	}
    }
slReverse(&mlList);
slFreeList(&mpList);
freeHash(&rnaHash);
return mlList;
}

struct pair
/* Keep track of a pair. */
    {
    struct pair *next;  /* next in list */
    char *a;		/* First in pair. Allocated in hash. */
    char *b;            /* Second in pair. Allocated in hash. */
    struct psl *aPslList; /* List of all psl's referencing a.  Owned here. */
    struct psl *bPslList; /* List of all psl's referencing b.  Owned here. */
    int minDistance;	/* Minimum distance between pair. */
    int maxDistance;	/* Max distance between pair. */
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

struct pair *loadUnrangedPairs(char *pairName, struct hash *hash, 
	int minDistance, int maxDistance)
/* Make up a hash out of the pair file. */
{
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
    if ((wordCount = chopLine(line, words)) < 2)
	errAbort("Expecting 2 words line %d of %s\n", lf->lineIx, lf->fileName);
    AllocVar(pair);
    helA = hashAdd(hash, words[0], pair);
    helB = hashAdd(hash, words[1], pair);
    pair->a = helA->name;
    pair->b = helB->name;
    pair->minDistance = minDistance;
    pair->maxDistance = maxDistance;
    slAddHead(&list, pair);
    ++pairCount;
    }
printf("Got %d pairs in %s\n", pairCount, pairName);
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct psl *bestBacPsl(struct psl *pslList, int *retScore)
/* Return best scoring psl in list according to bac scoring function,
 * or NULL if none of them are good enough. */
{
struct psl *psl;
int bestScore = -BIGNUM;
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

struct raft *raftFromPslList(struct psl *pslList, struct hash *fragHash, 
	int *retScore, struct psl **retPsl, struct raftFrag **retRf, struct oogFrag **retFrag)
/* Lookup raft from best scoring psl in list. */
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
*retFrag = frag = hel->val;
raft = frag->raft;
*retRf = findRaftFrag(raft, frag);
return raft;
}

struct lineGraph *makeFragChains(char *fileName, struct hash *fragHash)
/* Create a lineGraphs out of fragChains file */
{
struct lineFile *lf;
char *words[16];
int wordCount;
struct lineGraph *mlList = NULL, *ml;
struct lineConnection *mc;
struct oogFrag *aFrag, *bFrag;
struct raft *aRaft, *bRaft;
struct raftFrag *aRf, *bRf;
int orientation;
char cloneName[128];

if ((lf = lineFileMayOpen(fileName, TRUE)) == NULL)
    {
    warn("Couldn't open %s", fileName);
    return NULL;
    }
while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    lineFileExpectWords(lf, 6, wordCount);
    aFrag = fragForName(fragHash, words[0]);
    bFrag = fragForName(fragHash, words[2]);
    if (aFrag != NULL && bFrag != NULL)
	{
	fragToCloneName(words[0], cloneName);
	aRaft = aFrag->raft;
	bRaft = bFrag->raft;
	if (aRaft != bRaft)
	    {
	    AllocVar(ml);
	    slAddTail(&mlList, ml);
	    ml->cableType = ctChain;
	    ml->minDistance = lineFileNeedNum(lf, words, 3);
	    ml->maxDistance = lineFileNeedNum(lf, words, 4);
	    ml->score = lineFileNeedNum(lf, words, 5);
	    orientation = ((words[1][0] == '-') ? -1 : 1);

	    AllocVar(mc);
	    mc->name = aFrag->name;
	    mc->raft = aRaft;
	    aRf = findRaftFrag(aRaft, aFrag);
	    mc->orientation = aRf->orientation;
	    mc->position = aRf->raftOffset + aFrag->size/2;
	    mc->score = ml->score;
	    slAddTail(&ml->mcList, mc);

	    AllocVar(mc);
	    mc->name = bFrag->name;
	    mc->raft = bRaft;
	    bRf = findRaftFrag(bRaft, bFrag);
	    mc->orientation = orientation * bRf->orientation;
	    mc->position = bRf->raftOffset + bFrag->size/2;
	    mc->score = ml->score;
	    slAddTail(&ml->mcList, mc);
	    }
	}
    }
lineFileClose(&lf);
slReverse(&mlList);
return mlList;
}

struct lineGraph *mlAntiFlip(struct raft *fixedRaft, 
	struct raft *raft, int flipAbs, int flipDir)
/* Make a line graph that encourages raft stay in "flipDir"
 * orientation with score proportional to flipAbs. ~~~ */
{
struct lineGraph *ml;
struct lineConnection *mc;
char nameBuf[12];

sprintf(nameBuf, "raft_%d", raft->id);

AllocVar(ml);
ml->score = flipAbs/10000;
ml->cableType = ctBigRaft;
ml->minDistance = 0;
ml->maxDistance = BIGNUM/10;

AllocVar(mc);
mc->name = "fixed";
mc->raft = fixedRaft;
mc->orientation = 1;
mc->score = ml->score;
slAddTail(&ml->mcList, mc);

AllocVar(mc);
mc->name = cloneString(nameBuf);
mc->raft = raft;
mc->orientation = flipDir;
mc->score = ml->score;
slAddTail(&ml->mcList, mc);

return ml;
}


struct lineGraph *makeLargeRaftLines(struct raft *fixedRaft, struct raft *raftList)
/* Make lines that connect from fixed, untwistable raft to
 * some of the rafts with stronger orientation info. */
{
struct raft *raft;
int flipAbs, flipDir;
struct lineGraph *mlList = NULL, *ml;

for (raft = raftList; raft != NULL; raft = raft->next)
    {
    /* Get magnitude and direction of raft's flipping tendency. */
    flipAbs = raft->flipTendency;
    flipDir = 1;
    if (flipAbs < 0)
        {
	flipDir = -1;
	flipAbs = -flipAbs;
	}
    if (flipAbs > 400000)
        {
	ml = mlAntiFlip(fixedRaft, raft, flipAbs, flipDir);
	slAddHead(&mlList, ml);
	}
    }
return mlList;
}


struct lineGraph *mlFromPair(struct pair *pair, struct hash *fragHash, enum cableType cableType)
/* Create an lineGraph describing connection between rafts implied by pair. */
{
struct raft *aRaft, *bRaft;
struct psl *aPsl, *bPsl;
struct raftFrag *aRf, *bRf;
int aScore, bScore;
int aOrient, bOrient;
struct hashEl *hel;
struct lineGraph *ml;
struct lineConnection *mc;
char buf[256];
struct oogFrag *aFrag, *bFrag;
int cloneDif;

if ((aRaft = raftFromPslList(pair->aPslList, fragHash, &aScore, &aPsl, &aRf, &aFrag)) == NULL)
    return NULL;
if ((bRaft = raftFromPslList(pair->bPslList, fragHash, &bScore, &bPsl, &bRf, &bFrag)) == NULL)
    return NULL;
if (aRaft == bRaft)
    return NULL;

cloneDif = maxMapDif(aFrag->clone, bFrag->clone);
if (cloneDif > maxMapDeviation)
    return NULL;

AllocVar(ml);
ml->cableType = cableType;
ml->minDistance = pair->minDistance;
ml->maxDistance = pair->maxDistance;
ml->score = min(aScore, bScore);
if (cableType == ctBacEndPair)	/* BAC ends get no respect! */
    ml->score -=  50;
if (cloneDif > 200000)
    ml->score -= (cloneDif-200000)/1500;

AllocVar(mc);
mc->name = pair->a;
mc->raft = aRaft;
mc->orientation = aOrient = pslOrientation(aPsl) * aRf->orientation;
mc->position = pslCenterInRaft(aPsl, aRf);
mc->score = aScore;
slAddTail(&ml->mcList, mc);

AllocVar(mc);
mc->name = pair->b;
mc->raft = bRaft;
mc->orientation = bOrient = -pslOrientation(bPsl) * bRf->orientation;
mc->position = pslCenterInRaft(bPsl, bRf);
mc->score = bScore;
slAddTail(&ml->mcList, mc);

return ml;
}

struct lineGraph *makePairedLines(struct pair *pairList, char *pslName, 
	struct hash *fragHash, struct hash *pairHash, enum cableType cableType,
	struct hash *splitFinFragHash, int maxMapDistance)
/* Read in pair file and psl file to create list of bac end connections
 * in lineGraph format. */
{
struct lineGraph *mlList = NULL, *ml;
struct pair *pair;
struct psl *psl, *nextPsl, *pslList;
struct hashEl *hel;
char *qName;
int totalPsl = 0;
int pairedPsl = 0;
int aMapPos, bMapPos;

pslList = pslLoadAll(pslName);
for (psl = pslList; psl != NULL; psl = nextPsl)
    {
    nextPsl = psl->next;
    if (pslTargetFragFilter(psl, fragHash))
	{
	++totalPsl;
	qName = psl->qName;
	if ((hel = hashLookup(pairHash, qName)) == NULL)
	    {
	    pslFree(&psl);
	    continue;
	    }
	++pairedPsl;
	pair = hel->val;
	if (sameString(qName, pair->a))
	    {
	    slAddHead(&pair->aPslList, psl);
	    }
	else
	    {
	    slAddHead(&pair->bPslList, psl);
	    }
	}
    else
        pslFree(&psl);
    }
fprintf(stdout, "%d paired %s psls.  %d total psls\n", pairedPsl, 
	cableTypeName(cableType), totalPsl);
fprintf(logFile, "%d paired %s psls.  %d total psls\n", pairedPsl, 
        cableTypeName(cableType), totalPsl);

for (pair = pairList; pair != NULL; pair = pair->next)
    {
    ml = mlFromPair(pair, fragHash, cableType);
    if (ml != NULL)
	slAddHead(&mlList, ml);
    }
pairFreeList(&pairList);
slReverse(&mlList);
return mlList;
}


struct pair *loadRangedPairs(char *fileName, struct hash *hash)
/* Load up pairs in file to list/hash */
{
struct pair *rpList = NULL, *rp;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[16];
int wordCount;
struct hashEl *hel;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    lineFileExpectWords(lf, 5, wordCount);
    AllocVar(rp);
    hel = hashAddUnique(hash, words[0], rp);
    rp->a = hel->name;
    hel = hashAddUnique(hash, words[1], rp);
    rp->b = hel->name;
    rp->minDistance = atoi(words[3]);
    rp->maxDistance = atoi(words[4]);
    slAddHead(&rpList, rp);
    }
slReverse(&rpList);
lineFileClose(&lf);
return rpList;
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

boolean enclosingPsl(struct psl *psl)
/* Return TRUE if one or other fragment of PSL pretty much encloses
 * the other. */
{
if (psl->tStart <= 300 && psl->tEnd + 300 >= psl->tSize)
    return TRUE;
if (psl->qStart <= 300 && psl->qEnd + 300 >= psl->qSize)
    return TRUE;
return FALSE;
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

if (qForward)
    {
    qFloppyStart = psl->qStart - qFrag->startGood;
    if (qFloppyStart < 0) qFloppyStart = 0;
    qFloppyEnd = qFrag->endGood - psl->qEnd;
    if (qFloppyEnd < 0) qFloppyEnd = 0;
    tFloppyStart = psl->tStart - tFrag->startGood;
    if (tFloppyStart < 0) tFloppyStart = 0;
    tFloppyEnd = tFrag->endGood - psl->tEnd;
    if (tFloppyEnd < 0) tFloppyEnd = 0;
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
    qFloppyStart = qFrag->endGood - psl->qEnd;
    if (qFloppyStart < 0) qFloppyStart = 0;
    qFloppyEnd = psl->qStart - qFrag->startGood;
    if (qFloppyEnd < 0) qFloppyEnd = 0;
    tFloppyStart = psl->tStart - tFrag->startGood;
    if (tFloppyStart < 0) tFloppyStart = 0;
    tFloppyEnd = tFrag->endGood - psl->tEnd;
    if (tFloppyEnd < 0) tFloppyEnd = 0;
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
edge->match = psl->match;
edge->repMatch = psl->repMatch;
edge->milliBad = pslCalcMilliBad(psl, FALSE);
edge->tailA = startTail;
edge->tailB = endTail;

edge->score = -30*edge->milliBad - (startTail+endTail)/2 + 
	round(25*log(edge->match+1) + log(edge->repMatch+1)); 

/* Fold in penalties for not having any unique matches
 * or for being a small fragment. */
if (!enclosingPsl(psl))
    {
    if (edge->match <= 20)
	edge->score -= (20-edge->match)*25;
    if (psl->qSize < 5000)
	edge->score -= (5000-psl->qSize)/40;
    if (psl->tSize < 5000)
	edge->score -= (5000-psl->tSize)/40;
    }

edge->psl = psl;
return edge;
}

void processSelf(struct psl *pslList, struct barge *bargeList,
        struct oogClone **pCloneList, struct oogFrag **pFragList, 
	struct hash *cloneHash, struct hash *fragHash, struct hash *mapOverlapHash)
/* Process self psl list.  Make up clone, fragment, and self edge list. */
{
struct psl *psl;
struct oogEdge *edge;
struct oogClone *qClone, *tClone;
struct oogFrag *qFrag, *tFrag;
struct barge *barge;

/* Create edges from psl's that score decently and connect
 * clones in the same barge. */
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    if (pslFragFilter(psl, fragHash))
	{
	qClone = cloneFromName(psl->qName, pCloneList, cloneHash);
	tClone = cloneFromName(psl->tName, pCloneList, cloneHash);
	barge = qClone->barge;
	if (barge == tClone->barge && inPairHash(mapOverlapHash, qClone->name, tClone->name))
	    {
	    qFrag = fragFromName(psl->qName, pFragList, fragHash, qClone, psl->qSize);
	    tFrag = fragFromName(psl->tName, pFragList, fragHash, tClone, psl->tSize);
	    if (!sameString(psl->qName, psl->tName))
		{
		edge = edgeFromSelfPsl(psl, qFrag, tFrag);
		if (edge != NULL && edge->score >= minScore)
		    {
		    slAddHead(&barge->selfEdges, edge);
		    }
		}
	    }
	}
    }

/* Sort edges by score on all barges. */
for (barge = bargeList; barge != NULL; barge = barge->next)
    slSort(&barge->selfEdges, oogEdgeCmp);
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

void normalizeRaft(struct raft *raft)
/* Rearrange all offsets so that raft goes from zero to length of raft.  Fix up
 * graph begin and end.*/
{
int sMin = BIGNUM;
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

fprintf(logFile, "raftOfTwo #%d: %s(%d) %c %s(%d) @ %d   .... psl (%d %d) (%d %d) start %d end %d\n", 
        raft->id, edge->aSide->name,  edge->aSide->size,
	(edge->orientation > 0 ? '+' : '-'),
	edge->bSide->name, edge->bSide->size, bOffset, 
	psl->qStart, psl->qEnd, psl->tStart, psl->tEnd,
	raft->start, raft->end);
normalizeRaft(raft);
return raft;
}

/* Buffers to check raft/frag and raft/raft overlap. */
static int usedArrayAlloc = 0;
static bool *usedArray = NULL;

static void allocEnoughUsed(int checkSize)
/* Make sure usedArray is big enough for checkSize and
 * zero it out. */
{
if (checkSize > usedArrayAlloc)
    {
    usedArrayAlloc = checkSize + 10000;
    usedArray = needMoreMem(usedArray, 0, usedArrayAlloc);
    }
zeroBytes(usedArray, checkSize);
}

boolean fragMostlyAligns(struct oogFrag *frag, int raftOffset, short raftOrientation, 
	int fragStart, int fragEnd, struct raft *raft, struct oogEdge *selfEdgeList)
/* Return TRUE if frag aligns with something already in raft between start and stop. */
{
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

/* Allocate array that will track which parts have an alignment. */
allocEnoughUsed(checkSize);

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

void hashFragNames(struct hash *hash, struct raftFrag *rfList)
/* Put names of all frags in list onto hash. */
{
struct raftFrag *rf;
for (rf = rfList; rf != NULL; rf = rf->next)
    {
    hashAdd(hash, rf->frag->name, rf);
    }
}

boolean bMostlyAlignsWithA(struct raft *aRaft, int aStart, int aEnd, struct raft *bRaft,
   int bStart, int bEnd, short relOrientation, struct oogEdge *mergeEdge, 
   struct oogEdge *selfEdgeList, int diag)
/* Return TRUE if the indicated part of b aligns roughly with the indicated part
 * of a. */
{
struct hash *aHash = newHash(0), *bHash = newHash(0);
int checkSize = aEnd - aStart;
int bCheckSize = bEnd - bStart;
struct oogEdge *edge;
int i;
int misMatchCount = 0;
int aOk = FALSE;


if (checkSize <= maxTailSize)
    return TRUE;
if (bCheckSize != checkSize)
    {
    dumpEdge(mergeEdge, logFile);
    errAbort("internal error: aCheckSize(%d) != bCheckSize(%d)", checkSize, bCheckSize);
    }
if (aStart < aRaft->start)
    {
    dumpEdge(mergeEdge, logFile);
    errAbort("internal error: aStart(%d) < aRaft->start(%d)", aStart, aRaft->start);
    }
if (bStart < bRaft->start)
    {
    dumpEdge(mergeEdge, logFile);
    errAbort("internal error: bStart(%d) < bRaft->start(%d)", bStart, bRaft->start);
    }
if (aEnd > aRaft->end)
    {
    dumpEdge(mergeEdge, logFile);
    errAbort("internal error: aEnd(%d) > aRaft->end(%d)", aEnd, aRaft->end);
    }
if (bEnd > bRaft->end)
    {
    dumpEdge(mergeEdge, logFile);
    errAbort("internal error: bEnd(%d) > bRaft->end(%d)", bEnd, bRaft->end);
    }


hashFragNames(aHash, aRaft->fragList);
hashFragNames(bHash, bRaft->fragList);

allocEnoughUsed(checkSize);
for (edge = selfEdgeList; edge != NULL; edge = edge->next)
    {
    struct psl *psl = edge->psl;
    struct hashEl *aHel, *bHel;
    int as,ae,bs,be,asz,bsz;
    boolean relevant = FALSE;

    if ((aHel = hashLookup(aHash, psl->qName)) != NULL 
    	&& ((bHel = hashLookup(bHash, psl->tName)) != NULL))
	{
	as = psl->qStart;
	ae = psl->qEnd;
	asz = psl->qSize;
	bs = psl->tStart;
	be = psl->tEnd;
	bsz = psl->tSize;
	relevant = TRUE;
	}
    else if ((aHel = hashLookup(aHash, psl->tName)) != NULL
	&& ((bHel = hashLookup(bHash, psl->qName)) != NULL))
	{
	as = psl->tStart;
	ae = psl->tEnd;
	asz = psl->tSize;
	bs = psl->qStart;
	be = psl->qEnd;
	bsz = psl->qSize;
	relevant = TRUE;
	}
    if (relevant)
	{
	struct raftFrag *aRf = aHel->val, *bRf = bHel->val;
	int lDiag;
	int diagDistance;

	fragRangeToRaftRange(as, ae, asz, aRf->orientation, aRf->raftOffset, &as, &ae);
	fragRangeToRaftRange(bs, be, bsz, bRf->orientation, bRf->raftOffset, &bs, &be);
	if (relOrientation > 0)
	    lDiag = as - bs;
	else
	    lDiag = as - (bRaft->end - be);
	diagDistance = intAbs(lDiag - diag);
	if (diagDistance < 10)
	    {
	    int s,e,sz;
	    s = as - aStart;
	    if (s < 0)
		s = 0;
	    e = ae - aStart;
	    if (e > checkSize)
		e = checkSize;
	    sz = e - s;
	    fprintf(logFile, "as %d, ae %d, s %d, e %d, sz %d, checkSize %d\n",
	    	as, ae, s, e, sz, checkSize);
	    if (sz == checkSize)
		{
		aOk = TRUE;	/* We're totally covered... */
		break;
		}
	    if (sz > 0)
		{
		memset(usedArray + s, 1, sz);
		}
	    }
	}
    }
if (!aOk)
    {
    for (i=0; i<checkSize; ++i)
	{
	if (!usedArray[i]) ++misMatchCount;
	}
    }

freeHash(&aHash);
freeHash(&bHash);
return (misMatchCount < 16);
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

void noteRedundant(struct oogEdge *edge, struct raft *raft)
/* Note that given edge is redundant. */
{
fprintf(logFile, "redundant edge from %s to %s in raft %d\n", edge->aSide->name, edge->bSide->name, raft->id);
}

void noteConflicting(struct oogEdge *edge, char *why)
/* Note that given edge is redundant. */
{
fprintf(logFile, "conflicting edge from %s to %s (%s).\n", edge->aSide->name, edge->bSide->name, why);
}

boolean extendRaft(struct raft *raft, struct raftFrag *socket, 
	struct oogEdge *edge, struct oogFrag *newFrag, struct oogEdge *selfEdgeList)
/* Try to extend raft to include other fragment in edge. */
{
int offset;
short orientation;

fprintf(logFile, "extend raft %d\n", raft->id);
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
slReverse(&raft->fragList);
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

boolean canMergeRaftInMiddle(struct oogEdge *selfEdgeList, struct raft *aRaft, struct raft *bRaft,
	struct raftFrag *aRf, struct raftFrag *bRf, struct oogEdge *edge,
	int relativeRaftOrientation, int aRaftAliStart, int aRaftAliEnd,
	int bRaftAliStart, int bRaftAliEnd, int startTail, int endTail)
/* Here we have an edge that connects the middle of two rafts.  We need to
 * make sure that the rafts are compatable along the full length of their
 * overlap, not just along the part aligned by edge. */
{
int as, bs, diag;
fprintf(logFile, "canMergeRaftInMiddle(raftA %d:%d-%d of %d, raftB %d:%d-%d of %d, relOrient %d)\n",
	aRaft->id, aRaftAliStart, aRaftAliEnd, aRaft->end,
	bRaft->id, bRaftAliStart, bRaftAliEnd, bRaft->end,
	relativeRaftOrientation);
if (relativeRaftOrientation > 0)
    {
    fprintf(logFile, " >>>case 1<<<\n");
    as = aRaftAliStart;
    bs = bRaftAliStart;
    diag = as-bs;
    if (!bMostlyAlignsWithA(aRaft, aRaftAliStart-startTail, aRaftAliStart, 
    	bRaft, bRaftAliStart - startTail, bRaftAliStart, 
    	relativeRaftOrientation, edge, selfEdgeList, diag))
	return FALSE;
    if (!bMostlyAlignsWithA(aRaft, aRaftAliEnd, aRaftAliEnd+endTail, 
    	bRaft, bRaftAliEnd, bRaftAliEnd+endTail,
    	relativeRaftOrientation, edge, selfEdgeList, diag))
	return FALSE;
    }
else
    {
    fprintf(logFile, " >>>case -1<<<\n");
    as = aRaftAliStart;
    bs = bRaft->end - bRaftAliEnd;
    diag = as-bs;
    if (!bMostlyAlignsWithA(aRaft, aRaftAliStart-startTail, aRaftAliStart, 
    	bRaft, bRaftAliEnd, bRaftAliEnd+startTail, 
    	relativeRaftOrientation, edge, selfEdgeList, diag))
	return FALSE;
    if (!bMostlyAlignsWithA(aRaft, aRaftAliEnd, aRaftAliEnd+endTail, 
    	bRaft, bRaftAliStart-endTail, bRaftAliStart, 
    	relativeRaftOrientation, edge, selfEdgeList, diag))
	return FALSE;
    }
return TRUE;
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
    fprintf(logFile, " startTail %d endTail %d\n", startTail, endTail);
    if (!canMergeRaftInMiddle(selfEdgeList, aRaft, bRaft, aRf, bRf, edge, relativeRaftOrientation,
    	aRaftAliStart, aRaftAliEnd, bRaftAliStart, bRaftAliEnd, startTail, endTail))
	return FALSE;
    }
else if (endTail > maxTailSize)
    {
    noteConflicting(edge, "Merge long end tail");
    fprintf(logFile, " startTail %d endTail %d\n", startTail, endTail);
    if (!canMergeRaftInMiddle(selfEdgeList, aRaft, bRaft, aRf, bRf, edge, relativeRaftOrientation,
    	aRaftAliStart, aRaftAliEnd, bRaftAliStart, bRaftAliEnd, startTail, endTail))
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


struct dlNode *findBestRaftEdge(struct oogFrag *frag, struct raft *origRaft,
	struct oogEdge *origEdge, struct dlNode *origNode, struct dlList *todoList, 
	struct raft *raftList)
/* Find biggest raft that frag could glom into reasonably.  */
{
int scoreThreshold;
struct raft *longestRaft = origRaft;
int longestSize = origRaft->end - origRaft->start;
struct dlNode *bestNode = origNode;
struct oogEdge *edge;
struct raft *raft;
int size;
struct dlNode *node;

scoreThreshold = origEdge->score - 100;
if (scoreThreshold < minScore)
    scoreThreshold = minScore;
for (node = todoList->head; node->next != NULL; node = node->next)
    {
    edge = node->val;
    if (edge->score >= scoreThreshold && !edge->used)
	{
	if (edge->aSide == frag)
	    {
	    if ((raft = edge->bSide->raft) != NULL)
		{
		size = raft->end - raft->start;
		if (size > longestSize)
		    {
		    longestSize = size;
		    bestNode = node;
		    longestRaft = raft;
		    }
		}
	    }
	else if (edge->bSide == frag)
	    {
	    if ((raft = edge->aSide->raft) != NULL)
		{
		size = raft->end - raft->start;
		if (size > longestSize)
		    {
		    longestSize = size;
		    bestNode = node;
		    longestRaft = raft;
		    }
		}
	    }
	}
    }
/* If have a new node, then put origNode back on todoList and take
 * off bestNode. */
if (bestNode != origNode)
    {
    dlRemove(bestNode);
    dlAddHead(todoList, origNode);
    }
return bestNode;
}

void mergeBestRaftEdge(struct oogFrag *frag, struct raft *origRaft,
	struct oogEdge *origEdge, struct dlList *todoList, struct dlNode *origNode, 
	struct raft *raftList, struct oogEdge *selfEdgeList, boolean mergeMax)
/* Merge into best reasonable looking raft. */
{
struct oogEdge *e;
struct oogFrag *otherFrag;
struct raft *raft;
boolean edgeOk;
struct dlNode *node;

if (mergeMax)
    node = findBestRaftEdge(frag, origRaft, origEdge, origNode, todoList, raftList);
else
    node = origNode;
e = node->val;
freez(&node);
e->used = TRUE;
if (e->aSide == frag)
    {
    otherFrag = e->bSide;
    }
else if (e->bSide == frag)
    {
    otherFrag = e->aSide;
    }
else
    {
    assert(FALSE);
    }
raft = otherFrag->raft;
assert(raft != NULL);
edgeOk = extendRaft(raft, findRaftFrag(raft, otherFrag), e, frag, selfEdgeList);
if (edgeOk)
    {
    addEdgeRefToRaft(raft, e);
    maybeLogRaft(raft);
    }
}


struct raft *makeRafts(struct oogFrag *fragList, 
    struct oogEdge *selfEdgeList, boolean mergeMax)
/* Make connected rafts of fragments. Returns doubly linked list of rafts. */
{
struct raft *raftList = NULL;
struct oogEdge *edge;
struct raft *aRaft, *bRaft, *raft;
struct raftFrag *aRf, *bRf, *rf;
struct dlList *todoList = newDlList();
struct dlNode *todoNode;
boolean aIn, bIn;
int max = 10;
struct mergeFailure *mergeFailureList = NULL;

/* Clear used flags and put edges on todo list. */
for (edge = selfEdgeList; edge != NULL; edge = edge->next)
    {
    edge->used = FALSE;
    dlAddValTail(todoList, edge);
    }
while ((todoNode = dlPopHead(todoList)) != NULL)
    {
    struct psl *psl;
    boolean edgeOk = TRUE;

    /* Get next edge and psl from list and record it in log. */
    edge = todoNode->val;
    psl = edge->psl;
    dumpEdge(edge, logFile);
    fprintf(logFile, "psl: (%s %d %d of %d) %s (%s %d %d of %d)\n", 
	   psl->qName, psl->qStart, psl->qEnd, psl->qSize, 
	   psl->strand,
	   psl->tName, psl->tStart, psl->tEnd, psl->tSize);

    /* Figure out which sides of edge are already in a raft. */
    aIn = fragInSomeRaft(raftList, edge->aSide, &aRaft, &aRf);
    bIn = fragInSomeRaft(raftList, edge->bSide, &bRaft, &bRf);


    if (!aIn && !bIn)
	{
	/* If neither side in a raft make up a new raft of just the two. */
	raft = raftOfTwo(edge);
	addEdgeRefToRaft(raft, edge);
	maybeLogRaft(raft);
	slAddHead(&raftList, raft);
	freeMem(todoNode);
	}
    else if (aIn && bIn)
	{
	/* If both sides in a raft check to see if in same raft, in
	 * which case don't have to do much.  If sides are in different
	 * rafts try to merge the two rafts. */
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
		noteRedundant(edge, aRaft);
		addEdgeRefToRaft(aRaft, edge);
		maybeLogRaft(aRaft);
		}
	    else
		{
		edgeOk = FALSE;
		fprintf(logFile, "bad orientation in redundant edge\n");
		}
	    }
	freeMem(todoNode);
	}
    /* In next two cases only one side is in a raft.  See if can merge the
     * other side into this raft. */
    else if (aIn)
	{
	mergeBestRaftEdge(edge->bSide, aRaft, edge, todoList, todoNode, 
	    raftList, selfEdgeList, mergeMax);
	}
    else if (bIn)
	{
	mergeBestRaftEdge(edge->aSide, bRaft, edge, todoList, todoNode, 
	    raftList, selfEdgeList, mergeMax);
	}
    fprintf(logFile, "\n");
    if (edgeOk && raftPsl != NULL)
	pslTabOut(psl, raftPsl);
    }
reprocessMergeFailures(mergeFailureList);
slFreeList(&mergeFailureList);
freeDlList(&todoList);

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
    if (frag->raft == NULL && filterFrag(frag))
	{
	raft = raftOfOne(frag);
	slAddHead(&list, raft);
	}
    }
slReverse(&list);
*pRaftList = slCat(*pRaftList, list);
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

void loadCloneOverlap(char *fileName, struct hash *cloneHash, 
    struct overlappingClonePair **retOcpList, struct hash *ocpHash)
/* Load in basic clone overlap info. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct overlappingClonePair *ocpList = NULL, *ocp;
char *row[11];
int overlap;
struct oogClone *a, *b;

while (lineFileRow(lf, row))
    {
    a = hashFindVal(cloneHash, row[0]);
    b = hashFindVal(cloneHash, row[4]);
    if (a && b)
	{
	AllocVar(ocp);
	slAddHead(&ocpList, ocp);
	ocp->a = a;
	ocp->b = b;
	hashAddUnique(ocpHash, ocpHashName(a->name, b->name), ocp);

	/* Use strict overlap unless it is empty. */
	overlap = lineFileNeedNum(lf, row, 8);
	if (overlap == 0)
	    overlap = lineFileNeedNum(lf, row, 9);
	ocp->overlap = overlap;
	}
    }
lineFileClose(&lf);
slReverse(&ocpList);
*retOcpList = ocpList;
}

struct overlappingClonePair *findHashedOcp(char *aName, char *bName, struct hash *ocpHash)
/* Find overlappingClonePair if any associated with a/b overlap. */
{
char *ocpName = ocpHashName(aName, bName);
return hashFindVal(ocpHash, ocpName);
}

int findHashedOverlap(char *aName, char *bName, struct hash *ocpHash)
/* Find how much a and b overlap. */
{
struct overlappingClonePair *ocp;
ocp = findHashedOcp(aName, bName, ocpHash);
if (ocp == NULL)
   return 0;
return ocp->overlap;
}

void fillInBargeSize(struct barge *barge)
/* Fill in barge->size and bel->offset . */
{
struct oogClone *clone;
struct bargeEl *bel;


for (bel = barge->cloneList; bel != NULL; bel = bel->next)
    {
    if (bel->next == NULL)
        {
	clone = bel->clone;
	barge->size = bel->offset + clone->size;
	}
    }
}


static int nextBargeId()
/* Return next ID for a barge. */
{
static int id = 0;
return ++id;
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

void linkRaftsToBarges(struct raft *raftList)
/* Link rafts to barges. */
{
struct raft *raft;
struct oogClone *clone;
for (raft = raftList; raft != NULL; raft = raft->next)
    {
    clone = raft->fragList->frag->clone;
    if (clone->barge == NULL)
	errAbort("Clone %s never assigned a barge", clone->name);
    raft->barge = clone->barge;
    }
}

void setMaxShare(struct oogClone *clone, struct oogClone *share, int overlap)
/* See if share overlaps clone more than any other so far.  If so update
 * clone->maxShare... */
{
if (overlap > clone->maxShare)
    {
    clone->maxShare = overlap;
    clone->maxShareClone = share;
    }
}

void findMaxShare(struct overlappingClonePair *ocpList)
/* Find the maximally sharing sequence for each clone. */
{
struct overlappingClonePair *ocp;
struct oogClone *a, *b;

for (ocp = ocpList; ocp != NULL; ocp = ocp->next)
    {
    a = ocp->a;
    b = ocp->b;
    setMaxShare(a, b, ocp->overlap);
    setMaxShare(b, a, ocp->overlap);
    }
}

struct cloneEnd *parseEnds(struct lineFile *lf, char *line, struct hash *cloneHash)
/* Parse out a line of clone ends ends. */
{
struct cloneEnd *ceList = NULL, *ce;
char c;
int orientation;
struct oogClone *clone;
char *word;

while ((word = nextWord(&line)) != NULL)
    {
    AllocVar(ce);
    if (word[1] == '(')
	{
	c = word[0];
	if (c == '-')
	    orientation = -1;
	else if (c == '+')
	    orientation = +1;
	else if (c == '?')
	    orientation = 0;
	else
	    errAbort("Unknown orientation on %s, line %d of %s", 
		    word, lf->lineIx, lf->fileName);
	clone = hashMustFindVal(cloneHash, word+2);
	clone->orientation = orientation;
	ce->clone = clone;
	ce->orientation = orientation;
	ce->isStart = TRUE;
	slAddHead(&ceList, ce);
	}
    else
	{
	int end = strlen(word) - 1;
	c = word[end];
	if (c != ')')
	     {
	     errAbort("Expecting %s to end with ')', line %d of %s",
		    word, lf->lineIx, lf->fileName);
	     }
	word[end] = 0;
	ce->clone = clone = hashMustFindVal(cloneHash, word);
	ce->orientation = clone->orientation;
	slAddHead(&ceList, ce);
	}
    }
slReverse(&ceList);
return ceList;
}

struct barge *loadBarges(char *endFileName, struct oogClone *cloneList, struct hash *cloneHash)
/* Construct barges around end info in file. */
{
struct lineFile *lf = lineFileOpen(endFileName, TRUE);
char *line;
int lineSize;
struct barge *bargeList = NULL, *barge;
struct bargeEl *bel;
struct oogClone *clone;
struct cloneEnd *ce;
int offset, firstOffset;

while (lineFileNext(lf, &line, &lineSize))
    {
    /* Skip empty and commented out lines. */
    line = skipLeadingSpaces(line);
    if (line == NULL || line[0] == '#' || line[0] == 0)
        continue;

    AllocVar(barge);
    slAddHead(&bargeList, barge);
    barge->id = nextBargeId();
    barge->endList = parseEnds(lf, line, cloneHash);
    firstOffset = barge->endList->clone->mapPos;
    for (ce = barge->endList; ce != NULL; ce = ce->next)
        {
	if (ce->isStart)
	    {
	    clone = ce->clone;
	    AllocVar(bel);
	    bel->clone = clone;
	    clone->bel = bel;
	    clone->barge = barge;
	    bel->offset = clone->mapPos - firstOffset;
	    slAddHead(&barge->cloneList, bel);
	    }
	}
    slReverse(&barge->cloneList);
    fillInBargeSize(barge);
    }
slReverse(&bargeList);
return bargeList;
}

void saveBarges(char *fileName, struct barge *bargeList, struct diGraph *bargeGraph, struct hash *ocpHash)
/* Save barge list to file. */
{
FILE *f = mustOpen(fileName, "w");
struct barge *barge, *nextBarge;
struct bargeEl *bel;
struct oogClone *clone;

fprintf(f, "Barge (Connected Clone) File ooGreedy Version %d\n\n", version);
fprintf(f, "start   accession  size overlap maxClone  maxOverlap\n");
fprintf(f, "-------------------------------------------------------\n");
for (barge = bargeList; barge != NULL; barge = barge->next)
    {
    for (bel = barge->cloneList; bel != NULL; bel = bel->next)
	{
	int overlap = 0;
	struct bargeEl *nextBel;
	char *shareName = "n/a";
	clone = bel->clone;
	if (clone->maxShareClone != NULL)
	    shareName = clone->maxShareClone->name;
	if ((nextBel = bel->next) != NULL)
	    overlap = findHashedOverlap(clone->name, nextBel->clone->name, ocpHash);
	fprintf(f, "%7d %-9s %6d %6d %-9s %6d\n",
	    clone->defaultPos, clone->name, clone->size, overlap,
		shareName, clone->maxShare);
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

void figureMapOverlaps(struct barge *bargeList, struct hash *mapOverlapHash)
/* Figure out overlapping clones according to end info from map
 * which is stored in barges.  Make an entry in mapOverlapHash
 * for each overlapping pair. */
{
struct barge *barge;
struct cloneEnd *ce, *oce;
struct oogClone *clone, *oClone;
char *pairName;
int mapOverlapCount = 0;

for (barge = bargeList; barge != NULL; barge = barge->next)
    {
    for (ce = barge->endList; ce != NULL; ce = ce->next)
        {
	if (ce->isStart)
	    {
	    clone = ce->clone;
	    pairName = ocpHashName(clone->name, clone->name);
	    hashAdd(mapOverlapHash, pairName, NULL);
	    for (oce = ce->next; oce->clone != clone; oce = oce->next)
	        {
		pairName = ocpHashName(clone->name, oce->clone->name);
		if (!hashLookup(mapOverlapHash, pairName))
		    {
		    hashAdd(mapOverlapHash, pairName, NULL);
		    ++mapOverlapCount;
		    }
		}
	    }
	}
    }
printf("Got %d map overlaps\n", mapOverlapCount);
}

void figureMapEnclosures(struct barge *bargeList)
/* Figure out clones that are completely enclosed according to
 * map end info. */
{
struct barge *barge;
struct oogClone **openClones;
int openCount = 0, openAlloc = 128, i;
struct cloneEnd *ce, *oce;
struct oogClone *clone, *oClone;
int enclosedCount = 0;

AllocArray(openClones, openAlloc);
for (barge = bargeList; barge != NULL; barge = barge->next)
    {
    for (ce = barge->endList; ce != NULL; ce = ce->next)
        {
	if (ce->isStart)
	    {
	    /* Loop through end list until get end of same clone. */
	    clone = ce->clone;
	    openCount = 0;
	    for (oce = ce->next; oce->clone != clone; oce = oce->next)
		{
		oClone = oce->clone;
		/* Track clones that start inside of this one. 
		 * If necessary expand array needed to track them. */
		if (oce->isStart)
		    {
		    if (openCount >= openAlloc)
		        {
			openAlloc *= 2;
			ExpandArray(openClones, openCount, openAlloc);
			}
		    openClones[openCount++] = oClone;
		    }
		/* See if clones that end inside also started inside.
		 * If so, mark them as enclosed. */
		else
		    {
		    for (i=0; i<openCount; ++i)
		        {
			if (openClones[i] == oClone)
			    {
			    if (oClone->bel->enclosed == NULL)
				{
				oClone->bel->enclosed = clone->bel;
				++enclosedCount;
				}
			    }
			}
		    }
		}
	    }
	}
    }
printf("Found %d enclosed clones\n", enclosedCount);
freeMem(openClones);
}

void calcFragDefaultPositions(struct oogClone *clone, int cloneOff)
/* Set default positions of frags from clone default or map pos. */
{
struct dlNode *fragNode;
int fragOff = 0;
struct oogFrag *frag;


for (fragNode = clone->fragList->head; fragNode->next != NULL; fragNode = fragNode->next)
    {
    frag = fragNode->val;
    frag->defaultPos = cloneOff + fragOff;
    fragOff += 10;
    }
}

void calcPositions(struct barge *bargeList, struct oogFrag *fragList, struct diGraph *bargeGraph)
/* Calculate positions of barges, clones, frags. */
{
int bargeOff = 0;
struct barge *barge, *nextBarge;
struct bargeEl *bel;
struct oogClone *clone;

/* Set barge positions. */
for (barge = bargeList; barge != NULL; barge = barge->next)
    {
    barge->offset = bargeOff;
    bargeOff += barge->size;
    if ((nextBarge = barge->next) != NULL)
	{
	if (bargeGraph != NULL)
	    {
	    if (dgDirectlyFollows(bargeGraph, barge->node, nextBarge->node))
		bargeOff += bridgedBargeGapSize;
	    else
		bargeOff += unbridgedBargeGapSize;
	    }
	else
	    {
	    bargeOff += bridgedBargeGapSize;
	    }
	}
    }

/* Set clone positions. */
for (barge = bargeList; barge != NULL; barge = barge->next)
    {
    for (bel = barge->cloneList; bel != NULL; bel = bel->next)
	{
	clone = bel->clone;
	clone->defaultPos = barge->offset + bel->offset;
	if (bargeGraph != NULL)
	    {
	    clone->startNode->priority = clone->defaultPos;
	    clone->endNode->priority = clone->defaultPos + clone->size;
	    }
	calcFragDefaultPositions(clone, clone->defaultPos);
	}
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
struct lineConnectionRef *lRef;
struct lineConnection *mc;

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
boolean ok = TRUE;

/* Write raft to log.  */
logIt("Flipping connected component of %d rafts\n", slCount(raftList));
for (ref = raftList; ref != NULL; ref = ref->next)
    {
    raft = ref->node->val;
    logIt(" %d", raft->id);
    }
logIt("\n", raft->id);

/* See if the fixed raft is part of this component, if so
 * refuser to flip. ~~~ */
for (ref = raftList; ref != NULL; ref = ref->next)
    {
    raft = ref->node->val;
    if (raft->id == srOriented)
        {
	logIt("Couldn't flip because contains oriented raft\n");
	return FALSE;
	}
    }
    

erList = dgFindSubEdges(raftGraph, raftList);
dgSwapEdges(raftGraph, erList);
ok = !dgHasCycles(raftGraph);
if (ok)
    {
    for (ref = raftList; ref != NULL; ref = ref->next)
	{
	raft = ref->node->val;
	raft->orientation = -raft->orientation;
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
struct lineConnectionRef *lRef;
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

boolean wouldAddCycle(struct diGraph *graph, struct lineConnection *mcList)
/* Add bits of mcList not already in graph to graph.  See if it has
 * cycles, remove them. Report if has cycles. */
{
int mcCount = slCount(mcList);
struct lineConnection *mc;
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

void printAboutLineGraph(char *prefix, FILE *f, struct lineGraph *ml)
/* Print message identifying lineGraph. */
{
struct lineConnection *mc;
fprintf(f, "%s", prefix);
for (mc = ml->mcList; mc != NULL; mc = mc->next)
    {
    fprintf(f, " %s", mc->name);
    if (ml->cableType == ctMrna || ml->cableType == ctEst)
        break;
    }
fprintf(f, ", score %d\n", ml->score);
}

int raftCenterOffset(struct raft *raft, int position)
/* Calculate offset of position from center of raft, adjusting
 * for raft orientation. */
{
int size = raft->end;
int center = size/2;
int offset;

assert(raft->start == 0);	/* Do we need to normalize raft? */
if (raft->orientation > 0)
    return center - position;
else
    return position - center;
}

void lineConnectionPairDistanceRange(struct lineGraph *ml,
    struct lineConnection *a, struct lineConnection *b,
    int *retMin, int *retMax)
/* Find min and max positions between a and b.  */
{
struct raft *aRaft, *bRaft;
int minDistance, maxDistance;
int aCenOff, bCenOff;
int x;
int estimatedReadSize = 700;

aRaft = a->raft;
bRaft = b->raft;
aCenOff = raftCenterOffset(aRaft, a->position);
bCenOff = raftCenterOffset(bRaft, b->position);
minDistance = round(0.47 * (aRaft->end + bRaft->end));
x = ml->minDistance - aCenOff + bCenOff;
if (x > minDistance)
   minDistance = x;
maxDistance = ml->maxDistance - aCenOff + bCenOff + estimatedReadSize;
// fprintf(logFile, "   minDistance %d, maxDistance %d\n", minDistance, maxDistance);
*retMin = minDistance;
*retMax = maxDistance;
}

boolean localFindEdgeRange(struct dgEdge *edge, int *retMin, int *retMax)
/* Return min/max range value associated with an edge. */
{
struct bridge *bridge = edge->val;
int minDist = bridge->minDistance;
int maxDist = bridge->maxDistance;
if (minDist == 0 && maxDist == BIGNUM)
    return FALSE;
else
    {
    *retMin = bridge->minDistance;
    *retMax = bridge->maxDistance;
    return TRUE;
    }
}


boolean canAddLineGraph(struct diGraph *graph, struct lineGraph *ml, 
	struct connectedComponent *ccList)
/* Return TRUE if can add line to graph.  Reverse direction of mcList if need be
 * according to orientation.  Return orientation. */
{
struct lineConnection *mc;
short lineOrientation = 0;
short oneOrientation;
struct dgNode *a, *b;
struct connectedComponent *cc;
int minDistance, maxDistance;

/* Don't bother with 0 and 1 element lists. */
if (ml->mcList == NULL || ml->mcList->next == NULL)
    return FALSE;

/* First check for conflicts in orientation. */
for (cc = ccList; cc != NULL; cc = cc->next)
    {
    if (!setCcOrientation(cc))
	{
	printAboutLineGraph("Line orientation conflict with graph and", logFile, ml);
	return FALSE;
	}
    }
if (!forceForwardOrientation(graph, ccList))
    return FALSE;


/* Check for cycles. */
if (wouldAddCycle(graph, ml->mcList))
    {
    printAboutLineGraph("Cycle conflict with graph and ", logFile, ml);
    return FALSE;
    }

/* Check that distances are consistent. */
if (ml->mcList != NULL)
    {
    for (mc = ml->mcList; mc->next != NULL; mc = mc->next)
	{
	lineConnectionPairDistanceRange(ml, mc, mc->next, &minDistance, &maxDistance);
	if (minDistance > maxDistance)
	    {
	    printAboutLineGraph("Impossible distance constraint", logFile, ml);
	    fprintf(logFile, "  minDistance %d, maxDistance %d\n", minDistance, maxDistance);
	    return FALSE;
	    }
	if (!dgAddedRangeConsistent(graph, 
	    mc->raft->node, mc->next->raft->node, minDistance, maxDistance, localFindEdgeRange))
	    {
	    printAboutLineGraph("Inconsistent distance constraint", logFile, ml);
	    fprintf(logFile, "  minDistance %d, maxDistance %d\n", minDistance, maxDistance);
	    return FALSE;
	    }
	}
    }
return TRUE;
}

void addLineGraph(struct diGraph *graph, struct lineGraph *ml, 
	struct connectedComponent *ccList)
/* Add line (already checked not to have cycle or orientation conflicts) to graph. */
{
struct lineConnection *a, *b;
int minDistance, maxDistance;
boolean ok;

for (a = ml->mcList; (b = a->next) != NULL; a = b)
    {
    lineConnectionPairDistanceRange(ml, a, b, &minDistance, &maxDistance);
    assert(minDistance <= maxDistance);
    addCable(graph, a->raft->node, a->name, a->orientation, 
         b->raft->node, b->name, b->orientation, 
         ml->cableType, minDistance, maxDistance);
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
	struct raft *raftList, struct lineGraph *ml)
/* Make up a connectedComponent list. */
{
struct connectedComponent *ccList = NULL, *cc;
struct lineConnection *mc;
struct lineConnectionRef *mcRef;
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

void printBridgeInfo(FILE *f, struct bridge *bridge)
/* Print some basic info on bridge. */
{
struct cable *cable;
int cableType;

fprintf(f, "min %d, max %d, ", bridge->minDistance, bridge->maxDistance);
for (cable = bridge->cableList; cable != NULL; cable = cable->next)
    {
    cableType = cable->cableType;
    if (cableType == ctMrna || cableType == ctEst)
	fprintf(f, "%s(%s),", cable->aName, cableTypeName(cableType));
    else
	fprintf(f, "%s&%s(%s),", cable->aName, cable->bName, cableTypeName(cableType));
    }
}


void ooDumpGraph(struct diGraph *dg, FILE *out)
/* Dump info on graph to output. */
{
struct dgNode *node;
struct dgConnection *con;
struct raft *raft;

for (node = dg->nodeList; node != NULL; node = node->next)
    {
    if (node->nextList == NULL && node->prevList == NULL)
	continue;
    raft = node->val;
    if (raft != NULL)
	{
	fprintf(out, "%s\t%c\n", node->name, orientChar(raft->orientation));
	}
    else
	{
	fprintf(out, "%s\n", node->name);
	}
    for (con = node->nextList; con != NULL; con = con->next)
	{
	struct dgEdge *edge = con->edgeOnList->val;
	struct bridge *bridge = edge->val;
	fprintf(out, "\t%s ", con->node->name);
	printBridgeInfo(out, bridge);
	fprintf(out, "\n");
	}
    }
}


void addToBargeGraph(struct diGraph *bargeGraph, struct lineGraph *ml)
/* Add lineGraph to bargeGraph.  Connect in both directions... */
{
struct barge *lastBarge = NULL, *barge;
struct lineConnection *mc;

if (ml->cableType == ctBigRaft)
    return;
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

void addBargeToRaftGraph(struct diGraph *raftGraph, struct barge *barge,
	struct oogClone **pLastClone)
/* Add a barge to the raft graph. */
{
struct bargeEl *bel;
struct oogClone *clone;
struct oogClone *lastClone = *pLastClone;
struct oogClone *newLastClone;
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
	{
	addDummyCable(raftGraph, lastNode, node);
	}
    lastNode = node;
    }
*pLastClone = clone;

if (lastClone != NULL)
    {
    clone = cepList->clone;
    addDummyCable(raftGraph, lastClone->endNode, clone->startNode);
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
int maxEndPos = -BIGNUM;
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
	    {
	    end = clone;
	    maxEndPos = endPos;
	    }
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
    addDummyCable(raftGraph, clone->startNode, raftNode);
    addDummyCable(raftGraph, raftNode, clone->endNode);
    }
else if (cloneBoundsInBarge(raft, &startClone, &endClone))
    {
    addDummyCable(raftGraph, startClone->startNode, raftNode);
    addDummyCable(raftGraph, raftNode, endClone->endNode);
    }
else
    {
    warn("Raft %d lies in multiple barges", raft->id);
    }
}

void makeRaftGraphSkeleton(struct diGraph *raftGraph, struct barge *bargeList, 
	struct raft *raftList, struct raft *fixedRaft)
/* Make skeleton of raftGraph - dummy nodes for ends of each clone and 
 * real nodes for each raft. Constrain raft nodes to lie between appropriate
 * clone end nodes. */
{
struct barge *barge;
struct raft *raft;
struct oogClone *lastClone = NULL;

for (barge = bargeList; barge != NULL; barge = barge->next)
    addBargeToRaftGraph(raftGraph, barge, &lastClone);
fixedRaft->node = dgAddNode(raftGraph, "fixed", fixedRaft);
for (raft = raftList; raft != NULL; raft = raft->next)
    addRaftToRaftGraph(raftGraph, raft);
slReverse(&raftGraph->nodeList);
}

void reverseMl(struct lineGraph *ml)
/* Reverse order of lineGraph. */
{
struct lineConnection *mc;

for (mc = ml->mcList; mc != NULL; mc = mc->next)
    mc->orientation = -mc->orientation;
slReverse(&ml->mcList);
}

void makeGraphs(struct raft *fixedRaft, struct raft *raftList, struct barge *bargeList, 
	struct lineGraph *mrnaLineList, struct diGraph **retRaftGraph, 
	struct diGraph **retBargeGraph)
/* Make a graphs out of mRNA lines. */
{
struct lineGraph *ml;
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
makeRaftGraphSkeleton(raftGraph, bargeList, raftList, fixedRaft);

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
	dumpLineGraph(ml, logFile);
	dumpCcList(ccList, logFile);
	if (canAddLineGraph(raftGraph, ml, ccList))
	    {
	    addLineGraph(raftGraph, ml, ccList);
	    addToBargeGraph(bargeGraph, ml);
	    dumpLineGraph(ml, logFile);
	    fprintf(logFile, "Graph after addLineGraph:\n");
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
	fprintf(logFile, "Couldn't add lineGraph\n");
	++conflictCount;
	}
    fprintf(logFile, "\n");
    if (dgHasCycles(raftGraph))
	errAbort("Cycles after makeGraphs!\n");
    }
fprintf(logFile, "%d lineGraphs ok, %d conflicted\n", okCount, conflictCount);
fprintf(stdout, "%d lineGraphs ok, %d conflicted\n", okCount, conflictCount);
*retRaftGraph = raftGraph;
*retBargeGraph = bargeGraph;
}

void calcGoodRange(struct oogFrag *frag)
/* Calculate start and end of high quality sequence. */
{
frag->endGood = frag->seq->size;
}

void loadClone(char *faName, struct oogClone *clone, struct hash *fragHash, struct oogFrag **pFragList)
/* Position fragments within a clone based on position in
 * .fa file. */
{
DNA *dna;
int sizeOne;
int totalSize = 0;
struct hashEl *hel;
struct oogFrag *frag;
char *commentLine;
struct qaSeq *qaList, *qa;
char *qaName = NULL;

clone->fragList = newDlList();
qaList = qaReadBoth(qaName, faName);
for (qa = qaList; qa != NULL; qa = qa->next)
    {
    if ((hel = hashLookup(fragHash, qa->name)) != NULL)
	{
	warn("Duplicate %s, ignoring all but first\n", qa->name);
	}
    else
	{
	AllocVar(frag);
	slAddHead(pFragList, frag);
	hel = hashAdd(fragHash, qa->name, frag);
	frag->name = hel->name;
	frag->seq = qa;
	frag->clone = clone;
	frag->size = qa->size;
	frag->clonePos = totalSize;
	dlAddValTail(clone->fragList, frag);
	calcGoodRange(frag);
	totalSize += qa->size;
	}
    }
clone->size = totalSize;
}

int cmpCloneMapPos(const void *va, const void *vb)
/* Compare two oogClones by mapPos. */
{
const struct oogClone *a = *((struct oogClone **)va);
const struct oogClone *b = *((struct oogClone **)vb);
return (a->mapPos - b->mapPos);
}

void loadAllClones(char *infoName, char *genoList, 
	struct hash *fragHash, struct hash *cloneHash, 
	struct oogFrag **pFragList, struct oogClone **pCloneList,
	char **retCtgName)
/* Consult info and geno list and associated fa files to build
 * default positions for fragments. */
{
struct lineFile *lf;
char *line;
int lineSize;
char *words[4];
int wordCount;
char **genoFiles;
int genoCount;
char *faFile;
char *acc;
int offset;
struct oogClone *clone;
struct oogFrag *frag;
char *gBuf;
int i;
char dir[256], name[128],extension[64];

/* Process info file. */
lf = lineFileOpen(infoName, TRUE);
if (!lineFileNext(lf, &line, &lineSize))
    errAbort("Empty info file %s\n", infoName);
if ((wordCount = chopLine(line, words)) < 2)
    errAbort("Bad header on %s\n", lf->fileName);
if (!sameWord("PLACED", words[1]))
    errAbort("%s doesn't have type PLACED on line 1\n", infoName);
*retCtgName = cloneString(words[0]);
while (lineFileNext(lf, &line, &lineSize))
    {
    if ((wordCount = chopLine(line, words)) < 4)
	errAbort("Bad info file format line %d of %s\n", lf->lineIx, lf->fileName);
    acc = words[0];
    if ((clone = hashFindVal(cloneHash, acc)) != NULL)
	{
	warn("Duplicate %s in %s, ignoring all but first", acc, lf->fileName);
	}
    else
	{
	int phase = lineFileNeedNum(lf, words, 2);
	if (phase > 0)
	    {
	    AllocVar(clone);
	    slAddHead(pCloneList, clone);
	    hashAddSaveName(cloneHash, acc, clone, &clone->name);
	    clone->mapPos = lineFileNeedNum(lf, words, 1) * 1000;
	    clone->phase = phase;
	    clone->flipTendency = lineFileNeedNum(lf, words, 3);
	    }
	}
    }
lineFileClose(&lf);

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
	clone = hashFindVal(cloneHash, acc);
	if (clone != NULL)
	    {
	    if (clone->fragList != NULL)
		warn("Duplicate %s in geno.lst, ignoring all but first", acc);
	    else
		{
		loadClone(faFile, clone, fragHash, pFragList);
		calcFragDefaultPositions(clone, clone->mapPos);
		}
	    }
	}
    else
	{
	errAbort("No sequence for %s", acc);
	}
    }

freeMem(gBuf);
slReverse(pFragList);
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
    posOne = frag->defaultPos;
    weightedTotal += sizeOne*posOne;
    }
raft->defaultPos = round(weightedTotal/totalBases) ;
if (raft->node != NULL)
    raft->node->priority = raft->defaultPos;
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

int calcRaftFlipTendency(struct raft *raft)
/* Return how badly raft would like to fit to better
 * accomodate default coordinates. */
{
/* raft->fragList is sorted already, and needs to be for
 * this to work. */
int diffTotal = 0;
int diffOne;
struct raftFrag *rf, *rfNext;
struct oogFrag *frag;
int diffInMap;

for (rf = raft->fragList; rf != NULL; rf = rf->next)
    {
    frag = rf->frag;
    diffTotal += frag->clone->flipTendency * rf->orientation;
    if ((rfNext = rf->next) != NULL)
	{
	diffInMap = rfNext->frag->defaultPos - frag->defaultPos;
	if (diffInMap > frag->size)
	    diffInMap = frag->size;
	else if (diffInMap < -frag->size)
	    diffInMap = -frag->size;
	diffTotal += diffInMap;
	}
    }
return diffTotal;
}

#ifdef OLD
int calcRaftFlipTendency(struct raft *raft)
/* Return how badly raft would like to fit to better
 * accomodate default coordinates. */
{
/* raft->fragList is sorted already, and needs to be for
 * this to work. */
int diffTotal = 0;
int diffOne;
struct raftFrag *rf, *rfNext;
struct oogFrag *aFrag, *bFrag;
int diffInMap;

for (rf = raft->fragList; rf != NULL; rf = rf->next)
    {
    if ((rfNext = rf->next) != NULL)
        {
        diffInMap = rfNext->frag->defaultPos - rf->frag->defaultPos;
        if (intAbs(diffInMap) < maxMapDeviation - rf->frag->size)
            {
            diffTotal += diffInMap;
            }
        else
            {
            logIt("  Unreasonable distance in map %d\n", diffInMap /* uglyf */);
            }
        }
    }
return diffTotal;
}
#endif /* OLD */


void setRaftFlipTendencies(struct raft *raftList)
/* Set flipTendencies for all rafts. */
{
struct raft *raft;
logIt("Raft flip tendencies\n");
for (raft = raftList; raft != NULL; raft = raft->next)
    {
    raft->flipTendency = calcRaftFlipTendency(raft);
    logIt("  %2d %d\n", raft->id, raft->flipTendency);
    }
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
	    oneFlipTendency = raft->flipTendency * raft->orientation;
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
	boolean ok;
	fprintf(logFile, "Flipping tendency = %d\n", flipTendency);
	ok = flipConnectedRafts(graph, ccList);
	fprintf(logFile, "Flipped %s\n", (ok ? "ok" : "not ok"));
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
    gtContig = 3,       /* Gap between contigs. */
    gtSplitFin = 4      /* Gap between pieces of finished clone. */
    };
char *gapTypeNames[] = { "?", "fragment", "clone", "contig", "split_finished"};

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
int bestScore = -BIGNUM;
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
int startPos = BIGNUM;
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
	struct hash *fragHash, struct hash *splitFinCloneHash)
/* Return the best way through raft given orientation. */
{
int raftSize;
struct raftFrag **bestFrag, *rf;
int *bestScore;
struct raftFragRef *rfrList = NULL, *rfr;
struct goldenPath *gp;

fprintf(logFile, "pathThroughRaft %d at %d orientation %d\n", 
	raft->id, raft->defaultPos, raft->orientation);
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

int writeGpFa(FILE *f, char *ctgName, struct goldenPath *goldenPath)
/* Save out fa with golden path. */
{
struct goldenPath *gp;

faLinePos = faTotalSize = 0;
fprintf(f, ">%s\n", ctgName);
for (gp = goldenPath; gp != NULL; gp = gp->next)
    {
    struct oogFrag *frag = gp->frag;
    int size = gp->fragEnd - gp->fragStart;
    int i;
    if (frag != NULL)
	{
	struct qaSeq *seq = frag->seq;
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
return faTotalSize;
}

void makeGoldenFa(char *faOutName, char *ctgName, struct goldenPath *goldenPath)
/* Save out fa with golden path. */
{
FILE *f = mustOpen(faOutName, "w");
int faTotalSize = writeGpFa(f, ctgName, goldenPath);
fclose(f);
printf("Wrote %d total bases to %s\n", faTotalSize, faOutName);
}

void makeCopperFa(char *fileName, struct raftPath *raftPath)
/* Write out fa file with rafts as separate records, no N's */
{
FILE *f = mustOpen(fileName, "w");
struct raftPath *rp;
char name[16];

for (rp = raftPath; rp != NULL; rp = rp->next)
    {
    sprintf(name, "raft%d", rp->raft->id);
    writeGpFa(f, name, rp->gpList);
    }
fclose(f);
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

void edgesIntoBarges(struct barge *bargeList, struct oogEdge **pEdgeList)
/* edgesIntoBarges - move edges from master edge list to barge-specific
 * edge lists.  Leave edges that cross between barges on *pEdgeList. */
{
struct oogEdge *oldList = *pEdgeList, *bargeCrossers = NULL, *edge, *nextEdge;
struct barge *barge;
struct oogClone *clone;
int crossCount = 0;

for (edge = oldList; edge != NULL; edge = nextEdge)
    {
    nextEdge = edge->next;
    barge = edge->aSide->clone->barge;
    assert(barge != NULL);
    if (barge == edge->bSide->clone->barge)
	{
	slAddHead(&barge->selfEdges, edge);
	}
    else
	{
	slAddHead(&bargeCrossers, edge);
	++crossCount;
	}
    }
status("eliminated %d raft edges that cross barges\n", crossCount);
for (barge = bargeList; barge != NULL; barge = barge->next)
    slReverse(&barge->selfEdges);
slReverse(&bargeCrossers);
*pEdgeList = bargeCrossers;
}

void testFakePsl(struct psl *pslList, char *fileName)
/* Test that fake psl to ff works. */
{
struct ffAli *ff;
struct psl *psl, *fakePsl;
FILE *f = mustOpen(fileName, "w");

pslWriteHead(f);
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    ff = pslToFakeFfAli(psl, NULL, NULL);
    fakePsl = pslFromFakeFfAli(ff, NULL, NULL, psl->strand[0], psl->qName, psl->qSize, psl->tName, psl->tSize);
    fakePsl->match = psl->match;
    fakePsl->misMatch = psl->misMatch;
    fakePsl->repMatch = psl->repMatch;
    fakePsl->nCount = psl->nCount;
    fakePsl->qNumInsert = psl->qNumInsert;
    fakePsl->qBaseInsert = psl->qBaseInsert;
    fakePsl->tNumInsert = psl->tNumInsert;
    fakePsl->tBaseInsert = psl->tBaseInsert;
    fakePsl->strand[0] = psl->strand[0];
    pslTabOut(fakePsl, f);
    pslFree(&fakePsl);
    ffFreeAli(&ff);
    }
fclose(f);
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
static char ocpFileName[512];  /* Where to read overlapping clone pairs. */
static char endsName[512];
static char genoListName[512];
static char fragMapName[512];
static char raftOrderName[512];
static char goldName[512];
static char faOutName[512];
static char bacPairName[512];
static char bacPslName[512];
static char readPairName[512];
static char readPslName[512];
static char glName[512];
static char layoutName[512];
static char fragChainsName[512];
static char splitFinName[512];
struct lineGraph *mlList = NULL, *ml;
struct oogClone *cloneList = NULL;
struct splitClone *splitFinCloneList = NULL;
struct oogFrag *fragList = NULL;
struct oogFrag *ntOrigFragList = NULL;  /* Frags from NT clones (not NT psuedo clones) */
struct hash *cloneHash = newHash(9);	/* Hash of all oogClones (including psuedos) */
struct hash *fragHash = newHash(12);    /* Hash of all oogFrags (including psuedos) */
struct hash *ocpHash = newHash(0);      /* Overlapping clone pairs. */
struct hash *mapOverlapHash = newHash(0);   /* 'Real' overlap according to map. */
struct hash *bacPairHash = newHash(0);  /* Bac end pairs. */
struct hash *readPairHash = newHash(0); /* Paired plasmid read pairs. */
struct hash *splitFinCloneHash = newHash(6);   /* Finished clones in more than one fragment. */
struct hash *splitFinFragHash = newHash(6);    /* Finished clones in more than one fragment. */
struct overlappingClonePair *ocpList = NULL;
struct pair *readPairList = NULL;
struct pair *bacPairList = NULL;
struct raft *raftList = NULL, *raft, *oneRaftSet;
struct raft *fixedRaft = makeFixedRaft();
struct barge *bargeList = NULL, *barge;
struct psl *selfPslList = NULL;
FILE *graphOut; /* File to write graph to. */
FILE *raftOut = NULL;  /* File to write raft to. */
int raftIx;
struct diGraph *raftGraph;
struct diGraph *bargeGraph;
int raftTotalSize = 0;
int contigPos = 0;
int raftCount = 0;
struct dgNodeRef *orderList = NULL, *orderEl;
struct goldenPath *goldenPath = NULL, *gp;
struct raftPath *raftPath = NULL, *rp;
char *ctgName;
boolean literalMap = FALSE;

/* Make input file names. */
sprintf(selfPslName, "%s/self.psl", inDir);
sprintf(mrnaPslName, "%s/mrna.psl", inDir);
sprintf(estPslName, "%s/est.psl", inDir);
sprintf(infoName, "%s/info.mm", inDir);
sprintf(endsName, "%s/mmEnds", inDir);
sprintf(fragMapName, "%s/fragMap.%d", inDir, version);
sprintf(raftOrderName, "%s/raftOrder.%d", inDir, version);
sprintf(genoListName, "%s/geno.lst", inDir);
sprintf(bacPairName, "%s/bacEndPairs", inDir);
sprintf(bacPslName, "%s/bacEnd.psl", inDir);
sprintf(readPairName, "%s/readPairs", inDir);
sprintf(readPslName, "%s/pairedReads.psl", inDir);
sprintf(splitFinName, "%s/splitFin", inDir);
sprintf(fragChainsName, "%s/fragChains", inDir);

/* Make output file names. */
sprintf(oogLogName, "%s/ooGreedy.log", inDir, version);
sprintf(ocpFileName, "%s/cloneOverlap", inDir, version);
sprintf(bargeOutName, "%s/barge.%d", inDir, version);
sprintf(raftOutName, "%s/raft.%d", inDir, version);
sprintf(graphOutName, "%s/graph.%d", inDir, version);
sprintf(glName, "%s/ooGreedy.%d.gl.noNt", inDir, version);
sprintf(layoutName, "%s/gl.%d", inDir, version);
sprintf(raftPslName, "%s/raft.psl", inDir);
sprintf(contaminatedName, "%s/contaminated.%d", inDir, version);
sprintf(goldName, "%s/gold.%d.noNt", inDir, version);

/* Open log file file. */
logFile = mustOpen(oogLogName, "w");
pushWarnHandler(warnHandler);

/* Let user know we're alive. */
status("ooGreedy version %d on %s\n", version, inDir);

/* Load in special case list of 'finished' clones that are in
 * more than one piece after contamination removed. */
if (fileExists(splitFinName))
    {
    splitFinCloneList = readSplitFin(splitFinName, splitFinCloneHash, splitFinFragHash);
    status("Got %d split finished clones\n", slCount(splitFinCloneList));
    }

/* Load in clones and their map positions. */
loadAllClones(infoName, genoListName, fragHash, 
	cloneHash, &fragList, &cloneList, &ctgName);
sprintf(faOutName, "%s/%s.fa", inDir, ctgName);	/* Don't know ctgName until here. */
slSort(&cloneList, cmpCloneMapPos);
status( "Got %d clones %d frags\n", 
	slCount(cloneList), slCount(fragList));

/* Load in list of overlapping clone pairs. */
loadCloneOverlap(ocpFileName, cloneHash, &ocpList, ocpHash);

/* Read map. Make barges.  Figure out clones that overlap and that are
 * completely enclosed according to the map. */
bargeList = loadBarges(endsName, cloneList, cloneHash);
status("Read %d barges from %s\n", slCount(bargeList), endsName);
figureMapOverlaps(bargeList, mapOverlapHash);
figureMapEnclosures(bargeList);

/* Load up and process self edge list, converting psl's to edges in barges. */
    {
    selfPslList = pslLoadAll(selfPslName);
    status("%d self psls\n", slCount(selfPslList));
    processSelf(selfPslList, bargeList,  &cloneList, &fragList, cloneHash, fragHash,
    	mapOverlapHash);
    }

/* Make rafts inside of barges. */
raftOut = mustOpen(raftOutName, "w");
raftPsl = mustOpen(raftPslName, "w");
pslWriteHead(raftPsl);
for (barge = bargeList; barge != NULL; barge = barge->next)
    {
    oneRaftSet = makeRafts(fragList, barge->selfEdges, TRUE);
    raftList = slCat(raftList, oneRaftSet);
    }
fprintf(raftOut, "ooGreedy version %d found %d rafts (%d of %d frags) in %s\n\n", 
	version, slCount(raftList), raftedFragCount(fragList),
	slCount(fragList), selfPslName);
status("found %d rafts (%d of %d frags) in %s\n", 
	slCount(raftList), raftedFragCount(fragList),
	slCount(fragList), selfPslName);

makeSingletonRafts(fragList, &raftList);
linkRaftsToBarges(raftList);

/* Figure out approximate default positions.  Will
 * Recalculate these again after have bridging info.
 * Approximate positions help us determine if a BAC
 * end or other bridge could be reasonable. */
calcPositions(bargeList, fragList, NULL);
setRaftDefaultPositions(raftList);

setRaftFlipTendencies(raftList);

/* Dump rafts to log file for EZ debugging. */
for (raft = raftList; raft != NULL; raft = raft->next)
    {
    dumpRaft(raft, logFile);
    logIt("\n");
    }


/* Read in rna, est, paired reads and bac end pair lists and
 * merge them into one big list sorted by score. */  
    {
    int lCount = 0, mCount = 0, eCount = 0, bCount = 0, pCount = 0, fCount = 0;
    struct lineGraph *largeRaftLineList;  /* Map based orientation of large rafts. */
    struct lineGraph *mrnaLineList;
    struct lineGraph *estLineList;
    struct lineGraph *bacEndLineList;
    struct lineGraph *pairedReadLineList;
    struct lineGraph *fragChainsLineList;

    /* Large fragment map based orientation. */
    largeRaftLineList = makeLargeRaftLines(fixedRaft, raftList);
    lCount = slCount(largeRaftLineList);
    mlList = slCat(mlList, largeRaftLineList);

    /* mRNA */
    mrnaLineList = makeMrnaLines(mrnaPslName, fragHash, ctMrna, 25, 
    	splitFinFragHash);
    mCount = slCount(mrnaLineList);
    mlList = slCat(mlList, mrnaLineList);

    /* paired reads */
    readPairList = loadRangedPairs(readPairName, readPairHash);
    pairedReadLineList = makePairedLines(readPairList, readPslName, 
    	fragHash, readPairHash, ctReadPair, 
	splitFinFragHash, maxMapDeviation+10000);
    pCount = slCount(pairedReadLineList);
    mlList = slCat(mlList, pairedReadLineList);
    
    /* Fragment chains. */
    fragChainsLineList = makeFragChains(fragChainsName, fragHash);
    fCount = slCount(fragChainsLineList);
    mlList = slCat(mlList, fragChainsLineList);

    /* ESTs */
    estLineList = makeMrnaLines(estPslName, fragHash, ctEst, -25, 
    	splitFinFragHash);
    eCount = slCount(estLineList);
    mlList = slCat(mlList, estLineList);

    /* BAC end pairs. */
    bacPairList = loadUnrangedPairs(bacPairName, bacPairHash, 40000, 400000) ;
    bacEndLineList = makePairedLines(bacPairList, bacPslName, 
    	fragHash, bacPairHash, ctBacEndPair, 
	splitFinFragHash, maxMapDeviation+100000);
    bCount = slCount(bacEndLineList);
    mlList = slCat(mlList, bacEndLineList);

    /* Sort and report. */
    slSort(&mlList, lineGraphCmp);
    status("Got %d map based orientations of large rafts\n", lCount);
    status("Got %d mRNA, %d paired reads, %d fragment chains, %d EST, %d BAC end pair bridges\n",
	mCount, pCount, fCount, eCount, bCount);
    }


fprintf(logFile, "\nList of lineGraphs:\n");
for (ml = mlList; ml != NULL; ml = ml->next)
    dumpLineGraph(ml, logFile);
makeGraphs(fixedRaft, raftList, bargeList, mlList, &raftGraph, &bargeGraph);
status("Made bridge graphs\n");

calcPositions(bargeList, fragList, bargeGraph);
setRaftDefaultPositions(raftList);
slSort(&fragList, cmpFragDefaultPos);
writeFragMap(fragMapName, fragList);
for (raft = raftList; raft != NULL; raft = raft->next)
    {
    normalizeRaft(raft);
    dumpRaft(raft, raftOut);
    fprintf(raftOut, "\n");
    }
carefulClose(&raftOut);
status("Updated default positions\n");

flipNearDefaults(raftGraph);
graphOut = mustOpen(graphOutName, "w");
fprintf(graphOut, "mRNA/BAC ends graph by ooGreedy version %d\n\n", version); 
dgConnectedComponentsWithVals(raftGraph);
ooDumpGraph(raftGraph, graphOut);
carefulClose(&graphOut);
status("Flipped and saved graphs.\n");

saveBarges(bargeOutName, bargeList, bargeGraph, ocpHash);

orderList = orderRafts(raftGraph);
status("Got %d ordered rafts\n", slCount(orderList));

	{
	int count = 0;
	fprintf(logFile, "Order of rafts:\n");
	fprintf(logFile, "\t");
	for (orderEl = orderList; orderEl != NULL; orderEl = orderEl->next)
	    {
	    raft = orderEl->node->val;
	    if (raft != NULL && raft->id != srOriented)
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
    if (raft != NULL && raft->id != srOriented)
	{
	AllocVar(rp);
	rp->gpList = pathThroughRaft(raft, fragHash, splitFinCloneHash);
	rp->raft = raft;
	rp->size = goldenPathSize(rp->gpList);
	raftTotalSize += rp->size;
	++raftCount;
	slAddHead(&raftPath, rp);
	}
    }
slReverse(&raftPath);

#ifdef SOMETIMES
	{
	char fileName[512];
	status("Writing copper path\n");
	sprintf(fileName, "%s/copper.fa", inDir);
	makeCopperFa(fileName, raftPath);
	}
#endif /* SOMETIMES */

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
status("Found golden path\n");

/* Save results to disk. */
saveGoldenPath(goldName, ctgName, goldenPath);
makeGoldenFa(faOutName, ctgName, goldenPath);
saveGl(glName, raftPath);
saveLayout(layoutName, raftPath, raftGraph);
status("\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
if (argc != 2)
    {
    usage();
    }
ooGreedy(argv[1]);
return 0;
}
