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
#include "binRange.h"

static char const rcsid[] = "$Id: mafAddIRows.c,v 1.19 2008/10/27 17:54:53 braney Exp $";

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
  "mafAddIRows - add 'i' rows to a maf\n"
  "usage:\n"
  "   mafAddIRows mafIn twoBitFile mafOut\n"
  "WARNING:  requires a maf with only a single target sequence\n"
  "options:\n"
  "   -nBeds=listOfBedFiles\n"
  "       reads in list of bed files, one per species, with N locations\n"
  "   -addN\n"
  "       adds rows of N's into maf blocks (rather than just annotating them)\n"
  "   -addDash\n"
  "       adds rows of -'s into maf blocks (rather than just annotating them)\n"
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
    char strand;
    char *name;
    char *qName;
    int qSize;
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
char *ourChrom = NULL;

while((maf = mafNext(mf)) != NULL)
    {
    struct mafComp *mc, *masterMc = maf->components;
    char *species = buffer;
    char *chrom;

    if (ourChrom == NULL)
	ourChrom = masterMc->src;
    else 
	{
	if (differentString(masterMc->src, ourChrom))
	    errAbort("ERROR: mafAddIrows requires maf have only one target sequence.\n"
		"Use mafSplit -byTarget -useFullSequenceName to split maf");
	}

    strcpy(species, masterMc->src);
    chrom = strchr(species,'.');
    if (chrom)
	*chrom++ = 0;
    else
	errAbort("reference species has no chrom name\n");

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
	struct linkBlock *linkBlock;
	struct subSpecies *subSpecies = NULL;

	strcpy(species, mc->src);
	chrom = strchr(species,'.');
	*chrom++ = 0;

	if ((subSpecies = hashFindVal(speciesHash, species)) == NULL)
	    {
	    //printf("new species %s\n",species);
	    AllocVar(subSpecies);
	    subSpecies->name = cloneString(species);
	    subSpecies->hash = newHash(6);
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
	    strandHead->qName = cloneString(chrom);
	    strandHead->qSize = mc->srcSize;
	    strandHead->strand = mc->strand;
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

void chainStrands(struct strandHead *strandHead, struct hash *bedHash)
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
    prevLink->mc->leftStatus = MAF_NEW_STATUS;
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
	    prevLink->mc->rightStatus = link->mc->leftStatus = MAF_MISSING_STATUS;
	    prevLink->mc->rightLen = link->mc->leftLen = nCount;
	    }
	else if  ((tDiff > 100000) ||  
		  (qDiff > 100000) || (qDiff < -100000))
	    {
	    prevLink->mc->rightStatus = link->mc->leftStatus = MAF_NEW_STATUS;
	    prevLink->mc->rightLen = link->mc->leftLen = 0;
	    }
	else if  (qDiff < 0)
	    {
	    prevLink->mc->rightStatus = link->mc->leftStatus = MAF_TANDEM_STATUS;
	    prevLink->mc->rightLen = link->mc->leftLen = -qDiff;
	    }
	else if (qDiff == 0)
	    {
	    prevLink->mc->rightStatus = link->mc->leftStatus = MAF_CONTIG_STATUS;
	    prevLink->mc->rightLen = link->mc->leftLen = 0;
	    }
	else
	    {
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
struct mafComp *masterMc, *mc, *prevMc;

prevMc = mc = NULL;
for(; subSpecies; subSpecies = subSpecies->next)
    {
    //printf("bridging %s\n",subSpecies->name);
    pushState = 0;
    leftLen = 0;
    for(maf = mafList; maf ;  maf = maf->next)
	{
	masterMc = maf->components;
	prevMc = mc;
	if ((mc = mafMayFindCompPrefix(maf, subSpecies->name,NULL)) == NULL)
	    {
	    continue;
	    }
	if (mc->leftStatus == 0) 
	    errAbort("zero left status %s:%d\n",mc->src, mc->start);
#ifdef NOTNOW
	if (pushState && (mc->leftStatus == MAF_INSERT_STATUS))
	    {
	    if (prevMc && 
		!((prevMc->rightStatus == mc->leftStatus) &&
		(prevMc->rightLen == mc->leftLen)))
		{
		printf("boo\n");
		mc->leftStatus = MAF_NEW_NESTED_STATUS;
		mc->leftLen = 0;
		}
	    }
#endif
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
	else if (isContigOrTandem(mc->rightStatus) || (mc->rightStatus == MAF_INSERT_STATUS))
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
	    mc = NULL;
//	    printf("looking at %s\n",species->name);
	    blockStatus = &species->blockStatus;
	    if (blockStatus->mc)
		{
//	    printf("should match at %s\n",blockStatus->mc->src);
		switch (blockStatus->mc->rightStatus)
		    {
		    case MAF_MISSING_STATUS:
		    //printf("missing right\n");
		    case MAF_NEW_NESTED_STATUS:
		    case MAF_MAYBE_NEW_NESTED_STATUS:
		    case MAF_CONTIG_STATUS:
		    case MAF_TANDEM_STATUS:
		    case MAF_INSERT_STATUS:
			AllocVar(mc);
			mc->rightStatus = mc->leftStatus = blockStatus->mc->rightStatus;
			mc->rightLen = mc->leftLen = blockStatus->mc->rightLen;
			mc->src = blockStatus->mc->src;
			mc->srcSize = blockStatus->mc->srcSize;
			mc->strand = blockStatus->mc->strand;
			mc->start = blockStatus->mc->start + blockStatus->mc->size;
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

			    if ((seqName = strchr(miniMasterMc->src, '.')) != NULL)
				seqName++;
			    else 
			    	seqName = miniMasterMc->src;

//			    printf("hole filled from %d to %d\n",lastEnd, masterMc->start);
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
			    //masterMc = miniMasterMc;
			    }
			else
			    {
			    lastMc->next = mc;
			    }
			lastMc = mc;
			if  (blockStatus->mc->rightStatus ==  MAF_MISSING_STATUS)
			    {
			    if (addN)
				{
				char buffer[256];

				safef(buffer, sizeof(buffer), "%s.N",species->name);
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
				mc->srcSize = blockStatus->mc->srcSize;
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
	}
    lastEnd = masterMc->start + masterMc->size;
    for(lastMc = masterMc; lastMc->next; lastMc = lastMc->next)
	;

    for(species = speciesList; species; species = species->next)
	{
	blockStatus = &species->blockStatus;
	
	mc = NULL;
	if ((blockStatus->masterStart <= masterMc->start) && 
	    (blockStatus->masterEnd > masterMc->start) && 
	 ((mc = mafMayFindCompPrefix(maf, species->name,NULL)) == NULL))
	    {
	    if (blockStatus->mc != NULL)
		{
		switch (blockStatus->mc->rightStatus)
		    {
		    case MAF_MISSING_STATUS:
		    case MAF_CONTIG_STATUS:
		    case MAF_TANDEM_STATUS:
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
			mc->strand = blockStatus->mc->strand;
			mc->srcSize = blockStatus->mc->srcSize;
			mc->start = blockStatus->mc->start + blockStatus->mc->size ;
			lastMc->next = mc;
			lastMc = mc;
			if  (addN && (blockStatus->mc->rightStatus ==  MAF_MISSING_STATUS))
			    {
			    char buffer[256];

			    safef(buffer, sizeof(buffer), "%s.N",species->name);
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
	    blockStatus->mc = mc;
	    }
	}
    }
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
	if ((ptr = strchr(row[0], '.')) != NULL)
	    ptr++;
	else 
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
/* mafAddIRows - Filter out maf files. */
{
FILE *f = mustOpen(mafOut, "w");
struct twoBitFile *twoBit = twoBitOpen(twoBitIn);
struct mafAli *mafList, *maf;
struct mafFile *mf = mafOpen(mafIn);
struct hash *bedHash = newHash(6); 

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
mafList = readMafs(mf);
mafWriteStart(f, mf->scoring);
mafFileFree(&mf);

chainStrands(strandHeads, bedHash);
bridgeSpecies(mafList, speciesList);
fillHoles(mafList, speciesList, twoBit);

for(maf = mafList; maf ; maf = maf->next)
    mafWrite(f, maf);
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
