/* mafAddIRows - Filter out maf files. */
#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "chain.h"
#include "options.h"
#include "maf.h"
#include "bed.h"
#include "twoBit.h"

static char const rcsid[] = "$Id: mafAddIRows.c,v 1.6 2005/07/22 23:47:29 braney Exp $";

char *masterSpecies;
char *masterChrom;
struct hash *speciesHash;
struct subSpecies *speciesList;
struct strandHead *strandHeads;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafAddIRows - add 'i' rows to a maf\n"
  "usage:\n"
  "   mafAddIRows mafIn twoBitFile mafOut\n"
  "options:\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct blockStatus
{
    char *chrom;
    int start, end;
    char strand;
    struct mafComp *mc;
    int masterStart, masterEnd;
};

struct subSpecies
{
    struct subSpecies *next;
    char *name;
    struct hash *hash;
    struct blockStatus blockStatus;
};

struct linkBlock
{
    struct linkBlock *next;
    struct cBlock cb;
    struct mafComp *mc;
};

struct strandHead
{
    struct strandHead *next;
    char *name;
    char *species;
    struct linkBlock *links;
};

struct mafAli *readMafs(struct mafFile *mf)
{
struct mafAli *maf;
char buffer[2048];
char buffer2[2048];
struct strandHead *strandHead;
struct mafAli *mafList = NULL;

while((maf = mafNext(mf)) != NULL)
    {
    struct mafComp *mc, *masterMc = maf->components;
    char *species = buffer;
    char *chrom;

    strcpy(species, masterMc->src);
    chrom = strchr(species,'.');
    *chrom++ = 0;
    if (masterSpecies == NULL)
	{
	masterSpecies = cloneString(species);
	masterChrom = cloneString(chrom);
	//printf("master %s %s\n",masterSpecies,masterChrom);
	}
    else
	{
	if (!sameString(masterSpecies, species))
	    errAbort("first species (%s) not master species (%s)\n",species,masterSpecies);
	}

    for(mc= masterMc->next; mc; mc = mc->next)
	{
	struct hash *strandHash;
	struct linkBlock *linkBlock;
	struct subSpecies *subSpecies = NULL;

	strcpy(species, mc->src);
	chrom = strchr(species,'.');
	*chrom++ = 0;

	if ((subSpecies = hashFindVal(speciesHash, species)) == NULL)
	    {
	//    printf("new species %s\n",species);
	    AllocVar(subSpecies);
	    subSpecies->name = cloneString(species);
	    subSpecies->hash = newHash(0);
	    subSpecies->blockStatus.strand = '+';
	    subSpecies->blockStatus.masterStart = masterMc->start;
	    slAddHead(&speciesList, subSpecies);
	    hashAdd(speciesHash, species, subSpecies);
	    }
	subSpecies->blockStatus.masterEnd = masterMc->start + masterMc->size ;
	sprintf(buffer2, "%s%c%s", masterChrom,mc->strand,chrom);
	if ((strandHead = hashFindVal(subSpecies->hash, buffer2)) == NULL)
	    {
	    //printf("new strand %s for species %s\n",buffer2, species);
	    AllocVar(strandHead);
	    hashAdd(subSpecies->hash, buffer2, strandHead);
	    strandHead->name = cloneString(buffer2);
	    strandHead->species = cloneString(species);
	    slAddHead(&strandHeads, strandHead);
	    }
	AllocVar(linkBlock);
	linkBlock->mc = mc;
	linkBlock->cb.qStart = mc->start;
	linkBlock->cb.qEnd = mc->start + mc->size;
	linkBlock->cb.tStart = masterMc->start;
	linkBlock->cb.tEnd = masterMc->start + masterMc->size;


	slAddHead(&strandHead->links, linkBlock);
	}
    slAddHead(&mafList, maf);
    }
slReverse(&mafList);

return mafList;
}

void chainStrands(struct strandHead *strandHead)
{
for(; strandHead ; strandHead = strandHead->next)
    {
    struct linkBlock *link, *prevLink;
    int lastEnd;

    //printf("chaining %s %s\n",strandHead->name, strandHead->species);
    slReverse(&strandHead->links);

    prevLink = strandHead->links;
    prevLink->mc->leftStatus = MAF_NEW_STATUS;
    //printf("new %d %d %d %d\n",prevLink->cb.tStart,prevLink->cb.tEnd,prevLink->cb.qStart,prevLink->cb.qEnd);
    for(link = prevLink->next; link; prevLink = link, link = link->next)
	{
	int tDiff = link->cb.tStart - prevLink->cb.tEnd;
	int qDiff = link->cb.qStart - prevLink->cb.qEnd;

	if  ((tDiff > 100000) || ((qDiff < -1000) || (qDiff > 100000)))
	    {
	//printf("new %d %d %d %d\n",link->cb.tStart,link->cb.tEnd,link->cb.qStart,link->cb.qEnd);
	    prevLink->mc->rightStatus = link->mc->leftStatus = MAF_NEW_STATUS;
	    prevLink->mc->rightLen = link->mc->leftLen = 0;
	    }
	else if (qDiff == 0)
	    {
	    prevLink->mc->rightStatus = link->mc->leftStatus = MAF_CONTIG_STATUS;
	    prevLink->mc->rightLen = link->mc->leftLen = 0;
	    }
	else
	    {
	    //printf("cont %d %d %d %d\n",link->cb.tStart,link->cb.tEnd,link->cb.qStart,link->cb.qEnd);
	    prevLink->mc->rightStatus = link->mc->leftStatus = MAF_INSERT_STATUS;
	    prevLink->mc->rightLen = link->mc->leftLen = qDiff;
	    }
	}
    prevLink->mc->rightStatus = MAF_NEW_STATUS;
    }
}

void bridgeSpecies(struct mafAli *mafList, struct subSpecies *subSpecies)
{
struct mafAli *maf;
int pushState, leftLen;
struct mafComp *masterMc, *mc;

for(; subSpecies; subSpecies = subSpecies->next)
    {
    //printf("bridging %s\n",subSpecies->name);
    pushState = 0;
    leftLen = 0;
    for(maf = mafList; maf ;  maf = maf->next)
	{
	masterMc = maf->components;
	if ((mc = mafMayFindCompPrefix(maf, subSpecies->name,NULL)) == NULL)
	    {
	    continue;
	    }
	if (mc->leftStatus == 0) 
	    errAbort("zero left status\n");
	if (mc->leftStatus == MAF_NEW_STATUS)
	    {
	    if (pushState)
		{
		//printf("bridged on %s: %s:%d-%d\n",mc->src,"chr22",masterMc->start, masterMc->start + masterMc->size);
		mc->leftStatus = MAF_NEW_NESTED_STATUS;
		mc->leftLen = leftLen;
		}
	    pushState++;
	    }

	if (mc->rightStatus == 0)
	    {
	    errAbort("zero right status\n");
	    }
	else if ((mc->rightStatus == MAF_CONTIG_STATUS) || (mc->rightStatus == MAF_INSERT_STATUS))
	    leftLen = mc->rightLen;
	else if (mc->rightStatus == MAF_NEW_STATUS)
	    {
	    pushState--;
	    if (pushState)
		{
		//printf("arched on %s: %s:%d-%d\n",mc->src,"chr22",masterMc->start, masterMc->start + masterMc->size);
		mc->rightStatus = MAF_NEW_NESTED_STATUS;
		mc->rightLen = leftLen;
		}
	    }
	}
    //printf("pushState %d\n",pushState);
    }
}

void fillHoles(struct mafAli *mafList, struct subSpecies *speciesList, struct twoBitFile *twoBit)
{
int lastEnd = 100000000;
struct mafAli *prevMaf = NULL, *maf, *nextMaf;
struct subSpecies *species;
struct blockStatus *blockStatus;

/*
for(species = speciesList; species; species = species->next)
    {
    blockStatus = &species->blockStatus;
    blockStatus->mc->rightStatus = MAF_NEW_STATUS;
    blockStatus->mc->rightLen = 0;
    }
    */

for(maf = mafList; maf ; prevMaf = maf, maf = nextMaf)
    {
    struct mafComp *mc = NULL, *masterMc, *lastMc = NULL;
    struct mafAli *newMaf = NULL;
    struct blockStatus *blockStatus;

    nextMaf = maf->next;

    masterMc=maf->components;
    if (masterMc->start > lastEnd)
	{
	struct subSpecies *species;

	for(species = speciesList; species; species = species->next)
	    {
	    blockStatus = &species->blockStatus;
	    if (blockStatus->mc)
		{
		switch (blockStatus->mc->rightStatus)
		    {
		    case MAF_NEW_NESTED_STATUS:
		    case MAF_MAYBE_NEW_NESTED_STATUS:
		    case MAF_CONTIG_STATUS:
		    case MAF_INSERT_STATUS:
			AllocVar(mc);
			mc->rightStatus = mc->leftStatus = blockStatus->mc->rightStatus;
			mc->rightLen = mc->leftLen = blockStatus->mc->rightLen;
			mc->src = blockStatus->mc->src;
			if (lastMc == NULL)
			    {
			    struct mafComp *miniMasterMc = NULL;
			    char *seqName;
			    struct dnaSeq *seq;

			    AllocVar(miniMasterMc);
			    miniMasterMc->next = mc;
			    miniMasterMc->strand = '+';
			    miniMasterMc->srcSize = masterMc->srcSize;
			    miniMasterMc->src = masterMc->src;
			    miniMasterMc->start = lastEnd;
			    miniMasterMc->size =  masterMc->start - lastEnd;

			    if (seqName = strchr(miniMasterMc->src, '.'))
				seqName++;
			    else 
			    	seqName = miniMasterMc->src;

			    //printf("hole filled from %d to %d\n",lastEnd, masterMc->start);
			    seq = twoBitReadSeqFrag(twoBit, seqName, lastEnd, masterMc->start);
			    miniMasterMc->text = seq->dna;

			    AllocVar(newMaf);
			    newMaf->textSize = maf->textSize;
			    newMaf->components = miniMasterMc;
			    newMaf->next = maf;
			    if (prevMaf)
				prevMaf->next = newMaf;
			    else
				mafList = newMaf;
			    }
			else
			    lastMc->next = mc;
			lastMc = mc;
			break;
		    }
		}
	    }
	}
    lastEnd = masterMc->start + masterMc->size;
    for(lastMc = masterMc; lastMc->next; lastMc = lastMc->next)
	;

    for(species = speciesList; species; species = species->next)
	{
	blockStatus = &species->blockStatus;
	
	if ((blockStatus->masterStart <= masterMc->start) && 
	    (blockStatus->masterEnd > masterMc->start) && 
	 ((mc = mafMayFindCompPrefix(maf, species->name,NULL)) == NULL))
	    {
	    if (blockStatus->mc != NULL)
		{
		switch (blockStatus->mc->rightStatus)
		    {
		    case MAF_CONTIG_STATUS:
		    case MAF_INSERT_STATUS:
		    case MAF_NEW_NESTED_STATUS:
		    case MAF_MAYBE_NEW_NESTED_STATUS:
			AllocVar(mc);
			mc->rightStatus = mc->leftStatus = blockStatus->mc->rightStatus;
			if (mc->rightStatus == MAF_NEW_NESTED_STATUS)
			    mc->rightStatus = MAF_INSERT_STATUS;
			if (mc->leftStatus == MAF_NEW_NESTED_STATUS)
			    mc->leftStatus = MAF_INSERT_STATUS;
			mc->rightLen = mc->leftLen = blockStatus->mc->rightLen;
			mc->src = blockStatus->mc->src;
			lastMc->next = mc;
			lastMc = mc;
			break;
		    default:
			break;
		    }
		}
	    }
	if (mc)
	    blockStatus->mc = mc;
	}
    }
}

void mafAddIRows(char *mafIn, char *twoBitIn,  char *mafOut)
/* mafAddIRows - Filter out maf files. */
{
FILE *f = mustOpen(mafOut, "w");
struct twoBitFile *twoBit = twoBitOpen(twoBitIn);
struct mafAli *mafList, *maf;
struct mafFile *mf = mafOpen(mafIn);

speciesHash = newHash(0);
mafList = readMafs(mf);
mafWriteStart(f, mf->scoring);
mafFileFree(&mf);

chainStrands(strandHeads);
bridgeSpecies(mafList, speciesList);
fillHoles(mafList, speciesList, twoBit);


for(maf = mafList; maf ; maf = maf->next)
    mafWrite(f, maf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int totalCount;
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

mafAddIRows(argv[1], argv[2], argv[3]);
return 0;
}
