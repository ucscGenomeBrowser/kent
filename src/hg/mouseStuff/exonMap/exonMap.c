/* exonMap - map exons using two psls. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "exonMap - map exons using two psls\n"
  "usage:\n"
  "   exonMap reference.psl target.psl out.psl\n"
  "options:\n"
  "   -exons     output individual exons\n"
  );
}

static struct optionSpec options[] = {
   {"exons", OPTION_BOOLEAN},
   {NULL, 0},
   };

typedef void (*funcPtr) ( struct psl *psl, void *data);

void outPsl( struct psl *psl, void *data)
{
    pslTabOut(psl, (FILE *)data);
}

void addPsl( struct psl *newPsl, void *data)
{
    struct psl **pslPtr = (struct psl **)data;
    struct psl *psl = *pslPtr;
    int qEnd, tEnd;

    if (psl == NULL)
	{
	*pslPtr = newPsl;
	return;
	}

    psl->match += newPsl->match;
    psl->misMatch += newPsl->misMatch;
    psl->repMatch += newPsl->repMatch;
    psl->nCount += newPsl->nCount;
    psl->qNumInsert += newPsl->qNumInsert;
    psl->tNumInsert += newPsl->tNumInsert;
    psl->qBaseInsert += newPsl->qBaseInsert;
    psl->tBaseInsert += newPsl->tBaseInsert;
    assert(psl->qSize == newPsl->qSize);
    assert(psl->tSize == newPsl->tSize);
    assert(sameString(psl->qName, newPsl->qName));
    assert(sameString(psl->tName, newPsl->tName));
    psl->qStarts = realloc(psl->qStarts, sizeof(int) * (psl->blockCount + newPsl->blockCount)); 
    psl->tStarts = realloc(psl->tStarts, sizeof(int) * (psl->blockCount + newPsl->blockCount)); 
    psl->blockSizes = realloc(psl->blockSizes, sizeof(int) * (psl->blockCount + newPsl->blockCount)); 
    memcpy(&psl->qStarts[psl->blockCount], newPsl->qStarts, sizeof(int)*newPsl->blockCount);
    memcpy(&psl->tStarts[psl->blockCount], newPsl->tStarts, sizeof(int)*newPsl->blockCount);
    memcpy(&psl->blockSizes[psl->blockCount], newPsl->blockSizes, sizeof(int)*newPsl->blockCount);
    psl->blockCount += newPsl->blockCount;
    if(psl->strand[1] == '-')
	{
	tEnd = psl->tStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
	psl->tEnd = psl->tSize - psl->tStart;
	psl->tStart = psl->tSize - tEnd;
	}
    else
	{
	psl->tStart = psl->tStarts[0];
	psl->tEnd = psl->tStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
	}

    if(psl->strand[0] == '-')
	{
	qEnd = psl->qStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
	psl->qEnd = psl->qSize - psl->qStart;
	psl->qStart = psl->qSize - qEnd;
	}
    else
	{
	psl->qStart = psl->qStarts[0];
	psl->qEnd = psl->qStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
	}
}


void mapBlocks(struct psl *qPsl, struct psl *tPsl, funcPtr func, void *funcData)
{
    struct psl *psl = NULL;
    int ii;
    char buffer[512];
    //int tStarts[512],qStarts[512], blockSizes[512];
    int tBlock = 0;
    int qStart, qEnd, tStart, tEnd;
    boolean out = FALSE;
    int startBlock;
    int blocksAlloced = 10;

    for(ii=0; ii < qPsl->blockCount; ii++)
	{
	out = FALSE;
	AllocVar(psl);
	psl->tStarts = malloc(sizeof(int) * blocksAlloced); 
	psl->qStarts = malloc(sizeof(int) * blocksAlloced); 
	psl->blockSizes = malloc(sizeof(int) * blocksAlloced); 

	qStart = qPsl->qStarts[ii];
	qEnd = qStart + qPsl->blockSizes[ii];
	psl->strand[0] = qPsl->strand[1];
	psl->strand[1] = tPsl->strand[1];
	psl->qSize = qPsl->tSize;
	psl->qName = qPsl->tName;
	psl->tName = tPsl->tName;
	psl->tSize = tPsl->tSize;

	if((tPsl->qStarts[tBlock] >= qStart) && (tPsl->qStarts[tBlock] < qEnd) && (tBlock < tPsl->blockCount))
	    {
	    startBlock = tBlock;
	    while((tPsl->qStarts[tBlock] >= qStart) && (tPsl->qStarts[tBlock] < qEnd)&& (tBlock < tPsl->blockCount))
		{
		psl->tStarts[psl->blockCount] = tPsl->tStarts[tBlock];
		psl->qStarts[psl->blockCount] = qPsl->tStarts[ii] ;
		if (startBlock != tBlock)
		    psl->qStarts[psl->blockCount] +=  3*(tPsl->qStarts[tBlock] - tPsl->qStarts[startBlock]);
		psl->blockSizes[psl->blockCount] = 3*tPsl->blockSizes[tBlock];
		psl->match += psl->blockSizes[psl->blockCount];
		psl->blockCount++;
		tBlock++;
		if (tBlock == blocksAlloced)
		    {
		    blocksAlloced += 25;
		    psl->tStarts = realloc(psl->tStarts,sizeof(int) * blocksAlloced); 
		    psl->qStarts = realloc(psl->qStarts,sizeof(int) * blocksAlloced); 
		    psl->blockSizes = realloc(psl->blockSizes,sizeof(int) * blocksAlloced); 
		    }
		}

	    psl->tEnd =  psl->tStarts[psl->blockCount-1] + psl->blockSizes[psl->blockCount-1];
	    psl->qEnd =  psl->qStarts[psl->blockCount-1] + psl->blockSizes[psl->blockCount-1];
	    psl->qStart = psl->qStarts[0];
	    psl->tStart = psl->tStarts[0];

	    if(psl->strand[1] == '-')
		{
		tEnd = psl->tStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
		psl->tEnd = psl->tSize - psl->tStart;
		psl->tStart = psl->tSize - tEnd;
		}
	    else
		{
		psl->tStart = psl->tStarts[0];
		psl->tEnd = psl->tStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
		}

	    if(psl->strand[0] == '-')
		{
		qEnd = psl->qStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
		psl->qEnd = psl->qSize - psl->qStart;
		psl->qStart = psl->qSize - qEnd;
		}
	    else
		{
		psl->qStart = psl->qStarts[0];
		psl->qEnd = psl->qStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
		}

	    (*func)(psl, funcData);
	    }
	}
}

void exonMap(char *query, char *target, char *output)
/* exonMap - map exons using two psls. */
{
struct lineFile *qlf = pslFileOpen(query);
struct lineFile *tlf = pslFileOpen(target);
struct psl *psl, *pslList, *newPslList, *pslRef;
struct hash *pslHash = newHash(0);  
FILE *outF = mustOpen(output, "w");

while ((psl = pslNext(qlf)) != NULL)
    {
    pslList = hashFindVal(pslHash, psl->qName);
    if (pslList == NULL)
	hashAdd(pslHash, psl->qName, psl);
    else
	{
	psl->next = pslList->next;
	pslList->next = psl;
	}
    }

while ((psl = pslNext(tlf)) != NULL)
    {
    struct psl *newPsl = NULL;

    pslList = hashFindVal(pslHash,psl->qName);
//    if (pslList == NULL)
//	errAbort("can't find %s in query file",psl->qName);

    for(pslRef = pslList; pslRef ; pslRef = pslRef->next )
	{
	if (optionExists("exons"))
	    mapBlocks(pslRef, psl, outPsl, (void *)outF);
	else
	    {
	    newPsl = NULL;
	    mapBlocks(pslRef, psl, addPsl, &newPsl);
	    pslTabOut(newPsl, outF);
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if (argc != 4)
    usage();
exonMap(argv[1], argv[2], argv[3]);
return 0;
}
