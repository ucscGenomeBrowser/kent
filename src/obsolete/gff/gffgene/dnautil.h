/* Some stuff that you'll likely need in any program that works with
 * DNA.  
 *
 * Assumes that DNA is stored as a character.
 * The DNA it generates will include the bases 
 * as lowercase tcag.  It will generally accept
 * uppercase as well, and also 'n' or 'N' or '-'
 * for unknown bases. */

void dnaUtilOpen(); /* Good idea to call this before using any arrays
		     * here.  */

/* Numerical values for bases. */
#define T_BASE_VAL 0
#define U_BASE_VAL 0
#define C_BASE_VAL 1
#define A_BASE_VAL 2
#define G_BASE_VAL 3

char lookupCodon(); /* Return single letter code (upper case) for protein */
/* Parameter - char *dna - 1st three letters used for lookup. */


/* A little array to help us decide if a character is a 
 * nucleotide, and if so convert it to lower case. 
 * Contains zeroes for characters that aren't used
 * in DNA sequence. */
extern char ntChars[256];

/* Another array to help us do complement of DNA  */
char ntCompTable[256];

void reverseComplement(); /* Reverse complement DNA. */
/* Parameters :
 *    char *dna;  Dna to reverse complement
 *    long length;  Length of DNA
 */


/* Reverse offset - return what will be offset (0 based) to
 * same member of array after array is reversed. */
long reverseOffset();
/* Parameters:
 * long offset;
 * long arraySize;
 */
