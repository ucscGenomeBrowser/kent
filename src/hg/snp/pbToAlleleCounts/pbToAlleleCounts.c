/* pbToAlleleCounts.c -- reformat Seattel SNP data from prettybase format to bed with alleles and counts */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
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
#include "dystring.h"
#include "options.h"
#include "sqlNum.h"


static int ssnpId=0;
boolean strictSnp=FALSE;
boolean strictBiallelic=FALSE;

static struct optionSpec optionSpecs[] =
/* command line option specifications */
{
    {"strictBiallelic", OPTION_BOOLEAN},
    {"strictSnp", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort("pbToAlleleCounts -- reformat Seattle SNP data from prettybase format to bed with alleles and counts\n"
	 "Usage:   pbToAlleleCounts prettybaseFile strandFile bedFile\n"
	 "Options: strictSnp - ignore loci with non-SNP mutations\n"
	 "         strictBiallelic - ignore loci with more than two alleles \n");
}

struct alleleInfo
/* */
{
    struct alleleInfo *next;
    char  *allele;
    int    count;
};

struct locus
/*  */
{
    struct locus *next;
    char   *chrom;
    int     chromStart;
    int     chromEnd;
    char   *name; /* unique locus id */
    char   *strand;
    char   *hugoId;
    int     sampleSize;
    int     alleleCount;
    boolean strictSnp;
    struct  alleleInfo *alleles;
};

struct strand
{
    struct strand *next;
    char *name;
    char *strand;
};

void convertToUppercase(char *ptr)
/* convert and entire string to uppercase */
{
for( ; *ptr !='\0'; ptr++)
    *ptr=toupper(*ptr);
}

void printAlleles(FILE *f, struct alleleInfo *ai)
/* print alleles and counts for this locus */
{
struct alleleInfo *i = NULL;

for (i=ai; i!=NULL; i=i->next)
    fprintf(f,"\t%s\t%d", i->allele, i->count);
fprintf(f,"\n");
}

void printLocus(FILE *f, struct locus *l)
/* print positional information for this locus */
{
if (strictSnp&&!l->strictSnp)
    return;
if (strictBiallelic&&l->alleleCount!=2)
    return;
fprintf(f,"%s\t%d\t%d\t%s\t0\t%s\t%d\t%d", l->chrom, l->chromStart, l->chromEnd, l->name, l->strand, l->sampleSize, l->alleleCount);
printAlleles(f, l->alleles);
}

void printLoci(char *output, struct locus *head)
/* print all information for this locus */
{
FILE  *f        = mustOpen(output, "w");
struct locus *l = NULL;

for (l=head; l!=NULL; l=l->next)
    printLocus(f, l);
}

struct hash *readStrand(char *strandFile)
/* read the strands from a file */
{
struct hash *strandHash = newHash(16);
struct lineFile  *lf = lineFileOpen(strandFile, TRUE); /* input file */
char             *row[2]; /* number of fields in input file */

while (lineFileRow(lf, row)) /* process one snp at a time */
    {
    struct strand *strand;

    AllocVar(strand);
    strand->name   = cloneString(row[0]);
    strand->strand = cloneString(row[1]);
    hashAddSaveName(strandHash, strand->name, strand, &strand->name);
    }
return strandHash;
}

struct locus *readSs(char *pbFile, char *strandFile)
/* determine which allele matches assembly and store in details file */
{
struct hash *strandHash = readStrand(strandFile);
struct strand *strand = NULL;
struct hash *missingHugoIdHash = newHash(16);
struct hashCookie hashPtr;
char *missingName;
struct locus     *l  = NULL, *lPtr = NULL;
struct alleleInfo *aPtr = NULL;
struct lineFile  *lf = lineFileOpen(pbFile, TRUE); /* input file */
char             *row[4], *row2[3]; /* number of fields in input file */
char  *pbName;
char   chrom[32];
int    chromStart;
int    chromEnd;
char   name[32];
char  *allele;

while (lineFileRow(lf, row)) /* process one snp at a time */
    {
    struct alleleInfo *ai1 = NULL, *ai2 = NULL, *aiPtr;
    struct locus *m        = NULL;

    chopString(row[0], "-", row2, 3);
    chromStart = sqlUnsigned(row2[0]);
    chromEnd   = chromStart+1;
    safef(chrom, sizeof(chrom), "chr%s", row2[2]);

    if(l==NULL||l->chrom==NULL||l->chromStart!=chromStart||!(sameString(l->chrom,chrom)))
	{
	AllocVar(m);
	safef(name, sizeof(name), "%s_%d", row2[1], ++ssnpId);
	m->chrom       = cloneString(chrom);
	m->chromStart  = chromStart;
	m->chromEnd    = chromEnd;
	m->name        = cloneString(name);
	m->hugoId      = cloneString(row2[1]);
	m->strictSnp   = TRUE;
	slAddHead(&l, m);
	}

    allele=cloneString(row[2]);
    convertToUppercase(allele);
    if ( sameString(allele,"A") || sameString(allele,"C") || 
	 sameString(allele,"G") || sameString(allele,"T") )
	{
	for (aiPtr=l->alleles; aiPtr!=NULL; aiPtr=aiPtr->next)
	    if (sameString(aiPtr->allele, allele))
		break;
	if (aiPtr==NULL)
	    {
	    AllocVar(ai1);
	    ai1->allele=cloneString(allele);
	    slAddHead(&(l->alleles), ai1);
	    l->alleleCount++;
	    aiPtr=l->alleles;
	    }
	aiPtr->count++;
	l->sampleSize++;
	}

    allele=cloneString(row[3]);
    convertToUppercase(allele);
    if ( sameString(allele,"A") || sameString(allele,"C") || 
	 sameString(allele,"G") || sameString(allele,"T") )
	{
	for (aiPtr=l->alleles; aiPtr!=NULL; aiPtr=aiPtr->next)
	    if (sameString(aiPtr->allele, allele))
		break;
	if (aiPtr==NULL)
	    {
	    AllocVar(ai2);
	    ai2->allele=cloneString(allele);
	    slAddHead(&(l->alleles), ai2);
	    l->alleleCount++;
	    aiPtr=l->alleles;
	    }
	aiPtr->count++;
	l->sampleSize++;
	}
    }
slReverse(&l);
for(lPtr=l; lPtr!=NULL; lPtr=lPtr->next)
    {
    strand = hashFindVal(strandHash, lPtr->hugoId);
    if (strand == NULL)
	{
	hashStore(missingHugoIdHash, lPtr->hugoId);
	slRemoveEl(l, lPtr);
	continue;
	}
    lPtr->strand = cloneString(strand->strand);
    }
freeHash(&strandHash);
hashPtr = hashFirst(missingHugoIdHash);
while ( (missingName=hashNextName(&hashPtr)) != NULL )
    printf("HUGO ID was not found in strand.txt (usually from proteome.hgncXref): %s\n", missingName);
freeHash(&missingHugoIdHash);
return l;
}

int main(int argc, char *argv[])
/* error check, process command line input, and call getSnpDetails */
{
struct locus *l = NULL;

optionInit(&argc, argv, optionSpecs);
strictSnp = optionExists("strictSnp");
strictBiallelic = optionExists("strictBiallelic");
if (argc != 4)
    usage();
l = readSs(argv[1], argv[2]);
printLoci(argv[3], l);
return 0;
}
