/* cgapSageFind - Find all the locations in the genome with SAGE data including SNP variants. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "obscure.h"
#include "dystring.h"
#include "portable.h"
#include "linefile.h"
#include "hash.h"
#include "dnaLoad.h"
#include "bed.h"
#include "snp.h"
#include "cgapSage/cgapSage.h"
#include "cgapSage/cgapSageLib.h"
#include "cgapSageFind.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cgapSageFind - Find all the locations in the sequence with SAGE data including SNP variants\n"
  "usage:\n"
  "   cgapSageFind sequence frequencies.txt libraries.txt snps.txt output.bed\n"
  );
}

struct snp *narrowDownSnps(struct snp **pSnpList, char *chrom)
/* Get all the snps on a chrom and return it sorted. */
{
struct snp *newList = NULL;
struct snp *oldList = NULL;
struct snp *cur;
if (!pSnpList || !(*pSnpList))
    return NULL;
while ((cur = slPopHead(pSnpList)) != NULL)
    {
    if (sameString(cur->chrom, chrom))
	slAddHead(&newList, cur);
    else
	slAddHead(&oldList, cur);
    }
*pSnpList = oldList;
slSort(&newList, bedCmp);
return newList;
}

struct slRef *snpsInTag(struct snp **pChromSnps, int pos, int start)
/* Find the SNPs in the next 21 (TAG_SIZE) bases. */
/* chromSnps is sorted so quit search after we go beyond range. */
{
struct slRef *snpList = NULL;
struct snp *chromSnps, *cur;
if (!pChromSnps || !(*pChromSnps))
    return NULL;
/* Pop off SNPs until we're at the current position. */
cur = *pChromSnps;
while (((cur = *pChromSnps) != NULL) && (cur->chromStart < (pos + start)))
    {
    cur = slPopHead(pChromSnps);
    snpFree(&cur);
    }
cur = *pChromSnps;
while (cur && (cur->chromStart < (pos + TAG_SIZE + start)))
    {
    if ((cur->chromStart >= pos + start) && (cur->chromStart < (pos + TAG_SIZE + start)))
	{
	struct slRef *newRef;
	AllocVar(newRef);
	newRef->val = cur;
	slAddHead(&snpList, newRef);
	}
    cur = cur->next;
    }
slReverse(&snpList);
return snpList;
}

char otherStrand(char base)
/* Just return the base on the other strand. Super dumb. */
{
if (base == 'A')
    return 'T';
if (base == 'T')
    return 'A';
if (base == 'C')
    return 'G';
return 'C';
}

boolean checkSeqCATG(struct dnaSeq *chrom, int pos, int start, struct slRef *snpList)
/* This function is meant to speed things up by quickly checking if the */
/* current window begins or ends with a CATG or if there's SNPs in the first  */
/* or last four bases. */
{
struct slRef *snpRef;
if (startsWith("CATG", chrom->dna + pos))
    return TRUE;
if (((pos + TAG_SIZE) < chrom->size) && (startsWith("CATG", chrom->dna + pos + TAG_SIZE - 4)))
    return TRUE;
for (snpRef = snpList; snpRef != NULL; snpRef = snpRef->next)
    {
    struct snp *snp = snpRef->val;
    if ((snp->chromStart < pos + start + 4) || (snp->chromStart >= pos + start + TAG_SIZE - 4))
	return TRUE;
    }
return FALSE;
}

void possiblyOutputATag(struct dnaSeq *seq, int pos, int start, char strand, struct snp **snpsUsedArray, 
        int numSnps, struct hash *freqHash, struct hash *libTotHash, FILE *output)
/* If this sequence permutation appears in the frequency hash, output it. */
{
struct slPair *list;
list = hashFindVal(freqHash, seq->dna + 4); 
if (list)
    /* Output in cgapSage bed format. */
    {
    struct slPair *cur;
    int chromStart = pos + start;
    int chromEnd = chromStart + TAG_SIZE;
    int thickStart = (strand == '+') ? chromStart : chromEnd - 4;
    int thickEnd = thickStart + 4;
    int numLibs = slCount(list);
    int i;
    int snpsUsed = 0;
    /* chrom, chromStart, chromEnd  */
    fprintf(output, "%s\t%d\t%d\t", seq->name, chromStart, chromEnd);
    /* name */
    fprintf(output, "%s\t", seq->dna + 4);
    /* score, strand, thickStart, thickEnd */
    fprintf(output, "1000\t%c\t%d\t%d\t", strand, thickStart, thickEnd);
    /* numLibs */
    fprintf(output, "%d\t", numLibs);
    /* libIds */
    for (cur = list; cur != NULL; cur = cur->next)
	fprintf(output, "%s,", cur->name);
    fprintf(output, "\t");
    /* freqs */
    for (cur = list; cur != NULL; cur = cur->next)
	fprintf(output, "%d,", ptToInt(cur->val));
    fprintf(output, "\t");
    /* TPMs */
    for (cur = list; cur != NULL; cur = cur->next)
	{
	double totalTags = (double)hashIntVal(libTotHash, cur->name);
	int freq = ptToInt(cur->val);
	double tpm = (double)freq * (1000000 / totalTags);
	fprintf(output, "%.4f,", tpm);
	}
    fprintf(output, "\t");
    for (i = 0; i < numSnps; i++)
	if (snpsUsedArray[i] != NULL)
	    snpsUsed++;
    /* numSNps */
    fprintf(output, "%d\t", snpsUsed);
    /* snps */
    for (i = 0; i < numSnps; i++)
	if (snpsUsedArray[i] != NULL)
	    fprintf(output, "%s,", snpsUsedArray[i]->name);
    fprintf(output, "\n");
    }
}

void findTagsRecursingPosition(struct dnaSeq *seq, struct dnaSeq *refCopy, 
        int pos, int start, char strand, struct snp **snpArray,
        struct snp **snpsUsedArray, int numSnps, struct hash *freqHash, 
        struct hash *libTotHash, FILE *output, int ix, int snpIx)
/* Recursively permute all possible 21-mers containing SNPs. Takes into account */
/* that there can be more than one SNP per tag and even more than one SNP per */
/* base. */
{
/* Check to see if the first four bases are CATG */
/* If we're at a SNP, go through the versions of it. */
if ((ix < TAG_SIZE) &&
	 !((ix == 1) && (seq->dna[0] != 'C')) && 
	 !((ix == 2) && (seq->dna[1] != 'A')) && 
	 !((ix == 3) && (seq->dna[2] != 'T')) && 
	 !((ix == 4) && (seq->dna[3] != 'G')))	 
    {
    int chromIx = start + pos + (strand == '+' ? ix : (TAG_SIZE - ix));
    /* Deal with the possibility the current base is a SNP. */
    /* If the current SNP chromStart is < the base we're on, move */
    /* the snpIx. */
    while ((snpIx < numSnps) && (snpArray[snpIx]->chromStart < chromIx))
	snpIx++;
    /* If this base has one or more SNPs, do stuff. */
    if ((snpIx < numSnps) && (chromIx == snpArray[snpIx]->chromStart))
	{
	while ((snpIx < numSnps) && (chromIx == snpArray[snpIx]->chromStart))
	    {
	    int sizeClass = strlen(snpArray[snpIx]->class);
	    int i;
	    /* Loop through the SNP itself i.e. the strings "A/T", "C/G", etc. */
	    for (i = 0; i < sizeClass; i += 2)
		{
		char snpBase = snpArray[snpIx]->class[i];
		/* Stupid minus-strand SNPs.  How does that make sense?!?!? */
		if (((snpArray[snpIx]->strand[0] == '-') && (strand == '+')) || 
		    ((snpArray[snpIx]->strand[0] == '+') && (strand == '-')))
		    snpBase = otherStrand(snpBase);
		seq->dna[ix] = snpBase;		    
		/* Test the SNP base against the original base. */
		if ((snpBase != refCopy->dna[ix]) && (snpBase != '-'))
		    /* We're using this SNP, so reflect that in the snpsUsedArray. */
		    snpsUsedArray[snpIx] = snpArray[snpIx];
		else 
		    snpsUsedArray[snpIx] = NULL;
		findTagsRecursingPosition(seq, refCopy, pos, start, strand, snpArray, snpsUsedArray, numSnps,
					  freqHash, libTotHash, output, ix+1, snpIx);
		}
	    /* Check to see if the next SNP is on this base.  If so, clear */
	    /* the usedArray at this snpIx, so when we deal with that SNP */
	    /* on the next iteration of this while loop, there's no blatant */
	    /* disagreement. */ 
	    if ((snpIx+1 < numSnps) && (chromIx == snpArray[snpIx+1]->chromStart))
		snpsUsedArray[snpIx] = NULL;
	    snpIx++;
	    }
	}
    else 
	/* (if this base didn't have a SNP, just move on). */
	findTagsRecursingPosition(seq, refCopy, pos, start, strand, snpArray, snpsUsedArray, numSnps,
				  freqHash, libTotHash, output, ix+1, snpIx);
    }
else if (ix == TAG_SIZE)
    /* Finally, at the 21st level of recursion, output stuff. */
    {
    possiblyOutputATag(seq, pos, start, strand, snpsUsedArray, numSnps, freqHash, libTotHash, output);
    }
}

void findTagsAtPos(struct dnaSeq *chrom, int pos, int start,
			       struct snp **pChromSnps, struct hash *freqHash, 
			       struct hash *libTotHash, FILE *output)
/* Find the tags at the specific position. */
{
char *dna;
struct slRef *snpList = snpsInTag(pChromSnps, pos, start);
struct slRef *snpRef;
struct dnaSeq *seq;
struct dnaSeq *refCopy;
struct snp **snpArray = NULL;
struct snp **snpsUsedArray = NULL;
int i = 0;
int numSnps = slCount(snpList);
if (numSnps > 0)
    {
    AllocArray(snpArray, numSnps);
    AllocArray(snpsUsedArray, numSnps);
    }
//if ((pos > 4316050) && ((pos < 4316075)))
for (snpRef = snpList; snpRef != NULL; snpRef = snpRef->next)
    snpArray[i++] = (struct snp *)snpRef->val;
/* Start by making the most basic one. */
dna = cloneStringZ(chrom->dna + pos, TAG_SIZE);
seq = newDnaSeq(dna, TAG_SIZE, chrom->name);
/* Keep a copy so it's easy to tell which bases are introduced by SNPs. */
refCopy = cloneDnaSeq(seq);
findTagsRecursingPosition(seq, refCopy, pos, start, '+', snpArray, snpsUsedArray, numSnps, 
        freqHash, libTotHash, output, 0, 0);
/* Now do the minus strand. */
freeDnaSeq(&seq);
reverseComplement(refCopy->dna, refCopy->size);
seq = cloneDnaSeq(refCopy);
slReverse(&snpList);
i = 0;
for (snpRef = snpList; snpRef != NULL; snpRef = snpRef->next)
    {
    snpsUsedArray[i] = NULL;
    snpArray[i++] = (struct snp *)snpRef->val;
    }
findTagsRecursingPosition(seq, refCopy, pos, start, '-', snpArray, snpsUsedArray, numSnps, 
        freqHash, libTotHash, output, 0, 0);
freez(&snpArray);
freez(&snpsUsedArray);
freeDnaSeq(&seq);
freeDnaSeq(&refCopy);
}

int skipPosPastNs(struct dnaSeq *chrom, int pos)
/* If the next 21 bases have an 'N', skip to base after the 'N'. Then */
/* check again... and so on. Return the nearest base where the next */
/* 21 bases are clear of 'N's. */
{
int i = pos + TAG_SIZE;
int furthestN = pos;
if (pos >= chrom->size - TAG_SIZE)
    return pos;
while (i > pos)
    {
    if ((chrom->dna[i] == 'N') || (chrom->dna[i] == 'X'))
	{
	furthestN = i;
	break;
	}
    i--;
    }
if (furthestN > pos)
    {
    pos = skipPosPastNs(chrom, furthestN + 1);
    }
return pos;
}

void findTagsOnChrom(struct dnaSeq *chrom, int start, struct snp **pChromSnps, 
          struct hash *freqHash, struct hash *libTotHash, FILE *output)
/* Find the tags on just the given chromosome. */
{
int pos = 0;
while ((pos = skipPosPastNs(chrom, pos)) < chrom->size - TAG_SIZE)
    {
    findTagsAtPos(chrom, pos, start, pChromSnps, freqHash, libTotHash, output);
    pos++;
    }
}

void findTagLocations(struct dnaSeq *seqList, struct snp **pSnpList, 
          struct hash *freqHash, struct hash *libTotHash, FILE *output)
/* Find the tags on all given sequences. */
{
struct dnaSeq *seq;
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    int start = pickApartSeqName(&(seq->name));
    struct snp *chromSnps = narrowDownSnps(pSnpList, seq->name);
    findTagsOnChrom(seq, start, &chromSnps, freqHash, libTotHash, output);
    snpFreeList(&chromSnps);
    }
}

void cgapSageFind(char *genome, char *freqFile, char *libsFile, char *snpFile, 
		  char *outputBedFile)
/* cgapSageFind - Find all the locations in the genome with SAGE data */
/* including SNP variants. */
{
FILE *output = mustOpen(outputBedFile, "w");
struct hash *freqHash = getFreqHash(freqFile);
struct hash *libTotHash = getTotTagsHash(libsFile);
struct snp *snpList = snpLoadAllByTab(snpFile);
struct dnaSeq *seqList = dnaLoadAll(genome);
findTagLocations(seqList, &snpList, freqHash, libTotHash, output);
freeDnaSeqList(&seqList);
snpFreeList(&snpList);
hashFreeWithVals(&freqHash, hashElSlPairListFree);
freeHash(&libTotHash);
carefulClose(&output);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 6)
    usage();
cgapSageFind(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
