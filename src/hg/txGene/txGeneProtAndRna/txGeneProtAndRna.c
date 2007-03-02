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
#include "cdsEvidence.h"
#include "bed.h"

static char const rcsid[] = "$Id: txGeneProtAndRna.c,v 1.1 2007/03/02 02:34:27 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneProtAndRna - Create fasta files with our proteins and transcripts. \n"
  "These echo RefSeq when gene is based on RefSeq. Otherwise they are taken from\n"
  "the genome.\n"
  "usage:\n"
  "   txGeneProtAndRna tx.bed tx.info tx.fa weeded.tce refSeq.fa refPep.fa txToAcc.tab outTx.fa outPep.fa \n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void txGeneProtAndRna(char *inBed, char *inInfo, char *txFa,  char *cdsFile,
	char *refSeqFa, char *refToPepFile, char *refPepFa,
	char *txToAccFile, char *outTxFa, char *outPepFa)
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

/* Load CDS into hash. */
struct hash *cdsHash = hashNew(18);
struct cdsEvidence *cds, *cdsList = cdsEvidenceLoadAll(cdsFile);
for (cds = cdsList; cds != NULL; cds = cds->next)
    hashAddUnique(cdsHash, cds->name, cds);

/* Load up sequences into hashes. */
struct hash *txSeqHash = faReadAllIntoHash(txFa, dnaLower);
struct hash *refSeqHash = faReadAllIntoHash(refSeqFa, dnaLower);
struct hash *refPepHash = faReadAllIntoHash(refPepFa, dnaUpper);

/* Load up accession conversion hashes */
struct hash *refToPepHash = hashTwoColumnFile(refToPepFile);
struct hash *txToAccHash = hashTwoColumnFile(txToAccFile);

/* Make output files. */
FILE *fTx = mustOpen(outTxFa, "w");
FILE *fPep = mustOpen(outPepFa, "w");

/* Loop through bed list doing lookups and conversions. */
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    info = hashMustFindVal(infoHash, bed->name);
    char *acc = hashMustFindVal(txToAccHash, bed->name);
    bioSeq *txSeq, *protSeq;
    if (info->isRefSeq)
        {
	/* Parse out refSeq accession. */
	char *refAcc = strrchr(bed->name, '.');
	if (refAcc == NULL)
	    errAbort("Can't find last period in %s", bed->name);
	refAcc += 1;
	if (!startsWith("NM_", refAcc))
	    errAbort("Don't think I did find that refSeq acc, got %s", refAcc);

	/* Find sequence. */
	txSeq = hashMustFindVal(refSeqHash, refAcc);
	char *protAcc = hashMustFindVal(refToPepHash, refAcc);
	protSeq  = hashMustFindVal(refPepHash, protAcc);
	}
    else
        {
	/* Write out mRNA. */
	txSeq = hashMustFindVal(txSeqHash, bed->name);
	cds = hashFindVal(cdsHash, bed->name);
	if (cds == NULL)
	    protSeq = NULL;
	else
	    {
	    AllocVar(protSeq);
	    protSeq->size = (cds->end - cds->start)/3;
	    protSeq->dna = needMem(protSeq->size+2);
	    dnaTranslateSome(txSeq->dna + cds->start, protSeq->dna, protSeq->size);
	    if (protSeq->dna[protSeq->size-1] == 0)
		protSeq->size -= 1;
	    }
	}
	
    if (protSeq != NULL)
	faWriteNext(fPep, acc, protSeq->dna, protSeq->size);
    faWriteNext(fTx, acc, txSeq->dna, txSeq->size);
    }

carefulClose(&fTx);
carefulClose(&fPep);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 11)
    usage();
dnaUtilOpen();
txGeneProtAndRna(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], 
	argv[7], argv[8], argv[9], argv[10]);
return 0;
}
