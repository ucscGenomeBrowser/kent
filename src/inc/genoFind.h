/* genoFind.h - Interface to modules for fast finding of sequence
 * matches. Compile with -DGFSERVER_HUGE defined to get 64-bit indexes.
 */
/* Copyright 2001-2002 Jim Kent.  All rights reserved. */

#ifndef GENOFIND_H
#define GENOFIND_H

#ifndef DNASEQ_H
#include "dnaseq.h"
#endif

#ifndef FUZZYFIND_H
#include "fuzzyFind.h"
#endif

#ifndef HASH_H
#include "hash.h"
#endif

#ifndef ALITYPE_H
#include "aliType.h"
#endif

#ifndef LOCALMEM_H
#include "localmem.h"
#endif 

#ifndef BITS_H
#include "bits.h"
#endif

#ifndef AXT_H
#include "axt.h"
#endif

/* for use in deciding what type of error message to output, typical
 *  network connection timeout is 120 seconds, if elapsed time to this
 *  error exit is > NET_TIMEOUT_MS, something is taking too long.
 *  On the other hand, if the QUICKEXIT time of < 500 ms is noted, then
 *  something else is wrong about the connection.
 *  (these are used in gfClient and hgBlat for dynamic blat server messages)
 */
#define NET_TIMEOUT_MS	110000
#define NET_QUICKEXIT_MS	500

struct gfConnection
/* connection to a gfServer.  This supports reuse of the connection for dynamic 
 * servers and reopening a connection for static server. */
{
    
    int fd;  // socket descriptor, -1 if closed
    char *hostName;   // need when reconnecting
    int port;
    boolean isDynamic;  // is this a dynamic server?
    char *genome;   // genome name for dynamic server
    char *genomeDataDir; // genome data directory for dynamic server
};

enum gfConstants {
    gfMinMatch = 2,
    gfMaxGap = 2,
    gfTileSize = 11,
    gfMaxTileUse = 1024,
    gfPepMaxTileUse = 30000,
};

#ifdef GFSERVER_HUGE
typedef bits64 gfOffset;  /* offset/size of genome sequences */
#define GFINDEX_BITS 64
#define GFOFFSET_FMT "%lld"
#else
typedef bits32 gfOffset;  /* offset/size of genome sequences */
#define GFINDEX_BITS 32
#define GFOFFSET_FMT "%d"
#endif

struct gfSeqSource
/* Where a block of sequence comes from. */
    {
    struct gfSeqSource *next;
    char *fileName;	/* Name of file. */
    bioSeq *seq;	/* Sequences.  Usually either this or fileName is NULL. */
    gfOffset start,end;	/* Position within merged sequence. */
    Bits *maskedBits;	/* If non-null contains repeat-masking info. */
    };

struct gfHit
/* A genoFind hit. */
   {
   struct gfHit *next;
   gfOffset qStart;		/* Where it hits in query. */
   gfOffset tStart;		/* Where it hits in target. */
   gfOffset diagonal;		/* tStart + qSize - qStart. */
   };

/* gfHits are free'd with simple freeMem or slFreeList. */

struct gfClump
/* A clump of hits. */
/* Note: for clumps from regular (blat) queries, tStart and tEnd include 
 * target->start, but for clumps from gfPcrClumps(), tStart and tEnd have 
 * already had target->start subtracted.  So tStart and tEnd in PCR clumps 
 * are relative to that target sequence (not the collection of all target 
 * sequences). */
    {
    struct gfClump *next;	/* Next clump. */
    gfOffset qStart, qEnd;	/* Position in query. */
    struct gfSeqSource *target;	/* Target source sequence. */
    gfOffset tStart, tEnd;	/* Position in target. */
    int hitCount;		/* Number of hits. */
    struct gfHit *hitList;	/* List of hits. Not allocated here. */
    int queryCoverage;		/* Number of bases covered in query (thx AG!) */
    };

void gfClumpFree(struct gfClump **pClump);
/* Free a single clump. */

void gfClumpFreeList(struct gfClump **pList);
/* Free a list of dynamically allocated gfClump's */

typedef bits16 endListPart;  // endList structure (below) is packed into 3 or 5 16-bit values


struct genoFind
/* An index of all K-mers in the genome.  
 * WARNING: MUST MODIFY CODE TO STORE/LOAD INDEX TO FILES IF THIS STRUCTURE IS
 * MODIFIED!!!
 *
 * The endList structure in the index is a more complex list for each N-mer.
 * Each row of endList width is in listSizes.  Each entry packed last few
 * letters of the tile.  The next two are the offset in the genome.  This
 * would be a struct but that would take 8 bytes instead of 6, or nearly an
 * extra gigabyte of RAM for the 32-bit index.
 * 
 * The data is packed into an array to optimized. layout and functions are used
 * to access it.
 *     index   lastLetters   genomeOffset  entrySize
 *     32-bit  16-bits       32-bits       48-bits
 *     64-bit  16-bits       64-bits       80-bits
 */
{
    boolean isMapped;                    /* is this a mapped file? */    int maxPat;                          /* Max # of times pattern can occur
                                          * before it is ignored. */
    int minMatch;                        /* Minimum number of tile hits needed
                                          * to trigger a clump hit. */
    int maxGap;                          /* Max gap between tiles in a clump. */
    int tileSize;			 /* Size of each N-mer. */
    int stepSize;			 /* Spacing between N-mers. */
    int tileSpaceSize;                   /* Number of N-mer values. */
    int tileMask;			 /* 1-s for each N-mer. */
    int sourceCount;			 /* Count of source files. */
    bool isPep;			 	 /* Is a peptide. */
    bool allowOneMismatch;		 /* Allow a single mismatch? */
    bool noSimpRepMask;			 /* Dis-Allow simple repeat masking. */
    int segSize;			 /* Index is segmented if non-zero. */
    gfOffset totalSeqSize;		 /* Total size of all sequences. */
    struct gfSeqSource *sources;         /* List of sequence sources. */
    bits32 *listSizes;                   /* Size of list for each N-mer */
    void *allocated;                     /* Storage space for all lists. */
    gfOffset **lists;                    /* A list for each N-mer. Used if
                                          * if segSize is zero. */
    endListPart **endLists;              /* A more complex list for each N-mer.
                                          * Used if sequence is non-zero. */
    };


void genoFindFree(struct genoFind **pGenoFind);
/* Free up a genoFind index. */

struct gfSeqSource *gfFindNamedSource(struct genoFind *gf, char *name);
/* Find target of given name.  Return NULL if none. */

struct genoFindIndex
/* container for genoFind indexes, sorting either an untranslated index on six translated indexes.
 * these can be created in memory or saved to a file to quickly mmap */
{
    void *memMapped;     /* memory mapped if non-NULL, with amount allocated */
    size_t memLength;
    bool isTrans;        /* is this translated? */
    bool noSimpRepMask;  /* Suppresses simple repeat masking for very small genomes */
    struct genoFind *untransGf;
    struct genoFind *transGf[2][3];
};

struct genoFindIndex* genoFindIndexBuild(int fileCount, char *seqFiles[],
                                         int minMatch, int maxGap, int tileSize,
                                         int repMatch, boolean doTrans, char *oocFile,
                                         boolean allowOneMismatch, boolean doMask,
                                         int stepSize, boolean noSimpRepMask);
/* build a untranslated or translated index */

void genoFindIndexFree(struct genoFindIndex **pGfIdx);
/* free a genoFindIndex */

void genoFindIndexWrite(struct genoFindIndex *gfIdx, char *fileName);
/* write index to file that can be mapped */

struct genoFindIndex* genoFindIndexLoad(char *fileName, boolean isTrans);
/* load indexes from file. */


/* ---  Stuff for saving results ---- */


struct gfOutput
/* A polymorphic object to help us write many file types. */
    {
    struct gfOutput *next;
    void *data;		/* Type-specific data pointer.  Must be freeMem'able */
    void (*out)(char *chromName, int chromSize, int chromOffset,
	    struct ffAli *ali, bioSeq *tSeq, struct hash *t3Hash, bioSeq *qSeq, 
	    boolean qIsRc, boolean tIsRc,
	    enum ffStringency stringency, int minMatch, struct gfOutput *out);
    /* This is the type of a client provided function to save an alignment. 
     * The parameters are:
     *     chromName - name of target (aka genomic or database) sequence.
     *     chromSize - size of target sequence.
     *     chromOffset - offset of genoSequence in target.
     *     ffAli - alignment with pointers into tSeq/qSeq or in
     *             translated target case, into t3Hash.
     *     tSeq - part of target sequence in normal case.   In translated
     *             target case look at t3Hash instead.
     *     t3Hash - used only in translated target case.  A hash keyed by
     *             target sequence name with values *lists* of trans3 structures.
     *             This hash can be searched to find both the translated and
     *             untranslated versions of the bits of the target that are in 
     *             memory.  (You can assume at this point all parts needed for
     *             output are indeed in memory.)
     *     qSeq - query sequence (this isn't segmented at all). 
     *     isRc - True if query is reverse complemented.
     *     stringency - ffCdna, etc.  I'm hoping to move this elsewhere.
     *     minMatch - minimum score to output.  Also should be moved elsewhere.
     *     outputData - custom data for specific output function.
     * The interface is a bit complex - partly from the demands of translated
     * output, and partly from trying not to have the entire target sequence in
     * memory.
     */
    void (*queryOut)(struct gfOutput *out, FILE *f); 
    /* Called for each query */

    void (*fileHead)(struct gfOutput *out, FILE *f);
    /* Write file header if any */

    boolean reportTargetStrand; /* Report target as well as query strand? */
    struct hash *maskHash;	/* associates target sequence name and mask. */
    int minGood;		/* Minimum sequence identity in thousandths. */
    boolean qIsProt;		/* Query is peptide. */
    boolean tIsProt;		/* Target is peptide. */
    int queryIx;		/* Index of query */
    boolean includeTargetFile;	/* Prefix file: to target sequence name. */
    };

struct gfOutput *gfOutputAny(char *format, 
	int goodPpt, boolean qIsProt, boolean tIsProt, 
	boolean noHead, char *databaseName,
	int databaseSeqCount, double databaseLetters,
	double minIdentity, FILE *f);
/* Initialize output in a variety of formats in file or memory. 
 * Parameters:
 *    format - either 'psl', 'pslx', 'blast', 'wublast', 'axt'
 *    goodPpt - minimum identity of alignments to output in parts per thousand
 *    qIsProt - true if query side is a protein.
 *    tIsProt - true if target (database) side is a protein.
 *    noHead - if true suppress header in psl/pslx output.
 *    databaseName - name of database.  Only used for blast output
 *    databaseSeq - number of sequences in database - only for blast
 *    databaseLetters - number of bases/aas in database - only blast
 *    FILE *f - file.  
 */

struct gfOutput *gfOutputPsl(int goodPpt, 
	boolean qIsProt, boolean tIsProt, FILE *f, 
	boolean saveSeq, boolean noHead);
/* Set up psl/pslx output */

struct gfOutput *gfOutputAxt(int goodPpt, boolean qIsProt, 
	boolean tIsProt, FILE *f);
/* Setup output for axt format. */

struct gfOutput *gfOutputAxtMem(int goodPpt, boolean qIsProt, 
	boolean tIsProt);
/* Setup output for in memory axt output. */

struct gfOutput *gfOutputBlast(int goodPpt, 
	boolean qIsProt, boolean tIsProt, 
	char *databaseName, int databaseSeqCount, double databaseLetters,
	char *blastType, /* blast, blast8, blast9, wublast, or xml */
	double minIdentity, FILE *f);
/* Setup output for blast/wublast format. */

void gfOutputQuery(struct gfOutput *out, FILE *f);
/* Finish writing out results for a query to file. */

void gfOutputHead(struct gfOutput *out, FILE *f);
/* Write out header if any. */

void gfOutputFree(struct gfOutput **pOut);
/* Free up output. */

/* -------- Routines to build up index ------------ */

void gfCheckTileSize(int tileSize, boolean isPep);
/* Check that tile size is legal.  Abort if not. */

struct genoFind *gfIndexSeq(bioSeq *seqList,
	int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile,
	boolean isPep, boolean allowOneMismatch, boolean maskUpper,
	int stepSize, boolean noSimpRepMask);
/* Make index for all seqs in list. 
 *      minMatch - minimum number of matching tiles to trigger alignments
 *      maxGap   - maximum deviation from diagonal of tiles
 *      tileSize - size of tile in nucleotides
 *      maxPat   - maximum use of tile to not be considered a repeat
 *      oocFile  - .ooc format file that lists repeat tiles.  May be NULL. 
 *      isPep    - TRUE if indexing proteins, FALSE for DNA. 
 *      maskUpper - Mask out upper case sequence (currently only for nucleotides).
 *      stepSize - space between tiles.  Zero means default (which is tileSize). 
 *      noSimpRepMask - skip simple repeat masking. 
 * For DNA sequences upper case bits will be unindexed. */

struct genoFind *gfIndexNibsAndTwoBits(int fileCount, char *fileNames[],
	int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile, 
	boolean allowOneMismatch, int stepSize, boolean noSimpRepMask);
/* Make index for all .nib and .2bits in list. 
 *      minMatch - minimum number of matching tiles to trigger alignments
 *      maxGap   - maximum deviation from diagonal of tiles
 *      tileSize - size of tile in nucleotides
 *      maxPat   - maximum use of tile to not be considered a repeat
 *      oocFile  - .ooc format file that lists repeat tiles.  May be NULL. 
 *      allowOneMismatch - allow one mismatch in a tile.  
 *      stepSize - space between tiles.  Zero means default (which is tileSize).
 *      noSimpRepMask - skip simple repeat masking. */

void gfIndexTransNibsAndTwoBits(struct genoFind *transGf[2][3], 
    int fileCount, char *fileNames[], 
    int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile,
    boolean allowOneMismatch, boolean mask, int stepSize, boolean noSimpRepMask);
/* Make translated (6 frame) index for all .nib and .2bit files. */

/* -------- Routines to scan index for homolgous areas ------------ */

struct gfClump *gfFindClumps(struct genoFind *gf, struct dnaSeq *seq, 
	struct lm *lm, int *retHitCount);
/* Find clumps associated with one sequence. */

struct gfClump *gfFindClumpsWithQmask(struct genoFind *gf, bioSeq *seq, 
        Bits *qMaskBits, int qMaskOffset,
	struct lm *lm, int *retHitCount);
/* Find clumps associated with one sequence soft-masking seq according to qMaskBits */

struct gfHit *gfFindHitsInRegion(struct genoFind *gf, bioSeq *seq, 
	Bits *qMaskBits, int qMaskOffset, struct lm *lm, 
	struct gfSeqSource *target, int tMin, int tMax);
/* Find hits restricted to one particular region. 
 * The hits returned by this will be in target sequence
 * coordinates rather than concatenated whole genome
 * coordinates as hits inside of clumps usually are.  */

void gfTransFindClumps(struct genoFind *gfs[3], aaSeq *seq, struct gfClump *clumps[3], struct lm *lm, int *retHitCount);
/* Find clumps associated with one sequence in three translated reading frames. */

void gfTransTransFindClumps(struct genoFind *gfs[3], aaSeq *seqs[3], 
	struct gfClump *clumps[3][3], struct lm *lm, int *retHitCount);
/* Find clumps associated with three sequences in three translated 
 * reading frames. Used for translated/translated protein comparisons. */

void gfClumpDump(struct genoFind *gf, struct gfClump *clump, FILE *f);
/* Print out info on clump.  This routine subtracts clump->target->start 
 * from clump->tStart and from clump->tEnd for printing, so that printed 
 * coords are relative to that target sequence. */


void gfAlignAaClumps(struct genoFind *gf,  struct gfClump *clumpList, aaSeq *seq,
    boolean isRc,  int minMatch,  struct gfOutput *out);
/* Convert gfClumps to an actual alignment that gets saved via 
 * outFunction/outData. */

void gfFindAlignAaTrans(struct genoFind *gfs[3], aaSeq *qSeq, struct hash *t3Hash, 
	boolean tIsRc, int minMatch, struct gfOutput *out);
/* Look for qSeq alignment in three translated reading frames. Save alignment
 * via outFunction/outData. */


/* ---  Some routines for dealing with gfServer at a low level ---- */

char *gfSignature();
/* Return signature that starts each command to gfServer. Helps defend 
 * server from confused clients. */

void gfCatchPipes();
/* Set up to catch broken pipe signals. */

int gfReadMulti(int sd, void *vBuf, size_t size);
/* Read in until all is read or there is an error. */

/* ---  Some routines for dealing with gfServer at a high level ---- */

struct hash *gfFileCacheNew();
/* Create hash for storing info on .nib and .2bit files. */

void gfFileCacheFree(struct hash **pCache);
/* Free up resources in cache. */

void gfAlignStrand(struct gfConnection *conn, char *nibDir, struct dnaSeq *seq,
                   boolean isRc,  int minMatch, 
                   struct hash *tFileCache, struct gfOutput *out);
/* Search genome on server with one strand of other sequence to find homology. 
 * Then load homologous bits of genome locally and do detailed alignment.
 * Call 'outFunction' with each alignment that is found.  gfSavePsl is a handy
 * outFunction to use. */

void gfAlignTrans(struct gfConnection *conn, char *nibDir, aaSeq *seq,
                  int minMatch, struct hash *tFileHash, struct gfOutput *out);
/* Search indexed translated genome on server with an amino acid sequence. 
 * Then load homologous bits of genome locally and do detailed alignment.
 * Call 'outFunction' with each alignment that is found. */

void gfAlignTransTrans(struct gfConnection *conn, char *nibDir, struct dnaSeq *seq, 
                       boolean qIsRc, int minMatch, struct hash *tFileCache, 
                       struct gfOutput *out, boolean isRna);
/* Search indexed translated genome on server with an dna sequence.  Translate
 * this sequence in three frames. Load homologous bits of genome locally
 * and do detailed alignment.  Call 'outFunction' with each alignment
 * that is found. */

struct gfConnection *gfMayConnect(char *hostName, char *portName, char *genome, char *genomeDataDir);
/* Set up our network connection to server, or return NULL. genome and genomeDataDir are for dynamic server. */

struct gfConnection *gfConnect(char *hostName, char *portName, char *genome, char *genomeDataDir);
/* Set up our network connection to server. Aborts on error. genome and genomeDataDir are for dynamic server. */

void gfBeginRequest(struct gfConnection *conn);
/* called before a request is started.  If the connect is not open, reopen
 * it. */

void gfEndRequest(struct gfConnection *conn);
/* End a request that might be followed by another requests. For
 * a static server, this closed the connection.  A dynamic server
 * it is left open. *///

void gfDisconnect(struct gfConnection **pConn);
/* Disconnect from server */

int gfDefaultRepMatch(int tileSize, int stepSize, boolean protTiles);
/* Figure out appropriate step repMatch value. */

void gfMakeOoc(char *outName, char *files[], int fileCount, 
	int tileSize, bits32 maxPat, enum gfType tType, boolean noSimpRepMask);
/* Count occurences of tiles in seqList and make a .ooc file. */

void gfLongDnaInMem(struct dnaSeq *query, struct genoFind *gf, 
   boolean isRc, int minScore, Bits *qMaskBits, struct gfOutput *out,
   boolean fastMap, boolean band);
/* Chop up query into pieces, align each, and stitch back
 * together again. */

void gfLongTransTransInMem(struct dnaSeq *query, struct genoFind *gfs[3], 
   struct hash *t3Hash, boolean qIsRc, boolean tIsRc, boolean qIsRna,
   int minScore, struct gfOutput *out);
/* Chop up query into pieces, align each in translated space, and stitch back
 * together again as nucleotides. */

struct gfClump *gfPcrClumps(struct genoFind *gf, 
        char *fPrimer, int fPrimerSize, char *rPrimer, int rPrimerSize,
	int minDistance, int maxDistance);
/* Find possible PCR hits.  The fPrimer and rPrimer are on opposite strands.
 * Note: unlike clumps from other query functions, PCR clumps from this 
 * function have already had clump->target->start subtracted from 
 * clump->tStart and clump->tEnd so that the coords are relative to that 
 * target sequence (not the collection of all target sequences). */

#define MAXSINGLEPIECESIZE 5000 /* maximum size of a single piece */

#define gfVersion "39x1"	/* Current BLAT version number */

#endif /* GENOFIND_H */

