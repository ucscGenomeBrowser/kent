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

static char const rcsid[] = "$Id: atomAnno.c,v 1.2 2007/10/16 18:41:23 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "atomAnno - string together atoms\n"
  "usage:\n"
  "   atomAnno in.atom in.seq out.atom \n"
  "arguments:\n"
  "   in.atom        list of atoms to string\n"
  "   in.seq         list of sequence (atom orderings)\n"
  "   out.atom       atoms with annotation\n"
  "options:\n"
  "   -minLen=N       minimum size of atom to consider\n"
  );
}

static struct optionSpec options[] = {
   {"minLen", OPTION_INT},
   {NULL, 0},
};

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
};

struct sequence
{
struct sequence *next;
char *name;
int length;
struct instance **data;
char *chrom;
int chromStart;
int chromEnd;
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
	    //anAtom->name = cloneString(&bigWords[0][1]);
	    struct hashEl *hel = hashAdd(atomHash, &bigWords[0][1], anAtom);
	    anAtom->name = hel->name;
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
	sequence->data = needLargeMem((sequence->length + 2) * sizeof(struct instance *));
	sequence->chrom = getShareString(chrom);
	sequence->chromStart = atoi(start) - 1;
	sequence->chromEnd = atoi(end);
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
	    sequence->data[count] = instance;

	    count++;
	    }
	}
    }

lineFileClose(&lf);

slReverse(&seqHead);
return seqHead;
}

void outAtoms(struct atom *atoms, char *outName)
{
FILE *outF = mustOpen(outName, "w");

for(; atoms; atoms = atoms->next)
    {
    struct instance *instance;

    fprintf(outF, ">%s %d %d\n",atoms->name, atoms->length, atoms->numInstances);

    for( instance = atoms->instances;  instance; instance = instance->next)
	{
	struct instance *nextInSeq = NULL;
	struct instance *prevInSeq = NULL;
	char nextName[4096], prevName[4096];

	if (instance->seqPos == NULL)
	    {
	    errAbort("seqPos is NULL");
	    }
	else
	    {
	    if (instance->strand == '+')
		{
		nextInSeq = *(instance->seqPos + 1);
		prevInSeq = *(instance->seqPos - 1);
		}
	    else
		{
		nextInSeq = *(instance->seqPos - 1);
		prevInSeq = *(instance->seqPos + 1);
		}
	    if (nextInSeq == NULL)
		strcpy(nextName ,"null");
	    else
		{
		if (nextInSeq->strand == '-')
		    safef(nextName, sizeof nextName, "-%s.%d",
			nextInSeq->atom->name, nextInSeq->num);
		else
		    safef(nextName, sizeof nextName, "%s.%d",
			nextInSeq->atom->name, nextInSeq->num);
		}

	    if (prevInSeq == NULL)
		strcpy(prevName ,"null");
	    else
		{
		if (prevInSeq->strand == '-')
		    safef(prevName, sizeof prevName, "-%s.%d",
			prevInSeq->atom->name, prevInSeq->num);
		else
		    safef(prevName, sizeof prevName, "%s.%d",
			prevInSeq->atom->name, prevInSeq->num);
		}

	    }

	fprintf(outF,"%s.%s:%d-%d %c %s %s\n",instance->species,instance->chrom,
	    instance->start+1, instance->end, instance->strand, 
	    prevName, nextName);
	}
    }
fclose(outF);
}


void atomAnno(char *inAtomName, char *inSeqName, char *outAtomName)
{
struct hash *atomHash = newHash(20);
struct atom *atoms = getAtoms(inAtomName, atomHash);

getSequences(inSeqName, atomHash);

/*
for(;atoms; atoms = atoms->next)
    {
    struct instance *instance = atoms->instances;
    int count = 0;

    for(; instance; count++, instance = instance->next)
	if (instance->atom == NULL) 
	    errAbort("atom NULL for instance %d in species %s, atom %s\n",
		count,instance->species, atoms->name);
    }

*/
slReverse(&atoms);
outAtoms(atoms, outAtomName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

atomAnno(argv[1],argv[2], argv[3]);
return 0;
}
