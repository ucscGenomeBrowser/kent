/* findSnpClusters - Find weird SNP clusters and output beds spanning those regions.. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "snp.h"

static struct optionSpec options[] = {
    {"maxSpace", OPTION_INT},
    {"minSnps", OPTION_INT},
    {NULL, 0},
};

int maxSpace = 20;
int minSnps = 5;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "findSnpClusters - Find weird SNP clusters and output beds spanning those regions.\n"
  "usage:\n"
  "   findSnpClusters snps.txt output.bed\n"
  "options: \n"
  "   -maxSpace Number of maximum bases to span two SNPs residing in a cluster (default %d).\n"
  "   -minSnps  Minimum number of SNPs to call a cluster (default %d).\n", maxSpace, minSnps
  );
}

struct bed *getClusters(struct snp *snpList)
/* Simple cluster algorithm fixing a start point and extending each cluster */
/* by moving the end until finally it goes too far to be called a cluster. */
{
struct snp *start = snpList; 
struct snp *end; 
struct bed *retList = NULL;
int numClusters = 1;
while ((start != NULL) && ((end = start->next) != NULL))
    {
    struct snp *nextToEnd = start;
    int numSnps = 1;
    while ((end != NULL) && sameString(start->chrom, end->chrom) && (end->chromStart - nextToEnd->chromStart <= maxSpace))
	{
	nextToEnd = end;
	end = end->next;
	numSnps++;
	}
    if (numSnps >= minSnps)
	{
	char name[64];
	struct bed *bed;
	AllocVar(bed);
	bed->chrom = cloneString(start->chrom);
	bed->chromStart = start->chromStart;
	bed->chromEnd = end->chromEnd;
	safef(name, sizeof(name), "snpCluster.%d", numClusters);
	numClusters++;
	bed->name = cloneString(name);
	bed->score = numSnps;
	slAddHead(&retList, bed);
	}
    start = end;
    }
slReverse(&retList);
return retList;
}

void bedOutAll(struct bed *bedList, char *file)
/* Spit out the whole file. */
{
FILE *f = mustOpen(file, "w");
struct bed *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
    bedTabOutN(bed, 5, f);
carefulClose(&f);
}

void findSnpClusters(char *infile, char *outfile)
/* findSnpClusters - Find weird SNP clusters and output beds spanning those regions.. */
{
struct snp *snpList = snpLoadAll(infile);
struct bed *bedList; 
slSort(&snpList, bedCmp);
bedList = getClusters(snpList);
bedOutAll(bedList, outfile);
bedFreeList(&bedList);
snpFreeList(&snpList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
maxSpace = optionInt("maxSpace", maxSpace);
minSnps = optionInt("minSnps", minSnps);
findSnpClusters(argv[1], argv[2]);
return 0;
}
