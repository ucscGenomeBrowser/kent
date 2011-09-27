/* txGeneProtAndRna - Create fasta files with our proteins and transcripts. These echo 
 * RefSeq when gene is based on RefSeq. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "dnaseq.h"
#include "fa.h"
#include "txInfo.h"
#include "txCommon.h"
#include "bed.h"

static char const rcsid[] = "$Id: txGeneProtAndRna.c,v 1.4 2008/09/23 04:34:54 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneProtAndRna - Create fasta files with our proteins and transcripts:\n"
  "both (a) representative sequences and (b) direct transcriptions/translations\n"
  "The representatives echo RefSeq when gene is based on RefSeq. Otherwise they are\n"
  "taken from the genome.  The direct transcriptions/translations are always taken\n"
  "from the genome.\n"
  "usage:\n"
  "   txGeneProtAndRna tx.bed tx.info tx.fa tx.faa refSeq.fa refPep.fa txToAcc.tab outRepresentativeTx.fa outRepresentativePep.fa outDirectTx.fa outDirectPep.fa\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void txGeneProtAndRna(char *inBed, char *inInfo, char *txFa,  char *txFaa,
	char *refSeqFa, char *refToPepFile, char *refPepFa, char *txToAccFile, 
        char *outRepTxFa, char *outRepPepFa, char *outDirectTxFa, char *outDirectPepFa)
/* txGeneProtAndRna - Create fasta files with our proteins and transcripts. 
 * These echo RefSeq when gene is based on RefSeq.. */
{
/* Load up input bed list. */
struct bed *bed, *bedList = bedLoadNAll(inBed, 12);

/* Load info into hash. */
struct hash *infoHash = hashNew(18);
struct txInfo *info, *infoList = txInfoLoadAll(inInfo);
for (info = infoList; info != NULL; info = info->next)
    hashAdd(infoHash, info->name, info);

/* Load up sequences into hashes. */
struct hash *txSeqHash = faReadAllIntoHash(txFa, dnaLower);
struct hash *txPepHash = faReadAllIntoHash(txFaa, dnaUpper);
struct hash *refSeqHash = faReadAllIntoHash(refSeqFa, dnaLower);
struct hash *refPepHash = faReadAllIntoHash(refPepFa, dnaUpper);

/* Load up accession conversion hashes */
struct hash *refToPepHash = hashTwoColumnFile(refToPepFile);
struct hash *txToAccHash = hashTwoColumnFile(txToAccFile);

/* Make output files. */
FILE *fRepTx = mustOpen(outRepTxFa, "w");
FILE *fRepPep = mustOpen(outRepPepFa, "w");
FILE *fDirTx = mustOpen(outDirectTxFa, "w");
FILE *fDirPep = mustOpen(outDirectPepFa, "w");

/* Loop through bed list doing lookups and conversions. */
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    info = hashMustFindVal(infoHash, bed->name);
    char *acc = hashMustFindVal(txToAccHash, bed->name);
    bioSeq *repTxSeq, *repProtSeq = NULL;
    bioSeq *dirTxSeq, *dirProtSeq = NULL;
    if (info->isRefSeq)
        {
	char *refAcc = txAccFromTempName(bed->name);
	if (!startsWith("NM_", refAcc))
	    errAbort("Don't think I did find that refSeq acc, got %s", refAcc);

	/* Find sequence. */
	repTxSeq = hashMustFindVal(refSeqHash, refAcc);
	char *protAcc = hashMustFindVal(refToPepHash, refAcc);
	if (bed->thickStart != bed->thickEnd)
	    repProtSeq  = hashMustFindVal(refPepHash, protAcc);
	}
    else
        {
	/* Write out mRNA. */
	repTxSeq = hashMustFindVal(txSeqHash, bed->name);
	repProtSeq = hashFindVal(txPepHash, bed->name);
	}
    dirTxSeq = hashMustFindVal(txSeqHash, bed->name);
    dirProtSeq = hashFindVal(txPepHash, bed->name);
    if (repProtSeq != NULL)
	faWriteNext(fRepPep, acc, repProtSeq->dna, repProtSeq->size);
    if (dirProtSeq != NULL)
	faWriteNext(fDirPep, acc, dirProtSeq->dna, dirProtSeq->size);
    faWriteNext(fRepTx, acc, repTxSeq->dna, repTxSeq->size);
    faWriteNext(fDirTx, acc, dirTxSeq->dna, dirTxSeq->size);
    }

carefulClose(&fRepTx);
carefulClose(&fRepPep);
carefulClose(&fDirTx);
carefulClose(&fDirPep);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 13)
    usage();
dnaUtilOpen();
txGeneProtAndRna(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], 
		 argv[7], argv[8], argv[9], argv[10], argv[11], argv[12]);
return 0;
}
