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

enum gfConstants {
    gfMinMatch = 3,
    gfMaxGap = 2,
    gfTileSize = 12,
    gfMaxTileUse = 1024,
    gfPepMaxTileUse = 4096,
};

struct gfSeqSource
/* Where a block of sequence comes from. */
    {
    struct gfSeqSource *next;
    char *fileName;	/* Name of file. */
    bioSeq *seq;	/* Sequences.  Usually either this or fileName is NULL. */
    bits32 start,end;	/* Position within merged sequence. */
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
    struct gfHit *hitList;	/* List of hits. */
    };

void gfClumpFree(struct gfClump **pClump);
/* Free a single clump. */

void gfClumpFreeList(struct gfClump **pList);
/* Free a list of dynamically allocated gfClump's */

struct genoFind
/* A pattern space - something that holds an index of all N-mers in
 * genome. */
    {
    bits32 **lists;                      /* A list for each N-mer */
    bits32 *listSizes;                    /* Size of list for each N-mer */
    bits32 *allocated;                   /* Storage space for all lists. */
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
    boolean isPep;			 /* Is a peptide. */
    };

void genoFindFree(struct genoFind **pGenoFind);
/* Free up a genoFind index. */

struct genoFind *gfIndexSeq(bioSeq *seqList,
	int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile,
	boolean isPep);
/* Make index for all seqs in list. 
 *      minMatch - minimum number of matching tiles to trigger alignments
 *      maxGap   - maximum deviation from diagonal of tiles
 *      tileSize - size of tile in nucleotides
 *      maxPat   - maximum use of tile to not be considered a repeat
 *      oocFile  - .ooc format file that lists repeat tiles.  May be NULL. 
 *      isPep    - TRUE if indexing proteins, FALSE for DNA. */

struct genoFind *gfIndexNibs(int nibCount, char *nibNames[],
	int minMatch, int maxGap, int tileSize, int maxPat, char *oocFile);
/* Make index for all nib files. */

struct gfClump *gfFindClumps(struct genoFind *gf, struct dnaSeq *seq);
/* Find clumps associated with one sequence. */

struct gfClump *gfPepFindClumps(struct genoFind *gf, struct dnaSeq *seq);
/* Find clumps associated with one sequence. */

void gfClumpDump(struct genoFind *gf, struct gfClump *clump, FILE *f);
/* Print out info on clump */

typedef void (*GfSaveAli)(char *chromName, int chromSize, int chromOffset,
	struct ffAli *ali, bioSeq *genoSeq, bioSeq *otherSeq, 
	boolean isRc, enum ffStringency stringency, int minMatch, void  *outputData);
/* This is the type of a client provided function to save an alignment. */

void gfAlignDnaClumps(struct gfClump *clumpList, struct dnaSeq *seq,
    boolean isRc,  enum ffStringency stringency, int minMatch, 
    GfSaveAli outFunction, void *outData);
/* Convert gfClumps to an actual alignment that gets saved via 
 * outFunction/outData. gfSavePsl is a handy outFunction to use.  Put
 * a FILE as outData in this case.. */

void gfAlignAaClumps(struct genoFind *gf,  struct gfClump *clumpList, aaSeq *seq,
    boolean isRc,  enum ffStringency stringency, int minMatch, 
    GfSaveAli outFunction, void *outData);
/* Convert gfClumps to an actual alignment that gets saved via 
 * outFunction/outData. */

/* ---  Some routines for dealing with gfServer at a low level ---- */

char *gfSignature();
/* Return signature that starts each command to gfServer. Helps defend 
 * server from confused clients. */

void gfSendString(int sd, char *s);
/* Send a string down a socket - length byte first. */

char *gfRecieveString(int sd, char buf[256]);
/* Read string into buf and return it.  If buf is NULL
 * an internal buffer will be used. Abort if any problem. */

char *gfGetString(int sd, char buf[256]);
/* Read string into buf and return it.  If buf is NULL
 * an internal buffer will be used. Print warning message
 * and return NULL if any problem. */

int gfReadMulti(int sd, void *vBuf, size_t size);
/* Read in until all is read or there is an error. */

/* ---  Some routines for dealing with gfServer at a high level ---- */

void gfSavePsl(char *chromName, int chromSize, int chromOffset,
	struct ffAli *ali, struct dnaSeq *genoSeq, struct dnaSeq *otherSeq, 
	boolean isRc, enum ffStringency stringency, int minMatch, void *outputData);
/* Analyse one alignment and if it looks good enough write it out to file in
 * psl format.  */

void gfAlignStrand(char *hostName, char *portName, char *nibDir, struct dnaSeq *seq,
    boolean isRc,  enum ffStringency stringency, int minMatch, GfSaveAli outFunction, void *outData);
/* Search genome on server with one strand of other sequence to find homology. 
 * Then load homologous bits of genome locally and do detailed alignment.
 * Call 'outFunction' with each alignment that is found.  gfSavePsl is a handy
 * outFunction to use. */

#endif /* GENOFIND_H */

