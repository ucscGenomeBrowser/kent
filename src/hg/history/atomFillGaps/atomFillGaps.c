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
#include "values.h"
#include "atom.h"

static char const rcsid[] = "$Id: atomFillGaps.c,v 1.2 2007/10/16 18:54:24 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "atomFillGaps - fill gaps between atoms with lineage specific blocks\n"
  "usage:\n"
  "   atomFillGaps regions.bed in.atom out.atom\n"
  "arguments:\n"
  "   regions.bed    bed file with regions in all species\n"
  "   in.atom        list of atoms \n"
  "   out.atom       list of atoms with gaps filled\n"
  "options:\n"
  "   -minLen=N       minimum size of atom to consider\n"
  "   -padOut         pad out edges instead of adding new atoms\n"
  );
}

static struct optionSpec options[] = {
   {"padOut", OPTION_BOOLEAN},
   {"minLen", OPTION_INT},
   {NULL, 0},
};

extern int minLen;
boolean padOut= FALSE;

struct species
{
struct species *next;
char *name;
char *chrom;
int chromStart, chromEnd;
int *atomSequence;
struct psl *pslList;
}; 

struct speciesInfo
{
struct seqBlock *blockList;
char *name;
int min, max;
int count;
};

struct seqBlock
{
struct seqBlock *next;
char *chrom;
int start, end;
char strand;
char *atomName;
int instanceNum;
struct instance *instance;
};

struct species *getSpeciesRanges(char *bedName, struct hash *hash)
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
    hashAdd(hash, new->name, new);
    }

lineFileClose(&lf);
return list;
}

int blockCmp(const void *va, const void *vb)
{
const struct seqBlock *a = *((struct seqBlock **)va);
const struct seqBlock *b = *((struct seqBlock **)vb);

return a->start - b->start;
}

struct speciesInfo *getInfo(char *name, struct hash *hash)
{
struct hashEl *hel;

hel = hashStore(hash, name);

if (hel->val == NULL)
    {
    struct speciesInfo *si;

    AllocVar(si);

    si->name = name;
    hel->val = si;
    }

return hel->val;
}



int outputSI( FILE *f, int *atomNum, struct speciesInfo *si, 
    struct hash *speciesRangeHash)
{
struct seqBlock *sb = si->blockList;
struct species *range = hashMustFindVal(speciesRangeHash, si->name);
int current = range->chromStart;
struct instance *lastInstance = NULL;
char *chrom = sb->chrom;
int added = 0;

for(; sb ; sb = sb->next)
    {
    if (current < sb->start)
	{
	added += sb->start - current;
	if (!padOut)
	    {
	    *atomNum = *atomNum + 1;
	    fprintf(f, ">%d %d %d\n",*atomNum,sb->start - current, 1);
	    fprintf(f,"%s.%s:%d-%d %c\n", si->name,sb->chrom,current+1,sb->start,'+');
	    }
	else
	    {
	    int dif = sb->start - current;
	    int wid = sb->end - sb->start;

	    if (dif)
		{
		if (lastInstance == NULL)
		    {
		    sb->instance->start -= dif;
		    }
		else
		    {
		    int half = dif / 2;
		    lastInstance->end += half;
		    sb->instance->start -= (dif - half);
		    }
		verbose(5,"%d %d\n",wid,dif);
		}
	    }
	}
    current = sb->end;
    lastInstance = sb->instance;
    }

if (current < range->chromEnd)
    {
    added += range->chromEnd - current;
    *atomNum = *atomNum + 1;
    if (!padOut)
	{
	fprintf(f, ">%d %d %d\n",*atomNum,range->chromEnd - current, 1);
	fprintf(f,"%s.%s:%d-%d %c\n", si->name,chrom,current+1,range->chromEnd,'+');
	}
    else
	{
	int dif = range->chromEnd - current;
	if (lastInstance)
	    lastInstance->end += dif;
	}
    }

if (!padOut)
    verbose(2, "Species %s: percent lineage specific %g\n",
	si->name,100.0*added/(range->chromEnd- range->chromStart));

return added;
}

void atomFillGaps(char *bedName, char *inAtomName, char *outAtomName)
{
struct hash *atomHash = newHash(20);
struct atom *atomList = getAtoms(inAtomName, atomHash);
struct atom *atoms = atomList;
struct hash *speciesHash = newHash(0);
struct hash *speciesRangeHash = newHash(0);
struct species *speciesList = getSpeciesRanges(bedName, speciesRangeHash);
FILE *f = mustOpen(outAtomName, "w");
int maxNum = 0;
speciesList = NULL;

slReverse(&atomList);
for(atoms = atomList; atoms; atoms = atoms->next)
    {
    struct instance *instance = atoms->instances;
    int instanceNum = 0;
    int atomNum = atoi(atoms->name);

    maxNum = maxNum > atomNum ? maxNum : atomNum;

    if (!padOut)
	fprintf(f, ">%s %d %d\n",atoms->name, 
		atoms->length, atoms->numInstances);

    for(; instance; instance = instance->next)
	{
	struct speciesInfo *si = getInfo(instance->species, speciesHash);
	struct seqBlock *sb;

	AllocVar(sb);
	sb->chrom = instance->chrom;
	sb->start = instance->start;
	sb->end = instance->end;
	sb->strand = instance->strand;
	sb->atomName = atoms->name;
	sb->instanceNum = instanceNum++;
	sb->instance = instance;

	slAddHead(&si->blockList, sb);

	if (!padOut)
	    fprintf(f,"%s.%s:%d-%d %c\n",instance->species,instance->chrom,
		instance->start+1, instance->end, instance->strand); 
	}
    }

struct hashCookie cook = hashFirst(speciesHash);
struct hashEl *hel;
int totalSize = 0;
int linSize = 0;
int oldMax = maxNum;

while((hel = hashNext(&cook)) != NULL)
    {
    struct speciesInfo *si = hel->val;
    struct species *range = hashMustFindVal(speciesRangeHash, si->name);
    
    totalSize += range->chromEnd - range->chromStart;

    slSort(&si->blockList, blockCmp);

    linSize += outputSI(f, &maxNum, si, speciesRangeHash);
    }

verbose(2, "Percent lineage specific: %g (%d/%d)\n",
    100.0*linSize / totalSize, linSize,totalSize);
verbose(2, "Average size %g\n",(double)linSize/(maxNum - oldMax));

if (padOut)
    {
    for(atoms = atomList; atoms; atoms = atoms->next)
	{
	struct instance *instance;
	int max = 0;

	for( instance = atoms->instances;  instance; instance = instance->next)
	    {
	    int dist = instance->end - instance->start;

	    if (dist > max) 
		max = dist;
	    }

	atoms->length = max;
	} 

    for(atoms = atomList; atoms; atoms = atoms->next)
	{
	struct instance *instance;

	fprintf(f,">%s %d %d\n",atoms->name,atoms->length, atoms->numInstances);

	for( instance = atoms->instances;  instance; instance = instance->next)
	    {
	    fprintf(f,"%s.%s:%d-%d %c\n",instance->species,instance->chrom,
		instance->start+1, instance->end, instance->strand); 
	    }
	} 
    }

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

minLen = optionInt("minLen", minLen);
padOut = optionExists("padOut");

atomFillGaps(argv[1],argv[2],argv[3]);
return 0;
}
