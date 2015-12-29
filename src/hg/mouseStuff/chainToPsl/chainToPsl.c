/* chainToPsl - Convert chain to psl format. 
 * This file is copyright 2002 Robert Baertsch , but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "chainBlock.h"
#include "dnautil.h"
#include "verbose.h"
#include "dnaLoad.h"
#include "dnaseq.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainToPsl - Convert chain file to psl format\n"
  "usage:\n"
  "   chainToPsl in.chain tSizes qSizes target.lst query.lst out.psl\n"
  "Where tSizes and qSizes are tab-delimited files with\n"
  "       <seqName><size>\n"
  "columns.\n"
  "The target and query lists can either be fasta files, nib files, 2bit files\n"
  "or a list of fasta, 2bit and/or nib files one per line\n"
  );
}


//void aliStringToPsl(FILE *f, struct chain *chain, struct hash *tHash, struct hash *qHash, struct dlList *fileCache )
void aliStringToPsl(FILE *f, struct chain *chain, struct dnaSeq *tSeq, struct dnaSeq *qSeq)
/* Output alignment in a pair of strings with insert chars
 * to a psl line in a file. */
{
unsigned match = 0;	/* Number of bases that match */
unsigned misMatch = 0;	/* Number of bases that don't match */
unsigned repMatch = 0;	/* Number of bases that match but are part of repeats */
unsigned nCount = 0; /* Number of bases that are N in one or both sequences */
unsigned qNumInsert = 0;	/* Number of inserts in query */
int qBaseInsert = 0;	/* Number of bases inserted in query */
unsigned tNumInsert = 0;	/* Number of inserts in target */
int tBaseInsert = 0;	/* Number of bases inserted in target */
int maxBlocks = 10, blockIx = 0;
int i;
int *blocks = NULL, *qStarts = NULL, *tStarts = NULL;
struct cBlock *b = NULL, *nextB = NULL;
int qbSize = 0, tbSize = 0; /* sum of block sizes */

/* Don't ouput if either query or target is zero length */
if ((chain->qStart == chain->qEnd) || (chain->tStart == chain->tEnd))
    return;

/* Must undo reverse complement at the end of the function */
if (chain->qStrand == '-')
    reverseComplement(qSeq->dna, qSeq->size);

AllocArray(blocks, maxBlocks);
AllocArray(qStarts, maxBlocks);
AllocArray(tStarts, maxBlocks);

/* Figure block sizes and starts. */
for (b = chain->blockList; b != NULL; b = nextB)
    {
    if (blockIx >= maxBlocks)
        {
        ExpandArray(blocks, maxBlocks, maxBlocks*2);
        ExpandArray(qStarts, maxBlocks, maxBlocks*2);
        ExpandArray(tStarts, maxBlocks, maxBlocks*2);
        maxBlocks *= 2;
        }

    qbSize += b->qEnd - b->qStart + 1;
    tbSize += b->tEnd - b->tStart + 1;
	  qStarts[blockIx] = b->qStart;
	  tStarts[blockIx] = b->tStart;
	  blocks[blockIx] = b->tEnd - b->tStart;

    nextB = b->next;
    /* increment NumInsert and BaseInsert values */
    if (nextB != NULL)
        {
        if (b->qEnd != nextB->qStart)
            {
            qNumInsert++;
            qBaseInsert += nextB->qStart - b->qEnd;
            }
        if (b->tEnd != nextB->tStart)
            {
            tNumInsert++;
            tBaseInsert += nextB->tStart - b->tEnd;
            }
        }


            //printf("tStart %d b->tStart %d tEnd %d size %d block %d\n",tStart, b->tStart,  tEnd,tSeq->size, b->tEnd-b->tStart);
            //printf("qStart %d b->qStart %d qEnd %d size %d qend-qstart %d loop start %d loopend %d\n",qStart, b->qStart,  qEnd, qSeq->size, qEnd-qStart, (b->qStart)-qStart, b->qStart+(b->tEnd - b->tStart)-qStart);

    /* Count matches, etc. within this block */

    for (i = 0; i < b->tEnd - b->tStart; i++)
        {
        char q = qSeq->dna[b->qStart + i];
        char t = tSeq->dna[b->tStart + i];
        char qu = toupper(q);
        char tu = toupper(t);

        if (qu == 'N' || tu == 'N')
            nCount++;
        else if (isupper(t))
            {
            if (qu == tu)
                ++match;
            else
                ++misMatch;
            }
        else  /* islower(t) */
            {
            if (qu == tu)
                ++repMatch;
            else
                ++misMatch;
            }
        }

	  ++blockIx;
    }


/* Output header */
fprintf(f, "%d\t", match);
fprintf(f, "%d\t", misMatch);
fprintf(f, "%d\t", repMatch);
fprintf(f, "%d\t", nCount);
fprintf(f, "%d\t", qNumInsert);
fprintf(f, "%d\t", qBaseInsert);
fprintf(f, "%d\t", tNumInsert);
fprintf(f, "%d\t", tBaseInsert);
fprintf(f, "%c\t", chain->qStrand);
fprintf(f, "%s\t", chain->qName);
fprintf(f, "%d\t", chain->qSize);
if (chain->qStrand == '+')
    {
    fprintf(f, "%d\t", chain->qStart);
    fprintf(f, "%d\t", chain->qEnd);
    }
    else
    {
    fprintf(f, "%d\t", chain->qSize - chain->qEnd);
    fprintf(f, "%d\t", chain->qSize - chain->qStart);
    }
fprintf(f, "%s\t", chain->tName);
fprintf(f, "%d\t", chain->tSize);
fprintf(f, "%d\t", chain->tStart);
fprintf(f, "%d\t", chain->tEnd);
fprintf(f, "%d\t", blockIx);
if (ferror(f))
    {
    perror("Error writing psl file\n");
    errAbort("\n");
    }

/* Output block sizes */
for (i=0; i<blockIx; ++i)
    fprintf(f, "%d,", blocks[i]);
fprintf(f, "\t");

/* Output qStarts */
for (i=0; i<blockIx; ++i)
    fprintf(f, "%d,", qStarts[i]);
fprintf(f, "\t");

/* Output tStarts */
for (i=0; i<blockIx; ++i)
    fprintf(f, "%d,", tStarts[i]);
fprintf(f, "\n");

/* Clean Up. */
freez(&blocks);
freez(&qStarts);
freez(&tStarts);

if (chain->qStrand == '-')
    reverseComplement(qSeq->dna, qSeq->size);
}


void chainToPsl(char *inName, char *tSizeFile, char *qSizeFile,  char *targetList, char *queryList, char *outName)
/* chainToPsl - Convert chain file to psl format. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *outFile = mustOpen(outName, "w");
struct chain *chain;

verbose(1, "Scanning %s\n", targetList);
struct dnaSeq *tSeqList = dnaLoadAll(targetList);
struct hash *tHash = dnaSeqHash(tSeqList);

verbose(1, "Scanning %s\n", queryList);
struct dnaSeq *qSeqList = dnaLoadAll(queryList);
struct hash *qHash = dnaSeqHash(qSeqList);

verbose(1, "Converting %s\n", inName);
while ((chain = chainRead(lf)) != NULL)
    {
    struct dnaSeq *tSeq = hashMustFindVal(tHash, chain->tName);
    struct dnaSeq *qSeq = hashMustFindVal(qHash, chain->qName);
    aliStringToPsl(outFile, chain, tSeq, qSeq);
    chainFree(&chain);
    }
lineFileClose(&lf);
carefulClose(&outFile);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 7)
    usage();
chainToPsl(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
