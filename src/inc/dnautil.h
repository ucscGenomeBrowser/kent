/* Some stuff that you'll likely need in any program that works with
 * DNA.  Includes stuff for amino acids as well. 
 *
 * Assumes that DNA is stored as a character.
 * The DNA it generates will include the bases 
 * as lowercase tcag.  It will generally accept
 * uppercase as well, and also 'n' or 'N' or '-'
 * for unknown bases. 
 *
 * Amino acids are stored as single character upper case. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */


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
typedef char BIOPOL;	/* Biological polymer. */

/* A little array to help us decide if a character is a 
 * nucleotide, and if so convert it to lower case. 
 * Contains zeroes for characters that aren't used
 * in DNA sequence. */
extern DNA ntChars[256];
extern AA aaChars[256];

/* An array that converts alphabetical DNA representation
 * to numerical one: X_BASE_VAL as above.  For charaters
 * other than [atgcATGC], has -1. */
extern int ntVal[256];
extern int aaVal[256];
extern int ntValLower[256];	/* NT values only for lower case. */
extern int ntValUpper[256];	/* NT values only for upper case. */

/* Like ntVal, but with T_BASE_VAL in place of -1 for nonexistent nucleotides. */
extern int ntValNoN[256];     

/* Like ntVal but with N_BASE_VAL in place of -1 for 'n', 'x', '-', etc. */
extern int ntVal5[256];

/* Inverse array - takes X_BASE_VAL int to a DNA char
 * value. */
extern DNA valToNt[];

/* Similar array that doesn't convert to lower case. */
extern DNA ntMixedCaseChars[256];

/* Another array to help us do complement of DNA  */
extern DNA ntCompTable[256];

/* Arrays to convert between lower case indicating repeat masking, and
 * a 1/2 byte representation where the 4th bit indicates if the characeter
 * is masked. Uses N_BASE_VAL for `n', `x', etc.
*/
#define MASKED_BASE_BIT 8
extern int ntValMasked[256];
extern DNA valToNtMasked[256];

/*Complement DNA (not reverse)*/
void complement(DNA *dna, long length);

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

int cmpDnaStrings(DNA *a, DNA *b);
/* Compare using screwy non-alphabetical DNA order TCGA */

typedef char Codon; /* Our codon type. */

/* Return single letter code (upper case) for protein.
 * Returns X for bad input, 0 for stop codon.
 * The "Standard" Code */
AA lookupCodon(DNA *dna); 

boolean isStopCodon(DNA *dna);
/* Return TRUE if it's a stop codon. */

boolean isKozak(char *dna, int dnaSize, int pos);
/* Return TRUE if it's a Kozak compatible start, using a relatively
 * weak definition (either A/G 3 bases before or G after) . */

boolean isReallyStopCodon(char *dna, boolean selenocysteine);
/* Return TRUE if it's really a stop codon, even considering
 * possibilility of selenocysteine. */

/* Returns one letter code for protein, 
 * 0 for stop codon or X for bad input,
 * Vertebrate Mitochondrial Code */
AA lookupMitoCodon(DNA *dna);

Codon codonVal(DNA *start);
/* Return value from 0-63 of codon starting at start. 
 * Returns -1 if not a codon. */

DNA *valToCodon(int val);
/* Return  codon corresponding to val (0-63) */

extern char *aaAbbr(int i);
/* return pointer to AA abbrevation */

extern char aaLetter(int i);
/* return AA letter */

void dnaTranslateSome(DNA *dna, char *out, int outSize);
/* Translate DNA upto a stop codon or until outSize-1 amino acids, 
 * whichever comes first. Output will be zero terminated. */

char *skipIgnoringDash(char *a, int size, bool skipTrailingDash);
/* Count size number of characters, and any 
 * dash characters. */

int countNonDash(char *a, int size);
/* Count number of non-dash characters. */

int nextPowerOfFour(long x);
/* Return next power of four that would be greater or equal to x.
 * For instance if x < 4, return 1, if x < 16 return 2.... 
 * (From biological point of view how many bases are needed to
 * code this number.) */

long dnaFilteredSize(char *rawDna);
/* Return how long DNA will be after non-DNA is filtered out. */

long aaFilteredSize(char *rawDna);
/* Return how long peptide will be after non-peptide is filtered out. */

void dnaFilter(char *in, DNA *out);
/* Filter out non-DNA characters. */

void aaFilter(char *in, DNA *out);
/* Filter out non-peptide characters. */

void dnaFilterToN(char *in, DNA *out);
/* Change all non-DNA characters to N. */

void upperToN(char *s, int size);
/* Turn upper case letters to N's. */

void lowerToN(char *s, int size);
/* Turn lower case letters to N's. */

void dnaBaseHistogram(DNA *dna, int dnaSize, int histogram[4]);
/* Count up frequency of occurance of each base and store 
 * results in histogram. Use X_BASE_VAL to index histogram. */

void dnaMixedCaseFilter(char *in, DNA *out);
/* Filter out non-DNA characters but leave case intact. */

bits64 basesToBits64(char *dna, int size);
/* Convert dna of given size (up to 32) to binary representation */

bits32 packDna16(DNA *in);
/* pack 16 bases into a word */

bits16 packDna8(DNA *in);
/* Pack 8 bases into a short word */

UBYTE packDna4(DNA *in);
/* Pack 4 bases into a UBYTE */

void unpackDna(bits32 *tiles, int tileCount, DNA *out);
/* Unpack DNA. Expands to 16x tileCount in output. */

void unpackDna4(UBYTE *tiles, int byteCount, DNA *out);
/* Unpack DNA. Expands to 4x byteCount in output. */

void unalignedUnpackDna(bits32 *tiles, int start, int size, DNA *unpacked);
/* Unpack into out, even though not starting/stopping on tile 
 * boundaries. */

int intronOrientationMinSize(DNA *iStart, DNA *iEnd, int minIntronSize);
/* Given a gap in genome from iStart to iEnd, return 
 * Return 1 for GT/AG intron between left and right, -1 for CT/AC, 0 for no
 * intron. */

int intronOrientation(DNA *iStart, DNA *iEnd);
/* Given a gap in genome from iStart to iEnd, return 
 * Return 1 for GT/AG intron between left and right, -1 for CT/AC, 0 for no
 * intron.  Assumes minIntronSize of 32. */

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

int  dnaOrAaScoreMatch(char *a, char *b, int size, int matchScore, int mismatchScore, 
	char ignore);
/* Compare two sequences (without inserts or deletions) and score. */

void writeSeqWithBreaks(FILE *f, char *letters, int letterCount, int maxPerLine);
/* Write out letters with newlines every maxLine. */

int tailPolyASizeLoose(DNA *dna, int size);
/* Return size of PolyA at end (if present).  This allows a few non-A's as 
 * noise to be trimmed too, but skips first two aa for taa stop codon. 
 * It is less conservative in extending the polyA region than maskTailPolyA. */

int headPolyTSizeLoose(DNA *dna, int size);
/* Return size of PolyT at start (if present).  This allows a few non-T's as 
 * noise to be trimmed too, but skips last two tt for revcomp'd taa stop 
 * codon.  
 * It is less conservative in extending the polyA region than maskHeadPolyT. */

int maskTailPolyA(DNA *dna, int size);
/* Convert PolyA at end to n.  This allows a few non-A's as noise to be 
 * trimmed too.  Returns number of bases trimmed.  */

int maskHeadPolyT(DNA *dna, int size);
/* Convert PolyT at start.  This allows a few non-T's as noise to be 
 * trimmed too.  Returns number of bases trimmed.  */

boolean isDna(char *poly, int size);
/* Return TRUE if letters in poly are at least 90% ACGTU */

boolean isAllDna(char *poly, int size);
/* Return TRUE if letters in poly are 100% ACGTU */

#endif /* DNAUTIL_H */
