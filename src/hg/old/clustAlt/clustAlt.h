struct clusterInput
/* This holds the input for the core clustering algorithm. */
    {
    int seqCount;		/* Count of target sequences. */
    struct dnaSeq *seqList;	/* Linked list of target sequences. */
    struct dnaSeq **seqArray;	/* Array of same target sequences. */
    struct cdaAli *cdaList;     /* List of alignments. */
    struct clonePair *pairList; /* List of EST pairs. */
    };

struct clonePair
/* Info about an EST pair. This is stored in the val field of
 * the hash clusterInput.pairHash. */
    {
    struct clonePair *next;	/* Next in list. */
    struct cdaAli *p5, *p3;	/* Start/end of pair. */
    int chromIx;                /* Chromosome index. */
    int tStart,tEnd;		/* Positions within target sequence. */
    char strand;                /* Strand this is on. */
    };

 enum vertexType
 /* Classifies a vertex.  */
    {
    softStart, hardStart, softEnd, hardEnd, unused,
    };

struct vertex
/* A vertex in our gene graph. */
    {
    int position;	/* Where it is in genomic sequence. */
    UBYTE type;		/* vertex type. */
    };

struct cdaRef 
/* Holds a reference to a cda */
    {
    struct cdaRef *next;  /* Next in list. */
    struct cdaAli *cda;   /* Cdna alignment info. */
    };

struct geneGraph
/* A graph that represents the alternative splicing paths of a gene. */
    {
    struct vertex *vertices; /* Splice sites or soft start/ends. */
    int vertexCount;         /* Vertex count. */
    bool **edgeMatrix;	     /* Adjacency matrix for directed graph. */
    };

struct denseAliInfo
/* mRNA alignment info as it pertains to graph generator. */
    {
    struct denseAliInfo *next; /* Next in list. */
    struct vertex *vertices;   /* Splice sites or soft start/ends. */
    int vertexCount;           /* Vertex count. */
    };

struct ggInput
/* Input structure for gene grapher */
    {
    struct ggInput *next;               /* Next in list. */
    struct cdaRef *cdaRefList;          /* Full cDNA alignments. */
    struct denseAliInfo *mrnaList;      /* List of compact cDNA alignments. */
    int orientation;			/* +1 or -1 strand of genomic DNA. */
    int start,end;                      /* Position in genomic DNA. */
    };

struct clusterInput *bacInputFromHgap(char *bacAcc);
/* Read in clustering input from hgap database. */
