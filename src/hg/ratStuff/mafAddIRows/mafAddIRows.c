/* mafAddIRows - Filter out maf files. */
#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"
#include "bed.h"

static char const rcsid[] = "$Id: mafAddIRows.c,v 1.1 2005/04/06 18:43:25 braney Exp $";

int redCount;
int blackCount;
int greyCount;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafAddIRows - convert maf to valBed\n"
  "usage:\n"
  "   mafAddIRows file(s).maf\n"
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

void mafAddIRows(char *mafIn, char *mafOut)
/* mafAddIRows - Filter out maf files. */
{
FILE *f = mustOpen(mafOut, "w");
FILE *rf = NULL;
int i;
int status;
int rejects = 0;
struct mafFile *mf = mafOpen(mafIn);
struct mafAli *maf = NULL;
struct hash *statusHash = newHash(10);
struct mafAli *mafList = NULL;
struct hashEl *hashEl;
struct blockStatus *blockStatus;
struct hashCookie cookie;

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
	//    blockStatus->chrom = cloneString(chrom);
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
	    //printf("not found %s\n",species);
	    continue;
	    }

	//printf("species %s \n",species);
	if ((blockStatus->chrom == NULL) || !sameString(chrom, blockStatus->chrom))
	    {
	    if (blockStatus->mc)
		{
		blockStatus->mc->rightStatus =  NEW_STATUS;
		blockStatus->mc->rightLen = blockStatus->mc->srcSize - blockStatus->end;
		}
	    mc->leftStatus = NEW_STATUS;
	    mc->leftLen = mc->start;
	    //printf("newBefore %s\n",chrom);
	    }
	else
	    {

	    if (blockStatus->end > mc->start)
		{
		blockStatus->mc->rightStatus = mc->leftStatus = DUP_STATUS;
		blockStatus->mc->rightLen = mc->leftLen = blockStatus->end - mc->start;
		//printf("sameStrand %d ",blockStatus->end - mc->start);
		}
	    else if (blockStatus->end < mc->start)
		{
		blockStatus->mc->rightStatus = mc->leftStatus = INSERT_STATUS;
		blockStatus->mc->rightLen = mc->leftLen = -blockStatus->end + mc->start;
		//printf("insert %s %d ",mc->src,-blockStatus->end + mc->start);
		}
	    else 
		{
		blockStatus->mc->rightStatus = mc->leftStatus = CONTIG_STATUS;
		}
	    }
	///printf("|\n");
	    
	blockStatus->mc = mc;
	blockStatus->start = mc->start;
	blockStatus->end = mc->start + mc->size;
	blockStatus->chrom = chrom;
	blockStatus->strand = mc->strand;
	if (masterMc->start + masterMc->size <= blockStatus->masterEnd)
	    printf("bad %d %d\n", masterMc->start + masterMc->size , blockStatus->masterEnd);
	blockStatus->masterEnd = masterMc->start + masterMc->size ;
	}
    }
    for(cookie = hashFirst(statusHash),  hashEl = hashNext(&cookie); hashEl; hashEl = hashNext(&cookie))
	{
	blockStatus = hashEl->val;
	blockStatus->mc->rightStatus = NEW_STATUS;
	blockStatus->mc->rightLen = 0;
	}
mafFileFree(&mf);
slReverse(&mafList);

for(maf = mafList; maf ; maf = maf->next)
    {
    struct mafComp *mc = NULL, *masterMc, *lastMc;
    /*
    char *chrom = NULL, *species = cloneString(mc->src);

    if (chrom = strchr(species, '.'))
	*chrom++ = 0;
	*/

    masterMc=maf->components;
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
		    case CONTIG_STATUS:
		    case INSERT_STATUS:
		    case DUP_STATUS:
		    default:
			printf("insert i rwo black %c %d\n",blockStatus->mc->rightStatus,blockStatus->mc->rightLen);
			AllocVar(mc);
			mc->rightStatus = mc->leftStatus = blockStatus->mc->rightStatus;
			mc->rightLen = mc->leftLen = blockStatus->mc->rightLen;
			mc->src = blockStatus->mc->src;
			lastMc->next = mc;
			lastMc = mc;
			break;
		    }
		}
		else
			printf("first %s\n",blockStatus->species);
	    }
	if (mc)
	    blockStatus->mc = mc;
	}
    }
for(maf = mafList; maf ; maf = maf->next)
    mafWrite(f, maf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int totalCount;
optionInit(&argc, argv, options);
if (argc < 3)
    usage();

mafAddIRows(argv[1], argv[2]);
return 0;
}
