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
  "  -cdsFa=fasta - output FASTA with CDS that was used to generate protein.\n"
  "                 This will not include dropped partial codons.\n"
  "  -protIdSuffix=str - add this string to the end of the name for protein FASTA\n"
  "  -cdsIdSuffix=str - add this string to the end of the name for CDS FASTA\n"
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
    {"protIdSuffix", OPTION_STRING},
    {"cdsIdSuffix", OPTION_STRING},
    {"translateSeleno", OPTION_BOOLEAN},
    {"includeStop", OPTION_BOOLEAN},
    {"starForInframeStops", OPTION_BOOLEAN},
    {NULL, 0},
};

static char *protIdSuffix = "";
static char *cdsIdSuffix = "";

static void writeFa(struct genePred *gp, char* seq, FILE* faFh, char* suffix)
/* write fasta record, generating comment with genomic location */
{
char startLine[512];
safef(startLine, sizeof(startLine), "%s%s %s:%d-%d", gp->name, suffix,
      gp->chrom, gp->txStart, gp->txEnd);
faWriteNext(faFh, startLine, seq, strlen(seq));
}

static void translateGenePred(struct genePred *gp, struct nibTwoCache* genomeSeqs,
                              unsigned options, FILE* protFaFh, FILE* cdsFaFh)
/* translate one genePred record. */
{
char *cds, *prot;
genePredTranslate(gp, genomeSeqs, options, &prot, &cds);

writeFa(gp, prot, protFaFh, protIdSuffix);
if (cdsFaFh != NULL)
    writeFa(gp, cds, cdsFaFh, cdsIdSuffix);
freeMem(prot);
freeMem(cds);
}

void genePredToProt(char *genePredFile, char* genomeSeqsFile, unsigned options,
                    char* protFaFile, char *cdsFaFile)
/* genePredToProt - create protein sequences by translating gene annotations. */
{
struct genePred *genePreds = genePredReaderLoadFile(genePredFile, NULL);
struct nibTwoCache* genomeSeqs = nibTwoCacheNew(genomeSeqsFile);
slSort(&genePreds, genePredCmp);  // genomic order is much faster
FILE* protFaFh = mustOpen(protFaFile, "w");
FILE* cdsFaFh = (cdsFaFile != NULL) ? mustOpen(cdsFaFile, "w") : NULL;

struct genePred *gp;
for (gp = genePreds; gp != NULL; gp = gp->next)
    {
    if (gp->cdsStart < gp->cdsEnd)
        translateGenePred(gp, genomeSeqs, options, protFaFh, cdsFaFh);
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
protIdSuffix = optionVal("protIdSuffix", ""),
cdsIdSuffix = optionVal("cdsIdSuffix", "");
unsigned options = 0;
if (optionExists("translateSeleno"))
    options |= GENEPRED_TRANSLATE_SELENO;
if (optionExists("includeStop"))
    options |= GENEPRED_TRANSLATE_INCLUDE_STOP;
if (optionExists("starForInframeStops"))
    options |= GENEPRED_TRANSLATE_STAR_INFRAME_STOPS;

genePredToProt(argv[1], argv[2], options, argv[3],
               optionVal("cdsFa", NULL));
return 0;
}
