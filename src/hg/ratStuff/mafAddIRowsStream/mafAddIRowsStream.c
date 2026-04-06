/* mafAddIRowsStream - add 'i' rows to a maf, two-pass streaming version.
 *
 * Memory-efficient rewrite of mafAddIRows.c.  Two passes over the MAF:
 *   Pass 1: reads each block, extracts status metadata into a compact
 *           flat array (~12 bytes/component), builds linkBlock chains,
 *           then frees the mafAli immediately.  Runs chainStrands on
 *           linkBlocks and bridgeSpecies on the compact array.
 *   Pass 2: re-reads the MAF one block at a time, applies pre-computed
 *           statuses, runs fillHoles logic, writes and frees each block.
 *
 * Peak memory: ~40GB for 900M components (vs ~157GB with lightweight list
 * approach, vs entire-file-in-RAM for the original).
 */

/* Copyright (C) 2011 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "errAbort.h"
#include "linefile.h"
#include "hash.h"
#include "chain.h"
#include "options.h"
#include "maf.h"
#include "bed.h"
#include "twoBit.h"
#include "binRange.h"


char *masterSpecies;
char *masterChrom;
struct hash *speciesHash;
struct subSpecies *speciesList;
struct strandHead *strandHeads;

boolean addN = FALSE;
boolean addDash = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafAddIRowsStream - add 'i' rows to a maf (streaming, low memory)\n"
  "usage:\n"
  "   mafAddIRowsStream mafIn twoBitFile mafOut\n"
  "WARNING:  requires a maf with only a single target sequence\n"
  "options:\n"
  "   -nBeds=listOfBedFiles\n"
  "       reads in list of bed files, one per species, with N locations\n"
  "   -addN\n"
  "       adds rows of N's into maf blocks (rather than just annotating them)\n"
  "   -addDash\n"
  "       adds rows of -'s into maf blocks (rather than just annotating them)\n"
  "NOTE: as of November 2025 - can manage GenArk assembly names GCA_...\n"
  "      and GCF_... with their .n extensions.  Can only work with such\n"
  "      such names that begin with GC."
  );
}

static struct optionSpec options[] = {
   {"nBeds", OPTION_STRING},
   {"addN", OPTION_BOOLEAN},
   {"addDash", OPTION_BOOLEAN},
   {NULL, 0},
};

struct bedHead
{
    struct bed *list;
};

/* Compact per-component data.  Replaces keeping full mafComp structs
 * in memory during pass 1.  12 bytes vs ~110 bytes (struct+src+malloc
 * overhead) per component. */
struct compInfo
{
    int leftLen;
    int rightLen;
    char leftStatus;
    char rightStatus;
    short speciesIdx;     /* index into speciesByIdx[], -1 for master */
};

/* Block boundary tracking for the compInfo flat array. */
struct blockIndex
{
    int nBlocks;
    long totalComps;
    int *compCounts;      /* number of components per block */
    long *offsets;        /* offsets[i] = cumulative comp count before block i */
};

struct blockStatus
/* Per-species tracking state for fillHoles logic.
 * Owns its src string (cloned). */
{
    int masterStart, masterEnd;
    boolean active;
    char *src;
    int srcSize;
    char strand;
    int start;
    int size;
    char rightStatus;
    int rightLen;
};

struct subSpecies
{
    struct subSpecies *next;
    char *name;
    int idx;                  /* index for speciesByIdx[] reverse lookup */
    struct hash *hash;
    struct blockStatus blockStatus;
    /* Per-species state for block-major bridgeSpecies */
    int bridgePushState;
    int bridgeLeftLen;
    /* Per-block species lookup cache */
    boolean seenInBlock;
    struct mafComp *blockMc;
};

struct linkBlock
{
    struct linkBlock *next;
    struct cBlock cb;
    long compIdx;            /* index into compInfo array (not a pointer,
                              * survives realloc) */
};

struct strandHead
{
    struct strandHead *next;
    char strand;
    char *name;
    char *qName;
    int qSize;
    char *species;
    struct linkBlock *links;
};

static char *chromFromSrc(char *src)
/* get chrom name from <db>.<chrom>
   returned pointer should be on the . separator */
{
char *p;
if ((p = strchr(src, '.')) == NULL)
    errAbort("Can't find chrom in MAF component src: %s\n", src);
char *skipDot = p;
++skipDot;	/* skip the dot to the word following */
if (startsWith("GC", src))
    {
    char *nextDot = strchr(skipDot,'.');
    if (nextDot)
        {
        p = nextDot;	/* new answer */
        }
    }   /* else: no next dot, leave p it where it is */
return p;
}

static void parseSpeciesFromSrc(char *src, char *buf, int bufSize)
/* Extract species name from "species.chrom" src string into buf. */
{
safef(buf, bufSize, "%s", src);
char *dot = chromFromSrc(buf);
*dot = 0;
}

static void updateBlockStatus(struct blockStatus *bs, struct mafComp *mc)
/* Copy metadata fields from mc into blockStatus, cloning src. */
{
freez(&bs->src);
bs->src = cloneString(mc->src);
bs->srcSize = mc->srcSize;
bs->strand = mc->strand;
bs->start = mc->start;
bs->size = mc->size;
bs->rightStatus = mc->rightStatus;
bs->rightLen = mc->rightLen;
bs->active = TRUE;
}

static void freeAllLinkBlocks(struct strandHead *sh)
/* Free all linkBlock chains from strandHeads. */
{
for (; sh; sh = sh->next)
    {
    struct linkBlock *lb, *next;
    for (lb = sh->links; lb; lb = next)
	{
	next = lb->next;
	freeMem(lb);
	}
    sh->links = NULL;
    }
}

static void freeBlockIndex(struct blockIndex *bi)
{
freeMem(bi->compCounts);
freeMem(bi->offsets);
}

/* ---- Pass 1: compact read ---- */

static int nextSpeciesIdx = 0;
static struct subSpecies **speciesByIdx = NULL;
static int speciesByIdxCapacity = 0;

void readMafsCompact(struct mafFile *mf,
    struct compInfo **retCompArray, long *retCompCount,
    struct blockIndex *blockIdx)
/* Read all maf blocks.  For each block, extract component metadata into
 * the compInfo flat array, build linkBlock chains for strandHeads, then
 * free the mafAli immediately.  No mafComp structs are kept in memory.
 * Peak memory: compInfo array (~12 bytes/comp) + linkBlocks (~32 bytes/comp). */
{
struct mafAli *maf;
char buffer[2048];
char buffer2[2048];
struct strandHead *strandHead;
char *ourChrom = NULL;

/* Dynamic compInfo array */
long compCapacity = 4 * 1024 * 1024;
struct compInfo *compArray = needLargeMem(compCapacity * sizeof(struct compInfo));
long compCount = 0;

/* Dynamic block index arrays */
int blockCapacity = 1024 * 1024;
int *blockCompCounts = needLargeMem(blockCapacity * sizeof(int));
long *blockOffsets = needLargeMem(blockCapacity * sizeof(long));
int nBlocks = 0;

/* Species by index array */
speciesByIdxCapacity = 256;
speciesByIdx = needLargeMem(speciesByIdxCapacity * sizeof(struct subSpecies *));

while((maf = mafNext(mf)) != NULL)
    {
    struct mafComp *mc, *masterMc = maf->components;
    char *species = buffer;
    char *chrom;

    if (ourChrom == NULL)
	ourChrom = cloneString(masterMc->src);
    else
	{
	if (differentString(masterMc->src, ourChrom))
	    errAbort("ERROR: mafAddIrows requires maf have only one target sequence.\n"
		"Use mafSplit -byTarget -useFullSequenceName to split maf");
	}

    strcpy(species, masterMc->src);
    chrom = chromFromSrc(species);
    if (chrom)
	*chrom++ = 0;
    else
	errAbort("reference species has no chrom name\n");

    if (masterSpecies == NULL)
	{
	masterSpecies = cloneString(species);
	masterChrom = cloneString(chrom);
	}
    else
	{
	if (!sameString(masterSpecies, species))
	    errAbort("first species (%s) not master species (%s)\n",species,masterSpecies);
	}

    /* Grow block index if needed */
    if (nBlocks >= blockCapacity)
	{
	blockCapacity *= 2;
	blockCompCounts = needLargeMemResize(blockCompCounts,
	    blockCapacity * sizeof(int));
	blockOffsets = needLargeMemResize(blockOffsets,
	    blockCapacity * sizeof(long));
	}
    blockOffsets[nBlocks] = compCount;
    int blockCompStart = compCount;

    /* Store master component in compInfo (speciesIdx = -1) */
    if (compCount >= compCapacity)
	{
	compCapacity *= 2;
	compArray = needLargeMemResize(compArray,
	    compCapacity * sizeof(struct compInfo));
	}
    compArray[compCount].leftStatus = 0;
    compArray[compCount].rightStatus = 0;
    compArray[compCount].leftLen = 0;
    compArray[compCount].rightLen = 0;
    compArray[compCount].speciesIdx = -1;
    compCount++;

    for(mc = masterMc->next; mc; mc = mc->next)
	{
	struct linkBlock *linkBlock;
	struct subSpecies *subSpecies = NULL;

	strcpy(species, mc->src);
        chrom = chromFromSrc(species);
	*chrom++ = 0;

	if ((subSpecies = hashFindVal(speciesHash, species)) == NULL)
	    {
	    AllocVar(subSpecies);
	    subSpecies->name = cloneString(species);
	    subSpecies->idx = nextSpeciesIdx++;
	    subSpecies->hash = newHash(6);
	    subSpecies->blockStatus.masterStart = masterMc->start;
	    slAddHead(&speciesList, subSpecies);
	    hashAdd(speciesHash, species, subSpecies);
	    /* Grow speciesByIdx if needed */
	    if (subSpecies->idx >= speciesByIdxCapacity)
		{
		speciesByIdxCapacity *= 2;
		speciesByIdx = needLargeMemResize(speciesByIdx,
		    speciesByIdxCapacity * sizeof(struct subSpecies *));
		}
	    speciesByIdx[subSpecies->idx] = subSpecies;
	    }
	subSpecies->blockStatus.masterEnd = masterMc->start + masterMc->size;

	sprintf(buffer2, "%s%c%s", masterChrom, mc->strand, chrom);
	if ((strandHead = hashFindVal(subSpecies->hash, buffer2)) == NULL)
	    {
	    AllocVar(strandHead);
	    hashAdd(subSpecies->hash, buffer2, strandHead);
	    strandHead->name = cloneString(buffer2);
	    strandHead->species = cloneString(species);
	    strandHead->qName = cloneString(chrom);
	    strandHead->qSize = mc->srcSize;
	    strandHead->strand = mc->strand;
	    slAddHead(&strandHeads, strandHead);
	    }

	/* Store component in compInfo */
	if (compCount >= compCapacity)
	    {
	    compCapacity *= 2;
	    compArray = needLargeMemResize(compArray,
		compCapacity * sizeof(struct compInfo));
	    }
	compArray[compCount].leftStatus = 0;
	compArray[compCount].rightStatus = 0;
	compArray[compCount].leftLen = 0;
	compArray[compCount].rightLen = 0;
	compArray[compCount].speciesIdx = subSpecies->idx;

	/* Build linkBlock with index into compArray (not mc pointer) */
	AllocVar(linkBlock);
	linkBlock->compIdx = compCount;
	linkBlock->cb.qStart = mc->start;
	linkBlock->cb.qEnd = mc->start + mc->size;
	linkBlock->cb.tStart = masterMc->start;
	linkBlock->cb.tEnd = masterMc->start + masterMc->size;
	slAddHead(&strandHead->links, linkBlock);

	compCount++;
	}

    blockCompCounts[nBlocks] = compCount - blockCompStart;
    nBlocks++;

    /* Free the entire mafAli immediately -- we've extracted everything
     * we need into compInfo and linkBlocks. */
    mafAliFree(&maf);
    }

/* Trim arrays to exact size */
compArray = needLargeMemResize(compArray, compCount * sizeof(struct compInfo));
blockCompCounts = needLargeMemResize(blockCompCounts, nBlocks * sizeof(int));
blockOffsets = needLargeMemResize(blockOffsets, nBlocks * sizeof(long));

*retCompArray = compArray;
*retCompCount = compCount;
blockIdx->nBlocks = nBlocks;
blockIdx->totalComps = compCount;
blockIdx->compCounts = blockCompCounts;
blockIdx->offsets = blockOffsets;
}


void chainStrands(struct strandHead *strandHead, struct hash *bedHash,
    struct compInfo *compArray)
/* Compute i-row statuses by walking consecutive link pairs per strand chain.
 * Writes statuses into compArray via linkBlock->compIdx. */
{
for(; strandHead ; strandHead = strandHead->next)
    {
    struct linkBlock *link, *prevLink;
    struct hashEl *hel = hashLookup(bedHash, strandHead->species);
    struct hash *chromHash = (hel != NULL) ? hel->val : NULL;
    struct bedHead *bedHead = (chromHash != NULL) ?
	hashFindVal(chromHash, strandHead->qName): NULL;

    slReverse(&strandHead->links);

    prevLink = strandHead->links;
    compArray[prevLink->compIdx].leftStatus = MAF_NEW_STATUS;
    for(link = prevLink->next; link; prevLink = link, link = link->next)
	{
	int tDiff = link->cb.tStart - prevLink->cb.tEnd;
	int qDiff = link->cb.qStart - prevLink->cb.qEnd;
	int nCount = 0;
	int nStart, nEnd;
	struct bed *bed;

	if (strandHead->strand == '+')
	    {
	    nStart = prevLink->cb.qEnd;
	    nEnd = link->cb.qStart;
	    }
	else
	    {
	    nEnd = strandHead->qSize - prevLink->cb.qEnd;
	    nStart = strandHead->qSize - link->cb.qStart;
	    }

	/* a very inefficient search for an N bed */
	if ((nEnd - nStart > 0) && (bedHead))
	    {
	    for(bed = bedHead->list; bed; bed = bed->next)
		{
		if (bed->chromStart >= nEnd)
		    break;

		if ( bed->chromEnd > nStart)
		    {
		    nCount += positiveRangeIntersection(nStart, nEnd,
			bed->chromStart, bed->chromEnd);
		    }
		}
	    }


	if ((qDiff && (100 * nCount / qDiff > 95))
		&& (tDiff && (100 * nCount / tDiff > 10)))
	    {
	    compArray[prevLink->compIdx].rightStatus = MAF_MISSING_STATUS;
	    compArray[prevLink->compIdx].rightLen = nCount;
	    compArray[link->compIdx].leftStatus = MAF_MISSING_STATUS;
	    compArray[link->compIdx].leftLen = nCount;
	    }
	else if  ((tDiff > 100000) ||
		  (qDiff > 100000) || (qDiff < -100000))
	    {
	    compArray[prevLink->compIdx].rightStatus = MAF_NEW_STATUS;
	    compArray[prevLink->compIdx].rightLen = 0;
	    compArray[link->compIdx].leftStatus = MAF_NEW_STATUS;
	    compArray[link->compIdx].leftLen = 0;
	    }
	else if  (qDiff < 0)
	    {
	    compArray[prevLink->compIdx].rightStatus = MAF_TANDEM_STATUS;
	    compArray[prevLink->compIdx].rightLen = -qDiff;
	    compArray[link->compIdx].leftStatus = MAF_TANDEM_STATUS;
	    compArray[link->compIdx].leftLen = -qDiff;
	    }
	else if (qDiff == 0)
	    {
	    compArray[prevLink->compIdx].rightStatus = MAF_CONTIG_STATUS;
	    compArray[prevLink->compIdx].rightLen = 0;
	    compArray[link->compIdx].leftStatus = MAF_CONTIG_STATUS;
	    compArray[link->compIdx].leftLen = 0;
	    }
	else
	    {
	    compArray[prevLink->compIdx].rightStatus = MAF_INSERT_STATUS;
	    compArray[prevLink->compIdx].rightLen = qDiff;
	    compArray[link->compIdx].leftStatus = MAF_INSERT_STATUS;
	    compArray[link->compIdx].leftLen = qDiff;
	    }
	}
    compArray[prevLink->compIdx].rightStatus = MAF_NEW_STATUS;
    }
}

void bridgeSpecies(struct compInfo *compArray, struct blockIndex *blockIdx,
    struct subSpecies *speciesListHead)
/* Block-major bridgeSpecies.  Iterates compArray using block offsets.
 * Uses speciesIdx for O(1) species lookup. */
{
struct subSpecies *sp;
int bi;

for (sp = speciesListHead; sp; sp = sp->next)
    {
    sp->bridgePushState = 0;
    sp->bridgeLeftLen = 0;
    }

for (bi = 0; bi < blockIdx->nBlocks; bi++)
    {
    long offset = blockIdx->offsets[bi];
    int count = blockIdx->compCounts[bi];
    int i;

    for (sp = speciesListHead; sp; sp = sp->next)
	sp->seenInBlock = FALSE;

    /* Skip component 0 (master, speciesIdx == -1) */
    for (i = 1; i < count; i++)
	{
	struct compInfo *ci = &compArray[offset + i];
	struct subSpecies *sub;

	if (ci->speciesIdx < 0)
	    continue;
	sub = speciesByIdx[ci->speciesIdx];
	if (sub->seenInBlock)
	    continue;
	sub->seenInBlock = TRUE;

	if (ci->leftStatus == 0)
	    errAbort("zero left status in block %d, component %d\n", bi, i);

	if (ci->leftStatus == MAF_NEW_STATUS)
	    {
	    if (sub->bridgePushState)
		{
		ci->leftStatus = MAF_NEW_NESTED_STATUS;
		ci->leftLen = sub->bridgeLeftLen;
		}
	    sub->bridgePushState++;
	    }

	if (ci->rightStatus == 0)
	    {
	    errAbort("zero right status in block %d, component %d\n", bi, i);
	    }
	else if (isContigOrTandem(ci->rightStatus) || (ci->rightStatus == MAF_INSERT_STATUS))
	    sub->bridgeLeftLen = ci->rightLen;
	else if (ci->rightStatus == MAF_NEW_STATUS)
	    {
	    sub->bridgePushState--;
	    if (sub->bridgePushState)
		{
		ci->rightStatus = MAF_NEW_NESTED_STATUS;
		ci->rightLen = sub->bridgeLeftLen;
		}
	    }
	}
    }
}


/* ---- Pass 2: re-read, apply statuses, fill holes, write ---- */

static void applyStatuses(struct compInfo *compArray, struct blockIndex *blockIdx,
    int blockNum, struct mafAli *maf)
/* Copy pre-computed statuses from compArray onto freshly-read maf block. */
{
long offset = blockIdx->offsets[blockNum];
int count = blockIdx->compCounts[blockNum];
struct mafComp *mc;
int i;
for (mc = maf->components, i = 0; mc && i < count; mc = mc->next, i++)
    {
    mc->leftStatus = compArray[offset + i].leftStatus;
    mc->leftLen = compArray[offset + i].leftLen;
    mc->rightStatus = compArray[offset + i].rightStatus;
    mc->rightLen = compArray[offset + i].rightLen;
    }
}

void streamFillAndWrite(char *mafIn, struct compInfo *compArray,
    struct blockIndex *blockIdx,
    struct subSpecies *speciesList, struct twoBitFile *twoBit, FILE *f)
/* Pass 2: re-read the MAF one block at a time, apply statuses, fill holes,
 * write and free. */
{
struct mafFile *mf2 = mafOpen(mafIn);
struct mafAli *maf;
int lastEnd = 100000000;
struct subSpecies *species;
int blockNum = 0;

while ((maf = mafNext(mf2)) != NULL)
    {
    struct mafComp *mc = NULL, *masterMc, *lastMc = NULL;
    struct blockStatus *blockStatus;

    applyStatuses(compArray, blockIdx, blockNum, maf);

    masterMc = maf->components;

    /* SECTION A: fill gap between previous block and this one */
    if (masterMc->start > lastEnd)
	{
	struct mafAli *gapMaf = NULL;
	struct mafComp *lastGapMc = NULL;

	for(species = speciesList; species; species = species->next)
	    {
	    mc = NULL;
	    blockStatus = &species->blockStatus;
	    if (blockStatus->active)
		{
		switch (blockStatus->rightStatus)
		    {
		    case MAF_MISSING_STATUS:
		    case MAF_NEW_NESTED_STATUS:
		    case MAF_MAYBE_NEW_NESTED_STATUS:
		    case MAF_CONTIG_STATUS:
		    case MAF_TANDEM_STATUS:
		    case MAF_INSERT_STATUS:
			AllocVar(mc);
			mc->rightStatus = mc->leftStatus = blockStatus->rightStatus;
			mc->rightLen = mc->leftLen = blockStatus->rightLen;
			mc->src = cloneString(blockStatus->src);
			mc->srcSize = blockStatus->srcSize;
			mc->strand = blockStatus->strand;
			mc->start = blockStatus->start + blockStatus->size;
			if (lastGapMc == NULL)
			    {
			    struct mafComp *miniMasterMc = NULL;
			    char *seqName;
			    struct dnaSeq *seq;

			    AllocVar(miniMasterMc);
			    miniMasterMc->next = mc;
			    miniMasterMc->strand = '+';
			    miniMasterMc->srcSize = masterMc->srcSize;
			    miniMasterMc->src = cloneString(masterMc->src);
			    miniMasterMc->start = lastEnd;
			    miniMasterMc->size =  masterMc->start - lastEnd;

			    if ((seqName = chromFromSrc(miniMasterMc->src)) != NULL)
				seqName++;
			    else
			    	seqName = miniMasterMc->src;

			    seq = twoBitReadSeqFrag(twoBit, seqName, lastEnd, masterMc->start);
			    miniMasterMc->text = seq->dna;

			    AllocVar(gapMaf);
			    gapMaf->textSize = maf->textSize;
			    gapMaf->components = miniMasterMc;
			    }
			else
			    {
			    lastGapMc->next = mc;
			    }
			lastGapMc = mc;
			if  (blockStatus->rightStatus ==  MAF_MISSING_STATUS)
			    {
			    if (addN)
				{
				char buffer[256];

				safef(buffer, sizeof(buffer), "%s.N",species->name);
				freez(&mc->src);
				mc->src = cloneString(buffer);
				mc->start = 0;
				mc->srcSize = 200000;
				mc->size =  masterMc->start - lastEnd;
				mc->text = needMem(mc->size + 1);
				memset(mc->text, 'N', mc->size);
				}
			    }
			else
			    {
			    if (addDash)
				{
				mc->size = masterMc->size;
				mc->srcSize = blockStatus->srcSize;
				mc->text = needMem(mc->size + 1);
				memset(mc->text, '-', mc->size);
				mc->text[mc->size] = 0;
				if (mc->size == 0)
				    errAbort("bad dash add");
				mc->size = 0;
				}
			    }
			break;
		    }
		}
	    }

	if (gapMaf)
	    {
	    mafWrite(f, gapMaf);
	    mafAliFree(&gapMaf);
	    }
	}

    lastEnd = masterMc->start + masterMc->size;
    for(lastMc = masterMc; lastMc->next; lastMc = lastMc->next)
	;

    /* Pre-scan: build per-species component lookup for this block. */
	{
	char speciesBuf[2048];
	struct mafComp *scanMc;
	for (species = speciesList; species; species = species->next)
	    {
	    species->seenInBlock = FALSE;
	    species->blockMc = NULL;
	    }
	for (scanMc = masterMc->next; scanMc; scanMc = scanMc->next)
	    {
	    struct subSpecies *sub;
	    parseSpeciesFromSrc(scanMc->src, speciesBuf, sizeof(speciesBuf));
	    sub = hashFindVal(speciesHash, speciesBuf);
	    if (sub && !sub->seenInBlock)
		{
		sub->blockMc = scanMc;
		sub->seenInBlock = TRUE;
		}
	    }
	}

    /* SECTION B: add missing species to current block */
    for(species = speciesList; species; species = species->next)
	{
	blockStatus = &species->blockStatus;

	mc = NULL;
	if ((blockStatus->masterStart <= masterMc->start) &&
	    (blockStatus->masterEnd > masterMc->start))
	    {
	    mc = species->blockMc;
	    }
	if ((blockStatus->masterStart <= masterMc->start) &&
	    (blockStatus->masterEnd > masterMc->start) &&
	    (mc == NULL))
	    {
	    if (blockStatus->active)
		{
		switch (blockStatus->rightStatus)
		    {
		    case MAF_MISSING_STATUS:
		    case MAF_CONTIG_STATUS:
		    case MAF_TANDEM_STATUS:
		    case MAF_INSERT_STATUS:
		    case MAF_NEW_NESTED_STATUS:
		    case MAF_MAYBE_NEW_NESTED_STATUS:
			AllocVar(mc);
			mc->rightStatus = mc->leftStatus = blockStatus->rightStatus;
			if (mc->rightStatus == MAF_NEW_NESTED_STATUS)
			    mc->rightStatus = MAF_INSERT_STATUS;
			if (mc->leftStatus == MAF_NEW_NESTED_STATUS)
			    mc->leftStatus = MAF_INSERT_STATUS;
			mc->rightLen = mc->leftLen = blockStatus->rightLen;
			mc->src = cloneString(blockStatus->src);
			mc->strand = blockStatus->strand;
			mc->srcSize = blockStatus->srcSize;
			mc->start = blockStatus->start + blockStatus->size ;
			lastMc->next = mc;
			lastMc = mc;
			if  (addN && (blockStatus->rightStatus ==  MAF_MISSING_STATUS))
			    {
			    char buffer[256];

			    safef(buffer, sizeof(buffer), "%s.N",species->name);
			    freez(&mc->src);
			    mc->src = cloneString(buffer);
			    mc->start = 0;
			    mc->srcSize = 200000;
			    mc->size = maf->textSize;
			    mc->text = needMem(mc->size + 1);
			    memset(mc->text, 'N', mc->size);
			    }
			else if (addDash)
			    {
				mc->size = masterMc->size;
			    mc->text = needMem(mc->size + 1);
			    if (mc->size == 0)
				errAbort("bad dash add");
			    memset(mc->text, '-', mc->size);
			    mc->text[mc->size] = 0;
			    mc->size = 0;
			    }
			break;
		    default:
			break;
		    }
		}
	    }
	if (mc)
	    {
	    updateBlockStatus(blockStatus, mc);
	    }
	}

    mafWrite(f, maf);
    mafAliFree(&maf);
    blockNum++;
    }

mafFileFree(&mf2);
}


struct hash *readBed(char *fileName)
/* Read bed and return it as a hash keyed by chromName */
{
char *row[3];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct bedHead *bedHead = NULL;
struct hash *hash = newHash(6);
struct hashEl *hel, *lastHel = NULL;
struct bed3 *bed;

while (lineFileRow(lf, row))
    {
    hel = hashLookup(hash, row[0]);
    if ((lastHel) && (hel != lastHel))
	{
	assert(bedHead != NULL);
	slReverse(&bedHead->list);
	}

    if (hel == NULL)
       {
	char *ptr;

	AllocVar(bedHead);
	ptr = row[0];
	hel = hashAdd(hash, ptr, bedHead);
	}
    bedHead = hel->val;
    AllocVar(bed);
    bed->chrom = hel->name;
    bed->chromStart = lineFileNeedNum(lf, row, 1);
    bed->chromEnd = lineFileNeedNum(lf, row, 2);
    if (bed->chromStart > bed->chromEnd)
        errAbort("start after end line %d of %s", lf->lineIx, lf->fileName);
    slAddHead(&bedHead->list, (struct bed *)bed);
    lastHel = hel;
    }

if (bedHead != NULL)
    slReverse(&bedHead->list);
lineFileClose(&lf);
return hash;
}

void addBed(char *file, struct hash *fileHash)
{
char name[128];

if (!endsWith(file, ".bed"))
    errAbort("filenames in bed list must end in '.bed'");

splitPath(file, NULL, name, NULL);
hashAdd(fileHash, name, readBed(file));
}

void mafAddIRows(char *mafIn, char *twoBitIn,  char *mafOut, char *nBedFile)
/* mafAddIRows - add 'i' rows to a maf, streaming two-pass version. */
{
FILE *f = mustOpen(mafOut, "w");
struct twoBitFile *twoBit = twoBitOpen(twoBitIn);
struct mafFile *mf = mafOpen(mafIn);
struct hash *bedHash = newHash(6);
char *scoring;
struct compInfo *compArray;
long compCount;
struct blockIndex blockIdx;

if (nBedFile != NULL)
    {
    struct lineFile *lf = lineFileOpen(nBedFile, TRUE);
    char *row[1];
    while (lineFileRow(lf, row))
	{
	addBed(row[0], bedHash);
	}
    lineFileClose(&lf);
    }

speciesHash = newHash(6);

/* Pass 1: compact read into flat compInfo array + linkBlock chains */
readMafsCompact(mf, &compArray, &compCount, &blockIdx);
scoring = cloneString(mf->scoring);
mafFileFree(&mf);

verbose(1, "# pass 1 read: %d blocks, %ld components, %d species\n",
    blockIdx.nBlocks, compCount, nextSpeciesIdx);

/* Compute i-row statuses on compArray via linkBlock chain walks */
chainStrands(strandHeads, bedHash, compArray);

/* Free linkBlocks -- no longer needed.  Reclaims ~32 bytes/component. */
freeAllLinkBlocks(strandHeads);

verbose(1, "# chainStrands done, linkBlocks freed\n");

/* Compute bridge/nested statuses directly on compArray */
bridgeSpecies(compArray, &blockIdx, speciesList);

verbose(1, "# bridgeSpecies done, starting pass 2\n");

/* Pass 2: re-read MAF with text, apply statuses, fill holes, write */
mafWriteStart(f, scoring);
streamFillAndWrite(mafIn, compArray, &blockIdx, speciesList, twoBit, f);

freeMem(compArray);
freeBlockIndex(&blockIdx);
freez(&scoring);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *nBedFile;

optionInit(&argc, argv, options);
if (argc != 4)
    usage();

nBedFile = optionVal("nBeds", NULL);
addN = optionExists("addN");
addDash = optionExists("addDash");

mafAddIRows(argv[1], argv[2], argv[3], nBedFile);
return 0;
}
