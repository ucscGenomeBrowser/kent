/**************************************************
 * Copyright Jim Kent 2000.  All Rights Reserved. * 
 * Use permitted only by explicit agreement with  *
 * Jim Kent (jim_kent@pacbell.net).               *
 **************************************************/

#include "common.h"
#include "dnautil.h"

/* The dread codon table. */
struct _cdntbl
    {
    char *codon;	/* Lower case. */
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
    {"taa", '0',},
    {"tag", '0',},

    {"tgt", 'c',},
    {"tgc", 'c',},
    {"tga", '0',},
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
int dnaBaseVal[256];
static boolean inittedBaseVal = FALSE;

static void initBaseVal()
{
if (!inittedBaseVal)
    {
    int i;
    for (i=0; i<ArraySize(dnaBaseVal); i++)
	dnaBaseVal[i] = -1;
    dnaBaseVal['t'] = dnaBaseVal['T'] = T_BASE_VAL;
    dnaBaseVal['u'] = dnaBaseVal['U'] = U_BASE_VAL;
    dnaBaseVal['c'] = dnaBaseVal['C'] = C_BASE_VAL;
    dnaBaseVal['a'] = dnaBaseVal['A'] = A_BASE_VAL;
    dnaBaseVal['g'] = dnaBaseVal['G'] = G_BASE_VAL;
    inittedBaseVal = TRUE;
    }
}

/* Returns one letter code for protein, 
   0 for stop codon or X for bad input,
 */
char lookupCodon(dna)
char *dna;
{
int ix;
int i;
char c;

if (!inittedBaseVal)
    initBaseVal();
ix = 0;
for (i=0; i<3; ++i)
    {
    int bv = dnaBaseVal[dna[i]];
    if (bv<0)
	return 'X';
    ix = (ix<<2) + bv;
    }
c = _codonTable[ix].protCode;
c = toupper(c);
return c;
}


/* A little array to help us decide if a character is a 
 * nucleotide, and if so convert it to lower case. */
char ntChars[256];

static void initNtChars()
{
zeroBytes(ntChars, sizeof(ntChars));
ntChars['a'] = ntChars['A'] = 'a';
ntChars['c'] = ntChars['C'] = 'c';
ntChars['g'] = ntChars['G'] = 'g';
ntChars['t'] = ntChars['T'] = 't';
ntChars['n'] = ntChars['N'] = 'n';
ntChars['-'] = '-';
}

/* Another array to help us do complement of DNA */
char ntCompTable[256];
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
inittedCompTable = TRUE;
}

/* Reverse complement DNA. */
void reverseComplement(dna, length)
char *dna;
long length;
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
long reverseOffset(offset, arraySize)
long offset;
long arraySize;
{
return arraySize-1 - offset;
}


/* Initialize stuff herein. */
void dnaUtilOpen()
{
static opened = FALSE;
if (!opened)
    {
    initBaseVal();
    initNtChars();
    initNtCompTable();
    opened = TRUE;
    }
}

