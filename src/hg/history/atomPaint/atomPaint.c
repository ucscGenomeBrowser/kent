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

static char const rcsid[] = "$Id: atomPaint.c,v 1.1 2007/04/20 14:42:44 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "atomPaint - paint atoms to postscript files\n"
  "usage:\n"
  "   atomPaint in.atom in.seq\n"
  "arguments:\n"
  "   in.atom        list of atoms to string\n"
  "   in.seq         list of sequence (atom orderings)\n"
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

void atomPaint(char *inAtomName, char *inSeqName)
{
struct hash *atomHash = newHash(20);
struct atom *atoms = getAtoms(inAtomName, atomHash);
struct sequence *sequences = getSequences(inSeqName, atomHash);
struct sequence *seq;

atoms = NULL;

for(seq = sequences; seq;  seq = seq->next)
    {
    char fileName[2048];

    safef(fileName, sizeof fileName, "%s.ps", seq->name);

    seq->f = mustOpen(fileName, "w");
    }

for(; sequences; sequences = sequences->next)
    {
    struct instance **data = &sequences->data[1];
    struct instance **lastData = &sequences->data[sequences->length + 1];
    struct atom *anAtom;
    double length = sequences->chromEnd - sequences->chromStart;

    for(; data < lastData; data++)
	{
	struct instance *thisInstance = *data;
	
		printf("start %d end %d length %g\n", thisInstance->start, thisInstance->end, length);
	anAtom = thisInstance->atom;
	double offX = 1 * thisInstance->start / length;
	double width = 1 * (thisInstance->end - thisInstance->start) / length;
		printf( "%g %g %g %g fillBox\n",
		    width, (double)1.0, offX,(double)0.0);

	if (anAtom != NULL)
	    {
	    struct instance *instance = anAtom->instances;

	    for(;instance; instance = instance->next)
		{

		float gray = ((instance->start + instance->end) / 2)/length;
		fprintf(instance->sequence->f, "%f setgray\n",gray);
		fprintf(instance->sequence->f, "%g %g %g %g fillBox\n",
		    width, (double)1.0, offX,(double)0.0);
		}
	    }
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

atomPaint(argv[1],argv[2]);
return 0;
}
