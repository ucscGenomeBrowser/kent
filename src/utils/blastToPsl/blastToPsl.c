/***************************************************************************
                          blastToPsl.c  -  converts a blastFile to Psl
                             -------------------
    begin                : Thu May 15 2003
    copyright            : (C) 2003 by George Shackelford
    email                : ggshack@soe.ucsc.edu
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "common.h"

/* where the psl structure is */
#include "psl.h"
/* where the blast structure is */
#include "blastParse.h"
#include "blastToAxt.h"


#define MAX_WORD 100

static char* cloneWord(char* strg)
{
	char word[MAX_WORD], *wordPtr;
	long count;
	
	for (count=MAX_WORD-1, wordPtr = word;
		*strg && *strg!=' ' && *strg!='\t' && count;
		count--)
	{
		*wordPtr++ = *strg++;
	}

	*wordPtr = 0;
	return cloneString(word);
}


/* Routine I shamelessly "borrowed" from axtToPsl.c
*/
void countInserts(char *s, unsigned *retNumInsert, int 
*retBaseInsert)
/* Count up number and total size of inserts in s. */
{
int size = strlen(s);
char c, lastC = s[0];
int i;
int baseInsert = 0, numInsert = 0;
if (lastC == '-')
    errAbort("%s starts with -", s);
for (i=0; i<size; ++i)
    {
    c = s[i];
    if (c == '-')
        {
	if (lastC != '-')
	     numInsert += 1;
	baseInsert += 1;
	}
    lastC = c;
    }
*retNumInsert = numInsert;
*retBaseInsert = baseInsert;
}

/* each blast block corresponds to a Psl struct
*/
struct psl* blastBlockToPsl(struct blastBlock* block,
		struct blastGappedAli* target, struct blastQuery* query)
{
	bool eitherInsert;
	int qs,qe,ts,te,i;
	int aliSize;
	char q,t;
	int blockCount = 1, blockIx=0;
	struct psl *ret;

	aliSize = strlen(block->qSym);
	AllocVar(ret);
	ret->match = block->matchCount;
	ret->misMatch = block->totalCount - block->matchCount;
	
	/* the next two apparently are both zero */
	ret->repMatch = 0;
	ret->nCount = 0;

	countInserts(block->qSym, &(ret->qNumInsert), &(ret->qBaseInsert));
	countInserts(block->tSym, &(ret->tNumInsert), &(ret->tBaseInsert));
	ret->blockCount = blockCount = 1 + ret->qNumInsert + ret->tNumInsert;
	ret->strand[0] = block->qStrand;
	ret->strand[1] = block->tStrand;
	ret->strand[2] = 0;
	
	ret->qName = cloneWord(query->query);
	ret->qSize = query->queryBaseCount;
	ret->qStart = block->qStart;
	ret->qEnd = block->qEnd;
	
	ret->tName = cloneWord(target->targetName);
	ret->tSize = target->targetSize;
	ret->tStart = block->tStart;
	ret->tEnd = block->tEnd;

	/* NOTE:
		I am assuming that BLAST output is well behaved in the sense
		that it doesn't have leading or trailing gaps.
	*/

	/* More code brazenly copied from jk's axtToPsl.c
		for building the blocks.
		I did do some mods to it...
	*/
	
    /* Allocate dynamic memory for block lists. */
    AllocArray(ret->blockSizes, blockCount);
    AllocArray(ret->qStarts, blockCount);
    AllocArray(ret->tStarts, blockCount);

    /* Figure block sizes and starts. */
    eitherInsert = FALSE;
    qs = qe = ret->qStart;
    ts = te = ret->tStart;
    for (i=0; i<aliSize; ++i)
        {
        q = block->qSym[i];
        t = block->tSym[i];
        if (q == '-' || t == '-')
            {
    	if (!eitherInsert)
    	    {
    	    ret->blockSizes[blockIx] = qe - qs;
    	    ret->qStarts[blockIx] = qs;
    	    ret->tStarts[blockIx] = ts;
    	    ++blockIx;
    	    eitherInsert = TRUE;
    	    }
    	else if (i > 0)
    	    {
    	    /* Handle cases like
    	     *     aca---gtagtg
    	     *     acacag---gtg
    	     */
    	    if ((q == '-' && block->tSym[i-1] == '-') ||
    	    	(t == '-' && block->qSym[i-1] == '-'))
    	        {
    		ret->blockSizes[blockIx] = 0;
    		ret->qStarts[blockIx] = qe;
    		ret->tStarts[blockIx] = te;
    		++blockIx;
    		}
    	    }
    	if (q != '-')
    	   qe += 1;
    	if (t != '-')
    	   te += 1;
    	}
        else
            {
    	if (eitherInsert)
    	    {
    	    qs = qe;
    	    ts = te;
    	    eitherInsert = FALSE;
    	    }
    	qe += 1;
    	te += 1;
    	}
        }
    assert(blockIx == blockCount-1);
    ret->blockSizes[blockIx] = qe - qs;
    ret->qStarts[blockIx] = qs;
    ret->tStarts[blockIx] = ts;

	return ret;
}


struct psl* blastToPsl(struct blastFile* firstBlast)
{
	struct blastFile*		blast;
	struct blastQuery*		query;
	struct blastGappedAli*	target;
	struct blastBlock*		block;

	struct psl				*ret = NULL, *lastPsl = NULL;	/* the results */

	/* start with the first file */
	for (blast = firstBlast; blast; blast = blast->next)
	{
		/* process each query */
		for (query = blast->queries; query; query = query->next)
		{
			/* for each alignment to a target */
			for (target = query->gapped; target; target = target->next)
			{
				/* for each block of the alignment (if more than one) */
				for (block = target->blocks; block; block = block->next)
				{
					struct psl* nextPsl;
					nextPsl = blastBlockToPsl(block,target,query);
					if (lastPsl)
					{
						lastPsl->next = nextPsl;
					} else
					{
						ret = nextPsl;
					}
					lastPsl = nextPsl;
				}
			}
		}
	}
	if (lastPsl)
	{
		/* safeguard */
		lastPsl->next = NULL;
	}

	return ret;
}


