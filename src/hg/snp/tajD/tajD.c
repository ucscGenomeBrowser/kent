/* pbToAlleleCounts.c -- reformat Seattel SNP data from prettybase format to bed with alleles and counts */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include <math.h>
#include "common.h"
#include "errAbort.h"
#include "linefile.h"
#include "obscure.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "dnaseq.h"
#include "hdb.h"
#include "memalloc.h"
#include "hash.h"
#include "jksql.h"
#include "options.h"
#include "sqlNum.h"


boolean strictSnp=FALSE;
boolean strictBiallelic=FALSE;
boolean nConstant=FALSE;
boolean varOut=FALSE;
char *varOutFile;

static struct optionSpec optionSpecs[] =
/* command line option specifications */
{
    {"strictBiallelic", OPTION_BOOLEAN},
    {"strictSnp", OPTION_BOOLEAN},
    {"nConstant", OPTION_BOOLEAN},
    {"varOut", OPTION_STRING},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort("tajD: calculate Tajima's D on a set of regions for a set of variant data\n"
	 "Usage:   tajD variants.bed regions1.bed [regions2.bed [regions3.bed [...]]]\n"
	 "Options: -strictSnp          ignore loci with non-SNP mutations (not active)\n"
	 "         -strictBiallelic    ignore loci with more than two alleles (not active)\n"
	 "         -nConstant          sample size is the same for sites in a region (faster)\n"
	 "         -varOut=varOut.bed  save variants in a bed file (faster re-loading)\n"
	 );
}

struct allele
/* */
{
    struct allele *next;
    char  *seq;
    uint   na;
};

struct site
/*  */
{
    struct site *next;
    char   *chrom;
    uint    chromStart;
    uint    chromEnd;
    char   *name; /* unique site id */
    uint    score;
    char   *strand;
    uint    sampleSize;
    uint    alleleCount;
    boolean isSnp; /* default TRUE */
    boolean isIndel; /* default FALSE */
    boolean hasN; /* default FALSE */
    struct  allele *alleles;
};

struct region
{
    struct region *next;
    char   *chrom;
    uint    chromStart;
    uint    chromEnd;
    char   *name; /* unique region id */
    uint    score;
    char   *strand;
    uint    S; /* number of segregating sites */
    uint    genotypeCount;
    double  pi;
    double  theta;
    double  stDev;
    double  tajD;
    struct site *sites;
};

#define MAX_CONST 4096
double A1[MAX_CONST+1];
double E1[MAX_CONST+1];
double E2[MAX_CONST+1];

void initTDconst()
{
int i;
for(i=0; i<=MAX_CONST; i++)
    A1[i]=E1[i]=E2[i]=0.0;
}

void setTDconst(int n)
{
int i;
double a1=0.0, a2=0.0, b1, b2, c1, c2;

if (n>4096)
    errAbort("The maximum number of samples is currently set to %d; the sample size requested is %d.\n", MAX_CONST, n);
for(i=1; i<n; i++)
    {
    a1 += 1.0/i;
    a2 += 1.0/(i*i);
    }
b1 = (n + 1.0)/(3.0 * (n - 1.0));
b2 = 2 * ( (n*n) + n + 3.0 ) / ( 9.0 * n * (n - 1.0 ) );
c1 = b1 - (1.0 / a1);
c2 = b2 - (n + 2)/(a1 * n) + a2 / (a1*a1);
A1[n] = a1;
E1[n] = c1 / a1;
E2[n] = c2 / ( (a1*a1) + a2 );
return;
printf(" n: %d\n",n);
printf("a1: %f\n",a1);
printf("a2: %f\n",a2);
printf("b1: %f\n",b1);
printf("b2: %f\n",b2);
printf("c1: %f\n",c1);
printf("c2: %f\n",c2);
printf("e1: %f\n",E1[n]);
printf("e2: %f\n",E2[n]);
}

void printAllele(struct allele *allele)
{
printf("\t%s\t%d\n", allele->seq, allele->na);
}

void printSite(struct site *site)
{
struct allele *allele = NULL;

printf("%s\t%d\t%d\t%s\t%d\t%s\t%d\t%d\t%d\t%d\t%d\n",
       site->chrom, site->chromStart, site->chromEnd, 
       site->name, site->score, site->strand, 
       site->sampleSize, site->alleleCount,
       site->isSnp, site->isIndel, site->hasN);
for (allele=site->alleles;allele!=NULL;allele=allele->next)
    printAllele(allele);
}

void printSites(struct site *sites)
{
struct site *site = NULL;

for (site=sites;site!=NULL;site=site->next)
    printSite(site);
}

void printRegion(struct region *region)
{
struct site *site = NULL;

printf("------------------------------------------------------------------------\n"
       "%s\t%d\t%d\t%s\t%d\t%s\t%d\t%.3f\n",
       region->chrom, region->chromStart, region->chromEnd, 
       region->name, region->score, region->strand, region->S, region->tajD);
printf("chrom\tchrSt\tchrEnd\tname\tscore\tstrand\tsamSize\tallCnt\tisSnp\tisIndel\thasN\n");
printSites(region->sites);
}

void printRegions(struct region *regions)
{
struct region *region = NULL;

for (region=regions; region!=NULL; region=region->next)
    printRegion(region);
}

void printTajD(char *output, struct region *regions)
/* print Tajima's D for this region */
{
FILE          *f = mustOpen(output, "w");
struct region *r = NULL;

for (r=regions; r!=NULL; r=r->next)
    if (r->S >= 3)
	fprintf(f,"%s\t%d\t%d\t%s\t%d\t%.4f\t%.4f\t%.4f\t%.4f\n", 
		r->chrom, r->chromStart, r->chromEnd, r->name, r->S, r->tajD, r->pi, r->theta, r->stDev);
}

struct region *readRegions(char *regionFile)
/* reads regions from a bed file, 
 *    formatted by featureBits -bedRegionOut or similar, 
 *    regions are non-overlapping */
{
struct  region   *regions = NULL;
struct  lineFile *lf      = lineFileOpen(regionFile, TRUE);
char   *row[4]; /* featureBits -bedRegionOut */

while (lineFileRow(lf, row))
    {
    struct region *r = NULL;

    AllocVar(r);
    r->chrom      = cloneString(row[0]);
    r->chromStart = sqlUnsigned(row[1]);
    r->chromEnd   = sqlUnsigned(row[2]);
    r->name       = cloneString(row[3]);
    slAddHead(&regions, r);
    }
lineFileClose(&lf);
slReverse(&regions);
return regions;
}

void readVariants(char *variantFile, struct region *regions)
/* read variants from a bed file and store in a region */
{
struct lineFile *lf   = lineFileOpen(variantFile, TRUE);
struct region   *rPtr = NULL, *region = NULL;
struct site     *sPtr = NULL, *site   = NULL;
struct allele   *aPtr = NULL, *allele = NULL;
char            *row[8];
char  *chrom, *name, *strand, *seq, **seqs;
int    chromStart, chromEnd, score, *na, sizeOne, i;

while (lineFileRow(lf, row)) /* process one snp at a time */
    {
    // get site / allele info
    chrom      = row[0];
    chromStart = sqlUnsigned(row[1]);
    chromEnd   = sqlUnsigned(row[2]);
    name       = row[3];
    score      = sqlUnsigned(row[4]);
    strand     = row[5];
    touppers(row[6]);
    sqlStringStaticArray(row[6], &seqs, &sizeOne);
    assert(sizeOne == score);
    sqlSignedStaticArray(row[7], &na, &sizeOne);
    assert(sizeOne == score);

    // get the region it is in from the parent region list
    for (rPtr=regions; rPtr!=NULL; rPtr=rPtr->next)
	if (sameString(rPtr->chrom, chrom) && rPtr->chromStart<=chromStart && rPtr->chromEnd>=chromEnd)
	    break;
    if (rPtr==NULL)
	continue;

    // get the site it is in from the region's site list
    for (sPtr=rPtr->sites; sPtr!=NULL; sPtr=sPtr->next)
	if (sPtr->chromStart==chromStart && sPtr->chromEnd==chromEnd)
	    break;
    if (sPtr==NULL)	// add new site
	{
	struct site *s = NULL;
	
	AllocVar(s);
	s->isSnp      = TRUE;
	s->isIndel    = FALSE;
	s->hasN       = FALSE;
	s->chrom      = cloneString(chrom);
	s->chromStart = chromStart;
	s->chromEnd   = chromEnd;
	s->name       = cloneString(name);
	s->score      = score;
	s->strand     = cloneString(strand);
	slAddHead(&rPtr->sites, s);
	sPtr = s;
	rPtr->S++;
	}
    else 
	{
	if (differentString(sPtr->strand, strand))
	    verbose(4, "Warning: alleles reported at one site on both strands:\t%s\t%d\t%d\t%s\t%s\n", chrom, chromStart, chromEnd, sPtr->name, name);
	else if (differentString(sPtr->name, name))
	    verbose(4, "Warning: alleles reported at one site with different names:\t%s\t%d\t%d\t%s\t%s\n", chrom, chromStart, chromEnd, sPtr->name, name);
	}

    for(i=0;i<score;i++)
	{
	seq = seqs[i];
	// add allele to the site's allele list
	for (aPtr=sPtr->alleles; aPtr!=NULL; aPtr=aPtr->next)
	    if (sameString(aPtr->seq, seq))
		break;
	if (aPtr==NULL)	// add new allele
	    {
	    struct allele *a = NULL;
	    
	    AllocVar(a);
	    a->seq            = cloneString(seq);
	    a->na             = na[i];
	    slAddTail(&sPtr->alleles, a);
	    aPtr              = a;
	    sPtr->sampleSize += na[i];
	    sPtr->alleleCount++;
	    }
	else 
	    verbose(4, "Warning: same allele present twice (only the first is used):\t%s\t%d\t%d\t%s\t%s\t%s\n", chrom, chromStart, chromEnd, sPtr->name, name, seq);
	if ( differentString(seq, "A") && differentString(seq,"C") && differentString(seq, "G") && differentString(seq,"T") )
	    sPtr->isSnp = FALSE;
	if ( stringIn("-", seq) )
	    sPtr->isIndel = TRUE;
	if ( stringIn("N", seq) )
	    sPtr->hasN = TRUE;
	}
    rPtr->genotypeCount +=sPtr->sampleSize;
    }
lineFileClose(&lf);

// remove this section later, and add checks to the calculation steps for site counts (S), 
//     allele counts, and sample sizes.
for (region=regions; region!=NULL && region->sites!=NULL; region=region->next)
    {
    slReverse(region->sites);
    for (site=region->sites; site!=NULL; site=site->next)
	{
	for (allele=site->alleles; allele!=NULL; allele=allele->next)
	    if(allele->na==0)
		{
		site->alleleCount--;
		ZeroVar(allele->seq);
		}
	if(site->alleleCount<2)
	    {
	    region->S--;
	    region->genotypeCount -= site->sampleSize;
	    site->alleles=NULL; // memory leak
	    }
	}
    if (region->S < 3)
	region->sites=NULL; // memory leak
    }
}

struct site *getSiteList(char *variantFile)
/* read variants from a bed file into a list of sites */
{
struct lineFile *lf   = lineFileOpen(variantFile, TRUE);
struct site     *sPtr = NULL, *site   = NULL, *siteList = NULL ;
struct allele   *aPtr = NULL, *allele = NULL;
char            *row[8];
char  *chrom, *name, *strand, *seq, **seqs;
int    chromStart, chromEnd, score, *na, sizeOne, i;

while (lineFileRow(lf, row)) /* process one snp at a time */
    {
    struct site *s = NULL;
    // get site / allele info
    chrom      = row[0];
    chromStart = sqlUnsigned(row[1]);
    chromEnd   = sqlUnsigned(row[2]);
    name       = row[3];
    score      = sqlUnsigned(row[4]);
    strand     = row[5];
    touppers(row[6]);
    sqlStringStaticArray(row[6], &seqs, &sizeOne);
    assert(sizeOne == score);
    sqlSignedStaticArray(row[7], &na, &sizeOne);
    assert(sizeOne == score);

    // check to see if the site is already in the list
    // this is painfully slow for large datasets; split by chromosome
/*
    for (sPtr=siteList; sPtr!=NULL; sPtr=sPtr->next)
	if (sPtr->chromStart==chromStart && sPtr->chromEnd==chromEnd)
	    break;
    if (sPtr==NULL)	// add new site
	{
*/
//	struct site *s = NULL;
	
	AllocVar(s);
	s->isSnp      = TRUE;
	s->isIndel    = FALSE;
	s->hasN       = FALSE;
	s->chrom      = cloneString(chrom);
	s->chromStart = chromStart;
	s->chromEnd   = chromEnd;
	s->name       = cloneString(name);
	s->score      = score;
	s->strand     = cloneString(strand);
	slAddHead(&siteList, s);
	sPtr = s;
/*
	}
    else 
	{
	if (differentString(sPtr->strand, strand))
	    verbose(4, "Warning: alleles reported at one site on both strands:\t%s\t%d\t%d\t%s\t%s\n", chrom, chromStart, chromEnd, sPtr->name, name);
	else if (differentString(sPtr->name, name))
	    verbose(4, "Warning: alleles reported at one site with different names:\t%s\t%d\t%d\t%s\t%s\n", chrom, chromStart, chromEnd, sPtr->name, name);
	}
*/
    for(i=0;i<score;i++)
	{
	struct allele *a = NULL;

	seq = seqs[i];
	// add allele to the site's allele list
	for (aPtr=sPtr->alleles; aPtr!=NULL; aPtr=aPtr->next)
	    if (sameString(aPtr->seq, seq))
		break;
	if (aPtr==NULL)	// add new allele
	    {
	    struct allele *a = NULL;
	    
	    AllocVar(a);
	    a->seq            = cloneString(seq);
	    a->na             = na[i];
	    slAddTail(&sPtr->alleles, a);
	    aPtr              = a;
	    sPtr->sampleSize += na[i];
	    sPtr->alleleCount++;
	    }
	else 
	    verbose(4, "Warning: same allele present twice (only the first is used):\t%s\t%d\t%d\t%s\t%s\t%s\n", chrom, chromStart, chromEnd, sPtr->name, name, seq);
	if ( differentString(seq, "A") && differentString(seq,"C") && 
	     differentString(seq, "G") && differentString(seq,"T") )
	    sPtr->isSnp = FALSE;
	if ( stringIn("-", seq) )
	    sPtr->isIndel = TRUE;
	if ( stringIn("N", seq) )
	    sPtr->hasN = TRUE;
	}
    }
lineFileClose(&lf);
slReverse(&siteList);
for (site=siteList; site!=NULL; site=site->next)
    if(site->alleleCount<2)
	site->alleles=NULL; // memory leak
return siteList;
}

void cloneAddAlleleToList(struct allele *allele, struct site *site)
{
struct allele **alleleList = &site->alleles;
struct allele *newAllele = NULL;

if(allele==NULL)
    errAbort("Allele is NULL");
AllocVar(newAllele);
newAllele->seq = cloneString(allele->seq);
newAllele->na = allele->na;
slAddHead(alleleList, newAllele);
}

void cloneAddSiteToList(struct site *site, struct region *region)
{
struct site *newSite = NULL;
struct site **siteList = &region->sites;
struct allele *aPtr = NULL;

AllocVar(newSite);
newSite->chrom = cloneString(site->chrom);
newSite->chromStart = site->chromStart;
newSite->chromEnd = site->chromEnd;
newSite->name = cloneString(site->name);
newSite->score = site->score;
newSite->strand = cloneString(site->strand);
newSite->sampleSize = site->sampleSize;
newSite->alleleCount = site->alleleCount;
newSite->isSnp = site->isSnp;
newSite->isIndel = site->isIndel;
newSite->hasN = site->hasN;
slAddHead(siteList, newSite);
for(aPtr=site->alleles; aPtr!=NULL; aPtr=aPtr->next)
    cloneAddAlleleToList(aPtr, *siteList);
region->S++;
region->genotypeCount+=site->sampleSize;
}

void addSiteListToRegions(struct site *siteList, struct region *regions)
{
struct site *sPtr = NULL;
struct region *rPtr = NULL;

for(sPtr=siteList;sPtr!=NULL;sPtr=sPtr->next)
    for(rPtr=regions;rPtr!=NULL;rPtr=rPtr->next)
	if (sameString(rPtr->chrom,sPtr->chrom) && rPtr->chromStart<=sPtr->chromStart && rPtr->chromEnd>=sPtr->chromEnd)// && rPtr->sites!=NULL)
	    {
	    cloneAddSiteToList(sPtr,rPtr);
	    continue; // assumes that regions are nonoverlapping, so each site will be in a single region
	    }
}

double tajimasKhat(struct region *region) /* pi total for Tajima's D */
{
struct site *site = NULL;
struct allele *allele = NULL;
int ni;
double p, hom, kHat=0.0;

if (region->sites==NULL)
    return 0;
for (site=region->sites; site!=NULL; site=site->next)
    {
    ni  = site->sampleSize;
    hom = 0.0;
    for (allele=site->alleles; allele!=NULL; allele=allele->next)
	{
	p = 1.0*allele->na/ni;
	hom += p * p;
	}
    kHat += (1-hom)*(1.0*ni/(ni-1));
    }
return kHat;
}

double tajimasDsite(struct site *site, int s, double k)
{
int n = site->sampleSize;
double a1, e1, e2, d, stDev, Dtmp=0.0;
    
if (A1[n]==0.0)
    setTDconst(n);
a1    = A1[n];
e1    = E1[n];
e2    = E2[n];
d     = k - 1.0*s/a1;
stDev = sqrt( e1*s + e2*s*(s-1) );
return d/stDev;
}

void calculateTajD(struct region *region)
{
int s = region->S, n;
double a1, e1, e2, d=0.0, stDev=0.0;
double k = tajimasKhat(region), Dsum=0.0;
struct site *site = NULL;

if (optionExists("nConstant"))
    {
    n = region->genotypeCount/s; /* clean this up */
    if (A1[n]==0.0)
	setTDconst(n);
    a1    = A1[n];
    e1    = E1[n];
    e2    = E2[n];
    d     = k - s/a1;
    stDev = sqrt( e1*s + e2*s*(s-1) );
    region->pi = k;
    region->theta = d;
    region->stDev = stDev;
    region->tajD = d/stDev;
    return;
    }
for(site=region->sites; site!=NULL; site=site->next)
    Dsum += tajimasDsite(site, s, k)*site->sampleSize;
region->pi = k;
//region->theta = d;
//region->stDev = stDev;
region->tajD = Dsum/region->genotypeCount;
}

void writeVariants(struct region *regions)
{
FILE *f = mustOpen(varOutFile, "w");
struct region *region = NULL;
struct site   *site   = NULL;
struct allele *allele = NULL;
struct dyString *seqList = NULL;
struct dyString *nList   = NULL;

for (region=regions; region!=NULL; region=region->next)
    for (site=region->sites; site!=NULL; site=site->next)
	{
	seqList = newDyString(32);
	nList   = newDyString(32);
	for (allele=site->alleles; allele!=NULL; allele=allele->next)
	    {
	    dyStringPrintf(seqList, "%s,", allele->seq);
	    dyStringPrintf(nList,   "%d,", allele->na);
	    }
	fprintf(f,"%s\t%d\t%d\t%s\t%d\t%s\t%s\t%s\n", 
		site->chrom, site->chromStart, site->chromEnd, site->name, 
		site->score, site->strand, seqList->string, nList->string);
	freeDyString(&seqList);
	freeDyString(&nList);
	}
}

/*  fix memory leaks ??
void freeAllele(struct allele *allele){}
void freeAlleles(struct allele *alleles){}
void freeSite(struct site *site){}
void freeSites(struct site *sites){}
void freeRegion(struct region *region){}
void freeRegions(struct region *regions)
{
struct region *rPtr1 = NULL;
struct region *rPtr2 = NULL;

if (regions==NULL)
    return;
for (rPtr1=regions, rPtr2=regions->next; rPtr2!=NULL; rPtr1=rPtr2, rPtr2=rPtr2->next)
    freeRegion(rPtr1);
}
*/

int main(int argc, char *argv[])
/* error check, read regions, read variants, comput TajD, print output */
{
struct region *regions = NULL, *region = NULL;
struct site *sites = NULL;
char *variantFile = NULL, *regionFile = NULL;
int i;
char *inFile = NULL;
struct dyString *outFile = NULL;

optionInit(&argc, argv, optionSpecs);
if (argc < 3)
    usage();
variantFile = cloneString(argv[1]);
varOutFile = optionVal("varOut", NULL);
strictSnp = optionExists("strictSnp");
strictBiallelic = optionExists("strictBiallelic");
nConstant = optionExists("nConstant");
initTDconst();

if(argc > 3) // multiple files of regions
    {
    verbose(1, "- reading variants file '%s'.\n", variantFile); 
    sites = getSiteList(variantFile);
    verbose(1, "\t%d variants were read\n",slCount(sites));
    if (endsWith(variantFile, ".gz"))
	chopSuffix(variantFile);
    if (endsWith(variantFile, ".bed"))
	chopSuffix(variantFile);
    for (i=2; i<argc; i++)
	{
	inFile = argv[i];
	verbose(1, "- reading regions file '%s'.\n", inFile); 
	regions = readRegions(inFile);
	verbose(1, "\t%d regions were read\n",slCount(regions));
	addSiteListToRegions(sites, regions);
	verbose(4, "- calculating Tajima's D\n"); 
	for(region=regions; region!=NULL; region=region->next)
	    calculateTajD(region);
	verbose(4, "- writing Tajima's D\n"); 
	if (endsWith(inFile, ".gz"))
	    chopSuffix(inFile);
	if (endsWith(inFile, ".bed"))
	    chopSuffix(inFile);
	outFile = newDyString(32);
	dyStringPrintf(outFile, "%s_%s.tajd.bed", variantFile, inFile);
	printTajD(outFile->string, regions);
	freeDyString(&outFile);
//	freeRegions(regions);
	slFreeList(regions); // memory leak?
	}
    }
else
    {
    inFile = argv[2];
    verbose(1, "- reading regions file %s\n", inFile); 
    regions = readRegions(inFile);

    verbose(1, "- reading variants\n"); 
    readVariants(variantFile, regions);
    if(varOutFile!=NULL)
	{
	verbose(2, "- writing varOut\n");
	writeVariants(regions);
	}
    verbose(2, "- calculating Tajima's D\n"); 
    for(region=regions; region!=NULL; region=region->next)
	calculateTajD(region);
    
    verbose(2, "- writing Tajima's D\n"); 
    if (endsWith(inFile, ".gz"))
	chopSuffix(inFile);
    if (endsWith(inFile, ".bed"))
	chopSuffix(inFile);
    if (endsWith(variantFile, ".gz"))
	chopSuffix(variantFile);
    if (endsWith(variantFile, ".bed"))
	chopSuffix(variantFile);
    outFile = newDyString(32);
    dyStringPrintf(outFile, "%s_%s.tajd", variantFile, inFile);
    printTajD(outFile->string, regions);
    freeDyString(&outFile);
    }
return 0;
}
