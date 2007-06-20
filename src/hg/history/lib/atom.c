
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

static char const rcsid[] = "$Id: atom.c,v 1.2 2007/06/20 23:06:42 braney Exp $";

int minLen = 1;

char *bigWords[10*10*1024];

char *getShareString(char *name)
{
static struct hash *stringHash = NULL;

if (stringHash == NULL)
    stringHash = newHash(4);

return hashStoreName(stringHash, name);
}

struct atom *getPrevAtom(struct instance *instance)
{
struct instance *prevInSeq ;

//if (instance->atom->isTandem)
    //return NULL;

if (instance->strand == '+')
    prevInSeq = *(instance->seqPos - 1);
else
    prevInSeq = *(instance->seqPos + 1);

if (prevInSeq)
    {
    if (prevInSeq->atom == NULL)
	errAbort("atom is null");
    return prevInSeq->atom;
    }

return NULL;
}

struct atom *getNextAtom(struct instance *instance)
{
struct instance *nextInSeq ;

//if (instance->atom->isTandem)
    //return NULL;

if (instance->strand == '+')
    nextInSeq = *(instance->seqPos + 1);
else
    nextInSeq = *(instance->seqPos - 1);

if (nextInSeq)
    {
    if (nextInSeq->atom == NULL)
	errAbort("atom is null");
    return nextInSeq->atom;
    }

return NULL;
}

struct atom *instancesSameNext(struct atom *atom, boolean checkPrev);

struct atom *instancesSamePrev(struct atom *atom, boolean checkNext)
{
struct instance *instance;
struct atom *prevAtom = NULL;
boolean firstTime = TRUE;

//printf("in SamePrev looking at %s\n",atom->name);
for( instance = atom->instances; instance;  instance = instance->next)
    {
    struct atom *thisPrev = getPrevAtom(instance);

    if (thisPrev == NULL)
	return NULL;

    //printf("insam thisPrev %s %d\n",thisPrev->name,thisPrev->stringNum);
   // if (thisPrev->stringNum)
//	return NULL;

    //printf("insamprev numInstances %d %d\n",atom->numInstances,thisPrev->numInstances);
    if (atom->numInstances != thisPrev->numInstances)
	return NULL;

    //printf("inSamePrev %s\n",thisPrev->name);
    if (firstTime)
	{
	firstTime = FALSE;
	prevAtom = thisPrev;
	}
    else
	{
	if (prevAtom != thisPrev)
	    return NULL;
	}
    }

if (checkNext)
    {
    struct atom *nextAtom = instancesSameNext(prevAtom, FALSE);

    if (nextAtom != atom)
	return NULL;
    }

struct instance *strInt = atom->instances;
struct instance *atomInt = prevAtom->instances;

for(; strInt; atomInt = atomInt->next,strInt = strInt->next)
    if (strInt->strand != atomInt->strand)
	{
	//printf("strand not matching\n");
	return NULL;
	}
if ((strInt != NULL ) || (atomInt != NULL))
    return NULL;


//printf("returning prev %s\n",prevAtom->name);
return prevAtom;
}

struct atom *instancesSameNext(struct atom *atom, boolean checkPrev)
{
struct instance *instance;
struct atom *nextAtom = NULL;
boolean firstTime = TRUE;

//printf("in instancesSameNext checking %s\n",atom->name);
for( instance = atom->instances; instance;  instance = instance->next)
    {
    struct atom *thisNext = getNextAtom(instance);

    if (thisNext == NULL)
	return NULL;

    //printf("inSameNext %s\n",thisNext->name);
    //if (thisNext->stringNum)
	//return NULL;

    if (atom->numInstances != thisNext->numInstances)
	return NULL;

    //printf("1 inSameNext %s\n",thisNext->name);
    if (firstTime)
	{
	firstTime = FALSE;
	nextAtom = thisNext;
	}
    else
	{
	if (nextAtom != thisNext)
	    return NULL;
	
	}
    }

if (checkPrev)
    {
    //printf("checking prev for %s\n", nextAtom->name);
    struct atom *prevAtom = instancesSamePrev(nextAtom, FALSE);

    if (prevAtom != atom)
	return NULL;
    }

struct instance *strInt = atom->instances;
struct instance *atomInt = nextAtom->instances;

for(; strInt; atomInt = atomInt->next,strInt = strInt->next)
    if (strInt->strand != atomInt->strand)
	{
	//printf("strand not matching\n");
	return NULL;
	}
if ((strInt != NULL ) || (atomInt != NULL))
    return NULL;

//printf("returning next %s\n",nextAtom->name);
return nextAtom;
}

struct atomString *getNewString(struct atomString **strings, struct atom *atom)
{
struct atomString *string;
static int stringNum = 1;

AllocVar(string);
string->num = stringNum++;
string->length = MAXINT;
slAddHead(strings, string);
slAddHead(&string->baseAtoms, atom->baseAtoms);
atom->stringNum = string->num;

struct instance *instance;
for( instance = atom->instances; instance;  instance = instance->next)
    {
    struct instance *newInt;
    int diff;

    string->count++;
    AllocVar(newInt);
    *newInt = *instance;
    slAddHead(&string->instances, newInt);
    diff = newInt->end - newInt->start;

    if (diff < string->length)
	string->length = diff;
    }
slReverse(&string->instances);

return string;
}

void clumpPrevNeighbors(struct atom *atom, struct atomString *string)
{
struct atom *prevAtom = instancesSamePrev(atom, TRUE);
struct atom *lastAtom = NULL;

//printf("in climpPRev\n");
for(; prevAtom; 
    lastAtom = prevAtom, prevAtom = instancesSamePrev(prevAtom, TRUE))
    {
    prevAtom->stringNum = string->num;
    //printf("joining %s\n",prevAtom->name);
    slAddHead(&string->baseAtoms, prevAtom->baseAtoms);
    }

if (lastAtom)
    {
    struct instance *strInt = string->instances;
    struct instance *atomInt = lastAtom->instances;

    for(; strInt; atomInt = atomInt->next,strInt = strInt->next)
	{
	if (strInt->strand == '+')
	    strInt->start = atomInt->start;
	else
	    strInt->end = atomInt->end;

	int diff = strInt->end - strInt->start;
	if (diff < string->length)
	    {
	    string->length = diff;
	    if (diff < 1)
		errAbort("diff is less than 1 %s %s %d\n",atom->name,lastAtom->name,diff);
	    }
	}
    }
}

void clumpNextNeighbors(struct atom *atom, struct atomString *string)
{
struct atom *nextAtom = instancesSameNext(atom, TRUE);
struct atom *lastAtom = NULL;

//printf("inClimpNext\n");
for(; nextAtom; 
    lastAtom = nextAtom, nextAtom = instancesSameNext(nextAtom, TRUE))
    {
    nextAtom->stringNum = string->num;
    slAddHead(&string->baseAtoms, nextAtom->baseAtoms);
    //printf("joining %s %s\n",atom->name, nextAtom->name);
    }

if (lastAtom)
    {
    struct instance *strInt = string->instances;
    struct instance *atomInt = lastAtom->instances;

    string->length = MAXINT;
    for(; strInt; atomInt = atomInt->next,strInt = strInt->next)
	{
	if (strInt->strand == '+')
	    strInt->end = atomInt->end;
	else
	    strInt->start = atomInt->start;

	int diff = strInt->end - strInt->start;
	//printf("diff is %d\n",diff);
	if (diff < string->length)
	    {
	    string->length = diff;
	    if (diff < 1)
		errAbort("diff is less than 1 %s %s %d\n",atom->name,lastAtom->name,diff);
	    }
	}
    }
}

void clumpNeighbors(struct atom *atom, struct atomString *string)
{
clumpNextNeighbors(atom, string);
clumpPrevNeighbors(atom, string);

slReverse(&string->baseAtoms);
}


void buildStrings(struct atomString **strings, struct atom *atoms)
{
struct atom *atom;

for(atom = atoms; atom; atom = atom->next)
    {
    if (atom->stringNum == 0)
	{
	struct atomString *string = getNewString(strings, atom);
	clumpNeighbors(atom, string);
	}
    }
}

#ifdef NOTNOW
for(; data < last; )
    {
    struct atom *atom = (*data)->atom;
    struct atom *startAtom = atom;
    struct atom *nextAtom = NULL;

    if (atom->stringNum != 0)
	errAbort("atom already has a string");

    data++;

    if (instancesSameNext(atom))
	{
	nextAtom = getNextAtom(atom->instances);

	if (nextAtom == NULL)
	    {
	    /* don't start a new string */
	    continue;
	    }
	}

    struct atomString *string = getNewString(strings, atom);

    if (nextAtom == NULL)
	continue;
    
    int count = 0;
    struct instance *instance;
    for( instance = atom->instances; instance;  instance = instance->next)
	{
	if (instance->strand == '+')
	    instanceStarts[count++] = instance->start;
	else
	    instanceStarts[count++] = instance->end;
	}

    for(;;) 
	{
	struct atom *firstNextAtom = NULL;

	for( instance = atom->instances; instance;  instance = instance->next)
	    {
	    struct instance *nextInSeq ;
	    struct atom *nextAtom = NULL;
	    
	    if (instance->atom == NULL)
		break;

	    if (instance->strand == '+')
		nextInSeq = *(instance->seqPos + 1);
	    else
		nextInSeq = *(instance->seqPos - 1);

	    if (nextInSeq == NULL)
		{
		//printf("nextInSeq is null\n");
		break;
		}
	    else
		{

		nextAtom = nextInSeq->atom;

		if (nextAtom->stringNum)
		    {
		    //printf("next atom has string num %d\n",nextAtom->stringNum);
		    break;
		    }
		}

	    if (firstNextAtom == NULL)
		{
		firstNextAtom = nextAtom;
		if ((nextAtom != NULL) && (firstNextAtom->numInstances != atom->numInstances))
		    {
		    //printf("numInstances not the same\n");
		    break;
		    }
		}
	    else if (nextAtom != firstNextAtom)
		{
		//printf("next not same as first\n");
		break;
		}
	    }
	if (instance != NULL)
	    {
	    //printf("instance is not NULL\n");
	    break;
	    }


	//printf("looking for prev\n");
	struct atom *firstPrevAtom = NULL;
//	if (firstNextAtom == NULL)
//	    firstNextAtom = atom;
	for( instance = firstNextAtom->instances; instance;  instance = instance->next)
	    {
	    struct instance *prevInSeq;

	    if (instance->atom == NULL)
		break;

	    if (instance->strand == '+')
		prevInSeq = *(instance->seqPos - 1);
	    else
		prevInSeq = *(instance->seqPos + 1);

	    if (prevInSeq == NULL)
		break;

	    struct atom *prevAtom = prevInSeq->atom;

	    if (firstPrevAtom == NULL)
		firstPrevAtom = prevAtom;
	    else if (prevAtom != firstPrevAtom)
		break;
	    }

	if (instance != NULL)
	    break;

	firstNextAtom->stringNum = string->num;

	atom = (*data)->atom;
	slAddHead(&string->baseAtoms, atom->baseAtoms);
	data++;
	}

    count = 0;
    for( instance = atom->instances;  instance; instance = instance->next)
	{
	struct instance *newInt;
	int length;

	AllocVar(newInt);
	*newInt = *instance;
	slAddHead(&string->instances, newInt);
	if (instance->strand == '+') 
	    newInt->start = instanceStarts[count];
	else
	    newInt->end = instanceStarts[count];

	count++;
	length = newInt->end - newInt->start;

	if (length < string->length)
	    {
	    string->length = length;
	    if (length < 1)
		errAbort("length below one: %s %d",startAtom->name,length);
	    }
	}

    string->count = count;
    slReverse(&string->instances);
    slReverse(&string->baseAtoms);
    }
}
#endif

struct atom *readAtom( struct lineFile *lf)
{
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
	if (wordsRead != 3)
	    errAbort("need 3 fields on atom def line");

	intCount = atoi(bigWords[2]);
	AllocVar(anAtom);
	anAtom->name = cloneString(&bigWords[0][1]);
	anAtom->length = atoi(bigWords[1]);
	anAtom->numInstances = intCount;
	count = 0;
	}
    else
	{
	struct instance *instance;
	char *ptr, *chrom, *start;

	intCount--;
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
	
	if (intCount == 0)
	    {
	    slReverse(&anAtom->instances);
	    return anAtom;
	    }
	}
    }

return NULL;
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

void outBaseNums (struct atomString *strings, char *outName)
{
FILE *outF = mustOpen(outName, "w");

for(; strings; strings = strings->next)
    {
    struct baseAtom *baseAtom = strings->baseAtoms;
    int size = 0;
    int count = 0;

    for(; baseAtom; baseAtom = baseAtom->next)
	size += baseAtom->numBaseAtoms;
    fprintf(outF, ">%d %d\n",strings->num, size);

    baseAtom = strings->baseAtoms;
    for(; baseAtom; baseAtom = baseAtom->next)
	{
	int *basePtr = baseAtom->baseAtomNums;
	int *lastBasePtr = &baseAtom->baseAtomNums[baseAtom->numBaseAtoms];
	for(; basePtr < lastBasePtr; basePtr++)
	    {
	    fprintf(outF,"%d ", *basePtr);
	    count++;
	    if ((count & 0x7) == 0x7)
		fprintf(outF,"\n");
	    }
	}
    if (!((count & 0x7) == 0x7))
	fprintf(outF,"\n");
    }
fclose(outF);
}

void outAtoms(struct atomString *strings, char *outName)
{
FILE *outF = mustOpen(outName, "w");

for(; strings; strings = strings->next)
    {
    struct instance *instance;

    fprintf(outF, ">%d %d %d\n",strings->num, strings->length, strings->count);

    for( instance = strings->instances;  instance; instance = instance->next)
	{
	fprintf(outF,"%s.%s:%d-%d %c\n",instance->species,instance->chrom,
	    instance->start+1, instance->end, instance->strand); 
	}
    }
fclose(outF);
}

void outSequence(struct sequence *sequences, char *outName)
{
FILE *outF = mustOpen(outName, "w");

for(; sequences; sequences = sequences->next)
    {
    struct instance **data = &sequences->data[1];
    struct instance **lastData = &sequences->data[sequences->length + 1];
    struct atom *anAtom;
    int lastStringNum = 0;
    char *lastAtomName = NULL;
    int count = 0;

    for(; data < lastData; data++)
	{
	struct instance *thisInstance = *data;
	
	anAtom = thisInstance->atom;

	if (anAtom != NULL)
	    {
	    int val = anAtom->stringNum;

	    if (val == 0)
		errAbort("found an atom without a string");

	    if ((val != lastStringNum) || sameString(lastAtomName , anAtom->name)) 
		count++;
	    lastStringNum = val;
	    }
	lastAtomName = anAtom->name;
	}

    fprintf(outF, ">%s.%s:%d-%d %d\n",sequences->name,sequences->chrom,
	sequences->chromStart+1, sequences->chromEnd, count);

    data = &sequences->data[1];
    lastStringNum = 0;
    lastAtomName = NULL;
    count = 0;
    for(; data < lastData; data++)
	{
	struct instance *thisInstance = *data;
	boolean isNeg = thisInstance->strand == '-';
	
	anAtom = thisInstance->atom;

	if (anAtom != NULL)
	    {
	    int val = anAtom->stringNum;

	    if ((val != lastStringNum) || sameString(lastAtomName, anAtom->name))
		{
		if (isNeg)
		    fprintf(outF,"%d.%d ",-val,thisInstance->num);
		else
		    fprintf(outF,"%d.%d ",val, thisInstance->num);
		count++;
		if ((count & 0x7) == 0x7)
		    fprintf(outF,"\n");
		lastStringNum = val;
		}
	    lastAtomName = anAtom->name;
	    }
	}
    if (!((count & 0x7) == 0x7))
	fprintf(outF,"\n");
    }
}

void getBaseAtomNames(char *fileName, struct hash *atomHash)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordsRead;
boolean needAtomName = TRUE;
int count = 0;
int *basePtr = NULL;
struct baseAtom *baseAtom = NULL;
int numBaseAtoms = 0;

while( (wordsRead = lineFileChopNext(lf, bigWords, sizeof(bigWords)/sizeof(char *)) ))
    {
    if (wordsRead == 0)
	continue;

    if (needAtomName)
	{
	if (bigWords[0][0] != '>')
	    errAbort("expected '>' on line %d\n",lf->lineIx);

	numBaseAtoms = atoi(bigWords[1]);
	struct atom *atom = hashFindVal(atomHash, &bigWords[0][1]);
	needAtomName = FALSE;
	count = 0;
	if (atom == NULL)
	    baseAtom = NULL;
	else
	    {
	    AllocVar(baseAtom);
	    atom->baseAtoms = baseAtom;

	    baseAtom->numBaseAtoms = numBaseAtoms;
	    baseAtom->baseAtomNums = 
		needLargeMem(sizeof(int) * baseAtom->numBaseAtoms);
	    basePtr = baseAtom->baseAtomNums;
	    }
	}
    else
	{
	if (bigWords[0][0] == '>')
	    errAbort("not enough base atoms (expected %d, got %d) on line %d\n",
		numBaseAtoms, count, lf->lineIx);

	if (count + wordsRead > numBaseAtoms)
	    errAbort("too many base atoms (expected %d, got %d) on line %d\n",
		numBaseAtoms, count + wordsRead, lf->lineIx);

	if (baseAtom != NULL)
	    {
	    int ii;
	    for (ii=0; ii < wordsRead; ii++)
		*basePtr++ = atoi(bigWords[ii]);
	    }

	count += wordsRead;

	if (count == numBaseAtoms)
	    needAtomName = TRUE;
	}
    }
}

void atomString(char *inAtomName, char *inSeqName, char *inBaseName,
    char *outAtomName, char *outSeqName, char *outBaseName)
{
struct atomString *strings = NULL;
struct hash *atomHash = newHash(20);
struct atom *atoms = getAtoms(inAtomName, atomHash);
struct sequence *sequences = getSequences(inSeqName, atomHash);
struct atom *atom;

getBaseAtomNames(inBaseName, atomHash);


slReverse(&atoms);

for(atom = atoms;atom; atom = atom->next)
    {
    struct instance *instance = atom->instances;
    int count = 0;

    for(; instance; count++, instance = instance->next)
	if (instance->atom == NULL) 
	    //atom->isTandem = TRUE;
	    errAbort("atom NULL for instance %d in species %s, atom %s\n",
		count,instance->species, atom->name);
    }

buildStrings(&strings, atoms);
/*
for(seq = sequences ; seq ; seq = seq->next)
    {
    printf("building strings with %s\n",seq->name);
    buildStrings(&strings, seq);
    }
    */

slReverse(&strings);

outAtoms(strings, outAtomName);
outSequence(sequences, outSeqName);
outBaseNums(strings, outBaseName);
}

