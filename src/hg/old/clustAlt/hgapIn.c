/* HGAP input.  Loads input for clusterer from HGAP MySQL database. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "dnautil.h"
#include "jksql.h"
#include "hgap.h"
#include "cda.h"
#include "clustAlt.h"

struct hgNest *findBelow(struct hgNest *nest, HGID id)
/* Return element in nest matching ID. */
{
struct hgNest *child;
struct hgNest *grand;

if (nest->id == id)
    return nest;
for (child = nest->children; child != NULL; child = child->next)
    {
    if (child->children != NULL)
	{
	if ((grand = findBelow(child->children, id)) != NULL)
	    return grand;
	}
    else if (child->id == id)
	return child;
    }
return NULL;
}

static struct hgNest *mustFindBelow(struct hgNest *nest, HGID id)
/* Return matching element in nest or squawk and die. */
{
struct hgNest *match;
if ((match = findBelow(nest, id)) == NULL)
    errAbort("couldn't findBelow %lu", id);
return match;
}

static void finishCda(struct cdaAli *cda, struct cdaBlock *blocks, int blockCount)
/* Finish up cdaAli */
{
if (cda != NULL)
    {
    int blockByteCount = blockCount * sizeof(blocks[0]);
    cda->blockCount = blockCount;
    cda->blocks = needMem(blockByteCount);
    memcpy(cda->blocks, blocks, blockByteCount);
    }
}

static void freeSlListInHelVal(struct hashEl *hel)
/* Free singly linked list in hash table. */
{
slFreeList(&hel->val);
}

struct estClone
/* This keeps track of all alignments associated with a particular 
 * EST clone. */
    {
    struct estClone *next;	/* Next in list. */
    struct cdaAli *cda;		/* Alignment. */
    boolean used;		/* Is this one used? */
    };

static void loadAliFromHgap(char *bacName, struct cdaAli **retCdaList, struct hash **retCloneHash)
/* Get lists of mrna alignments and sort into convenient categories for UI.  The cloneHash
 * values are estClones. */
{
struct hgBac *bac = hgGetBac(bacName);
struct hgNest *bacNest = bac->nest;
struct hgNest *conNest;
struct dyString *query = newDyString(1024);
struct sqlConnection *conn = hgAllocConn();
struct hash *estCloneHash = newHash(14);
struct hashEl *hel;
struct estClone *estClone;
struct cdaAli *cdaList = NULL, *cda = NULL;
struct sqlResult *sr = NULL;
char **row;
boolean firstTime = TRUE;
int blockAlloc = 256;
int blockCount = 0;
struct cdaBlock *blocks = needMem(blockAlloc*sizeof(blocks[0]));
struct cdaBlock *b;
HGID lastParent = 0;

/* Make up SQL query to get all alignment info in any contigs that are part of
 * bac. */
dyStringAppend(query, 
    "select cdna_block.parent, cdna_block.target, cdna_block.tstart, cdna_block.tend,"
           "cdna_block.score, cdna_block.qstart, cdna_block.qend,"
	   "cdna_ali.intron_orientation, cdna_ali.qstart, cdna_ali.qend,"
	   "cdna_ali.score, cdna_ali.qorientation, cdna_ali.tstart, cdna_ali.tend,"
	   "mrna.acc, mrna.direction, mrna.clone "
    "from cdna_block,cdna_ali,mrna "
    "where cdna_block.target in (");
dyStringPrintf(query, "%lu,", bacNest->id);
for (conNest = bacNest->children; conNest != NULL; conNest = conNest->next)
    {
    if (firstTime)
	firstTime = FALSE;
    else
	dyStringAppend(query, ",");
    dyStringPrintf(query, "%lu", conNest->id);
    }
dyStringAppend(query, ") ");
dyStringAppend(query,
     "and cdna_block.parent = cdna_ali.id "
     "and cdna_ali.query = mrna.id "
     "order by cdna_block.parent,cdna_block.piece");

sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    HGID parent = sqlUnsigned(row[0]);
    HGID targetId = sqlUnsigned(row[1]);
    int tStart = sqlUnsigned(row[2]);
    int tEnd = sqlUnsigned(row[3]);
    int score = sqlUnsigned(row[4]);
    int qStart = sqlUnsigned(row[5]);
    int qEnd = sqlUnsigned(row[6]);
    char *s;
    if (parent != lastParent)
	{
	int intronOrientation = sqlSigned(row[7]);
	int  parentQstart = sqlUnsigned(row[8]);
	int parentQend = sqlUnsigned(row[9]);
	int parentScore = sqlUnsigned(row[10]);
	int baseCount = parentQend - parentQstart;
	int milliScore = 1000*parentScore/baseCount;
	finishCda(cda, blocks, blockCount);
	blockCount = 0;
	lastParent = parent;

	if (milliScore >= 900)
	    {
	    int qorientation = sqlSigned(row[11]);
	    int tStart = sqlUnsigned(row[12]);
	    int tEnd = sqlUnsigned(row[13]);
	    char *queryAcc = row[14];
	    char *dir = row[15];
	    char *clone = row[16];
	    AllocVar(cda);
	    cda->name = cloneString(queryAcc);
	    cda->baseCount = baseCount;
	    cda->milliScore = milliScore;
	    conNest = mustFindBelow(bacNest, targetId);
	    cda->chromIx = conNest->contig->ix;
	    cda->strand = (qorientation > 0 ? '+' : '-');
	    if (intronOrientation != 0)
		cda->direction = (intronOrientation*qorientation > 0 ? '+' : '-');
	    else
		{
		cda->direction =  (dir[0] == '3' ? '-' : '+');
		}
	    cda->chromStart = tStart;
	    cda->chromEnd = tEnd;
	    cda->hasIntrons = (intronOrientation != 0);
	    slAddHead(&cdaList, cda);

	    AllocVar(estClone);
	    estClone->cda = cda;
	    hel = hashStore(estCloneHash, clone);
	    slAddHead(&hel->val, estClone);
	    }
	else
	    {
	    cda = NULL;
	    }
	}
    if (blockCount >= blockAlloc)
	{
	blockAlloc <<= 1;
	blocks = needMoreMem(blocks, blockCount*sizeof(blocks[0]), blockAlloc*sizeof(blocks[0]));
	}
    b = &blocks[blockCount++];
    b->nStart = qStart;
    b->nEnd = qEnd;
    b->hStart = tStart;
    b->hEnd = tEnd;
    b->midScore = 255 * score / (qEnd - qStart);
    }
finishCda(cda, blocks, blockCount);

/* Clean up. */
sqlFreeResult(&sr);
hgFreeConn(&conn);
freeMem(blocks);
slReverse(&cdaList);

/* Return */
*retCdaList = cdaList;
*retCloneHash = estCloneHash;
return;
}

static char oppositeStrand(char c)
/* Return + for - and - for +. */
{
if (c == '+')
    return '-';
else if (c == '-')
    return '+';
else
    errAbort("Unrecognized strand %c", c);
}

static struct clonePair *lcpList = NULL; /* Communicates list between cloneIntoPairs and makePairs. */

static void cloneIntoPairs(struct hashEl *hel)
/* Turn list of cdas in hel->val to pairs. */
{
struct estClone *ec, *mate;
struct cdaAli *ecCda, *mateCda;
struct clonePair *cp;
boolean gotClone = differentString(hel->name,"0");
boolean ecGotIntron, bestMateGotIntron;

for (ec = hel->val; ec != NULL; ec = ec->next)
    {
    int bestMateDistance = 0x3fffffff;
    struct estClone *bestMate = NULL;

    if (ec->used)
	continue;
    ecCda = ec->cda;
    if (gotClone)
	{
	for (mate = ec->next; mate != NULL; mate = mate->next)
	    {
	    if (!mate->used)
		{
		mateCda = mate->cda;
		if (mateCda->direction != ecCda->direction && mateCda->chromIx == ecCda->chromIx 
		    && mateCda->strand != ecCda->strand)
		    {
		    int distance;
		    int s, e;
		    s = min(mateCda->chromStart, ecCda->chromStart);
		    e = max(mateCda->chromEnd, ecCda->chromEnd);
		    distance = e - s;
		    if (distance < bestMateDistance)
			{
			bestMateDistance = distance;
			bestMate = mate;
			}
		    }
		}
	    }
	}
    if (ecCda->hasIntrons || (bestMate != NULL && bestMate->cda->hasIntrons))
	{
	AllocVar(cp);
	cp->chromIx = ecCda->chromIx;
	if (bestMate != NULL)
	    {
	    bestMate->used = TRUE;
	    mateCda = bestMate->cda;
	    cp->tStart = min(mateCda->chromStart, ecCda->chromStart);
	    cp->tEnd = min(mateCda->chromEnd, ecCda->chromEnd);
	    }
	else
	    {
	    mateCda = NULL;
	    cp->tStart = ecCda->chromStart;
	    cp->tEnd = ecCda->chromEnd;
	    }
	if (ecCda->direction == '+')
	    {
	    cp->p5 = ecCda;
	    cp->p3 = mateCda;
	    cp->strand = ecCda->strand;
	    }
	else
	    {
	    cp->p5 = mateCda;
	    cp->p3 = ecCda;
	    cp->strand = oppositeStrand(ecCda->strand);
	    }
	slAddHead(&lcpList, cp);
	}
    }
}

static struct clonePair *makePairs(struct hash *estCloneHash)
/* Turn clone hash into pair list. */
{
lcpList = NULL;
hashTraverseEls(estCloneHash, cloneIntoPairs);
return lcpList;
}

static void filterNonIntronic(struct clusterInput *ci)
/* Free up elements from ci->cdaList that are from clones with
 * no introns.  (Easy since everything on clonePair list
 * at this point does have an intron.) */
{
struct clonePair *cp;
struct cdaAli *newList = NULL, *cda, *next;
for (cp = ci->pairList; cp != NULL; cp = cp->next)
    {
    if ((cda = cp->p3) != NULL)
	cda->hasIntrons = TRUE;
    if ((cda = cp->p5) != NULL)
	cda->hasIntrons = TRUE;
    }
for (cda = ci->cdaList; cda != NULL; cda = next)
    {
    next = cda->next;
    if (cda->hasIntrons)
	{
	slAddHead(&newList, cda);
	}
    else
	{
	cdaFreeAli(cda);
	}
    }
slReverse(&newList);
ci->cdaList = newList;
}

struct clusterInput *bacInputFromHgap(char *bacAcc)
/* Read in clustering input from hgap database. */
{
struct clusterInput *ci;
struct dnaSeq *seqList, *seq, **seqArray;
int seqCount;
int i;
struct hash *estCloneHash = NULL; /* Values are estClones. */

/* Get contig list and set it up so can access it as
 * an array as well. */
AllocVar(ci);
ci->seqList = seqList = hgBacContigSeq(bacAcc);
ci->seqCount = seqCount = slCount(seqList);
ci->seqArray = seqArray = needMem(seqCount * sizeof(seqArray[0]));
for (i=0,seq=seqList; i<seqCount; ++i,seq = seq->next)
    seqArray[i] = seq;

/* Get list of alignments and blocks from database that hit bac. */
loadAliFromHgap(bacAcc, &ci->cdaList, &estCloneHash);

/* Process clones into pairs. */
ci->pairList = makePairs(estCloneHash);
hashTraverseEls(estCloneHash, freeSlListInHelVal);	/* Slow... */
freeHash(&estCloneHash);
filterNonIntronic(ci);


return ci;
}

