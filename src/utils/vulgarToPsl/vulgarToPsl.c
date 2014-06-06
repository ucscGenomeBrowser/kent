/* vulgarToPsl - Convert the vulgar exonerate format to PSL. */

/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "psl.h"
#include "twoBit.h"
#include "linefile.h"
#include "dystring.h"
#include "dnaLoad.h"

void usage() 
{
errAbort("vulgarToPsl - Convert the vulgar exonerate format to PSL.\n"
	 "usage:\n"
	 "   vulgarToPsl input.vul proteinQ.fa dnaT.fa output.psl");
}

struct hash *seqHash(bioSeq *seqs)
/* Hash up all the sequences. */
{
bioSeq *seq;
struct hash *ret = NULL;
int numSeqs = slCount(seqs);
ret = newHash(logBase2(numSeqs)+1);
for (seq = seqs; seq != NULL; seq = seq->next)
    hashAdd(ret, seq->name, seq);
return ret;
}

struct psl *vulgarToPsl(struct lineFile *vulg, struct hash *queryHash, struct hash *targetHash)
/* Convert it. */
{
struct psl *list = NULL;
char *words[32768];
int numWords;
while (numWords = lineFileChop(vulg, words))
    {
    struct psl *aln;
    struct dyString *cDNA = newDyString(2048);
    struct dnaSeq *dna;
    aaSeq *prot;
    char *translated = NULL;
    boolean revComp = FALSE;
    int i, wordIx, dnaIx, blockIx, protIx;
    int startWord = (startsWith("vulgar", words[0])) ? 1 : 0;
    char strand;

    /* Do a check, then load the data, error on not finding it. */
    if (numWords == 32768)
	{
	warn("Line %d of %s actually had 32768 words on that line.  Skipping this one.\n", vulg->lineIx, vulg->fileName);
	continue;
	}
    dna = (struct dnaSeq *)hashMustFindVal(targetHash, words[startWord+4]);
    prot = (aaSeq *)hashMustFindVal(queryHash, words[startWord]);

    /* Everything's cool, so initialize data */
    AllocVar(aln);
    aln->match = 0;
    aln->misMatch = 0;
    aln->repMatch = 0;
    aln->nCount = 0;
    aln->qNumInsert = 0;
    aln->qBaseInsert = 0;
    aln->tNumInsert = 0;
    aln->tBaseInsert = 0;
    strand = words[startWord + 7][0];
    aln->strand[0] = '+';
    aln->strand[1] = (strand == '+') ? '+' : '-';
    aln->qName = cloneString(words[startWord]);
    aln->qStart = atoi(words[startWord + 1]);
    aln->qEnd = atoi(words[startWord + 2]);
    aln->qSize = prot->size;
    aln->tName = cloneString(words[startWord + 4]);
    dnaIx = atoi(words[startWord + 5]);
    if (strand == '-')
	revComp = TRUE;
    if (revComp)
	{
	aln->tStart = atoi(words[startWord + 6]);
	aln->tEnd = dnaIx;
	dnaIx--;
	}
    else
	{
	aln->tStart = dnaIx;
	aln->tEnd = atoi(words[startWord + 6]);
	}
    aln->tSize = dna->size;
    aln->blockCount = 0;

    /* First pass: count the number of blocks in the alignment and various things that only need one pass. */
    for (wordIx = startWord + 9; wordIx < numWords; wordIx += 3)
        {
	char type = words[wordIx][0];
	int first = atoi(words[wordIx+1]);
	int second = atoi(words[wordIx+2]);
	if (type == 'M')
	    aln->blockCount++;
	else if (type == 'G')
	    {
	    if (first > 0)
		{
		aln->qNumInsert++;
		aln->qBaseInsert += first;
		}
	    else 
		{
		aln->tNumInsert++;
		aln->qBaseInsert += second;		
		}
	    }
	else if ((type == 'I') || (type == '5') || (type == '3') || (type == 'F'))
	    {
	    if ((type == 'I') || (type == 'F'))
		aln->tNumInsert++;
	    aln->tBaseInsert += second;
	    }	
	}
    aln->blockSizes = needMem(sizeof(unsigned) * aln->blockCount);
    aln->qStarts = needMem(sizeof(unsigned) * aln->blockCount);
    aln->tStarts = needMem(sizeof(unsigned) * aln->blockCount);

    /* Second pass: go through the vulgar blocks and build a protein out of the cDNA. */
    blockIx = 0;
    protIx = 0;
    for (wordIx = startWord + 9; wordIx < numWords; wordIx += 3)
        {
	char type = words[wordIx][0];
	int first = atoi(words[wordIx+1]);
	int second = atoi(words[wordIx+2]);

	if (type == 'M')
	    {
	    aln->blockSizes[blockIx] = first;
	    aln->qStarts[blockIx] = protIx;
	    aln->tStarts[blockIx] = (revComp) ? dna->size - dnaIx - 1 : dnaIx;
	    blockIx++;
	    }	
	protIx += first;
	for (i = 0; i < second; i++)
	    {
	    char base = 0;
	    if (revComp) 
		{
		base = ntCompTable[dna->dna[dnaIx]];
		dnaIx--;
		}
	    else 
		{
		base = dna->dna[dnaIx];
		dnaIx++;
		}
	    if ((type == 'M') || (type == 'S'))
		dyStringAppendC(cDNA, base);
	    }
	}

    /* Translate the cDNA and count matches, etc. */
    translated = needMem(sizeof(char) * (aln->qSize + 1));
    dnaTranslateSome(cDNA->string, translated, aln->qSize+1);
    protIx = 0;
    for (i = aln->qStart; i < aln->qEnd; i++)
	{
	boolean repAA = FALSE;
	if (protIx >= strlen(translated))
	    break;
	/* count the repeats/N's at this amino acid (in the codon). */
	for (dnaIx = protIx*3; dnaIx < (protIx*3) + 3; dnaIx++)
	    if ((cDNA->string[dnaIx] == 'N') || (cDNA->string[dnaIx] == 'X'))
		aln->nCount++;
	    else if ((!repAA) && (islower(cDNA->string[dnaIx])))
		repAA = TRUE;
	if (translated[protIx++] == prot->dna[i])
	    {
	    if (repAA)
		aln->repMatch++;
	    else
		aln->match++;
	    }
	else
	    aln->misMatch++;
	}
    freeMem(translated);
    freeDyString(&cDNA);
    slAddHead(&list, aln);
    }
slReverse(&list);
return list;
}

int main(int argc, char *argv[])
/* The program */
{
struct psl *pslList = NULL, *psl;
struct hash *queryHash, *targetHash;
struct lineFile *vulg;
aaSeq *querySeqs;
struct dnaSeq *targetSeqs;
if (argc != 5)
    usage();
/* Load up everything at beginning */
vulg = lineFileOpen(argv[1], TRUE);
querySeqs = dnaLoadAll(argv[2]);
targetSeqs = dnaLoadAll(argv[3]);
queryHash = seqHash(querySeqs);
targetHash = seqHash(targetSeqs);
/* Main business */
pslList = vulgarToPsl(vulg, queryHash, targetHash);
pslWriteAll(pslList, argv[4], FALSE);
/* Free up everything */
freeDnaSeqList(&querySeqs);
freeDnaSeqList(&targetSeqs);
freeHash(&targetHash);
freeHash(&queryHash);
pslFreeList(&pslList);
lineFileClose(&vulg);
return 0;
}
