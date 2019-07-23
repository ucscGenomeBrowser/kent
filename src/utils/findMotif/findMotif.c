/* findMotif - find specified motif in fa/nib/2bit files. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include <popcntintrin.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "dnaLoad.h"
#include "portable.h"
#include "memalloc.h"
#include "obscure.h"


char *chr = (char *)NULL;	/*	process the one chromosome listed */
char *motif = (char *)NULL;	/*	specified motif string */
unsigned motifLen = 0;		/*	length of motif	*/
unsigned misMatch = 0;		/*	command line arg	*/
unsigned long long motifVal;	/*	motif converted to a number	*/
unsigned long long complementVal;	/*	- strand complement	*/
boolean bedOutputOpt = TRUE;	/*	output bed file (default) */
boolean wigOutput = FALSE;	/*  output wiggle format instead of bed file */
char *strand = (char *)NULL;	/*	command line argument + or -	*/
boolean doPlusStrand = TRUE;	/*	strand argument can turn this off */
boolean doMinusStrand = TRUE;	/*	strand argument can turn this off */
static int lowerLimitMotifLength = 4;	/* must be at least 4 characters */

#define bitsPerLongLong	(8 * sizeof(unsigned long long))
#define perfectScore 1000	/*	mis-matches will lower the score */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "findMotif - find specified motif in sequence\n"
  "usage:\n"
  "   findMotif [options] -motif=<acgt...> sequence\n"
  "where:\n"
  "   sequence is a .fa , .nib or .2bit file or a file which is a list of sequence files.\n"
  "options:\n"
  "   -motif=<acgt...> - search for this specified motif (case ignored, [acgt] only)\n"
  "   NOTE: motif must be at least %d characters, less than %d\n"
  "   -chr=<chrN> - process only this one chrN from the sequence\n"
  "   -strand=<+|-> - limit to only one strand.  Default is both.\n"
  "   -bedOutput - output bed format (this is the default)\n"
  "   -wigOutput - output wiggle data format instead of bed file\n"
  "   -misMatch=N - allow N mismatches (0 default == perfect match)\n"
  "   -verbose=N - set information level [1-4]\n"
  "   -verbose=4 - will display gaps as bed file data lines to stderr",
lowerLimitMotifLength, (int)(bitsPerLongLong / 2)
  );
}

static struct optionSpec options[] = {
   {"chr", OPTION_STRING},
   {"strand", OPTION_STRING},
   {"motif", OPTION_STRING},
   {"misMatch", OPTION_INT},
   {"bedOutput", OPTION_BOOLEAN},
   {"wigOutput", OPTION_BOOLEAN},
   {NULL, 0},
};

/* Numerical values (and redefinitions here) for bases from danutil.h:
 *	#define T_BASE_VAL 0	3
 *	#define U_BASE_VAL 0	3
 *	#define C_BASE_VAL 1	1
 *	#define A_BASE_VAL 2	0
 *	#define G_BASE_VAL 3	2
 *  These do not work for the XOR comparison bit count,
 *      using the ones below instead
*/

static int dnaUtilBases[4];	/* translation values from dnautil.h */

#define A_BASE  0
#define C_BASE  1
#define G_BASE  2
#define T_BASE  3
#define U_BASE  3
#define N_BASE  4

static char bases[4];  /* for two-bits to ascii conversion */

static void initBases()
/* set up the translation matrix for two-bits to ascii */
{
bases[A_BASE] = 'A';
bases[C_BASE] = 'C';
bases[G_BASE] = 'G';
bases[T_BASE] = 'T';
dnaUtilBases[A_BASE_VAL] = A_BASE;
dnaUtilBases[C_BASE_VAL] = C_BASE;
dnaUtilBases[G_BASE_VAL] = G_BASE;
dnaUtilBases[T_BASE_VAL] = T_BASE;
}

static void misMatchString(char *stringReturn, unsigned long long misMatch,
    unsigned stringLength)
/* return ascii string ...*.....*.*.....*.. to indicate mis matches */
{
int i = 0;
unsigned long long twoBitMask = 0xc000000000000000;
int startShift = (bitsPerLongLong - (2 * stringLength));
twoBitMask >>= startShift;
while (twoBitMask)
    {
    stringReturn[i] = '.';
    if (twoBitMask & misMatch)
        stringReturn[i] = '*';
    ++i;
    twoBitMask >>= 2;
    }
stringReturn[i] = 0;
}       // static void misMatchString()

static void valToString(char *stringReturn, unsigned long long val,
    unsigned stringLength)
/* return ASCII string for binary sequence value */
{
unsigned long long twoBitMask = 0xc000000000000000;
int startShift = (bitsPerLongLong - (2 * stringLength));
int baseCount = 0;

/* shorter string than 32 characters needs to get the mask into
 * initial position.
 */
twoBitMask >>= startShift;

while (twoBitMask)
    {
    int base = (val & twoBitMask) >> ((62-startShift) - (baseCount * 2));
    stringReturn[baseCount++] = bases[base];
    twoBitMask >>= 2;
    }
stringReturn[baseCount] = 0;
}       //      static void valToString()

static void scanSeq(struct dnaSeq *seq)
{
int i, val;
FILE *nf = NULL;
DNA *dna;
unsigned long long mask;
unsigned long long chromPosition = 0;
register unsigned long long incomingVal = 0;
unsigned long long incomingLength = 0;
unsigned long long posFound = 0;
unsigned long long negFound = 0;
unsigned long long posPreviousPosition = 0;
unsigned long long negPreviousPosition = 0;
register unsigned long long posNeedle = motifVal;
register unsigned long long negNeedle = complementVal;
unsigned long long enterGap = 1;
boolean inGap = FALSE;
unsigned long long gapCount = 0;

mask = 3;
for (i=1; i < motifLen; ++i )
	mask = (mask << 2) | 3;
verbose(3, "#\tsequence: %s size: %d, motifMask: %#llx\n", seq->name, seq->size, mask);
verbose(3, "#\tmotif numerical value: %llu (%#llx)\n", posNeedle, posNeedle);

/* Need "chrom" */

dna = seq->dna;
for (i=0; i < seq->size; ++i)
    {
    ++chromPosition;
    int nVal = ntVal[(int)dna[i]];
    if (nVal < 0)
	val = N_BASE;
    else
	val = dnaUtilBases[ntVal[(int)dna[i]]];
    switch (val)
	{
	case T_BASE:
	case C_BASE:
	case A_BASE:
	case G_BASE:
	    incomingVal = mask & ((incomingVal << 2) | val);
	    if (! incomingLength)
		{
		if (inGap &&
		 (((long long int)chromPosition - (long long int)enterGap) > 0))
		    {
		    ++gapCount;
		    verbose(3,
			    "#\treturn from gap at %llu, gap length: %llu\n",
			    chromPosition, chromPosition - enterGap);
		    verbose(4, "#GAP %s\t%llu\t%llu\t%llu\t%llu\t%s\n",
			    seq->name, enterGap-1, chromPosition-1, gapCount,
			    chromPosition - enterGap, "+");
		    /* assume no more gaps, this is safe to a chrom size
		     *	of 1 Tb */
		    enterGap = (unsigned long long)BIGNUM << 10;
		    }
		}
	    inGap = FALSE;
	    ++incomingLength;

	    char misMatchPrint[33];
	    int score = perfectScore;
	    if (doPlusStrand && (incomingLength >= motifLen))
		{
		unsigned long long bitsXor = incomingVal ^ posNeedle;
		boolean matchOK = FALSE;
		if (bitsXor)
		    {
		    if (misMatch)
			{
			/* possible bitsXor bit values: 01 10 11
			 *  turn those three values into just: 01
			 */
			bitsXor=(bitsXor|(bitsXor >> 1)) & 0x5555555555555555;
			/* now count those 1 bits on */
			int bitsOn = _mm_popcnt_u64(bitsXor);
			if (bitsOn <= misMatch)
			    {
			    misMatchString(misMatchPrint, bitsXor, motifLen);
verbose(2, "# misMatch count: %d position: %llu\n", bitsOn, chromPosition-motifLen);
			    matchOK = TRUE;
			    score -= bitsOn;
			    }
			}
		    }
		else
		    {
		    matchOK = TRUE;
		    }
		if (matchOK)
		    {
                    char needleString[33];
                    char incomingString[33];
                    valToString(needleString, posNeedle, motifLen);
                    valToString(incomingString, incomingVal, motifLen);
		    if (score < perfectScore)
			{
			verbose(2, "#\tmotif: %s\n", needleString);
			verbose(2, "#\t       %s\n", misMatchPrint);
			verbose(2, "#\tmatch: %s\n", incomingString);
			}
		++posFound;
		if (wigOutput)
		    printf("%llu 1 %#llx == %#llx\n", chromPosition-motifLen+1, incomingVal&mask,posNeedle);
		else
		    printf("%s\t%llu\t%llu\t%llu\t%d\t%s\n", seq->name, chromPosition-motifLen, chromPosition, posFound+negFound, score, "+");

		if ((posPreviousPosition + motifLen) > chromPosition)
		    verbose(2, "#\toverlapping + at: %s:%llu-%llu\n", seq->name, posPreviousPosition, chromPosition);
		posPreviousPosition = chromPosition;
		    }
		}

	    if (doMinusStrand && (incomingLength >= motifLen)
		&& (incomingVal == negNeedle))
		{
		score = perfectScore;
		unsigned long long bitsXor = incomingVal ^ negNeedle;
		boolean matchOK = FALSE;
		if (bitsXor)
		    {
		    if (misMatch)
			{
			/* possible bitsXor bit values: 01 10 11
			 *  turn those three values into just: 01
			 */
			bitsXor=(bitsXor|(bitsXor >> 1)) & 0x5555555555555555;
			/* now count those 1 bits on */
			int bitsOn = _mm_popcnt_u64(bitsXor);
			if (bitsOn <= misMatch)
			    {
			    misMatchString(misMatchPrint, bitsXor, motifLen);
verbose(2, "# misMatch count: %d position: %llu\n", bitsOn, chromPosition-motifLen);
			    matchOK = TRUE;
			    score -= bitsOn;
			    }
			}
		    }
		else
		    {
		    matchOK = TRUE;
		    }
		if (matchOK)
		    {
                    char needleString[33];
                    char incomingString[33];
                    valToString(needleString, negNeedle, motifLen);
                    valToString(incomingString, incomingVal, motifLen);
		    if (score < perfectScore)
			{
			verbose(2, "#\tmotif: %s\n", needleString);
			verbose(2, "#\t       %s\n", misMatchPrint);
			verbose(2, "#\tmatch: %s\n", incomingString);
			}
		++negFound;
		if (wigOutput)
		    printf("%llu -1 %#llx == %#llx\n", chromPosition-motifLen+1, incomingVal&mask,negNeedle);
		else
		    printf("%s\t%llu\t%llu\t%llu\t%d\t%s\n", seq->name, chromPosition-motifLen, chromPosition, posFound+negFound, 1000, "-");

		if ((negPreviousPosition + motifLen) > chromPosition)
		    verbose(2, "#\toverlapping - at: %s:%llu-%llu\n", seq->name, negPreviousPosition, chromPosition);
		negPreviousPosition = chromPosition;
		    }
		}
	    break;

	default:
	    if (incomingLength)
		{
		verbose(3, "#\tenter gap at %llu\n", chromPosition);
		enterGap = chromPosition;
		}
	    inGap = TRUE;
	    incomingVal = 0;
	    incomingLength = 0;
	    break;
	}
    }
if (inGap && (((long long int)chromPosition - (long long int)enterGap) > 0))
    {
    ++gapCount;
    verbose(3,
	    "#\treturn from gap at %llu, gap length: %llu\n",
	    chromPosition+1, chromPosition - enterGap + 1);
    verbose(4, "#GAP %s\t%llu\t%llu\t%llu\t%llu\t%s\n",
	    seq->name, enterGap-1, chromPosition, gapCount,
	    chromPosition - enterGap + 1, "+");
    }

verbose(2, "#\tfound: %llu times + strand, %llu times - strand\n",
    posFound, negFound );
verbose(2, "#\t%% of chromosome: %g %% + strand %g %% - strand\n",
    (double)(posFound*motifLen)/(double)chromPosition,
    (double)(negFound*motifLen)/(double)chromPosition);

carefulClose(&nf);
}

static void findMotif(char *input)
/* findMotif - find specified motif in sequence file. */
{
struct dnaLoad *dl = dnaLoadOpen(input);
struct dnaSeq *seq;

while ((seq = dnaLoadNext(dl)) != NULL)
    {
    verbose(2, "#\tprocessing: %s\n", seq->name);
    scanSeq(seq);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
int i;
char *cp;
unsigned long long reversed;
size_t maxAlloc;
char asciiAlloc[32];

optionInit(&argc, argv, options);

if (argc < 2)
    usage();

initBases();

maxAlloc = 2100000000 *
	 (((sizeof(size_t)/4)*(sizeof(size_t)/4)*(sizeof(size_t)/4)));
sprintLongWithCommas(asciiAlloc, (long long) maxAlloc);
verbose(4, "#\tmaxAlloc: %s\n", asciiAlloc);
setMaxAlloc(maxAlloc);
/* produces: size_t is 4 == 2100000000 ~= 2^31 = 2Gb
 *      size_t is 8 = 16800000000 ~= 2^34 = 16 Gb
 */

dnaUtilOpen();

motif = optionVal("motif", NULL);
chr = optionVal("chr", NULL);
strand = optionVal("strand", NULL);
misMatch = optionInt("misMatch", misMatch);
bedOutputOpt = optionExists("bedOutput");
wigOutput = optionExists("wigOutput");

if (wigOutput)
    bedOutputOpt = FALSE;
else
    bedOutputOpt = TRUE;

motifLen = strlen(motif);

if (chr)
    verbose(2, "#\tprocessing chr: %s\n", chr);
if (strand)
    verbose(2, "#\tprocessing strand: '%s'\n", strand);
if (motif)
    verbose(2, "#\tsearching for motif: %s, length: %d\n", motif, motifLen);
else
    {
    warn("ERROR: -motif string empty, please specify a motif\n");
    usage();
    }
if (misMatch)
    verbose(2, "#\tallow mismatch %d bases\n", misMatch);
verbose(2, "#\ttype output: %s\n", wigOutput ? "wiggle data" : "bed format");
verbose(2, "#\tspecified sequence: %s\n", argv[1]);
verbose(2, "#\tsizeof(motifVal): %d\n", (int)sizeof(motifVal));
if (strand)
    {
    if (! (sameString(strand,"+") | sameString(strand,"-")))
	{
	warn("ERROR: -strand specified ('%s') is not + or - ?\n", strand);
	usage();
	}
    /*	They are both on by default, turn off the one not specified */
    if (sameString(strand,"-"))
	doPlusStrand = FALSE;
    if (sameString(strand,"+"))
	doMinusStrand = FALSE;
    }
/*	at two bits per character, size limit of motif is
 *	(number of bits in motifVal) / 2
 */
if (motifLen > (bitsPerLongLong/2) )
    {
    warn("ERROR: motif string '%s'\n\ttoo long at %d characters, limit %d\n", motif, motifLen, (int)(bitsPerLongLong/2));
    usage();
    }
if (motifLen < lowerLimitMotifLength)
    {
    warn("ERROR: motif string must be at least %d characters\n", lowerLimitMotifLength);
    usage();
    }
/* convert the motif string into 2 bits per base into the motifVal */
cp = motif;
motifVal = 0;
complementVal = 0;
for (i = 0; i < motifLen; ++i)
    {
	switch (*cp)
	{
	case 'a':
	case 'A':
	    motifVal = (motifVal << 2) | A_BASE;
	    complementVal = (complementVal << 2) | T_BASE;
	    break;
	case 'c':
	case 'C':
	    motifVal = (motifVal << 2) | C_BASE;
	    complementVal = (complementVal << 2) | G_BASE;
	    break;
	case 'g':
	case 'G':
	    motifVal = (motifVal << 2) | G_BASE;
	    complementVal = (complementVal << 2) | C_BASE;
	    break;
	case 't':
	case 'T':
	    motifVal = (motifVal << 2) | T_BASE;
	    complementVal = (complementVal << 2) | A_BASE;
	    break;
	default:
	    warn("ERROR: character in motif: '%c' is not one of ACGT\n", *cp);
	    usage();
	}
	++cp;
    }
char needleString[33];
valToString(needleString, motifVal, motifLen);
verbose(2, "#\tmotif: '%s' length %d\n", needleString, motifLen);
/* and swap the complement value to get it reversed */
reversed = 0;
for (i = 0; i < motifLen; ++i)
    {
    int base;
    base = complementVal & 3;
    reversed = (reversed << 2) | base;
    complementVal >>= 2;
    }
complementVal = reversed;
valToString(needleString, complementVal, motifLen);
verbose(2, "#\trever: '%s' length %d\n", needleString, motifLen);
verbose(3, "#\t     motif numerical value: %llu (%#llx)\n", motifVal, motifVal);
verbose(3, "#\tcomplement numerical value: %llu (%#llx)\n", complementVal, complementVal);

findMotif(argv[1]);
return 0;
}
