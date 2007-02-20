/* txCdsToGene - Convert transcript bed and best cdsEvidence to genePred and 
 * protein sequence.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "fa.h"
#include "bed.h"
#include "dnautil.h"
#include "genePred.h"
#include "cdsEvidence.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsToGene - Convert transcript bed and best cdsEvidence to genePred and \n"
  "protein sequence.\n"
  "usage:\n"
  "   txCdsToGene in.bed in.fa in.tce out.gtf out.fa\n"
  "Where:\n"
  "   in.bed describes the genome position of transcripts, often from txWalk\n"
  "   in.fa contains the transcript sequence\n"
  "   in.tce is the best cdsEvidence (from txCdsPick) for the transcripts\n"
  "          For noncoding transcripts it need not have a line\n"
  "   out.gtf is the output gene predictions\n"
  "   out.fa is the output protein predictions\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *cdsEvidenceReadAllIntoHash(char *fileName)
/* Return hash full of cdsEvidence keyed by transcript name. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = hashNew(18);
char *row[CDSEVIDENCE_NUM_COLS];
while (lineFileRowTab(lf, row))
    {
    struct cdsEvidence *cds = cdsEvidenceLoad(row);
    if (hashLookup(hash, cds->name))
        errAbort("%s duplicated in %s, perhaps you want to run txCdsPick?",
		cds->name, fileName);
    hashAdd(hash, cds->name, cds);
    }
lineFileClose(&lf);
return hash;
}

void outputGtf(struct cdsEvidence *cds, struct bed *bed, FILE *f)
/* Write out bed in gtf format. */
{
}

void outputProtein(struct cdsEvidence *cds, struct dnaSeq *txSeq, FILE *f)
/* Translate txSeq to protein guided by cds, and output to file */
{
struct dyString *dy = dyStringNew(4*1024);
int blockIx;
for (blockIx=0; blockIx<cds->cdsCount; ++blockIx)
    {
    DNA *dna = txSeq->dna + cds->cdsStarts[blockIx];
    int aaSize = cds->cdsSizes[blockIx];
    assert(aaSize%3 == 0);
    int i;
    for (i=0; i<aaSize; ++i)
        {
	AA aa = lookupCodon(dna);
	assert(aa != 0);
	dyStringAppendC(dy, aa);
	dna += 3;
	}
    }
faWriteNext(f, cds->name, dy->string, dy->stringSize);
dyStringFree(&dy);
}

void txCdsToGene(char *txBed, char *txFa, char *txCds, char *outGtf, char *outFa)
/* txCdsToGene - Convert transcript bed and best cdsEvidence to genePred and 
 * protein sequence. */
{
struct hash *txSeqHash = faReadAllIntoHash(txFa, dnaLower);
verbose(2, "Read %d transcript sequences from %s\n", txSeqHash->elCount, txFa);
struct hash *cdsHash = cdsEvidenceReadAllIntoHash(txCds);
verbose(2, "Read %d cdsEvidence from %s\n", cdsHash->elCount, txCds);
struct lineFile *lf = lineFileOpen(txBed, TRUE);
FILE *fGtf = mustOpen(outGtf, "w");
FILE *fFa = mustOpen(outFa, "w");
char *row[12];
while (lineFileRow(lf, row))
    {
    struct bed *bed = bedLoad12(row);
    verbose(2, "processing %s\n", bed->name);
    struct cdsEvidence *cds = hashFindVal(cdsHash, bed->name);
    struct dnaSeq *txSeq = hashFindVal(txSeqHash, bed->name);
    if (txSeq == NULL)
        errAbort("%s is in %s but not %s", bed->name, txBed, txFa);
    outputGtf(cds, bed, fGtf);
    if (cds != NULL)
        outputProtein(cds, txSeq, fFa);
    bedFree(&bed);
    }
lineFileClose(&lf);
carefulClose(&fFa);
carefulClose(&fGtf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
dnaUtilOpen();
if (argc != 6)
    usage();
txCdsToGene(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
