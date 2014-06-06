/* txCdsEvFromRna - Convert transcript/rna alignments, genbank CDS file, 
 * and other info to transcript CDS evidence (tce) file.. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "psl.h"
#include "genbank.h"
#include "fa.h"
#include "verbose.h"

/* Variables set from command line. */
char *refStatusFile = NULL;
char *mgcStatusFile = NULL;
FILE *fUnmapped = NULL;
char *defaultSource = "genbankCds";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsEvFromRna - Convert transcript/rna alignments, genbank CDS file, and \n"
  "other info to transcript CDS evidence (tce) file.\n"
  "usage:\n"
  "   txCdsEvFromRna rna.fa rna.cds txRna.psl tx.fa output.tce\n"
  "options:\n"
  "   -refStatus=refStatus.tab - include two column file with refSeq status\n"
  "         Selectively overrides source field of output\n"
  "   -mgcStatus=mgcStatus.tab - include two column file with MGC status\n"
  "         Selectively overrides source field of output\n"
  "   -source=name - Name to put in tce source field. Default is \"%s\"\n"
  "   -unmapped=name - Put info about why stuff didn't map here\n"
  "   -exceptions=xxx.exceptions - Include file with info on selenocysteine\n"
  "            and other exceptions.  You get this file by running\n"
  "            files txCdsRaExceptions on ra files parsed out of genbank flat\n"
  "            files.\n"
  , defaultSource
  );
}

struct hash *selenocysteineHash = NULL;
struct hash *altStartHash = NULL;

static struct optionSpec options[] = {
   {"refStatus", OPTION_STRING},
   {"mgcStatus", OPTION_STRING},
   {"source", OPTION_STRING},
   {"unmapped", OPTION_STRING},
   {"exceptions", OPTION_STRING},
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


struct hash *cdsReadAllIntoHash(char *fileName)
/* Return hash full of genbankCds records. */
{
char *row[2];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = hashNew(18);
while (lineFileRowTab(lf, row))
    {
    struct genbankCds *cds;
    char *cdsString = row[1];
    if (!startsWith("join", cdsString) && !startsWith("complement", cdsString))
	{
	lmAllocVar(hash->lm, cds);
	if (!genbankCdsParse(cdsString, cds))
	    errAbort("Bad CDS %s line %d of %s", cdsString, lf->lineIx, lf->fileName);
	hashAdd(hash, row[0], cds);
	}
    }
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
	safef(typeBuf, sizeof(typeBuf), "RefSeq%s", row[1]);
	char *type = hashStoreName(typeHash, typeBuf);
	hashAdd(sourceHash, row[0], type);
	}
    lineFileClose(&lf);
    }
if (mgcStatusFile != NULL)
    {
    struct lineFile *lf = lineFileOpen(mgcStatusFile, TRUE);
    char *row[2];
    while (lineFileRow(lf, row))
        {
	char typeBuf[128];
	safef(typeBuf, sizeof(typeBuf), "MGC%s", row[1]);
	char *type = hashStoreName(typeHash, typeBuf);
	hashAdd(sourceHash, row[0], type);
	}
    lineFileClose(&lf);
    }
verbose(2, "%d sources from %d elements\n", typeHash->elCount, sourceHash->elCount);
return sourceHash;
}

boolean hasStopCodons(char *dna, int codons, boolean selenocysteine)
/* Return TRUE if there are stop codons in target coding region */
{
int i;
for (i=0; i<codons; ++i)
    {
    if (selenocysteine)
        {
	if (startsWith("taa", dna) || startsWith("tag", dna))
	    return TRUE;
	}
    else
	{
	if (isStopCodon(dna))
	    return TRUE;
	}
    dna += 3;
    }
return FALSE;
}

boolean checkCds(struct genbankCds *cds, struct dnaSeq *seq, boolean selenocysteine)
/* Make sure no stop codons, and if marked complete that it does
 * really start with ATG and end with TAA/TAG/TGA */
{
int size = cds->end - cds->start;
if (size%3 != 0)
    {
    unmappedPrint("%s input CDS size %d not a multiple of 3\n", seq->name, size);
    return FALSE;
    }
size /= 3;
char *dna = seq->dna + cds->start;
if (cds->startComplete)
    {
    if (!startsWith("atg", dna))
	{
	if (!hashLookup(altStartHash, seq->name))
	    {
	    unmappedPrint("%s input CDS \"start complete\" but begins with %c%c%c\n", 
		    seq->name, dna[0], dna[1], dna[2]);
	    return FALSE;
	    }
	}
    }
if (hasStopCodons(dna, size-1, selenocysteine))
    {
    unmappedPrint("%s input CDS has internal stop codon\n", seq->name);
    return FALSE;
    }
if (cds->endComplete)
    {
    dna = seq->dna + cds->end - 3;
    if (!isStopCodon(dna))
	{
        unmappedPrint("%s input CDS  \"end complete\" but ends with %c%c%c\n", 
		seq->name, dna[0], dna[1], dna[2]);
	return FALSE;
	}
    }
if (verboseLevel() >= 3)
    {
    char *s = seq->dna + cds->start;
    char *e = seq->dna + cds->end-3;
    verbose(3, "%s\t%d\t%d\t%c%c%c\t%c%c%c\n", seq->name, cds->startComplete, 
    	cds->endComplete, s[0], s[1], s[2], e[0], e[1], e[2]);
    }
return TRUE;
}

boolean hasFrameShiftInCds(struct psl *psl, int cdsStart, int cdsEnd)
/* Return TRUE if has frame shift (gaps of size not multiples of 3)
 * in CDS. */
{
int i, lastBlock = psl->blockCount-1;
for (i=0; i<lastBlock; ++i)
    {
    int blockSize = psl->blockSizes[i];
    int itStart = psl->tStarts[i] + blockSize;
    int itEnd = psl->tStarts[i+1];
    if (rangeIntersection(itStart, itEnd, cdsStart, cdsEnd) > 0)
	{
	int iqStart = psl->qStarts[i] + blockSize;
	int iqEnd = psl->qStarts[i+1];
	int tGap = itEnd - itStart;
	int qGap = iqEnd - iqStart;
	int gapDifference = intAbs(tGap-qGap);
	if (gapDifference % 3 != 0)
	    return TRUE;
	}
    }
return FALSE;
}


void mapAndOutput(struct genbankCds *cds, char *source, 
	struct dnaSeq *rnaSeq, struct psl *psl, struct dnaSeq *txSeq, FILE *f)
/* Map cds through psl from rnaSeq to txSeq.  If mapping is good write to file */
{
boolean selenocysteine = (hashLookup(selenocysteineHash, psl->qName) != NULL);

/* First, because we're paranoid, check that input RNA CDS really is an
 * open reading frame. */
if (!checkCds(cds, rnaSeq, selenocysteine))
    return;

/* We don't map on reverse strand.  Supposively we are all on +
 * strand by now. */
if (psl->strand[0] != '+' || psl->strand[1] != 0)
    {
    unmappedPrint("%s/%s has funny strand %s, skipping\n", psl->qName, psl->tName, 
    	psl->strand);
    return;
    }

/* Next, attempt to map through psl, which is a lot easier since we're on
 * + strand. */
int mappedStart = -1, mappedEnd = -1;
int i;
for (i=0; i<psl->blockCount; ++i)
    {
    int blockSize = psl->blockSizes[i];
    int tStart = psl->tStarts[i];
    int qStart = psl->qStarts[i];
    int qEnd = qStart + blockSize;
    if (rangeIntersection(qStart,qEnd,cds->start,cds->end) > 0)
        {
	if (qStart <= cds->start && cds->start < qEnd)
	    {
	    int placeInBlock = cds->start - qStart;
	    mappedStart = tStart + placeInBlock;
	    }
	if (qStart < cds->end && cds->end <= qEnd)
	    {
	    int placeInBlock = cds->end - qStart;
	    mappedEnd = tStart + placeInBlock;
	    }
	}
    }

/* If we mapped it output it. */
if (mappedStart >= 0 && mappedEnd >= 0)
    {
    int cdsSize = mappedEnd - mappedStart;
    if (cdsSize%3 == 0 || cdsSize == 0)
        {
	if (!hasFrameShiftInCds(psl, mappedStart, mappedEnd))
	    {
	    if (!hasStopCodons(txSeq->dna + mappedStart, cdsSize/3 - 1, selenocysteine))
	        {
		char *s = txSeq->dna + mappedStart;
		char *e = txSeq->dna + mappedEnd;
		int score = 1000 - 50*pslCalcMilliBad(psl, FALSE);
		if (score < 0) score = 0;
		fprintf(f, "%s\t", psl->tName);
		fprintf(f, "%d\t", mappedStart);
		fprintf(f, "%d\t", mappedEnd);
		fprintf(f, "%s\t", source);
		fprintf(f, "%s\t", psl->qName);
		fprintf(f, "%d\t", score);
		fprintf(f, "%d\t", startsWith("atg", s));
		fprintf(f, "%d\t", isStopCodon(e-3));
		fprintf(f, "1\t");	/* Block count */
		fprintf(f, "%d,\t", mappedStart);
		fprintf(f, "%d,\n", mappedEnd - mappedStart);
		}
	    else
	        {
		unmappedPrint("%s has stop codon in mapped CDS %d %d\n", psl->qName,
			mappedStart, mappedEnd);
		}
	    }
	else
	    {
	    unmappedPrint("%s frame shift in mapped CDS %d %d\n", psl->qName,
	    	mappedStart, mappedEnd);
	    }
	}
    else
        {
	unmappedPrint("%s mapped CDS not a multiple of 3 %d %d\n", 
	    psl->qName, mappedStart, mappedEnd);
	}
    }
else
    {
    unmappedPrint("%s ends not mapped %d %d\n", 
    	psl->qName, mappedStart, mappedEnd);
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

void txCdsEvFromRna(char *rnaFa, char *rnaCds, char *txRnaPsl, char *txFa, 
	char *output)
/* txCdsEvFromRna - Convert transcript/rna alignments, genbank CDS file, 
 * and other info to transcript CDS evidence (tce) file.. */
{
/* Read sequences and CDSs into hash */
struct hash *txSeqHash = faReadAllIntoHash(txFa, dnaLower);
verbose(2, "Read %d sequences from %s\n", txSeqHash->elCount, txFa);
struct hash *rnaSeqHash = faReadAllIntoHash(rnaFa, dnaLower);
verbose(2, "Read %d sequences from %s\n", rnaSeqHash->elCount, rnaFa);
struct hash *cdsHash = cdsReadAllIntoHash(rnaCds);
verbose(2, "Read %d cds records from %s\n", cdsHash->elCount, rnaCds);

/* Create hash of sources. */
struct hash *sourceHash = getSourceHash();

/* Loop through input one psl at a time writing output. */
struct lineFile *lf = pslFileOpen(txRnaPsl);
FILE *f = mustOpen(output, "w");
struct psl *psl;
while ((psl = pslNext(lf)) != NULL)
    {
    verbose(3, "Processing %s vs %s\n", psl->qName, psl->tName);
    struct genbankCds *cds = hashFindVal(cdsHash, psl->qName);
    if (cds != NULL)
        {
	char *source = hashFindVal(sourceHash, psl->qName);
	if (source == NULL)
	    source = defaultSource;
	struct dnaSeq *txSeq = hashFindVal(txSeqHash, psl->tName);
	if (txSeq == NULL)
	    errAbort("%s is in %s but not %s", psl->tName, txRnaPsl, txFa);
	struct dnaSeq *rnaSeq = hashFindVal(rnaSeqHash, psl->qName);
	if (rnaSeq == NULL)
	    errAbort("%s is in %s but not %s", psl->qName, txRnaPsl, rnaFa);
	mapAndOutput(cds, source, rnaSeq, psl, txSeq, f);
	}
    else
        unmappedPrint("%s has no CDS in input %s\n",  psl->qName, rnaCds);
    }


lineFileClose(&lf);
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
refStatusFile = optionVal("refStatus", refStatusFile);
mgcStatusFile = optionVal("mgcStatus", mgcStatusFile);
defaultSource = optionVal("source", defaultSource);
if (optionExists("unmapped"))
    fUnmapped = mustOpen(optionVal("unmapped", NULL), "w");
makeExceptionHashes();
txCdsEvFromRna(argv[1], argv[2], argv[3], argv[4], argv[5]);
carefulClose(&fUnmapped);
return 0;
}
