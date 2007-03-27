/* cgapSageFind - Find all the locations in the genome with SAGE data including SNP variants. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaLoad.h"
#include "bed.h"
#include "snp.h"
#include "cgapSage/cgapSage.h"

#define TAG_SIZE 21

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cgapSageFind - Find all the locations in the genome with SAGE data including SNP variants\n"
  "usage:\n"
  "   cgapSageFind sequence frequencies.txt snps.txt output.sage.bed\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *getFreqHash(char *freqFile)
/* Read the frequency file in, and store it in a hash and return that. */
{
struct hash *freqHash = newHash(21);
struct lineFile *lf = lineFileOpen(freqFile, TRUE);
char *words[3];
while (lineFileRowTab(lf, words))
    {
    char *tag = words[0];
    if (tag && startsWith("Tag", tag))
	continue;
    hashStoreName(freqHash, tag);
    }
lineFileClose(&lf);
return freqHash;
}

struct snp *narrowDownSnps(struct snp **pSnpList, char *chrom)
/* Get all the snps on a chrom and return it sorted. */
{
struct snp *newList = NULL;
struct snp *oldList = NULL;
struct snp *cur;
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

struct slRef *snpsInTag(struct snp *chromSnps, int pos)
/* Find the SNPs in the next 21 (TAG_SIZE) bases. */
/* chromSnps is sorted so quit search after we go beyond range. */
{
struct slRef *snpList = NULL;
struct snp *cur = chromSnps;
while (cur && (cur->chromStart < (pos + TAG_SIZE)))
    {
    if ((cur->chromStart >= pos) && (cur->chromStart < (pos + TAG_SIZE)))
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

struct dnaSeq *constructSnpVariants(struct dnaSeq *basicSeq, int pos, struct snp *snp)
/* Given a short sequence and a SNP, construct the variants. */
{
struct dnaSeq *seqVars = NULL;
int sizeClass = strlen(snp->class);
int i;
int snpPos = snp->chromStart - pos;
for (i = 0; i < sizeClass; i += 2)
    {
    char snpBase = snp->class[i];
    if (snp->strand[0] == '-')
	snpBase = otherStrand(snpBase);
    if (basicSeq->dna[snpPos] != snpBase)
	{
	struct dnaSeq *newVariant = cloneDnaSeq(basicSeq);
	char seqName[1024];
	newVariant->dna[snpPos] = snpBase;
	safef(seqName, sizeof(seqName), "%s.%s", newVariant->name, snp->name);
	freeMem(newVariant->name);
	newVariant->name = cloneString(seqName);
	slAddHead(&seqVars, newVariant);
	}
    }
return seqVars;
}

boolean checkSeqCATG(struct dnaSeq *chrom, int pos)
/* This function is meant to speed things up by quickly checking if the */
/* current window begins or ends with a CATG. */
{
if (startsWith("CATG", chrom->dna + pos))
    return TRUE;
if (((pos + TAG_SIZE) < chrom->size) && (startsWith("CATG", chrom->dna + pos + TAG_SIZE - 4)))
    return TRUE;
return FALSE;
}

struct dnaSeq *addSnpVariants(struct dnaSeq *seqList, int pos, struct snp *snp)
/* Recursively add SNP variants to a list of sequences given a SNP. */
{
struct dnaSeq *seqListHead = seqList;
struct dnaSeq *seqListRest = NULL;
struct dnaSeq *newVars;
if (!seqListHead)
    return NULL;
seqListRest = seqListHead->next;
seqListHead->next = constructSnpVariants(seqList, pos, snp);
seqListHead = slCat(seqListHead, addSnpVariants(seqListRest, pos, snp));
return seqListHead;
}

struct cgapSage *tagFromSeq(struct dnaSeq *seq, struct dnaSeq *chrom, int pos,
			    struct hash *freqHash)
/* Find out if the sequence is a tag in our frequencies hash. */
/* Otherwise, return NULL. */
{
struct cgapSage *newTag;
char strand = '+';
if (endsWith(seq->dna, "CATG"))
    {
    strand = '-';
    reverseComplement(seq->dna, seq->size);
    }
else if (!startsWith("CATG", seq->dna))
    return NULL;
if (hashLookup(freqHash, seq->dna + 4) != NULL)
    {
    char *snpName = chopPrefix(seq->name);
    AllocVar(newTag);
    newTag->chrom = cloneString(chrom->name);
    newTag->chromStart = pos;
    newTag->chromEnd = pos + TAG_SIZE;
    newTag->name = cloneString(seq->dna + 4);
    newTag->score = 1000;
    newTag->strand[0] = strand;
    newTag->thickStart =  (strand == '+') ? newTag->chromStart : newTag->chromEnd-4;
    newTag->thickEnd = newTag->thickStart + 4;
    if (!sameString(snpName, seq->name))
	{
	char *snps[128];
	int numSnps = chopByChar(snpName, '.', snps, sizeof(snps));
	int i;
	newTag->numSnps = numSnps;
	AllocArray(newTag->snps, numSnps);
	for (i = 0; i < numSnps; i++)
	    newTag->snps[i] = cloneString(snps[i]);
	}
    }
else 
    return NULL;
return newTag;
}

struct cgapSage *findTagsAtPos(struct dnaSeq *chrom, int pos, 
			       struct snp *chromSnps, struct hash *freqHash)
/* Find the tags at the specific position. */
{
struct slRef *snpList = snpsInTag(chromSnps, pos);
struct slRef *snpRef;
struct dnaSeq *snpVariants;
struct dnaSeq *seq;
struct cgapSage *list = NULL;
/* Do quick checks first. We need to know if the */
if (!snpList && !checkSeqCATG(chrom, pos))
    return NULL;
/* Start by making the most basic one. */
char *dna = cloneStringZ(chrom->dna + pos, TAG_SIZE);
snpVariants = newDnaSeq(dna, TAG_SIZE, dna);
/* Go through each SNP. */
for (snpRef = snpList; snpRef != NULL; snpRef = snpRef->next)
    {
    struct snp *snp = snpRef->val;
    addSnpVariants(snpVariants, pos, snpRef->val);
    }
/* Go through each SNP variant and see if it's a tag. */
for (seq = snpVariants; seq != NULL; seq = seq->next)
    {
    struct cgapSage *newTag = tagFromSeq(seq, chrom, pos, freqHash);
    if (newTag)
	slAddHead(&list, newTag);
    }
slReverse(&list);
return list;
}

int pickApartSeqName(char **pName)
/* Change /path/chr:start-end into chr and return start. */
{
char *name;
char *words[3];
int numWords = 0;
char *chrom, *range;
int skip = 0;
int start = 0;
if (!pName || ((name = *pName) == NULL))
    return 0;
numWords = chopByChar(name, ':', words, sizeof(words));
if (numWords == 3)
    skip = 1;
chrom = words[0 + skip];
*pName = chrom;
range = words[1 + skip];
chopPrefixAt(range, '-');
start = sqlUnsigned(range);
return start;
}

struct cgapSage *findTagsOnChrom(struct dnaSeq *chrom, struct snp *chromSnps, 
				 struct hash *freqHash)
/* Find the tags on just the given chromosome. */
{
struct cgapSage *cgapList = NULL;
int start = pickApartSeqName(&(chrom->name));
int pos;
for (pos = start; pos < (chrom->size - TAG_SIZE + start); pos++)
    {
    struct cgapSage *foundTags = findTagsAtPos(chrom, pos, chromSnps, freqHash);
    cgapList = slCat(cgapList, foundTags);
    }
return cgapList;
}

struct cgapSage *findTagLocations(struct dnaSeq *seqList, struct snp **pSnpList, 
				  struct hash *freqHash)
/* Find the tags on all given sequences. */
{
struct dnaSeq *seq;
struct cgapSage *theList = NULL;
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    struct snp *chromSnps = narrowDownSnps(pSnpList, seq->name);
    struct cgapSage *chromTags = findTagsOnChrom(seq, chromSnps, freqHash);
    theList = slCat(theList, chromTags);
    snpFreeList(&chromSnps);
    }
return theList;
}

void cgapSageTabOutAll(struct cgapSage *sageList, char *output)
/* Loop through the list and tabOut... how come autoSql doesn't make one of */
/* these for all of them? */
{
FILE *f = mustOpen(output, "w");
struct cgapSage *cur;
for (cur = sageList; cur != NULL; cur = cur->next)
    cgapSageTabOut(cur, f);
carefulClose(&f);
}

void cgapSageFind(char *genome, char *freqFile, char *snpFile, 
		  char *outputBedFile)
/* cgapSageFind - Find all the locations in the genome with SAGE data */
/* including SNP variants. */
{
struct hash *freqHash = getFreqHash(freqFile);
struct snp *snpList = snpLoadAllByTab(snpFile);
struct dnaSeq *seqList = dnaLoadAll(genome);
struct cgapSage *sageList = findTagLocations(seqList, &snpList, freqHash);
cgapSageTabOutAll(sageList, outputBedFile);
cgapSageFreeList(&sageList);
freeDnaSeqList(&seqList);
snpFreeList(&snpList);
freeHash(&freqHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
cgapSageFind(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
