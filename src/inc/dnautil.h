/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* Some stuff that you'll likely need in any program that works with
 * DNA.  Includes stuff for amino acids as well. 
 *
 * Assumes that DNA is stored as a character.
 * The DNA it generates will include the bases 
 * as lowercase tcag.  It will generally accept
 * uppercase as well, and also 'n' or 'N' or '-'
 * for unknown bases. 
 *
 * Amino acids are stored as single character upper case. */

#ifndef DNAUTIL_H
#define DNAUTIL_H

void dnaUtilOpen(); /* Good idea to call this before using any arrays
		     * here.  */

/* Numerical values for bases. */
#define T_BASE_VAL 0
#define U_BASE_VAL 0
#define C_BASE_VAL 1
#define A_BASE_VAL 2
#define G_BASE_VAL 3
#define N_BASE_VAL 4   /* Used in 1/2 byte representation. */

typedef char DNA;
typedef char AA;

/* A little array to help us decide if a character is a 
 * nucleotide, and if so convert it to lower case. 
 * Contains zeroes for characters that aren't used
 * in DNA sequence. */
extern DNA ntChars[256];
extern AA aaChars[256];

/* An array that converts alphabetical DNA representation
 * to numerical one: X_BASE_VAL as above. */
extern int ntVal[256];
extern int aaVal[256];

/* Like ntVal, but with T_BASE_VAL in place of -1 for nonexistent nucleotides. */
extern int ntValNoN[256];     

/* Like ntVal but with N_BASE_VAL in place of -1 for 'n', 'x', etc. */
extern int ntVal5[256];

/* Inverse array - takes X_BASE_VAL int to a DNA char
 * value. */
extern DNA valToNt[5];

/* Similar array that doesn't convert to lower case. */
extern DNA ntMixedCaseChars[256];

/* Another array to help us do complement of DNA  */
extern DNA ntCompTable[256];

/* Reverse complement DNA. */
void reverseComplement(DNA *dna, long length);


/* Reverse offset - return what will be offset (0 based) to
 * same member of array after array is reversed. */
long reverseOffset(long offset, long arraySize);

/* Switch start/end (zero based half open) coordinates
 * to opposite strand. */
void reverseIntRange(int *pStart, int *pEnd, int size);

/* Switch start/end (zero based half open) coordinates
 * to opposite strand. */
void reverseUnsignedRange(unsigned *pStart, unsigned *pEnd, int size); 

enum dnaCase {dnaUpper,dnaLower,dnaMixed,};
/* DNA upper, lower, or mixed case? */

/* Convert T's to U's */
void toRna(DNA *dna);

typedef char Codon; /* Our codon type. */

/* Return single letter code (upper case) for protein.
 * Returns X for bad input, 0 for stop codon. */
AA lookupCodon(DNA *dna); 

/* Return value from 0-63 of codon starting at start. 
 * Returns -1 if not a codon. */
Codon codonVal(DNA *start);

/* Return  codon corresponding to val (0-63) */
DNA *valToCodon(int val);

void dnaTranslateSome(DNA *dna, char *out, int outSize);
/* Translate up to outSize bases of DNA.  Output will be zero terminated. */

/* Return next power of four that would be greater or equal to x.
 * For instance if x < 4, return 1, if x < 16 return 2.... 
 * (From biological point of view how many bases are needed to
 * code this number.) */
int nextPowerOfFour(long x);

/* Return how long DNA will be after non-DNA is filtered out. */
long dnaFilteredSize(char *rawDna);

/* Filter out non-DNA characters. */
void dnaFilter(char *in, DNA *out);

/* Change all non-DNA characters to N. */
void dnaFilterToN(char *in, DNA *out);

/* Count up frequency of occurance of each base and store 
 * results in histogram. Use X_BASE_VAL to index histogram. */
void dnaBaseHistogram(DNA *dna, int dnaSize, int histogram[4]);

/* Filter out non-DNA characters but leave case intact. */
void dnaMixedCaseFilter(char *in, DNA *out);

/* pack 16 bases into a word */
bits32 packDna16(DNA *in);

/* Pack 8 bases into a short word */
bits16 packDna8(DNA *in);

/* Unpack DNA. Expands to 16x tileCount in output. */
void unpackDna(bits32 *tiles, int tileCount, DNA *out);

/* Unpack into out, even though not starting/stopping on tile 
 * boundaries. */
void unalignedUnpackDna(bits32 *tiles, int start, int size, DNA *unpacked);

int intronOrientation(DNA *iStart, DNA *iEnd);
/* Given a gap in genome from iStart to iEnd, return 
 * Return 1 for GT/AG intron between left and right, -1 for CT/AC, 0 for no
 * intron. */

int dnaScore2(DNA a, DNA b);
/* Score match between two bases (relatively crudely). */

int dnaScoreMatch(DNA *a, DNA *b, int size);
/* Compare two pieces of DNA base by base. Total mismatches are
 * subtracted from total matches and returned as score. 'N's 
 * neither hurt nor help score. */

int aaScore2(AA a, AA b);
/* Score match between two bases (relatively crudely). */

int aaScoreMatch(AA *a, AA *b, int size);
/* Compare two peptides aa by aa. */

#endif /* DNAUTIL_H */
