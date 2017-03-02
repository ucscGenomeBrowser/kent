/* cdwVcfStats - Make a pass through vcf file gatherings some stats.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dnautil.h"
#include "options.h"
#include "sqlNum.h"
#include "bigBed.h"
#include "genomeRangeTree.h"
#include "vcf.h"
#include "hmmstats.h"

char *clBed = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwVcfStats - Make a pass through vcf file gatherings some stats.\n"
  "usage:\n"
  "   cdwVcfStats in.vcf out.ra\n"
  "options:\n"
  "   -bed=out.bed - make simple bed3 here\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"bed", OPTION_STRING},
   {NULL, 0},
};

boolean isSorted = TRUE;

void genomeRangeTreeWriteAsBed3(struct genomeRangeTree *grt, char *fileName)
/* Write as bed3 file */
{
FILE *f = mustOpen(fileName, "w");
struct hashEl *chrom, *chromList = hashElListHash(grt->hash);
slSort(&chromList, hashElCmpWithEmbeddedNumbers);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    char *chromName = chrom->name;
    struct rbTree *rangeTree = chrom->val;
    struct range *range, *rangeList = rangeTreeList(rangeTree);
    for (range = rangeList; range != NULL; range = range->next)
	fprintf(f, "%s\t%d\t%d\n", chromName, range->start, range->end);
    }
carefulClose(&f);
}

boolean vcfIsSnp(struct vcfRecord *rec)
/* Return TRUE if rec is SNP in a narrow sense - just substitution */
{
int i;
for (i=0; i<rec->alleleCount; ++i)
    {
    char *allele = rec->alleles[i];
    int len = strlen(allele);
    if (len != 1)
        return FALSE;
    unsigned char base = allele[0];
    if (base != '.' && ntVal[base] < 0)
        return FALSE;
    }
return TRUE;
}

struct genomeRangeTree *genomeRangeTreeFromBigBed(char *fileName)
/* Make up a genomeRangeTree from a bed file looking at just first three columns. */
{
struct genomeRangeTree *grt = genomeRangeTreeNew();
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct lm *lm = lmInit(0);
    char *chromName = chrom->name;
    struct bigBedInterval *list = bigBedIntervalQuery(bbi,chromName,0,chrom->size,0,lm);
    struct bigBedInterval *el;
    for (el = list; el != NULL; el = el->next)
	genomeRangeTreeAdd(grt, chromName, el->start, el->end);
    lmCleanup(&lm);
    }
bbiFileClose(&bbi);
return grt;
}

long long chromCoverage(struct genomeRangeTree *grt, char *chrom)
/* Return number of bases on chromosome with coverage in tree. */
{
struct rbTree *rangeTree = genomeRangeTreeFindRangeTree(grt, chrom);
if (rangeTree == NULL)
    return 0;
return rangeTreeSumRanges(rangeTree);
}

void cdwVcfStats(char *inVcf, char *outRa)
/* cdwVcfStats - Make a pass through vcf file gatherings some stats.. */
{
int batchSize = 8*1024;
int inBatchIx = 0;

struct vcfFile *vcf = vcfFileMayOpen(inVcf, NULL, 0, 0, 0, 0, FALSE);
if (vcf == NULL)
    errAbort("Couldn't open %s as a VCF file", inVcf);

FILE *f = mustOpen(outRa, "w");
fprintf(f, "vcfMajorVersion %d\n", vcf->majorVersion);
fprintf(f, "vcfMinorVersion %d\n", vcf->minorVersion);
fprintf(f, "genotypeCount %d\n", vcf->genotypeCount);

struct genomeRangeTree *grt = genomeRangeTreeNew();
long long sumOfSizes = 0;
long long itemCount = 0;
int countDp=0;
double minDp = 0, maxDp = 0, sumDp = 0, sumSquareDp = 0;
long long countPass=0;
long long haveFilterCount=0;
long long snpCount = 0;

struct vcfRecord *rec;
vcfFileMakeReusePool(vcf, 64*1024);
while ((rec = vcfNextRecord(vcf)) != NULL)
    {
    char chrom[128];
    if (startsWith("chr", rec->chrom))
	safef(chrom, sizeof(chrom), "%s", rec->chrom);
    else
	safef(chrom, sizeof(chrom), "chr%s", rec->chrom);
    genomeRangeTreeAdd(grt, chrom, rec->chromStart, rec->chromEnd);
    long long size = rec->chromEnd - rec->chromStart;
    sumOfSizes += size;
    itemCount += 1;
    boolean isSnp = vcfIsSnp(rec);
    if (isSnp)
	{
        snpCount += 1;
	}
    if (rec->filterCount > 0)
        {
	haveFilterCount += 1;
	if (sameString(rec->filters[0], "PASS"))
	    {
	    ++countPass;
	    }
	}

    int infoIx;
    for (infoIx=0; infoIx<rec->infoCount; ++infoIx)
	{
	struct vcfInfoElement *info = &rec->infoElements[infoIx];
	if (sameString(info->key, "DP"))
	    {
	    int i;
	    for (i=0; i<info->count; ++i)
		{
		union vcfDatum *uv = &info->values[i];
		double dp = uv->datInt;
		if (countDp == 0)
		    {
		    minDp = maxDp = sumDp = dp;
		    sumSquareDp = dp*dp;
		    countDp = 1;
		    }
		else
		    {
		    if (minDp > dp)
			minDp = dp;
		    if (maxDp < dp)
			maxDp = dp;
		    sumDp += dp;
		    sumSquareDp += dp*dp;
		    countDp += 1;
		    }
		}
	    }
	}

    /* Every now and then free up some memory */
    if (++inBatchIx >= batchSize)
	{
        vcfFileFlushRecords(vcf);
	inBatchIx = 0;
	}
    }

if (clBed != NULL)
    genomeRangeTreeWriteAsBed3(grt, clBed);

long long basesCovered = genomeRangeTreeSumRanges(grt);
fprintf(f, "itemCount %lld\n", itemCount);

fprintf(f, "chromsHit %d\n", grt->hash->elCount);
fprintf(f, "passItemCount %lld\n", countPass);
if (itemCount > 0)
    fprintf(f, "passRatio %g\n", (double)countPass/itemCount);
fprintf(f, "snpItemCount %lld\n", snpCount);
if (itemCount > 0)
    fprintf(f, "snpRatio %g\n", (double)snpCount/itemCount);

fprintf(f, "sumOfSizes %lld\n", sumOfSizes);
fprintf(f, "basesCovered %lld\n", basesCovered);

fprintf(f, "xBasesCovered %lld\n", chromCoverage(grt, "chrX"));
fprintf(f, "yBasesCovered %lld\n", chromCoverage(grt, "chrY"));
fprintf(f, "mBasesCovered %lld\n", chromCoverage(grt, "chrM"));

fprintf(f, "gotDepth %d\n", (int)(countDp > 0));
if (countDp > 0)
    {
    fprintf(f, "depthMean %g\n", sumDp/countDp);
    fprintf(f, "depthMin %g\n", minDp);
    fprintf(f, "depthMax %g\n", maxDp);
    fprintf(f, "depthStd %g\n", calcStdFromSums(sumDp, sumSquareDp, countDp));
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
dnaUtilOpen();
clBed = optionVal("bed", clBed);
if (argc != 3)
    usage();
cdwVcfStats(argv[1], argv[2]);
return 0;
}
