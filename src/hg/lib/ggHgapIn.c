This file is depreciated.  Underlying database has changed radically.

/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* HGAP input.  Loads input for clusterer from HGAP MySQL database. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "dnautil.h"
#include "jksql.h"
#include "hgRelate.h"
#include "ens.h"
#include "ggPrivate.h"
#include "maDbRep.h"

static void loadAliFromHgap(char *bacName, struct ggMrnaAli **retMaList)
/* Get lists of mrna alignments and sort into convenient categories for UI.  The cloneHash
 * values are estClones. */
{
char query[256];
struct hgBac *bac = hgGetBac(bacName);
HGID bacId = bac->id;
struct sqlConnection *conn = hgAllocConn();
struct hash *estCloneHash = newHash(14);
struct hashEl *hel;
struct estClone *estClone;
struct ggMrnaAli *maList = NULL, *ma = NULL;
struct sqlResult *sr = NULL;
char **row;
boolean firstTime = TRUE;
static char *tables[2] = {"mrnaAli", "estAli"};
int tableIx;

/* Make up SQL query to get all alignment info in any contigs that are part of
 * bac. */
for (tableIx = 0; tableIx < ArraySize(tables); ++tableIx)
    {
    char *table = tables[tableIx];
    sprintf(query, "select * from %s where tStartBac=%u", table, bacId);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	struct mrnaAli *ali = mrnaAliLoad(row);
	int blockCount = ali->blockCount;
	int baseCount = ali->qEnd - ali->qStart;
	int milliScore = ali->score*1000/baseCount;
	int milliCoverage = baseCount*1000/ali->qTotalSize;
	struct ggMrnaBlock *blocks;
	int i;

	if (ali->score >= 100 && milliScore >= 850 && milliCoverage >= 200 
	    && (ali->score > 800 || ali->hasIntrons))
	    {
	    int mergedBlockCount = 0;
	    struct ggMrnaBlock *lastB = NULL;
	    AllocVar(ma);
	    ma->id = ali->id;
	    ma->name = cloneString(ali->qAcc);
	    ma->baseCount = baseCount;
	    ma->milliScore = milliScore;
	    ma->seqIx = 0;
	    ma->strand = ali->orientation;
	    ma->direction = ali->readDir;
	    ma->hasIntrons = ali->hasIntrons;
	    ma->orientation = ma->strand*ma->direction;
	    ma->contigStart = ali->tStartPos;
	    ma->contigEnd = ali->tEndPos;
	    ma->blocks = AllocArray(blocks, blockCount);
	    for (i=0; i<blockCount; ++i)
		{
		struct ggMrnaBlock *b = &blocks[mergedBlockCount];
		int size = ali->blockSizes[i];
		b->qStart = ali->qBlockStarts[i];
		b->qEnd = b->qStart + size;
		b->tStart = ali->tBlockStarts[i];
		b->tEnd = b->tStart + size;
		if (lastB != NULL && 
		   intAbs(lastB->qEnd - b->qStart) <= 2 && 
		   intAbs(lastB->tEnd - b->tStart) <= 2)
		   {
		   lastB->qEnd = b->qEnd;
		   lastB->tEnd = b->tEnd;
		   }
		else
		    {
		    ++mergedBlockCount;
		    lastB = b;
		    }
		}
	    ma->blockCount = mergedBlockCount;
	    slAddHead(&maList, ma);
	    }
	mrnaAliFree(&ali);
	}
    }

/* Clean up. */
sqlFreeResult(&sr);
hgFreeConn(&conn);
slReverse(&maList);

/* Return */
*retMaList = maList;
return;
}

struct ggMrnaInput *ggGetMrnaForBac(char *bacAcc)
/* Read in clustering input from hgap database. */
{
struct ggMrnaInput *ci;
struct dnaSeq *seqList, *seq, **seqArray;
int seqCount;
int i;
struct hash *estCloneHash = NULL; /* Values are estClones. */
struct hgBac *bac = hgGetBac(bacAcc);
HGID *seqIds;

/* Get contig list and set it up so can access it as
 * an array as well. */
AllocVar(ci);
ci->seqList = seqList = ensDnaInBac(bacAcc, dnaLower);
ci->seqCount = seqCount = slCount(seqList);
ci->seqArray = seqArray = needMem(seqCount * sizeof(seqArray[0]));
ci->seqIds = AllocArray(seqIds, seqCount);
for (i=0,seq=seqList; i<seqCount; ++i,seq=seq->next)
    {
    seqArray[i] = seq;
    seqIds[i] = bac->id;
    }

/* Get list of alignments and blocks from database that hit bac. */
loadAliFromHgap(bacAcc, &ci->maList);

return ci;
}

