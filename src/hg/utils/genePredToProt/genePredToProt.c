/* genePredToProt - create protein sequences by translating gene annotations. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "genePred.h"
#include "genePredReader.h"
#include "nibTwo.h"
#include "fa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "genePredToProt - create protein sequences by translating gene annotations\n"
  "usage:\n"
  "   genePredToProt genePredFile genomeSeqs protFa\n\n"
  "This honors frame if genePred has frames, dropping partial codons.\n"
  "genomeSeqs is a 2bit or directory of nib files.\n\n"
  "options:\n"
  "  -cdsFa=fasta - output fasta with CDS that was used to generate protein.\n"
  "                 This will not include dropped partial codons.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {"cdsFa", OPTION_STRING},
    {NULL, 0},
};

static struct dnaSeq* getGeneRegion(struct nibTwoCache* genomeSeqs,  struct genePred *genePred, int start, int end)
/* Get the genome sequence covering the a region of a gene, in transcription order */
{
struct dnaSeq *dna = nibTwoCacheSeqPart(genomeSeqs, genePred->chrom, start, end-start, NULL);
if (genePred->strand[0] == '-')
    reverseComplement(dna->dna, dna->size);
return dna;
}

static int removePartialCodon(char* cds, int iCds, int nextFrame)
/* remove partial codon that has already been copied to CDS */
{
int iCdsNew = iCds - nextFrame;
if (iCdsNew < 0)
    iCdsNew = 0;
zeroBytes(cds, (iCds - iCdsNew));
return iCdsNew;
}

static void addCdsExon(struct nibTwoCache* genomeSeqs, struct genePred *genePred,
                       int exonCdsStart, int exonCdsEnd, int frame,
                       char *cds, int *iCds, int *nextFrame)
/* get CDS from one exon */
{
struct dnaSeq *dna = getGeneRegion(genomeSeqs, genePred, exonCdsStart, exonCdsEnd);
int iDna = 0;
if (frame != *nextFrame)
    {
    // adjust for frame shift, dropping partial codon
    *iCds = removePartialCodon(cds, *iCds, *nextFrame);
    iDna += frame;
    *nextFrame = 0;
    }

int iCdsStart = *iCds;
while (iDna < dna->size)
    cds[(*iCds)++] = dna->dna[iDna++];
*nextFrame = ((*nextFrame) + (*iCds - iCdsStart)) % 3;
dnaSeqFree(&dna);
}

static char* getCdsCodons(struct genePred *genePred, struct nibTwoCache* genomeSeqs)
/* get the CDS sequence, dropping partial codons */
{
char *cds = needMem(genePredCdsSize(genePred) + 1);
// walk in direction of transcription
int dir = (genePred->strand[0] == '+') ? 1 : -1;
int iExon = (dir > 0) ? 0 : genePred->exonCount-1;
int iStop = (dir > 0) ? genePred->exonCount : -1;
int nextFrame = 0, iCds = 0, exonCdsStart, exonCdsEnd;
for (; iExon != iStop; iExon += dir)
    {
    if (genePredCdsExon(genePred, iExon, &exonCdsStart, &exonCdsEnd))
        addCdsExon(genomeSeqs, genePred, exonCdsStart, exonCdsEnd, genePred->exonFrames[iExon], cds, &iCds, &nextFrame);
    }
if (nextFrame != 0)
    removePartialCodon(cds, iCds, nextFrame);
assert((strlen(cds) % 3) == 0);  // ;-)
return cds;
}

static char* translateCds(char* chrom, char* cds)
/* translate the CDS */
{
int cdsLen = strlen(cds);
char *prot = needMem((cdsLen/3)+1);
boolean isChrM = sameString(chrom, "chrM");
int iCds, iProt;
for (iCds = 0, iProt = 0; iCds < cdsLen; iCds+=3, iProt++)
    {
    prot[iProt] = isChrM ? lookupMitoCodon(cds+iCds)
        : lookupCodon(cds+iCds);
    if ((prot[iProt] == '\0') && (iCds < cdsLen-3))  
        prot[iProt] = 'X';  // selenocysteine
    }
return prot;
}

static void writeFa(struct genePred *genePred, char* seq, FILE* protFaFh)
/* write fasta record, generating comment with genomic location */
{
char startLine[512];
safef(startLine, sizeof(startLine), "%s %s:%d-%d", genePred->name,
      genePred->chrom, genePred->txStart, genePred->txEnd);
faWriteNext(protFaFh, startLine, seq, strlen(seq));
}

static void translateGenePred(struct genePred *genePred, struct nibTwoCache* genomeSeqs,
                              FILE* protFaFh, FILE* cdsFaFh)
/* translate one genePred record. */
{
if (genePred->exonFrames == NULL)
    genePredAddExonFrames(genePred);  // assume correct frame if not included
char* cds = getCdsCodons(genePred, genomeSeqs);
char *prot = translateCds(genePred->chrom, cds);
writeFa(genePred, prot, protFaFh);
if (cdsFaFh != NULL)
    writeFa(genePred, cds, cdsFaFh);
freeMem(prot);
freeMem(cds);
}


void genePredToProt(char *genePredFile, char* genomeSeqsFile, char* protFaFile,
                    char *cdsFaFile)
/* genePredToProt - create protein sequences by translating gene annotations. */
{
struct genePred *genePreds = genePredReaderLoadFile(genePredFile, NULL);
struct nibTwoCache* genomeSeqs = nibTwoCacheNew(genomeSeqsFile);
slSort(&genePreds, genePredCmp);  // genomic order
FILE* protFaFh = mustOpen(protFaFile, "w");
FILE* cdsFaFh = (cdsFaFile != NULL) ? mustOpen(cdsFaFile, "w") : NULL;

struct genePred *genePred;
for (genePred = genePreds; genePred != NULL; genePred = genePred->next)
    {
    if (genePred->cdsStart < genePred->cdsEnd)
        translateGenePred(genePred, genomeSeqs, protFaFh, cdsFaFh);
    }

carefulClose(&protFaFh);
carefulClose(&cdsFaFh);
genePredFreeList(&genePreds);
nibTwoCacheFree(&genomeSeqs);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
genePredToProt(argv[1], argv[2], argv[3], optionVal("cdsFa", NULL));
return 0;
}
