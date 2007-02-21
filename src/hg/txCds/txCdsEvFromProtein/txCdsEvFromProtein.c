/* txCdsEvFromProtein - Convert transcript/protein alignments and other evidence into a transcript CDS evidence (tce) file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "psl.h"

/* Variables set from command line. */
char *refStatusFile = NULL;
char *uniStatusFile = NULL;
FILE *fUnmapped = NULL;
char *defaultSource = "blatUniprot";

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
  , defaultSource
  );
}

static struct optionSpec options[] = {
   {"refStatus", OPTION_STRING},
   {"uniStatus", OPTION_STRING},
   {"source", OPTION_STRING},
   {"unmapped", OPTION_STRING},
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
	    source = "tremble";
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

boolean aliGotStopCodons(struct psl *psl, struct dnaSeq *seq)
/* Return TRUE if alignment includes stop codons. */
{
int i;
for (i=0; i<psl->blockCount; ++i)
    {
    int qBlockSize = psl->blockSizes[i];
    int tStart = psl->tStarts[i];
    DNA *dna = seq->dna + tStart;
    int i;
    for (i=0; i<qBlockSize; ++i)
        {
	if (isStopCodon(dna))
	    return TRUE;
	dna += 3;
	}
    }
return FALSE;
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



void mapAndOutput(char *source, aaSeq *protSeq, struct psl *psl, 
	struct dnaSeq *txSeq, FILE *f)
/* Attempt to define CDS in txSeq by looking at alignment between
 * it and a protein. */
{
if (!sameString(psl->strand, "++"))
    {
    unmappedPrint("%s alignment with %s funny strand %s\n", psl->qName, psl->tName,
    	psl->strand);
    return;
    }
removeNegativeGaps(psl);
if (aliGotStopCodons(psl, txSeq))
    {
    unmappedPrint("%s alignment with %s has stop codons\n", psl->qName, psl->tName);
    return;
    }
int mappedStart = psl->tStart;
int mappedEnd = psl->tEnd+3;
char *s = txSeq->dna + mappedStart;
char *e = txSeq->dna + mappedEnd-3;
verbose(3, "%s s %c%c%c e %c%c%c %d %d \n", psl->qName, s[0], s[1], s[2], e[0], e[1], e[2], mappedStart, mappedEnd);
int milliBad = pslCalcMilliBad(psl, FALSE);
int score = 1000 - 10*milliBad;
if (score <= 0)
    {
    unmappedPrint("%s too divergent compared to %s (%4.1f%% sequence identity)\n", 
    	psl->qName, psl->tName, 0.1*(1000-milliBad));
    return;
    }
int totalCovAa = totalAminoAcids(psl);
double coverage = (double)totalCovAa/psl->qSize;
if (coverage < 0.75)
    {
    unmappedPrint("%s covers too little of %s (%d of %d amino acids)\n",
    	psl->qName, psl->tName, totalCovAa, psl->qSize);
    return;
    }
score *= coverage;
fprintf(f, "%s\t", psl->tName);
fprintf(f, "%d\t", mappedStart);
fprintf(f, "%d\t", mappedEnd);
fprintf(f, "%s\t", source);
fprintf(f, "%s\t", psl->qName);
fprintf(f, "%d\t", score);
fprintf(f, "%d\t", startsWith("atg", s));
fprintf(f, "%d\t", isStopCodon(e));
fprintf(f, "%d\t", psl->blockCount);	
int i;
for (i=0; i < psl->blockCount; ++i)
    fprintf(f, "%d,", psl->tStarts[i]);
fprintf(f, "\t");
for (i=0; i < psl->blockCount; ++i)
    fprintf(f, "%d,", psl->blockSizes[i]*3);
fprintf(f, "\n");
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
txCdsEvFromProtein(argv[1], argv[2], argv[3], argv[4]);
carefulClose(&fUnmapped);
return 0;
}
