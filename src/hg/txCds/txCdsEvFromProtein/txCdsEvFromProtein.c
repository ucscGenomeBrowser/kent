/* txCdsEvFromProtein - Convert transcript/protein alignments and other evidence into a transcript CDS evidence (tce) file. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "psl.h"
#include "genbank.h"
#include "rangeTree.h"

/* Variables set from command line. */
char *refStatusFile = NULL;
char *uniStatusFile = NULL;
FILE *fUnmapped = NULL;
char *defaultSource = "blatUniprot";
struct hash *pepToRefHash = NULL;
int dodgeStop = 0;
double minCoverage = 0.75;


void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsEvFromProtein - Convert transcript/protein alignments and other evidence \n"
  "into a transcript CDS evidence (tce) file\n"
  "usage:\n"
  "   txCdsEvFromProtein protein.fa txProtein.psl tx.fa output.tce\n"
  "options:\n"
  "   -refStatus=refStatus.tab - include two column file with refSeq status\n"
  "         Selectively overrides source field of output\n"
  "   -uniStatus=uniStatus.tab - include two column file with uniProt status\n"
  "         Selectively overrides source field of output\n"
  "   -source=name - Name to put in tce source field. Default is \"%s\"\n"
  "   -unmapped=name - Put info about why stuff didn't map here\n"
  "   -exceptions=xxx.exceptions - Include file with info on selenocysteine\n"
  "            and other exceptions.  You get this file by running\n"
  "            files txCdsRaExceptions on ra files parsed out of genbank flat\n"
  "            files.\n"
  "   -refToPep=refToPep.tab - Put refSeq mrna to protein mapping file here\n"
  "            Usually used with exceptions flag when processing refSeq\n"
  "   -dodgeStop=N - Dodge (put gaps in place of) up to this many stop codons\n"
  "            Remark about it in unmapped file though. Also allows frame shifts\n"
  "            This *only* is applied to refPep/refSeq alignments\n"
  "   -minCoverage=0.N - minimum coverage of protein to accept, default %g\n"
  , defaultSource, minCoverage
  );
}

struct hash *selenocysteineHash = NULL;
struct hash *altStartHash = NULL;

static struct optionSpec options[] = {
   {"refStatus", OPTION_STRING},
   {"uniStatus", OPTION_STRING},
   {"source", OPTION_STRING},
   {"unmapped", OPTION_STRING},
   {"exceptions", OPTION_STRING},
   {"refToPep", OPTION_STRING},
   {"dodgeStop", OPTION_INT},
   {"minCoverage", OPTION_DOUBLE},
   {NULL, 0},
};

void unmappedPrint(char *format, ...)
/* Print out info to unmapped file if it exists. */
{
if (fUnmapped != NULL)
    {
    va_list args;
    va_start(args, format);
    vfprintf(fUnmapped, format, args);
    va_end(args);
    }
}

struct hash *hashTwoColumnFileReverse(char *fileName)
/* Return hash of strings with keys from second column
 * in file, values from first column. */
{
struct hash *hash = hashNew(19);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
while (lineFileRow(lf, row))
    hashAdd(hash, row[1], cloneString(row[0]));
lineFileClose(&lf);
return hash;
}

struct hash *getSourceHash()
/* Return hash of sources to override default source. */
{
struct hash *sourceHash = hashNew(18);
struct hash *typeHash = hashNew(0);
if (refStatusFile != NULL)
    {
    struct lineFile *lf = lineFileOpen(refStatusFile, TRUE);
    char *row[2];
    while (lineFileRow(lf, row))
        {
	char typeBuf[128];
	safef(typeBuf, sizeof(typeBuf), "RefPep%s", row[1]);
	char *type = hashStoreName(typeHash, typeBuf);
	hashAdd(sourceHash, row[0], type);
	}
    lineFileClose(&lf);
    }
if (uniStatusFile != NULL)
    {
    struct lineFile *lf = lineFileOpen(uniStatusFile, TRUE);
    char *row[2];
    while (lineFileRow(lf, row))
        {
	char *source;
	if (row[1][0] == '1')
	    source = "swissProt";
	else
	    source = "trembl";
	hashAdd(sourceHash, row[0], source);
	}
    lineFileClose(&lf);
    }
verbose(2, "%d sources from %d elements\n", typeHash->elCount, sourceHash->elCount);
return sourceHash;
}

void removeNegativeGaps(struct psl *psl)
/* It can happen that there will be negative sized gaps in a 
 * translated psl.  This gets rid of these.  It's easier here
 * because we assume the strand is ++. */
{
int i, lastBlock = psl->blockCount-1;
for (i=0; i<lastBlock; ++i)
    {
    int qBlockSize = psl->blockSizes[i];
    int tBlockSize = qBlockSize*3;
    int iqStart = psl->qStarts[i] + qBlockSize;
    int itStart = psl->tStarts[i] + tBlockSize;
    int iqEnd = psl->qStarts[i+1];
    int itEnd = psl->tStarts[i+1];
    int qGap = iqEnd - iqStart;
    int tGap = itEnd - itStart;
    if (qGap < 0)
	{
	psl->blockSizes[i] += qGap;
	verbose(3, "%s - correcting %d size qGap.\n", psl->qName, qGap);
	}
    else if (tGap < 0)
        {
	int gapSize = (-tGap + 2)/3;
	psl->blockSizes[i] -= gapSize;
	verbose(3, "%s - correcting %d size tGap.\n", psl->qName, tGap);
	}
    }
}

int aliCountStopCodons(struct psl *psl, struct dnaSeq *seq,
	boolean selenocysteine)
/* Return count of stop codons in alignment. */
{
int blockIx;
int stopCount = 0;
for (blockIx=0; blockIx<psl->blockCount; ++blockIx)
    {
    int qBlockSize = psl->blockSizes[blockIx];
    int tStart = psl->tStarts[blockIx];
    DNA *dna = seq->dna + tStart;
    int i;
    for (i=0; i<qBlockSize; ++i)
        {
	if (isReallyStopCodon(dna, selenocysteine))
	    {
	    ++stopCount;
	    }
	dna += 3;
	}
    }
return stopCount;
}


int totalAminoAcids(struct psl *psl)
/* Count total number of amino acids in psl. */
{
int i = 0, blockCount = psl->blockCount;
int total = 0;
for (i=0; i<blockCount; ++i)
    total += psl->blockSizes[i];
return total;
}

int countUntilNextStop(char *dna, int maxCount, boolean selenocysteine)
/* Count up number of codons until next stop */
{
int count;
for (count=0; count<maxCount; ++count)
    {
    if (isReallyStopCodon(dna, selenocysteine))
        break;
    dna += 3;
    }
return count;
}


void outputDodgedBlocks(struct psl *psl, aaSeq *protSeq, struct dnaSeq *txSeq, 
	boolean selenocysteine, FILE *f)
/* Write out block count and the actual blocks to file.  What makes this
 * fun is that we are inserting gaps to avoid stop codone. */
{
/* Build up list of ranges corresponding to parts of blocks 
 * broken up by stops. */
struct range *rangeList = NULL, *range;
int blockIx;
for (blockIx = 0; blockIx < psl->blockCount; ++blockIx)
    {
    int tStart = psl->tStarts[blockIx];
    int size = psl->blockSizes[blockIx]*3;
    char *dna = txSeq->dna + tStart;
    int pos, brokenSize;
    for (pos = 0; pos<size; pos += brokenSize+3)
        {
	brokenSize = 3*countUntilNextStop(dna+pos, (size-pos)/3, selenocysteine);
	verbose(3, "blockIx %d, size %d, pos %d, size-pos %d, brokenSize %d\n", 
		blockIx, pos, size, size-pos, brokenSize);
	if (brokenSize > 0)
	    {
	    AllocVar(range);
	    range->start = tStart+pos;
	    range->end = range->start + brokenSize;
	    slAddHead(&rangeList, range);
	    }
	}
    }
slReverse(&rangeList);

/* Output to file */
fprintf(f, "%d\t", slCount(rangeList));	
for (range = rangeList; range != NULL; range = range->next)
    fprintf(f, "%d,", range->start);
fprintf(f, "\t");
for (range = rangeList; range != NULL; range = range->next)
    fprintf(f, "%d,", range->end - range->start);
fprintf(f, "\n");

/* Clean up and go home. */
slFreeList(&rangeList);
}

void mapAndOutput(char *source, aaSeq *protSeq, struct psl *psl, 
	struct dnaSeq *txSeq, FILE *f)
/* Attempt to define CDS in txSeq by looking at alignment between
 * it and a protein. */
{
boolean selenocysteine = FALSE;
boolean doDodge = FALSE;
int lDodgeStop = 0;
if (hashLookup(selenocysteineHash, psl->qName))
    selenocysteine = TRUE;
if (pepToRefHash != NULL)
    {
    char *refAcc = hashFindVal(pepToRefHash, psl->qName);
    if (refAcc != NULL)
	{
        if (hashLookup(selenocysteineHash, refAcc))
	    selenocysteine = TRUE;
	if (endsWith(psl->tName, refAcc))
	    lDodgeStop = dodgeStop;
	}
    }
	    
if (!sameString(psl->strand, "++"))
    {
    unmappedPrint("%s alignment with %s funny strand %s\n", psl->qName, psl->tName,
    	psl->strand);
    return;
    }
removeNegativeGaps(psl);
int stopCount = aliCountStopCodons(psl, txSeq, selenocysteine);
if (stopCount > 0)
    {
    if (lDodgeStop)
	{
	if (stopCount > lDodgeStop)
	    {
	    unmappedPrint("%s alignment with %s has too many (%d) stop codons\n", 
	    	psl->qName, psl->tName, stopCount);
	    return;
	    }
	else
	    {
	    unmappedPrint("%s alignment with %s dodging %d stop codons\n", 
	    	psl->qName, psl->tName, stopCount);
	    doDodge = TRUE;
	    }
	}
    else
	{
	unmappedPrint("%s alignment with %s has stop codons\n", psl->qName, psl->tName);
	return;
	}
    }
if (psl->blockCount > 1)
    {
    /* Make sure that all blocks are in same frame. */
    int frame = psl->tStarts[0]%3;
    int i;
    for (i=1; i<psl->blockCount; ++i)
        if (frame != psl->tStarts[i]%3)
	    {
	    if (lDodgeStop > 0)
		{
		unmappedPrint("%s alignment with %s allowing frame shift\n", 
		    psl->qName, psl->tName, stopCount);
		}
	    else
		{
		unmappedPrint("%s alignment with %s in multiple different frames\n", 
		    psl->qName, psl->tName);
		return;
		}
	    }

    }
int mappedStart = psl->tStart;
int mappedEnd = psl->tEnd;
char *s = txSeq->dna + mappedStart;
char *e = txSeq->dna + mappedEnd;

/* If next base is a stop codon, add it to alignment. */
if (mappedEnd + 3 <= psl->tSize && isStopCodon(e))
    {
    mappedEnd += 3;
    psl->blockSizes[psl->blockCount-1] += 1;
    psl->tEnd += 3;
    psl->qEnd += 1;
    }

verbose(3, "%s s %c%c%c e %c%c%c %d %d \n", psl->qName, s[0], s[1], s[2], e[0], e[1], e[2], mappedStart, mappedEnd);
int milliBad = pslCalcMilliBad(psl, FALSE);
int score = 1000 - 10*milliBad;
if (score <= 0)
    {
    unmappedPrint("%s too divergent compared to %s (%d score (of 1000), %d milliBad)\n", 
    	psl->qName, psl->tName, score, milliBad);
    return;
    }
int totalCovAa = totalAminoAcids(psl);
double protCoverage = (double)totalCovAa/psl->qSize;
if (protCoverage < minCoverage)
    {
    unmappedPrint("%s covers too little of %s (%d of %d amino acids)\n",
    	psl->qName, psl->tName, totalCovAa, psl->qSize);
    return;
    }
score *= protCoverage;
double mrnaInternalCoverage = 3.0*totalCovAa/(psl->tEnd - psl->tStart);
score *= mrnaInternalCoverage*mrnaInternalCoverage;
fprintf(f, "%s\t", psl->tName);
fprintf(f, "%d\t", mappedStart);
fprintf(f, "%d\t", mappedEnd);
fprintf(f, "%s\t", source);
fprintf(f, "%s\t", psl->qName);
fprintf(f, "%d\t", score);
fprintf(f, "%d\t", startsWith("atg", s));
fprintf(f, "%d\t", isStopCodon(e));
if (doDodge)
    {
    outputDodgedBlocks(psl, protSeq, txSeq, selenocysteine, f);
    }
else
    {
    fprintf(f, "%d\t", psl->blockCount);	
    int i;
    for (i=0; i < psl->blockCount; ++i)
	fprintf(f, "%d,", psl->tStarts[i]);
    fprintf(f, "\t");
    for (i=0; i < psl->blockCount; ++i)
	fprintf(f, "%d,", psl->blockSizes[i]*3);
    fprintf(f, "\n");
    }
}

void makeExceptionHashes()
/* Create hash that has accessions using selanocysteine in it
 * if using the exceptions option.  Otherwise the hash will be
 * empty. */
{
char *fileName = optionVal("exceptions", NULL);
if (fileName != NULL)
    genbankExceptionsHash(fileName, &selenocysteineHash, &altStartHash);
else
    selenocysteineHash = altStartHash = hashNew(4);
}

void txCdsEvFromProtein(char *protFa, char *txProtPsl, char *txFa, char *output)
/* txCdsEvFromProtein - Convert transcript/protein alignments and other evidence 
 * into a transcript CDS evidence (tce) file. */
{
struct hash *protHash = faReadAllIntoHash(protFa, dnaUpper);
struct hash *txHash = faReadAllIntoHash(txFa, dnaLower);
struct lineFile *lf = pslFileOpen(txProtPsl);
struct hash *sourceHash = getSourceHash();
FILE *f = mustOpen(output, "w");
struct psl *psl;

while ((psl = pslNext(lf)) != NULL)
    {
    struct dnaSeq *txSeq = hashFindVal(txHash, psl->tName);
    if (txSeq == NULL)
        errAbort("%s in %s but not %s\n", psl->tName, txProtPsl, txFa);
    aaSeq *protSeq = hashFindVal(protHash, psl->qName);
    if (protSeq == NULL)
        errAbort("%s in %s but not %s\n", psl->qName, txProtPsl, protFa);
    char *source = hashFindVal(sourceHash, psl->qName);
    if (source == NULL)
	source = defaultSource;
    mapAndOutput(source, protSeq, psl, txSeq, f);
    }

carefulClose(&f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
refStatusFile = optionVal("refStatus", refStatusFile);
uniStatusFile = optionVal("uniStatus", uniStatusFile);
defaultSource = optionVal("source", defaultSource);
if (optionExists("unmapped"))
    fUnmapped = mustOpen(optionVal("unmapped", NULL), "w");
if (optionExists("refToPep"))
    pepToRefHash = hashTwoColumnFileReverse(optionVal("refToPep", NULL));
dodgeStop = optionInt("dodgeStop", dodgeStop);
minCoverage = optionDouble("minCoverage", minCoverage);
makeExceptionHashes();
txCdsEvFromProtein(argv[1], argv[2], argv[3], argv[4]);
carefulClose(&fUnmapped);
return 0;
}
