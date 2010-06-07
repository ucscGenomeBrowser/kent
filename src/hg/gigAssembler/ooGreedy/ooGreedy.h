/* ooGreedy - A greedy method for doing ordering and orienting
 * of contigs. */
#ifndef OOGREEDY_H
#define OOGREEDY_H

void logg(char *format, ...);
/* Print to log file. */

void status(char *format, ...);
/* Print status message to stdout and to log file. */

/* Make uglyf debugging statements go to log file too. */
#undef uglyf
#define uglyf status


enum htgPhase
    {
    htgSurvey = 0,
    htgDraft = 1,
    htgOrdered = 2,
    htgFinished = 3,
    };

struct barge
/* A barge - a set of welded together clones. */
    {
    struct barge *next;	    /* Next in list. */
    int id;                 /* Barge id. */
    struct dlList *cloneList; /* List of clones. */
    struct oogEdge *selfEdges;  /* List of edges inside barge. */
    struct dgNode *graphNode;        /* Node in barge graph. */
    int offset;                /* Offset within contig. */
    int mapPos;                /* Position within map. */
    int size;                  /* Total size. */
    struct clonePair *usedPairs;	/* Pairs used in building barge. */
    struct dlNode *listNode;	/* Node in doubly-linked list of barges. */
    };

#ifdef OLD
struct bargeEl
/* A clone that's part of a barge. */
    {
    struct bargeEl *next;   /* Next in list. */
    struct oogClone *clone; /* Clone. */
//    struct dlNode *dlNode;  /* List node ref. */
    int offset;             /* Offset within barge. */
//    int abOverlap;	    /* Overlap between this and next clone. */
//    int axOverlap;          /* Overlap between this and clone we're positioning (temp) */
//    struct bargeEl *enclosed;  /* Clone enclosed by another?. */
    };
#endif

struct oogClone
/* Describes a clone (BAC) */
    {
    struct oogClone *next;     /* Next in clone list. */
    char *name;                /* Name of clone. */
    int mapPos;                /* Relative position in contig according to map. */
    int bargePos;              /* Relative position in barge in bases. */
    int contigPos;	       /* Relative position in contig in bases. */
    int size;                  /* Size in bases. */
    struct barge *barge;       /* Barge this is part of. */
    struct dlNode *bargeNode; /* Node in barge's cloneList. */
    int maxOverlapPpt;		/* Max overlap parts per thousand. */
    struct oogClone *maxOverlapClone; /* Clone with max overlap. */
    struct seqSharer *sharers;  /* List of overlapping clones. */
    boolean isEnclosed;		/* True if enclosed at current enclosure threshold. */
    struct bargeEl *bel;       /* Point back to barge el to get position... (obsolete?) */
    struct dgNode *startNode;  /* Start node in raft graph. */
    struct dgNode *endNode;    /* End node in raft graph. */
    enum htgPhase phase;       /* HTG phase. */
    bool hitFlag;              /* Used by raftsShareClones. (obsolete?) */
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

struct raftFrag
/* An oog frag with a little extra info - puts a fragment on a raft. */
    {
    struct raftFrag *next;      /* Next in list. */
    int raftOffset;             /* Offset from frag to raft coordinates. */
    short orientation;		/* Orientation of frag in raft. */
    struct oogFrag *frag;   /* Frag itself. */
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

struct clonePair
/* Info on a pair of clones which overlaps. */
    {
    struct clonePair *next;  /* Next in list. */
    struct oogClone *a, *b;             /* Pair of clones. */
    int overlap;                        /* Size of overlap. */
    int aUses, bUses;		/* Number of times a,b used. */
    float score;		/* Score function for greedy algorithm. */
    };

struct seqSharer
/* A clone that shares some sequence. */
    {
    struct seqSharer  *next;	/* Next in list. */
    struct oogClone *clone;	/* Clone. */
    struct clonePair *pair;     /* Overlap info. */
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

struct mrnaPsl
/* psl's associated with one piece of mrna. */
    {
    struct mrnaPsl *next;	/* Next in list. */
    char *mrnaName;             /* Name of mrna. */
    struct psl *pslList;        /* List of .psls. */
    };

struct fragPair
/* Keep track of a pair. */
    {
    struct fragPair *next;  /* next in list */
    char *a;		/* First in pair. Allocated in hash. */
    char *b;            /* Second in pair. Allocated in hash. */
    struct psl *aPslList; /* List of all psl's referencing a.  Owned here. */
    struct psl *bPslList; /* List of all psl's referencing b.  Owned here. */
    };

struct aliFilter
/* Info on how to filter an alignment. */
    {
    int maxTail;      /* Maximum length of non-aligning tails at end. */
    int maxBad;       /* Maximum mismatch/insert ratio in parts per thousand. */
    int minUnique;    /* Minimum number of non-repeat masked matches. */
    int minMatch;     /* Minimum number of total matches. */
    int minFragSize;  /* Minimum size of fragment. */
    };

void findPslTails(struct psl *psl, int *retStartTail, int *retEndTail);
/* Find the length of "tails" (rather than extensions) implied by psl. */


#endif /* OOGREEDY_H */
