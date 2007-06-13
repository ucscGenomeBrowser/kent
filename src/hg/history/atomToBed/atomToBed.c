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

static char const rcsid[] = "$Id: atomToBed.c,v 1.1 2007/06/13 18:22:05 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "atomToBed - output a chain from a set of atoms and two species\n"
  "usage:\n"
  "   atomToBed in.atom species out.bed\n"
  "arguments:\n"
  "   in.atom        list of atoms\n"
  "   species        name of species to consider\n"
  "   out.bed        name of file to output bed\n"
  "options:\n"
  "   -minLen=N       minimum size of atom to consider\n"
  );
}

static struct optionSpec options[] = {
   {"minLen", OPTION_INT},
   {NULL, 0},
};

int minLen = 1;


struct instance;

struct atom
{
struct atom *next;
char *name;
int numInstances;
int length;
struct instance *instances;
int stringNum;
};

struct instance
{
struct instance *next; /* in atom */
int num; /* in atom */
char *species;
char *chrom;
int start;
int end;
char strand;
struct instance **seqPos; /* in sequence */
struct atom *atom;
struct sequence *sequence;
};

struct sequence
{
struct sequence *next;
char *name;
int length;
struct instance **data;
FILE *f;
char *chrom;
int chromStart, chromEnd;
};


char *bigWords[10*10*1024];

char *getShareString(char *name)
{
static struct hash *stringHash = NULL;

if (stringHash == NULL)
    stringHash = newHash(4);

return hashStoreName(stringHash, name);
}

struct atom *getAtoms(char *atomsName, struct hash *atomHash)
{
struct lineFile *lf = lineFileOpen(atomsName, TRUE);
struct atom *atoms = NULL;
int wordsRead;
struct atom *anAtom = NULL;
int intCount = 0;
int count = 0;

while( (wordsRead = lineFileChopNext(lf, bigWords, sizeof(bigWords)/sizeof(char *)) ))
    {
    if (wordsRead == 0)
	continue;

    if (*bigWords[0] == '>')
	{
	int num;

	if (anAtom != NULL)
	    slReverse(&anAtom->instances);
	if (wordsRead != 3)
	    errAbort("need 3 fields on atom def line");

	intCount = atoi(bigWords[2]);
	if ((num = atoi(bigWords[1])) >= minLen)
	    {
	    AllocVar(anAtom);
	    slAddHead(&atoms, anAtom);
	    anAtom->name = cloneString(&bigWords[0][1]);
	    hashAdd(atomHash, anAtom->name, anAtom);
	    anAtom->length = num;
	    anAtom->numInstances = intCount;
//	    if (anAtom->numInstances >= NUMSTARTS)
//		errAbort("exceeded static max number dups %d\n",NUMSTARTS);
	    }
	else
	    anAtom = NULL;
	count = 0;
	}
    else
	{
	struct instance *instance;
	char *ptr, *chrom, *start;

	intCount--;
	if (intCount < 0)
	    errAbort("bad atom: too many instances");
	if (wordsRead != 2)
	    errAbort("need 2 fields on instance def line");
	if (anAtom == NULL)
	    continue;

	AllocVar(instance);
	instance->num = count++;
	slAddHead(&anAtom->instances, instance);

	ptr = strchr(bigWords[0], '.');
	*ptr++ = 0;
	chrom = ptr;
	instance->species = getShareString(bigWords[0]);

	ptr = strchr(chrom, ':');
	*ptr++ = 0;
	start = ptr;
	instance->chrom = getShareString(chrom);

	ptr = strchr(start, '-');
	*ptr++ = 0;
	instance->start = atoi(start) - 1;
	instance->end = atoi(ptr);
	instance->strand = *bigWords[1];
	
	}
    }
if (anAtom != NULL)
    slReverse(&anAtom->instances);
lineFileClose(&lf);

return atoms;
}


struct hash *getSizes(char *fileName)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordsRead;
struct hash *sizeHash = newHash(12);

while( (wordsRead = lineFileChopNext(lf, bigWords, sizeof(bigWords)/sizeof(char *)) ))
    {
    if (wordsRead != 2)
	errAbort("incorrect number of words");

    int val = sqlUnsigned(bigWords[1]);

    //printf("chrom %s size %d\n",bigWords[0], val);
    hashAddInt(sizeHash, bigWords[0], val);
    }
lineFileClose(&lf);

return sizeHash;
}

char *colors[8] =
{
    "0,0,0",
    "0,0,120",
    "0,120,0",
    "0,120,120",
    "120,0,0",
    "120,0,120",
    "120,120,0",
    "120,120,120",
};

void atomToBed(char *inAtomName, char *species, char *outBedName)
{
struct hash *atomHash = newHash(20);
struct atom *atoms = getAtoms(inAtomName, atomHash);
FILE *f = mustOpen(outBedName, "w");

slReverse(&atoms);
for(; atoms; atoms = atoms->next)
    {
    struct instance *instances = atoms->instances;
    unsigned colorBits = 0;

    for(; instances; instances = instances->next)
	{
	if (sameString("canFam2", instances->species))
	    colorBits |= (1 << 0);
	else if (sameString("mm8", instances->species))
	    colorBits |= (1 << 1);
	else if (sameString("rn4", instances->species))
	    colorBits |= (1 << 2);
	}

    for(instances = atoms->instances; instances; instances = instances->next)
	{
	if (sameString(species, instances->species))
	    {
	    fprintf(f, "%s %d %d %s 0 %c %d %d %s\n",
		instances->chrom, instances->start, instances->end,
		atoms->name, instances->strand,
		instances->start, instances->end,
		colors[colorBits]);
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

atomToBed(argv[1],argv[2],argv[3]);
return 0;
}
