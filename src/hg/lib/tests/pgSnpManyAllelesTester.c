/* pgSnpManyAllelesTester - check the allele counts pgSnp takes from a VCF record with many
 * alleles.
 *
 * pgSnpFromVcfRecord() turns the AN and AC INFO fields into one count per allele.  It used to
 * collect them in a fixed array of 80, and a repeat site can have hundreds of alleles: the
 * Japan ToMMo 61k VCF on hg38 crashed hgTracks this way.  This test reads a VCF whose middle
 * record has 150 ALT alleles, each with its own AC value, and prints the allele count and the
 * per-allele counts pgSnp builds for every record.  refs #38154 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "vcf.h"
#include "pgSnp.h"

static char *vcfFileName = "input/pgSnpManyAlleles/test.vcf";

static void checkCounts(struct vcfRecord *rec, struct pgSnp *pgs)
/* For a record whose AC values are all present, the counts must be AN minus the sum of AC for
 * the reference allele, then each AC value in order. */
{
struct vcfInfoElement *an = (struct vcfInfoElement *)vcfRecordFindInfo(rec, "AN");
struct vcfInfoElement *ac = (struct vcfInfoElement *)vcfRecordFindInfo(rec, "AC");
int i, refCount = an->values[0].datInt;
for (i = 0; i < ac->count; i++)
    {
    if (ac->missingData[i])
        {
        printf("    has missing AC values, not compared\n");
        return;
        }
    refCount -= ac->values[i].datInt;
    }
struct dyString *dy = dyStringNew(0);
dyStringPrintf(dy, "%d", refCount);
for (i = 0; i < ac->count; i++)
    dyStringPrintf(dy, ",%d", ac->values[i].datInt);
printf("    counts %s the AN and AC fields\n",
       sameString(dy->string, pgs->alleleFreq) ? "match" : "DO NOT MATCH");
dyStringFree(&dy);
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct vcfFile *vcff = vcfFileMayOpen(vcfFileName, NULL, 0, 0, 0, -1, TRUE);
if (vcff == NULL)
    errAbort("could not open %s", vcfFileName);
struct vcfRecord *rec;
for (rec = vcff->records; rec != NULL; rec = rec->next)
    {
    struct pgSnp *pgs = pgSnpFromVcfRecord(rec);
    printf("%s: %d alleles\n", rec->name, pgs->alleleCount);
    printf("    alleleFreq %s\n", pgs->alleleFreq);
    checkCounts(rec, pgs);
    }
vcfFileFree(&vcff);
return 0;
}
