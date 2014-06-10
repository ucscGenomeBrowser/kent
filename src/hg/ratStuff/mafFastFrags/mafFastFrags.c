/* mafFastFrags - Extract pieces of multiple alignment, bypassing database.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "rangeTree.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafFastFrags - Extract pieces of multiple alignment, bypassing database.\n"
  "usage:\n"
  "   mafFastFrags in.maf in.ix in.bed out.maf\n"
  "THIS DOES NOT WORK YET.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct rbTree *indexToTree(char *indexFile)
/* Read in index file and make a size_t-valued range tree from it. */
{
struct lineFile *lf = lineFileOpen(indexFile, TRUE);
struct rbTree *rangeTree = rangeTreeNew();
char *row[3];
while (lineFileRow(lf, row))
    {
    int start = lineFileNeedNum(lf, row, 0);
    int end = lineFileNeedNum(lf, row, 1);
    size_t offset = atol(row[2]);
    struct range *range = rangeTreeAdd(rangeTree, start, end);
    range->val = sizetToPt(offset);
    }
lineFileClose(&lf);
return rangeTree;
}

#ifdef SOON
struct mafAli *mafAliOverlappingRegion(struct mafFile *mf, struct rbTree *rangeTree,
	int start, int end)
/* Get list of all mafs that overlap region. */
{
}

struct mafAli *mafFromBed12(struct mafFile *mf, struct rbTree *rangeTree, 
	struct bed *bed, struct slName *orgList)
/* Construct a maf out of exons in bed. */
{
/* Loop through all block in bed, collecting a list of mafs, one
 * for each block.  While we're at make a hash of all species seen. */
struct hash *speciesHash = hashNew(0);
struct mafAli *mafList = NULL, *maf, *bigMaf;
struct mafComp *comp, *bigComp;
int totalTextSize = 0;
int i;
for (i=0; i<bed->blockCount; ++i)
    {
    int start = bed->chromStart + bed->chromStarts[i];
    int end = start + bed->blockSizes[i];
    if (thickOnly)
        {
	start = max(start, bed->thickStart);
	end = min(end, bed->thickEnd);
	}
    if (start < end)
        {
	maf = mafFrag(mf, rangeTree, bed->chrom, start, end, '+', database, NULL);
	maf = hgMafFrag(database, track, bed->chrom, start, end, '+',
	   database, NULL);
	slAddHead(&mafList, maf);
	for (comp = maf->components; comp != NULL; comp = comp->next)
	    hashStore(speciesHash, comp->src);
	totalTextSize += maf->textSize; 
	}
    }
slReverse(&mafList);

/* Add species in order list too */
struct slName *org;
for (org = orgList; org != NULL; org = org->next)
    hashStore(speciesHash, org->name);

/* Allocate memory for return maf that contains all blocks concatenated together. 
 * Also fill in components with any species seen at all. */
AllocVar(bigMaf);
bigMaf->textSize = totalTextSize;
struct hashCookie it = hashFirst(speciesHash);
struct hashEl *hel;
while ((hel = hashNext(&it)) != NULL)
    {
    AllocVar(bigComp);
    bigComp->src = cloneString(hel->name);
    bigComp->text = needLargeMem(totalTextSize + 1);
    memset(bigComp->text, '.', totalTextSize);
    bigComp->text[totalTextSize] = 0;
    bigComp->strand = '+';
    bigComp->srcSize = totalTextSize;	/* It's safe if a bit of a lie. */
    hel->val = bigComp;
    slAddHead(&bigMaf->components, bigComp);
    }

/* Loop through maf list copying in data. */
int textOffset = 0;
for (maf = mafList; maf != NULL; maf = maf->next)
    {
    for (comp = maf->components; comp != NULL; comp = comp->next)
        {
	bigComp = hashMustFindVal(speciesHash, comp->src);
	memcpy(bigComp->text + textOffset, comp->text, maf->textSize);
	bigComp->size += comp->size;
	}
    textOffset += maf->textSize;
    }

/* Cope with strand of darkness. */
if (bed->strand[0] == '-')
    {
    for (comp = bigMaf->components; comp != NULL; comp = comp->next)
	reverseComplement(comp->text, bigMaf->textSize);
    }

/* If got an order list then reorder components according to it. */
if (orgList != NULL)
    {
    struct mafComp *newList = NULL;
    for (org = orgList; org != NULL; org = org->next)
        {
	comp = hashMustFindVal(speciesHash, org->name);
	slAddHead(&newList, comp);
	}
    slReverse(&newList);
    bigMaf->components = newList;
    }

/* Rename our own component to bed name */
comp = hashMustFindVal(speciesHash, database);
freeMem(comp->src);
comp->src = cloneString(bed->name);


/* Clean up and go home. */
hashFree(&speciesHash);
mafAliFreeList(&mafList);
return bigMaf;
}
#endif /* SOON  */


void mafFastFrags(char *mafFile, char *indexFile, char *bedFile, char *outFile)
/* mafFastFrags - Extract pieces of multiple alignment, bypassing database.. */
{
struct rbTree *rangeTree = indexToTree(indexFile);
verbose(2, "Read %d from %s\n", rangeTree->n, indexFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
mafFastFrags(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
