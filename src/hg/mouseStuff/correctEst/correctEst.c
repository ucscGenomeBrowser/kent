/* correctEst - Correct ESTs by passing them through genome. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "psl.h"
#include "fa.h"
#include "nib.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "correctEst - Correct ESTs by passing them through genome\n"
  "usage:\n"
  "   correctEst oldEst.fa ali.psl nibDir out.fa\n"
  "The corrected sequence will be in upper case\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct mrnaBlock
/*  A single block of an mRNA alignment. */
    {
    struct mrnaBlock *next;  /* Next in singly linked list. */
    int qStart; /* Start of block in query */
    int qEnd;   /* End of block in query */
    int tStart; /* Start of block in target */
    int tEnd;   /* End of block in target */
    };

struct mrnaBlock *mrnaBlockFromPsl(struct psl *psl)
/* Convert psl to mrna blocks - merging small gaps. 
 * Free result with slFreeList. */
{
struct mrnaBlock *mbList = NULL, *mb = NULL;
int i;
int maxGap = 5;
for (i=0; i<psl->blockCount; ++i)
    {
    int qStart = psl->qStarts[i];
    int tStart = psl->tStarts[i];
    int size = psl->blockSizes[i];
    if (mb == NULL || qStart - mb->qEnd > maxGap 
    	|| tStart - mb->tEnd > maxGap)
        {
	AllocVar(mb);
	slAddHead(&mbList, mb);
	mb->qStart = qStart;
	mb->tStart = tStart;
	}
    mb->qEnd = qStart + size;
    mb->tEnd = tStart + size;
    }
slReverse(&mbList);
return mbList;
}



struct hash *hashPsls(char *fileName)
/* Return hash of all psls in file. */
{
struct psl *pslList = pslLoadAll(fileName), *psl;
struct hash *hash = newHash(20);
for (psl = pslList; psl != NULL; psl = psl->next)
    hashAdd(hash, psl->qName, psl);
uglyf("Loaded %d psls from %s\n", slCount(pslList), fileName);
return hash;
}


struct dnaSeq *readCachedNib(struct hash *nibHash, char *nibDir,
	char *chrom, int start, int size)
/* Return sequence using cache of nibs. */
{
struct nibInfo *ni = hashFindVal(nibHash, chrom);
if (ni == NULL)
    {
    char fileName[512];
    sprintf(fileName, "%s/%s.nib", nibDir, chrom);
    AllocVar(ni);
    ni->fileName = cloneString(fileName);
    nibOpenVerify(fileName, &ni->f, &ni->size);
    }
return nibLdPart(ni->fileName, ni->f, ni->size, start, size);
}


void correctOne(struct dnaSeq *est, struct psl *psl, char *nibDir, 
   struct hash *nibHash, FILE *f)
/* Write one corrected EST to file. */
{
struct dnaSeq *geno = readCachedNib(nibHash, nibDir, psl->tName, 
	psl->tStart, psl->tEnd - psl->tStart);
struct dyString *t = newDyString(est->size+20);
int qSize = psl->qSize;
int tSize = psl->tSize;
int qLastEnd = 0;
int blockIx;
struct mrnaBlock *mbList, *mb;
int genoOffset = psl->tStart;
boolean isRc = FALSE;

/* Load sequence and alignment blocks, coping with reverse
 * strand as necessary. */
toUpperN(geno->dna, geno->size);	/* This helps debug... */
mbList = mrnaBlockFromPsl(psl);
if (psl->strand[0] == '-')
    {
    reverseComplement(geno->dna, geno->size);
    genoOffset = tSize - psl->tEnd;
    for (mb = mbList; mb != NULL; mb = mb->next)
         {
	 reverseIntRange(&mb->tStart, &mb->tEnd, tSize);
	 reverseIntRange(&mb->qStart, &mb->qEnd, qSize);
	 }
    slReverse(&mbList);
    isRc = TRUE;
    }

/* Make t have corrected sequence. */
for (mb = mbList; mb != NULL; mb = mb->next)
    {
    int qStart = mb->qStart;
    int qEnd = mb->qEnd;
    int uncovSize = qStart - qLastEnd;
    if (uncovSize > 0)
	dyStringAppendN(t, est->dna + qLastEnd, uncovSize);
    dyStringAppendN(t, geno->dna + mb->tStart - genoOffset, 
    	mb->tEnd - mb->tStart);
    qLastEnd = qEnd;
    }
if (qLastEnd != qSize)
    {
    int uncovSize = qSize - qLastEnd;
    dyStringAppendN(t, est->dna + qLastEnd, uncovSize);
    }

/* Output */
faWriteNext(f, est->name, t->string, t->stringSize);

/* Clean up time. */
slFreeList(&mbList);
freeDyString(&t);
freeDnaSeq(&geno);
}


void correctEst(char *oldFa, char *pslFile, char *nibDir, char *outFa)
/* correctEst - Correct ESTs by passing them through genome. */
{
struct hash *pslHash = hashPsls(pslFile);
struct lineFile *lf = lineFileOpen(oldFa, FALSE);
FILE *f = mustOpen(outFa, "w");
static struct dnaSeq est;
struct hashEl *hel;
struct psl *psl;
struct hash *nibHash = newHash(8);

while (faSpeedReadNext(lf, &est.dna, &est.size, &est.name))
    {
    if ((psl = hashFindVal(pslHash, est.name)) != NULL)
        {
	correctOne(&est, psl, nibDir, nibHash, f);
	}
    else
        {
	faWriteNext(f, est.name, est.dna, est.size);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
correctEst(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
