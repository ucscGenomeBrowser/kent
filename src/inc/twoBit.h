/* twoBit - DNA sequence represented as two bits per pixel
 * with associated list of regions containing N's, and
 * masked regions. */

#ifndef TWOBIT_H
#define TWOBIT_H

struct twoBit
/* Two bit representation of DNA. */
    {
    struct twoBit *next;	/* Next sequence in list */
    char *name;			/* Name of sequence. */
    UBYTE *data;		/* DNA at two bits per base. */
    bits32 size;			/* Size of this sequence. */
    bits32 nBlockCount;		/* Count of blocks of Ns. */
    bits32 *nStarts;		/* Starts of blocks of Ns. */
    bits32 *nSizes;		/* Sizes of blocks of Ns. */
    bits32 maskBlockCount;		/* Count of masked blocks. */
    bits32 *maskStarts;		/* Starts of masked regions. */
    bits32 *maskSizes;		/* Sizes of masked regions. */
    bits32 reserved;		/* Reserved for future expansion. */
    };

struct twoBitIndex
/* An entry in twoBit index. */
    {
    struct twoBitIndex *next;	/* Next in list. */
    char *name;			/* Name - allocated in hash */
    bits32 offset;		/* Offset in file. */
    };

struct twoBitHeader
/* Holds header and index info from .2bit file. */
    {
    struct twoBitHeader *next;
    char *fileName;	/* Name of this file, for error reporting. */
    FILE *f;		/* Open file. */
    boolean isSwapped;	/* Is byte-swapping needed. */
    bits32 version;	/* Version of .2bit file */
    bits32 seqCount;	/* Number of sequences. */
    bits32 reserved;	/* Reserved, always zero for now. */
    struct twoBitIndex *indexList;	/* List of sequence. */
    struct hash *hash;	/* Hash of sequences. */
    };


struct twoBit *twoBitFromDnaSeq(struct dnaSeq *seq, boolean doMask);
/* Convert dnaSeq representation in memory to twoBit representation.
 * If doMask is true interpret lower-case letters as masked. */

void twoBitWriteOne(struct twoBit *twoBit, FILE *f);
/* Write out one twoBit sequence to binary file. 
 * Note this does not include the name, which is
 * stored only in index. */

void twoBitWriteHeader(struct twoBit *twoBitList, FILE *f);
/* Write out header portion of twoBit file, including initial
 * index */

struct twoBitHeader *twoBitHeaderRead(char *fileName, FILE *f);
/* Read in header and index from already opened file.  
 * Squawk and die if there is a problem. */

void twoBitHeaderFree(struct twoBitHeader **pTbh);
/* Free up resources associated with twoBitHeader. */

struct dnaSeq *twoBitReadSeqFrag(struct twoBitHeader *tbh, char *name,
	int fragStart, int fragEnd);
/* Read part of sequence from .2bit file.  To read full
 * sequence call with start=end=0. */

#endif /* TWOBIT_H */
