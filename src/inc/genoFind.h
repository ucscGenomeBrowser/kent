/* genoFind.h - Interface to modules for fast finding of sequence
 * matches. */

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
    {
    struct gfClump *next;	/* Next clump. */
    bits32 qStart, qEnd;	/* Position in query. */
    struct gfSeqSource *target;	/* Target source sequence. */
    bits32 tStart, tEnd;	/* Position in target. */
    int hitCount;		/* Number of hits. */
    struct gfHit *hitList;	/* List of hits. Not allocated here. */
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

void gfCheckTileSize(int tileSize, boolean isPep);
/* Check that tile size is legal.  Abort if not. */

struct genoFind *gfIndexSeq(bioSeq *seqList,
	int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile,
	boolean isPep, boolean allowOneMismatch, boolean maskUpper);
/* Make index for all seqs in list. 
 *      minMatch - minimum number of matching tiles to trigger alignments
 *      maxGap   - maximum deviation from diagonal of tiles
 *      tileSize - size of tile in nucleotides
 *      maxPat   - maximum use of tile to not be considered a repeat
 *      oocFile  - .ooc format file that lists repeat tiles.  May be NULL. 
 *      isPep    - TRUE if indexing proteins, FALSE for DNA. 
 *      maskUpper - Mask out upper case sequence (currently only for nucleotides).
 * For DNA sequences upper case bits will be unindexed. */

struct genoFind *gfIndexNibs(int nibCount, char *nibNames[],
	int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile, 
	boolean allowOneMismatch);
/* Make index for all nib files. */

void gfIndexTransNibs(struct genoFind *transGf[2][3], int nibCount, char *nibNames[], 
    int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile,
    boolean allowOneMismatch);
/* Make translated (6 frame) index for all nib files. */

struct gfClump *gfFindClumps(struct genoFind *gf, struct dnaSeq *seq, 
	struct lm *lm, int *retHitCount);
/* Find clumps associated with one sequence. */

struct gfClump *gfFindClumpsWithQmask(struct genoFind *gf, bioSeq *seq, 
        Bits *qMaskBits, int qMaskOffset,
	struct lm *lm, int *retHitCount);
/* Find clumps associated with one sequence soft-masking seq according to qMaskBits */

struct gfClump *gfPepFindClumps(struct genoFind *gf, aaSeq *seq, 
	struct lm *lm, int *retHitCount);
/* Find clumps associated with one sequence. */

void gfTransFindClumps(struct genoFind *gfs[3], aaSeq *seq, struct gfClump *clumps[3], struct lm *lm, int *retHitCount);
/* Find clumps associated with one sequence in three translated reading frames. */

void gfTransTransFindClumps(struct genoFind *gfs[3], aaSeq *seqs[3], 
	struct gfClump *clumps[3][3], struct lm *lm, int *retHitCount);
/* Find clumps associated with three sequences in three translated 
 * reading frames. Used for translated/translated protein comparisons. */

void gfClumpDump(struct genoFind *gf, struct gfClump *clump, FILE *f);
/* Print out info on clump */

typedef void (*GfSaveAli)(char *chromName, int chromSize, int chromOffset,
	struct ffAli *ali, bioSeq *genoSeq, bioSeq *otherSeq, 
	boolean isRc, enum ffStringency stringency, int minMatch, void  *outputData);
/* This is the type of a client provided function to save an alignment. */

#ifdef NOWSTATIC
void gfAlignDnaClumps(struct genoFind *gf, struct gfClump *clumpList, 
    struct dnaSeq *seq, boolean isRc,  int minMatch, GfSaveAli outFunction, 
    void *outData, boolean fastN);
/* Convert gfClumps to an actual alignment that gets saved via 
 * outFunction/outData. gfSavePsl is a handy outFunction to use.  Put
 * a FILE as outData in this case.. */
#endif /* NOWSTATIC */

void gfAlignAaClumps(struct genoFind *gf,  struct gfClump *clumpList, aaSeq *seq,
    boolean isRc,  int minMatch,  GfSaveAli outFunction, void *outData);
/* Convert gfClumps to an actual alignment that gets saved via 
 * outFunction/outData. */

void gfFindAlignAaTrans(struct genoFind *gfs[3], aaSeq *qSeq, struct hash *t3Hash, 
	int minMatch,  GfSaveAli outFunction, void *outData);
/* Look for qSeq alignment in three translated reading frames. Save alignment
 * via outFunction/outData. */

#ifdef NOWSTATIC
void gfFindAlignTransTrans(struct genoFind *gfs[3], struct dnaSeq *qSeq, struct hash *t3Hash, 
	boolean isRc, int minMatch,  GfSaveAli outFunction, void *outData, boolean isRna);
/* Look for alignment to three translations of qSeq in three translated reading frames. 
 * Save alignment via outFunction/outData. */
#endif /* NOWSTATIC */

/* ---  Some routines for dealing with gfServer at a low level ---- */

char *gfSignature();
/* Return signature that starts each command to gfServer. Helps defend 
 * server from confused clients. */

void gfCatchPipes();
/* Set up to catch broken pipe signals. */

int gfReadMulti(int sd, void *vBuf, size_t size);
/* Read in until all is read or there is an error. */

/* ---  Some routines for dealing with gfServer at a high level ---- */

struct gfSavePslxData
/* This is the data structure passed as output data for gfSavePslx below. */
    {
    FILE *f;			/* Output file. */
    struct hash *t3Hash;	/* Hash to associate names and frames. */
    boolean reportTargetStrand; /* Report target as well as query strand? */
    boolean targetRc;		/* Is target reverse complemented? */
    struct hash *maskHash;	/* Hash to associate target sequence name and mask. */
    int minGood;		/* Minimum sequence identity in parts per thousand. */
    boolean saveSeq;		/* Save sequence too? */
    boolean qIsProt;		/* Query is peptide. */
    boolean tIsProt;		/* Target is peptide. */
    };

void gfSavePslx(char *chromName, int chromSize, int chromOffset,
	struct ffAli *ali, struct dnaSeq *genoSeq, struct dnaSeq *otherSeq, 
	boolean isRc, enum ffStringency stringency, int minMatch, void *outputData);
/* Analyse one alignment and if it looks good enough write it out to file in
 * pslx format.  This is meant for translated alignments. */

void gfAlignStrand(int conn, char *nibDir, struct dnaSeq *seq,
    boolean isRc,  int minMatch, GfSaveAli outFunction, void *outData);
/* Search genome on server with one strand of other sequence to find homology. 
 * Then load homologous bits of genome locally and do detailed alignment.
 * Call 'outFunction' with each alignment that is found.  gfSavePsl is a handy
 * outFunction to use. */

void gfAlignTrans(int conn, char *nibDir, aaSeq *seq,
    int minMatch, GfSaveAli outFunction, struct gfSavePslxData *outData);
/* Search indexed translated genome on server with an amino acid sequence. 
 * Then load homologous bits of genome locally and do detailed alignment.
 * Call 'outFunction' with each alignment that is found. */

void gfAlignTransTrans(int conn, char *nibDir, struct dnaSeq *seq, boolean isRc,
    int minMatch, GfSaveAli outFunction, struct gfSavePslxData *outData, boolean isRna);
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
   boolean isRc, int minScore, Bits *qMaskBits, GfSaveAli outFunction, void *outData);
/* Chop up query into pieces, align each, and stitch back
 * together again. */

void gfLongTransTransInMem(struct dnaSeq *query, struct genoFind *gfs[3], 
   struct hash *t3Hash, boolean qIsRc, boolean qIsRna,
   int minScore, GfSaveAli outFunction, void *outData);
/* Chop up query into pieces, align each in translated space, and stitch back
 * together again as nucleotides. */

#endif /* GENOFIND_H */

