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
#include "psl.h"

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
  "  -cdsFa=fasta - output FASTA with CDS that was used to generate protein.\n"
  "                 This will not include dropped partial codons.\n"
  "  -cdsPsl=psl - output a PSL with alignment of the CDS region to the genome.\n"
  "  -protIdSuffix=str - add this string to the end of the name for protein FASTA\n"
  "  -cdsIdSuffix=str - add this string to the end of the name for CDS FASTA and PSL\n"
  "  -translateSeleno - assume internal TGA code for selenocysteine and translate to `U'.\n"
  "  -includeStop - If the CDS ends with a stop codon, represent it as a `*'\n"
  "  -starForInframeStops - use `*' instead of `X' for in-frame stop codons.\n"
  "                  This will result in selenocysteine's being `*', with only codons\n"
  "                  containing `N' being translated to `X'.  This doesn't include terminal\n"
  "                  stop\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {"cdsFa", OPTION_STRING},
    {"cdsPsl", OPTION_STRING},
    {"protIdSuffix", OPTION_STRING},
    {"cdsIdSuffix", OPTION_STRING},
    {"translateSeleno", OPTION_BOOLEAN},
    {"includeStop", OPTION_BOOLEAN},
    {"starForInframeStops", OPTION_BOOLEAN},
    {NULL, 0},
};

static char *protIdSuffix = "";
static char *cdsIdSuffix = "";
static boolean translateSeleno = FALSE;
static boolean includeStop = FALSE;
static boolean starForInframeStops = FALSE;

struct cds
/* CDS sequence being assembled */
{
    char *bases;    // CDS string being built
    int iCds;       // Index into CDS
    int nextFrame;  // next required frame
    struct psl* cdsPsl;     // PSL of being built, or NULL if not being constructed
    int cdsPslBlockSpace;   // allocated space in psl
};

static struct dnaSeq* getGeneRegion(struct nibTwoCache* genomeSeqs,  struct genePred *genePred, int start, int end,
                                    int *chromSize)
/* Get the genome sequence covering the a region of a gene, in transcription order */
{
struct dnaSeq *dna = nibTwoCacheSeqPart(genomeSeqs, genePred->chrom, start, end-start, chromSize);
if (genePred->strand[0] == '-')
    reverseComplement(dna->dna, dna->size);
return dna;
}

static void removePartialCodon(struct cds* cds)
/* remove partial codon that has already been copied to CDS */
{
int iCdsNew = cds->iCds - cds->nextFrame;
if (iCdsNew < 0)
    iCdsNew = 0;
zeroBytes(cds->bases+iCdsNew, (cds->iCds - iCdsNew));
cds->iCds = iCdsNew;
cds->nextFrame = 0;
}

static void addCdsExonBases(struct nibTwoCache* genomeSeqs, struct genePred *genePred,
                            int exonCdsStart, int exonCdsEnd, struct cds* cds, int *chromSize)
{
struct dnaSeq *dna = getGeneRegion(genomeSeqs, genePred, exonCdsStart, exonCdsEnd, chromSize);
int iDna = 0;
int iCdsStart = cds->iCds;
while (iDna < dna->size)
    cds->bases[cds->iCds++] = dna->dna[iDna++];
cds->nextFrame = ((cds->nextFrame) + (cds->iCds - iCdsStart)) % 3;
dnaSeqFree(&dna);
}
    
static void addCdsExonPslBlock(struct genePred *genePred,
                               int exonCdsStart, int exonCdsEnd, int chromSize,
                               struct cds* cds)
/* add a PSL block for current CDS portion of exon */
{
int iBlock = cds->cdsPsl->blockCount;
int exonCdsSize = exonCdsEnd - exonCdsStart;
assert(exonCdsSize <= cds->iCds);
if (cds->cdsPsl->blockCount >= cds->cdsPslBlockSpace)
    pslGrow(cds->cdsPsl, &cds->cdsPslBlockSpace);

cds->cdsPsl->blockSizes[iBlock] = exonCdsSize;

// query coordinates, always + strand
cds->cdsPsl->qSize = cds->cdsPsl->qEnd = cds->iCds;
cds->cdsPsl->qStarts[iBlock] = cds->iCds - exonCdsSize;

// target coordinates, strand-specific, will get bounds later
cds->cdsPsl->tSize = chromSize;
if (genePred->strand[0] == '+')
    cds->cdsPsl->tStarts[iBlock] = exonCdsStart;
else
    cds->cdsPsl->tStarts[iBlock] = chromSize - exonCdsEnd;

cds->cdsPsl->match += exonCdsSize;
cds->cdsPsl->blockCount++;
}

static void finishCdsPsl(struct psl* cdsPsl)
/* finish constructing the CDS psl */
{
cdsPsl->tStart = cdsPsl->tStarts[0];
cdsPsl->tEnd = pslTEnd(cdsPsl, cdsPsl->blockCount-1);
if (cdsPsl->strand[1] == '-')
    {
    reverseIntRange(&cdsPsl->tStart, &cdsPsl->tEnd, cdsPsl->tSize);
    pslRc(cdsPsl);   // make standard blat mRNA alignment
    }
cdsPsl->strand[1] = '\0';
pslComputeInsertCounts(cdsPsl);
}

static void addCdsExon(struct nibTwoCache* genomeSeqs, struct genePred *genePred,
                       int exonCdsStart, int exonCdsEnd, int frame,
                       struct cds* cds)
/* get CDS from one exon */
{
// adjust for frame shift, dropping partial codon
if (frame != cds->nextFrame)
    {
    removePartialCodon(cds);
    if (genePred->strand[0] == '+')
        exonCdsStart += frame;
    else
        exonCdsEnd -= frame;
    }
if (exonCdsStart < exonCdsEnd)
    {
    int chromSize = 0;
    addCdsExonBases(genomeSeqs, genePred, exonCdsStart, exonCdsEnd, cds, &chromSize);
    addCdsExonPslBlock(genePred, exonCdsStart, exonCdsEnd, chromSize, cds);
    }
}

static char* getCdsCodons(struct genePred *genePred, struct nibTwoCache* genomeSeqs, struct psl **cdsPsl)
/* get the CDS sequence, dropping partial codons */
{
struct cds cds;
cds.bases = needMem(genePredCdsSize(genePred) + 1);
cds.iCds = 0;
cds.nextFrame = 0;
cds.cdsPslBlockSpace = genePred->exonCount;
char strand[3] = {'+', genePred->strand[0], '\0'};  // builds with query + strand
char qName[512];
safef(qName, sizeof(qName), "%s%s", genePred->name, cdsIdSuffix);
cds.cdsPsl = pslNew(qName, 0, 0, 0, genePred->chrom,  0, 0, 0, strand,
                    cds.cdsPslBlockSpace, 0);
     
// walk in direction of transcription
int dir = (genePred->strand[0] == '+') ? 1 : -1;
int iExon = (dir > 0) ? 0 : genePred->exonCount-1;
int iStop = (dir > 0) ? genePred->exonCount : -1;
int exonCdsStart, exonCdsEnd;
for (; iExon != iStop; iExon += dir)
    {
    if (genePredCdsExon(genePred, iExon, &exonCdsStart, &exonCdsEnd))
        addCdsExon(genomeSeqs, genePred, exonCdsStart, exonCdsEnd, genePred->exonFrames[iExon], &cds);
    }
if (cds.nextFrame != 0)
    removePartialCodon(&cds);
assert((strlen(cds.bases) % 3) == 0);  // ;-)
finishCdsPsl(cds.cdsPsl);
*cdsPsl = cds.cdsPsl;
return cds.bases;
}

static char translateCodon(boolean isChrM, char* codon, bool lastCodon)
/* translate the first three bases starting at codon, handling weird
 * biology as requested giving */
{
char aa = isChrM ? lookupMitoCodon(codon) : lookupCodon(codon);
if (aa == '\0')
    {
    // stop, contains `N' or selenocysteine
    boolean isStopOrSelno = isStopCodon(codon);
    boolean isRealStop = isReallyStopCodon(codon, !lastCodon); // internal could be selenocysteine
    if (lastCodon)
        {
        if (includeStop)
            aa = '*';
        else if (!isRealStop)
            aa = 'X';
        // others, \0' will terminate
        }
    else if (translateSeleno && isStopOrSelno && !isRealStop)
        {
        aa = 'U';
        }
    else if (isRealStop && starForInframeStops)
        {
        aa = '*';
        }
    else
        {
        aa = 'X';
        }
    }
return aa;
}

static char* translateCds(char* chrom, char* cds)
/* translate the CDS */
{
int cdsLen = strlen(cds);
char *prot = needMem((cdsLen/3)+1);
boolean isChrM = sameString(chrom, "chrM");
int iCds, iProt;
for (iCds = 0, iProt = 0; iCds < cdsLen; iCds+=3, iProt++)
    prot[iProt] = translateCodon(isChrM, cds+iCds, (iCds == cdsLen-3));
return prot;
}

static void writeFa(struct genePred *genePred, char* seq, FILE* faFh, char* suffix)
/* write fasta record, generating comment with genomic location */
{
char startLine[512];
safef(startLine, sizeof(startLine), "%s%s %s:%d-%d", genePred->name, suffix,
      genePred->chrom, genePred->txStart, genePred->txEnd);
faWriteNext(faFh, startLine, seq, strlen(seq));
}

static void translateGenePred(struct genePred *genePred, struct nibTwoCache* genomeSeqs,
                              FILE* protFaFh, FILE* cdsFaFh, FILE* cdsPslFh)
/* translate one genePred record. */
{
if (genePred->exonFrames == NULL)
    genePredAddExonFrames(genePred);  // assume correct frame if not included
struct psl *cdsPsl = NULL;
char* cds = getCdsCodons(genePred, genomeSeqs, &cdsPsl);
char *prot = translateCds(genePred->chrom, cds);
writeFa(genePred, prot, protFaFh, protIdSuffix);
if (cdsFaFh != NULL)
    writeFa(genePred, cds, cdsFaFh, cdsIdSuffix);
if (cdsPslFh != NULL)
    pslTabOut(cdsPsl, cdsPslFh);
freeMem(prot);
pslFree(&cdsPsl);
freeMem(cds);
}

void genePredToProt(char *genePredFile, char* genomeSeqsFile, char* protFaFile,
                    char *cdsFaFile, char *cdsPslFile)
/* genePredToProt - create protein sequences by translating gene annotations. */
{
struct genePred *genePreds = genePredReaderLoadFile(genePredFile, NULL);
struct nibTwoCache* genomeSeqs = nibTwoCacheNew(genomeSeqsFile);
slSort(&genePreds, genePredCmp);  // genomic order
FILE* protFaFh = mustOpen(protFaFile, "w");
FILE* cdsFaFh = (cdsFaFile != NULL) ? mustOpen(cdsFaFile, "w") : NULL;
FILE* cdsPslFh = (cdsPslFile != NULL) ? mustOpen(cdsPslFile, "w") : NULL;

struct genePred *genePred;
for (genePred = genePreds; genePred != NULL; genePred = genePred->next)
    {
    if (genePred->cdsStart < genePred->cdsEnd)
        translateGenePred(genePred, genomeSeqs, protFaFh, cdsFaFh, cdsPslFh);
    }

carefulClose(&protFaFh);
carefulClose(&cdsPslFh);
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
protIdSuffix = optionVal("protIdSuffix", ""),
cdsIdSuffix = optionVal("cdsIdSuffix", "");
translateSeleno = optionExists("translateSeleno");
includeStop = optionExists("includeStop");
starForInframeStops = optionExists("starForInframeStops");

genePredToProt(argv[1], argv[2], argv[3],
               optionVal("cdsFa", NULL),
               optionVal("cdsPsl", NULL));
return 0;
}
