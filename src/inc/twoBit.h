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
    bits32 size;		/* Size of this sequence. */
    bits32 nBlockCount;		/* Count of blocks of Ns. */
    bits32 *nStarts;		/* Starts of blocks of Ns. */
    bits32 *nSizes;		/* Sizes of blocks of Ns. */
    bits32 maskBlockCount;	/* Count of masked blocks. */
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

struct twoBitFile
/* Holds header and index info from .2bit file. */
    {
    struct twoBitFile *next;
    char *fileName;	/* Name of this file, for error reporting. */
    void *f;            /* Open file. */
    boolean isSwapped;	/* Is byte-swapping needed. */
    bits32 version;	/* Version of .2bit file */
    bits32 seqCount;	/* Number of sequences. */
    bits32 reserved;	/* Reserved, always zero for now. */
    struct twoBitIndex *indexList;	/* List of sequence. */
    struct hash *hash;	/* Hash of sequences. */
    struct bptFile *bpt;	/* Alternative index. */

    /* the routines we use to access the twoBit.
     * These may be UDC routines, or stdio
     */
    void (*ourSeek)(void *file, bits64 offset);
    void (*ourSeekCur)(void *file, bits64 offset);
    bits32 (*ourReadBits32)(void *f, boolean isSwapped);
    void (*ourClose)(void *pFile);
    boolean (*ourFastReadString)(void *f, char buf[256]);
    void (*ourMustRead)(void *file, void *buf, size_t size);
    };

struct twoBitSpec
/* parsed .2bit file and sequence specs */
{
    char *fileName;                 /* path to file */
    struct twoBitSeqSpec *seqs;     /* list of sequences and subsequences */
};

struct twoBitSeqSpec
/* specification for a seq or subsequence in a .2bit file */
{
    struct twoBitSeqSpec *next;
    char *name;                 /* name of sequence */
    bits32 start;              /* start of subsequence 0 */
    bits32 end;                /* end of subsequence;
                                 * 0 if not a subsequence */
};

struct twoBitFile *twoBitOpen(char *fileName);
/* Open file, read in header and index.  
 * Squawk and die if there is a problem. */

struct twoBitFile *twoBitOpenExternalBptIndex(char *twoBitName, char *bptName);
/* Open file, read in header, but not regular index.  Instead use
 * bpt index.   Beware if you use this the indexList field will be NULL
 * as will the hash. */

void twoBitClose(struct twoBitFile **pTbf);
/* Free up resources associated with twoBitFile. */

int twoBitSeqSize(struct twoBitFile *tbf, char *name);
/* Return size of sequence in two bit file in bases. */

long long twoBitTotalSize(struct twoBitFile *tbf);
/* Return total size of all sequences in two bit file. */

struct dnaSeq *twoBitReadSeqFragExt(struct twoBitFile *tbf, char *name,
                                    int fragStart, int fragEnd, boolean doMask, int *retFullSize);
/* Read part of sequence from .2bit file.  To read full
 * sequence call with start=end=0.  Sequence will be lower
 * case if doMask is false, mixed case (repeats in lower)
 * if doMask is true. */

struct dnaSeq *twoBitReadSeqFrag(struct twoBitFile *tbf, char *name,
	int fragStart, int fragEnd);
/* Read part of sequence from .2bit file.  To read full
 * sequence call with start=end=0.  Note that sequence will
 * be mixed case, with repeats in lower case and rest in
 * upper case. */

struct dnaSeq *twoBitReadSeqFragLower(struct twoBitFile *tbf, char *name,
	int fragStart, int fragEnd);
/* Same as twoBitReadSeqFrag, but sequence is returned in lower case. */

struct dnaSeq *twoBitLoadAll(char *spec);
/* Return list of all sequences matching spec, which is in
 * the form:
 *
 *    file/path/input.2bit[:seqSpec1][,seqSpec2,...]
 *
 * where seqSpec is either
 *     seqName
 *  or
 *     seqName:start-end */

struct slName *twoBitSeqNames(char *fileName);
/* Get list of all sequences in twoBit file. */

struct twoBit *twoBitFromDnaSeq(struct dnaSeq *seq, boolean doMask);
/* Convert dnaSeq representation in memory to twoBit representation.
 * If doMask is true interpret lower-case letters as masked. */

struct twoBit *twoBitFromFile(char *fileName);
/* Get twoBit list of all sequences in twoBit file. */

struct twoBit *twoBitOneFromFile(struct twoBitFile *tbf, char *name);
/* Get single sequence as two bit. */

void twoBitFree(struct twoBit **pTwoBit);
/* Free up a two bit structure. */

void twoBitFreeList(struct twoBit **pList);
/* Free a list of dynamically allocated twoBit's */


void twoBitWriteOne(struct twoBit *twoBit, FILE *f);
/* Write out one twoBit sequence to binary file. 
 * Note this does not include the name, which is
 * stored only in index. */

void twoBitWriteHeader(struct twoBit *twoBitList, FILE *f);
/* Write out header portion of twoBit file, including initial
 * index */

boolean twoBitIsFile(char *fileName);
/* Return TRUE if file is in .2bit format. */

boolean twoBitParseRange(char *rangeSpec, char **retFile, 
	char **retSeq, int *retStart, int *retEnd);
/* Parse out something in format
 *    file/path/name:seqName:start-end
 * or
 *    file/path/name:seqName
 * This will destroy the input 'rangeSpec' in the process.
 * Returns FALSE if it doesn't fit this format. 
 * If it is the shorter form then start and end will both
 * be returned as zero, which is ok by twoBitReadSeqFrag. */

boolean twoBitIsRange(char *rangeSpec);
/* Return TRUE if it looks like a two bit range specifier. */

boolean twoBitIsFileOrRange(char *spec);
/* Return TRUE if it is a two bit file or subrange. */

boolean twoBitIsSpec(char *spec);
/* Return TRUE spec is a valid 2bit spec (see twoBitSpecNew) */

struct twoBitSpec *twoBitSpecNew(char *specStr);
/* Parse a .2bit file and sequence spec into an object.
 * The spec is a string in the form:
 *
 *    file/path/input.2bit[:seqSpec1][,seqSpec2,...]
 *
 * where seqSpec is either
 *     seqName
 *  or
 *     seqName:start-end
 *
 * free result with twoBitSpecFree().
 */

struct twoBitSpec *twoBitSpecNewFile(char *twoBitFile, char *specFile);
/* parse a file containing a list of specifications for sequences in the
 * specified twoBit file. Specifications are one per line in forms:
 *     seqName
 *  or
 *     seqName:start-end
 */

void twoBitSpecFree(struct twoBitSpec **specPtr);
/* free a twoBitSpec object */

void twoBitOutNBeds(struct twoBitFile *tbf, char *seqName, FILE *outF);
/* output a series of bed3's that enumerate the number of N's in a sequence*/

int twoBitSeqSizeNoNs(struct twoBitFile *tbf, char *seqName);
/* return the length of the sequence, not counting N's */

long long twoBitTotalSizeNoN(struct twoBitFile *tbf);
/* return the size of the all the sequence in file, not counting N's*/

boolean twoBitIsSequence(struct twoBitFile *tbf, char *chromName);
/* Return TRUE if chromName is in 2bit file. */
#endif /* TWOBIT_H */
