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
#include "maf.h"
#include "twoBit.h"

static char const rcsid[] = "$Id: atomToMaf.c,v 1.1 2007/06/10 22:34:42 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "atomToMaf - output a maf from a set of atoms and a reference species\n"
  "usage:\n"
  "   atomToMaf in.atom species sizeFile seqFile out.maf\n"
  "arguments:\n"
  "   in.atom        list of atoms\n"
  "   species        name of species to consider (\"none\" for atom order)\n"
  "   sizeFile       a file with a list of species with size tables\n"
  "   seqFile        a file with a list of species with 2bit files\n"
  "   out.maf/outdir name of file to output MAF or dir if base option\n"
  "options:\n"
  "   -minLen=N       minimum size of atom to consider\n"
  "   -base=filename  name of file containing atoms with base atoms\n"
  "   -dups           output only dup'ed atoms\n"
  );
}

static struct optionSpec options[] = {
   {"minLen", OPTION_INT},
   {"base", OPTION_STRING},
   {"dups", OPTION_BOOLEAN},
   {NULL, 0},
};

char *baseAtoms = NULL;
int minLen = 1;
boolean justDups = FALSE;

struct nameList
{
struct nameList *next;
char *name;
};

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

struct twoBits
{
struct twoBits *next;
char *species;
char *twoBitName;
};

char *bigWords[10*10*1024];

char *getShareString(char *name)
{
static struct hash *stringHash = NULL;

if (stringHash == NULL)
    stringHash = newHash(4);

return hashStoreName(stringHash, name);
}

struct twoBits *get2BitNames(char *fileName)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordsRead;
struct twoBits *allBits = NULL;

while( (wordsRead = lineFileChopNext(lf, bigWords, sizeof(bigWords)/sizeof(char *)) ))
    {
    struct twoBits *bits;

    AllocVar(bits);
    slAddHead(&allBits, bits);
    bits->species = cloneString(bigWords[0]);
    bits->twoBitName = cloneString(bigWords[1]);
    }

return allBits;
}

struct hash *getSizeHashes(char *fileName)
{
struct hash *sizeHash = newHash(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordsRead;

while( (wordsRead = lineFileChopNext(lf, bigWords, sizeof(bigWords)/sizeof(char *)) ))
    {
    struct lineFile *lf2 = lineFileOpen(bigWords[1], TRUE);
    struct hash *hash = newHash(0);

    hashAdd(sizeHash, bigWords[0], hash);

    while( (wordsRead = lineFileChopNext(lf2, bigWords, sizeof(bigWords)/sizeof(char *)) ))
	{
	hashAddInt(hash, bigWords[0], atoi(bigWords[1]));
	}

    lineFileClose(&lf2);
    }

lineFileClose(&lf);

return sizeHash;
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

int cmpAli(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct mafAli *a = *((struct mafAli **)va);
const struct mafAli *b = *((struct mafAli **)vb);
struct mafComp *aComp = a->components;
struct mafComp *bComp = b->components;

return aComp->start - bComp->start;
}

void outAtomList(char *species, struct atom *atoms, struct twoBits *twoBits, 
    struct hash *sizeHash, FILE *f)
{
char buffer[4096];
struct mafAli *allAli = NULL;
struct mafAli *ali = NULL;
boolean noSpecies = sameString(species, "none");

mafWriteStart(f, NULL);
for(; atoms; atoms = atoms->next)
    {
    struct instance *instance = atoms->instances;
    struct mafAli *aliList = NULL;

    //printf("outing atom %s\n",atoms->name);
    for(; instance; instance = instance->next)
	{
	if ((aliList == NULL) && (noSpecies || sameString(species, instance->species)))
	    {
	    struct mafComp *firstComp = NULL;

	    AllocVar(ali);
	    slAddHead(&aliList, ali);
	    ali->textSize = atoms->length;

	    AllocVar(firstComp);
	    ali->components = firstComp;

	    safef(buffer, sizeof buffer, "%s.%s",
		instance->species,instance->chrom);
	    //printf("firstComp %s\n",buffer);
	    firstComp->src = getShareString(buffer);
	    firstComp->strand = instance->strand;
	    firstComp->start = instance->start;
	    firstComp->size = instance->end - instance->start;
	    }
	}

    for(ali = aliList; ali ; ali = ali->next)
	{
	boolean first = TRUE;
	instance = atoms->instances;
	for(; instance; instance = instance->next)
	    {
	    struct mafComp *comp = NULL;

	    if (first && (noSpecies || sameString(species, instance->species)))
		first = FALSE;
	    else
		{
		AllocVar(comp);
		slAddHead(&ali->components, comp);

		safef(buffer, sizeof buffer, "%s.%s",
		    instance->species,instance->chrom);
//		printf("later Comp %s\n",buffer);
		comp->src = getShareString(buffer);
		comp->strand = instance->strand;
		comp->start = instance->start;
		comp->size = instance->end - instance->start;
		}
	    }
	slReverse(&ali->components);
	}
    if (aliList)
	slAddHead(&allAli, aliList);
    }

if (noSpecies)
    slReverse(&allAli);
else
    slSort(&allAli, cmpAli);

for(; twoBits; twoBits = twoBits->next)
    {
    struct twoBitFile *bitFile = twoBitOpen(twoBits->twoBitName);
    struct hash *hash = hashMustFindVal(sizeHash, twoBits->species);

    for(ali = allAli; ali ; ali = ali->next)
	{
	struct mafComp *comp;

	for(comp = ali->components; comp ; comp = comp->next)
	    {
	    strcpy(buffer, comp->src);
	    char *species = buffer;
	    char *seqName = strchr(species, '.');

	    *seqName++ = 0;

	    if (sameString(species, twoBits->species))
		{
		struct dnaSeq *seq;
		int size = hashIntVal(hash, seqName);

		seq = twoBitReadSeqFrag(bitFile, seqName, 
		    comp->start, comp->start + comp->size);
		comp->text = seq->dna;
		if (comp->strand == '-')
		    reverseComplement(comp->text, strlen(comp->text));
		comp->srcSize = size;
		}
	    }
	}
    twoBitClose(&bitFile);
    }

for(ali = allAli; ali ; ali = ali->next)
    mafWrite(f, ali);
}

struct hash *getBaseHash(char *fileName)
{
struct hash *hash = newHash(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordsRead;
boolean needAtomName = TRUE;
int count = 0;
int numBaseAtoms = 0;
struct nameList *nmHead = NULL;
char *headName = NULL;

while( (wordsRead = lineFileChopNext(lf, bigWords, sizeof(bigWords)/sizeof(char *)) ))
    {
    if (wordsRead == 0)
	continue;

    if (needAtomName)
	{
	if (bigWords[0][0] != '>')
	    errAbort("expected '>' on line %d\n",lf->lineIx);

	numBaseAtoms = atoi(bigWords[1]);
	needAtomName = FALSE;
	count = 0;

	if (nmHead != NULL)
	    {
	    slReverse(&nmHead);
	    //printf("adding to base hash %s\n",headName);
	    hashAddUnique(hash, headName, nmHead);
	    }

	nmHead = NULL;
	headName = cloneString(&bigWords[0][1]);
	}
    else
	{
	if (bigWords[0][0] == '>')
	    errAbort("not enough base atoms (expected %d, got %d) on line %d\n",
		numBaseAtoms, count, lf->lineIx);

	if (count + wordsRead > numBaseAtoms)
	    errAbort("too many base atoms (expected %d, got %d) on line %d\n",
		numBaseAtoms, count + wordsRead, lf->lineIx);

	int ii;

	for(ii=0; ii < wordsRead; ii++)
	    {
	    struct nameList *nl;

	    AllocVar(nl);
	    nl->name = cloneString(bigWords[ii]);
	    slAddHead(&nmHead, nl);
	    }

	count += wordsRead;

	if (count == numBaseAtoms)
	    needAtomName = TRUE;

	}
    }

if (nmHead != NULL)
    {
    slReverse(&nmHead);
    hashAddUnique(hash, headName, nmHead);
    }

return hash;
}

boolean isDup(struct atom *atom)
{
struct instance *instance = atom->instances;
char *lastSpecies = NULL;

for(; instance; instance = instance->next)
    {
    if (instance->species == lastSpecies)
	return TRUE;
    lastSpecies = instance->species;
    }

return FALSE;
}

void atomToMaf(char *inAtomName, char *species, char *sizeFile,
	char *seqFile, char *outMafName)
{
struct hash *atomHash = newHash(20);
struct atom *atoms = getAtoms(inAtomName, atomHash);
struct hash *sizeHash = getSizeHashes(sizeFile);
struct twoBits *twoBits = get2BitNames(seqFile);

if (baseAtoms == NULL)
    {
    FILE *f = mustOpen(outMafName, "w");

    slReverse(&atoms);

    outAtomList(species, atoms, twoBits, sizeHash, f);
    }
else
    {
    struct hash *baseHash = getBaseHash(baseAtoms);
    struct hashCookie cook = hashFirst(baseHash);
    struct hashEl *hashEl;

    while((hashEl = hashNext(&cook)) != NULL)
	{
	struct nameList *nl = hashEl->val;
	char buffer[4096];
	struct atom *atomList = NULL;

	safef(buffer, sizeof buffer, "%s/%s.maf",outMafName, hashEl->name);


	for(; nl; nl = nl->next)
	    {
	    //printf("waling hash looking for %s\n",nl->name);
	    struct atom *anAtom = hashMustFindVal(atomHash, nl->name);
	    //if (anAtom)
		//printf("found\n");
	    if (justDups && isDup(anAtom))
		{
		//printf("adding\n");
		slAddHead(&atomList, anAtom);
		}
	    }


	if (atomList != NULL)
	    {
	    slReverse(&atomList);
	    FILE *f = mustOpen(buffer, "w");
	    verbose(2, "opened %s\n",buffer);
	    outAtomList(species, atomList, twoBits, sizeHash, f);
	    fclose(f);
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();

minLen = optionInt("minLen", minLen);
baseAtoms = optionVal("base", NULL);
justDups = optionExists("dups");

atomToMaf(argv[1],argv[2],argv[3],argv[4],argv[5]);
return 0;
}
