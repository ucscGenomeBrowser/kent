/* iupac - routines to help cope with IUPAC ambiguity codes in DNA sequence. */

#ifndef IUPAC_H
#define IUPAC_H

boolean iupacMatch(char iupac, char dna);
/* See if iupac ambiguity code matches dna character */

boolean iupacMatchLower(char iupac, char dna);
/* See if iupac ambiguity code matches dna character where
 * both are lower case */

boolean isIupac(char c);
/* See if iupac c is a legal iupac char */

boolean isIupacLower(char c);
/* See if iupac c is a legal (lower case) iupac char */

void iupacFilter(char *in, char *out);
/* Filter out non-DNA non-UIPAC ambiguity code characters and change to lower case. */

boolean anyIupac(char *s);
/* Return TRUE if there are any IUPAC ambiguity codes in s */

char iupacComplementBaseLower(char iupac);
/* Return IUPAC complement for a single base */

void iupacComplementLower(char *iupac, int iuSize);
/* Return IUPAC complement many bases. Assumes iupac is lower case. */

void iupacReverseComplement(char *iu, int iuSize);
/* Reverse complement a string containing DNA and IUPAC codes. Result will be always
 * lower case. */

boolean iupacMatchStart(char *iupacPrefix, char *dnaString);
/* Return TRUE if start of DNA is compatible with iupac */

char *iupacIn(char *needle, char *haystack);
/* Return first place in haystack (DNA) that matches needle that may contain IUPAC codes. */

#endif /* IUPAC_H */

