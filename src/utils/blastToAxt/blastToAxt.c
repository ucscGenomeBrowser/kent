/***************************************************************************
                          blastToAxt.c  -  conversion of a blast to an axt
                             -------------------
    begin                : Wed May 14 2003
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

/* where the axt structure is */
#include "axt.h"
/* where the blast structure is */
#include "blastParse.h"
// #include "blastToAxt.h"

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


struct axt* blastBlockToAxt(struct blastBlock* block,
		struct blastGappedAli* target, struct blastQuery* query)
{
	struct axt* axt = NULL;
	long symCount;
    AllocVar(axt);

    /* we need to make sure that the names are single words */
    axt->qName = cloneWord(query->query);
    axt->qStart = block->qStart;
    axt->qEnd = block->qEnd;
    axt->qStrand = block->qStrand;
    
    axt->tName = cloneWord(target->targetName);
    axt->tStart = block->tStart;
    axt->tEnd = block->tEnd;
    axt->tStrand = block->tStrand;
	axt->score = block->bitScore;
	
    axt->symCount = symCount = block->totalCount;
    axt->tSym = cloneMem(block->tSym, symCount+1);
    axt->qSym = cloneMem(block->qSym, symCount+1);

    return axt;
}


/* either returns a completed axt structure
	or NULL if none is found in the blastFile
*/

struct axt* blastToAxt(struct blastFile* firstBlast)
{
	struct blastFile*		blast;
	struct blastQuery*		query;
	struct blastGappedAli*	target;
	struct blastBlock*		block;

	struct axt				*axt = NULL, *lastAxt = NULL;	/* the results */
	
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
					struct axt* nextAxt;
					nextAxt = blastBlockToAxt(block,target,query);
					if (lastAxt)
					{
						lastAxt->next = nextAxt;
					} else
					{
						axt = nextAxt;
					}
					lastAxt = nextAxt;
				}
			}
		}
	}
	if (lastAxt)
	{
		/* safeguard */
		lastAxt->next = NULL;
	}
	
	return axt;
}



