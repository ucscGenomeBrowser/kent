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

static char const rcsid[] = "$Id: makeAtoms.c,v 1.2 2007/04/09 16:52:59 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "makeAtoms - takes a list of regions on different species and corresponding\n"
  "     alignments between the regions.  Outputs a set of atoms and the \n"
  "     atom sequence for each range\n"
  "usage:\n"
  "   makeAtoms regions.bed input.psl out.atoms out.seq\n"
  "arguments:\n"
  "   regions.bed    bed4 file with name set to species name\n"
  "   input.psl      PSL alignments between \n"
  "   out.atoms      list of atoms\n"
  "   out.seq        sequence of atoms for each species\n"
  "options:\n"
  );
}

static struct optionSpec options[] = {
   {"trans", OPTION_BOOLEAN},
   {NULL, 0},
};


boolean doTransitive = FALSE;

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
int getBase(struct psl *psl, int base, char *strand)
{
int offset = -1;
int  diff;
int *tStart = psl->tStarts;
int *lastTStart = &psl->tStarts[psl->blockCount];
int *qStart = psl->qStarts;
int *block = psl->blockSizes;

for(;  tStart < lastTStart; tStart++, qStart++, block++)
    {
    if (*tStart > base)
	return -1;

    if (*tStart + *block > base)
	break;
    }

if (tStart == lastTStart)
    return -1;

diff = base - *tStart;

*strand = psl->strand[0];
if (*strand == '+')
    offset = *qStart + diff;
else
    offset = psl->qSize - (*qStart + *block) + diff;

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

struct column *getSet(int base, struct psl *psl, struct hash *speciesHash)
{
struct column *aCol = NULL;

AllocVar(aCol);

for(; psl && (psl->tStart <= base); psl = psl->next)
    {
    if (psl->tEnd > base)
	{
	char strand;
	int offset = getBase(psl, base, &strand);

	if (offset < 0)
	    continue;

	char *ptr = strchr(psl->qName, '.');

	*ptr = 0;
	struct species *species = hashMustFindVal(speciesHash, psl->qName);
	*ptr = '.';

	//printf("off %d\n",offset);
	if (offset >= 0)
	    AddBaseToCol(aCol, species, offset, strand);
	}
    }
return aCol;
}

boolean colContig(struct column *colOne, struct atom *atom)
{
struct aBase *one = colOne->bases;
struct aBase *prev = atom->run->bases;

for(; one && prev ; one = one->next, prev = prev->next)
    {
    if (one->strand != prev->strand)
	return FALSE;
    if (1) // one->strand == '+')
	{
	if (one->offset != prev->offset + atom->run->length )
	    return FALSE;
	}
    else
	{
	if (one->offset != prev->offset - 1)
	    return FALSE;
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
    }
}

static struct atom *atoms = NULL;
static int numAtoms = 0;

struct atom *getNewAtom()
{
static int allocAtoms = 0;
struct atom *ret;

if (numAtoms == allocAtoms)
    {
    int oldSize = allocAtoms * sizeof(struct atom);

    allocAtoms += 100;
    int newSize = allocAtoms * sizeof(struct atom);

    if (atoms == NULL)
	atoms = needMem(newSize);
    else
    	atoms = needMoreMem(atoms, oldSize, newSize);
    }

ret = &atoms[numAtoms];
ret->num = numAtoms + 1;
numAtoms++;

return ret;
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

    species->atomSequence[aBase->offset - species->chromStart] = atom->num;
    }
}

int baseInRange(struct aBase *aBase, int length, struct aBase *colBase, boolean *doReverse)
{
if (/*(aBase->strand == colBase->strand) &&*/ (aBase->species == colBase->species ))
    {
    int delta; 
    *doReverse = FALSE;

    delta = colBase->offset - aBase->offset ;
    if ((delta >= 0) && ( delta < length))
	{
	if (aBase->strand != colBase->strand)
	    *doReverse = TRUE;
	return delta;
	}
    }
return -1;
}

void dropDups(struct column *aCol)
{
struct aBase *prevBase = NULL;
struct aBase *aBase;

slSort(&aCol->bases, baseSort);

for( aBase = aCol->bases; aBase ;  aBase = aBase->next)
    {
    if (prevBase)
	{
	if (aBase->species == prevBase->species) 
	    {
	    int diff = aBase->offset - prevBase->offset;

	    if (diff == 0)
		{
		prevBase->next = aBase->next;
		continue;
		}
	    }
	}
    prevBase = aBase;
    }
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
	    //freeMem(aBase);
	    }
	continue;
	}

    prevBase = aBase;
    }

return prevBase == NULL;
}


struct aBase *findBase(int atomNum, int offset, struct species *species)
{
struct atom *anAtom = &atoms[atomNum - 1];
struct aBase *aBase = anAtom->run->bases;
int length = anAtom->run->length;
offset += species->chromStart;

for(; aBase; aBase = aBase->next)
    {
    if ((aBase->species ==  species)
        && (offset >= aBase->offset) && (offset < aBase->offset + length))
	{
	return aBase;
	}
    }
errAbort("didn't find species %s in atom %d\n",species->name, atomNum);
return NULL;
}

void cleanPsl(int base, struct psl **list)
{
struct psl *psl, *prevPsl, *nextPsl;

prevPsl = NULL;
for(psl = *list; psl;  psl = nextPsl)
    {
    nextPsl = psl->next;
    if (psl->tStart > base)
	break;

    if (psl->tEnd < base)
	{
	if (prevPsl != NULL)
	    prevPsl->next = nextPsl;
	else
	    *list = nextPsl;
	continue;
	}
    prevPsl = psl;
    }
}

void makeAtoms(char *bedName, char *pslName, 
	char *outAtomName, char *outSeqName)
{
struct hash *speciesHash = newHash(3);
struct species *speciesList = getSpeciesRanges(bedName, &speciesHash);
struct species *species;
struct psl *allPsl = pslLoadAll(pslName);

for(species = speciesList; species; species = species->next)
    {
    struct psl *nextPsl, *psl;
    int rangeWidth = species->chromEnd - species->chromStart;

    /* get space for atom sequence */
    species->atomSequence = needLargeMem(sizeof(int) * rangeWidth);

    /* grab psl's for this species */
    for(psl = allPsl; psl ; psl = nextPsl)
	{
	nextPsl = psl->next;

	if (startsWith(species->name,psl->tName))
	    {
	    slAddHead(&species->pslList, psl);
	    }
	}
    slSort(&species->pslList, pslCmpTarget);
    }

for(species = speciesList; species; species = species->next)
    {
    int base;
    struct atom *atom = NULL;

    for(base = species->chromStart; base < species->chromEnd; base++)
	{
	/* get rid of psl's we won't use since we're passed them */
	cleanPsl(base, &species->pslList);

	struct column *aCol = getSet(base, species->pslList, speciesHash);
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
	    atom = getNewAtom();
	    }

	SetAtomForCol(atom, aCol);
	AddColToAtom(atom, aCol);

	deleteCol(aCol);
	}
    }

FILE *outAtoms = mustOpen(outAtomName,"w");
struct atom *atom;

for(atom = atoms; atom < &atoms[numAtoms] ; atom++)
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
		abr->offset, abr->offset + atom->run->length, abr->strand);
	}
    }

FILE *outSeq = mustOpen(outSeqName,"w");

for(species = speciesList; species; species = species->next)
    {
    int offset;
    int count = 0;
    int prev = 0;
    int thisNum = 0;
    int width = species->chromEnd - species->chromStart;

    for(offset = 0; offset < width; offset++)
	{
	thisNum = species->atomSequence[offset];
	if (prev !=  thisNum)
	    count++;
	prev = thisNum;
	}

    fprintf(outSeq,">%s %d\n",species->name,count);

    count = prev = 0;
    for(offset = 0; offset < width; offset++)
	{
	thisNum = species->atomSequence[offset];
	if (prev !=  thisNum)
	    {
	    struct aBase *base = findBase(thisNum, offset, species);

	    if  (base->strand == '-')
		fprintf(outSeq,"%d.%d ",-thisNum,base->order);
	    else
		fprintf(outSeq,"%d.%d ",thisNum,base->order);
	    count++;
	    if ((count & 0x7) == 0x7)
		fprintf(outSeq,"\n");
	    }
	prev = species->atomSequence[offset];
	}
    if (!((count & 0x7) == 0x7))
	fprintf(outSeq,"\n");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();

if (optionExists("trans"))
    {
    errAbort("can't do transitive now");
    doTransitive = TRUE;
    }

makeAtoms(argv[1],argv[2],argv[3],argv[4]);
printf("Success\n");
return 0;
}
