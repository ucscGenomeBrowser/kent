/* simpleGap - build gap and repeats bed files */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "portable.h"


char *chr = (char *)NULL;	/*	process the one chromosome listed */
char *strand = (char *)NULL;
boolean doPlusStrand = TRUE;	/*	output bed file instead of wiggle */
boolean doMinusStrand = TRUE;	/*	output bed file instead of wiggle */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "simpleGap - find gaps and lower case letters (repeats)\n"
  "usage:\n"
  "   simpleGap [options] nibDir outGap outRep\n"
  "options:\n"
  "   nibDir - path to nib directory, relative or absolute path OK\n"
  "   outGap - BED file to put gaps in \n"
  "   outRep - BED file to put repeats in \n"
  "   -chr=<chrN> - process only this one chrN from the nibDir\n"
  "   -verbose=N - set information level [1-4]\n"
  );
}

static struct optionSpec options[] = {
   {"chr", OPTION_STRING},
   {NULL, 0},
};

unsigned long long gapCount = 0;
unsigned long long repCount = 0;

static void scanNib(char *nibFile, char *chrom, FILE *gapIo, FILE *repIo)
{
static int rmScore = 0;
int chromSize, start, end, blockSize;
int i, val;
int winSize=131072;
FILE *nf = NULL;
struct dnaSeq *seq = NULL;
DNA *dna;
int blockCount = 0;
unsigned long long mask;
unsigned long long chromPosition = 0;
register unsigned long long incomingVal = 0;
unsigned long long incomingLength = 0;
unsigned long long posFound = 0;
unsigned long long negFound = 0;
unsigned long long posPreviousPosition = 0;
unsigned long long negPreviousPosition = 0;
unsigned long long enterGap = 1;
unsigned long long enterRep = 1;
int inGap = 0, inRep=0;

nibOpenVerify(nibFile, &nf, &chromSize);
for (start=0; start<chromSize; start = end)
    {
    end = start + winSize;
    if (end > chromSize)
	end = chromSize;
    blockSize = end - start;
    seq = nibLdPartMasked(NIB_MASK_MIXED, nibFile, nf, chromSize, start, blockSize);
    dna = seq->dna;
    for (i=0; i < blockSize; ++i)
	{
	val = dna[i];
	switch (val)
	    {
	    case 'N':
	    case 'n':
		if (inRep)
		    {
		    fprintf(repIo, "%s\t%llu\t%llu\n", chrom, enterRep, chromPosition);
		    inRep = 0;
		    }
		if (!inGap)
		    {
		    ++gapCount;
		    inGap = 1;
		    enterGap = chromPosition;
		    }
		    break;
	    case 't':
	    case 'c':
	    case 'a':
	    case 'g':
		if (inGap)
		    {
		    fprintf(gapIo, "%s\t%llu\t%llu\t%llu\tN\t%lld\tfragment\tyes\n", chrom, enterGap, chromPosition, gapCount, chromPosition - enterGap);
		    inGap = 0;
		    }
		if (!inRep)
		    {
		    inRep = 1; 
		    enterRep = chromPosition;
		    repCount++;
		    }
		break;
	    case 'T':
	    case 'C':
	    case 'A':
	    case 'G':
		if (inGap)
		    {
		    fprintf(gapIo, "%s\t%llu\t%llu\t%llu\tN\t%lld\tfragment\tyes\n", chrom, enterGap, chromPosition, gapCount, chromPosition - enterGap);
		    inGap = 0;
		    }

		if (inRep)
		    {
		    inRep = 0;
		    fprintf(repIo, "%s\t%llu\t%llu\n", chrom, enterRep, chromPosition);
		    }
		break;
	    default:
		errAbort("unknown char");
		break;
	    }
        ++chromPosition;
	}

    freeDnaSeq(&seq);
    ++blockCount;
    }

if (inGap)
    fprintf(gapIo, "%s\t%llu\t%llu\t%llu\tN\t%lld\tfragment\tyes\n", chrom, enterGap, chromPosition, gapCount, chromPosition - enterGap);

if (inRep)
    fprintf(repIo, "%s\t%llu\t%llu\n", chrom, enterRep, chromPosition);

carefulClose(&nf);
}

static void simpleGap(char *nibDir, char *gapFile, char *repFile)
/* simpleGap - find specified motif in nib files. */
{
char dir[256], chrom[128], ext[64];
struct fileInfo *nibList = listDirX(nibDir, "*.nib", TRUE), *nibEl;
FILE *gapIo = mustOpen(gapFile, "w");
FILE *repIo = mustOpen(repFile, "w");

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
	    scanNib(nibEl->name, chrom, gapIo, repIo);
	    }
	}
    else
	{
	verbose(2, "#\tprocessing: %s\n", nibEl->name);
	scanNib(nibEl->name, chrom, gapIo, repIo);
	}
    }
carefulClose(&gapIo);
carefulClose(&repIo);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int i;
char *cp;
unsigned long long reversed;

optionInit(&argc, argv, options);

if (argc < 4)
    usage();

dnaUtilOpen();

chr = optionVal("chr", NULL);

if (chr)
    verbose(2, "#\tprocessing chr: %s\n", chr);
verbose(2, "#\tspecified nibDir: %s\n", argv[1]);
simpleGap(argv[1], argv[2], argv[3]);
return 0;
}
