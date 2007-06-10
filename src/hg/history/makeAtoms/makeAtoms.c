#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hdb.h"
#include "hash.h"
#include "bed.h"
#include "psl.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"
#include "dystring.h"

static char const rcsid[] = "$Id: makeAtoms.c,v 1.5 2007/06/10 22:06:29 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "makeAtoms - takes a list of regions on different species and corresponding\n"
  "     alignments between the regions.  Outputs a set of atoms and the \n"
  "     atom sequence for each range\n"
  "usage:\n"
  "   makeAtoms regions.bed input.blocks out.atoms\n"
  "arguments:\n"
  "   regions.bed    bed4 file with name set to species name\n"
  "   input.blocks   alignment blocks\n"
  "   out.atoms      list of atoms\n"
  "options:\n"
  );
}

extern char *bigWords[10*10*1024];

static struct optionSpec options[] = {
   {"trans", OPTION_BOOLEAN},
   {NULL, 0},
};


boolean doTransitive = FALSE;

struct block
{
struct block *next;
unsigned tStart,qStart,size;
char strand;
} ;

struct blockList
{
struct block *blocks;
char *tName;
char *qName;
struct block *currentBlock;
struct block *lastBlock;
};


struct listList
{
struct listList *next;
struct blockList *blockList;
};

struct listHead
{
struct listList *listHead;
};

struct species
{
struct species *next;
char *name;
char *chrom;
int chromStart, chromEnd;
int *atomSequence;
struct psl *pslList;
};


struct aBase
{
struct aBase *next;
struct species *species;
int offset;
char strand;
int order;
};

struct run
{
struct aBase *bases;
int length;
};

struct column
{
struct aBase *bases;
};

struct atom
{
//struct atom *next;
int num;
struct run *run;
};

static unsigned numAtoms = 1;

struct species *getSpeciesRanges(char *bedName, struct hash **hash)
{
struct lineFile *lf = lineFileOpen(bedName, TRUE);
struct species *new, *list = NULL;
char *row[4];

while (lineFileRow(lf, row))
    {
    AllocVar(new);
    slAddHead(&list, new);
    new->chrom = cloneString(row[0]);
    new->chromStart = sqlUnsigned(row[1]);
    new->chromEnd = sqlUnsigned(row[2]);
    new->name = cloneString(row[3]);
    hashAdd(*hash, new->name, new);
    }

lineFileClose(&lf);
return list;
}

int baseSort(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct aBase *a = *((struct aBase **)va);
const struct aBase *b = *((struct aBase **)vb);
int diff;

diff = strcmp(a->species->name, b->species->name);

if (diff)
    return diff;

diff = a->offset - b->offset;

if (diff)
    return diff;

diff = a->strand - b->strand;

return diff;
}

/* get the query base for a given target base */
int getBase(struct block *block, int base, char *strand)
{
int offset = -1;
int  diff;

diff = base - block->tStart;

*strand = block->strand;
if (*strand == '+')
    offset = block->qStart + diff;
else
    offset = (block->qStart + block->size) - diff - 1;

return offset;
}


void AddBaseToCol(struct column *aCol, struct species *species, 
    int offset, char strand)
{
struct aBase *abr = NULL;

AllocVar(abr);
slAddHead(&aCol->bases, abr);

abr->species = species;
abr->offset = offset;
abr->strand = strand;
}

struct column *getSet(int base, char *name, struct listHead *listHead, 
    struct hash *speciesHash)
{
struct column *aCol = NULL;
struct listList *listList = listHead->listHead;

AllocVar(aCol);

for(; listList ; listList = listList->next)
    {
    struct blockList *blockList = listList->blockList;
    struct block *block = blockList->currentBlock;

    if (block == NULL)
	continue;

    for(; (blockList->currentBlock < blockList->lastBlock) && 
	    (blockList->currentBlock->tStart + blockList->currentBlock->size <= base); 
	    blockList->currentBlock++)
	;

    block = blockList->currentBlock;
    for(; (block < blockList->lastBlock) && (block->tStart <= base); block++)
	{
	if(block->tStart + block->size <= base)
	    {
	//    ++blockList->currentBlock;
	    continue;
	    }

	char strand;
	int offset = getBase(block, base, &strand);

	struct species *species = hashMustFindVal(speciesHash, 
	    blockList->qName);

	//if (offset > species->chromEnd)
	    //errAbort("adding bad base");
    
	AddBaseToCol(aCol, species, offset, strand);
	}
    }

return aCol;
}

boolean colContig(struct column *colOne, struct atom *atom)
{
struct aBase *one = colOne->bases;
if (atom->run == NULL)
    return FALSE;

struct aBase *prev = atom->run->bases;

for(; one && prev ; one = one->next, prev = prev->next)
    {
    if (one->species->name != prev->species->name)
	return FALSE;
    if (one->strand != prev->strand)
	return FALSE;

    if (one->strand == '+')
	{
	if (one->offset != prev->offset + atom->run->length )
	    return FALSE;
	}
    else
	{
	    //printf("one %d prev %d length %d\n",one->offset,prev->offset, atom->run->length);
	if (one->offset != prev->offset - 1)
	    {

	    return FALSE;
	    }
	}
    }
return (one == NULL ) && (prev == NULL);
}


void AddColToAtom(struct atom *atom, struct column *aCol)
{
if (atom == NULL)
    errAbort("null atom");

if (atom->run == NULL)
    {
    AllocVar(atom->run);
    atom->run->bases = aCol->bases;
    atom->run->length = 1;
    aCol->bases = NULL;
    }
else 
    {
    atom->run->length++;

    struct aBase *aBase;

    for(aBase = atom->run->bases; aBase; aBase = aBase->next)
	if (aBase->strand == '-')
	    aBase->offset--;
    }
}

void deleteCol(struct column *col)
{
    struct aBase *aBase, *nextBase;

    for( aBase = col->bases; aBase ; aBase = nextBase)
	{
	nextBase = aBase->next;
	freeMem(aBase);
	}
    freeMem(col);
}

void SetAtomForCol(struct atom *atom, struct column *aCol)
{
struct aBase *aBase = aCol->bases;

for(; aBase ; aBase = aBase->next)
    {
    struct species *species = aBase->species;

    if (aBase->strand == '-')
	species->atomSequence[aBase->offset - species->chromStart] = -atom->num;
    else
	species->atomSequence[aBase->offset - species->chromStart] = atom->num;
    }
}


void dropDups(struct column *aCol)
{
struct aBase *prevBase = NULL;
struct aBase *aBase, *nextBase;

slSort(&aCol->bases, baseSort);

for( aBase = aCol->bases; aBase ;  aBase = nextBase)
    {
    nextBase = aBase->next;
    if (prevBase)
	{
	if (aBase->species == prevBase->species) 
	    {
	    int diff = aBase->offset - prevBase->offset;

	    if (diff == 0)
		{
		prevBase->next = aBase->next;
		freez(&aBase);
		continue;
		}
	    }
	}
    prevBase = aBase;
    }
}

void addList(struct hash *blockHash, char *name, struct blockList *blockList)
{
struct listList *listList;
struct listHead *listHead;

if ((listHead = hashFindVal(blockHash, name)) == NULL)
    {
    AllocVar(listHead);
    hashAdd(blockHash, name, listHead);
    }

AllocVar(listList);
listList->blockList = blockList;
slAddHead(&listHead->listHead, listList);
}

struct hash *getBlocks(char *blockName)
{
struct lineFile *lf = lineFileOpen(blockName, TRUE);
char *row[30];
boolean needHeader = TRUE;
struct hash *blockHash = newHash(5);
struct blockList *blockList = NULL;
struct block *block = NULL;
int wordsRead;

while( (wordsRead = lineFileChopNext(lf, row, sizeof(row)/sizeof(char *)) ))
    {
    if (needHeader)
	{
	int count;

	if (row[0][0] != '>')
	    errAbort("expecting '>' on line %d\n",lf->lineIx);

	char *qName = strchr(row[0], '.');
	char *ptr;

	*qName++ = 0;
	qName = strchr(qName, '.');
	*qName++ = 0;
	ptr = strchr(qName, '.');
	*ptr++ = 0;

	AllocVar(blockList);
	blockList->tName = cloneString(&row[0][1]);
	blockList->qName = cloneString(qName);

	addList(blockHash, blockList->tName, blockList);
	if (!sameString(blockList->qName, blockList->tName))
	    addList(blockHash, blockList->qName, blockList);

	count = atoi(row[1]);
	block = blockList->blocks = 
	    needLargeMem(count * sizeof(struct block));
	blockList->lastBlock = &blockList->blocks[count];
	needHeader = FALSE;
	}
    else
	{
	if (row[0][0] == '>')
	    errAbort("not expecting '>' on line %d\n",lf->lineIx);

	block->tStart = atoi(row[0]);
	block->qStart = atoi(row[1]);
	block->size = atoi(row[2]);
	block->strand = row[3][0];
	block++;

	if (block == blockList->lastBlock)
	    needHeader = TRUE;
	}
    }

return blockHash;
}

struct listHead *getListHead(struct hash *blockHash, char *name)
{
return hashFindVal(blockHash, name);
}

int blockCmp(const void *va, const void *vb)
{
const struct block *a = *((struct block **)va);
const struct block *b = *((struct block **)vb);

return a->tStart - b->tStart;
}


void swapBlocks(struct blockList *blockList)
{
struct block *block = blockList->blocks;
unsigned tmp;
char *tmpName;

for(; block < blockList->lastBlock; block++)
    {
    tmp = block->tStart;
    block->tStart = block->qStart;
    block->qStart = tmp;
    }

tmpName = blockList->tName;
blockList->tName = blockList->qName;
blockList->qName = tmpName;

slSort(&blockList->blocks, blockCmp);
}

boolean checkBases(struct column *aCol)
{
struct aBase *aBase = aCol->bases;
struct aBase *nextBase, *prevBase;

if (aBase == NULL)
    return TRUE;

struct species *species = aBase->species;
int prevAtomNum = species->atomSequence[aBase->offset - species->chromStart];

/* check to see if all bases already have the same atom */
for(aBase = aBase->next; aBase ; aBase = aBase->next)
    {
    species = aBase->species;
    int nextAtomNum = species->atomSequence[aBase->offset - species->chromStart];
    if (prevAtomNum != nextAtomNum)
	break;
    prevAtomNum = nextAtomNum;
    }
if ((aBase == NULL) && (prevAtomNum > 0))
    return TRUE;

/* go through column to see if any bases are already part of an atom */
/* if they are, and we're transitive, grab the whole column from that atom */
/* otherwise don't reference that base */
prevBase = NULL;
for( aBase = aCol->bases; aBase ;  aBase = nextBase)
    {
    nextBase = aBase->next;
    species = aBase->species;
    int nextAtomNum = species->atomSequence[aBase->offset-species->chromStart];

    if (nextAtomNum)
	{
#ifdef NOTNOW
	if (doTransitive)
	    {
	    /* this will deassign atoms which will change the atomSequence
	     * array above by side effect */
	    MoveColumnFromAtom(nextAtomNum, aBase, aCol);
	    nextBase = aCol->bases;
	    }
	else	/* drop this base from column */
#endif
	    {
	    //printf("droppoing base %d\n",nextAtomNum);
	    if (prevBase)
		prevBase->next = nextBase;
	    else
		aCol->bases = nextBase;
	    aBase->offset = -1;
	    freeMem(aBase);
	    }
	continue;
	}

    prevBase = aBase;
    }

return prevBase == NULL;
}

void makeAtoms(char *bedName, char *blockName, char *outAtomName)
{
struct hash *speciesHash = newHash(3);
struct species *speciesList = getSpeciesRanges(bedName, &speciesHash);
struct species *species;
struct hash *blockHash = getBlocks(blockName);
FILE *outAtoms = mustOpen(outAtomName,"w");
struct atom ourAtom;

memset(&ourAtom, 0, sizeof ourAtom);

for(species = speciesList; species; species = species->next)
    {
    // struct psl *nextPsl, *psl;
    int rangeWidth = species->chromEnd - species->chromStart;

    /* get space for atom sequence */
    species->atomSequence = needLargeZeroedMem(sizeof(int) * rangeWidth);
    }

for(species = speciesList; species; species = species->next)
    {
    int base;
    struct atom *atom = NULL;
    struct listHead *listHead = getListHead(blockHash, species->name);
    int rangeWidth = species->chromEnd - species->chromStart;

    if (listHead == NULL)
	{
	fprintf(outAtoms, ">%d %d %d\n", numAtoms, rangeWidth, 1);
	numAtoms++;
	fprintf(outAtoms, "%s.%s:%d-%d %c\n",species->name, species->chrom, 
		species->chromStart + 1, species->chromEnd, '+');
	continue;
	}

    struct listList *list = listHead->listHead;

    //FILE *debugF = NULL;
    //printf("species %s\n",species->name);
    //if (sameString("hg18", species->name))
	//debugF = mustOpen("test.bed", "w");

    for(; list ; list = list->next)
	{
	struct blockList *blockList = list->blockList;

	if (blockList->blocks == NULL)
	    continue;

	if (!sameString(blockList->tName, species->name))
	    swapBlocks(blockList);
	blockList->currentBlock = blockList->blocks;

	/*
	if (sameString("hg18", species->name))
	    {
	    struct block *block = blockList->blocks;
	    for(; block < blockList->lastBlock; block++)
		fprintf(debugF, "%s\t%d\t%d\t%s\n","chrX", block->tStart, block->tStart + block->size, blockList->qName);
	    }
	    */
	}

    for(base = species->chromStart; base < species->chromEnd; base++)
	{
	//if (species->atomSequence)
	    //if ( species->atomSequence[base])
		//continue;

	struct column *aCol = getSet(base, species->name,
	    listHead, speciesHash);
	AddBaseToCol(aCol, species, base, '+');

	dropDups(aCol);

	if ((base & 0xfffff) == 0)
	    {
	    printf("species %s base %d end %d\r",species->name, base, species->chromEnd);
	    fflush(stdout);
	    }

	if (checkBases(aCol))
	    {
	    deleteCol(aCol);
	    continue;
	    }

	if ((atom == NULL) || !colContig(aCol, atom))
	    {
	    //printf("base %d\n",base);
	    if (atom)
		{
		if (atom->run == NULL)
		    continue;

		struct aBase *abr = atom->run->bases; 
		int count = 0;

		for(; abr; abr = abr->next)
		    abr->order = count++;

		fprintf(outAtoms, ">%d %d %d\n", atom->num, atom->run->length,count);

		abr = atom->run->bases; 
		for(; abr; abr = abr->next)
		    {
		    fprintf(outAtoms, "%s.%s:%d-%d %c\n",abr->species->name, abr->species->chrom, 
			    abr->offset + 1, abr->offset + atom->run->length, abr->strand);
		    }

		//fprintf(outSeq,"%d\n",numAtoms);
	
		//unsigned int *pint = getNewAtomLen();
		//*pint = atom->run->length;
		numAtoms++;

		}
	    atom = &ourAtom;
	    if (atom->run)
		{
		struct aBase *abr = atom->run->bases; 
		struct aBase *nextAbr;

		for(; abr; abr = nextAbr)
		    {
		    nextAbr = abr->next;
		    freez(&abr);
		    }
		freez(&atom->run);
		}
	    atom->num = numAtoms;
	    }

	SetAtomForCol(atom, aCol);
	AddColToAtom(atom, aCol);

	deleteCol(aCol);
	}
    if (atom)
	{
	if (atom->run == NULL)
	    continue;

	struct aBase *abr = atom->run->bases; 
	int count = 0;

	for(; abr; abr = abr->next)
	    abr->order = count++;

	fprintf(outAtoms, ">%d %d %d\n", atom->num, atom->run->length,count);

	abr = atom->run->bases; 
	for(; abr; abr = abr->next)
	    {
	    fprintf(outAtoms, "%s.%s:%d-%d %c\n",abr->species->name, abr->species->chrom, 
		    abr->offset + 1, abr->offset + atom->run->length, abr->strand);
	    }

	//fprintf(outSeq,"%d\n",numAtoms);

	//unsigned int *pint = getNewAtomLen();
	//*pint = atom->run->length;
	numAtoms++;

	}
    list = listHead->listHead;

    for(; list ; list = list->next)
	freez(&list->blockList->blocks);
    if (species->atomSequence)
	freez(&species->atomSequence);
    }


#ifdef NOTNOW
for(species = speciesList; species; species = species->next)
    {
    int offset;
    int count = 0;
    int thisNum = 0;
    int width = species->chromEnd - species->chromStart;

    for(offset = 0; offset < width; )
	{
	thisNum = species->atomSequence[offset];
	int length = atoms[thisNum - 1].run->length;

	count++;
	offset += length;
	}

    fprintf(outSeq,">%s.%s:%d-%d %d\n",species->name,
	species->chrom, species->chromStart+1, species->chromEnd, count);

    count = 0;
    for(offset = 0; offset < width; )
	{
	thisNum = species->atomSequence[offset];
	int length = atoms[thisNum - 1].run->length;

	struct aBase *base = findBase(thisNum, offset, species);

	if  (base->strand == '-')
	    fprintf(outSeq,"%d.%d ",-thisNum,base->order);
	else
	    fprintf(outSeq,"%d.%d ",thisNum,base->order);
	count++;
	if ((count & 0x7) == 0x7)
	    fprintf(outSeq,"\n");
	offset += length;
	}
    if (!((count & 0x7) == 0x7))
	fprintf(outSeq,"\n");
    }
#endif
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

makeAtoms(argv[1],argv[2],argv[3]);
printf("Success\n");
return 0;
}
