/* simpleGap - find specified motif in nib files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "portable.h"
#include "rmskOut.h"

static char const rcsid[] = "$Id: simpleGap.c,v 1.1 2004/06/21 20:12:44 braney Exp $";

char *chr = (char *)NULL;	/*	process the one chromosome listed */
char *motif = (char *)NULL;	/*	specified motif string */
unsigned motifLen = 0;		/*	length of motif	*/
unsigned long long motifVal;	/*	motif converted to a number	*/
unsigned long long complementVal;	/*	- strand complement	*/
boolean bedOutput = TRUE;	/*	output bed file (default) */
boolean wigOutput = FALSE;	/*  output wiggle format instead of bed file */
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
  "   outGap - file to put gaps in \n"
  "   outRep - file to put repeats in \n"
  "   -motif=<acgt...> - search for this specified motif (case ignored, [acgt] only)\n"
  "   -chr=<chrN> - process only this one chrN from the nibDir\n"
  "   -strand=<+|-> - limit to only one strand.  Default is both.\n"
  "   -bedOutput - output bed format (this is the default)\n"
  "   -wigOutput - output wiggle data format instead of bed file\n"
  "   -verbose=N - set information level [1-4]\n"
  "   NOTE: motif must be longer than 4 characters, less than 17\n"
  "   -verbose=4 - will display gaps as bed file data lines to stderr"
  );
}

static struct optionSpec options[] = {
   {"chr", OPTION_STRING},
   {"strand", OPTION_STRING},
   {"motif", OPTION_STRING},
   {"bedOutput", OPTION_BOOLEAN},
   {"wigOutput", OPTION_BOOLEAN},
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
struct rmskOut rm;

memset(&rm, 0, sizeof(rm));

rm.swScore = 0;	/* Smith Waterman alignment score */
rm.milliDiv = 0;	/* Base mismatches in parts per thousand */
rm.milliDel = 0;	/* Bases deleted in parts per thousand */
rm.milliIns = 0;	/* Bases inserted in parts per thousand */
rm.genoLeft = 0;	/* Size left in genomic sequence */
rm.strand[0] = '+';	/* Relative orientation + or - */
rm.repName ="Other";	/* Name of repeat */
rm.repClass = "Other";	/* Class of repeat */
rm.repFamily = "Other";	/* Family of repeat */
rm.repStart = 0;	/* Start in repeat sequence */
rm.repEnd = 0;		/* End in repeat sequence */
rm.repLeft = 0;	/* Size left in repeat sequence */
rm.id[0] = '*';		/* '*' or ' '.  I don't know what this means */
rm.genoName = chrom;	/* Genomic sequence name */

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
		    rm.swScore = rmScore++;
		    rm.genoStart = enterRep;	/* Start in genomic sequence */
		    rm.genoEnd = chromPosition;	/* End in genomic sequence */
		    rmskOutTabOut(&rm, repIo);
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
		    fprintf(gapIo, "%s\t%llu\t%llu\t%llu\tN\t%d\tfragment\tyes\n", chrom, enterGap, chromPosition, gapCount, 1000);
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
		    fprintf(gapIo, "%s\t%llu\t%llu\t%llu\tN\t%d\tfragment\tyes\n", chrom, enterGap, chromPosition, gapCount, 1000);
		    inGap = 0;
		    }

		if (inRep)
		    {
		    rm.swScore = rmScore++;
		    rm.genoStart = enterRep;	/* Start in genomic sequence */
		    rm.genoEnd = chromPosition;	/* End in genomic sequence */
		    rmskOutTabOut(&rm, repIo);
		    inRep = 0;
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
    {
    fprintf(gapIo, "%s\t%llu\t%llu\t%llu\tN\t%d\tfragment\tyes\n", chrom, enterGap, chromPosition, gapCount, 1000);
    inGap = 0;
    }
if (inRep)
    {
    rm.genoStart = enterRep;	/* Start in genomic sequence */
    rm.genoEnd = chromPosition;	/* End in genomic sequence */
    rmskOutTabOut(&rm, repIo);
    inRep = 0;
    }

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
strand = optionVal("strand", NULL);
bedOutput = optionExists("bedOutput");
wigOutput = optionExists("wigOutput");

if (wigOutput)
    bedOutput = FALSE;
else
    bedOutput = TRUE;

if (chr)
    verbose(2, "#\tprocessing chr: %s\n", chr);
if (strand)
    verbose(2, "#\tprocessing strand: '%s'\n", strand);
verbose(2, "#\ttype output: %s\n", wigOutput ? "bed format" : "wiggle data");
verbose(2, "#\tspecified nibDir: %s\n", argv[1]);
verbose(2, "#\tsizeof(motifVal): %d\n", sizeof(motifVal));
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
simpleGap(argv[1], argv[2], argv[3]);
return 0;
}
