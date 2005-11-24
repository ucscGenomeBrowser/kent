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

static char const rcsid[] = "$Id: mafAddIRows.c,v 1.8 2005/11/24 17:13:55 braney Exp $";

char *masterSpecies;
char *masterChrom;
struct hash *speciesHash;
struct subSpecies *speciesList;
struct strandHead *strandHeads;

boolean addN = FALSE;
boolean addDash = FALSE;

struct bed3 
/* A three field bed. */
    {
    struct bed3 *next;
    char *chrom;	/* Allocated in hash. */
    int start;		/* Start (0 based) */
    int end;		/* End (non-inclusive) */
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafAddIRows - add 'i' rows to a maf\n"
  "usage:\n"
  "   mafAddIRows mafIn twoBitFile mafOut\n"
  "options:\n"
  "   -sizes=listOfChromSizes\n"
  "       where listOfChromSizes is a list of chrom.sizes files named with\n"
  "       the species name in the maf\n"
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
   {"sizes", OPTION_STRING},
   {"addN", OPTION_BOOLEAN},
   {"addDash", OPTION_BOOLEAN},
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

while((maf = mafNext(mf)) != NULL)
    {
    struct mafComp *mc, *masterMc = maf->components;
    char *species = buffer;
    char *chrom;

    strcpy(species, masterMc->src);
    chrom = strchr(species,'.');
    if (chrom)
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
	    //printf("new species %s\n",species);
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
    int lastEnd;
    struct hashEl *hel = hashLookup(bedHash, strandHead->species);
    struct hash *chromHash = (hel != NULL) ? hel->val : NULL;
    struct binKeeper *bk = (chromHash != NULL) ? hashFindVal(chromHash, strandHead->qName): NULL;

    //if (chromHash == NULL)
    //fprintf(stderr,"bk for %s %s %x\n",strandHead->species,strandHead->qName,bk);
    //printf("chaining %s %s\n",strandHead->name, strandHead->species);
    slReverse(&strandHead->links);

    prevLink = strandHead->links;
    prevLink->mc->leftStatus = MAF_NEW_STATUS;
    //printf("new %d %d %d %d\n",prevLink->cb.tStart,prevLink->cb.tEnd,prevLink->cb.qStart,prevLink->cb.qEnd);
    for(link = prevLink->next; link; prevLink = link, link = link->next)
	{
	int tDiff = link->cb.tStart - prevLink->cb.tEnd;
	int qDiff = link->cb.qStart - prevLink->cb.qEnd;
	struct binElement *hitList = NULL, *hit;
	int nCount = 0;
	int nStart, nEnd;

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
	if (bk != NULL)
	    hitList = binKeeperFind(bk, nStart, nEnd);
	//if (hitList && hitList->next)
	    //printf("more than one N region in gap. Fix me");
	for (hit = hitList; hit != NULL; hit = hit->next)
	    nCount += positiveRangeIntersection(nStart, nEnd, hit->start, hit->end);
	slFreeList(&hitList);
//	    if (nCount)
//	    printf("found %d N's in gap (%d) %c %d %d\n",nCount, qDiff,strandHead->strand, nStart, nEnd);

	//tDiff -= nCount;
	//qDiff -= nCount;

	if ((qDiff && (100 * nCount / qDiff > 95))
		&& (tDiff && (100 * nCount / tDiff > 10)))
	    {
	    //printf("Nfill %s qDiff %d nCount %d %d %d\n",strandHead->species,qDiff,nCount, prevLink->cb.tEnd, link->cb.tStart);
	    prevLink->mc->rightStatus = link->mc->leftStatus = MAF_MISSING_STATUS;
	    prevLink->mc->rightLen = link->mc->leftLen = nCount;
	    }
	//else if  ((tDiff > 100000) || ((qDiff < -1000) || (qDiff > 100000)))
	else if  ((tDiff > 100000) || ((qDiff < 0) || (qDiff > 100000)))
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

			    if (seqName = strchr(miniMasterMc->src, '.'))
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
			    }
			else
			    {
//			    printf("no fill\n");
			    lastMc->next = mc;
			    }
			lastMc = mc;
			if  (blockStatus->mc->rightStatus ==  MAF_MISSING_STATUS)
			    {
			    //mc->size = maf->textSize;
//			    printf("back filling %s\n",species->name);
//			    printf("blockStatus %s\n",mc->src);
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
			    //mc->size = maf->textSize;
			    //char buffer[256];

			    //safef(buffer, sizeof(buffer), "%s.dash",species->name);
			    //mc->src = cloneString(buffer);
			    //mc->start = 0;
			    //mc->srcSize = 200000;
			    //mc->size =  masterMc->start - lastEnd;

			    if (addDash)
				{
				mc->srcSize = blockStatus->mc->srcSize;
				mc->text = needMem(mc->size + 1);
				memset(mc->text, '-', mc->size);
				mc->size = 0;
				}
			    //else mc->srcSize =  0;
			    }
			break;
		    }
		}
	    }
	//if (mc)
	    //blockStatus->mc = mc;
	}
    lastEnd = masterMc->start + masterMc->size;
    for(lastMc = masterMc; lastMc->next; lastMc = lastMc->next)
	;

    for(species = speciesList; species; species = species->next)
	{
	blockStatus = &species->blockStatus;
	
//printf("second round species %s\n",species->name);
	mc = NULL;
	if ((blockStatus->masterStart <= masterMc->start) && 
	    (blockStatus->masterEnd > masterMc->start) && 
	 ((mc = mafMayFindCompPrefix(maf, species->name,NULL)) == NULL))
	    {
	    //printf("don't have mc for %s\n",species->name);
	    if (blockStatus->mc != NULL)
		{
	    //printf("have blockstatus for %s\n",blockStatus->mc->src);
		switch (blockStatus->mc->rightStatus)
		    {
		    case MAF_MISSING_STATUS:
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
			//mc->leftLen = blockStatus->mc->start + blockStatus->mc->size;
			//mc->rightLen = blockStatus->mc->rightLen;
			mc->src = blockStatus->mc->src;
			mc->strand = blockStatus->mc->strand;
			mc->srcSize = blockStatus->mc->srcSize;
			mc->start = blockStatus->mc->start + blockStatus->mc->size ;
			lastMc->next = mc;
			lastMc = mc;
			if  (addN && blockStatus->mc->rightStatus ==  MAF_MISSING_STATUS)
			    {
			    char buffer[256];
			    int ii;

			    //printf("filling %s\n",species->name);
			    safef(buffer, sizeof(buffer), "%s.N",species->name);
			    mc->src = cloneString(buffer);
			    mc->start = 0;
			    mc->srcSize = 200000;
			    mc->size = maf->textSize;
			   // mc->size =  masterMc->size;
			    mc->text = needMem(mc->size + 1);
			    //for(ii=0; ii < mc->size; ii++)
			//	{
			//	if (masterMc->text[ii] == '-')
			//	    mc->text[ii] = '-';
			//	else
			//	    mc->text[ii] = 'N';
			//	}
			    memset(mc->text, 'N', mc->size);
			    }
			else if (addDash)
			    {
			    //char buffer[256];

			    //safef(buffer, sizeof(buffer), "%s.dash",species->name);
			    //mc->src = cloneString(buffer);
			    //mc->start = 0;
			    //mc->srcSize = 200000;
			   // mc->size =  masterMc->size;
			    mc->size = maf->textSize;
			    mc->text = needMem(mc->size + 1);
			    memset(mc->text, '-', mc->size);
			    mc->size = 0;
			    }
			//else
			    //mc->srcSize = 0;
			    
			break;
		    default:
			break;
		    }
		}
	    }
	    //else if (mc) printf("have mc %s for species %s\n",mc->src,species->name);
	    //else printf("no mc, no nothing\n");
	if (mc)
	{
	    //printf("on species %s adding mc %s \n",species->name,mc->src);
	    blockStatus->mc = mc;
	}
	}
    }
}

struct hash *readSize(char *fileName)
{
char *row[2];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct binKeeper *bk;
struct hash *hash = newHash(0);
struct hashEl *hel;
struct bed3 *bed;
int size;

while (lineFileRow(lf, row))
    {
    hel = hashLookup(hash, row[0]);
    if (hel == NULL)
	hashAdd(hash, row[0], cloneString(row[1]));
    else
	errAbort("already have size for %s\n",row[0]);
    }
lineFileClose(&lf);
return hash;
}
struct hash *readBed(char *fileName, struct hash *sizeHash)
/* Read bed and return it as a hash keyed by chromName
 * with binKeeper values. */
{
char *row[3];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct binKeeper *bk;
struct hash *hash = newHash(0);
struct hashEl *hel;
struct bed3 *bed;
int size;

while (lineFileRow(lf, row))
    {
    hel = hashLookup(hash, row[0]);
    if (hel == NULL)
       {
	if (sizeHash == NULL)
	    errAbort("reading %s don't have size for %s\n",fileName,row[0]);
	size = atoi(hashFindVal(sizeHash, row[0]));
	//printf("got %d for %s\n",size,row[0]);
	bk = binKeeperNew(0, size);
	hel = hashAdd(hash, row[0], bk);
	}
    bk = hel->val;
    AllocVar(bed);
    bed->chrom = hel->name;
    bed->start = lineFileNeedNum(lf, row, 1);
    bed->end = lineFileNeedNum(lf, row, 2);
    if (bed->start > bed->end)
        errAbort("start after end line %d of %s", lf->lineIx, lf->fileName);
    binKeeperAdd(bk, bed->start, bed->end, bed);
    }
lineFileClose(&lf);
return hash;
}

void addSize(char *file, struct hash *fileHash)
{
char name[128];

if (!endsWith(file, ".len"))
    errAbort("filenames in size list must end in '.len'");
splitPath(file, NULL, name, NULL);
hashAdd(fileHash, name, readSize(file));
}

void addBed(char *file, struct hash *fileHash, struct hash *sizeFileHash)
{
char name[128];
struct hash *sizeHash;

if (!endsWith(file, ".bed"))
    errAbort("filenames in bed list must end in '.bed'");

splitPath(file, NULL, name, NULL);
sizeHash = hashFindVal(sizeFileHash,name);
hashAdd(fileHash, name, readBed(file,sizeHash));
}

void mafAddIRows(char *mafIn, char *twoBitIn,  char *mafOut, char *nBedFile,
		    char *sizeFile)
/* mafAddIRows - Filter out maf files. */
{
FILE *f = mustOpen(mafOut, "w");
struct twoBitFile *twoBit = twoBitOpen(twoBitIn);
struct mafAli *mafList, *maf;
struct mafFile *mf = mafOpen(mafIn);
struct hash *bedHash = newHash(0); 
struct hash *sizeFileHash = newHash(0); 

if (sizeFile != NULL)
    {
    struct lineFile *lf = lineFileOpen(sizeFile, TRUE);
    char *row[1];
    while (lineFileRow(lf, row))
	addSize(row[0], sizeFileHash);
    lineFileClose(&lf);
    }

if (nBedFile != NULL)
    {
    struct lineFile *lf = lineFileOpen(nBedFile, TRUE);
    char *row[1];
    while (lineFileRow(lf, row))
	addBed(row[0], bedHash, sizeFileHash);
    lineFileClose(&lf);
    }

speciesHash = newHash(0);
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
int totalCount;
char *nBedFile;
char *sizeFile;

optionInit(&argc, argv, options);
if (argc != 4)
    usage();

sizeFile = optionVal("sizes", NULL);
nBedFile = optionVal("nBeds", NULL);
addN = optionExists("addN");
addDash = optionExists("addDash");

if (nBedFile && (sizeFile == NULL))
    errAbort("sizes file list must be specified if nBed file list is\n");

mafAddIRows(argv[1], argv[2], argv[3], nBedFile,sizeFile);
return 0;
}
