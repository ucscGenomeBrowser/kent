/* iupac - routines to help cope with IUPAC ambiguity codes in DNA sequence. */

#include "common.h"
#include "dnautil.h"
#include "iupac.h"

boolean iupacMatchLower(char iupac, char dna)
/* See if iupac ambiguity code matches dna character where
 * both are lower case */
{
switch (iupac)
    {
    case 'a':
        return dna == 'a';
    case 'c':
        return dna == 'c';
    case 'g':
        return dna == 'g';
    case 't':
    case 'u':
        return dna == 't';
    case 'r':
        return dna == 'a' || dna == 'g';
    case 'y':
        return dna == 'c' || dna == 't';
    case 's':
        return dna == 'g' || dna == 'c';
    case 'w':
        return dna == 'a' || dna == 't';
    case 'k':
        return dna == 'g' || dna == 't';
    case 'm':
        return dna == 'a' || dna == 'c';
    case 'b':
        return dna == 'c' || dna == 'g' || dna == 't';
    case 'd':
        return dna == 'a' || dna == 'g' || dna == 't';
    case 'h':
        return dna == 'a' || dna == 'c' || dna == 't';
    case 'v':
        return dna == 'a' || dna == 'c' || dna == 'g';
    case 'n':
        return TRUE;
    default:
        errAbort("Unrecognized IUPAC code '%c'", iupac);
	return FALSE;   // Not actually used but prevent compiler complaints
    }
}

boolean iupacMatch(char iupac, char dna)
/* See if iupac ambiguity code matches dna character */
{
return iupacMatchLower(tolower(iupac), tolower(dna));
}

boolean isIupacLower(char c)
/* See if iupac c is a legal (lower case) iupac char */
{
switch (c)
    {
    case 'a':
    case 'c':
    case 'g':
    case 't':
    case 'u':
    case 'r':
    case 'y':
    case 's':
    case 'w':
    case 'k':
    case 'm':
    case 'b':
    case 'd':
    case 'h':
    case 'v':
    case 'n':
        return TRUE;
    default:
	return FALSE;
    }
}

boolean isIupac(char c)
/* See if iupac c is a legal iupac char */
{
return isIupacLower(tolower(c));
}

void iupacFilter(char *in, char *out)
/* Filter out non-DNA non-UIPAC ambiguity code characters and change to lower case. */
{
char c;
while ((c = *in++) != 0)
    {
    c = tolower(c);
    if (isIupacLower(c))
       *out++ = c;
    }
*out++ = 0;
}

boolean anyIupac(char *s)
/* Return TRUE if there are any IUPAC ambiguity codes in s */
{
dnaUtilOpen();
int c;
while ((c = *s++) != 0)
    {
    switch (c)
	{
	case 'r':
	case 'y':
	case 's':
	case 'w':
	case 'k':
	case 'm':
	case 'b':
	case 'd':
	case 'h':
	case 'v':
	case 'n':
	    return TRUE;
	}
    }
return FALSE;
}

char iupacComplementBaseLower(char iupac)
/* Return IUPAC complement for a single base */
{
switch (iupac)
    {
    case 'a':
        return 't';
    case 'c':
        return 'g';
    case 'g':
        return 'c';
    case 't':
    case 'u':
        return 'a';
    case 'r':
	return 'y';
    case 'y':
	return 'r';
    case 's':
	return 's';
    case 'w':
	return 'w';
    case 'k':
	return 'm';
    case 'm':
	return 'k';
    case 'b':
	return 'v';
    case 'd':
	return 'h';
    case 'h':
	return 'd';
    case 'v':
	return 'b';
    case 'n':
	return 'n';
    default:
        errAbort("Unrecognized IUPAC code '%c'", iupac);
	return 0;   // Just to keep compiler from complaining, control won't reach here.
    }
}

void iupacComplementLower(char *iupac, int iuSize)
/* Return IUPAC complement many bases. Assumes iupac is lower case. */
{
int i;
for (i=0; i<iuSize; ++i)
    iupac[i] = iupacComplementBaseLower(iupac[i]);
}

void iupacReverseComplement(char *iu, int iuSize)
/* Reverse complement a string containing DNA and IUPAC codes. Result will be always
 * lower case. */
{
toLowerN(iu, iuSize);
reverseBytes(iu, iuSize);
iupacComplementLower(iu, iuSize);
}

boolean iupacMatchStart(char *iupacPrefix, char *dnaString)
/* Return TRUE if start of DNA is compatible with iupac */
{
char iupac;
while ((iupac = *iupacPrefix++) != 0)
    {
    if (!iupacMatch(iupac, *dnaString++))
        return FALSE;
    }
return TRUE;
}

char *iupacIn(char *needle, char *haystack)
/* Return first place in haystack (DNA) that matches needle that may contain IUPAC codes. */
{
int needleSize = strlen(needle);
int haySize = strlen(haystack);
char *endOfHay = haystack + haySize - needleSize;
char *h;
for (h = haystack; h<=endOfHay; ++h)
    {
    if (iupacMatchStart(needle, h))
        return h;
    }
return NULL;
}

