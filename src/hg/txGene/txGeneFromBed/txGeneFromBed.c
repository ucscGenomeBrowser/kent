/* txGeneFromBed - Convert from bed to knownGenes format table (genePred + uniProt ID). */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"
#include "bed.h"
#include "cdsPick.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneFromBed - Convert from bed to knownGenes format table (genePred + uniProt ID)\n"
  "usage:\n"
  "   txGeneFromBed in.bed in.picks in.faa uniProt.fa refPep.fa out.kg\n"
  "where:\n"
  "   in.bed is input file in bed 12 format\n"
  "   in.picks is one of the outputs of txCdsPick\n"
  "   in.faa is the proteins associated with the known genes\n"
  "   uniProt.fa is the uniProt proteins\n"
  "   out.kg is known genes table in tab-separated format\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void outputKg(struct bed *bed, char *uniProtAcc, FILE *f)
/* Convert bed to genePred with a few extended fields and write. */
{
fprintf(f, "%s\t", bed->name);
fprintf(f, "%s\t", bed->chrom);
fprintf(f, "%s\t", bed->strand);
fprintf(f, "%d\t", bed->chromStart);
fprintf(f, "%d\t", bed->chromEnd);
fprintf(f, "%d\t", bed->thickStart);
fprintf(f, "%d\t", bed->thickEnd);
fprintf(f, "%d\t", bed->blockCount);
int i;
for (i=0; i<bed->blockCount; ++i)
    fprintf(f, "%d,", bed->chromStart + bed->chromStarts[i]);
fprintf(f, "\t");
for (i=0; i<bed->blockCount; ++i)
    fprintf(f, "%d,", bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i]);
fprintf(f, "\t");
fprintf(f, "%s\t", uniProtAcc);
fprintf(f, "%s\n", bed->name);
}

void txGeneFromBed(char *inBed, char *inPicks, char *ucscFa, char *uniProtFa, char *refPepFa, char *outKg)
/* txGeneFromBed - Convert from bed to knownGenes format table (genePred + uniProt ID). */
{
/* Load protein sequence into hashes */
struct hash *uniProtHash = faReadAllIntoHash(uniProtFa, dnaUpper);
struct hash *ucscProtHash = faReadAllIntoHash(ucscFa, dnaUpper);
struct hash *refProtHash =faReadAllIntoHash(refPepFa, dnaUpper);

/* Load picks into hash.  We don't use cdsPicksLoadAll because empty fields
 * cause that autoSql-generated routine problems. */
struct hash *pickHash = newHash(18);
struct cdsPick *pick;
struct lineFile *lf = lineFileOpen(inPicks, TRUE);
char *row[CDSPICK_NUM_COLS];
while (lineFileRowTab(lf, row))
    {
    pick = cdsPickLoad(row);
    hashAdd(pickHash, pick->name, pick);
    }

/* Load in bed */
struct bed *bed, *bedList = bedLoadNAll(inBed, 12);

/* Do reformatting and write output. */
FILE *f = mustOpen(outKg, "w");
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    char *protAcc = NULL;
    if (bed->thickStart < bed->thickEnd)
	{
        pick = hashMustFindVal(pickHash, bed->name);
	struct dnaSeq *spSeq = NULL, *uniSeq = NULL, *refPep = NULL, *ucscSeq;
	ucscSeq = hashMustFindVal(ucscProtHash, bed->name);
	if (pick->swissProt[0])
	    spSeq = hashMustFindVal(uniProtHash, pick->swissProt);
	if (pick->uniProt[0])
	    uniSeq = hashMustFindVal(uniProtHash, pick->uniProt);
	if (pick->refProt[0])
	    refPep = hashFindVal(refProtHash, pick->refProt);

	/* First we look for an exact match between the ucsc protein and
	 * something from swissProt/uniProt. */
	if (spSeq != NULL && sameString(ucscSeq->dna, spSeq->dna))
	    protAcc = pick->swissProt;
	if (protAcc == NULL && uniSeq != NULL && sameString(ucscSeq->dna, uniSeq->dna))
	    protAcc = pick->uniProt;
	if (protAcc == NULL && refPep != NULL && sameString(ucscSeq->dna, refPep->dna))
	    {
	    protAcc = cloneString(pick->refProt);
	    chopSuffix(protAcc);
	    }

	if (protAcc == NULL)
	    {
	    if (pick->uniProt[0])
	        protAcc = pick->uniProt;
	    else 
		{
	        protAcc = cloneString(pick->refProt);
		chopSuffix(protAcc);
		}
	    }
	}
    outputKg(bed, emptyForNull(protAcc), f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
txGeneFromBed(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
