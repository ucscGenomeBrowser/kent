/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
#include "common.h"
#include "dnautil.h"

/* The dread codon table. */
struct _cdntbl
    {
    DNA *codon;	/* Lower case. */
    char protCode;	/* Lower case. */
    };
static struct _cdntbl _codonTable [64] = 
{
    {"ttt", 'f',},
    {"ttc", 'f',},
    {"tta", 'l',},
    {"ttg", 'l',},

    {"tct", 's',},
    {"tcc", 's',},
    {"tca", 's',},
    {"tcg", 's',},

    {"tat", 'y',},
    {"tac", 'y',},
    {"taa", 0,},
    {"tag", 0,},

    {"tgt", 'c',},
    {"tgc", 'c',},
    {"tga", 0,},
    {"tgg", 'w',},


    {"ctt", 'l',},
    {"ctc", 'l',},
    {"cta", 'l',},
    {"ctg", 'l',},

    {"cct", 'p',},
    {"ccc", 'p',},
    {"cca", 'p',},
    {"ccg", 'p',},

    {"cat", 'h',},
    {"cac", 'h',},
    {"caa", 'q',},
    {"cag", 'q',},

    {"cgt", 'r',},
    {"cgc", 'r',},
    {"cga", 'r',},
    {"cgg", 'r',},


    {"att", 'i',},
    {"atc", 'i',},
    {"ata", 'i',},
    {"atg", 'm',},

    {"act", 't',},
    {"acc", 't',},
    {"aca", 't',},
    {"acg", 't',},

    {"aat", 'n',},
    {"aac", 'n',},
    {"aaa", 'k',},
    {"aag", 'k',},

    {"agt", 's',},
    {"agc", 's',},
    {"aga", 'r',},
    {"agg", 'r',},


    {"gtt", 'v',},
    {"gtc", 'v',},
    {"gta", 'v',},
    {"gtg", 'v',},

    {"gct", 'a',},
    {"gcc", 'a',},
    {"gca", 'a',},
    {"gcg", 'a',},

    {"gat", 'd',},
    {"gac", 'd',},
    {"gaa", 'e',},
    {"gag", 'e',},

    {"ggt", 'g',},
    {"ggc", 'g',},
    {"gga", 'g',},
    {"ggg", 'g',},
};

/* A table that gives values 0 for t
			     1 for c
			     2 for a
			     3 for g
 * (which is order aa's are in biochemistry codon tables)
 * and gives -1 for all others. */
int ntVal[256];
int ntVal5[256];
int ntValNoN[256]; /* Like ntVal, but with T_BASE_VAL in place of -1 for nonexistent ones. */
DNA valToNt[5];

static boolean inittedNtVal = FALSE;



static void initNtVal()
{
if (!inittedNtVal)
    {
    int i;
    for (i=0; i<ArraySize(ntVal); i++)
        {
	ntVal[i] = -1;
        ntValNoN[i] = T_BASE_VAL;
	if (isspace(i) || isdigit(i))
	    ntVal5[i] = -1;
	else
	    ntVal5[i] = N_BASE_VAL;
        }
    ntVal5['t'] = ntVal5['T'] = ntValNoN['t'] = ntValNoN['T'] = ntVal['t'] = ntVal['T'] = T_BASE_VAL;
    ntVal5['u'] = ntVal5['U'] = ntValNoN['u'] = ntValNoN['U'] = ntVal['u'] = ntVal['U'] = U_BASE_VAL;
    ntVal5['c'] = ntVal5['C'] = ntValNoN['c'] = ntValNoN['C'] = ntVal['c'] = ntVal['C'] = C_BASE_VAL;
    ntVal5['a'] = ntVal5['A'] = ntValNoN['a'] = ntValNoN['A'] = ntVal['a'] = ntVal['A'] = A_BASE_VAL;
    ntVal5['g'] = ntVal5['G'] = ntValNoN['g'] = ntValNoN['G'] = ntVal['g'] = ntVal['G'] = G_BASE_VAL;

    valToNt[T_BASE_VAL] = 't';
    valToNt[C_BASE_VAL] = 'c';
    valToNt[A_BASE_VAL] = 'a';
    valToNt[G_BASE_VAL] = 'g';
    valToNt[N_BASE_VAL] = 'n';
    inittedNtVal = TRUE;
    }
}


/* Returns one letter code for protein, 
   0 for stop codon or X for bad input,
 */
char lookupCodon(DNA *dna)
{
int ix;
int i;
char c;

if (!inittedNtVal)
    initNtVal();
ix = 0;
for (i=0; i<3; ++i)
    {
    int bv = ntVal[dna[i]];
    if (bv<0)
	return 'X';
    ix = (ix<<2) + bv;
    }
c = _codonTable[ix].protCode;
c = toupper(c);
return c;
}

Codon codonVal(DNA *start)
/* Return value from 0-63 of codon starting at start. 
 * Returns -1 if not a codon. */
{
int v1,v2,v3;

if ((v1 = ntVal[start[0]]) < 0)
    return -1;
if ((v2 = ntVal[start[1]]) < 0)
    return -1;
if ((v3 = ntVal[start[2]]) < 0)
    return -1;
return ((v1<<4) + (v2<<2) + v3);
}

DNA *valToCodon(int val)
/* Return  codon corresponding to val (0-63) */
{
assert(val >= 0 && val < 64);
return _codonTable[val].codon;
}

void dnaTranslateSome(DNA *dna, char *out, int outSize)
/* Translate up to outSize bases of DNA.  Output will be zero terminated. */
{
int i;
int dnaSize;
int protSize = 0;

outSize -= 1;  /* Room for terminal zero */
dnaSize = strlen(dna);
for (i=0; i<dnaSize-2; i+=3)
    {
    if (protSize >= outSize)
        break;
    if ((out[protSize++] = lookupCodon(dna+i)) == 0)
        break;
    }
out[protSize] = 0;
}

/* A little array to help us decide if a character is a 
 * nucleotide, and if so convert it to lower case. */
char ntChars[256];

static void initNtChars()
{
static boolean initted = FALSE;

if (!initted)
    {
    zeroBytes(ntChars, sizeof(ntChars));
    ntChars['a'] = ntChars['A'] = 'a';
    ntChars['c'] = ntChars['C'] = 'c';
    ntChars['g'] = ntChars['G'] = 'g';
    ntChars['t'] = ntChars['T'] = 't';
    ntChars['n'] = ntChars['N'] = 'n';
    ntChars['u'] = ntChars['U'] = 'u';
    ntChars['-'] = 'n';
    initted = TRUE;
    }
}

char ntMixedCaseChars[256];

static void initNtMixedCaseChars()
{
static boolean initted = FALSE;

if (!initted)
    {
    zeroBytes(ntMixedCaseChars, sizeof(ntMixedCaseChars));
    ntMixedCaseChars['a'] = 'a';
    ntMixedCaseChars['A'] = 'A';
    ntMixedCaseChars['c'] = 'c';
    ntMixedCaseChars['C'] = 'C';
    ntMixedCaseChars['g'] = 'g';
    ntMixedCaseChars['G'] = 'G';
    ntMixedCaseChars['t'] = 't';
    ntMixedCaseChars['T'] = 'T';
    ntMixedCaseChars['n'] = 'n';
    ntMixedCaseChars['N'] = 'N';
    ntMixedCaseChars['u'] = 'u';
    ntMixedCaseChars['U'] = 'U';
    ntMixedCaseChars['-'] = 'n';
    initted = TRUE;
    }
}

/* Another array to help us do complement of DNA */
DNA ntCompTable[256];
static boolean inittedCompTable = FALSE;

static void initNtCompTable()
{
zeroBytes(ntCompTable, sizeof(ntCompTable));
ntCompTable['a'] = 't';
ntCompTable['c'] = 'g';
ntCompTable['g'] = 'c';
ntCompTable['t'] = 'a';
ntCompTable['n'] = 'n';
ntCompTable['-'] = '-';
ntCompTable['A'] = 'T';
ntCompTable['C'] = 'G';
ntCompTable['G'] = 'C';
ntCompTable['T'] = 'A';
ntCompTable['N'] = 'N';
ntCompTable['('] = ')';
ntCompTable[')'] = '(';
inittedCompTable = TRUE;
}

/* Reverse complement DNA. */
void reverseComplement(DNA *dna, long length)
{
int i;
reverseBytes(dna, length);

if (!inittedCompTable) initNtCompTable();
for (i=0; i<length; ++i)
    {
    *dna = ntCompTable[*dna];
    ++dna;
    }
}

/* Reverse offset - return what will be offset (0 based) to
 * same member of array after array is reversed. */
long reverseOffset(long offset, long arraySize)
{
return arraySize-1 - offset;
}

/* Switch start/end (zero based half open) coordinates
 * to opposite strand. */
void reverseIntRange(int *pStart, int *pEnd, int size)
{
int temp;
temp = *pStart;
*pStart = size - *pEnd;
*pEnd = size - temp;
}

/* Switch start/end (zero based half open) coordinates
 * to opposite strand. */
void reverseUnsignedRange(unsigned *pStart, unsigned *pEnd, int size)
{
unsigned temp;
temp = *pStart;
*pStart = size - *pEnd;
*pEnd = size - temp;
}


/* Convert T's to U's */
void toRna(DNA *dna)
{
DNA c;
for (;;)
    {
    c = *dna;
    if (c == 't')
	*dna = 'u';
    else if (c == 'T')
	*dna = 'U';
    else if (c == 0)
	break;
    ++dna;
    }
}

int nextPowerOfFour(long x)
/* Return next power of four that would be greater or equal to x.
 * For instance if x < 4, return 1, if x < 16 return 2.... 
 * (From biological point of view how many bases are needed to
 * code this number.) */
{
int count = 1;
while (x > 4)
    {
    count += 1;
    x >>= 2;
    }
return count;
}


/* Return how long DNA will be after non-DNA is filtered out. */
long dnaFilteredSize(char *rawDna)
{
DNA c;
long count = 0;
initNtChars();
while ((c = *rawDna++) != 0)
    {
    if (ntChars[c]) ++count;
    }
return count;
}

/* Filter out non-DNA characters and change to lower case. */
void dnaFilter(char *in, DNA *out)
{
DNA c;
initNtChars();
while ((c = *in++) != 0)
    {
    if ((c = ntChars[c]) != 0) *out++ = c;
    }
*out++ = 0;
}

void dnaFilterToN(char *in, DNA *out)
/* Change all non-DNA characters to N. */
{
DNA c;
initNtChars();
while ((c = *in++) != 0)
    {
    if ((c = ntChars[c]) != 0) *out++ = c;
    else *out++ = 'n';
    }
*out++ = 0;
}

/* Filter out non-DNA characters but leave case intact. */
void dnaMixedCaseFilter(char *in, DNA *out)
{
DNA c;
initNtMixedCaseChars();
while ((c = *in++) != 0)
    {
    if ((c = ntMixedCaseChars[c]) != 0) *out++ = c;
    }
*out++ = 0;
}

void dnaBaseHistogram(DNA *dna, int dnaSize, int histogram[4])
/* Count up frequency of occurance of each base and store 
 * results in histogram. */
{
int val;
zeroBytes(histogram, 4*sizeof(int));
while (--dnaSize >= 0)
    {
    if ((val = ntVal[*dna++]) >= 0)
        ++histogram[val];
    }
}

bits32 packDna16(DNA *in)
/* pack 16 bases into a word */
{
bits32 out = 0;
int count = 16;
int bVal;
while (--count >= 0)
    {
    if ((bVal = ntVal[*in++]) < 0)
        bVal = T_BASE_VAL;
    out <<= 2;
    out += bVal;
    }
return out;
}

bits16 packDna8(DNA *in)
/* Pack 8 bases into a short word */
{
bits16 out = 0;
int count = 8;
int bVal;
while (--count >= 0)
    {
    if ((bVal = ntVal[*in++]) < 0)
        bVal = T_BASE_VAL;
    out <<= 2;
    out += bVal;
    }
return out;
}

void unpackDna(bits32 *tiles, int tileCount, DNA *out)
/* Unpack DNA. Expands to 16x tileCount in output. */
{
int i, j;
bits32 tile;

for (i=0; i<tileCount; ++i)
    {
    tile = tiles[i];
    for (j=15; j>=0; --j)
        {
        out[j] = valToNt[tile & 0x3];
        tile >>= 2;
        }
    out += 16;
    }
}


static void checkSizeTypes()
/* Make sure that some of our predefined types are the right size. */
{
assert(sizeof(UBYTE) == 1);
assert(sizeof(WORD) == 2);
assert(sizeof(bits32) == 4);
assert(sizeof(bits16) == 2);
}


int intronOrientation(DNA *iStart, DNA *iEnd)
/* Given a gap in genome from iStart to iEnd, return 
 * Return 1 for GT/AG intron between left and right, -1 for CT/AC, 0 for no
 * intron.  Assumes DNA is lower cased. */
{
if (iEnd - iStart < 32)
    return 0;
if (iStart[0] == 'g' && iStart[1] == 't' && iEnd[-2] == 'a' && iEnd[-1] == 'g')
    {
    return 1;
    }
else if (iStart[0] == 'c' && iStart[1] == 't' && iEnd[-2] == 'a' && iEnd[-1] == 'c')
    {
    return -1;
    }
else
    return 0;
}


void dnaUtilOpen()
/* Initialize stuff herein. */
{
static boolean opened = FALSE;
if (!opened)
    {
    checkSizeTypes();
    initNtVal();
    initNtChars();
    initNtCompTable();
    opened = TRUE;
    }
}

