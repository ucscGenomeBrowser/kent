/* vcfParseTest - Parse VCF header and data lines in given position range.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "vcf.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "vcfParseTest - Parse VCF header and data lines in given position range.\n"
  "usage:\n"
  "   vcfParseTest fileOrUrl.vcf.gz seqName start end\n"
  "\n"
  "fileOrUrl.vcf.gz needs to have been compressed by tabix, and index file\n"
  "fileOrUrl.vcf.gz.tbi must exist.\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void vcfParseTest(char *fileOrUrl, char *seqName, int start, int end)
/* vcfParseTest - Parse VCF header and data lines in given position range.. */
{
struct vcfFile *vcff = vcfTabixFileMayOpen(fileOrUrl, seqName, start, end, 100);
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
if (argc != 5)
    usage();
vcfParseTest(argv[1], argv[2], sqlUnsigned(argv[3]), sqlUnsigned(argv[4]));
return 0;
}
