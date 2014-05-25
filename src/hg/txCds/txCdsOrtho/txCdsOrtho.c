/* txCdsOrtho - Figure out how CDS looks in other organisms.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "sqlNum.h"
#include "maf.h"


FILE *fRa = NULL;

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
  "    out.tab is the output with the following columns\n"
  "       0 geneName\n"
  "       1 cdsStart\n"
  "       2 cdsEnd\n"
  "       3 otherSpecies\n"
  "       4 atgDots\n"
  "       5 atgDashes\n"
  "       6 kozakDots\n"
  "       7 kozakDashes\n"
  "       8 stopDots\n"
  "       9 stopDashes\n"
  "       10 atgConserved\n"
  "       11 kozakConserved\n"
  "       12 stopConserved\n"
  "       13 nativeAtg\n"
  "       14 nativeKozak\n"
  "       15 possibleSize\n"
  "       16 orfSize\n"
  "options:\n"
  "   -ra=out.ra - Put out as ra as well as tab\n"
  );
}

static struct optionSpec options[] = {
   {"ra", OPTION_STRING},
   {NULL, 0},
};

struct cdsInterval
/* Name, start, end of CDS. */
    {
    struct cdsInterval *next;
    char *name;		/* Name of gene. */
    int start;		/* Gene start. */
    int end;		/* Gene end. */
    };

struct cdsInterval *cdsIntervalLoad(char **row)
/* Create a cdsInterval from an array of strings. */
{
struct cdsInterval *cds;
AllocVar(cds);
cds->name = cloneString(row[0]);
cds->start = sqlUnsigned(row[1]);
cds->end = sqlUnsigned(row[2]);
return cds;
}

struct cdsInterval *cdsIntervalLoadAll(char *fileName)
/* Load all cdsIntervals in file, and return as list. */
{
struct cdsInterval *el, *list = NULL;
char *row[3];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, row))
    {
    el = cdsIntervalLoad(row);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

int biggestOrf(char *dna, int size, int *retStart, int *retEnd)
/* Return size of biggest ORF (region without a stop codon) in DNA 
 * Just checks frame 0. */
{
int lastCodon = size - 3;
int i;
int bestOrfSize = 0, orfSize = 0, start=0,bestStart=0,bestEnd=0;
for (i=0; i<=lastCodon; i += 3)
    {
    if (isStopCodon(dna+i))
	{
        orfSize = 0;
	start = i;
	}
    else
        {
	orfSize += 1;
	if (orfSize > bestOrfSize)
	    {
	    bestOrfSize = orfSize;
	    bestStart = start;
	    bestEnd = i+3;
	    }
	}
    }
*retStart = bestStart;
*retEnd = bestEnd;
return bestOrfSize;
}

void evaluateOneSpecies(struct mafComp *native, int cdsStart, int cdsEnd, 
	struct mafComp *xeno, int textSize, FILE *f)
/* See how well maf in xeno species matches up with maf in other. */
{
/* Figure out start and end columns, as opposed to native coordinates. */
int kozStart = cdsStart-3;
int kozCol = -1, startCol = 0, endCol = -1, i;
int nativePos = 0;
for (i=0; i < textSize; ++i)
    {
    char c = native->text[i];
    if (c != '-')
        {
	if (nativePos == kozStart)
	    kozCol = i;
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

/* Scan through one more time counting up missing sequence in CDS. */
int cdsBasesInXeno = 0;
int nativeCdsSize = 0;
for (i = startCol; i<endCol; ++i)
    {
    if (native->text[i] != '-')
        {
	char c = xeno->text[i];
	if (c != '.' && c != '-')
	    cdsBasesInXeno += 1;
	nativeCdsSize += 1;
	}
    }

/* Figure out if have atg, kozak, stop in native species. */
boolean nativeKozak = FALSE, nativeAtg = FALSE, nativeStop = FALSE;
char *nativeSeq = cloneString(native->text);
int nativeSize = strlen(nativeSeq);
stripChar(nativeSeq, '-');
tolowers(nativeSeq);
nativeAtg = startsWith("atg", nativeSeq + cdsStart);
nativeKozak = isKozak(nativeSeq, nativeSize, cdsStart);
nativeStop = isStopCodon(nativeSeq + cdsEnd - 3);
freez(&nativeSeq);

/* Figure out if atg is conserved, and also if kozak criteria is conserved. */
boolean atgConserved = FALSE, kozakConserved = FALSE;
int atgDots = 0, atgDashes = 0, kozakDots = 0, kozakDashes = 0;
if (startCol >= 0 && nativeAtg)
    {
    char codon[3];
    codon[0] = xeno->text[startCol];
    codon[1] = xeno->text[startCol+1];
    codon[2] = xeno->text[startCol+2];
    toUpperN(codon, 3);
    atgDots = countCharsN(codon, '.', 3);
    atgDashes = countCharsN(codon, '-', 3);
    atgConserved = (memcmp(codon, "ATG", 3) == 0);
    if (atgConserved && nativeKozak)
        {
	int ix = startCol+3;
	if (ix < textSize)
	   {
	   char c = xeno->text[ix];
	   if (c == 'g' || c == 'G')
	       kozakConserved = TRUE;
	   if (c == '.')
	       ++kozakDots;
	   if (c == '-')
	       ++kozakDashes;
	   }
	if (kozCol >= 0 && !kozakConserved)
	   {
	   char c = xeno->text[kozCol];
	   if (c == 'g' || c == 'G' || c == 'a' || c == 'A')
	       kozakConserved = TRUE;
	   if (c == '.')
	       ++kozakDots;
	   if (c == '-')
	       ++kozakDashes;
	   }
	}
    }

/* Figure out if stop codon is conserved */
boolean stopConserved = FALSE;
int stopDots = 0, stopDashes = 0;
if (endCol > 0 && nativeStop)
    {
    char codon[3];
    codon[0] = xeno->text[endCol-3];
    codon[1] = xeno->text[endCol-2];
    codon[2] = xeno->text[endCol-1];
    stopDots = countCharsN(codon, '.', 3);
    stopDashes = countCharsN(codon, '-', 3);
    stopConserved = isStopCodon(codon);
    }

/* Get xeno sequence in CDS region minus the dashes */
int cdsColCount = endCol - startCol;
char *xenoText = cloneStringZ(xeno->text + startCol, cdsColCount);
stripChar(xenoText, '-');
int xenoTextSize = strlen(xenoText);

/* Figure out biggest ORF in best frame and output it. */
int orfStart,orfEnd;
int orfSize = biggestOrf(xenoText + bestFrame, xenoTextSize - bestFrame, &orfStart, &orfEnd)*3;
int possibleSize = cdsEnd - cdsStart;
if (orfSize > possibleSize) orfSize = possibleSize;
fprintf(f, "%s\t%d\t%d\t%s\t", native->src, cdsStart, cdsEnd, xeno->src);
fprintf(f, "%d\t%d\t%d\t%d\t%d\t%d\t",
	atgDots, atgDashes, kozakDots, kozakDashes, stopDots, stopDashes);
fprintf(f, "%d\t%d\t%d\t%d\t",
	atgConserved, kozakConserved, stopConserved, nativeAtg);
fprintf(f, "%d\t%d\t%d\t%d\n", nativeKozak, nativeStop, possibleSize, orfSize);

/* Handle output to ra file. */
if (fRa)
    {
    double orfCov = (double)cdsBasesInXeno/nativeCdsSize;
    double orfRatio = (double)orfSize/possibleSize;
    fprintf(fRa, "%sOrfSize %d\n", xeno->src, orfSize);
    fprintf(fRa, "%sOrfRatio %f\n", xeno->src, orfRatio);
    fprintf(fRa, "%sOrfCoverage %f\n", xeno->src, orfCov);
    fprintf(fRa, "%sOrfCovRatio %f\n", xeno->src, orfCov*orfRatio);
    }

/* Clean up and go home. */
freez(&xenoText);
}

void evaluateAndWrite(char *txName, int cdsStart, int cdsEnd, struct mafAli *maf, FILE *f)
/* Given CDS in one species, evaluate it in all other species. */
{
struct mafComp *native, *xeno;
native = maf->components;
if (fRa)
    fprintf(fRa, "name %s\n", native->src);
for (xeno = native->next; xeno != NULL; xeno = xeno->next)
    evaluateOneSpecies(native, cdsStart, cdsEnd, xeno, maf->textSize, f);
if (fRa)
    fprintf(fRa, "\n");
}

struct mafComp *findCompInHash(struct mafComp *list, struct hash *hash)
/* Return first component that is also in hash. */
{
struct mafComp *comp;
for (comp = list; comp != NULL; comp = comp->next)
    if (hashLookup(hash, comp->src))
	return comp;
return NULL;
}

void txCdsOrtho(char *cdsTab, char *txMaf, char *outTab)
/* txCdsOrtho - Figure out how CDS looks in other organisms.. */
{
/* Load cds into list and hash. */
struct cdsInterval *cds, *cdsList = cdsIntervalLoadAll(cdsTab);
struct hash *cdsHash = hashNew(20);
for (cds = cdsList; cds != NULL; cds = cds->next)
    hashAdd(cdsHash, cds->name, cds);

/* Read in MAF and put it into a hash table keyed by tx. */
struct mafFile *mf = mafReadAll(txMaf);
struct mafAli *maf;
verbose(2, "Read %d multiple alignment records from %s\n", slCount(mf->alignments), txMaf);
struct hash *mafHash = hashNew(18);
for (maf = mf->alignments; maf != NULL; maf = maf->next)
    {
    /* Rearrange maf, to put component that hits hash on top. */
    struct mafComp *comp = findCompInHash(maf->components, cdsHash);
    if (comp == NULL)
        errAbort("No component from %s for a maf.", cdsTab);
    slRemoveEl(&maf->components, comp);
    slAddHead(&maf->components, comp);
    hashAdd(mafHash, comp->src, maf);
    }

FILE *f = mustOpen(outTab, "w");
for (cds = cdsList; cds != NULL; cds = cds->next)
    {
    if (cds->end > cds->start)
	{
	maf = hashFindVal(mafHash, cds->name);
	if (maf == NULL)
	    errAbort("%s is in %s but not %s", cds->name, cdsTab, txMaf);
	evaluateAndWrite(cds->name, cds->start, cds->end, maf, f);
	}
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
char *fileName = optionVal("ra", NULL);
if (fileName != NULL)
    fRa = mustOpen(fileName, "w");
txCdsOrtho(argv[1], argv[2], argv[3]);
carefulClose(&fRa);
return 0;
}
