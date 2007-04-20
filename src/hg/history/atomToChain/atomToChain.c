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

static char const rcsid[] = "$Id: atomToChain.c,v 1.1 2007/04/20 14:39:40 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "atomToChain - output a chain from a set of atoms and two species\n"
  "usage:\n"
  "   atomToChain in.atom target target.sizes query query.sizes out.chain\n"
  "arguments:\n"
  "   in.atom        list of atoms\n"
  "   target         name of target genome\n"
  "   target.sizes   chrom sizes\n"
  "   query          name of query genome\n"
  "   query.sizes    chrom sizes\n"
  "   out.chain      name of file to output chains\n"
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

void atomToChain(char *inAtomName, char *target, char *targetSizes,
    char *query, char *querySizes, char *outChainName)
{
struct hash *atomHash = newHash(20);
struct atom *atoms = getAtoms(inAtomName, atomHash);
FILE *f = mustOpen(outChainName, "w");
struct hash *tSizeHash = getSizes(targetSizes);
struct hash *qSizeHash = getSizes(querySizes);
int chainId = 0;

for(; atoms; atoms = atoms->next)
    {
    boolean hasTarget = FALSE;
    boolean hasQuery = FALSE;
    int qStart=0, qEnd=0;
    int tStart=0, tEnd=0;
    char *tChrom=NULL;
    char *qChrom=NULL;
    char tStrand = 0, qStrand = 0;
    struct instance *instances = atoms->instances;
    int tSize = 0, qSize = 0;

    for(; instances; instances = instances->next)
	{
	if (sameString(target, instances->species))
	    {
	    hasTarget = TRUE;
	    tStart = instances->start;
	    tEnd = instances->end;
	    tChrom = instances->chrom;
	    tStrand = instances->strand;
	    tSize = hashIntVal(tSizeHash, tChrom);
	    if (tStrand == '-')
		{
		int swap = tStart;

		tStart = tSize - tEnd;
		tEnd = tSize - swap;
		}
	    }
	else if (sameString(query, instances->species))
	    {
	    hasQuery = TRUE;
	    qStart = instances->start;
	    qEnd = instances->end;
	    qChrom = instances->chrom;
	    qStrand = instances->strand;
	    qSize = hashIntVal(qSizeHash, qChrom);
	    if (qStrand == '-')
		{
		int swap = qStart;

		qStart = qSize - qEnd;
		qEnd = qSize - swap;
		}
	    }
	}

    if (hasTarget && hasQuery)
	{
	int tSpan = tEnd - tStart;
	int qSpan = qEnd - qStart;
	int diff = tSpan - qSpan;
	int minSpan = tSpan < qSpan ? tSpan : qSpan;

	fprintf(f, "chain %d %s %d %c %d %d %s %d %c %d %d %d\n",
	    0, tChrom, tSize, tStrand, tStart, tEnd,
	    qChrom, qSize, qStrand, qStart, qEnd, chainId++);
	if (diff < 0)
	    fprintf(f, "1 0 %d\n", -diff);
	else
	    fprintf(f, "1 %d 0\n", diff);
	fprintf(f, "%d\n", minSpan - 1);

	}
    }

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();

minLen = optionInt("minLen", minLen);

atomToChain(argv[1],argv[2],argv[3],argv[4],argv[5],argv[6]);
return 0;
}
