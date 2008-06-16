/* genoFind.h - Interface to modules for fast finding of sequence
 * matches. */
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

enum gfConstants {
    gfMinMatch = 2,
    gfMaxGap = 2,
    gfTileSize = 11,
    gfMaxTileUse = 1024,
    gfPepMaxTileUse = 30000,
};

struct gfSeqSource
/* Where a block of sequence comes from. */
    {
    struct gfSeqSource *next;
    char *fileName;	/* Name of file. */
    bioSeq *seq;	/* Sequences.  Usually either this or fileName is NULL. */
    bits32 start,end;	/* Position within merged sequence. */
    Bits *maskedBits;	/* If non-null contains repeat-masking info. */
    };

struct gfHit
/* A genoFind hit. */
   {
   struct gfHit *next;
   bits32 qStart;		/* Where it hits in query. */
   bits32 tStart;		/* Where it hits in target. */
   bits32 diagonal;		/* tStart + qSize - qStart. */
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
    bits32 qStart, qEnd;	/* Position in query. */
    struct gfSeqSource *target;	/* Target source sequence. */
    bits32 tStart, tEnd;	/* Position in target. */
    int hitCount;		/* Number of hits. */
    struct gfHit *hitList;	/* List of hits. Not allocated here. */
    int queryCoverage;		/* Number of bases covered in query (thx AG!) */
    };

void gfClumpFree(struct gfClump **pClump);
/* Free a single clump. */

void gfClumpFreeList(struct gfClump **pList);
/* Free a list of dynamically allocated gfClump's */

struct genoFind
/* An index of all K-mers in the genome. */
    {
    int maxPat;                          /* Max # of times pattern can occur
                                          * before it is ignored. */
    int minMatch;                        /* Minimum number of tile hits needed
                                          * to trigger a clump hit. */
    int maxGap;                          /* Max gap between tiles in a clump. */
    int tileSize;			 /* Size of each N-mer. */
    int stepSize;			 /* Spacing between N-mers. */
    int tileSpaceSize;                   /* Number of N-mer values. */
    int tileMask;			 /* 1-s for each N-mer. */
    int sourceCount;			 /* Count of source files. */
    struct gfSeqSource *sources;         /* List of sequence sources. */
    bool isPep;			 	 /* Is a peptide. */
    bool allowOneMismatch;		 /* Allow a single mismatch? */
    int segSize;			 /* Index is segmented if non-zero. */
    bits32 totalSeqSize;		 /* Total size of all sequences. */
    bits32 *listSizes;                   /* Size of list for each N-mer */
    void *allocated;                     /* Storage space for all lists. */
    bits32 **lists;                      /* A list for each N-mer. Used if
                                          * isSegmented is false. */
    bits16 **endLists;                   /* A more complex list for each N-mer.
                                          * Used if isSegmented is true.
					  * Values come in groups of threes.
					  * The first is the packed last few
					  * letters of the tile.  The next two
					  * are the offset in the genome.  This
					  * would be a struct but that would take
					  * 8 bytes instead of 6, or nearly an
					  * extra gigabyte of RAM. */
    };

void genoFindFree(struct genoFind **pGenoFind);
/* Free up a genoFind index. */

struct gfSeqSource *gfFindNamedSource(struct genoFind *gf, char *name);
/* Find target of given name.  Return NULL if none. */

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
	int stepSize);
/* Make index for all seqs in list. 
 *      minMatch - minimum number of matching tiles to trigger alignments
 *      maxGap   - maximum deviation from diagonal of tiles
 *      tileSize - size of tile in nucleotides
 *      maxPat   - maximum use of tile to not be considered a repeat
 *      oocFile  - .ooc format file that lists repeat tiles.  May be NULL. 
 *      isPep    - TRUE if indexing proteins, FALSE for DNA. 
 *      maskUpper - Mask out upper case sequence (currently only for nucleotides).
 *      stepSize - space between tiles.  Zero means default (which is tileSize). 
 * For DNA sequences upper case bits will be unindexed. */

struct genoFind *gfIndexNibsAndTwoBits(int fileCount, char *fileNames[],
	int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile, 
	boolean allowOneMismatch, int stepSize);
/* Make index for all .nib and .2bits in list. 
 *      minMatch - minimum number of matching tiles to trigger alignments
 *      maxGap   - maximum deviation from diagonal of tiles
 *      tileSize - size of tile in nucleotides
 *      maxPat   - maximum use of tile to not be considered a repeat
 *      oocFile  - .ooc format file that lists repeat tiles.  May be NULL. 
 *      allowOneMismatch - allow one mismatch in a tile.  
 *      stepSize - space between tiles.  Zero means default (which is tileSize). */

void gfIndexTransNibsAndTwoBits(struct genoFind *transGf[2][3], 
    int fileCount, char *fileNames[], 
    int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile,
    boolean allowOneMismatch, boolean mask, int stepSize);
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

void gfAlignStrand(int *pConn, char *nibDir, struct dnaSeq *seq,
    boolean isRc,  int minMatch, 
    struct hash *tFileCache, struct gfOutput *out);
/* Search genome on server with one strand of other sequence to find homology. 
 * Then load homologous bits of genome locally and do detailed alignment.
 * Call 'outFunction' with each alignment that is found.  gfSavePsl is a handy
 * outFunction to use. */

void gfAlignTrans(int *pConn, char *nibDir, aaSeq *seq,
    int minMatch, struct hash *tFileHash, struct gfOutput *out);
/* Search indexed translated genome on server with an amino acid sequence. 
 * Then load homologous bits of genome locally and do detailed alignment.
 * Call 'outFunction' with each alignment that is found. */

void gfAlignTransTrans(int *pConn, char *nibDir, struct dnaSeq *seq, 
	boolean qIsRc, int minMatch, struct hash *tFileCache, 
	struct gfOutput *out, boolean isRna);
/* Search indexed translated genome on server with an dna sequence.  Translate
 * this sequence in three frames. Load homologous bits of genome locally
 * and do detailed alignment.  Call 'outFunction' with each alignment
 * that is found. */

int gfConnect(char *hostName, char *portName);
/* Set up our network connection to server. */

void gfMakeOoc(char *outName, char *files[], int fileCount, 
	int tileSize, bits32 maxPat, enum gfType tType);
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

#define gfVersion "34x2"	/* Current BLAT version number */

#endif /* GENOFIND_H */

