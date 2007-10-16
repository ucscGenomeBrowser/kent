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
#include "math.h"
#include "atom.h"
#include "bits.h"

static char const rcsid[] = "$Id: atomBest.c,v 1.2 2007/10/16 18:43:27 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "atomBest - choose the best atoms from a list of sorted atoms\n"
  "usage:\n"
  "   atomBest regions.bed out.atom in.atom [in.atom ...]\n"
  "arguments:\n"
  "   regions.bed    bed file with regions in every species\n"
  "   out.atom       output atoms\n"
  "   in.atom        list of atoms to string\n"
  "options:\n"
  "   -minLen=N       minimum size of atom to consider\n"
  "   -dupScore       use scoring that encourages dups\n"
  );
}

static struct optionSpec options[] = {
   {"minLen", OPTION_INT},
   {"dupScore", OPTION_BOOLEAN},
   {NULL, 0},
};

extern int minLen;
boolean dupScore = FALSE;

struct score
{
double score;
int index;
};

struct species
{
struct species *next;
char *name;
char *chrom;
int chromStart, chromEnd;
Bits *bitmap;
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

void getStats(struct atom *atom, int *numSpecies, int *numDuped)
{
int count = 0;
int dups = 0;
char *lastSpecies = NULL;
struct instance *instance;

*numDuped = 0;
for( instance = atom->instances;  instance; instance = instance->next)
    {
    boolean same = FALSE;

    if (lastSpecies && sameString(instance->species, lastSpecies))
	same = TRUE;

    if (same)
	dups++;
    if (lastSpecies && !same)
	{
	if (dups >= 1)
	    (*numDuped)++;

	dups = 0;
	count++;
	}
    lastSpecies = instance->species;
    }
*numSpecies = count + 1;
}

double scoreAtom(struct atom *a)
{
int numSpecies, numDuped;
getStats(a, &numSpecies, &numDuped);
double aMult = (numDuped + 1);
double aScore;

if (dupScore)
    aScore = (1 + log(a->numInstances)) * aMult * (double)a->length * (double)numSpecies* (double)numSpecies;
else
    aScore =  (1 + log((double)a->length)) * (double)numSpecies* (double)numSpecies;

return aScore;
}

void readAtoms(struct lineFile **fileList, struct atom **atomList, int numInputFiles)
{
int ii;

for(ii=0; ii < numInputFiles; ii++)
    {
    if (atomList[ii] == NULL)
	{
	//printf("filling in slot %d\n",ii);
	atomList[ii] = readAtom(fileList[ii]);
	}
    }
}

int scoreCmp(const void *va, const void *vb)
{
struct score a = *((struct score *)va);
struct score b = *((struct score *)vb);

if (b.score > a.score)
    return 1;
return -1;
}

void freeAtom(struct atom **atom)
{
struct instance *instance, *nextInstance;;

for( instance = (*atom)->instances;  instance; instance = nextInstance)
    {
    nextInstance = instance->next;
    freez(&instance);
    }

freez(atom);
}

void printAtom(FILE *f, struct atom *atom)
{
struct instance *instance;
static int atomNum = 1;

fprintf(f, ">%d %d %d\n",atomNum++, atom->length, atom->numInstances);

for( instance = atom->instances;  instance; instance = instance->next)
    {
    fprintf(f,"%s.%s:%d-%d %c\n",instance->species,instance->chrom,
	instance->start + 1, instance->end, instance->strand);
    }
}

boolean checkAtom(struct hash *speciesHash, struct atom *atom)
{
struct instance *instance;

for( instance = atom->instances;  instance; instance = instance->next)
    {
    struct species *species = hashMustFindVal(speciesHash, instance->species);

    if (bitCountRange(species->bitmap, instance->start - species->chromStart, 
	instance->end - instance->start) > 0)
	{
	//printf("found conflict in %s at %d to %d\n",
	    //instance->species, instance->start, instance->end);
	return FALSE;
	}
    }
for( instance = atom->instances;  instance; instance = instance->next)
    {
    struct species *species = hashMustFindVal(speciesHash, instance->species);

    bitSetRange( species->bitmap, instance->start - species->chromStart, 
	instance->end - instance->start);
    }

return TRUE;
}

void atomBest( char *bedName, char *outName, char **inNames, int numInputFiles)
{
int ii;
struct atom **atomList;
struct lineFile **fileList;
struct hash *speciesHash = newHash(0);
struct species *speciesList = getSpeciesRanges(bedName, &speciesHash);
struct species *species;
struct score *atomScores;

for(species = speciesList ; species;  species = species->next)
    {
    species->bitmap = bitAlloc(species->chromEnd - species->chromStart);
    }

atomScores = needMem(sizeof (struct score ) * numInputFiles);
fileList = needMem(sizeof (FILE *) * numInputFiles);
for(ii=0; ii < numInputFiles; ii++)
    fileList[ii] = lineFileOpen(inNames[ii], TRUE);
atomList = needMem(sizeof (struct atom *) * numInputFiles);
memset(atomList, 0, sizeof (struct atom *) * numInputFiles);

FILE *f = fopen(outName, "w");

for(;;)
    {
    readAtoms(fileList, atomList,  numInputFiles);
    for(ii=0; ii < numInputFiles; ii++)
	if (atomList[ii] != NULL)
	    break;
    if (ii == numInputFiles)
	break;

    for(ii=0; ii < numInputFiles; ii++)
	if (atomList[ii] != NULL)
	    {
	    atomScores[ii].index = ii;
	    atomScores[ii].score = scoreAtom(atomList[ii]);
	    }
	else
	    {
	    atomScores[ii].index = ii;
	    atomScores[ii].score = 0.0;
	    }

    qsort(atomScores, numInputFiles, sizeof(struct score ), scoreCmp);
    //printf("next\n");
    //for(ii=0; ii < numInputFiles; ii++)
//	printf("%d %g\n",atomScores[ii].index,atomScores[ii].score);

    if (checkAtom(speciesHash, atomList[atomScores[0].index]))
	{
	verbose(2, "grabbed atom from %d\n",atomScores[0].index);
	printAtom(f, atomList[atomScores[0].index]);
	}
    freeAtom(&atomList[atomScores[0].index]);

    fflush(f);
    //exit(1);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 4)
    usage();

minLen = optionInt("minLen", minLen);
dupScore = optionExists("dupScore");

atomBest(argv[1], argv[2], &argv[3], argc - 3);
return 0;
}
