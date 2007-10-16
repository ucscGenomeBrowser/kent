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

static char const rcsid[] = "$Id: atomDrop.c,v 1.4 2007/10/16 18:44:18 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "atomDrop - drop some atoms\n"
  "usage:\n"
  "   atomDrop in.atom out.atom\n"
  "arguments:\n"
  "   in.atom        list of atoms to string\n"
  "   out.atom       output atoms\n"
  "options:\n"
  "   -minLen=N       minimum size of atom to consider\n"
  "   -justSort       just sort the atoms and output in sort order\n"
  "   -justDups       just drop the atoms with dups\n"
  "   -percent        percent to drop (default 5)\n"
  "   -dupScore       use score that encourages dups\n"
  );
}

static struct optionSpec options[] = {
   {"percent", OPTION_INT},
   {"minLen", OPTION_INT},
   {"justSort", OPTION_BOOLEAN},
   {"justDups", OPTION_BOOLEAN},
   {"dupScore", OPTION_BOOLEAN},
   {NULL, 0},
};

int maxGap = 100000;
int minLen = 1;
boolean justSort = FALSE;
boolean justDups = FALSE;
boolean dupScore = FALSE;
boolean noOverlap = FALSE;
char *stringsFile = NULL;
int dropPercent = 5;

#define NUMSTARTS 10000
int instanceStarts[NUMSTARTS];

struct instance;

struct atom
{
struct atom *next;
char *name;
int numInstances;
int length;
struct instance *instances;
int stringNum;
int numSpecies;
int numSpeciesDuped;
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
	    if (anAtom->numInstances >= NUMSTARTS)
		errAbort("exceeded static max number dups %d\n",NUMSTARTS);
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

int instanceCmp(const void *va, const void *vb)
/* Compare to sort based species */
{
const struct instance *a = *((struct instance **)va);
const struct instance *b = *((struct instance **)vb);

return strcmp(a->species, b->species);
}

int atomNameCmp(const void *va, const void *vb)
{
const struct atom *a = *((struct atom **)va);
const struct atom *b = *((struct atom **)vb);
int aNum = atoi(a->name);
int bNum = atoi(b->name);

return aNum - bNum;
}


int atomScoreCmp(const void *va, const void *vb)
{
const struct atom *a = *((struct atom **)va);
const struct atom *b = *((struct atom **)vb);
double aMult = (a->numSpeciesDuped + 1);
double bMult = (b->numSpeciesDuped + 1);
double aScore;
double bScore;

//double aScore = aMult * pow( a->length, a->numSpecies);
//double bScore = bMult * pow( b->length, b->numSpecies);
if (dupScore)
    {
    aScore = log(a->numInstances) * aMult * (double)a->length * (double)a->numSpecies* (double)a->numSpecies;
    bScore = log(b->numInstances) * bMult * (double)b->length * (double)b->numSpecies* (double)b->numSpecies;
    }
else
    {
    aScore = (1 + log((double)a->length)) * (double)a->numSpecies* (double)a->numSpecies;
    bScore = (1 + log((double)b->length)) * (double)b->numSpecies* (double)b->numSpecies;
    }

if (aScore - bScore > 0)
    return 1;

return -1;
}

struct sequence *getSequences(char *inName, struct hash *atomHash)
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
int wordsRead, count = 0;
struct sequence *seqHead = NULL;
struct sequence *sequence = NULL;

while( (wordsRead = lineFileChopNext(lf, bigWords, 
		sizeof(bigWords)/sizeof(char *)) ))
    {
    if (*bigWords[0] == '>')
	{
	char *name = &bigWords[0][1];
	char *chrom = strchr(name, '.');

	*chrom++ = 0;
	char *start = strchr(chrom, ':');
	*start++ = 0;
	char *end = strchr(start, '-');
	*end++ = 0;

	AllocVar(sequence);
	slAddHead(&seqHead, sequence);

	sequence->name = getShareString(name);
	sequence->length = atoi(bigWords[1]);
	sequence->chrom = getShareString(chrom);
	sequence->chromStart = atoi(start);
	sequence->chromEnd = atoi(end);

	sequence->data = needLargeMem((sequence->length + 2) * sizeof(struct instance *));
	count = 0;
	sequence->data[count++] = NULL;
	}
    else
	{
	int ii;

	if (count + wordsRead - 1 > sequence->length)
	    errAbort("too many words for sequence");

	for(ii=0; ii < wordsRead; ii++)
	    {
	    char *ptr = strchr(bigWords[ii], '.');
	    char *name = bigWords[ii];

	    *ptr++ = 0;
	    if (*name == '-')
		name++;
	    struct atom *atom = hashFindVal(atomHash, name);

	    if (atom == NULL)
		{
		sequence->length--;
		continue;
		}

	    int num = atoi(ptr);

	    struct instance *instance = atom->instances;
	    for(; (num > 0) && instance; instance = instance->next)
		num--;

	    instance->seqPos = &sequence->data[count];
	    instance->atom = atom;
	    instance->sequence = sequence;
	    sequence->data[count] = instance;

	    count++;
	    }
	}
    }

lineFileClose(&lf);

slReverse(&seqHead);
return seqHead;
}

void calcNumSpecies(struct atom *atoms)
{
struct instance *instance;

slSort(&atoms->instances, instanceCmp);

for(; atoms; atoms = atoms->next)
    {
    int count = 0;;
    int dups = 0;
    char *lastSpecies = NULL;

    for( instance = atoms->instances;  instance; instance = instance->next)
	{
	boolean same = FALSE;

	printf("looking at %s  instance %d\n",atoms->name, instance->num);
	if (lastSpecies && sameString(instance->species, lastSpecies))
	    same = TRUE;

	if (same)
	    dups++;
	printf("dups %d lastSpecies %s same %d\n",dups,lastSpecies,same);
	if (lastSpecies && !same)
	    {
	    if (dups >= 1)
		{
		printf("species duped\n");
		atoms->numSpeciesDuped++;
		}

	    dups = 0;
	    count++;
	    }
	lastSpecies = instance->species;
	}
    if (dups >= 1)
	{
	printf("out species duped\n");
	atoms->numSpeciesDuped++;
	}
    atoms->numSpecies = count + 1;
    }
}

void atomDrop(char *inAtomName, char *outAtomName)
{
struct hash *atomHash = newHash(20);
struct atom *atoms = getAtoms(inAtomName, atomHash);
struct atom *atom;
int count = 0;

calcNumSpecies(atoms);

if (justDups)
    {
    struct atom *prev = NULL;

    for(atom = atoms; atom ; atom = atom->next)
	{
	if (atom->numSpeciesDuped)
	    {
	    if (prev == NULL)
		atoms = atom->next;
	    else
		prev->next = atom->next;
	    }
	else
	    prev = atom;
	}
    atom = atoms;
    }
else 
    {
    slSort(&atoms, atomScoreCmp);
    if (!justSort)
	{
	for(atom = atoms; atom ; atom = atom->next)
	    count++;

	count *= dropPercent;
	count /= 100;
	count++;

	for(atom = atoms; count-- ; atom = atom->next)
	    ;
	slSort(&atom, atomNameCmp);
	}
    else
	{
	slReverse(&atoms);
	atom = atoms;
	}
    }

FILE *f = mustOpen(outAtomName, "w");

for(; atom; atom = atom->next)
    {
    double pow();
    struct instance *instance;
    //double aMult = (atom->numSpeciesDuped + 1);
    //double aScore = aMult * pow( (double)atom->length, (double)atom->numSpecies);
    //double aScore = aMult * log(atom->numInstances) * (double)atom->length * (double)atom->numSpecies* (double)atom->numSpecies;

    // fprintf(f, ">%s %d %d %d %d %g\n",atom->name, atom->length, atom->numInstances, atom->numSpecies, atom->numSpeciesDuped, aScore + 0.2 );
    fprintf(f, ">%s %d %d\n",atom->name, atom->length, atom->numInstances);

    for( instance = atom->instances;  instance; instance = instance->next)
	{
	fprintf(f,"%s.%s:%d-%d %c\n",instance->species,instance->chrom,
	    instance->start+1, instance->end, instance->strand);
	}
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();

dropPercent = optionInt("percent", dropPercent);
minLen = optionInt("minLen", minLen);
justSort = optionExists("justSort");
justDups = optionExists("justDups");
dupScore = optionExists("dupScore");

atomDrop(argv[1],argv[2]);
return 0;
}
