/* mafAddIRows - Filter out maf files. */
#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"
#include "bed.h"
#include "twoBit.h"

static char const rcsid[] = "$Id: mafAddIRows.c,v 1.4 2005/04/14 02:40:21 braney Exp $";

int redCount;
int blackCount;
int greyCount;

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
    char *species;
    int masterStart, masterEnd;
};


int minBlockOut = 1;

void mafAddIRows(char *mafIn, char *twoBitIn, char *mafOut)
/* mafAddIRows - Filter out maf files. */
{
struct dnaSeq *seq;
FILE *f = mustOpen(mafOut, "w");
FILE *rf = NULL;
int i;
int status;
int rejects = 0;
struct mafFile *mf = mafOpen(mafIn);
struct mafAli *maf = NULL, *prevMaf;
struct hash *statusHash = newHash(10);
struct mafAli *mafList = NULL;
struct mafAli *nextMaf = NULL;
struct hashEl *hashEl;
struct blockStatus *blockStatus;
struct hashCookie cookie;
int lastEnd = 0;
struct twoBitFile *twoBit = twoBitOpen(twoBitIn);

while((maf = mafNext(mf)) != NULL)
    {
    struct mafComp *mc, *prevMc, *masterMc;

    slAddHead(&mafList, maf);
    prevMc = NULL;
    masterMc=maf->components;
    for(mc = maf->components->next; mc; prevMc = mc, mc = mc->next)
	{
	char *chrom = NULL, *species = cloneString(mc->src);

	if (chrom = strchr(species, '.'))
	    *chrom++ = 0;

	if ((blockStatus = hashFindVal(statusHash, species)) == NULL)
	    {
	    AllocVar(blockStatus);
	    blockStatus->strand = '+';
	    blockStatus->species = species;
	    blockStatus->masterStart = masterMc->start;
	    hashAdd(statusHash, species, blockStatus);
	    }
	}
    for(cookie = hashFirst(statusHash),  hashEl = hashNext(&cookie); hashEl; hashEl = hashNext(&cookie))
	{
	char *chrom = NULL, *species;
	blockStatus = hashEl->val;
	species = blockStatus->species;
	
	if ((mc = mafMayFindCompPrefix(maf, species,NULL)) == NULL)
	    {
	    continue;
	    }
	species = cloneString(mc->src);

	if (chrom = strchr(species, '.'))
	    *chrom++ = 0;

	if ((blockStatus->chrom == NULL) || !sameString(chrom, blockStatus->chrom))
	    {
	    if (blockStatus->mc)
		{
		blockStatus->mc->rightStatus =  MAF_NEW_STATUS;
		blockStatus->mc->rightLen = blockStatus->mc->srcSize - blockStatus->end;
		}
	    mc->leftStatus = MAF_NEW_STATUS;
	    mc->leftLen = mc->start;
	    }
	else
	    {

	    if (blockStatus->strand != mc->strand)
		{
		blockStatus->mc->rightStatus = mc->leftStatus = MAF_INVERSE_STATUS;
		blockStatus->mc->rightLen = mc->leftLen = blockStatus->end - mc->start;
		}
	    else if (blockStatus->end > mc->start)
		{
		blockStatus->mc->rightStatus = mc->leftStatus = MAF_DUP_STATUS;
		blockStatus->mc->rightLen = mc->leftLen = blockStatus->end - mc->start;
		}
	    else if (blockStatus->end < mc->start)
		{
		blockStatus->mc->rightStatus = mc->leftStatus = MAF_INSERT_STATUS;
		blockStatus->mc->rightLen = mc->leftLen = -blockStatus->end + mc->start;
		}
	    else 
		{
		blockStatus->mc->rightStatus = mc->leftStatus = MAF_CONTIG_STATUS;
		}
	    }
	    
	blockStatus->mc = mc;
	blockStatus->start = mc->start;
	blockStatus->end = mc->start + mc->size;
	blockStatus->chrom = chrom;
	blockStatus->strand = mc->strand;
	if (masterMc->start + masterMc->size <= blockStatus->masterEnd)
	    ;//printf("bad %d %d\n", masterMc->start + masterMc->size , blockStatus->masterEnd);
	blockStatus->masterEnd = masterMc->start + masterMc->size ;
	}
    }
    for(cookie = hashFirst(statusHash),  hashEl = hashNext(&cookie); hashEl; hashEl = hashNext(&cookie))
	{
	blockStatus = hashEl->val;
	blockStatus->mc->rightStatus = MAF_NEW_STATUS;
	blockStatus->mc->rightLen = 0;
	}
mafFileFree(&mf);
slReverse(&mafList);

lastEnd = 100000000;
prevMaf = NULL;
for(maf = mafList; maf ; prevMaf = maf, maf = nextMaf)
    {
    struct mafComp *mc = NULL, *masterMc, *lastMc = NULL;
    struct mafAli *newMaf = NULL;

    nextMaf = maf->next;

    masterMc=maf->components;
    if (masterMc->start > lastEnd)
	{

	for(cookie = hashFirst(statusHash),  hashEl = hashNext(&cookie); hashEl; hashEl = hashNext(&cookie))
	    {
	    blockStatus = hashEl->val;
	    if (blockStatus->mc)
		{
		switch (blockStatus->mc->rightStatus)
		    {
		    case MAF_CONTIG_STATUS:
		    case MAF_INSERT_STATUS:
		    case MAF_INVERSE_STATUS:
		    case MAF_DUP_STATUS:
			AllocVar(mc);
			mc->rightStatus = mc->leftStatus = blockStatus->mc->rightStatus;
			mc->rightLen = mc->leftLen = blockStatus->mc->rightLen;
			mc->src = blockStatus->mc->src;
			if (lastMc == NULL)
			    {
			    struct mafComp *miniMasterMc = NULL;
			    int ii;
			    char *text, *src;
			    int fullSize;
			    char *seqName;

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

    for(cookie = hashFirst(statusHash),  hashEl = hashNext(&cookie); hashEl; hashEl = hashNext(&cookie))
	{
	blockStatus = hashEl->val;
	
	if ((blockStatus->masterStart <= masterMc->start) && 
	    (blockStatus->masterEnd > masterMc->start) && 
	 ((mc = mafMayFindCompPrefix(maf, blockStatus->species,NULL)) == NULL))
	    {
	    if (blockStatus->mc != NULL)
		{
		switch (blockStatus->mc->rightStatus)
		    {
		    case MAF_CONTIG_STATUS:
		    case MAF_INSERT_STATUS:
		    case MAF_INVERSE_STATUS:
		    case MAF_DUP_STATUS:
			AllocVar(mc);
			mc->rightStatus = mc->leftStatus = blockStatus->mc->rightStatus;
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
fprintf(f, "##maf version=1 scoring=mafAddIRows\n");

for(maf = mafList; maf ; maf = maf->next)
    mafWrite(f, maf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int totalCount;
optionInit(&argc, argv, options);
if (argc < 4)
    usage();

mafAddIRows(argv[1], argv[2], argv[3]);
return 0;
}
