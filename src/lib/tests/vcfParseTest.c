/* vcfParseTest - Parse VCF header and data lines in given position range.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "vcf.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "vcfParseTest - Parse VCF header and data lines in given position range.\n"
  "usage:\n"
  "   vcfParseTest fileOrUrl.vcf.gz seqName start end\n"
  "options:\n"
  "   -headerOnly  print header summary only (version, def counts, sample IDs)\n"
  "                and skip the position-range parse.  seqName/start/end are ignored.\n"
  "   -genotypes   print the parsed allele index of each genotype, and the allele\n"
  "                counts tallied from them.\n"
  "\n"
  "fileOrUrl.vcf.gz needs to have been compressed by tabix, and index file\n"
  "fileOrUrl.vcf.gz.tbi must exist.\n"
  );
}

static struct optionSpec options[] = {
   {"headerOnly", OPTION_BOOLEAN},
   {"genotypes", OPTION_BOOLEAN},
   {NULL, 0},
};

static void vcfHeaderTest(char *fileOrUrl)
/* Open via tabix path and dump header summary -- regression-tests that the
 * VCF header is actually read through the tabix iterator code path. */
{
struct vcfFile *vcff = vcfTabixFileMayOpen(fileOrUrl, NULL, 0, 0, 100, -1);
if (vcff == NULL)
    errAbort("Failed to open \"%s\"", fileOrUrl);
printf("file: %s\n", fileOrUrl);
printf("version: %d.%d\n", vcff->majorVersion, vcff->minorVersion);
printf("infoDefs: %d\n", slCount(vcff->infoDefs));
printf("filterDefs: %d\n", slCount(vcff->filterDefs));
printf("gtFormatDefs: %d\n", slCount(vcff->gtFormatDefs));
printf("genotypeCount: %d\n", vcff->genotypeCount);
int i;
for (i = 0;  i < vcff->genotypeCount && i < 5;  i++)
    printf("genotypeId[%d]: %s\n", i, vcff->genotypeIds[i]);
vcfFileFree(&vcff);
}

static void vcfGenotypeTest(char *fileOrUrl, char *seqName, int start, int end)
/* Dump the allele index that each genotype parsed to, and the allele counts tallied from
 * those indexes.  An index that the record has no allele for should come out as missing
 * data (-1). */
{
struct vcfFile *vcff = vcfTabixFileMayOpen(fileOrUrl, seqName, start, end, 100, -1);
if (vcff == NULL)
    errAbort("Failed to parse \"%s\" and/or its index file \"%s.tbi\"", fileOrUrl, fileOrUrl);
struct vcfRecord *rec;
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    {
    vcfParseGenotypes(rec);
    printf("%s\t%d\t%s\talleleCount=%d\n", rec->chrom, rec->chromStart, rec->name,
           rec->alleleCount);
    int i;
    for (i = 0;  i < vcff->genotypeCount;  i++)
        {
        struct vcfGenotype *gt = &(rec->genotypes[i]);
        printf("  %s\thapIxA=%d\thapIxB=%d\tisHaploid=%d\tisPhased=%d\n",
               vcff->genotypeIds[i], gt->hapIxA, gt->hapIxB, gt->isHaploid, gt->isPhased);
        }
    int *gtCounts = NULL, *alCounts = NULL, phasedCount = 0, diploidCount = 0;
    vcfCountGenotypes(rec, &gtCounts, &alCounts, &phasedCount, &diploidCount);
    printf("  alleleCounts (last is missing data):");
    for (i = 0;  i <= rec->alleleCount;  i++)
        printf(" %d", alCounts[i]);
    printf("\n  phased=%d diploid=%d\n", phasedCount, diploidCount);
    freeMem(gtCounts);
    freeMem(alCounts);
    }
vcfFileFree(&vcff);
}

void vcfParseTest(char *fileOrUrl, char *seqName, int start, int end)
/* vcfParseTest - Parse VCF header and data lines in given position range.. */
{
struct vcfFile *vcff = vcfTabixFileMayOpen(fileOrUrl, seqName, start, end, 100, -1);
if (vcff == NULL)
    errAbort("Failed to parse \"%s\" and/or its index file \"%s.tbi\"", fileOrUrl, fileOrUrl);
int recCount = slCount(vcff->records);
printf("Finished parsing \"%s\" items in %s:%d-%d, got %d data rows\n",
       fileOrUrl, seqName, start+1, end, recCount);
if (recCount > 0)
    printf("First (up to) 100 rows in range:\n");
int i = 0;
struct vcfRecord *rec = vcff->records;
while (rec != NULL && i < 100)
    {
    printf("%s\t%d\t%d\t%s:%s/%s\t%s\n",
	   rec->chrom, rec->chromStart, rec->chromEnd,
	   rec->name, rec->alleles[0], rec->alleles[1], rec->qual);
    rec = rec->next;
    i++;
    }
vcfFileFree(&vcff);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (optionExists("headerOnly"))
    {
    if (argc != 2)
	usage();
    vcfHeaderTest(argv[1]);
    return 0;
    }
if (argc != 5)
    usage();
if (optionExists("genotypes"))
    vcfGenotypeTest(argv[1], argv[2], sqlUnsigned(argv[3]), sqlUnsigned(argv[4]));
else
    vcfParseTest(argv[1], argv[2], sqlUnsigned(argv[3]), sqlUnsigned(argv[4]));
return 0;
}
