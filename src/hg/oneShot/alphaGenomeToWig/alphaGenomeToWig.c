/* alphaGenomeToWig - convert AlphaGenome variant impact scores to fixedStep wig,
 * one file per alternate allele. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

/* The input has no row where ref == alt, so each per-allele stream is missing
 * roughly every fourth position.  Those one-base holes are filled with zero so
 * that a fixedStep section can span a whole contiguous run; a hole longer than
 * maxGap is treated as a genuinely unscored region and starts a new section. */
static int maxGap = 10;
static char *outDir = ".";

static char *nuclNames = "ACGT";

struct alleleOut
/* One output wig file, plus the state of the fixedStep section being written. */
    {
    FILE *f;			/* output file */
    int lastPos;		/* last position written, -1 if no section open */
    long long valCount;		/* values written, including zero fills */
    long long zeroFills;	/* values written only to bridge a hole */
    long long sections;		/* fixedStep headers written */
    };

static struct alleleOut alleleOuts[4];
static char curChrom[256];	/* chrom of the sections currently open */

static long long lineCount = 0;
static long long variantCount = 0;
static long long skipFieldCount = 0;
static long long skipMultiBase = 0;
static long long skipBadNucl = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "alphaGenomeToWig - convert AlphaGenome variant impact scores to fixedStep wig\n"
  "usage:\n"
  "   alphaGenomeToWig in.tsv.gz\n"
  "Writes alphaGenomeA.wig, alphaGenomeC.wig, alphaGenomeG.wig and\n"
  "alphaGenomeT.wig, one per alternate allele.  The input must be a tab-separated\n"
  "file of chrom, pos, ref, alt, rawScore, phred sorted by chrom then pos, and\n"
  "may be gzipped.  The phred column is copied through as text, so no precision\n"
  "is lost.\n"
  "options:\n"
  "   -maxGap=N   fill an unscored run of up to N bases with zeroes rather than\n"
  "               starting a new fixedStep section (default %d)\n"
  "   -outDir=dir write the wig files to dir (default the current directory)\n"
  , maxGap);
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"maxGap", OPTION_INT},
   {"outDir", OPTION_STRING},
   {NULL, 0},
};

void alleleOutsOpen()
/* Open the four output files and give each a generous stdio buffer. */
{
int i;
for (i = 0; i < 4; ++i)
    {
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s/alphaGenome%c.wig", outDir, nuclNames[i]);
    struct alleleOut *out = &alleleOuts[i];
    out->f = mustOpen(path, "w");
    setvbuf(out->f, needMem(1024*1024), _IOFBF, 1024*1024);
    out->lastPos = -1;
    }
}

void alleleOutWrite(int nucIdx, int pos, char *scoreText)
/* Append one score, opening a new fixedStep section or bridging a short hole
 * with zeroes as needed. */
{
struct alleleOut *out = &alleleOuts[nucIdx];
int gap = pos - out->lastPos;

if (out->lastPos < 0 || gap > maxGap + 1)
    {
    fprintf(out->f, "fixedStep chrom=%s span=1 step=1 start=%d\n", curChrom, pos);
    out->sections += 1;
    }
else if (gap <= 0)
    errAbort("%s is not sorted: position %d follows %d on %s (allele %c)",
	     "input", pos, out->lastPos, curChrom, nuclNames[nucIdx]);
else
    {
    int i;
    for (i = 1; i < gap; ++i)
	{
	fputs("0\n", out->f);
	out->valCount += 1;
	out->zeroFills += 1;
	}
    }

fputs(scoreText, out->f);
fputc('\n', out->f);
out->valCount += 1;
out->lastPos = pos;
}

void alleleOutsClose()
/* Flush and close the output files. */
{
int i;
for (i = 0; i < 4; ++i)
    carefulClose(&alleleOuts[i].f);
}

void reportCounts()
/* Print an accounting of what went in and what came out. */
{
fprintf(stderr, "read %lld data lines, used %lld\n", lineCount, variantCount);
if (skipFieldCount)
    fprintf(stderr, "skipped %lld lines with fewer than 6 fields\n", skipFieldCount);
if (skipMultiBase)
    fprintf(stderr, "skipped %lld lines whose alt allele was not a single base\n",
	    skipMultiBase);
if (skipBadNucl)
    fprintf(stderr, "skipped %lld lines whose alt allele was not A, C, G or T\n",
	    skipBadNucl);

long long total = 0, fills = 0, sections = 0;
int i;
for (i = 0; i < 4; ++i)
    {
    struct alleleOut *out = &alleleOuts[i];
    fprintf(stderr, "  %c: %lld values in %lld sections (%lld zero fills)\n",
	    nuclNames[i], out->valCount, out->sections, out->zeroFills);
    total += out->valCount;
    fills += out->zeroFills;
    sections += out->sections;
    }
fprintf(stderr, "wrote %lld values in %lld sections, %lld of them zero fills\n",
	total, sections, fills);
if (variantCount + fills != total)
    errAbort("value accounting is off: %lld scores + %lld fills != %lld written",
	     variantCount, fills, total);
}

void alphaGenomeToWig(char *inFile)
/* alphaGenomeToWig - convert AlphaGenome variant impact scores to fixedStep wig. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
struct hash *chromsSeen = hashNew(0);
char *line;

alleleOutsOpen();
curChrom[0] = 0;

while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '#')
	continue;
    lineCount += 1;

    char *row[8];
    int fieldCount = chopTabs(line, row);
    if (fieldCount < 6)
	{
	skipFieldCount += 1;
	continue;
	}

    char *chrom = row[0];
    int pos = lineFileNeedNum(lf, row, 1);
    char *alt = row[3];
    char *scoreText = row[5];

    if (!sameString(chrom, curChrom))
	{
	/* wigToBigWig needs each chrom's data in one run, so a chrom coming
	 * back after we left it is a fatal sort problem, not a warning. */
	if (hashLookup(chromsSeen, chrom))
	    errAbort("%s: %s reappears after other chroms, input is not sorted",
		     lf->fileName, chrom);
	hashAdd(chromsSeen, chrom, NULL);
	safecpy(curChrom, sizeof(curChrom), chrom);

	int i;
	for (i = 0; i < 4; ++i)
	    alleleOuts[i].lastPos = -1;
	}

    if (alt[0] == 0 || alt[1] != 0)
	{
	skipMultiBase += 1;
	continue;
	}
    char *nuclPos = strchr(nuclNames, alt[0]);
    if (nuclPos == NULL)
	{
	skipBadNucl += 1;
	continue;
	}

    alleleOutWrite(nuclPos - nuclNames, pos, scoreText);
    variantCount += 1;

    if (variantCount % 100000000 == 0)
	fprintf(stderr, "%lld variants, at %s:%d\n", variantCount, curChrom, pos);
    }

alleleOutsClose();
lineFileClose(&lf);
reportCounts();
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
maxGap = optionInt("maxGap", maxGap);
outDir = optionVal("outDir", outDir);
if (argc != 2)
    usage();
alphaGenomeToWig(argv[1]);
return 0;
}
