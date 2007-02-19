/* txCdsEvFromRna - Convert transcript/rna alignments, genbank CDS file, 
 * and other info to transcript CDS evidence (tce) file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "psl.h"
#include "genbank.h"
#include "fa.h"

/* Variables set from command line. */
char *refStatusFile = NULL;
char *mgcStatusFile = NULL;
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
  , defaultSource
  );
}

static struct optionSpec options[] = {
   {"refSeqStatus", OPTION_STRING},
   {"mgcStatus", OPTION_STRING},
   {"source", OPTION_STRING},
   {NULL, 0},
};

struct hash *faReadAllIntoHash(char *fileName)
/* Return hash full of dnaSeq (lower case) from file */
{
struct dnaSeq *seq, *list = faReadAllDna(fileName);
struct hash *hash = hashNew(18);
for (seq = list; seq != NULL; seq = seq->next)
    {
    if (hashLookup(hash, seq->name))
        errAbort("%s duplicated in %s", seq->name, fileName);
    hashAdd(hash, seq->name, seq);
    }
return hash;
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
	safef(typeBuf, sizeof(typeBuf), "RefSeq%s", row[2]);
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
	safef(typeBuf, sizeof(typeBuf), "MGC%s", row[2]);
	char *type = hashStoreName(typeHash, typeBuf);
	hashAdd(sourceHash, row[0], type);
	}
    lineFileClose(&lf);
    }
return sourceHash;
}

boolean isStopCodon(char *dna)
/* Return TRUE if it's a stop codon. */
{
return (startsWith("taa", dna) || startsWith("tga", dna) || startsWith("tag", dna));
}

boolean checkCds(struct genbankCds *cds, struct dnaSeq *seq)
/* Make sure no stop codons, and if marked complete that it does
 * really start with ATG and end with TAA/TAG/TGA */
{
int size = cds->end - cds->start;
if (size%3 != 0)
    {
    verbose(2, "%s CDS size %d not a multiple of 3\n", seq->name, size);
    return FALSE;
    }
size /= 3;
char *dna = seq->dna + cds->start;
if (cds->startComplete)
    {
    if (!startsWith("atg", dna))
	{
        verbose(2, "%s \"start complete\" but begins with %c%c%c\n", seq->name,
		dna[0], dna[1], dna[2]);
	return FALSE;
	}
    }
int i;
for (i=1; i<size; ++i)
    {
    if (isStopCodon(dna))
	{
        verbose(2, "%s has internal stop at codon %d of %d\n", seq->name, i-1, size);
	return FALSE;
	}
    }
if (cds->endComplete)
    {
    dna = seq->dna + cds->end - 3;
    if (!isStopCodon(dna))
	{
        verbose(2, "%s \"end complete\" but ends with %c%c%c\n", seq->name,
		dna[0], dna[1], dna[2]);
	return FALSE;
	}
    }
return TRUE;
}

void mapAndOutput(struct genbankCds *cds, char *source, 
	struct dnaSeq *rnaSeq, struct psl *psl, struct dnaSeq *txSeq, FILE *f)
/* Map cds through psl from rnaSeq to txSeq.  If mapping is good write to file */
{
/* First, because we're paranoid, check that input RNA CDS really is an
 * open reading frame. */
if (!checkCds(cds, rnaSeq))
    return;
}

void txCdsEvFromRna(char *rnaFa, char *rnaCds, char *txRnaPsl, char *txFa, 
	char *output)
/* txCdsEvFromRna - Convert transcript/rna alignments, genbank CDS file, 
 * and other info to transcript CDS evidence (tce) file.. */
{
/* Read sequences and CDSs into hash */
struct hash *txSeqHash = faReadAllIntoHash(txFa);
verbose(2, "Read %d sequences from %s\n", txSeqHash->elCount, txFa);
struct hash *rnaSeqHash = faReadAllIntoHash(rnaFa);
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
defaultSource = optionVal("defaultSource", defaultSource);
txCdsEvFromRna(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
