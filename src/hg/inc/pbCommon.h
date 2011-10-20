/* pbCommon - contains data shared within the pb* family of programs
 * (which are used in building the Proteome Browser and data */

#ifndef PBCOMMON_H
#define PBCOMMON_H

/* define the number of expected amino acid residues, including wildcards */
#define NUM_AMINO_ACIDS 26 

/* define the expected amino acid residues, including wildcards and 
 * nonstandard residues.  It's possible that the order is important*/
#define AA_ALPHABET "WCMHYNFIDQKRTVPGEASLXZBJOU"

/* define the size of the standard amino acid alphabet */
#define NUM_STANDARD_AMINO_ACIDS 20

/* define the standard amino acid alphabet.  The order here _might_ be
 * important (I'm guessing that the unusual order is there for a reason) */
#define STNADARD_AA_ALPHABET "WCMHYNFIDQKRTVPGEASL"

#endif /* PBCOMMON_H */

