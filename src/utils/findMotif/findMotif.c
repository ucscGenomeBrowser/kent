/* findMotif - find specified motif in nib files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "portable.h"

static char const rcsid[] = "$Id: findMotif.c,v 1.2 2004/02/27 00:30:27 hiram Exp $";

char *chr = (char *)NULL;	/*	process the one chromosome listed */
char *motif = (char *)NULL;	/*	specified motif string */
unsigned motifLen = 0;		/*	length of motif	*/
unsigned long long motifVal;	/*	motif converted to a number	*/

void usage()
/* Explain usage and exit. */
{
errAbort(
  "findMotif - find specified motif in nib files\n"
  "usage:\n"
  "   findMotif [options] -motif=acgt nibDir\n"
  "options:\n"
  "   nibDir - path to nib directory, relative or absolute path OK\n"
  "   -motif=acgt - search for this specified motif (case ignored, acgt only)\n"
  "   -chr=<chrN> - process only this one chrN from the nibDir\n"
  "   NOTE: motif must be longer than 4 characters, less than 17"
  );
}

static struct optionSpec options[] = {
   {"chr", OPTION_STRING},
   {"motif", OPTION_STRING},
   {NULL, 0},
};

static void scanNib(char *nibFile, char *chrom)
{
int chromSize, start, end, blockSize;
int i, val;
int winSize=131072;
FILE *nf = NULL;
struct dnaSeq *seq = NULL;
DNA *dna;
int blockCount = 0;
unsigned long long mask;
unsigned long long chromPosition = 0;
unsigned long long incomingVal = 0;
unsigned long long incomingLength = 0;
unsigned long long timesFound = 0;
unsigned long long previousPosition = 0;

nibOpenVerify(nibFile, &nf, &chromSize);
mask = 3;
for (i=1; i < motifLen; ++i )
	mask = (mask << 2) | 3;
verbose(2, "#\tnib: %s size: %d, motifMask: %#lx\n", nibFile, chromSize, mask);
verbose(2, "#\tmotif numerical value: %llu (%#lx)\n", motifVal, motifVal);
for (start=0; start<chromSize; start = end)
    {
    end = start + winSize;
    if (end > chromSize)
	end = chromSize;
    blockSize = end - start;
    seq = nibLdPart(nibFile, nf, chromSize, start, blockSize);
    dna = seq->dna;
    for (i=0; i < blockSize; ++i)
	{
        ++chromPosition;
	val = ntVal[dna[i]];
	switch (val)
	    {
	    case T_BASE_VAL:
	    case C_BASE_VAL:
	    case A_BASE_VAL:
	    case G_BASE_VAL:
    		incomingVal = mask & ((incomingVal << 2) | val);
		++incomingLength;
		if ((incomingLength >= motifLen) && (incomingVal == motifVal))
		    {
			++timesFound;
		printf("%llu 1 %#llx == %#llx\n", chromPosition-motifLen+1, incomingVal&mask,motifVal);
		    if ((previousPosition + motifLen) > chromPosition)
verbose(2, "#\toverlapping at: %llu, %llu\n", previousPosition, chromPosition);
		    previousPosition = chromPosition;
		    }
		break;
	    default:
    		incomingVal = 0;
    		incomingLength = 0;
		break;
	    }
	}
    freeDnaSeq(&seq);
    ++blockCount;
    }

verbose(2, "#\tfound: %llu times, %g %% of chromosome\n", timesFound, (double)(timesFound*motifLen)/(double)chromPosition);
carefulClose(&nf);
}

static void findMotif(char *nibDir)
/* findMotif - find specified motif in nib files. */
{
char dir[256], chrom[128], ext[64];
struct fileInfo *nibList = listDirX(nibDir, "*.nib", TRUE), *nibEl;

for (nibEl = nibList; nibEl != NULL; nibEl = nibEl->next)
    {
    splitPath(nibEl->name, dir, chrom, ext);
    if (chr)
	{
	char chrNib[256];
	safef(chrNib, ArraySize(chrNib), "%s/%s.nib", nibDir, chr);
	verbose(3, "#\tchecking name: %s =? %s\n", chrNib, nibEl->name);
	if (sameString(chrNib,nibEl->name))
	    {
	    verbose(2, "#\tprocessing: %s\n", nibEl->name);
	    scanNib(nibEl->name, chrom);
	    }
	}
    else
	{
	verbose(2, "#\tprocessing: %s\n", nibEl->name);
	scanNib(nibEl->name, chrom);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
int i;
char *cp;

optionInit(&argc, argv, options);

if (argc < 2)
    usage();

dnaUtilOpen();

motif = optionVal("motif", NULL);
chr = optionVal("chr", NULL);

if (chr)
    verbose(2, "#\tprocessing chr: %s\n", chr);
if (motif)
    verbose(2, "#\tsearching for motif: %s\n", motif);
else {
    warn("ERROR: -motif string empty, please specify a motif\n");
    usage();
}
verbose(2, "#\tspecified nibDir: %s\n", argv[1]);
verbose(2, "#\tsizeof(motifVal): %d\n", sizeof(motifVal));
motifLen = strlen(motif);
/*	at two bits per character, size limit of motif is
 *	number of bits in motifVal / 2
 */
if (motifLen > (4*sizeof(motifVal))/2 )
    {
    warn("ERROR: motif string too long, limit %d\n", (4*sizeof(motifVal))/2 );
    usage();
    }
cp = motif;
motifVal = 0;
for (i = 0; i < motifLen; ++i)
    {
	switch (*cp)
	{
	case 'a':
	case 'A':
	    motifVal = (motifVal << 2) | A_BASE_VAL;
	    break;
	case 'c':
	case 'C':
	    motifVal = (motifVal << 2) | C_BASE_VAL;
	    break;
	case 'g':
	case 'G':
	    motifVal = (motifVal << 2) | G_BASE_VAL;
	    break;
	case 't':
	case 'T':
	    motifVal = (motifVal << 2) | T_BASE_VAL;
	    break;
	default:
	    warn(
		"ERROR: character in motif: '%c' is not one of ACGT\n", *cp);
	    usage();
	}
	++cp;
    }
verbose(2, "#\tmotif numerical value: %llu (%#llx)\n", motifVal, motifVal);
if (motifLen < 5)
    {
    warn("ERROR: motif string must be more than 4 characters\n");
    usage();
    }

findMotif(argv[1]);
return 0;
}
