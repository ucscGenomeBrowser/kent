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

static char const rcsid[] = "$Id: atomDrop.c,v 1.1 2007/04/20 14:38:42 braney Exp $";

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
  );
}

static struct optionSpec options[] = {
   {"minLen", OPTION_INT},
   {NULL, 0},
};

int maxGap = 100000;
int minLen = 1;
boolean noOverlap = FALSE;
char *stringsFile = NULL;

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
	instance->start = atoi(start);
	instance->end = atoi(ptr);
	instance->strand = *bigWords[1];
	
	}
    }
if (anAtom != NULL)
    slReverse(&anAtom->instances);
lineFileClose(&lf);

return atoms;
}

int atomCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct atom *a = *((struct atom **)va);
const struct atom *b = *((struct atom **)vb);
int diff = a->length - b->length;

if (diff == 0)
    return a->numInstances - b->numInstances;

return diff;
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

void atomDrop(char *inAtomName, char *outAtomName)
{
struct hash *atomHash = newHash(20);
struct atom *atoms = getAtoms(inAtomName, atomHash);
struct atom *atom;
int count = 0;

slSort(&atoms, atomCmp);

for(atom = atoms; atom ; atom = atom->next)
    count++;

count *= 1;
count /= 100;
count++;

for(atom = atoms; count-- ; atom = atom->next)
    ;

FILE *f = fopen(outAtomName, "w");

for(; atom; atom = atom->next)
    {
    struct instance *instance;

    fprintf(f, ">%s %d %d\n",atom->name, atom->length, atom->numInstances);

    for( instance = atom->instances;  instance; instance = instance->next)
	{
	fprintf(f,"%s.%s:%d-%d %c\n",instance->species,instance->chrom,
	    instance->start, instance->end, instance->strand);
	}
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();

minLen = optionInt("minLen", minLen);

atomDrop(argv[1],argv[2]);
return 0;
}
