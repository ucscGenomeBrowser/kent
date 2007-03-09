/* txCdsOrtho - Figure out how CDS looks in other organisms.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "maf.h"

static char const rcsid[] = "$Id: txCdsOrtho.c,v 1.2 2007/03/09 07:29:00 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsOrtho - Figure out how CDS looks in other organisms.\n"
  "usage:\n"
  "   txCdsOrtho cdsIntervals.tab tx.maf out.tab\n"
  "Where:\n"
  "    cdsIntervals.tab is a three column file containing tx, cdsStart, cdsEnd\n"
  "    tx.maf is a maf file containing alignments against all the transcripts\n"
  "           The first 'a' line in each maf record should have the transcript\n"
  "           name and the native sequence, the other 'a' lines should have\n"
  "           the database name and sequence for the other organism\n"
  "    out.tab is the output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int biggestOrf(char *dna, int size)
/* Return size of biggest ORF (region without a stop codon) in DNA 
 * Just checks frame 0. */
{
int lastCodon = size - 3;
int i;
int bestOrfSize = 0, orfSize = 0;
for (i=0; i<=lastCodon; i += 3)
    {
    if (isStopCodon(dna+i))
        orfSize = 0;
    else
        {
	orfSize += 1;
	if (orfSize > bestOrfSize)
	    bestOrfSize = orfSize;
	}
    }
return bestOrfSize;
}

void evaluateOneSpecies(struct mafComp *native, int cdsStart, int cdsEnd, 
	struct mafComp *xeno, int textSize, FILE *f)
/* See how well maf in xeno species matches up with maf in other. */
{
/* Figure out start and end columns, as opposed to native coordinates. */
int startCol = 0, endCol = 0, i;
int nativePos = 0;
for (i=0; i < textSize; ++i)
    {
    char c = native->text[i];
    if (c != '-')
        {
	if (nativePos == cdsStart)
	    startCol = i;
	++nativePos;
	if (nativePos == cdsEnd)
	    {
	    endCol = i+1;
	    break;
	    }
	}
    }
verbose(3, "%s:%d-%d startCol %d, endCol %d\n", native->src, cdsStart, cdsEnd, startCol, endCol);

/* Scan through counting indels, figuring out what relative frame
 * covers the most of the CDS. */
int relFrameCount[3];
relFrameCount[0] = relFrameCount[1] = relFrameCount[2] = 0;
int relFrame = 0;
for (i = startCol; i<endCol; ++i)
    {
    char nc = native->text[i];
    if (nc == '-')
        {
	relFrame += 1;
	if (relFrame > 2) relFrame = 0;
	}
    char xc = xeno->text[i];
    if (xc == '-')
        {
	relFrame -= 1;
	if (relFrame < 0) relFrame = 2;
	}
    if (nc != '-' && xc != '.' && xc != '-')
        relFrameCount[relFrame] += 1;
    }
/* Relative frame - xeno frame - native frame for column.
 *   NATIVE  ATG-GATAAATCAAATTA
 *           012 01201201201201
 *           0120120120120 1201
 *     XENO  ATGAGATAAATCA-ATTA
 *           000.111111111.0000 */

/* Pick best relative frame. */
int bestFrame = -1, bestCount = -1, totalCount = 0;
for (relFrame = 0; relFrame < 3; ++relFrame)
    {
    int count = relFrameCount[relFrame];
    totalCount += count;
    if (count > bestCount)
        {
	bestCount = count;
	bestFrame = relFrame;
	}
    }
verbose(3, "Best frame for %s:%d-%d vs %s is %d with count of %d of %d\n", 
	native->src, cdsStart, cdsEnd, xeno->src, bestFrame, bestCount, totalCount);

/* Get xeno sequence in CDS region minus the dashes */
int cdsColCount = endCol - startCol;
char *xenoText = cloneStringZ(xeno->text + startCol, cdsColCount);
stripChar(xenoText, '-');
int xenoTextSize = strlen(xenoText);

/* Figure out biggest ORF in best frame and output it. */
int orfSize = biggestOrf(xenoText + bestFrame, xenoTextSize - bestFrame)*3;
int possibleSize = cdsEnd - cdsStart;
if (orfSize > possibleSize) orfSize = possibleSize;
fprintf(f, "%s\t%d\t%d\t%s\t%d\t%d\t%f\n", native->src, cdsStart, cdsEnd, 
	xeno->src, orfSize, possibleSize,
	(double)orfSize/(possibleSize));

/* Clean up and go home. */
freez(&xenoText);
}

void evaluateAndWrite(char *txName, int cdsStart, int cdsEnd, struct mafAli *maf, FILE *f)
/* Given CDS in one species, evaluate it in all other species. */
{
struct mafComp *native, *xeno;
native = maf->components;
for (xeno = native->next; xeno != NULL; xeno = xeno->next)
    evaluateOneSpecies(native, cdsStart, cdsEnd, xeno, maf->textSize, f);
}

void txCdsOrtho(char *cdsTab, char *txMaf, char *outTab)
/* txCdsOrtho - Figure out how CDS looks in other organisms.. */
{
/* Open tab separated input here (so will fail quickly without reading
 * 100 meg of MAF data if there's a typo in file name */
struct lineFile *lf = lineFileOpen(cdsTab, TRUE);

/* Read in MAF and put it into a hash table keyed by tx. */
struct mafFile *mf = mafReadAll(txMaf);
struct mafAli *maf;
verbose(2, "Read %d multiple alignment records from %s\n", slCount(mf->alignments), txMaf);
struct hash *mafHash = hashNew(18);
for (maf = mf->alignments; maf != NULL; maf = maf->next)
    {
    struct mafComp *comp = maf->components;
    if (comp == NULL)
        errAbort("Empty maf in %s", txMaf);
    hashAdd(mafHash, comp->src, maf);
    }
char *row[3];
FILE *f = mustOpen(outTab, "w");
while (lineFileRow(lf, row))
    {
    char *txName = row[0];
    int cdsStart = lineFileNeedNum(lf, row, 1);
    int cdsEnd = lineFileNeedNum(lf, row, 2);
    maf = hashFindVal(mafHash, txName);
    if (maf == NULL)
        errAbort("%s is in %s but not %s", txName, cdsTab, txMaf);
    evaluateAndWrite(txName, cdsStart, cdsEnd, maf, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
dnaUtilOpen();
txCdsOrtho(argv[1], argv[2], argv[3]);
return 0;
}
